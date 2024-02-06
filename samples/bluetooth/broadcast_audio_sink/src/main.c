/*
 * Copyright (c) 2022-2023 Nordic Semiconductor ASA
 * Copyright (c) 2024 Demant A/S
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <ctype.h>
#include <strings.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/audio/audio.h>
#include <zephyr/bluetooth/audio/bap.h>
#include <zephyr/bluetooth/audio/pacs.h>
#include <zephyr/sys/byteorder.h>
#if defined(CONFIG_LIBLC3)
#include "lc3.h"
#endif /* defined(CONFIG_LIBLC3) */
#if defined(CONFIG_USB_DEVICE_AUDIO)
#include <zephyr/usb/usb_device.h>
#include <zephyr/usb/class/usb_audio.h>
#include <zephyr/sys/ring_buffer.h>
#endif /* defined(CONFIG_USB_DEVICE_AUDIO) */


BUILD_ASSERT(IS_ENABLED(CONFIG_SCAN_SELF) || IS_ENABLED(CONFIG_SCAN_OFFLOAD),
	     "Either SCAN_SELF or SCAN_OFFLOAD must be enabled");

#define SEM_TIMEOUT K_SECONDS(10)
#define BROADCAST_ASSISTANT_TIMEOUT K_SECONDS(120) /* 2 minutes */

#if defined(CONFIG_SCAN_SELF)
#define ADV_TIMEOUT K_SECONDS(CONFIG_SCAN_DELAY)
#else /* !CONFIG_SCAN_SELF */
#define ADV_TIMEOUT K_FOREVER
#endif /* CONFIG_SCAN_SELF */

#define INVALID_BROADCAST_ID      (BT_AUDIO_BROADCAST_ID_MAX + 1)
#define SYNC_RETRY_COUNT          6 /* similar to retries for connections */
#define PA_SYNC_SKIP              5
#define NAME_LEN                  sizeof(CONFIG_TARGET_BROADCAST_NAME) + 1

#if defined(CONFIG_LIBLC3)
#define MAX_SAMPLE_RATE         48000U
#define MAX_FRAME_DURATION_US   10000U
#define MAX_NUM_SAMPLES_MONO    ((MAX_FRAME_DURATION_US * MAX_SAMPLE_RATE) / USEC_PER_SEC)
#define MAX_NUM_SAMPLES_STEREO  (MAX_NUM_SAMPLES_MONO * 2)

#define LC3_ENCODER_STACK_SIZE  4096
#define LC3_ENCODER_PRIORITY    5
#endif /* defined(CONFIG_LIBLC3) */

#if defined(CONFIG_USB_DEVICE_AUDIO)
#define USB_SAMPLE_RATE	             48000U
#define USB_FRAME_DURATION_US        1000U
#define USB_TX_BUF_NUM               10U
#define BROADCAST_DATA_ELEMENT_SIZE  sizeof(int16_t)
#define BROADCAST_MONO_SAMPLE_SIZE   (MAX_NUM_SAMPLES_MONO * BROADCAST_DATA_ELEMENT_SIZE)
#define BROADCAST_STEREO_SAMPLE_SIZE (BROADCAST_MONO_SAMPLE_SIZE * BROADCAST_DATA_ELEMENT_SIZE)
#define USB_STEREO_SAMPLE_SIZE       ((USB_FRAME_DURATION_US * USB_SAMPLE_RATE * \
				      BROADCAST_DATA_ELEMENT_SIZE * 2) / USEC_PER_SEC)
#define AUDIO_RING_BUF_SIZE          10000U
#endif /* defined(CONFIG_USB_DEVICE_AUDIO) */

static K_SEM_DEFINE(sem_connected, 0U, 1U);
static K_SEM_DEFINE(sem_disconnected, 0U, 1U);
static K_SEM_DEFINE(sem_broadcaster_found, 0U, 1U);
static K_SEM_DEFINE(sem_pa_synced, 0U, 1U);
static K_SEM_DEFINE(sem_base_received, 0U, 1U);
static K_SEM_DEFINE(sem_syncable, 0U, 1U);
static K_SEM_DEFINE(sem_pa_sync_lost, 0U, 1U);
static K_SEM_DEFINE(sem_broadcast_code_received, 0U, 1U);
static K_SEM_DEFINE(sem_pa_request, 0U, 1U);
static K_SEM_DEFINE(sem_past_request, 0U, 1U);
static K_SEM_DEFINE(sem_bis_sync_requested, 0U, 1U);
static K_SEM_DEFINE(sem_bis_synced, 0U, CONFIG_BT_BAP_BROADCAST_SNK_STREAM_COUNT);

/* Sample assumes that we only have a single Scan Delegator receive state */
static const struct bt_bap_scan_delegator_recv_state *req_recv_state;
static struct bt_bap_broadcast_sink *broadcast_sink;
static struct bt_le_scan_recv_info broadcaster_info;
static bt_addr_le_t broadcaster_addr;
static struct bt_le_per_adv_sync *pa_sync;
static uint32_t broadcaster_broadcast_id;
static struct broadcast_sink_stream {
	struct bt_bap_stream stream;
	bool has_data;
	size_t recv_cnt;
	size_t loss_cnt;
	size_t error_cnt;
	size_t valid_cnt;
#if defined(CONFIG_LIBLC3)
	struct net_buf *in_buf;
	struct k_work_delayable lc3_decode_work;
/* Internal lock for protecting net_buf from multiple access */
	struct k_mutex lc3_decoder_mutex;
	lc3_decoder_t lc3_decoder;
	lc3_decoder_mem_48k_t lc3_decoder_mem;
#endif /* defined(CONFIG_LIBLC3) */
#if defined(CONFIG_USB_DEVICE_AUDIO)
	struct ring_buf audio_ring_buf;
	uint8_t _ring_buffer[AUDIO_RING_BUF_SIZE];
#endif /* defined(CONFIG_USB_DEVICE_AUDIO) */

} streams[CONFIG_BT_BAP_BROADCAST_SNK_STREAM_COUNT];
static struct bt_bap_stream *streams_p[ARRAY_SIZE(streams)];
static struct bt_conn *broadcast_assistant_conn;
static struct bt_le_ext_adv *ext_adv;

