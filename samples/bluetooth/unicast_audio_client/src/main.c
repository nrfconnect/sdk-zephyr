/*
 * Copyright (c) 2021-2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/types.h>
#include <stddef.h>
#include <errno.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/audio/audio.h>
#include <zephyr/bluetooth/audio/bap.h>
#include <zephyr/bluetooth/audio/bap_lc3_preset.h>
#include <zephyr/sys/byteorder.h>

static void start_scan(void);

static struct bt_bap_unicast_client_cb unicast_client_cbs;
static struct bt_conn *default_conn;
static struct k_work_delayable audio_send_work;
static struct bt_bap_unicast_group *unicast_group;
static struct audio_sink {
	struct bt_bap_ep *ep;
	uint16_t seq_num;
} sinks[CONFIG_BT_BAP_UNICAST_CLIENT_ASE_SNK_COUNT];
static struct bt_bap_ep *sources[CONFIG_BT_BAP_UNICAST_CLIENT_ASE_SRC_COUNT];
NET_BUF_POOL_FIXED_DEFINE(tx_pool, CONFIG_BT_BAP_UNICAST_CLIENT_ASE_SNK_COUNT,
			  BT_ISO_SDU_BUF_SIZE(CONFIG_BT_ISO_TX_MTU),
			  CONFIG_BT_CONN_TX_USER_DATA_SIZE, NULL);

static struct bt_bap_stream streams[CONFIG_BT_BAP_UNICAST_CLIENT_ASE_SNK_COUNT +
				      CONFIG_BT_BAP_UNICAST_CLIENT_ASE_SRC_COUNT];
static size_t configured_sink_stream_count;
static size_t configured_source_stream_count;

#define configured_stream_count (configured_sink_stream_count + \
				 configured_source_stream_count)

/* Select a codec configuration to apply that is mandatory to support by both client and server.
 * Allows this sample application to work without logic to parse the codec capabilities of the
 * server and selection of an appropriate codec configuration.
 */
static struct bt_bap_lc3_preset codec_configuration = BT_BAP_LC3_UNICAST_PRESET_16_2_1(
	BT_AUDIO_LOCATION_FRONT_LEFT, BT_AUDIO_CONTEXT_TYPE_UNSPECIFIED);

static K_SEM_DEFINE(sem_connected, 0, 1);
static K_SEM_DEFINE(sem_disconnected, 0, 1);
static K_SEM_DEFINE(sem_mtu_exchanged, 0, 1);
static K_SEM_DEFINE(sem_security_updated, 0, 1);
static K_SEM_DEFINE(sem_sinks_discovered, 0, 1);
static K_SEM_DEFINE(sem_sources_discovered, 0, 1);
static K_SEM_DEFINE(sem_stream_configured, 0, 1);
static K_SEM_DEFINE(sem_stream_qos, 0, ARRAY_SIZE(sinks) + ARRAY_SIZE(sources));
static K_SEM_DEFINE(sem_stream_enabled, 0, 1);
static K_SEM_DEFINE(sem_stream_started, 0, 1);

#define AUDIO_DATA_TIMEOUT_US 1000000UL /* Send data every 1 second */

static uint16_t get_and_incr_seq_num(const struct bt_bap_stream *stream)
{
	for (size_t i = 0U; i < configured_sink_stream_count; i++) {
		if (stream->ep == sinks[i].ep) {
			uint16_t seq_num;

			seq_num = sinks[i].seq_num;

			if (IS_ENABLED(CONFIG_LIBLC3)) {
				sinks[i].seq_num++;
			} else {
				sinks[i].seq_num += (AUDIO_DATA_TIMEOUT_US /
						     codec_configuration.qos.interval);
			}

			return seq_num;
		}
	}

	printk("Could not find endpoint from stream %p\n", stream);

	return 0;
}

#if defined(CONFIG_LIBLC3)

#include "lc3.h"
#include "math.h"

#define MAX_SAMPLE_RATE         48000
#define MAX_FRAME_DURATION_US   10000
#define MAX_NUM_SAMPLES         ((MAX_FRAME_DURATION_US * MAX_SAMPLE_RATE) / USEC_PER_SEC)
#define AUDIO_VOLUME            (INT16_MAX - 3000) /* codec does clipping above INT16_MAX - 3000 */
#define AUDIO_TONE_FREQUENCY_HZ   400

