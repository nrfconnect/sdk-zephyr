/* conn.c - Bluetooth connection handling */

/*
 * Copyright (c) 2015-2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>
#include <sys/atomic.h>
#include <sys/byteorder.h>
#include <sys/util.h>
#include <sys/slist.h>
#include <debug/stack.h>
#include <sys/__assert.h>

#include <bluetooth/hci.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/conn.h>
#include <drivers/bluetooth/hci_driver.h>
#include <bluetooth/att.h>

#define BT_DBG_ENABLED IS_ENABLED(CONFIG_BT_DEBUG_CONN)
#define LOG_MODULE_NAME bt_conn
#include "common/log.h"

#include "hci_core.h"
#include "id.h"
#include "adv.h"
#include "conn_internal.h"
#include "l2cap_internal.h"
#include "keys.h"
#include "smp.h"
#include "ssp.h"
#include "att_internal.h"
#include "gatt_internal.h"
#include "iso_internal.h"

/* Peripheral timeout to initialize Connection Parameter Update procedure */
#define CONN_UPDATE_TIMEOUT  K_MSEC(CONFIG_BT_CONN_PARAM_UPDATE_TIMEOUT)

struct tx_meta {
	struct bt_conn_tx *tx;
};

#define tx_data(buf) ((struct tx_meta *)net_buf_user_data(buf))

NET_BUF_POOL_DEFINE(acl_tx_pool, CONFIG_BT_L2CAP_TX_BUF_COUNT,
		    BT_L2CAP_BUF_SIZE(CONFIG_BT_L2CAP_TX_MTU),
		    sizeof(struct tx_meta), NULL);

#if CONFIG_BT_L2CAP_TX_FRAG_COUNT > 0
/* Dedicated pool for fragment buffers in case queued up TX buffers don't
 * fit the controllers buffer size. We can't use the acl_tx_pool for the
 * fragmentation, since it's possible that pool is empty and all buffers
 * are queued up in the TX queue. In such a situation, trying to allocate
 * another buffer from the acl_tx_pool would result in a deadlock.
 */
NET_BUF_POOL_FIXED_DEFINE(frag_pool, CONFIG_BT_L2CAP_TX_FRAG_COUNT,
			  BT_BUF_ACL_SIZE(CONFIG_BT_BUF_ACL_TX_SIZE), NULL);

#endif /* CONFIG_BT_L2CAP_TX_FRAG_COUNT > 0 */

#if defined(CONFIG_BT_SMP) || defined(CONFIG_BT_BREDR)
const struct bt_conn_auth_cb *bt_auth;
#endif /* CONFIG_BT_SMP || CONFIG_BT_BREDR */

static struct bt_conn acl_conns[CONFIG_BT_MAX_CONN];
static struct bt_conn_cb *callback_list;

static struct bt_conn_tx conn_tx[CONFIG_BT_CONN_TX_MAX];
K_FIFO_DEFINE(free_tx);

#if defined(CONFIG_BT_BREDR)
static struct bt_conn sco_conns[CONFIG_BT_MAX_SCO_CONN];
#endif /* CONFIG_BT_BREDR */

struct k_sem *bt_conn_get_pkts(struct bt_conn *conn)
{
#if defined(CONFIG_BT_BREDR)
	if (conn->type == BT_CONN_TYPE_BR || !bt_dev.le.acl_mtu) {
		return &bt_dev.br.pkts;
	}
#endif /* CONFIG_BT_BREDR */
#if defined(CONFIG_BT_ISO)
	if (conn->type == BT_CONN_TYPE_ISO || bt_dev.le.iso_mtu) {
		if (bt_dev.le.iso_pkts.limit) {
			return &bt_dev.le.iso_pkts;
		}
	}
#endif /* CONFIG_BT_ISO */
	return &bt_dev.le.acl_pkts;
}

static inline const char *state2str(bt_conn_state_t state)
{
	switch (state) {
	case BT_CONN_DISCONNECTED:
		return "disconnected";
	case BT_CONN_DISCONNECT_COMPLETE:
		return "disconnect-complete";
	case BT_CONN_CONNECT_SCAN:
		return "connect-scan";
	case BT_CONN_CONNECT_DIR_ADV:
		return "connect-dir-adv";
	case BT_CONN_CONNECT_ADV:
		return "connect-adv";
	case BT_CONN_CONNECT_AUTO:
		return "connect-auto";
	case BT_CONN_CONNECT:
		return "connect";
	case BT_CONN_CONNECTED:
		return "connected";
	case BT_CONN_DISCONNECT:
		return "disconnect";
	default:
		return "(unknown)";
	}
}

static void notify_connected(struct bt_conn *conn)
{
	struct bt_conn_cb *cb;

	for (cb = callback_list; cb; cb = cb->_next) {
		if (cb->connected) {
			cb->connected(conn, conn->err);
		}
	}

	if (!conn->err) {
		bt_gatt_connected(conn);
	}
}

static void notify_disconnected(struct bt_conn *conn)
{
	struct bt_conn_cb *cb;

	for (cb = callback_list; cb; cb = cb->_next) {
		if (cb->disconnected) {
			cb->disconnected(conn, conn->err);
		}
	}
}

#if defined(CONFIG_BT_REMOTE_INFO)
void notify_remote_info(struct bt_conn *conn)
{
	struct bt_conn_remote_info remote_info;
	struct bt_conn_cb *cb;
	int err;

	err = bt_conn_get_remote_info(conn, &remote_info);
	if (err) {
		BT_DBG("Notify remote info failed %d", err);
		return;
	}

	for (cb = callback_list; cb; cb = cb->_next) {
		if (cb->remote_info_available) {
			cb->remote_info_available(conn, &remote_info);
		}
	}
}
#endif /* defined(CONFIG_BT_REMOTE_INFO) */

void notify_le_param_updated(struct bt_conn *conn)
{
	struct bt_conn_cb *cb;

	/* If new connection parameters meet requirement of pending
	 * parameters don't send slave conn param request anymore on timeout
	 */
	if (atomic_test_bit(conn->flags, BT_CONN_SLAVE_PARAM_SET) &&
	    conn->le.interval >= conn->le.interval_min &&
	    conn->le.interval <= conn->le.interval_max &&
	    conn->le.latency == conn->le.pending_latency &&
	    conn->le.timeout == conn->le.pending_timeout) {
		atomic_clear_bit(conn->flags, BT_CONN_SLAVE_PARAM_SET);
	}

	for (cb = callback_list; cb; cb = cb->_next) {
		if (cb->le_param_updated) {
			cb->le_param_updated(conn, conn->le.interval,
					     conn->le.latency,
					     conn->le.timeout);
		}
	}
}

#if defined(CONFIG_BT_USER_DATA_LEN_UPDATE)
void notify_le_data_len_updated(struct bt_conn *conn)
{
	struct bt_conn_cb *cb;

	for (cb = callback_list; cb; cb = cb->_next) {
		if (cb->le_data_len_updated) {
			cb->le_data_len_updated(conn, &conn->le.data_len);
		}
	}
}
#endif

#if defined(CONFIG_BT_USER_PHY_UPDATE)
void notify_le_phy_updated(struct bt_conn *conn)
{
	struct bt_conn_cb *cb;

	for (cb = callback_list; cb; cb = cb->_next) {
		if (cb->le_phy_updated) {
			cb->le_phy_updated(conn, &conn->le.phy);
		}
	}
}
#endif

bool le_param_req(struct bt_conn *conn, struct bt_le_conn_param *param)
{
	struct bt_conn_cb *cb;

	if (!bt_le_conn_params_valid(param)) {
		return false;
	}

	for (cb = callback_list; cb; cb = cb->_next) {
		if (!cb->le_param_req) {
			continue;
		}

		if (!cb->le_param_req(conn, param)) {
			return false;
		}

		/* The callback may modify the parameters so we need to
		 * double-check that it returned valid parameters.
		 */
		if (!bt_le_conn_params_valid(param)) {
			return false;
		}
	}

	/* Default to accepting if there's no app callback */
	return true;
}

static int send_conn_le_param_update(struct bt_conn *conn,
				const struct bt_le_conn_param *param)
{
	BT_DBG("conn %p features 0x%02x params (%d-%d %d %d)", conn,
	       conn->le.features[0], param->interval_min,
	       param->interval_max, param->latency, param->timeout);

	/* Proceed only if connection parameters contains valid values*/
	if (!bt_le_conn_params_valid(param)) {
		return -EINVAL;
	}

	/* Use LE connection parameter request if both local and remote support
	 * it; or if local role is master then use LE connection update.
	 */
	if ((BT_FEAT_LE_CONN_PARAM_REQ_PROC(bt_dev.le.features) &&
	     BT_FEAT_LE_CONN_PARAM_REQ_PROC(conn->le.features) &&
	     !atomic_test_bit(conn->flags, BT_CONN_SLAVE_PARAM_L2CAP)) ||
	     (conn->role == BT_HCI_ROLE_MASTER)) {
		int rc;

		rc = bt_conn_le_conn_update(conn, param);

		/* store those in case of fallback to L2CAP */
		if (rc == 0) {
			conn->le.pending_latency = param->latency;
			conn->le.pending_timeout = param->timeout;
		}

		return rc;
	}

	/* If remote master does not support LL Connection Parameters Request
	 * Procedure
	 */
	return bt_l2cap_update_conn_param(conn, param);
}

static void tx_free(struct bt_conn_tx *tx)
{
	tx->cb = NULL;
	tx->user_data = NULL;
	tx->pending_no_cb = 0U;
	k_fifo_put(&free_tx, tx);
}

static void tx_notify(struct bt_conn *conn)
{
	BT_DBG("conn %p", conn);

	while (1) {
		struct bt_conn_tx *tx;
		unsigned int key;
		bt_conn_tx_cb_t cb;
		void *user_data;

		key = irq_lock();
		if (sys_slist_is_empty(&conn->tx_complete)) {
			irq_unlock(key);
			break;
		}

		tx = (void *)sys_slist_get_not_empty(&conn->tx_complete);
		irq_unlock(key);

		BT_DBG("tx %p cb %p user_data %p", tx, tx->cb, tx->user_data);

		/* Copy over the params */
		cb = tx->cb;
		user_data = tx->user_data;

		/* Free up TX notify since there may be user waiting */
		tx_free(tx);

		/* Run the callback, at this point it should be safe to
		 * allocate new buffers since the TX should have been
		 * unblocked by tx_free.
		 */
		cb(conn, user_data);
	}
}

