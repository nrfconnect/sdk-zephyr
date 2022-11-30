/*
 * Audio Video Distribution Protocol
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#include <zephyr/kernel.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/util.h>

#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/l2cap.h>
#include <zephyr/bluetooth/avdtp.h>

#include "hci_core.h"
#include "conn_internal.h"
#include "l2cap_internal.h"
#include "avdtp_internal.h"

#define LOG_LEVEL CONFIG_BT_AVDTP_LOG_LEVEL
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(bt_avdtp);

#define AVDTP_MSG_POISTION 0x00
#define AVDTP_PKT_POSITION 0x02
#define AVDTP_TID_POSITION 0x04
#define AVDTP_SIGID_MASK 0x3f

#define AVDTP_GET_TR_ID(hdr) ((hdr & 0xf0) >> AVDTP_TID_POSITION)
#define AVDTP_GET_MSG_TYPE(hdr) (hdr & 0x03)
#define AVDTP_GET_PKT_TYPE(hdr) ((hdr & 0x0c) >> AVDTP_PKT_POSITION)
#define AVDTP_GET_SIG_ID(s) (s & AVDTP_SIGID_MASK)

static struct bt_avdtp_event_cb *event_cb;

static struct bt_avdtp_seid_lsep *lseps;

#define AVDTP_CHAN(_ch) CONTAINER_OF(_ch, struct bt_avdtp, br_chan.chan)

#define AVDTP_KWORK(_work) CONTAINER_OF(_work, struct bt_avdtp_req,\
					timeout_work)

#define AVDTP_TIMEOUT K_SECONDS(6)

static const struct {
	uint8_t sig_id;
	void (*func)(struct bt_avdtp *session, struct net_buf *buf,
		     uint8_t msg_type);
} handler[] = {
};

static int avdtp_send(struct bt_avdtp *session,
		      struct net_buf *buf, struct bt_avdtp_req *req)
{
	int result;
	struct bt_avdtp_single_sig_hdr *hdr;

	hdr = (struct bt_avdtp_single_sig_hdr *)buf->data;

	result = bt_l2cap_chan_send(&session->br_chan.chan, buf);
	if (result < 0) {
		LOG_ERR("Error:L2CAP send fail - result = %d", result);
		net_buf_unref(buf);
		return result;
	}

	/*Save the sent request*/
	req->sig = AVDTP_GET_SIG_ID(hdr->signal_id);
	req->tid = AVDTP_GET_TR_ID(hdr->hdr);
	LOG_DBG("sig 0x%02X, tid 0x%02X", req->sig, req->tid);

	session->req = req;
	/* Start timeout work */
	k_work_reschedule(&session->req->timeout_work, AVDTP_TIMEOUT);
	return result;
}

static struct net_buf *avdtp_create_pdu(uint8_t msg_type,
					uint8_t pkt_type,
					uint8_t sig_id)
{
	struct net_buf *buf;
	static uint8_t tid;
	struct bt_avdtp_single_sig_hdr *hdr;

	LOG_DBG("");

	buf = bt_l2cap_create_pdu(NULL, 0);

	hdr = net_buf_add(buf, sizeof(*hdr));

	hdr->hdr = (msg_type | pkt_type << AVDTP_PKT_POSITION |
		    tid++ << AVDTP_TID_POSITION);
	tid %= 16; /* Loop for 16*/
	hdr->signal_id = sig_id & AVDTP_SIGID_MASK;

	LOG_DBG("hdr = 0x%02X, Signal_ID = 0x%02X", hdr->hdr, hdr->signal_id);
	return buf;
}

/* Timeout handler */
static void avdtp_timeout(struct k_work *work)
{
	LOG_DBG("Failed Signal_id = %d", (AVDTP_KWORK(work))->sig);

	/* Gracefully Disconnect the Signalling and streaming L2cap chann*/

}

/* L2CAP Interface callbacks */
void bt_avdtp_l2cap_connected(struct bt_l2cap_chan *chan)
{
	struct bt_avdtp *session;

	if (!chan) {
		LOG_ERR("Invalid AVDTP chan");
		return;
	}

	session = AVDTP_CHAN(chan);
	LOG_DBG("chan %p session %p", chan, session);
	/* Init the timer */
	k_work_init_delayable(&session->req->timeout_work, avdtp_timeout);

}

void bt_avdtp_l2cap_disconnected(struct bt_l2cap_chan *chan)
{
	struct bt_avdtp *session = AVDTP_CHAN(chan);

	LOG_DBG("chan %p session %p", chan, session);
	session->br_chan.chan.conn = NULL;
	/* Clear the Pending req if set*/
}

void bt_avdtp_l2cap_encrypt_changed(struct bt_l2cap_chan *chan, uint8_t status)
{
	LOG_DBG("");
}