static int16_t audio_buf[MAX_NUM_SAMPLES];
static lc3_encoder_t lc3_encoder;
static lc3_encoder_mem_48k_t lc3_encoder_mem;
static int freq_hz;
static int frame_duration_us;
static int frame_duration_100us;
static int frames_per_sdu;
static int octets_per_frame;


/**
 * Use the math lib to generate a sine-wave using 16 bit samples into a buffer.
 *
 * @param buf Destination buffer
 * @param length_us Length of the buffer in microseconds
 * @param frequency_hz frequency in Hz
 * @param sample_rate_hz sample-rate in Hz.
 */
static void fill_audio_buf_sin(int16_t *buf, int length_us, int frequency_hz, int sample_rate_hz)
{
	const int sine_period_samples = sample_rate_hz / frequency_hz;
	const unsigned int num_samples = (length_us * sample_rate_hz) / USEC_PER_SEC;
	const float step = 2 * 3.1415 / sine_period_samples;

	for (unsigned int i = 0; i < num_samples; i++) {
		const float sample = sin(i * step);

		buf[i] = (int16_t)(AUDIO_VOLUME * sample);
	}
}

static void lc3_audio_timer_timeout(struct k_work *work)
{
	/* For the first call-back we push multiple audio frames to the buffer to use the
	 * controller ISO buffer to handle jitter.
	 */
	const uint8_t prime_count = 2;
	static int64_t start_time;
	static int32_t sdu_cnt;
	int32_t sdu_goal_cnt;
	int64_t uptime, run_time_ms, run_time_100us;

	k_work_schedule(&audio_send_work, K_USEC(codec_configuration.qos.interval));

	if (lc3_encoder == NULL) {
		printk("LC3 encoder not setup, cannot encode data.\n");
		return;
	}

	if (start_time == 0) {
		/* Read start time and produce the number of frames needed to catch up with any
		 * inaccuracies in the timer. by calculating the number of frames we should
		 * have sent and compare to how many were actually sent.
		 */
		start_time = k_uptime_get();
	}

	uptime = k_uptime_get();
	run_time_ms = uptime - start_time;

	/* PDU count calculations done in 100us units to allow 7.5ms framelength in fixed-point */
	run_time_100us = run_time_ms * 10;
	sdu_goal_cnt = run_time_100us / (frame_duration_100us * frames_per_sdu);

	/* Add primer value to ensure the controller do not run low on data due to jitter */
	sdu_goal_cnt += prime_count;

	printk("LC3 encode %d frames in %d SDUs\n", (sdu_goal_cnt - sdu_cnt) * frames_per_sdu,
						    (sdu_goal_cnt - sdu_cnt));

	while (sdu_cnt < sdu_goal_cnt) {
		const uint16_t tx_sdu_len = frames_per_sdu * octets_per_frame;
		struct net_buf *buf;
		uint8_t *net_buffer;
		off_t offset = 0;

		buf = net_buf_alloc(&tx_pool, K_FOREVER);
		net_buf_reserve(buf, BT_ISO_CHAN_SEND_RESERVE);

		net_buffer = net_buf_tail(buf);
		buf->len += tx_sdu_len;

		for (int i = 0; i < frames_per_sdu; i++) {
			int lc3_ret;

			lc3_ret = lc3_encode(lc3_encoder, LC3_PCM_FORMAT_S16,
					     audio_buf, 1, octets_per_frame,
					     net_buffer + offset);
			offset += octets_per_frame;

			if (lc3_ret == -1) {
				printk("LC3 encoder failed - wrong parameters?: %d",
					lc3_ret);
				net_buf_unref(buf);
				return;
			}
		}

		for (size_t i = 0U; i < configured_sink_stream_count; i++) {
			struct bt_bap_stream *stream = &streams[i];
			struct net_buf *buf_to_send;
			int ret;

			/* Clone the buffer if sending on more than 1 stream */
			if (i == configured_sink_stream_count - 1) {
				buf_to_send = buf;
			} else {
				buf_to_send = net_buf_clone(buf, K_FOREVER);
			}

			ret = bt_bap_stream_send(stream, buf_to_send,
						   get_and_incr_seq_num(stream),
						   BT_ISO_TIMESTAMP_NONE);
			if (ret < 0) {
				printk("  Failed to send LC3 audio data on streams[%zu] (%d)\n",
				       i, ret);
				net_buf_unref(buf_to_send);
			} else {
				printk("  TX LC3 l on streams[%zu]: %zu\n",
				       tx_sdu_len, i);
				sdu_cnt++;
			}
		}
	}
}