static const struct bt_audio_codec_cap codec_cap = BT_AUDIO_CODEC_CAP_LC3(
	BT_AUDIO_CODEC_LC3_FREQ_16KHZ | BT_AUDIO_CODEC_LC3_FREQ_24KHZ,
	BT_AUDIO_CODEC_LC3_DURATION_10, BT_AUDIO_CODEC_LC3_CHAN_COUNT_SUPPORT(1), 40u, 60u, 1u,
	(BT_AUDIO_CONTEXT_TYPE_CONVERSATIONAL | BT_AUDIO_CONTEXT_TYPE_MEDIA));

/* Create a mask for the maximum BIS we can sync to using the number of streams
 * we have. We add an additional 1 since the bis indexes start from 1 and not
 * 0.
 */
static const uint32_t bis_index_mask = BIT_MASK(ARRAY_SIZE(streams) + 1U);
static uint32_t requested_bis_sync;
static uint32_t bis_index_bitfield;
static uint8_t sink_broadcast_code[BT_AUDIO_BROADCAST_CODE_SIZE];

uint64_t total_rx_iso_packet_count; /* This value is exposed to test code */

#if defined(CONFIG_USB_DEVICE_AUDIO)
static int16_t usb_audio_data[MAX_NUM_SAMPLES_STEREO] = {0};
static int16_t usb_audio_data_stereo[MAX_NUM_SAMPLES_STEREO] = {0};

RING_BUF_DECLARE(ring_buf_usb, AUDIO_RING_BUF_SIZE);
NET_BUF_POOL_DEFINE(usb_tx_buf_pool, USB_TX_BUF_NUM, BROADCAST_STEREO_SAMPLE_SIZE, 0,
		    net_buf_destroy);

static void mix_mono_to_stereo(enum bt_audio_location channels);
#endif /* defined(CONFIG_USB_DEVICE_AUDIO) */

#if defined(CONFIG_LIBLC3)
static int16_t audio_buf[MAX_NUM_SAMPLES_MONO];
static int frames_per_sdu;
static K_SEM_DEFINE(lc3_decoder_sem, 0, 1);

static void do_lc3_decode(struct broadcast_sink_stream *sink_stream);
static void lc3_decoder_thread(void *arg1, void *arg2, void *arg3);
K_THREAD_DEFINE(decoder_tid, LC3_ENCODER_STACK_SIZE, lc3_decoder_thread,
		NULL, NULL, NULL, LC3_ENCODER_PRIORITY, 0, -1);

/* Consumer thread of the decoded stream data */
static void lc3_decoder_thread(void *arg1, void *arg2, void *arg3)
{
	while (true) {
		k_sem_take(&lc3_decoder_sem, K_FOREVER);
#if defined(CONFIG_USB_DEVICE_AUDIO)
		int err = 0;
		enum bt_audio_location channels;
		struct broadcast_sink_stream *stream_for_usb = &streams[0];

		/* For now we only handle one BIS, so always only decode the first element in
		 * streams.
		 */
		do_lc3_decode(&streams[0]);

		err = bt_audio_codec_cfg_get_chan_allocation(stream_for_usb->stream.codec_cfg,
							     &channels);
		if (err != 0) {
			printk("Could not get channel allocation (err=%d)\n", err);
			continue;
		}

		/* If the ring buffer usage is larger than zero, then there is data to process */
		if (ring_buf_space_get(&stream_for_usb->audio_ring_buf)) {
			mix_mono_to_stereo(channels);
		}
#else
	for (size_t i = 0; i < ARRAY_SIZE(streams); i++) {
		if (streams[i].has_data) {
			do_lc3_decode(&streams[i]);
		}
	}

#endif /* #if defined(CONFIG_USB_DEVICE_AUDIO) */
	}
}

static void do_lc3_decode(struct broadcast_sink_stream *sink_stream)
{
	int err = 0;
	int offset = 0;
	uint8_t *buf_data;
	struct net_buf *ptr_net_buf;
	int octets_per_frame;

	k_mutex_lock(&sink_stream->lc3_decoder_mutex, K_FOREVER);

	sink_stream->has_data = false;

	if (sink_stream->in_buf == NULL) {
		k_mutex_unlock(&sink_stream->lc3_decoder_mutex);
		return;
	}

	ptr_net_buf = net_buf_ref(sink_stream->in_buf);
	net_buf_unref(sink_stream->in_buf);
	sink_stream->in_buf = NULL;
	k_mutex_unlock(&sink_stream->lc3_decoder_mutex);

	buf_data = ptr_net_buf->data;
	octets_per_frame = ptr_net_buf->len / frames_per_sdu;

	for (int i = 0; i < frames_per_sdu; i++) {
		err = lc3_decode(sink_stream->lc3_decoder, buf_data + offset, octets_per_frame,
				 LC3_PCM_FORMAT_S16, audio_buf, 1);

		if (err == 1) {
			printk("  decoder performed PLC\n");
		} else if (err < 0) {
			printk("  decoder failed - wrong parameters? (err = %d)\n", err);
		}

		offset += octets_per_frame;
	}

	net_buf_unref(ptr_net_buf);

#if defined(CONFIG_USB_DEVICE_AUDIO)
	uint32_t rbret;

	if (ring_buf_space_get(&sink_stream->audio_ring_buf) == 0) {
		/* Since the data in the buffer is old by now, and we add enough data for many
		 * request to consume at a time, just erase what is already in the buffer.
		 */
		ring_buf_reset(&sink_stream->audio_ring_buf);
	}

	/* Put in ring-buffer to be consumed */
	rbret = ring_buf_put(&sink_stream->audio_ring_buf, (uint8_t *)audio_buf,
			     BROADCAST_MONO_SAMPLE_SIZE);
	if (rbret != BROADCAST_MONO_SAMPLE_SIZE) {
		static int rb_add_failures;

		rb_add_failures++;
		if (rb_add_failures % 1000 == 0) {
			printk("Failure to add to ring buffer %d, %u\n", rb_add_failures, rbret);
		}
		return;
	}
#endif /*#if defined(CONFIG_USB_DEVICE_AUDIO)*/
}

