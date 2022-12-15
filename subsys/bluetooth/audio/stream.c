/*  Bluetooth Audio Stream */

/*
 * Copyright (c) 2020 Intel Corporation
 * Copyright (c) 2021-2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/check.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/iso.h>
#include <zephyr/bluetooth/audio/audio.h>

#include "../host/conn_internal.h"
#include "../host/iso_internal.h"

#include "audio_iso.h"
#include "endpoint.h"
#include "unicast_client_internal.h"
#include "unicast_server.h"

#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(bt_audio_stream, CONFIG_BT_AUDIO_STREAM_LOG_LEVEL);

static uint8_t pack_bt_codec_cc(const struct bt_codec *codec, uint8_t cc[])
{
	uint8_t len;

	len = 0U;
	for (size_t i = 0U; i < codec->data_count; i++) {
		const struct bt_data *data = &codec->data[i].data;

		/* We assume that data_len and data has previously been verified
		 * and that based on the Kconfigs we can assume that the length
		 * will always fit in `cc`
		 */
		cc[len++] = data->data_len + 1;
		cc[len++] = data->type;
		(void)memcpy(cc + len, data->data, data->data_len);
		len += data->data_len;
	}

	return len;
}

void bt_audio_codec_to_iso_path(struct bt_iso_chan_path *path,
				const struct bt_codec *codec)
{
	path->pid = codec->path_id;
	path->format = codec->id;
	path->cid = codec->cid;
	path->vid = codec->vid;
	path->delay = 0; /* TODO: Add to bt_codec? Use presentation delay? */
	path->cc_len = pack_bt_codec_cc(codec, path->cc);
}

void bt_audio_codec_qos_to_iso_qos(struct bt_iso_chan_io_qos *io,
				   const struct bt_codec_qos *codec_qos)
{
	io->sdu = codec_qos->sdu;
	io->phy = codec_qos->phy;
	io->rtn = codec_qos->rtn;
}

void bt_audio_stream_attach(struct bt_conn *conn,
			    struct bt_audio_stream *stream,
			    struct bt_audio_ep *ep,
			    struct bt_codec *codec)
{
	LOG_DBG("conn %p stream %p ep %p codec %p", conn, stream, ep, codec);

	if (conn != NULL) {
		__ASSERT(stream->conn == NULL || stream->conn == conn,
			 "stream->conn %p already attached", stream->conn);
		stream->conn = bt_conn_ref(conn);
	}
	stream->codec = codec;
	stream->ep = ep;
	ep->stream = stream;
}

struct bt_iso_chan *bt_audio_stream_iso_chan_get(struct bt_audio_stream *stream)
{
	if (stream != NULL && stream->ep != NULL && stream->ep->iso != NULL) {
		return &stream->ep->iso->chan;
	}

	return NULL;
}

void bt_audio_stream_cb_register(struct bt_audio_stream *stream,
				 struct bt_audio_stream_ops *ops)
{
	stream->ops = ops;
}

#if defined(CONFIG_BT_AUDIO_UNICAST) || defined(CONFIG_BT_AUDIO_BROADCAST_SOURCE)
bool bt_audio_valid_qos(const struct bt_codec_qos *qos)
{
	if (qos->interval < BT_ISO_SDU_INTERVAL_MIN ||
	    qos->interval > BT_ISO_SDU_INTERVAL_MAX) {
		LOG_DBG("Interval not within allowed range: %u (%u-%u)", qos->interval,
			BT_ISO_SDU_INTERVAL_MIN, BT_ISO_SDU_INTERVAL_MAX);
		return false;
	}

	if (qos->framing > BT_CODEC_QOS_FRAMED) {
		LOG_DBG("Invalid Framing 0x%02x", qos->framing);
		return false;
	}

	if (qos->phy != BT_CODEC_QOS_1M &&
	    qos->phy != BT_CODEC_QOS_2M &&
	    qos->phy != BT_CODEC_QOS_CODED) {
		LOG_DBG("Invalid PHY 0x%02x", qos->phy);
		return false;
	}

	if (qos->sdu > BT_ISO_MAX_SDU) {
		LOG_DBG("Invalid SDU %u", qos->sdu);
		return false;
	}

	if (qos->latency < BT_ISO_LATENCY_MIN ||
	    qos->latency > BT_ISO_LATENCY_MAX) {
		LOG_DBG("Invalid Latency %u", qos->latency);
		return false;
	}

	return true;
}

int bt_audio_stream_send(struct bt_audio_stream *stream, struct net_buf *buf,
			 uint16_t seq_num, uint32_t ts)
{
	struct bt_audio_ep *ep;

	if (stream == NULL || stream->ep == NULL) {
		return -EINVAL;
	}

	ep = stream->ep;

	if (ep->status.state != BT_AUDIO_EP_STATE_STREAMING) {
		LOG_DBG("Channel %p not ready for streaming (state: %s)", stream,
			bt_audio_ep_state_str(ep->status.state));
		return -EBADMSG;
	}

	/* TODO: Add checks for broadcast sink */

	return bt_iso_chan_send(bt_audio_stream_iso_chan_get(stream),
				buf, seq_num, ts);
}
#endif /* CONFIG_BT_AUDIO_UNICAST || CONFIG_BT_AUDIO_BROADCAST_SOURCE */