static void init_lc3(void)
{
	unsigned int num_samples;

	freq_hz = bt_codec_cfg_get_freq(&codec_configuration.codec);
	frame_duration_us = bt_codec_cfg_get_frame_duration_us(&codec_configuration.codec);
	octets_per_frame = bt_codec_cfg_get_octets_per_frame(&codec_configuration.codec);
	frames_per_sdu = bt_codec_cfg_get_frame_blocks_per_sdu(&codec_configuration.codec, true);
	octets_per_frame = bt_codec_cfg_get_octets_per_frame(&codec_configuration.codec);

	if (freq_hz < 0) {
		printk("Error: Codec frequency not set, cannot start codec.");
		return;
	}

	if (frame_duration_us < 0) {
		printk("Error: Frame duration not set, cannot start codec.");
		return;
	}

	if (octets_per_frame < 0) {
		printk("Error: Octets per frame not set, cannot start codec.");
		return;
	}

	frame_duration_100us = frame_duration_us / 100;


	/* Fill audio buffer with Sine wave only once and repeat encoding the same tone frame */
	fill_audio_buf_sin(audio_buf, frame_duration_us, AUDIO_TONE_FREQUENCY_HZ, freq_hz);

	num_samples = ((frame_duration_us * freq_hz) / USEC_PER_SEC);
	for (unsigned int i = 0; i < num_samples; i++) {
		printk("%3i: %6i\n", i, audio_buf[i]);
	}

	/* Create the encoder instance. This shall complete before stream_started() is called. */
	lc3_encoder = lc3_setup_encoder(frame_duration_us,
					freq_hz,
					0, /* No resampling */
					&lc3_encoder_mem);

	if (lc3_encoder == NULL) {
		printk("ERROR: Failed to setup LC3 encoder - wrong parameters?\n");
	}
}

#else

#define init_lc3(...)

/**
 * @brief Send audio data on timeout
 *
 * This will send an increasing amount of audio data, starting from 1 octet.
 * The data is just mock data, and does not actually represent any audio.
 *
 * First iteration : 0x00
 * Second iteration: 0x00 0x01
 * Third iteration : 0x00 0x01 0x02
 *
 * And so on, until it wraps around the configured MTU (CONFIG_BT_ISO_TX_MTU)
 *
 * @param work Pointer to the work structure
 */
static void audio_timer_timeout(struct k_work *work)
{
	static uint8_t buf_data[CONFIG_BT_ISO_TX_MTU];
	static bool data_initialized;
	struct net_buf *buf;
	static size_t len_to_send = 1;

	if (!data_initialized) {
		/* TODO: Actually encode some audio data */
		for (int i = 0; i < ARRAY_SIZE(buf_data); i++) {
			buf_data[i] = (uint8_t)i;
		}

		data_initialized = true;
	}

	buf = net_buf_alloc(&tx_pool, K_FOREVER);
	net_buf_reserve(buf, BT_ISO_CHAN_SEND_RESERVE);
	net_buf_add_mem(buf, buf_data, len_to_send);

	/* We configured the sink streams to be first in `streams`, so that
	 * we can use `stream[i]` to select sink streams (i.e. streams with
	 * data going to the server)
	 */
	for (size_t i = 0U; i < configured_sink_stream_count; i++) {
		struct bt_bap_stream *stream = &streams[i];
		struct net_buf *buf_to_send;
		int ret;

		/* Clone the buffer if sending on more than 1 stream */
		if (i == configured_sink_stream_count - 1) {
			buf_to_send = buf;
		} else {
			buf_to_send = net_buf_clone(buf, K_FOREVER);
		}

		ret = bt_bap_stream_send(stream, buf_to_send,
					   get_and_incr_seq_num(stream),
					   BT_ISO_TIMESTAMP_NONE);
		if (ret < 0) {
			printk("Failed to send audio data on streams[%zu]: (%d)\n",
			       i, ret);
			net_buf_unref(buf_to_send);
		} else {
			printk("Sending mock data with len %zu on streams[%zu]\n",
			       len_to_send, i);
		}
	}

	k_work_schedule(&audio_send_work, K_USEC(AUDIO_DATA_TIMEOUT_US));

	len_to_send++;
	if (len_to_send > codec_configuration.qos.sdu) {
		len_to_send = 1;
	}
}