static int lc3_enable(struct broadcast_sink_stream *sink_stream)
{
	int ret;
	int freq_hz;
	int frame_duration_us;

	printk("Enable: stream with codec %p\n", sink_stream->stream.codec_cfg);

	ret = bt_audio_codec_cfg_get_freq(sink_stream->stream.codec_cfg);
	if (ret > 0) {
		freq_hz = bt_audio_codec_cfg_freq_to_freq_hz(ret);
	} else {
		printk("Error: Codec frequency not set, cannot start codec.");
		return -1;
	}

	ret = bt_audio_codec_cfg_get_frame_dur(sink_stream->stream.codec_cfg);
	if (ret > 0) {
		frame_duration_us = bt_audio_codec_cfg_frame_dur_to_frame_dur_us(ret);
	} else {
		printk("Error: Frame duration not set, cannot start codec.");
		return ret;
	}

	frames_per_sdu = bt_audio_codec_cfg_get_frame_blocks_per_sdu(sink_stream->stream.codec_cfg,
								     true);

#if defined(CONFIG_USB_DEVICE_AUDIO)
	sink_stream->lc3_decoder = lc3_setup_decoder(frame_duration_us, freq_hz, USB_SAMPLE_RATE,
						     &sink_stream->lc3_decoder_mem);
#else
	sink_stream->lc3_decoder = lc3_setup_decoder(frame_duration_us, freq_hz, 0,
						     &sink_stream->lc3_decoder_mem);
#endif /* defined(CONFIG_USB_DEVICE_AUDIO) */

	if (sink_stream->lc3_decoder == NULL) {
		printk("ERROR: Failed to setup LC3 decoder - wrong parameters?\n");
		return -1;
	}

	k_thread_start(decoder_tid);

	return 0;
}
#endif /* defined(CONFIG_LIBLC3) */

#if defined(CONFIG_USB_DEVICE_AUDIO)
static uint8_t get_channel_index(const enum bt_audio_location allocated_channels,
				 const enum bt_audio_location channel)
{
	/* If we are looking for the right channel, and left channel is present, then the index is
	 * 1. For all other combinations the index has to be 0, since it would mean that it is the
	 * lowest possible bit enumeration
	 */
	if (channel == BT_AUDIO_LOCATION_FRONT_RIGHT &&
	    allocated_channels & BT_AUDIO_LOCATION_FRONT_LEFT) {
		return 1;
	}

	return 0;
}

/* Duplicate the audio from one channel and put it in both channels */
static void mix_mono_to_stereo(enum bt_audio_location channels)
{
	uint32_t size;
	uint8_t cidx;

	size = ring_buf_get(&streams[0].audio_ring_buf, (uint8_t *)usb_audio_data,
			    MAX_NUM_SAMPLES_STEREO);
	if (size != MAX_NUM_SAMPLES_STEREO) {
		memset(&((uint8_t *)usb_audio_data)[size], 0, sizeof(usb_audio_data) - size);
	}

	cidx = get_channel_index(channels, CONFIG_TARGET_BROADCAST_CHANNEL);

	/* Interleave the channel sample */
	for (size_t i = 0U; i < MAX_NUM_SAMPLES_MONO; i++) {
		usb_audio_data_stereo[i * 2] = usb_audio_data[MAX_NUM_SAMPLES_MONO * cidx + i];
		usb_audio_data_stereo[i * 2 + 1] = usb_audio_data[MAX_NUM_SAMPLES_MONO * cidx + i];
	}

	size = ring_buf_put(&ring_buf_usb, (uint8_t *)usb_audio_data_stereo,
			    BROADCAST_STEREO_SAMPLE_SIZE);
	if (size != BROADCAST_STEREO_SAMPLE_SIZE) {
		static int rb_put_failures;

		rb_put_failures++;
		if (rb_put_failures == 1000) {
			printk("%s: Failure to add to ring buffer %d, %u\n", __func__,
			       rb_put_failures, size);
			rb_put_failures = 0;
		}
	}
}

/* USB consumer callback, called every 1ms, consumes data from ring-buffer */
static void data_request(const struct device *dev)
{
	static struct net_buf *pcm_buf;
	int err;
	uint32_t size;
	void *out;
	int16_t usb_audio_data[USB_STEREO_SAMPLE_SIZE] = {0};

	size = ring_buf_get(&ring_buf_usb, (uint8_t *)usb_audio_data, USB_STEREO_SAMPLE_SIZE);
	if (size != USB_STEREO_SAMPLE_SIZE) {
		memset(&((uint8_t *)usb_audio_data)[size], 0, USB_STEREO_SAMPLE_SIZE);
	}

	pcm_buf = net_buf_alloc(&usb_tx_buf_pool, K_NO_WAIT);
	if (pcm_buf == NULL) {
		printk("Couldnt allocate pcm_buf\n");
		return;
	}

	out = net_buf_add(pcm_buf, USB_STEREO_SAMPLE_SIZE);
	memcpy(out, usb_audio_data, USB_STEREO_SAMPLE_SIZE);

	err = usb_audio_send(dev, pcm_buf, USB_STEREO_SAMPLE_SIZE);
	if (err) {
		net_buf_unref(pcm_buf);
	}
}

static void data_written(const struct device *dev, struct net_buf *buf, size_t size)
{
	/* Unreference the buffer now that the USB is done with it */
	net_buf_unref(buf);
}

static const struct usb_audio_ops ops = {
	.data_request_cb = data_request,
	.data_written_cb = data_written,
};
#endif /* defined(CONFIG_USB_DEVICE_AUDIO) */