static void tx_complete_work(struct k_work *work)
{
	struct bt_conn *conn = CONTAINER_OF(work, struct bt_conn,
					   tx_complete_work);

	BT_DBG("conn %p", conn);

	tx_notify(conn);
}

static void deferred_work(struct k_work *work)
{
	struct bt_conn *conn = CONTAINER_OF(work, struct bt_conn, deferred_work);
	const struct bt_le_conn_param *param;

	BT_DBG("conn %p", conn);

	if (conn->state == BT_CONN_DISCONNECTED) {
		bt_l2cap_disconnected(conn);
		notify_disconnected(conn);

		/* Release the reference we took for the very first
		 * state transition.
		 */
		bt_conn_unref(conn);
		return;
	}

	if (conn->type != BT_CONN_TYPE_LE) {
		return;
	}

	if (IS_ENABLED(CONFIG_BT_CENTRAL) &&
	    conn->role == BT_CONN_ROLE_MASTER) {
		/* we don't call bt_conn_disconnect as it would also clear
		 * auto connect flag if it was set, instead just cancel
		 * connection directly
		 */
		bt_le_create_conn_cancel();
		return;
	}

	/* if application set own params use those, otherwise use defaults. */
	if (atomic_test_and_clear_bit(conn->flags,
				      BT_CONN_SLAVE_PARAM_SET)) {
		param = BT_LE_CONN_PARAM(conn->le.interval_min,
					 conn->le.interval_max,
					 conn->le.pending_latency,
					 conn->le.pending_timeout);
		send_conn_le_param_update(conn, param);
	} else if (IS_ENABLED(CONFIG_BT_GAP_AUTO_UPDATE_CONN_PARAMS)) {
#if defined(CONFIG_BT_GAP_PERIPHERAL_PREF_PARAMS)
		param = BT_LE_CONN_PARAM(
				CONFIG_BT_PERIPHERAL_PREF_MIN_INT,
				CONFIG_BT_PERIPHERAL_PREF_MAX_INT,
				CONFIG_BT_PERIPHERAL_PREF_SLAVE_LATENCY,
				CONFIG_BT_PERIPHERAL_PREF_TIMEOUT);
		send_conn_le_param_update(conn, param);
#endif
	}

	atomic_set_bit(conn->flags, BT_CONN_SLAVE_PARAM_UPDATE);
}

struct bt_conn *bt_conn_new(struct bt_conn *conns, size_t size)
{
	struct bt_conn *conn = NULL;
	int i;

	for (i = 0; i < size; i++) {
		if (atomic_cas(&conns[i].ref, 0, 1)) {
			conn = &conns[i];
			break;
		}
	}

	if (!conn) {
		return NULL;
	}

	(void)memset(conn, 0, offsetof(struct bt_conn, ref));

	return conn;
}

static struct bt_conn *acl_conn_new(void)
{
	struct bt_conn *conn;

	conn = bt_conn_new(acl_conns, ARRAY_SIZE(acl_conns));
	if (!conn) {
		return conn;
	}

	k_work_init_delayable(&conn->deferred_work, deferred_work);

	k_work_init(&conn->tx_complete_work, tx_complete_work);

	return conn;
}

#if defined(CONFIG_BT_BREDR)
void bt_sco_cleanup(struct bt_conn *sco_conn)
{
	bt_conn_unref(sco_conn->sco.acl);
	sco_conn->sco.acl = NULL;
	bt_conn_unref(sco_conn);
}

static struct bt_conn *sco_conn_new(void)
{
	return bt_conn_new(sco_conns, ARRAY_SIZE(sco_conns));
}

struct bt_conn *bt_conn_create_br(const bt_addr_t *peer,
				  const struct bt_br_conn_param *param)
{
	struct bt_hci_cp_connect *cp;
	struct bt_conn *conn;
	struct net_buf *buf;

	conn = bt_conn_lookup_addr_br(peer);
	if (conn) {
		switch (conn->state) {
		case BT_CONN_CONNECT:
		case BT_CONN_CONNECTED:
			return conn;
		default:
			bt_conn_unref(conn);
			return NULL;
		}
	}

	conn = bt_conn_add_br(peer);
	if (!conn) {
		return NULL;
	}

	buf = bt_hci_cmd_create(BT_HCI_OP_CONNECT, sizeof(*cp));
	if (!buf) {
		bt_conn_unref(conn);
		return NULL;
	}

	cp = net_buf_add(buf, sizeof(*cp));

	(void)memset(cp, 0, sizeof(*cp));

	memcpy(&cp->bdaddr, peer, sizeof(cp->bdaddr));
	cp->packet_type = sys_cpu_to_le16(0xcc18); /* DM1 DH1 DM3 DH5 DM5 DH5 */
	cp->pscan_rep_mode = 0x02; /* R2 */
	cp->allow_role_switch = param->allow_role_switch ? 0x01 : 0x00;
	cp->clock_offset = 0x0000; /* TODO used cached clock offset */

	if (bt_hci_cmd_send_sync(BT_HCI_OP_CONNECT, buf, NULL) < 0) {
		bt_conn_unref(conn);
		return NULL;
	}

	bt_conn_set_state(conn, BT_CONN_CONNECT);
	conn->role = BT_CONN_ROLE_MASTER;

	return conn;
}

struct bt_conn *bt_conn_create_sco(const bt_addr_t *peer)
{
	struct bt_hci_cp_setup_sync_conn *cp;
	struct bt_conn *sco_conn;
	struct net_buf *buf;
	int link_type;

	sco_conn = bt_conn_lookup_addr_sco(peer);
	if (sco_conn) {
		switch (sco_conn->state) {
		case BT_CONN_CONNECT:
		case BT_CONN_CONNECTED:
			return sco_conn;
		default:
			bt_conn_unref(sco_conn);
			return NULL;
		}
	}

	if (BT_FEAT_LMP_ESCO_CAPABLE(bt_dev.features)) {
		link_type = BT_HCI_ESCO;
	} else {
		link_type = BT_HCI_SCO;
	}

	sco_conn = bt_conn_add_sco(peer, link_type);
	if (!sco_conn) {
		return NULL;
	}

	buf = bt_hci_cmd_create(BT_HCI_OP_SETUP_SYNC_CONN, sizeof(*cp));
	if (!buf) {
		bt_sco_cleanup(sco_conn);
		return NULL;
	}

	cp = net_buf_add(buf, sizeof(*cp));

	(void)memset(cp, 0, sizeof(*cp));

	BT_ERR("handle : %x", sco_conn->sco.acl->handle);

	cp->handle = sco_conn->sco.acl->handle;
	cp->pkt_type = sco_conn->sco.pkt_type;
	cp->tx_bandwidth = 0x00001f40;
	cp->rx_bandwidth = 0x00001f40;
	cp->max_latency = 0x0007;
	cp->retrans_effort = 0x01;
	cp->content_format = BT_VOICE_CVSD_16BIT;

	if (bt_hci_cmd_send_sync(BT_HCI_OP_SETUP_SYNC_CONN, buf,
				 NULL) < 0) {
		bt_sco_cleanup(sco_conn);
		return NULL;
	}

	bt_conn_set_state(sco_conn, BT_CONN_CONNECT);

	return sco_conn;
}

struct bt_conn *bt_conn_lookup_addr_sco(const bt_addr_t *peer)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(sco_conns); i++) {
		struct bt_conn *conn = bt_conn_ref(&sco_conns[i]);

		if (!conn) {
			continue;
		}

		if (conn->type != BT_CONN_TYPE_SCO) {
			bt_conn_unref(conn);
			continue;
		}

		if (bt_addr_cmp(peer, &conn->sco.acl->br.dst) != 0) {
			bt_conn_unref(conn);
			continue;
		}

		return conn;
	}

	return NULL;
}

struct bt_conn *bt_conn_lookup_addr_br(const bt_addr_t *peer)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(acl_conns); i++) {
		struct bt_conn *conn = bt_conn_ref(&acl_conns[i]);

		if (!conn) {
			continue;
		}

		if (conn->type != BT_CONN_TYPE_BR) {
			bt_conn_unref(conn);
			continue;
		}

		if (bt_addr_cmp(peer, &conn->br.dst) != 0) {
			bt_conn_unref(conn);
			continue;
		}

		return conn;
	}

	return NULL;
}

struct bt_conn *bt_conn_add_sco(const bt_addr_t *peer, int link_type)
{
	struct bt_conn *sco_conn = sco_conn_new();

	if (!sco_conn) {
		return NULL;
	}

	sco_conn->sco.acl = bt_conn_lookup_addr_br(peer);
	sco_conn->type = BT_CONN_TYPE_SCO;

	if (link_type == BT_HCI_SCO) {
		if (BT_FEAT_LMP_ESCO_CAPABLE(bt_dev.features)) {
			sco_conn->sco.pkt_type = (bt_dev.br.esco_pkt_type &
						  ESCO_PKT_MASK);
		} else {
			sco_conn->sco.pkt_type = (bt_dev.br.esco_pkt_type &
						  SCO_PKT_MASK);
		}
	} else if (link_type == BT_HCI_ESCO) {
		sco_conn->sco.pkt_type = (bt_dev.br.esco_pkt_type &
					  ~EDR_ESCO_PKT_MASK);
	}

	return sco_conn;
}

struct bt_conn *bt_conn_add_br(const bt_addr_t *peer)
{
	struct bt_conn *conn = acl_conn_new();

	if (!conn) {
		return NULL;
	}

	bt_addr_copy(&conn->br.dst, peer);
	conn->type = BT_CONN_TYPE_BR;

	return conn;
}

static int bt_hci_connect_br_cancel(struct bt_conn *conn)
{
	struct bt_hci_cp_connect_cancel *cp;
	struct bt_hci_rp_connect_cancel *rp;
	struct net_buf *buf, *rsp;
	int err;

	buf = bt_hci_cmd_create(BT_HCI_OP_CONNECT_CANCEL, sizeof(*cp));
	if (!buf) {
		return -ENOBUFS;
	}

	cp = net_buf_add(buf, sizeof(*cp));
	memcpy(&cp->bdaddr, &conn->br.dst, sizeof(cp->bdaddr));

	err = bt_hci_cmd_send_sync(BT_HCI_OP_CONNECT_CANCEL, buf, &rsp);
	if (err) {
		return err;
	}

	rp = (void *)rsp->data;

	err = rp->status ? -EIO : 0;

	net_buf_unref(rsp);

	return err;
}