#endif

static void print_hex(const uint8_t *ptr, size_t len)
{
	while (len-- != 0) {
		printk("%02x", *ptr++);
	}
}

static void print_codec_capabilities(const struct bt_codec *codec)
{
	printk("codec 0x%02x cid 0x%04x vid 0x%04x count %u\n",
	       codec->id, codec->cid, codec->vid, codec->data_count);

	for (size_t i = 0; i < codec->data_count; i++) {
		printk("data #%zu: type 0x%02x len %u\n",
		       i, codec->data[i].data.type,
		       codec->data[i].data.data_len);
		print_hex(codec->data[i].data.data,
			  codec->data[i].data.data_len -
			  sizeof(codec->data[i].data.type));
		printk("\n");
	}

	for (size_t i = 0; i < codec->meta_count; i++) {
		printk("meta #%zu: type 0x%02x len %u\n",
		       i, codec->meta[i].data.type,
		       codec->meta[i].data.data_len);
		print_hex(codec->meta[i].data.data,
			  codec->meta[i].data.data_len -
			  sizeof(codec->meta[i].data.type));
		printk("\n");
	}
}

static bool check_audio_support_and_connect(struct bt_data *data,
					    void *user_data)
{
	struct net_buf_simple ascs_svc_data;
	bt_addr_le_t *addr = user_data;
	uint8_t announcement_type;
	uint32_t audio_contexts;
	struct bt_uuid *uuid;
	uint16_t uuid_val;
	uint8_t meta_len;
	size_t min_size;
	int err;

	printk("[AD]: %u data_len %u\n", data->type, data->data_len);

	if (data->type != BT_DATA_SVC_DATA16) {
		return true; /* Continue parsing to next AD data type */
	}

	if (data->data_len < sizeof(uuid_val)) {
		printk("AD invalid size %u\n", data->data_len);
		return true; /* Continue parsing to next AD data type */
	}

	net_buf_simple_init_with_data(&ascs_svc_data, (void *)data->data,
				      data->data_len);

	uuid_val = net_buf_simple_pull_le16(&ascs_svc_data);
	uuid = BT_UUID_DECLARE_16(sys_le16_to_cpu(uuid_val));
	if (bt_uuid_cmp(uuid, BT_UUID_ASCS) != 0) {
		/* We are looking for the ASCS service data */
		return true; /* Continue parsing to next AD data type */
	}

	min_size = sizeof(announcement_type) + sizeof(audio_contexts) + sizeof(meta_len);
	if (ascs_svc_data.len < min_size) {
		printk("AD invalid size %u\n", data->data_len);
		return false; /* Stop parsing */
	}

	announcement_type = net_buf_simple_pull_u8(&ascs_svc_data);
	audio_contexts = net_buf_simple_pull_le32(&ascs_svc_data);
	meta_len = net_buf_simple_pull_u8(&ascs_svc_data);

	err = bt_le_scan_stop();
	if (err != 0) {
		printk("Failed to stop scan: %d\n", err);
		return false; /* Stop parsing */
	}

	printk("Audio server found with type %u, contexts 0x%08x and meta_len %u; connecting\n",
	       announcement_type, audio_contexts, meta_len);

	err = bt_conn_le_create(addr, BT_CONN_LE_CREATE_CONN,
				BT_LE_CONN_PARAM_DEFAULT,
				&default_conn);
	if (err != 0) {
		printk("Create conn to failed (%u)\n", err);
		start_scan();
	}

	return false; /* Stop parsing */
}

static void device_found(const bt_addr_le_t *addr, int8_t rssi, uint8_t type,
			 struct net_buf_simple *ad)
{
	char addr_str[BT_ADDR_LE_STR_LEN];

	if (default_conn != NULL) {
		/* Already connected */
		return;
	}

	/* We're only interested in connectable events */
	if (type != BT_GAP_ADV_TYPE_ADV_IND &&
	    type != BT_GAP_ADV_TYPE_ADV_DIRECT_IND &&
	    type != BT_GAP_ADV_TYPE_EXT_ADV) {
		return;
	}

	(void)bt_addr_le_to_str(addr, addr_str, sizeof(addr_str));
	printk("Device found: %s (RSSI %d)\n", addr_str, rssi);

	/* connect only to devices in close proximity */
	if (rssi < -70) {
		return;
	}