static void stream_started_cb(struct bt_bap_stream *stream)
{
	struct broadcast_sink_stream *sink_stream =
		CONTAINER_OF(stream, struct broadcast_sink_stream, stream);

	printk("Stream %p started\n", stream);

	total_rx_iso_packet_count = 0U;
	sink_stream->recv_cnt = 0U;
	sink_stream->loss_cnt = 0U;
	sink_stream->valid_cnt = 0U;
	sink_stream->error_cnt = 0U;


#if defined(CONFIG_LIBLC3)
	int err;

	if (stream->codec_cfg != 0 && stream->codec_cfg->id != BT_HCI_CODING_FORMAT_LC3) {
		/* No subgroups with LC3 was found */
		printk("Did not parse an LC3 codec\n");
		return;
	}

	err = lc3_enable(sink_stream);
	if (err < 0) {
		printk("Error: cannot enable LC3 codec: %d", err);
		return;
	}
#endif /* CONFIG_LIBLC3 */

	k_sem_give(&sem_bis_synced);
}

static void stream_stopped_cb(struct bt_bap_stream *stream, uint8_t reason)
{
	int err;

	printk("Stream %p stopped with reason 0x%02X\n", stream, reason);

	err = k_sem_take(&sem_bis_synced, K_NO_WAIT);
	if (err != 0) {
		printk("Failed to take sem_bis_synced: %d\n", err);
	}
}

static void stream_recv_cb(struct bt_bap_stream *stream, const struct bt_iso_recv_info *info,
			   struct net_buf *buf)
{
	struct broadcast_sink_stream *sink_stream =
		CONTAINER_OF(stream, struct broadcast_sink_stream, stream);

	if (info->flags & BT_ISO_FLAGS_ERROR) {
		sink_stream->error_cnt++;
	}

	if (info->flags & BT_ISO_FLAGS_LOST) {
		sink_stream->loss_cnt++;
	}

	if (info->flags & BT_ISO_FLAGS_VALID) {
		sink_stream->valid_cnt++;
#if defined(CONFIG_LIBLC3)
		k_mutex_lock(&sink_stream->lc3_decoder_mutex, K_FOREVER);
		if (sink_stream->in_buf != NULL) {
			net_buf_unref(sink_stream->in_buf);
			sink_stream->in_buf = NULL;
		}

		sink_stream->in_buf = net_buf_ref(buf);
		k_mutex_unlock(&sink_stream->lc3_decoder_mutex);
		sink_stream->has_data = true;
		k_sem_give(&lc3_decoder_sem);
#endif /* defined(CONFIG_LIBLC3) */
	}

	total_rx_iso_packet_count++;
	sink_stream->recv_cnt++;
	if ((sink_stream->recv_cnt % 1000U) == 0U) {
		printk("Stream %p: received %u total ISO packets: Valid %u | Error %u | Loss %u\n",
		       &sink_stream->stream, sink_stream->recv_cnt, sink_stream->valid_cnt,
		       sink_stream->error_cnt, sink_stream->loss_cnt);
	}
}

static struct bt_bap_stream_ops stream_ops = {
	.started = stream_started_cb,
	.stopped = stream_stopped_cb,
	.recv = stream_recv_cb,
};

#if defined(CONFIG_TARGET_BROADCAST_CHANNEL)
static bool find_valid_bis_cb(const struct bt_bap_base_subgroup_bis *bis,
					       void *user_data)
{
	int err;
	struct bt_audio_codec_cfg codec_cfg = {0};
	enum bt_audio_location chan_allocation;
	uint8_t *bis_index = user_data;

	err = bt_bap_base_subgroup_bis_codec_to_codec_cfg(bis, &codec_cfg);
	if (err != 0) {
		printk("Could not find codec configuration (err=%d)\n", err);
		return true;
	}

	err = bt_audio_codec_cfg_get_chan_allocation(&codec_cfg, &chan_allocation);
	if (err != 0) {
		printk("Could not find channel allocation (err=%d)\n", err);
		return true;
	}

	if (((CONFIG_TARGET_BROADCAST_CHANNEL) == BT_AUDIO_LOCATION_MONO_AUDIO &&
	    chan_allocation == BT_AUDIO_LOCATION_MONO_AUDIO) ||
	    chan_allocation & CONFIG_TARGET_BROADCAST_CHANNEL) {
		*bis_index = bis->index;

		return false;
	}

	return true;
}

static bool find_valid_bis_in_subgroup_cb(const struct bt_bap_base_subgroup *subgroup,
					  void *user_data)
{
	return bt_bap_base_subgroup_foreach_bis(subgroup, find_valid_bis_cb, user_data)
	       == -ECANCELED ? false : true;
}

static int base_get_first_valid_bis(const struct bt_bap_base *base, uint32_t *bis_index)
{
	int err;
	uint8_t valid_bis_index = 0U;

	err = bt_bap_base_foreach_subgroup(base, find_valid_bis_in_subgroup_cb, &valid_bis_index);
	if (err != -ECANCELED) {
		printk("Failed to parse subgroups: %d\n", err);
		return err != 0 ? err : -ENOENT;
	}

	*bis_index = 0;
	*bis_index |= ((uint8_t)1 << valid_bis_index);

	return 0;
}
#endif /* CONFIG_TARGET_BROADCAST_CHANNEL */

static void base_recv_cb(struct bt_bap_broadcast_sink *sink, const struct bt_bap_base *base,
			 size_t base_size)
{
	uint32_t base_bis_index_bitfield = 0U;
	int err;

	if (k_sem_count_get(&sem_base_received) != 0U) {
		return;
	}

	printk("Received BASE with %d subgroups from broadcast sink %p\n",
	       bt_bap_base_get_subgroup_count(base), sink);

#if defined(CONFIG_TARGET_BROADCAST_CHANNEL)
	err = base_get_first_valid_bis(base, &base_bis_index_bitfield);
	if (err != 0) {
		printk("Failed to find a valid BIS\n");
		return;
	}
#else
	err = bt_bap_base_get_bis_indexes(base, &base_bis_index_bitfield);
	if (err != 0) {
		printk("Failed to BIS indexes: %d\n", err);
		return;
	}
#endif /* CONFIG_TARGET_BROADCAST_CHANNEL */

	bis_index_bitfield = base_bis_index_bitfield & bis_index_mask;

	if (broadcast_assistant_conn == NULL) {
		/* No broadcast assistant requesting anything */
		requested_bis_sync = BT_BAP_BIS_SYNC_NO_PREF;
		k_sem_give(&sem_bis_sync_requested);
	}

	k_sem_give(&sem_base_received);
}