#endif /* CONFIG_BT_BREDR */

#if defined(CONFIG_BT_SMP)
void bt_conn_identity_resolved(struct bt_conn *conn)
{
	const bt_addr_le_t *rpa;
	struct bt_conn_cb *cb;

	if (conn->role == BT_HCI_ROLE_MASTER) {
		rpa = &conn->le.resp_addr;
	} else {
		rpa = &conn->le.init_addr;
	}

	for (cb = callback_list; cb; cb = cb->_next) {
		if (cb->identity_resolved) {
			cb->identity_resolved(conn, rpa, &conn->le.dst);
		}
	}
}

int bt_conn_le_start_encryption(struct bt_conn *conn, uint8_t rand[8],
				uint8_t ediv[2], const uint8_t *ltk, size_t len)
{
	struct bt_hci_cp_le_start_encryption *cp;
	struct net_buf *buf;

	buf = bt_hci_cmd_create(BT_HCI_OP_LE_START_ENCRYPTION, sizeof(*cp));
	if (!buf) {
		return -ENOBUFS;
	}

	cp = net_buf_add(buf, sizeof(*cp));
	cp->handle = sys_cpu_to_le16(conn->handle);
	memcpy(&cp->rand, rand, sizeof(cp->rand));
	memcpy(&cp->ediv, ediv, sizeof(cp->ediv));

	memcpy(cp->ltk, ltk, len);
	if (len < sizeof(cp->ltk)) {
		(void)memset(cp->ltk + len, 0, sizeof(cp->ltk) - len);
	}

	return bt_hci_cmd_send_sync(BT_HCI_OP_LE_START_ENCRYPTION, buf, NULL);
}
#endif /* CONFIG_BT_SMP */

#if defined(CONFIG_BT_SMP) || defined(CONFIG_BT_BREDR)
uint8_t bt_conn_enc_key_size(struct bt_conn *conn)
{
	if (!conn->encrypt) {
		return 0;
	}

	if (IS_ENABLED(CONFIG_BT_BREDR) &&
	    conn->type == BT_CONN_TYPE_BR) {
		struct bt_hci_cp_read_encryption_key_size *cp;
		struct bt_hci_rp_read_encryption_key_size *rp;
		struct net_buf *buf;
		struct net_buf *rsp;
		uint8_t key_size;

		buf = bt_hci_cmd_create(BT_HCI_OP_READ_ENCRYPTION_KEY_SIZE,
					sizeof(*cp));
		if (!buf) {
			return 0;
		}

		cp = net_buf_add(buf, sizeof(*cp));
		cp->handle = sys_cpu_to_le16(conn->handle);

		if (bt_hci_cmd_send_sync(BT_HCI_OP_READ_ENCRYPTION_KEY_SIZE,
					buf, &rsp)) {
			return 0;
		}

		rp = (void *)rsp->data;

		key_size = rp->status ? 0 : rp->key_size;

		net_buf_unref(rsp);

		return key_size;
	}

	if (IS_ENABLED(CONFIG_BT_SMP)) {
		return conn->le.keys ? conn->le.keys->enc_size : 0;
	}

	return 0;
}

static void reset_pairing(struct bt_conn *conn)
{
#if defined(CONFIG_BT_BREDR)
	if (conn->type == BT_CONN_TYPE_BR) {
		atomic_clear_bit(conn->flags, BT_CONN_BR_PAIRING);
		atomic_clear_bit(conn->flags, BT_CONN_BR_PAIRING_INITIATOR);
		atomic_clear_bit(conn->flags, BT_CONN_BR_LEGACY_SECURE);
	}
#endif /* CONFIG_BT_BREDR */

	/* Reset required security level to current operational */
	conn->required_sec_level = conn->sec_level;
}

void bt_conn_security_changed(struct bt_conn *conn, uint8_t hci_err,
			      enum bt_security_err err)
{
	struct bt_conn_cb *cb;

	reset_pairing(conn);
	bt_l2cap_security_changed(conn, hci_err);

	for (cb = callback_list; cb; cb = cb->_next) {
		if (cb->security_changed) {
			cb->security_changed(conn, conn->sec_level, err);
		}
	}
#if IS_ENABLED(CONFIG_BT_KEYS_OVERWRITE_OLDEST)
	if (!err && conn->sec_level >= BT_SECURITY_L2) {
		if (conn->type == BT_CONN_TYPE_LE) {
			bt_keys_update_usage(conn->id, bt_conn_get_dst(conn));
		}

#if defined(CONFIG_BT_BREDR)
		if (conn->type == BT_CONN_TYPE_BR) {
			bt_keys_link_key_update_usage(&conn->br.dst);
		}
#endif /* CONFIG_BT_BREDR */

	}
#endif
}

static int start_security(struct bt_conn *conn)
{
	if (IS_ENABLED(CONFIG_BT_BREDR) && conn->type == BT_CONN_TYPE_BR) {
		return bt_ssp_start_security(conn);
	}

	if (IS_ENABLED(CONFIG_BT_SMP)) {
		return bt_smp_start_security(conn);
	}

	return -EINVAL;
}

int bt_conn_set_security(struct bt_conn *conn, bt_security_t sec)
{
	int err;

	if (conn->state != BT_CONN_CONNECTED) {
		return -ENOTCONN;
	}

	if (IS_ENABLED(CONFIG_BT_SMP_SC_ONLY)) {
		sec = BT_SECURITY_L4;
	}

	if (IS_ENABLED(CONFIG_BT_SMP_OOB_LEGACY_PAIR_ONLY)) {
		sec = BT_SECURITY_L3;
	}

	/* nothing to do */
	if (conn->sec_level >= sec || conn->required_sec_level >= sec) {
		return 0;
	}

	atomic_set_bit_to(conn->flags, BT_CONN_FORCE_PAIR,
			  sec & BT_SECURITY_FORCE_PAIR);
	conn->required_sec_level = sec & ~BT_SECURITY_FORCE_PAIR;

	err = start_security(conn);

	/* reset required security level in case of error */
	if (err) {
		conn->required_sec_level = conn->sec_level;
	}

	return err;
}

bt_security_t bt_conn_get_security(struct bt_conn *conn)
{
	return conn->sec_level;
}
#else
bt_security_t bt_conn_get_security(struct bt_conn *conn)
{
	return BT_SECURITY_L1;
}
#endif /* CONFIG_BT_SMP */

void bt_conn_cb_register(struct bt_conn_cb *cb)
{
	cb->_next = callback_list;
	callback_list = cb;
}

void bt_conn_reset_rx_state(struct bt_conn *conn)
{
	if (!conn->rx) {
		return;
	}

	net_buf_unref(conn->rx);
	conn->rx = NULL;
}

void bt_conn_recv(struct bt_conn *conn, struct net_buf *buf, uint8_t flags)
{
	uint16_t acl_total_len;
	/* Make sure we notify any pending TX callbacks before processing
	 * new data for this connection.
	 */
	tx_notify(conn);

	BT_DBG("handle %u len %u flags %02x", conn->handle, buf->len, flags);

	if (IS_ENABLED(CONFIG_BT_ISO) &&
	    conn->type == BT_CONN_TYPE_ISO) {
		bt_iso_recv(conn, buf, flags);
		return;
	}

	/* Check packet boundary flags */
	switch (flags) {
	case BT_ACL_START:
		if (conn->rx) {
			BT_ERR("Unexpected first L2CAP frame");
			bt_conn_reset_rx_state(conn);
		}

		BT_DBG("First, len %u final %u", buf->len,
		       (buf->len < sizeof(uint16_t)) ?
		       0 : sys_get_le16(buf->data));

		conn->rx = buf;
		break;
	case BT_ACL_CONT:
		if (!conn->rx) {
			BT_ERR("Unexpected L2CAP continuation");
			bt_conn_reset_rx_state(conn);
			net_buf_unref(buf);
			return;
		}

		if (!buf->len) {
			BT_DBG("Empty ACL_CONT");
			net_buf_unref(buf);
			return;
		}

		if (buf->len > net_buf_tailroom(conn->rx)) {
			BT_ERR("Not enough buffer space for L2CAP data");
			bt_conn_reset_rx_state(conn);
			net_buf_unref(buf);
			return;
		}

		net_buf_add_mem(conn->rx, buf->data, buf->len);
		net_buf_unref(buf);
		break;
	default:
		/* BT_ACL_START_NO_FLUSH and BT_ACL_COMPLETE are not allowed on
		 * LE-U from Controller to Host.
		 * Only BT_ACL_POINT_TO_POINT is supported.
		 */
		BT_ERR("Unexpected ACL flags (0x%02x)", flags);
		bt_conn_reset_rx_state(conn);
		net_buf_unref(buf);
		return;
	}

	if (conn->rx->len < sizeof(uint16_t)) {
		/* Still not enough data recieved to retrieve the L2CAP header
		 * length field.
		 */
		return;
	}

	acl_total_len = sys_get_le16(conn->rx->data) + sizeof(struct bt_l2cap_hdr);

	if (conn->rx->len < acl_total_len) {
		/* L2CAP frame not complete. */
		return;
	}

	if (conn->rx->len > acl_total_len) {
		BT_ERR("ACL len mismatch (%u > %u)",
		       conn->rx->len, acl_total_len);
		bt_conn_reset_rx_state(conn);
		return;
	}

	/* L2CAP frame complete. */
	buf = conn->rx;
	conn->rx = NULL;

	BT_DBG("Successfully parsed %u byte L2CAP packet", buf->len);
	bt_l2cap_recv(conn, buf);
}

static struct bt_conn_tx *conn_tx_alloc(void)
{
	/* The TX context always get freed in the system workqueue,
	 * so if we're in the same workqueue but there are no immediate
	 * contexts available, there's no chance we'll get one by waiting.
	 */
	if (k_current_get() == &k_sys_work_q.thread) {
		return k_fifo_get(&free_tx, K_NO_WAIT);
	}

	if (IS_ENABLED(CONFIG_BT_DEBUG_CONN)) {
		struct bt_conn_tx *tx = k_fifo_get(&free_tx, K_NO_WAIT);

		if (tx) {
			return tx;
		}

		BT_WARN("Unable to get an immediate free conn_tx");
	}

	return k_fifo_get(&free_tx, K_FOREVER);
}