#if defined(CONFIG_BT_AUDIO_UNICAST)
#if defined(CONFIG_BT_AUDIO_UNICAST_CLIENT)
static struct bt_audio_unicast_group unicast_groups[UNICAST_GROUP_CNT];
#endif /* CONFIG_BT_AUDIO_UNICAST_CLIENT */

#if defined(CONFIG_BT_AUDIO_UNICAST_SERVER)
static struct bt_audio_stream *enabling[CONFIG_BT_ISO_MAX_CHAN];

static int bt_audio_stream_iso_accept(const struct bt_iso_accept_info *info,
				      struct bt_iso_chan **iso_chan)
{
	int i;

	LOG_DBG("acl %p", info->acl);

	for (i = 0; i < ARRAY_SIZE(enabling); i++) {
		struct bt_audio_stream *c = enabling[i];

		if (c && c->ep->cig_id == info->cig_id &&
		    c->ep->cis_id == info->cis_id) {
			*iso_chan = &enabling[i]->ep->iso->chan;
			enabling[i] = NULL;

			LOG_DBG("iso_chan %p", *iso_chan);

			return 0;
		}
	}

	LOG_ERR("No channel listening");

	return -EPERM;
}

static struct bt_iso_server iso_server = {
	.sec_level = BT_SECURITY_L2,
	.accept = bt_audio_stream_iso_accept,
};

int bt_audio_stream_iso_listen(struct bt_audio_stream *stream)
{
	static bool server;
	int err, i;
	struct bt_audio_stream **free_stream = NULL;

	LOG_DBG("stream %p conn %p", stream, stream->conn);

	if (server) {
		goto done;
	}

	err = bt_iso_server_register(&iso_server);
	if (err) {
		LOG_ERR("bt_iso_server_register: %d", err);
		return err;
	}

	server = true;

done:
	for (i = 0; i < ARRAY_SIZE(enabling); i++) {
		if (enabling[i] == stream) {
			return 0;
		}

		if (enabling[i] == NULL && free_stream == NULL) {
			free_stream = &enabling[i];
		}
	}

	if (free_stream != NULL) {
		*free_stream = stream;
		return 0;
	}

	LOG_ERR("Unable to listen: no slot left");

	return -ENOSPC;
}
#endif /* CONFIG_BT_AUDIO_UNICAST_SERVER */

static bool bt_audio_stream_is_broadcast(const struct bt_audio_stream *stream)
{
	return (IS_ENABLED(CONFIG_BT_AUDIO_BROADCAST_SOURCE) &&
		bt_audio_ep_is_broadcast_src(stream->ep)) ||
	       (IS_ENABLED(CONFIG_BT_AUDIO_BROADCAST_SINK) &&
		bt_audio_ep_is_broadcast_snk(stream->ep));
}

bool bt_audio_valid_stream_qos(const struct bt_audio_stream *stream,
			       const struct bt_codec_qos *qos)
{
	const struct bt_codec_qos_pref *qos_pref = &stream->ep->qos_pref;

	if (qos_pref->latency < qos->latency) {
		/* Latency is a preferred value. Print debug info but do not fail. */
		LOG_DBG("Latency %u higher than preferred max %u", qos->latency, qos_pref->latency);
	}

	if (!IN_RANGE(qos->pd, qos_pref->pd_min, qos_pref->pd_max)) {
		LOG_DBG("Presentation Delay not within range: min %u max %u pd %u",
			qos_pref->pd_min, qos_pref->pd_max, qos->pd);
		return false;
	}

	return true;
}

void bt_audio_stream_detach(struct bt_audio_stream *stream)
{
	const bool is_broadcast = bt_audio_stream_is_broadcast(stream);

	LOG_DBG("stream %p", stream);

	if (stream->conn != NULL) {
		bt_conn_unref(stream->conn);
		stream->conn = NULL;
	}
	stream->codec = NULL;
	stream->ep->stream = NULL;
	stream->ep = NULL;

	if (!is_broadcast) {
		bt_audio_stream_disconnect(stream);
	}
}

int bt_audio_stream_disconnect(struct bt_audio_stream *stream)
{
	struct bt_iso_chan *iso_chan = bt_audio_stream_iso_chan_get(stream);

	LOG_DBG("stream %p iso %p", stream, iso_chan);

	if (stream == NULL) {
		return -EINVAL;
	}

#if defined(CONFIG_BT_AUDIO_UNICAST_SERVER)
	/* Stop listening */
	for (size_t i = 0; i < ARRAY_SIZE(enabling); i++) {
		if (enabling[i] == stream) {
			enabling[i] = NULL;
			break;
		}
	}
#endif /* CONFIG_BT_AUDIO_UNICAST_SERVER */

	if (iso_chan == NULL || iso_chan->iso == NULL) {
		return -ENOTCONN;
	}

	return bt_iso_chan_disconnect(iso_chan);
}

void bt_audio_stream_reset(struct bt_audio_stream *stream)
{
	LOG_DBG("stream %p", stream);

	if (stream == NULL) {
		return;
	}

	bt_audio_stream_detach(stream);
}

#if defined(CONFIG_BT_AUDIO_UNICAST_CLIENT)

int bt_audio_stream_config(struct bt_conn *conn,
			   struct bt_audio_stream *stream,
			   struct bt_audio_ep *ep,
			   struct bt_codec *codec)
{
	uint8_t role;
	int err;