static void syncable_cb(struct bt_bap_broadcast_sink *sink, bool encrypted)
{
	k_sem_give(&sem_syncable);

	if (!encrypted) {
		/* Use the semaphore as a boolean */
		k_sem_reset(&sem_broadcast_code_received);
		k_sem_give(&sem_broadcast_code_received);
	}
}

static struct bt_bap_broadcast_sink_cb broadcast_sink_cbs = {
	.base_recv = base_recv_cb,
	.syncable = syncable_cb,
};

static void pa_timer_handler(struct k_work *work)
{
	if (req_recv_state != NULL) {
		enum bt_bap_pa_state pa_state;

		if (req_recv_state->pa_sync_state == BT_BAP_PA_STATE_INFO_REQ) {
			pa_state = BT_BAP_PA_STATE_NO_PAST;
		} else {
			pa_state = BT_BAP_PA_STATE_FAILED;
		}

		bt_bap_scan_delegator_set_pa_state(req_recv_state->src_id,
						   pa_state);
	}

	printk("PA timeout\n");
}

static K_WORK_DELAYABLE_DEFINE(pa_timer, pa_timer_handler);

static uint16_t interval_to_sync_timeout(uint16_t pa_interval)
{
	uint16_t pa_timeout;

	if (pa_interval == BT_BAP_PA_INTERVAL_UNKNOWN) {
		/* Use maximum value to maximize chance of success */
		pa_timeout = BT_GAP_PER_ADV_MAX_TIMEOUT;
	} else {
		/* Ensure that the following calculation does not overflow silently */
		__ASSERT(SYNC_RETRY_COUNT < 10,
			 "SYNC_RETRY_COUNT shall be less than 10");

		/* Add retries and convert to unit in 10's of ms */
		pa_timeout = ((uint32_t)pa_interval * SYNC_RETRY_COUNT) / 10;

		/* Enforce restraints */
		pa_timeout = CLAMP(pa_timeout, BT_GAP_PER_ADV_MIN_TIMEOUT,
				   BT_GAP_PER_ADV_MAX_TIMEOUT);
	}

	return pa_timeout;
}

static int pa_sync_past(struct bt_conn *conn, uint16_t pa_interval)
{
	struct bt_le_per_adv_sync_transfer_param param = { 0 };
	int err;

	param.skip = PA_SYNC_SKIP;
	param.timeout = interval_to_sync_timeout(pa_interval);

	err = bt_le_per_adv_sync_transfer_subscribe(conn, &param);
	if (err != 0) {
		printk("Could not do PAST subscribe: %d\n", err);
	} else {
		printk("Syncing with PAST: %d\n", err);
		(void)k_work_reschedule(&pa_timer, K_MSEC(param.timeout * 10));
	}

	return err;
}

static int pa_sync_req_cb(struct bt_conn *conn,
			  const struct bt_bap_scan_delegator_recv_state *recv_state,
			  bool past_avail, uint16_t pa_interval)
{
	int err;

	req_recv_state = recv_state;

	if (recv_state->pa_sync_state == BT_BAP_PA_STATE_SYNCED ||
	    recv_state->pa_sync_state == BT_BAP_PA_STATE_INFO_REQ) {
		/* Already syncing */
		/* TODO: Terminate existing sync and then sync to new?*/
		return -1;
	}

	if (IS_ENABLED(CONFIG_BT_PER_ADV_SYNC_TRANSFER_RECEIVER) && past_avail) {
		err = pa_sync_past(conn, pa_interval);
		k_sem_give(&sem_past_request);
	} else {
		/* start scan */
		err = 0;
	}

	k_sem_give(&sem_pa_request);

	return err;
}

static int pa_sync_term_req_cb(struct bt_conn *conn,
			       const struct bt_bap_scan_delegator_recv_state *recv_state)
{
	int err;

	req_recv_state = recv_state;

	err = bt_bap_broadcast_sink_delete(broadcast_sink);
	if (err != 0) {
		return err;
	}

	broadcast_sink = NULL;

	return 0;
}

static void broadcast_code_cb(struct bt_conn *conn,
			      const struct bt_bap_scan_delegator_recv_state *recv_state,
			      const uint8_t broadcast_code[BT_AUDIO_BROADCAST_CODE_SIZE])
{
	printk("Broadcast code received for %p\n", recv_state);

	req_recv_state = recv_state;

	(void)memcpy(sink_broadcast_code, broadcast_code, BT_AUDIO_BROADCAST_CODE_SIZE);

	/* Use the semaphore as a boolean */
	k_sem_reset(&sem_broadcast_code_received);
	k_sem_give(&sem_broadcast_code_received);
}

static int bis_sync_req_cb(struct bt_conn *conn,
			   const struct bt_bap_scan_delegator_recv_state *recv_state,
			   const uint32_t bis_sync_req[BT_BAP_SCAN_DELEGATOR_MAX_SUBGROUPS])
{
	const bool bis_synced = k_sem_count_get(&sem_bis_synced) > 0U;

	printk("BIS sync request received for %p: 0x%08x\n",
	       recv_state, bis_sync_req[0]);

	/* We only care about a single subgroup in this sample */
	if (bis_synced && requested_bis_sync != bis_sync_req[0]) {
		/* If the BIS sync request is received while we are already
		 * synced, it means that the requested BIS sync has changed.
		 */
		int err;

		/* The stream stopped callback will be called as part of this,
		 * and we do not need to wait for any events from the
		 * controller. Thus, when this returns, the `sem_bis_synced`
		 * is back to  0.
		 */
		err = bt_bap_broadcast_sink_stop(broadcast_sink);
		if (err != 0) {
			printk("Failed to stop Broadcast Sink: %d\n", err);

			return err;
		}
	}

	requested_bis_sync = bis_sync_req[0];
	broadcaster_broadcast_id = recv_state->broadcast_id;
	if (bis_sync_req[0] != 0) {
		k_sem_give(&sem_bis_sync_requested);
	}

	return 0;
}