int bt_conn_send_cb(struct bt_conn *conn, struct net_buf *buf,
		    bt_conn_tx_cb_t cb, void *user_data)
{
	struct bt_conn_tx *tx;

	BT_DBG("conn handle %u buf len %u cb %p user_data %p", conn->handle,
	       buf->len, cb, user_data);

	if (conn->state != BT_CONN_CONNECTED) {
		BT_ERR("not connected!");
		return -ENOTCONN;
	}

	if (cb) {
		tx = conn_tx_alloc();
		if (!tx) {
			BT_ERR("Unable to allocate TX context");
			return -ENOBUFS;
		}

		/* Verify that we're still connected after blocking */
		if (conn->state != BT_CONN_CONNECTED) {
			BT_WARN("Disconnected while allocating context");
			tx_free(tx);
			return -ENOTCONN;
		}

		tx->cb = cb;
		tx->user_data = user_data;
		tx->pending_no_cb = 0U;

		tx_data(buf)->tx = tx;
	} else {
		tx_data(buf)->tx = NULL;
	}

	net_buf_put(&conn->tx_queue, buf);
	return 0;
}

enum {
	FRAG_START,
	FRAG_CONT,
	FRAG_SINGLE,
	FRAG_END
};

static int send_acl(struct bt_conn *conn, struct net_buf *buf, uint8_t flags)
{
	struct bt_hci_acl_hdr *hdr;

	switch (flags) {
	case FRAG_START:
	case FRAG_SINGLE:
		flags = BT_ACL_START_NO_FLUSH;
		break;
	case FRAG_CONT:
	case FRAG_END:
		flags = BT_ACL_CONT;
		break;
	default:
		return -EINVAL;
	}

	hdr = net_buf_push(buf, sizeof(*hdr));
	hdr->handle = sys_cpu_to_le16(bt_acl_handle_pack(conn->handle, flags));
	hdr->len = sys_cpu_to_le16(buf->len - sizeof(*hdr));

	bt_buf_set_type(buf, BT_BUF_ACL_OUT);

	return bt_send(buf);
}

static int send_iso(struct bt_conn *conn, struct net_buf *buf, uint8_t flags)
{
	struct bt_hci_iso_hdr *hdr;

	switch (flags) {
	case FRAG_START:
		flags = BT_ISO_START;
		break;
	case FRAG_CONT:
		flags = BT_ISO_CONT;
		break;
	case FRAG_SINGLE:
		flags = BT_ISO_SINGLE;
		break;
	case FRAG_END:
		flags = BT_ISO_END;
		break;
	default:
		return -EINVAL;
	}

	hdr = net_buf_push(buf, sizeof(*hdr));
	hdr->handle = sys_cpu_to_le16(bt_iso_handle_pack(conn->handle, flags,
							 0));
	hdr->len = sys_cpu_to_le16(buf->len - sizeof(*hdr));

	bt_buf_set_type(buf, BT_BUF_ISO_OUT);

	return bt_send(buf);
}

static bool send_frag(struct bt_conn *conn, struct net_buf *buf, uint8_t flags,
		      bool always_consume)
{
	struct bt_conn_tx *tx = tx_data(buf)->tx;
	uint32_t *pending_no_cb;
	unsigned int key;
	int err;

	BT_DBG("conn %p buf %p len %u flags 0x%02x", conn, buf, buf->len,
	       flags);

	/* Wait until the controller can accept ACL packets */
	k_sem_take(bt_conn_get_pkts(conn), K_FOREVER);

	/* Check for disconnection while waiting for pkts_sem */
	if (conn->state != BT_CONN_CONNECTED) {
		goto fail;
	}

	/* Add to pending, it must be done before bt_buf_set_type */
	key = irq_lock();
	if (tx) {
		sys_slist_append(&conn->tx_pending, &tx->node);
	} else {
		struct bt_conn_tx *tail_tx;

		tail_tx = (void *)sys_slist_peek_tail(&conn->tx_pending);
		if (tail_tx) {
			pending_no_cb = &tail_tx->pending_no_cb;
		} else {
			pending_no_cb = &conn->pending_no_cb;
		}

		(*pending_no_cb)++;
	}
	irq_unlock(key);

	if (IS_ENABLED(CONFIG_BT_ISO) && conn->type == BT_CONN_TYPE_ISO) {
		err = send_iso(conn, buf, flags);
	} else {
		err = send_acl(conn, buf, flags);
	}

	if (err) {
		BT_ERR("Unable to send to driver (err %d)", err);
		key = irq_lock();
		/* Roll back the pending TX info */
		if (tx) {
			sys_slist_find_and_remove(&conn->tx_pending, &tx->node);
		} else {
			__ASSERT_NO_MSG(*pending_no_cb > 0);
			(*pending_no_cb)--;
		}
		irq_unlock(key);
		goto fail;
	}

	return true;

fail:
	k_sem_give(bt_conn_get_pkts(conn));
	if (tx) {
		tx_free(tx);
	}

	if (always_consume) {
		net_buf_unref(buf);
	}
	return false;
}

static inline uint16_t conn_mtu(struct bt_conn *conn)
{
#if defined(CONFIG_BT_BREDR)
	if (conn->type == BT_CONN_TYPE_BR || !bt_dev.le.acl_mtu) {
		return bt_dev.br.mtu;
	}
#endif /* CONFIG_BT_BREDR */
#if defined(CONFIG_BT_ISO)
	if (conn->type == BT_CONN_TYPE_ISO && bt_dev.le.iso_mtu) {
		return bt_dev.le.iso_mtu;
	}
#endif /* CONFIG_BT_ISO */
	return bt_dev.le.acl_mtu;
}

static struct net_buf *create_frag(struct bt_conn *conn, struct net_buf *buf)
{
	struct net_buf *frag;
	uint16_t frag_len;

	switch (conn->type) {
#if defined(CONFIG_BT_ISO)
	case BT_CONN_TYPE_ISO:
		frag = bt_iso_create_frag(0);
		break;
#endif
	default:
		frag = bt_conn_create_frag(0);
	}

	if (conn->state != BT_CONN_CONNECTED) {
		net_buf_unref(frag);
		return NULL;
	}

	/* Fragments never have a TX completion callback */
	tx_data(frag)->tx = NULL;

	frag_len = MIN(conn_mtu(conn), net_buf_tailroom(frag));

	net_buf_add_mem(frag, buf->data, frag_len);
	net_buf_pull(buf, frag_len);

	return frag;
}

static bool send_buf(struct bt_conn *conn, struct net_buf *buf)
{
	struct net_buf *frag;

	BT_DBG("conn %p buf %p len %u", conn, buf, buf->len);

	/* Send directly if the packet fits the ACL MTU */
	if (buf->len <= conn_mtu(conn)) {
		return send_frag(conn, buf, FRAG_SINGLE, false);
	}

	/* Create & enqueue first fragment */
	frag = create_frag(conn, buf);
	if (!frag) {
		return false;
	}

	if (!send_frag(conn, frag, FRAG_START, true)) {
		return false;
	}

	/*
	 * Send the fragments. For the last one simply use the original
	 * buffer (which works since we've used net_buf_pull on it.
	 */
	while (buf->len > conn_mtu(conn)) {
		frag = create_frag(conn, buf);
		if (!frag) {
			return false;
		}

		if (!send_frag(conn, frag, FRAG_CONT, true)) {
			return false;
		}
	}

	return send_frag(conn, buf, FRAG_END, false);
}

static struct k_poll_signal conn_change =
		K_POLL_SIGNAL_INITIALIZER(conn_change);

static void conn_cleanup(struct bt_conn *conn)
{
	struct net_buf *buf;

	/* Give back any allocated buffers */
	while ((buf = net_buf_get(&conn->tx_queue, K_NO_WAIT))) {
		if (tx_data(buf)->tx) {
			tx_free(tx_data(buf)->tx);
		}

		net_buf_unref(buf);
	}

	__ASSERT(sys_slist_is_empty(&conn->tx_pending), "Pending TX packets");
	__ASSERT_NO_MSG(conn->pending_no_cb == 0);

	bt_conn_reset_rx_state(conn);

	k_work_reschedule(&conn->deferred_work, K_NO_WAIT);
}

static int conn_prepare_events(struct bt_conn *conn,
			       struct k_poll_event *events)
{
	if (!atomic_get(&conn->ref)) {
		return -ENOTCONN;
	}

	if (conn->state == BT_CONN_DISCONNECTED &&
	    atomic_test_and_clear_bit(conn->flags, BT_CONN_CLEANUP)) {
		conn_cleanup(conn);
		return -ENOTCONN;
	}

	if (conn->state != BT_CONN_CONNECTED) {
		return -ENOTCONN;
	}

	BT_DBG("Adding conn %p to poll list", conn);

	k_poll_event_init(&events[0],
			K_POLL_TYPE_FIFO_DATA_AVAILABLE,
			K_POLL_MODE_NOTIFY_ONLY,
			&conn->tx_queue);
	events[0].tag = BT_EVENT_CONN_TX_QUEUE;

	return 0;
}

int bt_conn_prepare_events(struct k_poll_event events[])
{
	int i, ev_count = 0;
	struct bt_conn *conn;

	BT_DBG("");

	conn_change.signaled = 0U;
	k_poll_event_init(&events[ev_count++], K_POLL_TYPE_SIGNAL,
			  K_POLL_MODE_NOTIFY_ONLY, &conn_change);

	for (i = 0; i < ARRAY_SIZE(acl_conns); i++) {
		conn = &acl_conns[i];

		if (!conn_prepare_events(conn, &events[ev_count])) {
			ev_count++;
		}
	}

#if defined(CONFIG_BT_ISO)
	for (i = 0; i < ARRAY_SIZE(iso_conns); i++) {
		conn = &iso_conns[i];

		if (!conn_prepare_events(conn, &events[ev_count])) {
			ev_count++;
		}
	}
#endif

	return ev_count;
}

void bt_conn_process_tx(struct bt_conn *conn)
{
	struct net_buf *buf;

	BT_DBG("conn %p", conn);

	if (conn->state == BT_CONN_DISCONNECTED &&
	    atomic_test_and_clear_bit(conn->flags, BT_CONN_CLEANUP)) {
		BT_DBG("handle %u disconnected - cleaning up", conn->handle);
		conn_cleanup(conn);
		return;
	}

	/* Get next ACL packet for connection */
	buf = net_buf_get(&conn->tx_queue, K_NO_WAIT);
	BT_ASSERT(buf);
	if (!send_buf(conn, buf)) {
		net_buf_unref(buf);
	}
}