	LOG_DBG("conn %p stream %p, ep %p codec %p codec id 0x%02x "
	       "codec cid 0x%04x codec vid 0x%04x", conn, stream, ep,
	       codec, codec ? codec->id : 0, codec ? codec->cid : 0,
	       codec ? codec->vid : 0);

	CHECKIF(conn == NULL || stream == NULL || codec == NULL) {
		LOG_DBG("NULL value(s) supplied)");
		return -EINVAL;
	}

	if (stream->conn != NULL) {
		LOG_DBG("Stream already configured for conn %p", stream->conn);
		return -EALREADY;
	}

	role = conn->role;
	if (role != BT_HCI_ROLE_CENTRAL) {
		LOG_DBG("Invalid conn role: %u, shall be central", role);
		return -EINVAL;
	}

	switch (ep->status.state) {
	/* Valid only if ASE_State field = 0x00 (Idle) */
	case BT_AUDIO_EP_STATE_IDLE:
	 /* or 0x01 (Codec Configured) */
	case BT_AUDIO_EP_STATE_CODEC_CONFIGURED:
	 /* or 0x02 (QoS Configured) */
	case BT_AUDIO_EP_STATE_QOS_CONFIGURED:
		break;
	default:
		LOG_ERR("Invalid state: %s", bt_audio_ep_state_str(ep->status.state));
		return -EBADMSG;
	}

	bt_audio_stream_attach(conn, stream, ep, codec);

	err = bt_unicast_client_config(stream, codec);
	if (err != 0) {
		LOG_DBG("Failed to configure stream: %d", err);
		return err;
	}

	return 0;
}

static void bt_audio_codec_qos_to_cig_param(struct bt_iso_cig_param *cig_param,
					    const struct bt_codec_qos *qos)
{
	cig_param->framing = qos->framing;
	cig_param->packing = BT_ISO_PACKING_SEQUENTIAL; /*  TODO: Add to QoS struct */
	cig_param->interval = qos->interval;
	cig_param->latency = qos->latency;
	cig_param->sca = BT_GAP_SCA_UNKNOWN;
}

static int bt_audio_cig_create(struct bt_audio_unicast_group *group,
			       const struct bt_codec_qos *qos)
{
	struct bt_iso_cig_param param;
	uint8_t cis_count;
	int err;

	LOG_DBG("group %p qos %p", group, qos);

	cis_count = 0;
	for (size_t i = 0; i < ARRAY_SIZE(group->cis); i++) {
		if (group->cis[i] != NULL) {
			cis_count++;
		}
	}

	param.num_cis = cis_count;
	param.cis_channels = group->cis;
	bt_audio_codec_qos_to_cig_param(&param, qos);

	err = bt_iso_cig_create(&param, &group->cig);
	if (err != 0) {
		LOG_ERR("bt_iso_cig_create failed: %d", err);
		return err;
	}

	group->qos = qos;

	return 0;
}

static int bt_audio_cig_reconfigure(struct bt_audio_unicast_group *group,
				    const struct bt_codec_qos *qos)
{
	struct bt_iso_cig_param param;
	uint8_t cis_count;
	int err;

	LOG_DBG("group %p qos %p", group, qos);

	cis_count = 0U;
	for (size_t i = 0; i < ARRAY_SIZE(group->cis); i++) {
		if (group->cis[i] != NULL) {
			cis_count++;
		}
	}

	param.num_cis = cis_count;
	param.cis_channels = group->cis;
	bt_audio_codec_qos_to_cig_param(&param, qos);

	err = bt_iso_cig_reconfigure(group->cig, &param);
	if (err != 0) {
		LOG_ERR("bt_iso_cig_create failed: %d", err);
		return err;
	}

	group->qos = qos;

	return 0;
}

static void audio_stream_qos_cleanup(const struct bt_conn *conn,
				     struct bt_audio_unicast_group *group)
{
	struct bt_audio_stream *stream;

	SYS_SLIST_FOR_EACH_CONTAINER(&group->streams, stream, _node) {
		if (stream->conn != conn && stream->ep != NULL) {
			/* Channel not part of this ACL, skip */
			continue;
		}

		bt_audio_iso_unbind_ep(stream->ep->iso, stream->ep);
	}
}

int bt_audio_stream_qos(struct bt_conn *conn,
			struct bt_audio_unicast_group *group)
{
	struct bt_audio_stream *stream;
	struct net_buf_simple *buf;
	struct bt_ascs_qos_op *op;
	struct bt_audio_ep *ep;
	bool conn_stream_found;
	bool cig_connected;
	uint8_t role;
	int err;

	LOG_DBG("conn %p group %p", conn, group);

	CHECKIF(conn == NULL) {
		LOG_DBG("conn is NULL");
		return -EINVAL;
	}

	CHECKIF(group == NULL) {
		LOG_DBG("group is NULL");
		return -EINVAL;
	}

	if (sys_slist_is_empty(&group->streams)) {
		LOG_DBG("group stream list is empty");
		return -ENOEXEC;
	}

	role = conn->role;
	if (role != BT_HCI_ROLE_CENTRAL) {
		LOG_DBG("Invalid conn role: %u, shall be central", role);
		return -EINVAL;
	}