	bt_data_parse(ad, check_audio_support_and_connect, (void *)addr);
}

static void start_scan(void)
{
	int err;

	/* This demo doesn't require active scan */
	err = bt_le_scan_start(BT_LE_SCAN_PASSIVE, device_found);
	if (err != 0) {
		printk("Scanning failed to start (err %d)\n", err);
		return;
	}

	printk("Scanning successfully started\n");
}

static void stream_configured(struct bt_bap_stream *stream,
			      const struct bt_codec_qos_pref *pref)
{
	printk("Audio Stream %p configured\n", stream);

	k_sem_give(&sem_stream_configured);
}

static void stream_qos_set(struct bt_bap_stream *stream)
{
	printk("Audio Stream %p QoS set\n", stream);

	k_sem_give(&sem_stream_qos);
}

static void stream_enabled(struct bt_bap_stream *stream)
{
	printk("Audio Stream %p enabled\n", stream);

	k_sem_give(&sem_stream_enabled);
}

static void stream_started(struct bt_bap_stream *stream)
{
	printk("Audio Stream %p started\n", stream);

	/* Reset sequence number for sinks */
	for (size_t i = 0U; i < configured_sink_stream_count; i++) {
		if (stream->ep == sinks[i].ep) {
			sinks[i].seq_num = 0U;
			break;
		}
	}

	k_sem_give(&sem_stream_started);
}

static void stream_metadata_updated(struct bt_bap_stream *stream)
{
	printk("Audio Stream %p metadata updated\n", stream);
}

static void stream_disabled(struct bt_bap_stream *stream)
{
	printk("Audio Stream %p disabled\n", stream);
}

static void stream_stopped(struct bt_bap_stream *stream, uint8_t reason)
{
	printk("Audio Stream %p stopped with reason 0x%02X\n", stream, reason);

	/* Stop send timer */
	k_work_cancel_delayable(&audio_send_work);
}

static void stream_released(struct bt_bap_stream *stream)
{
	printk("Audio Stream %p released\n", stream);
}

static void stream_recv(struct bt_bap_stream *stream,
			const struct bt_iso_recv_info *info,
			struct net_buf *buf)
{
	if (info->flags & BT_ISO_FLAGS_VALID) {
		printk("Incoming audio on stream %p len %u\n", stream, buf->len);
	}
}

static struct bt_bap_stream_ops stream_ops = {
	.configured = stream_configured,
	.qos_set = stream_qos_set,
	.enabled = stream_enabled,
	.started = stream_started,
	.metadata_updated = stream_metadata_updated,
	.disabled = stream_disabled,
	.stopped = stream_stopped,
	.released = stream_released,
	.recv = stream_recv
};

static void add_remote_source(struct bt_bap_ep *ep)
{
	for (size_t i = 0U; i < ARRAY_SIZE(sources); i++) {
		if (sources[i] == NULL) {
			printk("Source #%zu: ep %p\n", i, ep);
			sources[i] = ep;
			return;
		}
	}

	printk("Could not add source ep\n");
}

static void add_remote_sink(struct bt_bap_ep *ep)
{
	for (size_t i = 0U; i < ARRAY_SIZE(sinks); i++) {
		if (sinks[i].ep == NULL) {
			printk("Sink #%zu: ep %p\n", i, ep);
			sinks[i].ep = ep;
			return;
		}
	}

	printk("Could not add sink ep\n");
}

static void print_remote_codec(const struct bt_codec *codec_capabilities, enum bt_audio_dir dir)
{
	printk("codec_capabilities %p dir 0x%02x\n", codec_capabilities, dir);

	print_codec_capabilities(codec_capabilities);
}

static void discover_sinks_cb(struct bt_conn *conn, int err, enum bt_audio_dir dir)
{
	if (err != 0 && err != BT_ATT_ERR_ATTRIBUTE_NOT_FOUND) {
		printk("Discovery failed: %d\n", err);
		return;
	}

	if (err == BT_ATT_ERR_ATTRIBUTE_NOT_FOUND) {
		printk("Discover sinks completed without finding any sink ASEs\n");
	} else {
		printk("Discover sinks complete: err %d\n", err);
	}

	k_sem_give(&sem_sinks_discovered);
}