bool bt_conn_exists_le(uint8_t id, const bt_addr_le_t *peer)
{
	struct bt_conn *conn = bt_conn_lookup_addr_le(id, peer);

	if (conn) {
		/* Connection object already exists.
		 * If the connection state is not "disconnected",then the
		 * connection was created but has not yet been disconnected.
		 * If the connection state is "disconnected" then the connection
		 * still has valid references. The last reference of the stack
		 * is released after the disconnected callback.
		 */
		BT_WARN("Found valid connection in %s state",
			state2str(conn->state));
		bt_conn_unref(conn);
		return true;
	}

	return false;
}

struct bt_conn *bt_conn_add_le(uint8_t id, const bt_addr_le_t *peer)
{
	struct bt_conn *conn = acl_conn_new();

	if (!conn) {
		return NULL;
	}

	conn->id = id;
	bt_addr_le_copy(&conn->le.dst, peer);
#if defined(CONFIG_BT_SMP)
	conn->sec_level = BT_SECURITY_L1;
	conn->required_sec_level = BT_SECURITY_L1;
#endif /* CONFIG_BT_SMP */
	conn->type = BT_CONN_TYPE_LE;
	conn->le.interval_min = BT_GAP_INIT_CONN_INT_MIN;
	conn->le.interval_max = BT_GAP_INIT_CONN_INT_MAX;

	return conn;
}

static void process_unack_tx(struct bt_conn *conn)
{
	/* Return any unacknowledged packets */
	while (1) {
		struct bt_conn_tx *tx;
		sys_snode_t *node;
		unsigned int key;

		key = irq_lock();

		if (conn->pending_no_cb) {
			conn->pending_no_cb--;
			irq_unlock(key);
			k_sem_give(bt_conn_get_pkts(conn));
			continue;
		}

		node = sys_slist_get(&conn->tx_pending);
		irq_unlock(key);

		if (!node) {
			break;
		}

		tx = CONTAINER_OF(node, struct bt_conn_tx, node);

		key = irq_lock();
		conn->pending_no_cb = tx->pending_no_cb;
		tx->pending_no_cb = 0U;
		irq_unlock(key);

		tx_free(tx);

		k_sem_give(bt_conn_get_pkts(conn));
	}
}

struct bt_conn *conn_lookup_handle(struct bt_conn *conns, size_t size,
				   uint16_t handle)
{
	int i;

	for (i = 0; i < size; i++) {
		struct bt_conn *conn = bt_conn_ref(&conns[i]);

		if (!conn) {
			continue;
		}

		/* We only care about connections with a valid handle */
		if (!bt_conn_is_handle_valid(conn)) {
			bt_conn_unref(conn);
			continue;
		}

		if (conn->handle != handle) {
			bt_conn_unref(conn);
			continue;
		}

		return conn;
	}

	return NULL;
}

struct bt_conn *conn_lookup_iso(struct bt_conn *conn)
{
#if defined(CONFIG_BT_ISO)
	int i;

	for (i = 0; i < ARRAY_SIZE(iso_conns); i++) {
		struct bt_conn *iso_conn = bt_conn_ref(&iso_conns[i]);

		if (!iso_conn) {
			continue;
		}

		if (iso_conn == conn) {
			return iso_conn;
		}

		if (bt_conn_iso(iso_conn)->acl == conn) {
			return iso_conn;
		}

		bt_conn_unref(iso_conn);
	}

	return NULL;
#else
	return NULL;
#endif /* CONFIG_BT_ISO */
}

void bt_conn_connected(struct bt_conn *conn)
{
	bt_l2cap_connected(conn);
	notify_connected(conn);
}

void bt_conn_set_state(struct bt_conn *conn, bt_conn_state_t state)
{
	bt_conn_state_t old_state;

	BT_DBG("%s -> %s", state2str(conn->state), state2str(state));

	if (conn->state == state) {
		BT_WARN("no transition %s", state2str(state));
		return;
	}

	old_state = conn->state;
	conn->state = state;

	/* Actions needed for exiting the old state */
	switch (old_state) {
	case BT_CONN_DISCONNECTED:
		/* Take a reference for the first state transition after
		 * bt_conn_add_le() and keep it until reaching DISCONNECTED
		 * again.
		 */
		bt_conn_ref(conn);
		break;
	case BT_CONN_CONNECT:
		if (IS_ENABLED(CONFIG_BT_CENTRAL) &&
		    conn->type == BT_CONN_TYPE_LE) {
			k_work_cancel_delayable(&conn->deferred_work);
		}
		break;
	default:
		break;
	}

	/* Actions needed for entering the new state */
	switch (conn->state) {
	case BT_CONN_CONNECTED:
		if (conn->type == BT_CONN_TYPE_SCO) {
			/* TODO: Notify sco connected */
			break;
		}
		k_fifo_init(&conn->tx_queue);
		k_poll_signal_raise(&conn_change, 0);

		if (IS_ENABLED(CONFIG_BT_ISO) &&
		    conn->type == BT_CONN_TYPE_ISO) {
			bt_iso_connected(conn);
			break;
		}

		sys_slist_init(&conn->channels);

		if (IS_ENABLED(CONFIG_BT_PERIPHERAL) &&
		    conn->role == BT_CONN_ROLE_SLAVE) {
			k_work_schedule(&conn->deferred_work,
					CONN_UPDATE_TIMEOUT);
		}

		break;
	case BT_CONN_DISCONNECTED:
		if (conn->type == BT_CONN_TYPE_SCO) {
			/* TODO: Notify sco disconnected */
			bt_conn_unref(conn);
			break;
		}

		if (IS_ENABLED(CONFIG_BT_ISO)) {
			struct bt_conn *iso;

			iso = conn_lookup_iso(conn);
			if (iso) {
				iso->err = conn->err;

				bt_iso_disconnected(iso);
				bt_iso_cleanup(iso);
				bt_conn_unref(iso);

				/* Stop if only ISO was Disconnected */
				if (iso == conn) {
					break;
				}
			}
		}

		/* Notify disconnection and queue a dummy buffer to wake
		 * up and stop the tx thread for states where it was
		 * running.
		 */
		switch (old_state) {
		case BT_CONN_DISCONNECT_COMPLETE:
			tx_notify(conn);

			/* Cancel Connection Update if it is pending */
			if (conn->type == BT_CONN_TYPE_LE) {
				k_work_cancel_delayable(&conn->deferred_work);
			}

			atomic_set_bit(conn->flags, BT_CONN_CLEANUP);
			k_poll_signal_raise(&conn_change, 0);
			/* The last ref will be dropped during cleanup */
			break;
		case BT_CONN_CONNECT:
			/* LE Create Connection command failed. This might be
			 * directly from the API, don't notify application in
			 * this case.
			 */
			if (conn->err) {
				notify_connected(conn);
			}

			bt_conn_unref(conn);
			break;
		case BT_CONN_CONNECT_SCAN:
			/* this indicate LE Create Connection with peer address
			 * has been stopped. This could either be triggered by
			 * the application through bt_conn_disconnect or by
			 * timeout set by bt_conn_le_create_param.timeout.
			 */
			if (conn->err) {
				notify_connected(conn);
			}

			bt_conn_unref(conn);
			break;
		case BT_CONN_CONNECT_DIR_ADV:
			/* this indicate Directed advertising stopped */
			if (conn->err) {
				notify_connected(conn);
			}

			bt_conn_unref(conn);
			break;
		case BT_CONN_CONNECT_AUTO:
			/* this indicates LE Create Connection with filter
			 * policy has been stopped. This can only be triggered
			 * by the application, so don't notify.
			 */
			bt_conn_unref(conn);
			break;
		case BT_CONN_CONNECT_ADV:
			/* This can only happen when application stops the
			 * advertiser, conn->err is never set in this case.
			 */
			bt_conn_unref(conn);
			break;
		case BT_CONN_CONNECTED:
		case BT_CONN_DISCONNECT:
		case BT_CONN_DISCONNECTED:
			/* Cannot happen. */
			BT_WARN("Invalid (%u) old state", state);
			break;
		}
		break;
	case BT_CONN_CONNECT_AUTO:
		break;
	case BT_CONN_CONNECT_ADV:
		break;
	case BT_CONN_CONNECT_SCAN:
		break;
	case BT_CONN_CONNECT_DIR_ADV:
		break;
	case BT_CONN_CONNECT:
		if (conn->type == BT_CONN_TYPE_SCO) {
			break;
		}
		/*
		 * Timer is needed only for LE. For other link types controller
		 * will handle connection timeout.
		 */
		if (IS_ENABLED(CONFIG_BT_CENTRAL) &&
		    conn->type == BT_CONN_TYPE_LE) {
			k_work_schedule(&conn->deferred_work,
					K_MSEC(10 * bt_dev.create_param.timeout));
		}

		break;
	case BT_CONN_DISCONNECT:
		break;
	case BT_CONN_DISCONNECT_COMPLETE:
		process_unack_tx(conn);
		break;
	default:
		BT_WARN("no valid (%u) state was set", state);

		break;
	}
}

struct bt_conn *bt_conn_lookup_handle(uint16_t handle)
{
	struct bt_conn *conn;

	conn = conn_lookup_handle(acl_conns, ARRAY_SIZE(acl_conns), handle);
	if (conn) {
		return conn;
	}

#if defined(CONFIG_BT_ISO)
	conn = conn_lookup_handle(iso_conns, ARRAY_SIZE(iso_conns), handle);
	if (conn) {
		return conn;
	}
#endif

 #if defined(CONFIG_BT_BREDR)
	conn = conn_lookup_handle(sco_conns, ARRAY_SIZE(sco_conns), handle);
	if (conn) {
		return conn;
	}
#endif

	return NULL;
}

bool bt_conn_is_peer_addr_le(const struct bt_conn *conn, uint8_t id,
			     const bt_addr_le_t *peer)
{
	if (id != conn->id) {
		return false;
	}

	/* Check against conn dst address as it may be the identity address */
	if (!bt_addr_le_cmp(peer, &conn->le.dst)) {
		return true;
	}

	/* Check against initial connection address */
	if (conn->role == BT_HCI_ROLE_MASTER) {
		return bt_addr_le_cmp(peer, &conn->le.resp_addr) == 0;
	}

	return bt_addr_le_cmp(peer, &conn->le.init_addr) == 0;
}

struct bt_conn *bt_conn_lookup_addr_le(uint8_t id, const bt_addr_le_t *peer)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(acl_conns); i++) {
		struct bt_conn *conn = bt_conn_ref(&acl_conns[i]);