	/* Used to determine if a stream for the supplied connection pointer
	 * was actually found
	 */
	conn_stream_found = false;

	/* User to determine if any stream in the group is in
	 * the connected state
	 */
	cig_connected = false;

	/* Validate streams before starting the QoS execution */
	SYS_SLIST_FOR_EACH_CONTAINER(&group->streams, stream, _node) {
		if (stream->conn != conn) {
			/* Channel not part of this ACL, skip */
			continue;
		}
		conn_stream_found = true;

		ep = stream->ep;
		if (ep == NULL) {
			LOG_DBG("stream->ep is NULL");
			return -EINVAL;
		}

		/* Can only be done if all the streams are in the codec
		 * configured state or the QoS configured state
		 */
		switch (ep->status.state) {
		case BT_AUDIO_EP_STATE_CODEC_CONFIGURED:
		case BT_AUDIO_EP_STATE_QOS_CONFIGURED:
			break;
		default:
			LOG_DBG("Invalid state: %s",
				bt_audio_ep_state_str(stream->ep->status.state));
			return -EINVAL;
		}

		if (!bt_audio_valid_stream_qos(stream, stream->qos)) {
			return -EINVAL;
		}

		/* Verify ep->dir */
		switch (ep->dir) {
		case BT_AUDIO_DIR_SINK:
		case BT_AUDIO_DIR_SOURCE:
			break;
		default:
			__ASSERT(false, "invalid endpoint dir: %u", ep->dir);
			return -EINVAL;
		}

		if (ep->iso == NULL) {
			/* This can only happen if the stream was somehow added
			 * to a group without the audio_iso being bound to it
			 */
			LOG_ERR("Could not find audio_iso for stream %p", stream);
			return -EINVAL;
		}
	}

	if (!conn_stream_found) {
		LOG_DBG("No streams in the group %p for conn %p", group, conn);
		return -EINVAL;
	}

	/* Generate the control point write */
	buf = bt_unicast_client_ep_create_pdu(BT_ASCS_QOS_OP);

	op = net_buf_simple_add(buf, sizeof(*op));

	(void)memset(op, 0, sizeof(*op));
	ep = NULL; /* Needed to find the control point handle */
	SYS_SLIST_FOR_EACH_CONTAINER(&group->streams, stream, _node) {
		if (stream->conn != conn) {
			/* Channel not part of this ACL, skip */
			continue;
		}

		op->num_ases++;

		err = bt_unicast_client_ep_qos(stream->ep, buf, stream->qos);
		if (err) {
			audio_stream_qos_cleanup(conn, group);
			return err;
		}

		if (ep == NULL) {
			ep = stream->ep;
		}
	}

	err = bt_unicast_client_ep_send(conn, ep, buf);
	if (err != 0) {
		LOG_DBG("Could not send config QoS: %d", err);
		audio_stream_qos_cleanup(conn, group);
		return err;
	}

	return 0;
}

int bt_audio_stream_enable(struct bt_audio_stream *stream,
			   struct bt_codec_data *meta,
			   size_t meta_count)
{
	uint8_t role;
	int err;

	LOG_DBG("stream %p", stream);

	if (stream == NULL || stream->ep == NULL || stream->conn == NULL) {
		LOG_DBG("Invalid stream");
		return -EINVAL;
	}

	role = stream->conn->role;
	if (role != BT_HCI_ROLE_CENTRAL) {
		LOG_DBG("Invalid conn role: %u, shall be central", role);
		return -EINVAL;
	}

	/* Valid for an ASE only if ASE_State field = 0x02 (QoS Configured) */
	if (stream->ep->status.state != BT_AUDIO_EP_STATE_QOS_CONFIGURED) {
		LOG_ERR("Invalid state: %s", bt_audio_ep_state_str(stream->ep->status.state));
		return -EBADMSG;
	}

	err = bt_unicast_client_enable(stream, meta, meta_count);
	if (err != 0) {
		LOG_DBG("Failed to enable stream: %d", err);
		return err;
	}

	return 0;
}

int bt_audio_stream_stop(struct bt_audio_stream *stream)
{
	struct bt_audio_ep *ep;
	uint8_t role;
	int err;

	if (stream == NULL || stream->ep == NULL || stream->conn == NULL) {
		LOG_DBG("Invalid stream");
		return -EINVAL;
	}

	role = stream->conn->role;
	if (role != BT_HCI_ROLE_CENTRAL) {
		LOG_DBG("Invalid conn role: %u, shall be central", role);
		return -EINVAL;
	}

	ep = stream->ep;

	switch (ep->status.state) {
	/* Valid only if ASE_State field = 0x03 (Disabling) */
	case BT_AUDIO_EP_STATE_DISABLING:
		break;
	default:
		LOG_ERR("Invalid state: %s", bt_audio_ep_state_str(ep->status.state));
		return -EBADMSG;
	}

	err = bt_unicast_client_stop(stream);
	if (err != 0) {
		LOG_DBG("Stopping stream failed: %d", err);
		return err;
	}

	return 0;
}

int bt_audio_cig_terminate(struct bt_audio_unicast_group *group)
{
	LOG_DBG("group %p", group);

	return bt_iso_cig_terminate(group->cig);
}