static void discover_sources_cb(struct bt_conn *conn, int err, enum bt_audio_dir dir)
{
	if (err != 0 && err != BT_ATT_ERR_ATTRIBUTE_NOT_FOUND) {
		printk("Discovery failed: %d\n", err);
		return;
	}

	if (err == BT_ATT_ERR_ATTRIBUTE_NOT_FOUND) {
		printk("Discover sinks completed without finding any source ASEs\n");
	} else {
		printk("Discover sources complete: err %d\n", err);
	}

	k_sem_give(&sem_sources_discovered);
}

static void connected(struct bt_conn *conn, uint8_t err)
{
	char addr[BT_ADDR_LE_STR_LEN];

	(void)bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	if (err != 0) {
		printk("Failed to connect to %s (%u)\n", addr, err);

		bt_conn_unref(default_conn);
		default_conn = NULL;

		start_scan();
		return;
	}

	if (conn != default_conn) {
		return;
	}

	printk("Connected: %s\n", addr);
	k_sem_give(&sem_connected);
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	char addr[BT_ADDR_LE_STR_LEN];

	if (conn != default_conn) {
		return;
	}

	(void)bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	printk("Disconnected: %s (reason 0x%02x)\n", addr, reason);

	bt_conn_unref(default_conn);
	default_conn = NULL;

	k_sem_give(&sem_disconnected);
}

static void security_changed_cb(struct bt_conn *conn, bt_security_t level,
				enum bt_security_err err)
{
	if (err == 0) {
		k_sem_give(&sem_security_updated);
	} else {
		printk("Failed to set security level: %u", err);
	}
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
	.connected = connected,
	.disconnected = disconnected,
	.security_changed = security_changed_cb
};

static void att_mtu_updated(struct bt_conn *conn, uint16_t tx, uint16_t rx)
{
	printk("MTU exchanged: %u/%u\n", tx, rx);
	k_sem_give(&sem_mtu_exchanged);
}

static struct bt_gatt_cb gatt_callbacks = {
	.att_mtu_updated = att_mtu_updated,
};

static void unicast_client_location_cb(struct bt_conn *conn,
				      enum bt_audio_dir dir,
				      enum bt_audio_location loc)
{
	printk("dir %u loc %X\n", dir, loc);
}

static void available_contexts_cb(struct bt_conn *conn,
				  enum bt_audio_context snk_ctx,
				  enum bt_audio_context src_ctx)
{
	printk("snk ctx %u src ctx %u\n", snk_ctx, src_ctx);
}

static void pac_record_cb(struct bt_conn *conn, enum bt_audio_dir dir, const struct bt_codec *codec)
{
	print_remote_codec(codec, dir);
}

static void endpoint_cb(struct bt_conn *conn, enum bt_audio_dir dir, struct bt_bap_ep *ep)
{
	if (dir == BT_AUDIO_DIR_SOURCE) {
		add_remote_source(ep);
	} else if (dir == BT_AUDIO_DIR_SINK) {
		add_remote_sink(ep);
	}
}

static struct bt_bap_unicast_client_cb unicast_client_cbs = {
	.location = unicast_client_location_cb,
	.available_contexts = available_contexts_cb,
	.pac_record = pac_record_cb,
	.endpoint = endpoint_cb,
};

static int init(void)
{
	int err;

	err = bt_enable(NULL);
	if (err != 0) {
		printk("Bluetooth enable failed (err %d)\n", err);
		return err;
	}

	for (size_t i = 0; i < ARRAY_SIZE(streams); i++) {
		streams[i].ops = &stream_ops;
	}

	bt_gatt_cb_register(&gatt_callbacks);

#if defined(CONFIG_LIBLC3)
	k_work_init_delayable(&audio_send_work, lc3_audio_timer_timeout);
#else
	k_work_init_delayable(&audio_send_work, audio_timer_timeout);
#endif

	return 0;
}

static int scan_and_connect(void)
{
	int err;

	start_scan();

	err = k_sem_take(&sem_connected, K_FOREVER);
	if (err != 0) {
		printk("failed to take sem_connected (err %d)\n", err);
		return err;
	}

	err = k_sem_take(&sem_mtu_exchanged, K_FOREVER);
	if (err != 0) {
		printk("failed to take sem_mtu_exchanged (err %d)\n", err);
		return err;
	}

	err = bt_conn_set_security(default_conn, BT_SECURITY_L2);
	if (err != 0) {
		printk("failed to set security (err %d)\n", err);
		return err;
	}

	err = k_sem_take(&sem_security_updated, K_FOREVER);
	if (err != 0) {
		printk("failed to take sem_security_updated (err %d)\n", err);
		return err;
	}

	return 0;
}