		if (!conn) {
			continue;
		}

		if (conn->type != BT_CONN_TYPE_LE) {
			bt_conn_unref(conn);
			continue;
		}

		if (!bt_conn_is_peer_addr_le(conn, id, peer)) {
			bt_conn_unref(conn);
			continue;
		}

		return conn;
	}

	return NULL;
}

struct bt_conn *bt_conn_lookup_state_le(uint8_t id, const bt_addr_le_t *peer,
					const bt_conn_state_t state)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(acl_conns); i++) {
		struct bt_conn *conn = bt_conn_ref(&acl_conns[i]);

		if (!conn) {
			continue;
		}

		if (conn->type != BT_CONN_TYPE_LE) {
			bt_conn_unref(conn);
			continue;
		}

		if (peer && !bt_conn_is_peer_addr_le(conn, id, peer)) {
			bt_conn_unref(conn);
			continue;
		}

		if (!(conn->state == state && conn->id == id)) {
			bt_conn_unref(conn);
			continue;
		}

		return conn;
	}

	return NULL;
}

void bt_conn_foreach(int type, void (*func)(struct bt_conn *conn, void *data),
		     void *data)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(acl_conns); i++) {
		struct bt_conn *conn = bt_conn_ref(&acl_conns[i]);

		if (!conn) {
			continue;
		}

		if (!(conn->type & type)) {
			bt_conn_unref(conn);
			continue;
		}

		func(conn, data);
		bt_conn_unref(conn);
	}
#if defined(CONFIG_BT_BREDR)
	if (type & BT_CONN_TYPE_SCO) {
		for (i = 0; i < ARRAY_SIZE(sco_conns); i++) {
			struct bt_conn *conn = bt_conn_ref(&sco_conns[i]);

			if (!conn) {
				continue;
			}

			func(conn, data);
			bt_conn_unref(conn);
		}
	}
#endif /* defined(CONFIG_BT_BREDR) */

#if defined(CONFIG_BT_ISO)
	if (type & BT_CONN_TYPE_ISO) {
		for (i = 0; i < ARRAY_SIZE(iso_conns); i++) {
			struct bt_conn *conn = bt_conn_ref(&iso_conns[i]);

			if (!conn) {
				continue;
			}

			func(conn, data);
			bt_conn_unref(conn);
		}
	}
#endif /* defined(CONFIG_BT_ISO) */
}

struct bt_conn *bt_conn_ref(struct bt_conn *conn)
{
	atomic_val_t old;

	/* Reference counter must be checked to avoid incrementing ref from
	 * zero, then we should return NULL instead.
	 * Loop on clear-and-set in case someone has modified the reference
	 * count since the read, and start over again when that happens.
	 */
	do {
		old = atomic_get(&conn->ref);

		if (!old) {
			return NULL;
		}
	} while (!atomic_cas(&conn->ref, old, old + 1));

	BT_DBG("handle %u ref %u -> %u", conn->handle, old, old + 1);

	return conn;
}

void bt_conn_unref(struct bt_conn *conn)
{
	atomic_val_t old = atomic_dec(&conn->ref);

	BT_DBG("handle %u ref %u -> %u", conn->handle, old,
	       atomic_get(&conn->ref));

	__ASSERT(old > 0, "Conn reference counter is 0");

	if (IS_ENABLED(CONFIG_BT_PERIPHERAL) &&
	    atomic_get(&conn->ref) == 0) {
		bt_le_adv_resume();
	}
}

const bt_addr_le_t *bt_conn_get_dst(const struct bt_conn *conn)
{
	return &conn->le.dst;
}

int bt_conn_get_info(const struct bt_conn *conn, struct bt_conn_info *info)
{
	info->type = conn->type;
	info->role = conn->role;
	info->id = conn->id;

	switch (conn->type) {
	case BT_CONN_TYPE_LE:
		info->le.dst = &conn->le.dst;
		info->le.src = &bt_dev.id_addr[conn->id];
		if (conn->role == BT_HCI_ROLE_MASTER) {
			info->le.local = &conn->le.init_addr;
			info->le.remote = &conn->le.resp_addr;
		} else {
			info->le.local = &conn->le.resp_addr;
			info->le.remote = &conn->le.init_addr;
		}
		info->le.interval = conn->le.interval;
		info->le.latency = conn->le.latency;
		info->le.timeout = conn->le.timeout;
#if defined(CONFIG_BT_USER_PHY_UPDATE)
		info->le.phy = &conn->le.phy;
#endif
#if defined(CONFIG_BT_USER_DATA_LEN_UPDATE)
		info->le.data_len = &conn->le.data_len;
#endif
		return 0;
#if defined(CONFIG_BT_BREDR)
	case BT_CONN_TYPE_BR:
		info->br.dst = &conn->br.dst;
		return 0;
#endif
	}

	return -EINVAL;
}

int bt_conn_get_remote_info(struct bt_conn *conn,
			    struct bt_conn_remote_info *remote_info)
{
	if (!atomic_test_bit(conn->flags, BT_CONN_AUTO_FEATURE_EXCH) ||
	    (IS_ENABLED(CONFIG_BT_REMOTE_VERSION) &&
	     !atomic_test_bit(conn->flags, BT_CONN_AUTO_VERSION_INFO))) {
		return -EBUSY;
	}

	remote_info->type = conn->type;
#if defined(CONFIG_BT_REMOTE_VERSION)
	/* The conn->rv values will be just zeroes if the operation failed */
	remote_info->version = conn->rv.version;
	remote_info->manufacturer = conn->rv.manufacturer;
	remote_info->subversion = conn->rv.subversion;
#else
	remote_info->version = 0;
	remote_info->manufacturer = 0;
	remote_info->subversion = 0;
#endif

	switch (conn->type) {
	case BT_CONN_TYPE_LE:
		remote_info->le.features = conn->le.features;
		return 0;
#if defined(CONFIG_BT_BREDR)
	case BT_CONN_TYPE_BR:
		/* TODO: Make sure the HCI commands to read br features and
		*  extended features has finished. */
		return -ENOTSUP;
#endif
	default:
		return -EINVAL;
	}
}

/* Read Transmit Power Level HCI command */
static int bt_conn_get_tx_power_level(struct bt_conn *conn, uint8_t type,
				      int8_t *tx_power_level)
{
	int err;
	struct bt_hci_rp_read_tx_power_level *rp;
	struct net_buf *rsp;
	struct bt_hci_cp_read_tx_power_level *cp;
	struct net_buf *buf;

	buf = bt_hci_cmd_create(BT_HCI_OP_READ_TX_POWER_LEVEL, sizeof(*cp));
	if (!buf) {
		return -ENOBUFS;
	}

	cp = net_buf_add(buf, sizeof(*cp));
	cp->type = type;
	cp->handle = sys_cpu_to_le16(conn->handle);

	err = bt_hci_cmd_send_sync(BT_HCI_OP_READ_TX_POWER_LEVEL, buf, &rsp);
	if (err) {
		return err;
	}

	rp = (void *) rsp->data;
	*tx_power_level = rp->tx_power_level;
	net_buf_unref(rsp);

	return 0;
}

int bt_conn_le_get_tx_power_level(struct bt_conn *conn,
				  struct bt_conn_le_tx_power *tx_power_level)
{
	int err;

	if (tx_power_level->phy != 0) {
		/* Extend the implementation when LE Enhanced Read Transmit
		 * Power Level HCI command is available for use.
		 */
		return -ENOTSUP;
	}

	err = bt_conn_get_tx_power_level(conn, BT_TX_POWER_LEVEL_CURRENT,
					 &tx_power_level->current_level);
	if (err) {
		return err;
	}

	err = bt_conn_get_tx_power_level(conn, BT_TX_POWER_LEVEL_MAX,
					 &tx_power_level->max_level);
	return err;
}

static int conn_disconnect(struct bt_conn *conn, uint8_t reason)
{
	int err;

	err = bt_hci_disconnect(conn->handle, reason);
	if (err) {
		return err;
	}

	if (conn->state == BT_CONN_CONNECTED) {
		bt_conn_set_state(conn, BT_CONN_DISCONNECT);
	}

	return 0;
}

int bt_conn_le_param_update(struct bt_conn *conn,
			    const struct bt_le_conn_param *param)
{
	BT_DBG("conn %p features 0x%02x params (%d-%d %d %d)", conn,
	       conn->le.features[0], param->interval_min,
	       param->interval_max, param->latency, param->timeout);

	/* Check if there's a need to update conn params */
	if (conn->le.interval >= param->interval_min &&
	    conn->le.interval <= param->interval_max &&
	    conn->le.latency == param->latency &&
	    conn->le.timeout == param->timeout) {
		atomic_clear_bit(conn->flags, BT_CONN_SLAVE_PARAM_SET);
		return -EALREADY;
	}

	if (IS_ENABLED(CONFIG_BT_CENTRAL) &&
	    conn->role == BT_CONN_ROLE_MASTER) {
		return send_conn_le_param_update(conn, param);
	}

	if (IS_ENABLED(CONFIG_BT_PERIPHERAL)) {
		/* if slave conn param update timer expired just send request */
		if (atomic_test_bit(conn->flags, BT_CONN_SLAVE_PARAM_UPDATE)) {
			return send_conn_le_param_update(conn, param);
		}

		/* store new conn params to be used by update timer */
		conn->le.interval_min = param->interval_min;
		conn->le.interval_max = param->interval_max;
		conn->le.pending_latency = param->latency;
		conn->le.pending_timeout = param->timeout;
		atomic_set_bit(conn->flags, BT_CONN_SLAVE_PARAM_SET);
	}

	return 0;
}

#if defined(CONFIG_BT_USER_DATA_LEN_UPDATE)
int bt_conn_le_data_len_update(struct bt_conn *conn,
			       const struct bt_conn_le_data_len_param *param)
{
	if (conn->le.data_len.tx_max_len == param->tx_max_len &&
	    conn->le.data_len.tx_max_time == param->tx_max_time) {
		return -EALREADY;
	}

	if (IS_ENABLED(CONFIG_BT_AUTO_DATA_LEN_UPDATE) &&
	    !atomic_test_bit(conn->flags, BT_CONN_AUTO_DATA_LEN_COMPLETE)) {
		return -EAGAIN;
	}

	return bt_le_set_data_len(conn, param->tx_max_len, param->tx_max_time);
}
#endif