int bt_audio_stream_connect(struct bt_audio_stream *stream)
{
	struct bt_iso_connect_param param;
	struct bt_iso_chan *iso_chan;

	iso_chan = bt_audio_stream_iso_chan_get(stream);

	LOG_DBG("stream %p iso %p", stream, iso_chan);

	if (stream == NULL || iso_chan == NULL) {
		return -EINVAL;
	}

	param.acl = stream->conn;
	param.iso_chan = iso_chan;

	switch (iso_chan->state) {
	case BT_ISO_STATE_DISCONNECTED:
		return bt_iso_chan_connect(&param, 1);
	case BT_ISO_STATE_CONNECTING:
	case BT_ISO_STATE_CONNECTED:
		return -EALREADY;
	default:
		return bt_iso_chan_connect(&param, 1);
	}
}

static bool unicast_group_valid_qos(const struct bt_codec_qos *group_qos,
				    const struct bt_codec_qos *stream_qos)
{
	if (group_qos->framing != stream_qos->framing ||
	    group_qos->interval != stream_qos->interval ||
	    group_qos->latency != stream_qos->latency) {
		return false;
	}

	return true;
}

static struct bt_audio_iso *get_new_iso(struct bt_audio_unicast_group *group,
					struct bt_conn *acl,
					enum bt_audio_dir dir)
{
	struct bt_audio_stream *stream;

	/* Check if there's already an ISO that can be used for this direction */
	SYS_SLIST_FOR_EACH_CONTAINER(&group->streams, stream, _node) {
		__ASSERT(stream->ep, "stream->ep is NULL");
		__ASSERT(stream->ep->iso, "ep->iso is NULL");

		if (stream->conn != acl) {
			continue;
		}

		if (bt_audio_iso_get_ep(stream->ep->iso, dir) == NULL) {
			return bt_audio_iso_ref(stream->ep->iso);
		}
	}

	return bt_unicast_client_new_audio_iso();
}

static int unicast_group_add_iso(struct bt_audio_unicast_group *group,
				 struct bt_audio_iso *iso)
{
	struct bt_iso_chan **chan_slot = NULL;

	__ASSERT_NO_MSG(group != NULL);
	__ASSERT_NO_MSG(iso != NULL);

	/* Append iso channel to the group->cis array */
	for (size_t i = 0; i < ARRAY_SIZE(group->cis); i++) {
		/* Return if already there */
		if (group->cis[i] == &iso->chan) {
			return 0;
		}

		if (chan_slot == NULL && group->cis[i] == NULL) {
			chan_slot = &group->cis[i];
		}
	}

	if (chan_slot == NULL) {
		return -ENOMEM;
	}

	*chan_slot = &iso->chan;

	return 0;
}

static void unicast_group_del_iso(struct bt_audio_unicast_group *group,
				  struct bt_audio_iso *iso)
{
	struct bt_audio_stream *stream;

	__ASSERT_NO_MSG(group != NULL);
	__ASSERT_NO_MSG(iso != NULL);

	SYS_SLIST_FOR_EACH_CONTAINER(&group->streams, stream, _node) {
		if (stream->ep->iso == iso) {
			/* still in use by some other stream */
			return;
		}
	}

	for (size_t i = 0; i < ARRAY_SIZE(group->cis); i++) {
		if (group->cis[i] == &iso->chan) {
			group->cis[i] = NULL;
			return;
		}
	}
}

static int unicast_group_add_stream(struct bt_audio_unicast_group *group,
				    struct bt_audio_stream *stream,
				    struct bt_codec_qos *qos,
				    enum bt_audio_dir dir)
{
	struct bt_audio_iso *iso;
	int err;

	__ASSERT_NO_MSG(group != NULL);
	__ASSERT_NO_MSG(stream != NULL);
	__ASSERT_NO_MSG(stream->ep != NULL);
	__ASSERT_NO_MSG(stream->ep->iso != NULL);

	iso = get_new_iso(group, stream->conn, dir);
	if (iso == NULL) {
		return -ENOMEM;
	}

	err = unicast_group_add_iso(group, iso);
	if (err < 0) {
		bt_audio_iso_unref(iso);
		return err;
	}

	/* iso initialized already */
	bt_audio_iso_bind_ep(iso, stream->ep);

	if (dir == BT_AUDIO_DIR_SINK) {
		/* If the endpoint is a sink, then we need to
		 * configure our TX parameters
		 */
		bt_audio_codec_qos_to_iso_qos(iso->chan.qos->tx, qos);
	} else {
		/* If the endpoint is a source, then we need to
		 * configure our RX parameters
		 */
		bt_audio_codec_qos_to_iso_qos(iso->chan.qos->rx, qos);
	}

	bt_audio_iso_unref(iso);

	stream->qos = qos;
	stream->unicast_group = group;
	sys_slist_append(&group->streams, &stream->_node);

	LOG_DBG("Added stream %p to group %p", stream, group);

	return 0;
}

static void unicast_group_del_stream(struct bt_audio_unicast_group *group,
				     struct bt_audio_stream *stream)
{
	__ASSERT_NO_MSG(group != NULL);
	__ASSERT_NO_MSG(stream != NULL);

	if (sys_slist_find_and_remove(&group->streams, &stream->_node)) {
		unicast_group_del_iso(group, stream->ep->iso);

		stream->unicast_group = NULL;
		bt_audio_iso_unbind_ep(stream->ep->iso, stream->ep);
	}
}