int bt_avdtp_l2cap_recv(struct bt_l2cap_chan *chan, struct net_buf *buf)
{
	struct bt_avdtp_single_sig_hdr *hdr;
	struct bt_avdtp *session = AVDTP_CHAN(chan);
	uint8_t i, msgtype, sigid, tid;

	if (buf->len < sizeof(*hdr)) {
		LOG_ERR("Recvd Wrong AVDTP Header");
		return 0;
	}

	hdr = net_buf_pull_mem(buf, sizeof(*hdr));
	msgtype = AVDTP_GET_MSG_TYPE(hdr->hdr);
	sigid = AVDTP_GET_SIG_ID(hdr->signal_id);
	tid = AVDTP_GET_TR_ID(hdr->hdr);

	LOG_DBG("msg_type[0x%02x] sig_id[0x%02x] tid[0x%02x]", msgtype, sigid, tid);

	/* validate if there is an outstanding resp expected*/
	if (msgtype != BT_AVDTP_CMD) {
		if (session->req == NULL) {
			LOG_DBG("Unexpected peer response");
			return 0;
		}

		if (session->req->sig != sigid ||
		    session->req->tid != tid) {
			LOG_DBG("Peer mismatch resp, expected sig[0x%02x]"
				"tid[0x%02x]", session->req->sig,
				session->req->tid);
			return 0;
		}
	}

	for (i = 0U; i < ARRAY_SIZE(handler); i++) {
		if (sigid == handler[i].sig_id) {
			handler[i].func(session, buf, msgtype);
			return 0;
		}
	}

	return 0;
}

/*A2DP Layer interface */
int bt_avdtp_connect(struct bt_conn *conn, struct bt_avdtp *session)
{
	static const struct bt_l2cap_chan_ops ops = {
		.connected = bt_avdtp_l2cap_connected,
		.disconnected = bt_avdtp_l2cap_disconnected,
		.encrypt_change = bt_avdtp_l2cap_encrypt_changed,
		.recv = bt_avdtp_l2cap_recv
	};

	if (!session) {
		return -EINVAL;
	}

	session->br_chan.chan.ops = &ops;
	session->br_chan.required_sec_level = BT_SECURITY_L2;

	return bt_l2cap_chan_connect(conn, &session->br_chan.chan,
				     BT_L2CAP_PSM_AVDTP);
}

int bt_avdtp_disconnect(struct bt_avdtp *session)
{
	if (!session) {
		return -EINVAL;
	}

	LOG_DBG("session %p", session);

	return bt_l2cap_chan_disconnect(&session->br_chan.chan);
}

int bt_avdtp_l2cap_accept(struct bt_conn *conn, struct bt_l2cap_chan **chan)
{
	struct bt_avdtp *session = NULL;
	int result;
	static const struct bt_l2cap_chan_ops ops = {
		.connected = bt_avdtp_l2cap_connected,
		.disconnected = bt_avdtp_l2cap_disconnected,
		.recv = bt_avdtp_l2cap_recv,
	};

	LOG_DBG("conn %p", conn);
	/* Get the AVDTP session from upper layer */
	result = event_cb->accept(conn, &session);
	if (result < 0) {
		return result;
	}
	session->br_chan.chan.ops = &ops;
	session->br_chan.rx.mtu = BT_AVDTP_MAX_MTU;
	*chan = &session->br_chan.chan;
	return 0;
}

/* Application will register its callback */
int bt_avdtp_register(struct bt_avdtp_event_cb *cb)
{
	LOG_DBG("");

	if (event_cb) {
		return -EALREADY;
	}

	event_cb = cb;

	return 0;
}

int bt_avdtp_register_sep(uint8_t media_type, uint8_t role,
			  struct bt_avdtp_seid_lsep *lsep)
{
	LOG_DBG("");

	static uint8_t bt_avdtp_seid = BT_AVDTP_MIN_SEID;

	if (!lsep) {
		return -EIO;
	}

	if (bt_avdtp_seid == BT_AVDTP_MAX_SEID) {
		return -EIO;
	}

	lsep->sep.id = bt_avdtp_seid++;
	lsep->sep.inuse = 0U;
	lsep->sep.media_type = media_type;
	lsep->sep.tsep = role;

	lsep->next = lseps;
	lseps = lsep;

	return 0;
}

/* init function */
int bt_avdtp_init(void)
{
	int err;
	static struct bt_l2cap_server avdtp_l2cap = {
		.psm = BT_L2CAP_PSM_AVDTP,
		.sec_level = BT_SECURITY_L2,
		.accept = bt_avdtp_l2cap_accept,
	};

	LOG_DBG("");

	/* Register AVDTP PSM with L2CAP */
	err = bt_l2cap_br_server_register(&avdtp_l2cap);
	if (err < 0) {
		LOG_ERR("AVDTP L2CAP Registration failed %d", err);
	}

	return err;
}

/* AVDTP Discover Request */
int bt_avdtp_discover(struct bt_avdtp *session,
		      struct bt_avdtp_discover_params *param)
{
	struct net_buf *buf;

	LOG_DBG("");
	if (!param || !session) {
		LOG_DBG("Error: Callback/Session not valid");
		return -EINVAL;
	}

	buf = avdtp_create_pdu(BT_AVDTP_CMD,
			       BT_AVDTP_PACKET_TYPE_SINGLE,
			       BT_AVDTP_DISCOVER);
	if (!buf) {
		LOG_ERR("Error: No Buff available");
		return -ENOMEM;
	}

	/* Body of the message */

	return avdtp_send(session, buf, &param->req);
}