#if defined(CONFIG_BT_USER_PHY_UPDATE)
int bt_conn_le_phy_update(struct bt_conn *conn,
			  const struct bt_conn_le_phy_param *param)
{
	uint8_t phy_opts, all_phys;

	if (IS_ENABLED(CONFIG_BT_AUTO_PHY_UPDATE) &&
	    !atomic_test_bit(conn->flags, BT_CONN_AUTO_PHY_COMPLETE)) {
		return -EAGAIN;
	}

	if ((param->options & BT_CONN_LE_PHY_OPT_CODED_S2) &&
	    (param->options & BT_CONN_LE_PHY_OPT_CODED_S8)) {
		phy_opts = BT_HCI_LE_PHY_CODED_ANY;
	} else if (param->options & BT_CONN_LE_PHY_OPT_CODED_S2) {
		phy_opts = BT_HCI_LE_PHY_CODED_S2;
	} else if (param->options & BT_CONN_LE_PHY_OPT_CODED_S8) {
		phy_opts = BT_HCI_LE_PHY_CODED_S8;
	} else {
		phy_opts = BT_HCI_LE_PHY_CODED_ANY;
	}

	all_phys = 0U;
	if (param->pref_tx_phy == BT_GAP_LE_PHY_NONE) {
		all_phys |= BT_HCI_LE_PHY_TX_ANY;
	}

	if (param->pref_rx_phy == BT_GAP_LE_PHY_NONE) {
		all_phys |= BT_HCI_LE_PHY_RX_ANY;
	}

	return bt_le_set_phy(conn, all_phys, param->pref_tx_phy,
			     param->pref_rx_phy, phy_opts);
}
#endif

int bt_conn_disconnect(struct bt_conn *conn, uint8_t reason)
{
	/* Disconnection is initiated by us, so auto connection shall
	 * be disabled. Otherwise the passive scan would be enabled
	 * and we could send LE Create Connection as soon as the remote
	 * starts advertising.
	 */
#if !defined(CONFIG_BT_WHITELIST)
	if (IS_ENABLED(CONFIG_BT_CENTRAL) &&
	    conn->type == BT_CONN_TYPE_LE) {
		bt_le_set_auto_conn(&conn->le.dst, NULL);
	}
#endif /* !defined(CONFIG_BT_WHITELIST) */

	switch (conn->state) {
	case BT_CONN_CONNECT_SCAN:
		conn->err = reason;
		bt_conn_set_state(conn, BT_CONN_DISCONNECTED);
		if (IS_ENABLED(CONFIG_BT_CENTRAL)) {
			bt_le_scan_update(false);
		}
		return 0;
	case BT_CONN_CONNECT:
#if defined(CONFIG_BT_BREDR)
		if (conn->type == BT_CONN_TYPE_BR) {
			return bt_hci_connect_br_cancel(conn);
		}
#endif /* CONFIG_BT_BREDR */

		if (IS_ENABLED(CONFIG_BT_CENTRAL)) {
			k_work_cancel_delayable(&conn->deferred_work);
			return bt_le_create_conn_cancel();
		}

		return 0;
	case BT_CONN_CONNECTED:
		return conn_disconnect(conn, reason);
	case BT_CONN_DISCONNECT:
		return 0;
	case BT_CONN_DISCONNECTED:
	default:
		return -ENOTCONN;
	}
}

#if defined(CONFIG_BT_CENTRAL)
static void bt_conn_set_param_le(struct bt_conn *conn,
				 const struct bt_le_conn_param *param)
{
	conn->le.interval_min = param->interval_min;
	conn->le.interval_max = param->interval_max;
	conn->le.latency = param->latency;
	conn->le.timeout = param->timeout;
}

static bool create_param_validate(const struct bt_conn_le_create_param *param)
{
#if defined(CONFIG_BT_PRIVACY)
	/* Initiation timeout cannot be greater than the RPA timeout */
	const uint32_t timeout_max = (MSEC_PER_SEC / 10) * CONFIG_BT_RPA_TIMEOUT;

	if (param->timeout > timeout_max) {
		return false;
	}
#endif

	return true;
}

static void create_param_setup(const struct bt_conn_le_create_param *param)
{
	bt_dev.create_param = *param;

	bt_dev.create_param.timeout =
		(bt_dev.create_param.timeout != 0) ?
		bt_dev.create_param.timeout :
		(MSEC_PER_SEC / 10) * CONFIG_BT_CREATE_CONN_TIMEOUT;

	bt_dev.create_param.interval_coded =
		(bt_dev.create_param.interval_coded != 0) ?
		bt_dev.create_param.interval_coded :
		bt_dev.create_param.interval;

	bt_dev.create_param.window_coded =
		(bt_dev.create_param.window_coded != 0) ?
		bt_dev.create_param.window_coded :
		bt_dev.create_param.window;
}

#if defined(CONFIG_BT_WHITELIST)
int bt_conn_le_create_auto(const struct bt_conn_le_create_param *create_param,
			   const struct bt_le_conn_param *param)
{
	struct bt_conn *conn;
	int err;

	if (!atomic_test_bit(bt_dev.flags, BT_DEV_READY)) {
		return -EAGAIN;
	}

	if (!bt_le_conn_params_valid(param)) {
		return -EINVAL;
	}

	conn = bt_conn_lookup_state_le(BT_ID_DEFAULT, BT_ADDR_LE_NONE,
				       BT_CONN_CONNECT_AUTO);
	if (conn) {
		bt_conn_unref(conn);
		return -EALREADY;
	}

	/* Scanning either to connect or explicit scan, either case scanner was
	 * started by application and should not be stopped.
	 */
	if (atomic_test_bit(bt_dev.flags, BT_DEV_SCANNING)) {
		return -EINVAL;
	}

	if (atomic_test_bit(bt_dev.flags, BT_DEV_INITIATING)) {
		return -EINVAL;
	}

	if (!bt_id_scan_random_addr_check()) {
		return -EINVAL;
	}

	conn = bt_conn_add_le(BT_ID_DEFAULT, BT_ADDR_LE_NONE);
	if (!conn) {
		return -ENOMEM;
	}

	bt_conn_set_param_le(conn, param);
	create_param_setup(create_param);

	atomic_set_bit(conn->flags, BT_CONN_AUTO_CONNECT);
	bt_conn_set_state(conn, BT_CONN_CONNECT_AUTO);

	err = bt_le_create_conn(conn);
	if (err) {
		BT_ERR("Failed to start whitelist scan");
		conn->err = 0;
		bt_conn_set_state(conn, BT_CONN_DISCONNECTED);
		bt_conn_unref(conn);
		return err;
	}

	/* Since we don't give the application a reference to manage in
	 * this case, we need to release this reference here.
	 */
	bt_conn_unref(conn);
	return 0;
}

int bt_conn_create_auto_stop(void)
{
	struct bt_conn *conn;
	int err;

	if (!atomic_test_bit(bt_dev.flags, BT_DEV_READY)) {
		return -EINVAL;
	}

	conn = bt_conn_lookup_state_le(BT_ID_DEFAULT, BT_ADDR_LE_NONE,
				       BT_CONN_CONNECT_AUTO);
	if (!conn) {
		return -EINVAL;
	}

	if (!atomic_test_bit(bt_dev.flags, BT_DEV_INITIATING)) {
		return -EINVAL;
	}

	bt_conn_set_state(conn, BT_CONN_DISCONNECTED);
	bt_conn_unref(conn);

	err = bt_le_create_conn_cancel();
	if (err) {
		BT_ERR("Failed to stop initiator");
		return err;
	}

	return 0;
}
#endif /* defined(CONFIG_BT_WHITELIST) */

int bt_conn_le_create(const bt_addr_le_t *peer,
		      const struct bt_conn_le_create_param *create_param,
		      const struct bt_le_conn_param *conn_param,
		      struct bt_conn **ret_conn)
{
	struct bt_conn *conn;
	bt_addr_le_t dst;
	int err;

	if (!atomic_test_bit(bt_dev.flags, BT_DEV_READY)) {
		return -EAGAIN;
	}

	if (!bt_le_conn_params_valid(conn_param)) {
		return -EINVAL;
	}

	if (!create_param_validate(create_param)) {
		return -EINVAL;
	}

	if (atomic_test_bit(bt_dev.flags, BT_DEV_EXPLICIT_SCAN)) {
		return -EAGAIN;
	}

	if (atomic_test_bit(bt_dev.flags, BT_DEV_INITIATING)) {
		return -EALREADY;
	}

	if (!bt_id_scan_random_addr_check()) {
		return -EINVAL;
	}

	if (bt_conn_exists_le(BT_ID_DEFAULT, peer)) {
		return -EINVAL;
	}

	if (peer->type == BT_ADDR_LE_PUBLIC_ID ||
	    peer->type == BT_ADDR_LE_RANDOM_ID) {
		bt_addr_le_copy(&dst, peer);
		dst.type -= BT_ADDR_LE_PUBLIC_ID;
	} else {
		bt_addr_le_copy(&dst, bt_lookup_id_addr(BT_ID_DEFAULT, peer));
	}

	/* Only default identity supported for now */
	conn = bt_conn_add_le(BT_ID_DEFAULT, &dst);
	if (!conn) {
		return -ENOMEM;
	}

	bt_conn_set_param_le(conn, conn_param);
	create_param_setup(create_param);

#if defined(CONFIG_BT_SMP)
	if (!bt_dev.le.rl_size || bt_dev.le.rl_entries > bt_dev.le.rl_size) {
		bt_conn_set_state(conn, BT_CONN_CONNECT_SCAN);

		err = bt_le_scan_update(true);
		if (err) {
			bt_conn_set_state(conn, BT_CONN_DISCONNECTED);
			bt_conn_unref(conn);

			return err;
		}

		*ret_conn = conn;
		return 0;
	}
#endif

	bt_conn_set_state(conn, BT_CONN_CONNECT);

	err = bt_le_create_conn(conn);
	if (err) {
		conn->err = 0;
		bt_conn_set_state(conn, BT_CONN_DISCONNECTED);
		bt_conn_unref(conn);

		bt_le_scan_update(false);
		return err;
	}

	*ret_conn = conn;
	return 0;
}