static struct bt_audio_unicast_group *unicast_group_alloc(void)
{
	struct bt_audio_unicast_group *group = NULL;

	for (size_t i = 0U; i < ARRAY_SIZE(unicast_groups); i++) {
		if (!unicast_groups[i].allocated) {
			group = &unicast_groups[i];

			(void)memset(group, 0, sizeof(*group));

			group->allocated = true;
			group->index = i;

			break;
		}
	}

	return group;
}

static void unicast_group_free(struct bt_audio_unicast_group *group)
{
	struct bt_audio_stream *stream, *next;

	__ASSERT_NO_MSG(group != NULL);

	SYS_SLIST_FOR_EACH_CONTAINER_SAFE(&group->streams, stream, next, _node) {
		stream->unicast_group = NULL;
		bt_audio_iso_unbind_ep(stream->ep->iso, stream->ep);
		sys_slist_remove(&group->streams, NULL, &stream->_node);
	}

	group->allocated = false;
}

int bt_audio_unicast_group_create(struct bt_audio_unicast_group_param params[],
				  size_t num_param,
				  struct bt_audio_unicast_group **out_unicast_group)
{
	struct bt_audio_unicast_group *unicast_group;
	const struct bt_codec_qos *group_qos = NULL;
	int err;

	CHECKIF(out_unicast_group == NULL) {
		LOG_DBG("out_unicast_group is NULL");
		return -EINVAL;
	}
	/* Set out_unicast_group to NULL until the source has actually been created */
	*out_unicast_group = NULL;

	CHECKIF(params == NULL) {
		LOG_DBG("streams is NULL");
		return -EINVAL;
	}

	CHECKIF(num_param > UNICAST_GROUP_STREAM_CNT) {
		LOG_DBG("Too many streams provided: %u/%u", num_param, UNICAST_GROUP_STREAM_CNT);
		return -EINVAL;
	}

	for (size_t i = 0U; i < num_param; i++) {
		CHECKIF(params[i].stream == NULL ||
			params[i].qos == NULL ||
			(params[i].dir != BT_AUDIO_DIR_SINK &&
			 params[i].dir != BT_AUDIO_DIR_SOURCE)) {
			LOG_DBG("Invalid params[%zu] values", i);
			return -EINVAL;
		}

		if (params[i].stream->group != NULL) {
			LOG_DBG("params[%zu] stream (%p) already part of group %p", i,
				params[i].stream, params[i].stream->group);
			return -EALREADY;
		}

		if (group_qos == NULL) {
			group_qos = params[i].qos;
		} else if (!unicast_group_valid_qos(group_qos, params[i].qos)) {
			LOG_DBG("Stream[%zu] QoS incompatible with group QoS", i);
			return -EINVAL;
		}

		CHECKIF(!bt_audio_valid_qos(params[i].qos)) {
			LOG_DBG("Invalid QoS");
			return -EINVAL;
		}
	}

	unicast_group = unicast_group_alloc();
	if (unicast_group == NULL) {
		LOG_DBG("Could not allocate any more unicast groups");
		return -ENOMEM;
	}

	for (size_t i = 0U; i < num_param; i++) {
		err = unicast_group_add_stream(unicast_group, params[i].stream,
					       params[i].qos, params[i].dir);
		if (err < 0) {
			LOG_DBG("unicast_group_add_stream failed: %d", err);
			unicast_group_free(unicast_group);

			return err;
		}
	}

	err = bt_audio_cig_create(unicast_group, group_qos);
	if (err != 0) {
		LOG_DBG("bt_audio_cig_create failed: %d", err);
		unicast_group_free(unicast_group);

		return err;
	}

	*out_unicast_group = unicast_group;

	return 0;
}