static struct bt_bap_scan_delegator_cb scan_delegator_cbs = {
	.pa_sync_req = pa_sync_req_cb,
	.pa_sync_term_req = pa_sync_term_req_cb,
	.broadcast_code = broadcast_code_cb,
	.bis_sync_req = bis_sync_req_cb,
};

static void connected(struct bt_conn *conn, uint8_t err)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	if (err != 0U) {
		printk("Failed to connect to %s (%u)\n", addr, err);

		broadcast_assistant_conn = NULL;
		return;
	}

	printk("Connected: %s\n", addr);
	broadcast_assistant_conn = bt_conn_ref(conn);

	k_sem_give(&sem_connected);
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	char addr[BT_ADDR_LE_STR_LEN];

	if (conn != broadcast_assistant_conn) {
		return;
	}

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	printk("Disconnected: %s (reason 0x%02x)\n", addr, reason);

	bt_conn_unref(broadcast_assistant_conn);
	broadcast_assistant_conn = NULL;

	k_sem_give(&sem_disconnected);
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
	.connected = connected,
	.disconnected = disconnected,
};

static struct bt_pacs_cap cap = {
	.codec_cap = &codec_cap,
};

static bool scan_check_and_sync_broadcast(struct bt_data *data, void *user_data)
{
	const struct bt_le_scan_recv_info *info = user_data;
	char le_addr[BT_ADDR_LE_STR_LEN];
	struct bt_uuid_16 adv_uuid;
	uint32_t broadcast_id;

	if (data->type != BT_DATA_SVC_DATA16) {
		return true;
	}

	if (data->data_len < BT_UUID_SIZE_16 + BT_AUDIO_BROADCAST_ID_SIZE) {
		return true;
	}

	if (!bt_uuid_create(&adv_uuid.uuid, data->data, BT_UUID_SIZE_16)) {
		return true;
	}

	if (bt_uuid_cmp(&adv_uuid.uuid, BT_UUID_BROADCAST_AUDIO)) {
		return true;
	}

	broadcast_id = sys_get_le24(data->data + BT_UUID_SIZE_16);

	bt_addr_le_to_str(info->addr, le_addr, sizeof(le_addr));

	printk("Found broadcaster with ID 0x%06X and addr %s and sid 0x%02X\n", broadcast_id,
	       le_addr, info->sid);

	if (broadcast_assistant_conn == NULL) {
		/* Not requested by Broadcast Assistant */
		k_sem_give(&sem_broadcaster_found);
	} else if (req_recv_state != NULL &&
		   bt_addr_le_eq(info->addr, &req_recv_state->addr) &&
		   info->sid == req_recv_state->adv_sid &&
		   broadcast_id == req_recv_state->broadcast_id) {
		k_sem_give(&sem_broadcaster_found);
	}

	/* Store info for PA sync parameters */
	memcpy(&broadcaster_info, info, sizeof(broadcaster_info));
	bt_addr_le_copy(&broadcaster_addr, info->addr);
	broadcaster_broadcast_id = broadcast_id;

	/* Stop parsing */
	return false;
}

static bool is_substring(const char *substr, const char *str)
{
	const size_t str_len = strlen(str);
	const size_t sub_str_len = strlen(substr);

	if (sub_str_len > str_len) {
		return false;
	}

	for (size_t pos = 0; pos < str_len; pos++) {
		if (pos + sub_str_len > str_len) {
			return false;
		}

		if (strncasecmp(substr, &str[pos], sub_str_len) == 0) {
			return true;
		}
	}

	return false;
}

static bool data_cb(struct bt_data *data, void *user_data)
{
	char *name = user_data;

	switch (data->type) {
	case BT_DATA_NAME_SHORTENED:
	case BT_DATA_NAME_COMPLETE:
	case BT_DATA_BROADCAST_NAME:
		memcpy(name, data->data, MIN(data->data_len, NAME_LEN - 1));
		return false;
	default:
		return true;
	}
}

static void broadcast_scan_recv(const struct bt_le_scan_recv_info *info, struct net_buf_simple *ad)
{
	if (info->interval != 0U) {
		/* call to bt_data_parse consumes netbufs so shallow clone for verbose output */
		if (strlen(CONFIG_TARGET_BROADCAST_NAME) > 0U) {
			struct net_buf_simple buf_copy;
			char name[NAME_LEN] = {0};

			net_buf_simple_clone(ad, &buf_copy);
			bt_data_parse(&buf_copy, data_cb, name);
			if (!(is_substring(CONFIG_TARGET_BROADCAST_NAME, name))) {
				return;
			}
		}
		bt_data_parse(ad, scan_check_and_sync_broadcast, (void *)info);
	}
}

static struct bt_le_scan_cb bap_scan_cb = {
	.recv = broadcast_scan_recv,
};

static void bap_pa_sync_synced_cb(struct bt_le_per_adv_sync *sync,
				  struct bt_le_per_adv_sync_synced_info *info)
{
	if (sync == pa_sync) {
		printk("PA sync %p synced for broadcast sink with broadcast ID 0x%06X\n", sync,
		       broadcaster_broadcast_id);

		k_sem_give(&sem_pa_synced);
	}
}

static void bap_pa_sync_terminated_cb(struct bt_le_per_adv_sync *sync,
				      const struct bt_le_per_adv_sync_term_info *info)
{
	if (sync == pa_sync) {
		printk("PA sync %p lost with reason %u\n", sync, info->reason);
		pa_sync = NULL;

		k_sem_give(&sem_pa_sync_lost);
	}
}