#if !defined(CONFIG_BT_WHITELIST)
int bt_le_set_auto_conn(const bt_addr_le_t *addr,
			const struct bt_le_conn_param *param)
{
	struct bt_conn *conn;

	if (!atomic_test_bit(bt_dev.flags, BT_DEV_READY)) {
		return -EAGAIN;
	}

	if (param && !bt_le_conn_params_valid(param)) {
		return -EINVAL;
	}

	if (!bt_id_scan_random_addr_check()) {
		return -EINVAL;
	}

	/* Only default identity is supported */
	conn = bt_conn_lookup_addr_le(BT_ID_DEFAULT, addr);
	if (!conn) {
		conn = bt_conn_add_le(BT_ID_DEFAULT, addr);
		if (!conn) {
			return -ENOMEM;
		}
	}

	if (param) {
		bt_conn_set_param_le(conn, param);

		if (!atomic_test_and_set_bit(conn->flags,
					     BT_CONN_AUTO_CONNECT)) {
			bt_conn_ref(conn);
		}
	} else {
		if (atomic_test_and_clear_bit(conn->flags,
					      BT_CONN_AUTO_CONNECT)) {
			bt_conn_unref(conn);
			if (conn->state == BT_CONN_CONNECT_SCAN) {
				bt_conn_set_state(conn, BT_CONN_DISCONNECTED);
			}
		}
	}

	if (conn->state == BT_CONN_DISCONNECTED &&
	    atomic_test_bit(bt_dev.flags, BT_DEV_READY)) {
		if (param) {
			bt_conn_set_state(conn, BT_CONN_CONNECT_SCAN);
		}
		bt_le_scan_update(false);
	}

	bt_conn_unref(conn);

	return 0;
}
#endif /* !defined(CONFIG_BT_WHITELIST) */
#endif /* CONFIG_BT_CENTRAL */

int bt_conn_le_conn_update(struct bt_conn *conn,
			   const struct bt_le_conn_param *param)
{
	struct hci_cp_le_conn_update *conn_update;
	struct net_buf *buf;

	buf = bt_hci_cmd_create(BT_HCI_OP_LE_CONN_UPDATE,
				sizeof(*conn_update));
	if (!buf) {
		return -ENOBUFS;
	}

	conn_update = net_buf_add(buf, sizeof(*conn_update));
	(void)memset(conn_update, 0, sizeof(*conn_update));
	conn_update->handle = sys_cpu_to_le16(conn->handle);
	conn_update->conn_interval_min = sys_cpu_to_le16(param->interval_min);
	conn_update->conn_interval_max = sys_cpu_to_le16(param->interval_max);
	conn_update->conn_latency = sys_cpu_to_le16(param->latency);
	conn_update->supervision_timeout = sys_cpu_to_le16(param->timeout);

	return bt_hci_cmd_send_sync(BT_HCI_OP_LE_CONN_UPDATE, buf, NULL);
}

#if defined(CONFIG_NET_BUF_LOG)
struct net_buf *bt_conn_create_frag_timeout_debug(size_t reserve,
						  k_timeout_t timeout,
						  const char *func, int line)
#else
struct net_buf *bt_conn_create_frag_timeout(size_t reserve, k_timeout_t timeout)
#endif
{
	struct net_buf_pool *pool = NULL;

#if CONFIG_BT_L2CAP_TX_FRAG_COUNT > 0
	pool = &frag_pool;
#endif

#if defined(CONFIG_NET_BUF_LOG)
	return bt_conn_create_pdu_timeout_debug(pool, reserve, timeout,
						func, line);
#else
	return bt_conn_create_pdu_timeout(pool, reserve, timeout);
#endif /* CONFIG_NET_BUF_LOG */
}

#if defined(CONFIG_NET_BUF_LOG)
struct net_buf *bt_conn_create_pdu_timeout_debug(struct net_buf_pool *pool,
						 size_t reserve,
						 k_timeout_t timeout,
						 const char *func, int line)
#else
struct net_buf *bt_conn_create_pdu_timeout(struct net_buf_pool *pool,
					   size_t reserve, k_timeout_t timeout)
#endif
{
	struct net_buf *buf;

	/*
	 * PDU must not be allocated from ISR as we block with 'K_FOREVER'
	 * during the allocation
	 */
	__ASSERT_NO_MSG(!k_is_in_isr());

	if (!pool) {
		pool = &acl_tx_pool;
	}

	if (IS_ENABLED(CONFIG_BT_DEBUG_CONN)) {
#if defined(CONFIG_NET_BUF_LOG)
		buf = net_buf_alloc_fixed_debug(pool, K_NO_WAIT, func, line);
#else
		buf = net_buf_alloc(pool, K_NO_WAIT);
#endif
		if (!buf) {
			BT_WARN("Unable to allocate buffer with K_NO_WAIT");
#if defined(CONFIG_NET_BUF_LOG)
			buf = net_buf_alloc_fixed_debug(pool, timeout, func,
							line);
#else
			buf = net_buf_alloc(pool, timeout);
#endif
		}
	} else {
#if defined(CONFIG_NET_BUF_LOG)
		buf = net_buf_alloc_fixed_debug(pool, timeout, func,
							line);
#else
		buf = net_buf_alloc(pool, timeout);
#endif
	}

	if (!buf) {
		BT_WARN("Unable to allocate buffer within timeout");
		return NULL;
	}

	reserve += sizeof(struct bt_hci_acl_hdr) + BT_BUF_RESERVE;
	net_buf_reserve(buf, reserve);

	return buf;
}

#if defined(CONFIG_BT_SMP) || defined(CONFIG_BT_BREDR)
int bt_conn_auth_cb_register(const struct bt_conn_auth_cb *cb)
{
	if (!cb) {
		bt_auth = NULL;
		return 0;
	}

	if (bt_auth) {
		return -EALREADY;
	}

	/* The cancel callback must always be provided if the app provides
	 * interactive callbacks.
	 */
	if (!cb->cancel &&
	    (cb->passkey_display || cb->passkey_entry || cb->passkey_confirm ||
#if defined(CONFIG_BT_BREDR)
	     cb->pincode_entry ||
#endif
	     cb->pairing_confirm)) {
		return -EINVAL;
	}

	bt_auth = cb;
	return 0;
}

int bt_conn_auth_passkey_entry(struct bt_conn *conn, unsigned int passkey)
{
	if (!bt_auth) {
		return -EINVAL;
	}

	if (IS_ENABLED(CONFIG_BT_SMP) && conn->type == BT_CONN_TYPE_LE) {
		bt_smp_auth_passkey_entry(conn, passkey);
		return 0;
	}

	if (IS_ENABLED(CONFIG_BT_BREDR) && conn->type == BT_CONN_TYPE_BR) {
		return bt_ssp_auth_passkey_entry(conn, passkey);
	}

	return -EINVAL;
}

int bt_conn_auth_passkey_confirm(struct bt_conn *conn)
{
	if (!bt_auth) {
		return -EINVAL;
	}

	if (IS_ENABLED(CONFIG_BT_SMP) &&
	    conn->type == BT_CONN_TYPE_LE) {
		return bt_smp_auth_passkey_confirm(conn);
	}

	if (IS_ENABLED(CONFIG_BT_BREDR) &&
	    conn->type == BT_CONN_TYPE_BR) {
		return bt_ssp_auth_passkey_confirm(conn);
	}

	return -EINVAL;
}

int bt_conn_auth_cancel(struct bt_conn *conn)
{
	if (!bt_auth) {
		return -EINVAL;
	}

	if (IS_ENABLED(CONFIG_BT_SMP) && conn->type == BT_CONN_TYPE_LE) {
		return bt_smp_auth_cancel(conn);
	}

#if defined(CONFIG_BT_BREDR)
	if (conn->type == BT_CONN_TYPE_BR) {
		return bt_ssp_auth_cancel(conn);
	}
#endif /* CONFIG_BT_BREDR */

	return -EINVAL;
}

int bt_conn_auth_pairing_confirm(struct bt_conn *conn)
{
	if (!bt_auth) {
		return -EINVAL;
	}

	switch (conn->type) {
#if defined(CONFIG_BT_SMP)
	case BT_CONN_TYPE_LE:
		return bt_smp_auth_pairing_confirm(conn);
#endif /* CONFIG_BT_SMP */
#if defined(CONFIG_BT_BREDR)
	case BT_CONN_TYPE_BR:
		return bt_ssp_auth_pairing_confirm(conn);
#endif /* CONFIG_BT_BREDR */
	default:
		return -EINVAL;
	}
}
#endif /* CONFIG_BT_SMP || CONFIG_BT_BREDR */

uint8_t bt_conn_index(struct bt_conn *conn)
{
	ptrdiff_t index;

	switch (conn->type) {
#if defined(CONFIG_BT_ISO)
	case BT_CONN_TYPE_ISO:
		index = conn - iso_conns;
		__ASSERT(0 <= index && index < ARRAY_SIZE(iso_conns),
			"Invalid bt_conn pointer");
		break;
#endif
#if defined(CONFIG_BT_BREDR)
	case BT_CONN_TYPE_SCO:
		index = conn - sco_conns;
		__ASSERT(0 <= index && index < ARRAY_SIZE(sco_conns),
			"Invalid bt_conn pointer");
		break;
#endif
	default:
		index = conn - acl_conns;
		__ASSERT(0 <= index && index < ARRAY_SIZE(acl_conns),
			 "Invalid bt_conn pointer");
		break;
	}

	return (uint8_t)index;
}

struct bt_conn *bt_conn_lookup_index(uint8_t index)
{
	if (index >= ARRAY_SIZE(acl_conns)) {
		return NULL;
	}

	return bt_conn_ref(&acl_conns[index]);
}

int bt_conn_init(void)
{
	int err, i;

	for (i = 0; i < ARRAY_SIZE(conn_tx); i++) {
		k_fifo_put(&free_tx, &conn_tx[i]);
	}

	bt_att_init();

	err = bt_smp_init();
	if (err) {
		return err;
	}

	bt_l2cap_init();

	/* Initialize background scan */
	if (IS_ENABLED(CONFIG_BT_CENTRAL)) {
		for (i = 0; i < ARRAY_SIZE(acl_conns); i++) {
			struct bt_conn *conn = bt_conn_ref(&acl_conns[i]);

			if (!conn) {
				continue;
			}

#if !defined(CONFIG_BT_WHITELIST)
			if (atomic_test_bit(conn->flags,
					    BT_CONN_AUTO_CONNECT)) {
				/* Only the default identity is supported */
				conn->id = BT_ID_DEFAULT;
				bt_conn_set_state(conn, BT_CONN_CONNECT_SCAN);
			}
#endif /* !defined(CONFIG_BT_WHITELIST) */

			bt_conn_unref(conn);
		}
	}

	return 0;
}