int bt_audio_unicast_group_add_streams(struct bt_audio_unicast_group *unicast_group,
				       struct bt_audio_unicast_group_param params[],
				       size_t num_param)
{
	const struct bt_codec_qos *group_qos = unicast_group->qos;
	struct bt_audio_stream *tmp_stream;
	size_t total_stream_cnt;
	struct bt_iso_cig *cig;
	size_t num_added;
	int err;

	CHECKIF(unicast_group == NULL) {
		LOG_DBG("unicast_group is NULL");
		return -EINVAL;
	}

	CHECKIF(params == NULL) {
		LOG_DBG("params is NULL");
		return -EINVAL;
	}

	CHECKIF(num_param == 0) {
		LOG_DBG("num_param is 0");
		return -EINVAL;
	}

	for (size_t i = 0U; i < num_param; i++) {
		CHECKIF(params[i].stream == NULL ||
			params[i].qos == NULL ||
			(params[i].dir != BT_AUDIO_DIR_SINK &&
			 params[i].dir != BT_AUDIO_DIR_SOURCE)) {
			LOG_DBG("Invalid params[%zu] values", i);
			return -EINVAL;
		}

		if (params[i].stream->group != NULL) {
			LOG_DBG("params[%zu] stream (%p) already part of group %p", i,
				params[i].stream, params[i].stream->group);
			return -EALREADY;
		}

		if (group_qos == NULL) {
			group_qos = params[i].qos;
		} else if (!unicast_group_valid_qos(group_qos, params[i].qos)) {
			LOG_DBG("Stream[%zu] QoS incompatible with group QoS", i);
			return -EINVAL;
		}
	}

	total_stream_cnt = num_param;
	SYS_SLIST_FOR_EACH_CONTAINER(&unicast_group->streams, tmp_stream, _node) {
		total_stream_cnt++;
	}

	if (total_stream_cnt > UNICAST_GROUP_STREAM_CNT) {
		LOG_DBG("Too many streams provided: %u/%u", total_stream_cnt,
			UNICAST_GROUP_STREAM_CNT);
		return -EINVAL;

	}

	/* We can just check the CIG state to see if any streams have started as
	 * that would start the ISO connection procedure
	 */
	cig = unicast_group->cig;
	if (cig != NULL && cig->state != BT_ISO_CIG_STATE_CONFIGURED) {
		LOG_DBG("At least one unicast group stream is started");
		return -EBADMSG;
	}

	for (num_added = 0U; num_added < num_param; num_added++) {
		err = unicast_group_add_stream(unicast_group,
					       params[num_added].stream,
					       params[num_added].qos,
					       params[num_added].dir);
		if (err < 0) {
			LOG_DBG("unicast_group_add_stream failed: %d", err);
			goto fail;
		}
	}

	err = bt_audio_cig_reconfigure(unicast_group, group_qos);
	if (err != 0) {
		LOG_DBG("bt_audio_cig_reconfigure failed: %d", err);
		goto fail;
	}

	return 0;

fail:
	/* Restore group by removing the newly added streams */
	while (num_added--) {
		unicast_group_del_stream(unicast_group, params[num_added].stream);
	}

	return err;
}

int bt_audio_unicast_group_delete(struct bt_audio_unicast_group *unicast_group)
{
	CHECKIF(unicast_group == NULL) {
		LOG_DBG("unicast_group is NULL");
		return -EINVAL;
	}

	if (unicast_group->cig != NULL) {
		const int err = bt_audio_cig_terminate(unicast_group);

		if (err != 0) {
			LOG_DBG("bt_audio_cig_terminate failed with err %d", err);

			return err;
		}
	}

	unicast_group_free(unicast_group);

	return 0;
}

#endif /* CONFIG_BT_AUDIO_UNICAST_CLIENT */

int bt_audio_stream_reconfig(struct bt_audio_stream *stream,
			     const struct bt_codec *codec)
{
	uint8_t state;
	uint8_t role;
	int err;

	LOG_DBG("stream %p codec %p", stream, codec);

	CHECKIF(stream == NULL || stream->ep == NULL || stream->conn == NULL) {
		LOG_DBG("Invalid stream");
		return -EINVAL;
	}

	CHECKIF(codec == NULL) {
		LOG_DBG("codec is NULL");
		return -EINVAL;
	}

	state = stream->ep->status.state;
	switch (state) {
	/* Valid only if ASE_State field = 0x00 (Idle) */
	case BT_AUDIO_EP_STATE_IDLE:
	 /* or 0x01 (Codec Configured) */
	case BT_AUDIO_EP_STATE_CODEC_CONFIGURED:
	 /* or 0x02 (QoS Configured) */
	case BT_AUDIO_EP_STATE_QOS_CONFIGURED:
		break;
	default:
		LOG_ERR("Invalid state: %s", bt_audio_ep_state_str(state));
		return -EBADMSG;
	}

	role = stream->conn->role;
	if (IS_ENABLED(CONFIG_BT_AUDIO_UNICAST_CLIENT) &&
	    role == BT_HCI_ROLE_CENTRAL) {
		err = bt_unicast_client_config(stream, codec);
	} else if (IS_ENABLED(CONFIG_BT_AUDIO_UNICAST_SERVER) &&
		   role == BT_HCI_ROLE_PERIPHERAL) {
		err = bt_unicast_server_reconfig(stream, codec);
	} else {
		err = -EOPNOTSUPP;
	}

	if (err != 0) {
		LOG_DBG("reconfiguring stream failed: %d", err);
	} else {
		stream->codec = codec;
	}

	return 0;
}

int bt_audio_stream_start(struct bt_audio_stream *stream)
{
	uint8_t state;
	uint8_t role;
	int err;

	LOG_DBG("stream %p ep %p", stream, stream == NULL ? NULL : stream->ep);

	CHECKIF(stream == NULL || stream->ep == NULL || stream->conn == NULL) {
		LOG_DBG("Invalid stream");
		return -EINVAL;
	}

	state = stream->ep->status.state;
	switch (state) {
	/* Valid only if ASE_State field = 0x03 (Enabling) */
	case BT_AUDIO_EP_STATE_ENABLING:
		break;
	default:
		LOG_ERR("Invalid state: %s", bt_audio_ep_state_str(state));
		return -EBADMSG;
	}

	role = stream->conn->role;
	if (IS_ENABLED(CONFIG_BT_AUDIO_UNICAST_CLIENT) &&
	    role == BT_HCI_ROLE_CENTRAL) {
		err = bt_unicast_client_start(stream);
	} else if (IS_ENABLED(CONFIG_BT_AUDIO_UNICAST_SERVER) &&
		   role == BT_HCI_ROLE_PERIPHERAL) {
		err = bt_unicast_server_start(stream);
	} else {
		err = -EOPNOTSUPP;
	}

	if (err != 0) {
		LOG_DBG("Starting stream failed: %d", err);
		return err;
	}

	return 0;
}