static struct bt_le_per_adv_sync_cb bap_pa_sync_cb = {
	.synced = bap_pa_sync_synced_cb,
	.term = bap_pa_sync_terminated_cb,
};

static int init(void)
{
	int err;

	err = bt_enable(NULL);
	if (err) {
		printk("Bluetooth enable failed (err %d)\n", err);
		return err;
	}

	printk("Bluetooth initialized\n");

	err = bt_pacs_cap_register(BT_AUDIO_DIR_SINK, &cap);
	if (err) {
		printk("Capability register failed (err %d)\n", err);
		return err;
	}

	bt_bap_broadcast_sink_register_cb(&broadcast_sink_cbs);
	bt_bap_scan_delegator_register_cb(&scan_delegator_cbs);
	bt_le_per_adv_sync_cb_register(&bap_pa_sync_cb);
	bt_le_scan_cb_register(&bap_scan_cb);

	for (size_t i = 0U; i < ARRAY_SIZE(streams); i++) {
		streams[i].stream.ops = &stream_ops;
	}

	/* Initialize ring buffers and USB */
#if defined(CONFIG_USB_DEVICE_AUDIO)
	int ret;
	const struct device *hs_dev = DEVICE_DT_GET(DT_NODELABEL(hs_0));

	for (int i = 0U; i < CONFIG_BT_BAP_BROADCAST_SNK_STREAM_COUNT; i++) {
		ring_buf_init(&streams[i].audio_ring_buf, AUDIO_RING_BUF_SIZE,
			      streams[i]._ring_buffer);
	}

	if (!device_is_ready(hs_dev)) {
		printk("Cannot get USB Headset Device\n");
		return -EIO;
	}

	usb_audio_register(hs_dev, &ops);
	ret = usb_enable(NULL);
	if (ret != 0) {
		printk("Failed to enable USB\n");
		return err;
	}
#endif /* defined(CONFIG_USB_DEVICE_AUDIO) */

	return 0;
}

static int reset(void)
{
	int err;

	bis_index_bitfield = 0U;
	requested_bis_sync = 0U;
	req_recv_state = NULL;
	(void)memset(sink_broadcast_code, 0, sizeof(sink_broadcast_code));
	(void)memset(&broadcaster_info, 0, sizeof(broadcaster_info));
	(void)memset(&broadcaster_addr, 0, sizeof(broadcaster_addr));
	broadcaster_broadcast_id = INVALID_BROADCAST_ID;

	if (broadcast_sink != NULL) {
		err = bt_bap_broadcast_sink_delete(broadcast_sink);
		if (err) {
			printk("Deleting broadcast sink failed (err %d)\n", err);

			return err;
		}

		broadcast_sink = NULL;
	}

	if (pa_sync != NULL) {
		bt_le_per_adv_sync_delete(pa_sync);
		if (err) {
			printk("Deleting PA sync failed (err %d)\n", err);

			return err;
		}

		pa_sync = NULL;
	}

	if (IS_ENABLED(CONFIG_SCAN_OFFLOAD)) {
		if (broadcast_assistant_conn != NULL) {
			err = bt_conn_disconnect(broadcast_assistant_conn,
						 BT_HCI_ERR_REMOTE_USER_TERM_CONN);
			if (err) {
				printk("Disconnecting Broadcast Assistant failed (err %d)\n",
				       err);

				return err;
			}

			err = k_sem_take(&sem_disconnected, SEM_TIMEOUT);
			if (err != 0) {
				printk("Failed to take sem_disconnected: %d\n", err);

				return err;
			}
		} else if (ext_adv != NULL) { /* advertising still running */
			err = bt_le_ext_adv_stop(ext_adv);
			if (err) {
				printk("Stopping advertising set failed (err %d)\n",
				       err);

				return err;
			}

			err = bt_le_ext_adv_delete(ext_adv);
			if (err) {
				printk("Deleting advertising set failed (err %d)\n",
				       err);

				return err;
			}

			ext_adv = NULL;
		}

		k_sem_reset(&sem_connected);
		k_sem_reset(&sem_disconnected);
		k_sem_reset(&sem_pa_request);
		k_sem_reset(&sem_past_request);
	}

	k_sem_reset(&sem_broadcaster_found);
	k_sem_reset(&sem_pa_synced);
	k_sem_reset(&sem_base_received);
	k_sem_reset(&sem_syncable);
	k_sem_reset(&sem_pa_sync_lost);
	k_sem_reset(&sem_broadcast_code_received);
	k_sem_reset(&sem_bis_sync_requested);
	k_sem_reset(&sem_bis_synced);
	return 0;
}

static int start_adv(void)
{
	const struct bt_data ad[] = {
		BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
		BT_DATA_BYTES(BT_DATA_UUID16_ALL,
			      BT_UUID_16_ENCODE(BT_UUID_BASS_VAL),
			      BT_UUID_16_ENCODE(BT_UUID_PACS_VAL)),
	};
	int err;

	/* Create a non-connectable non-scannable advertising set */
	err = bt_le_ext_adv_create(BT_LE_EXT_ADV_CONN_NAME, NULL, &ext_adv);
	if (err != 0) {
		printk("Failed to create advertising set (err %d)\n", err);

		return err;
	}

	err = bt_le_ext_adv_set_data(ext_adv, ad, ARRAY_SIZE(ad), NULL, 0);
	if (err != 0) {
		printk("Failed to set advertising data (err %d)\n", err);

		return err;
	}

	err = bt_le_ext_adv_start(ext_adv, BT_LE_EXT_ADV_START_DEFAULT);
	if (err != 0) {
		printk("Failed to start advertising set (err %d)\n", err);

		return err;
	}

	return 0;
}

static int stop_adv(void)
{
	int err;

	err = bt_le_ext_adv_stop(ext_adv);
	if (err != 0) {
		printk("Failed to stop advertising set (err %d)\n", err);

		return err;
	}

	err = bt_le_ext_adv_delete(ext_adv);
	if (err != 0) {
		printk("Failed to delete advertising set (err %d)\n", err);

		return err;
	}

	ext_adv = NULL;

	return 0;
}