static int discover_sinks(void)
{
	int err;

	unicast_client_cbs.discover = discover_sinks_cb;

	err = bt_bap_unicast_client_discover(default_conn, BT_AUDIO_DIR_SINK);
	if (err != 0) {
		printk("Failed to discover sinks: %d\n", err);
		return err;
	}

	err = k_sem_take(&sem_sinks_discovered, K_FOREVER);
	if (err != 0) {
		printk("failed to take sem_sinks_discovered (err %d)\n", err);
		return err;
	}

	return 0;
}

static int discover_sources(void)
{
	int err;

	unicast_client_cbs.discover = discover_sources_cb;

	err = bt_bap_unicast_client_discover(default_conn, BT_AUDIO_DIR_SOURCE);
	if (err != 0) {
		printk("Failed to discover sources: %d\n", err);
		return err;
	}

	err = k_sem_take(&sem_sources_discovered, K_FOREVER);
	if (err != 0) {
		printk("failed to take sem_sources_discovered (err %d)\n", err);
		return err;
	}

	return 0;
}

static int configure_stream(struct bt_bap_stream *stream, struct bt_bap_ep *ep)
{
	int err;

	err = bt_bap_stream_config(default_conn, stream, ep,
				     &codec_configuration.codec);
	if (err != 0) {
		return err;
	}

	err = k_sem_take(&sem_stream_configured, K_FOREVER);
	if (err != 0) {
		printk("failed to take sem_stream_configured (err %d)\n", err);
		return err;
	}

	return 0;
}

static int configure_streams(void)
{
	int err;

	for (size_t i = 0; i < ARRAY_SIZE(sinks); i++) {
		struct bt_bap_ep *ep = sinks[i].ep;
		struct bt_bap_stream *stream = &streams[i];

		if (ep == NULL) {
			continue;
		}

		err = configure_stream(stream, ep);
		if (err != 0) {
			printk("Could not configure sink stream[%zu]: %d\n",
			       i, err);
			return err;
		}

		printk("Configured sink stream[%zu]\n", i);
		configured_sink_stream_count++;
	}

	for (size_t i = 0; i < ARRAY_SIZE(sources); i++) {
		struct bt_bap_ep *ep = sources[i];
		struct bt_bap_stream *stream = &streams[i + configured_sink_stream_count];

		if (ep == NULL) {
			continue;
		}

		err = configure_stream(stream, ep);
		if (err != 0) {
			printk("Could not configure source stream[%zu]: %d\n",
			       i, err);
			return err;
		}

		printk("Configured source stream[%zu]\n", i);
		configured_source_stream_count++;
	}

	return 0;
}

static int create_group(void)
{
	const size_t params_count = MAX(configured_sink_stream_count,
					configured_source_stream_count);
	struct bt_bap_unicast_group_stream_pair_param pair_params[params_count];
	struct bt_bap_unicast_group_stream_param stream_params[configured_stream_count];
	struct bt_bap_unicast_group_param param;
	int err;

	for (size_t i = 0U; i < configured_stream_count; i++) {
		stream_params[i].stream = &streams[i];
		stream_params[i].qos = &codec_configuration.qos;
	}

	for (size_t i = 0U; i < params_count; i++) {
		if (i < configured_sink_stream_count) {
			pair_params[i].tx_param = &stream_params[i];
		} else {
			pair_params[i].tx_param = NULL;
		}

		if (i < configured_source_stream_count) {
			pair_params[i].rx_param = &stream_params[i + configured_sink_stream_count];
		} else {
			pair_params[i].rx_param = NULL;
		}
	}

	param.params = pair_params;
	param.params_count = params_count;
	param.packing = BT_ISO_PACKING_SEQUENTIAL;

	err = bt_bap_unicast_group_create(&param, &unicast_group);
	if (err != 0) {
		printk("Could not create unicast group (err %d)\n", err);
		return err;
	}

	return 0;
}

static int delete_group(void)
{
	int err;

	err = bt_bap_unicast_group_delete(unicast_group);
	if (err != 0) {
		printk("Could not create unicast group (err %d)\n", err);
		return err;
	}

	return 0;
}