int bt_audio_stream_metadata(struct bt_audio_stream *stream,
			     struct bt_codec_data *meta,
			     size_t meta_count)
{
	uint8_t state;
	uint8_t role;
	int err;

	LOG_DBG("stream %p metadata count %u", stream, meta_count);

	CHECKIF(stream == NULL || stream->ep == NULL || stream->conn == NULL) {
		LOG_DBG("Invalid stream");
		return -EINVAL;
	}

	CHECKIF((meta == NULL && meta_count != 0U) ||
		(meta != NULL && meta_count == 0U)) {
		LOG_DBG("Invalid meta (%p) or count (%zu)", meta, meta_count);
		return -EINVAL;
	}

	state = stream->ep->status.state;
	switch (state) {
	/* Valid for an ASE only if ASE_State field = 0x03 (Enabling) */
	case BT_AUDIO_EP_STATE_ENABLING:
	/* or 0x04 (Streaming) */
	case BT_AUDIO_EP_STATE_STREAMING:
		break;
	default:
		LOG_ERR("Invalid state: %s", bt_audio_ep_state_str(state));
		return -EBADMSG;
	}

	role = stream->conn->role;
	if (IS_ENABLED(CONFIG_BT_AUDIO_UNICAST_CLIENT) &&
	    role == BT_HCI_ROLE_CENTRAL) {
		err = bt_unicast_client_metadata(stream, meta, meta_count);
	} else if (IS_ENABLED(CONFIG_BT_AUDIO_UNICAST_SERVER) &&
		   role == BT_HCI_ROLE_PERIPHERAL) {
		err = bt_unicast_server_metadata(stream, meta, meta_count);
	} else {
		err = -EOPNOTSUPP;
	}

	if (err != 0) {
		LOG_DBG("Updating metadata failed: %d", err);
		return err;
	}

	return 0;
}

int bt_audio_stream_disable(struct bt_audio_stream *stream)
{
	uint8_t state;
	uint8_t role;
	int err;

	LOG_DBG("stream %p", stream);

	CHECKIF(stream == NULL || stream->ep == NULL || stream->conn == NULL) {
		LOG_DBG("Invalid stream");
		return -EINVAL;
	}

	state = stream->ep->status.state;
	switch (state) {
	/* Valid only if ASE_State field = 0x03 (Enabling) */
	case BT_AUDIO_EP_STATE_ENABLING:
	 /* or 0x04 (Streaming) */
	case BT_AUDIO_EP_STATE_STREAMING:
		break;
	default:
		LOG_ERR("Invalid state: %s", bt_audio_ep_state_str(state));
		return -EBADMSG;
	}

	role = stream->conn->role;
	if (IS_ENABLED(CONFIG_BT_AUDIO_UNICAST_CLIENT) &&
	    role == BT_HCI_ROLE_CENTRAL) {
		err = bt_unicast_client_disable(stream);
	} else if (IS_ENABLED(CONFIG_BT_AUDIO_UNICAST_SERVER) &&
		   role == BT_HCI_ROLE_PERIPHERAL) {
		err = bt_unicast_server_disable(stream);
	} else {
		err = -EOPNOTSUPP;
	}

	if (err != 0) {
		LOG_DBG("Disabling stream failed: %d", err);
		return err;
	}

	return 0;
}

int bt_audio_stream_release(struct bt_audio_stream *stream)
{
	uint8_t state;
	uint8_t role;
	int err;

	LOG_DBG("stream %p", stream);

	CHECKIF(stream == NULL || stream->ep == NULL || stream->conn == NULL) {
		LOG_DBG("Invalid stream");
		return -EINVAL;
	}

	state = stream->ep->status.state;
	switch (state) {
	/* Valid only if ASE_State field = 0x01 (Codec Configured) */
	case BT_AUDIO_EP_STATE_CODEC_CONFIGURED:
	 /* or 0x02 (QoS Configured) */
	case BT_AUDIO_EP_STATE_QOS_CONFIGURED:
	 /* or 0x03 (Enabling) */
	case BT_AUDIO_EP_STATE_ENABLING:
	 /* or 0x04 (Streaming) */
	case BT_AUDIO_EP_STATE_STREAMING:
	 /* or 0x04 (Disabling) */
	case BT_AUDIO_EP_STATE_DISABLING:
		break;
	default:
		LOG_ERR("Invalid state: %s", bt_audio_ep_state_str(state));
		return -EBADMSG;
	}

	role = stream->conn->role;
	if (IS_ENABLED(CONFIG_BT_AUDIO_UNICAST_CLIENT) &&
	    role == BT_HCI_ROLE_CENTRAL) {
		err = bt_unicast_client_release(stream);
	} else if (IS_ENABLED(CONFIG_BT_AUDIO_UNICAST_SERVER) &&
		   role == BT_HCI_ROLE_PERIPHERAL) {
		err = bt_unicast_server_release(stream);
	} else {
		err = -EOPNOTSUPP;
	}

	if (err != 0) {
		LOG_DBG("Releasing stream failed: %d", err);
		return err;
	}

	return 0;
}
#endif /* CONFIG_BT_AUDIO_UNICAST */