static int pa_sync_create(void)
{
	struct bt_le_per_adv_sync_param create_params = {0};

	bt_addr_le_copy(&create_params.addr, &broadcaster_addr);
	create_params.options = BT_LE_PER_ADV_SYNC_OPT_FILTER_DUPLICATE;
	create_params.sid = broadcaster_info.sid;
	create_params.skip = PA_SYNC_SKIP;
	create_params.timeout = interval_to_sync_timeout(broadcaster_info.interval);

	return bt_le_per_adv_sync_create(&create_params, &pa_sync);
}

int main(void)
{
	int err;

	err = init();
	if (err) {
		printk("Init failed (err %d)\n", err);
		return 0;
	}

	for (size_t i = 0U; i < ARRAY_SIZE(streams_p); i++) {
		streams_p[i] = &streams[i].stream;
#if defined(CONFIG_LIBLC3)
		k_mutex_init(&streams[i].lc3_decoder_mutex);
#endif /* defined(CONFIG_LIBLC3) */
	}

	while (true) {
		uint32_t sync_bitfield;

		err = reset();
		if (err != 0) {
			printk("Resetting failed: %d - Aborting\n", err);

			return 0;
		}

		if (IS_ENABLED(CONFIG_SCAN_OFFLOAD)) {
			printk("Starting advertising\n");
			err = start_adv();
			if (err != 0) {
				printk("Unable to start advertising connectable: %d\n",
				       err);

				return 0;
			}

			printk("Waiting for Broadcast Assistant\n");
			err = k_sem_take(&sem_connected, ADV_TIMEOUT);
			if (err != 0) {
				printk("No Broadcast Assistant connected\n");

				err = stop_adv();
				if (err != 0) {
					printk("Unable to stop advertising: %d\n",
					       err);

					return 0;
				}
			} else {
				/* Wait for the PA request to determine if we
				 * should start scanning, or wait for PAST
				 */
				err = k_sem_take(&sem_pa_request,
						 BROADCAST_ASSISTANT_TIMEOUT);
				if (err != 0) {
					printk("sem_pa_request timed out, resetting\n");
					continue;
				}

				if (k_sem_take(&sem_past_request, K_NO_WAIT) == 0) {
					goto wait_for_pa_sync;
				} /* else continue with scanning below */
			}
		}

		if (strlen(CONFIG_TARGET_BROADCAST_NAME) > 0U) {
			printk("Scanning for broadcast sources containing`"
			CONFIG_TARGET_BROADCAST_NAME "`\n");
		} else {
			printk("Scanning for broadcast sources\n");
		}

		err = bt_le_scan_start(BT_LE_SCAN_ACTIVE, NULL);
		if (err != 0 && err != -EALREADY) {
			printk("Unable to start scan for broadcast sources: %d\n",
			       err);
			return 0;
		}

		err = k_sem_take(&sem_broadcaster_found, SEM_TIMEOUT);
		if (err != 0) {
			printk("sem_broadcaster_found timed out, resetting\n");
			continue;
		}
		printk("Broadcast source found, waiting for PA sync\n");

		err = bt_le_scan_stop();
		if (err != 0) {
			printk("bt_le_scan_stop failed with %d, resetting\n", err);
			continue;
		}

		printk("Attempting to PA sync to the broadcaster with id 0x%06X\n",
		       broadcaster_broadcast_id);
		err = pa_sync_create();
		if (err != 0) {
			printk("Could not create Broadcast PA sync: %d, resetting\n", err);
			continue;
		}

wait_for_pa_sync:
		printk("Waiting for PA synced\n");
		err = k_sem_take(&sem_pa_synced, SEM_TIMEOUT);
		if (err != 0) {
			printk("sem_pa_synced timed out, resetting\n");
			continue;
		}

		printk("Broadcast source PA synced, creating Broadcast Sink\n");
		err = bt_bap_broadcast_sink_create(pa_sync, broadcaster_broadcast_id,
						   &broadcast_sink);
		if (err != 0) {
			printk("Failed to create broadcast sink: %d\n", err);
			continue;
		}

		printk("Broadcast Sink created, waiting for BASE\n");
		err = k_sem_take(&sem_base_received, SEM_TIMEOUT);
		if (err != 0) {
			printk("sem_base_received timed out, resetting\n");
			continue;
		}
		printk("BASE received, waiting for syncable\n");

		err = k_sem_take(&sem_syncable, SEM_TIMEOUT);
		if (err != 0) {
			printk("sem_syncable timed out, resetting\n");
			continue;
		}

		/* sem_broadcast_code_received is also given if the
		 * broadcast is not encrypted
		 */
		printk("Waiting for broadcast code OK\n");
		err = k_sem_take(&sem_broadcast_code_received, SEM_TIMEOUT);
		if (err != 0) {
			printk("sem_syncable timed out, resetting\n");
			continue;
		}

		printk("Waiting for BIS sync request\n");
		err = k_sem_take(&sem_bis_sync_requested, SEM_TIMEOUT);
		if (err != 0) {
			printk("sem_syncable timed out, resetting\n");
			continue;
		}

		sync_bitfield = bis_index_bitfield & requested_bis_sync;
		printk("Syncing to broadcast with bitfield: 0x%08x\n", sync_bitfield);
		err = bt_bap_broadcast_sink_sync(broadcast_sink, sync_bitfield, streams_p,
						 sink_broadcast_code);
		if (err != 0) {
			printk("Unable to sync to broadcast source: %d\n", err);
			return 0;
		}

		printk("Waiting for BIG sync\n");
		err = k_sem_take(&sem_bis_synced, SEM_TIMEOUT);
		if (err != 0) {
			printk("sem_bis_synced timed out, resetting\n");
			continue;
		}

		printk("Waiting for PA disconnected\n");
		k_sem_take(&sem_pa_sync_lost, K_FOREVER);
	}
	return 0;
}