static int set_stream_qos(void)
{
	int err;

	err = bt_bap_stream_qos(default_conn, unicast_group);
	if (err != 0) {
		printk("Unable to setup QoS: %d\n", err);
		return err;
	}

	for (size_t i = 0U; i < configured_stream_count; i++) {
		printk("QoS: waiting for %zu streams\n", configured_stream_count);
		k_sem_take(&sem_stream_qos, K_FOREVER);
	}

	return 0;
}

static int enable_streams(void)
{
	if (IS_ENABLED(CONFIG_LIBLC3)) {
		init_lc3();
	}

	for (size_t i = 0U; i < configured_stream_count; i++) {
		int err;

		err = bt_bap_stream_enable(&streams[i],
					     codec_configuration.codec.meta,
					     codec_configuration.codec.meta_count);
		if (err != 0) {
			printk("Unable to enable stream: %d\n", err);
			return err;
		}

		err = k_sem_take(&sem_stream_enabled, K_FOREVER);
		if (err != 0) {
			printk("failed to take sem_stream_enabled (err %d)\n", err);
			return err;
		}
	}

	return 0;
}

static int start_streams(void)
{
	for (size_t i = 0U; i < configured_stream_count; i++) {
		int err;

		err = bt_bap_stream_start(&streams[i]);
		if (err != 0) {
			printk("Unable to start stream: %d\n", err);
			return err;
		}

		err = k_sem_take(&sem_stream_started, K_FOREVER);
		if (err != 0) {
			printk("failed to take sem_stream_started (err %d)\n", err);
			return err;
		}
	}

	return 0;
}

static void reset_data(void)
{
	k_sem_reset(&sem_connected);
	k_sem_reset(&sem_disconnected);
	k_sem_reset(&sem_mtu_exchanged);
	k_sem_reset(&sem_security_updated);
	k_sem_reset(&sem_sinks_discovered);
	k_sem_reset(&sem_sources_discovered);
	k_sem_reset(&sem_stream_configured);
	k_sem_reset(&sem_stream_qos);
	k_sem_reset(&sem_stream_enabled);
	k_sem_reset(&sem_stream_started);

	configured_sink_stream_count = 0;
	configured_source_stream_count = 0;
	memset(sinks, 0, sizeof(sinks));
	memset(sources, 0, sizeof(sources));
}

int main(void)
{
	int err;

	printk("Initializing\n");
	err = init();
	if (err != 0) {
		return 0;
	}
	printk("Initialized\n");

	err = bt_bap_unicast_client_register_cb(&unicast_client_cbs);
	if (err != 0) {
		printk("Failed to register client callbacks: %d", err);
		return 0;
	}

	while (true) {
		reset_data();

		printk("Waiting for connection\n");
		err = scan_and_connect();
		if (err != 0) {
			return 0;
		}
		printk("Connected\n");

		printk("Discovering sinks\n");
		err = discover_sinks();
		if (err != 0) {
			return 0;
		}
		printk("Sinks discovered\n");

		printk("Discovering sources\n");
		err = discover_sources();
		if (err != 0) {
			return 0;
		}
		printk("Sources discovered\n");

		printk("Configuring streams\n");
		err = configure_streams();
		if (err != 0) {
			return 0;
		}

		if (configured_stream_count == 0U) {
			printk("No streams were configured\n");
			return 0;
		}

		printk("Creating unicast group\n");
		err = create_group();
		if (err != 0) {
			return 0;
		}
		printk("Unicast group created\n");

		printk("Setting stream QoS\n");
		err = set_stream_qos();
		if (err != 0) {
			return 0;
		}
		printk("Stream QoS Set\n");

		printk("Enabling streams\n");
		err = enable_streams();
		if (err != 0) {
			return 0;
		}
		printk("Streams enabled\n");

		printk("Starting streams\n");
		err = start_streams();
		if (err != 0) {
			return 0;
		}
		printk("Streams started\n");

		if (CONFIG_BT_BAP_UNICAST_CLIENT_ASE_SRC_COUNT > 0) {
			/* Start send timer */
			k_work_schedule(&audio_send_work, K_MSEC(0));
		}

		/* Wait for disconnect */
		err = k_sem_take(&sem_disconnected, K_FOREVER);
		if (err != 0) {
			printk("failed to take sem_disconnected (err %d)\n", err);
			return 0;
		}

		printk("Deleting group\n");
		err = delete_group();
		if (err != 0) {
			return 0;
		}
		printk("Group deleted\n");
	}
}
