/*
 * Copyright (c) 2018-2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <bluetooth/hci.h>
#include <sys/byteorder.h>

#include "util/util.h"
#include "util/memq.h"
#include "util/mayfly.h"

#include "hal/ticker.h"
#include "hal/ccm.h"
#include "hal/radio.h"

#include "ticker/ticker.h"

#include "pdu.h"
#include "ll.h"
#include "ll_feat.h"
#include "ll_settings.h"

#include "lll.h"
#include "lll_vendor.h"
#include "lll_clock.h"
#include "lll_adv.h"
#include "lll_scan.h"
#include "lll_conn.h"
#include "lll_master.h"
#include "lll_filter.h"
#include "lll_tim_internal.h"

#include "ull_adv_types.h"
#include "ull_scan_types.h"
#include "ull_conn_types.h"
#include "ull_filter.h"

#include "ull_internal.h"
#include "ull_scan_internal.h"
#include "ull_conn_internal.h"
#include "ull_master_internal.h"

#define BT_DBG_ENABLED IS_ENABLED(CONFIG_BT_DEBUG_HCI_DRIVER)
#define LOG_MODULE_NAME bt_ctlr_ull_master
#include "common/log.h"
#include <soc.h>
#include "hal/debug.h"

static void ticker_op_stop_scan_cb(u32_t status, void *params);
static void ticker_op_cb(u32_t status, void *params);
static inline void access_addr_get(u8_t access_addr[]);
static inline void conn_release(struct ll_scan_set *scan);

u8_t ll_create_connection(u16_t scan_interval, u16_t scan_window,
			  u8_t filter_policy, u8_t peer_addr_type,
			  u8_t *peer_addr, u8_t own_addr_type,
			  u16_t interval, u16_t latency, u16_t timeout)
{
	struct lll_conn *conn_lll;
	struct ll_scan_set *scan;
	u32_t conn_interval_us;
	struct lll_scan *lll;
	struct ll_conn *conn;
	memq_link_t *link;
	u8_t access_addr[4];
	u8_t hop;
	int err;

	scan = ull_scan_is_disabled_get(0);
	if (!scan) {
		return BT_HCI_ERR_CMD_DISALLOWED;
	}

	lll = &scan->lll;
	if (lll->conn) {
		return BT_HCI_ERR_CMD_DISALLOWED;
	}

	link = ll_rx_link_alloc();
	if (!link) {
		return BT_HCI_ERR_MEM_CAPACITY_EXCEEDED;
	}

	conn = ll_conn_acquire();
	if (!conn) {
		ll_rx_link_release(link);

		return BT_HCI_ERR_MEM_CAPACITY_EXCEEDED;
	}

	ull_scan_params_set(lll, 0, scan_interval, scan_window, filter_policy);

	lll->adv_addr_type = peer_addr_type;
	memcpy(lll->adv_addr, peer_addr, BDADDR_SIZE);
	lll->conn_timeout = timeout;
	lll->conn_ticks_slot = 0; /* TODO: */

	conn_lll = &conn->lll;

	access_addr_get(access_addr);
	memcpy(conn_lll->access_addr, &access_addr,
	       sizeof(conn_lll->access_addr));
	lll_trng_get(&conn_lll->crc_init[0], 3);

	conn_lll->handle = 0xFFFF;
	conn_lll->interval = interval;
	conn_lll->latency = latency;

	if (!conn_lll->link_tx_free) {
		conn_lll->link_tx_free = &conn_lll->link_tx;
	}

	memq_init(conn_lll->link_tx_free, &conn_lll->memq_tx.head,
		  &conn_lll->memq_tx.tail);
	conn_lll->link_tx_free = NULL;

	conn_lll->packet_tx_head_len = 0;
	conn_lll->packet_tx_head_offset = 0;

	conn_lll->sn = 0;
	conn_lll->nesn = 0;
	conn_lll->empty = 0;

#if defined(CONFIG_BT_CTLR_DATA_LENGTH)
	conn_lll->max_tx_octets = PDU_DC_PAYLOAD_SIZE_MIN;
	conn_lll->max_rx_octets = PDU_DC_PAYLOAD_SIZE_MIN;

#if defined(CONFIG_BT_CTLR_PHY)
	conn_lll->max_tx_time = PKT_US(PDU_DC_PAYLOAD_SIZE_MIN, PHY_1M);
	conn_lll->max_rx_time = PKT_US(PDU_DC_PAYLOAD_SIZE_MIN, PHY_1M);
#endif /* CONFIG_BT_CTLR_PHY */
#endif /* CONFIG_BT_CTLR_DATA_LENGTH */

#if defined(CONFIG_BT_CTLR_PHY)
	conn_lll->phy_tx = BIT(0);
	conn_lll->phy_flags = 0;
	conn_lll->phy_tx_time = BIT(0);
	conn_lll->phy_rx = BIT(0);
#endif /* CONFIG_BT_CTLR_PHY */

#if defined(CONFIG_BT_CTLR_CONN_RSSI)
	conn_lll->rssi_latest = 0x7F;
#if defined(CONFIG_BT_CTLR_CONN_RSSI_EVENT)
	conn_lll->rssi_reported = 0x7F;
	conn_lll->rssi_sample_count = 0;
#endif /* CONFIG_BT_CTLR_CONN_RSSI_EVENT */
#endif /* CONFIG_BT_CTLR_CONN_RSSI */

#if defined(CONFIG_BT_CTLR_TX_PWR_DYNAMIC_CONTROL)
	conn_lll->tx_pwr_lvl = RADIO_TXP_DEFAULT;
#endif /* CONFIG_BT_CTLR_TX_PWR_DYNAMIC_CONTROL */

	/* FIXME: BEGIN: Move to ULL? */
	conn_lll->latency_prepare = 0;
	conn_lll->latency_event = 0;
	conn_lll->event_counter = 0;

	conn_lll->data_chan_count =
		ull_conn_chan_map_cpy(conn_lll->data_chan_map);
	lll_trng_get(&hop, sizeof(u8_t));
	conn_lll->data_chan_hop = 5 + (hop % 12);
	conn_lll->data_chan_sel = 0;
	conn_lll->data_chan_use = 0;
	conn_lll->role = 0;
	/* FIXME: END: Move to ULL? */
#if defined(CONFIG_BT_CTLR_CONN_META)
	memset(&conn_lll->conn_meta, 0, sizeof(conn_lll->conn_meta));
#endif /* CONFIG_BT_CTLR_CONN_META */

	conn->connect_expire = 6U;
	conn->supervision_expire = 0U;
	conn_interval_us = (u32_t)interval * 1250U;
	conn->supervision_reload = RADIO_CONN_EVENTS(timeout * 10000U,
							 conn_interval_us);

	conn->procedure_expire = 0U;
	conn->procedure_reload = RADIO_CONN_EVENTS(40000000,
						       conn_interval_us);

#if defined(CONFIG_BT_CTLR_LE_PING)
	conn->apto_expire = 0U;
	/* APTO in no. of connection events */
	conn->apto_reload = RADIO_CONN_EVENTS((30000000), conn_interval_us);
	conn->appto_expire = 0U;
	/* Dispatch LE Ping PDU 6 connection events (that peer would listen to)
	 * before 30s timeout
	 * TODO: "peer listens to" is greater than 30s due to latency
	 */
	conn->appto_reload = (conn->apto_reload > (conn_lll->latency + 6)) ?
			     (conn->apto_reload - (conn_lll->latency + 6)) :
			     conn->apto_reload;
#endif /* CONFIG_BT_CTLR_LE_PING */

	conn->common.fex_valid = 0U;
	conn->master.terminate_ack = 0U;

	conn->llcp_req = conn->llcp_ack = conn->llcp_type = 0U;
	conn->llcp_rx = NULL;
	conn->llcp_cu.req = conn->llcp_cu.ack = 0;
	conn->llcp_feature.req = conn->llcp_feature.ack = 0;
	conn->llcp_feature.features = LL_FEAT;
	conn->llcp_version.req = conn->llcp_version.ack = 0;
	conn->llcp_version.tx = conn->llcp_version.rx = 0U;
	conn->llcp_terminate.reason_peer = 0U;
	/* NOTE: use allocated link for generating dedicated
	 * terminate ind rx node
	 */
	conn->llcp_terminate.node_rx.hdr.link = link;

#if defined(CONFIG_BT_CTLR_LE_ENC)
	conn_lll->enc_rx = conn_lll->enc_tx = 0U;
	conn->llcp_enc.req = conn->llcp_enc.ack = 0U;
	conn->llcp_enc.pause_tx = conn->llcp_enc.pause_rx = 0U;
	conn->llcp_enc.refresh = 0U;
#endif /* CONFIG_BT_CTLR_LE_ENC */

#if defined(CONFIG_BT_CTLR_CONN_PARAM_REQ)
	conn->llcp_conn_param.req = 0U;
	conn->llcp_conn_param.ack = 0U;
	conn->llcp_conn_param.disabled = 0U;
#endif /* CONFIG_BT_CTLR_CONN_PARAM_REQ */

#if defined(CONFIG_BT_CTLR_DATA_LENGTH)
	conn->llcp_length.req = conn->llcp_length.ack = 0U;
	conn->llcp_length.disabled = 0U;
	conn->llcp_length.cache.tx_octets = 0U;
	conn->default_tx_octets = ull_conn_default_tx_octets_get();

#if defined(CONFIG_BT_CTLR_PHY)
	conn->default_tx_time = ull_conn_default_tx_time_get();
#endif /* CONFIG_BT_CTLR_PHY */
#endif /* CONFIG_BT_CTLR_DATA_LENGTH */

#if defined(CONFIG_BT_CTLR_PHY)
	conn->llcp_phy.req = conn->llcp_phy.ack = 0U;
	conn->llcp_phy.disabled = 0U;
	conn->llcp_phy.pause_tx = 0U;
	conn->phy_pref_tx = ull_conn_default_phy_tx_get();
	conn->phy_pref_rx = ull_conn_default_phy_rx_get();
	conn->phy_pref_flags = 0U;
#endif /* CONFIG_BT_CTLR_PHY */

	conn->tx_head = conn->tx_ctrl = conn->tx_ctrl_last =
	conn->tx_data = conn->tx_data_last = 0;

	lll->conn = conn_lll;

	ull_hdr_init(&conn->ull);
	lll_hdr_init(&conn->lll, conn);

#if defined(CONFIG_BT_CTLR_PRIVACY)
	ull_filter_scan_update(filter_policy);

	lll->rl_idx = FILTER_IDX_NONE;
	lll->rpa_gen = 0;
	if (!filter_policy && ull_filter_lll_rl_enabled()) {
		/* Look up the resolving list */
		lll->rl_idx = ull_filter_rl_find(peer_addr_type, peer_addr,
						 NULL);
	}

	if (own_addr_type == BT_ADDR_LE_PUBLIC_ID ||
	    own_addr_type == BT_ADDR_LE_RANDOM_ID) {

		/* Generate RPAs if required */
		ull_filter_rpa_update(false);
		own_addr_type &= 0x1;
		lll->rpa_gen = 1;
	}
#endif

	scan->own_addr_type = own_addr_type;

	/* wait for stable clocks */
	err = lll_clock_wait();
	if (err) {
		conn_release(scan);

		return BT_HCI_ERR_HW_FAILURE;
	}

	return ull_scan_enable(scan);
}

u8_t ll_connect_disable(void **rx)
{
	struct lll_conn *conn_lll;
	struct ll_scan_set *scan;
	u8_t status;

	scan = ull_scan_is_enabled_get(0);
	if (!scan) {
		return BT_HCI_ERR_CMD_DISALLOWED;
	}

	conn_lll = scan->lll.conn;
	if (!conn_lll) {
		return BT_HCI_ERR_CMD_DISALLOWED;
	}

	status = ull_scan_disable(0, scan);
	if (!status) {
		struct ll_conn *conn = (void *)HDR_LLL2EVT(conn_lll);
		struct node_rx_ftr *ftr;
		struct node_rx_pdu *cc;
		memq_link_t *link;

		cc = (void *)&conn->llcp_terminate.node_rx;
		link = cc->hdr.link;
		LL_ASSERT(link);

		/* free the memq link early, as caller could overwrite it */
		ll_rx_link_release(link);

		cc->hdr.type = NODE_RX_TYPE_CONNECTION;
		cc->hdr.handle = 0xffff;
		*((u8_t *)cc->pdu) = BT_HCI_ERR_UNKNOWN_CONN_ID;

		ftr = &(cc->hdr.rx_ftr);
		ftr->param = &scan->lll;

		*rx = cc;
	}

	return status;
}

u8_t ll_chm_update(u8_t *chm)
{
	u16_t handle;
	u8_t ret;

	ull_conn_chan_map_set(chm);

	handle = CONFIG_BT_MAX_CONN;
	while (handle--) {
		struct ll_conn *conn;

		conn = ll_connected_get(handle);
		if (!conn || conn->lll.role) {
			continue;
		}

		ret = ull_conn_llcp_req(conn);
		if (ret) {
			return ret;
		}

		memcpy(conn->llcp.chan_map.chm, chm,
		       sizeof(conn->llcp.chan_map.chm));
		/* conn->llcp.chan_map.instant     = 0; */
		conn->llcp.chan_map.initiate = 1U;

		conn->llcp_type = LLCP_CHAN_MAP;
		conn->llcp_req++;
	}

	return 0;
}

#if defined(CONFIG_BT_CTLR_LE_ENC)
u8_t ll_enc_req_send(u16_t handle, u8_t *rand, u8_t *ediv, u8_t *ltk)
{
	struct ll_conn *conn;
	struct node_tx *tx;

	conn = ll_connected_get(handle);
	if (!conn) {
		return BT_HCI_ERR_UNKNOWN_CONN_ID;
	}

	if ((conn->llcp_enc.req != conn->llcp_enc.ack) ||
	    ((conn->llcp_req != conn->llcp_ack) &&
	     (conn->llcp_type == LLCP_ENCRYPTION))) {
		return BT_HCI_ERR_CMD_DISALLOWED;
	}

	tx = ll_tx_mem_acquire();
	if (tx) {
		struct pdu_data *pdu_data_tx;

		pdu_data_tx = (void *)tx->pdu;

		memcpy(&conn->llcp_enc.ltk[0], ltk, sizeof(conn->llcp_enc.ltk));

		if (!conn->lll.enc_rx && !conn->lll.enc_tx) {
			struct pdu_data_llctrl_enc_req *enc_req;

			pdu_data_tx->ll_id = PDU_DATA_LLID_CTRL;
			pdu_data_tx->len =
				offsetof(struct pdu_data_llctrl, enc_rsp) +
				sizeof(struct pdu_data_llctrl_enc_req);
			pdu_data_tx->llctrl.opcode =
				PDU_DATA_LLCTRL_TYPE_ENC_REQ;
			enc_req = (void *)
				&pdu_data_tx->llctrl.enc_req;
			memcpy(enc_req->rand, rand, sizeof(enc_req->rand));
			enc_req->ediv[0] = ediv[0];
			enc_req->ediv[1] = ediv[1];
			lll_trng_get(enc_req->skdm, sizeof(enc_req->skdm));
			lll_trng_get(enc_req->ivm, sizeof(enc_req->ivm));
		} else if (conn->lll.enc_rx && conn->lll.enc_tx) {
			memcpy(&conn->llcp_enc.rand[0], rand,
			       sizeof(conn->llcp_enc.rand));

			conn->llcp_enc.ediv[0] = ediv[0];
			conn->llcp_enc.ediv[1] = ediv[1];

			pdu_data_tx->ll_id = PDU_DATA_LLID_CTRL;
			pdu_data_tx->len = offsetof(struct pdu_data_llctrl,
						    enc_req);
			pdu_data_tx->llctrl.opcode =
				PDU_DATA_LLCTRL_TYPE_PAUSE_ENC_REQ;
		} else {
			ll_tx_mem_release(tx);

			return BT_HCI_ERR_CMD_DISALLOWED;
		}

		if (ll_tx_mem_enqueue(handle, tx)) {
			ll_tx_mem_release(tx);

			return BT_HCI_ERR_CMD_DISALLOWED;
		}

		conn->llcp_enc.req++;

		return 0;
	}

	return BT_HCI_ERR_CMD_DISALLOWED;
}
#endif /* CONFIG_BT_CTLR_LE_ENC */

void ull_master_setup(memq_link_t *link, struct node_rx_hdr *rx,
		      struct node_rx_ftr *ftr, struct lll_conn *lll)
{
	u32_t conn_offset_us, conn_interval_us;
	u8_t ticker_id_scan, ticker_id_conn;
	u8_t peer_addr[BDADDR_SIZE];
	u32_t ticks_slot_overhead;
	u32_t ticks_slot_offset;
	struct ll_scan_set *scan;
	struct node_rx_cc *cc;
	struct ll_conn *conn;
	struct pdu_adv *pdu_tx;
	u8_t peer_addr_type;
	u32_t ticker_status;
	u8_t chan_sel;

	((struct lll_scan *)ftr->param)->conn = NULL;

	scan = ((struct lll_scan *)ftr->param)->hdr.parent;
	conn = lll->hdr.parent;

	pdu_tx = (void *)((struct node_rx_pdu *)rx)->pdu;

	peer_addr_type = pdu_tx->rx_addr;
	memcpy(peer_addr, &pdu_tx->connect_ind.adv_addr[0], BDADDR_SIZE);

	/* This is the chan sel bit from the received adv pdu */
	chan_sel = pdu_tx->chan_sel;

	cc = (void *)pdu_tx;
	cc->status = 0U;
	cc->role = 0U;

#if defined(CONFIG_BT_CTLR_PRIVACY)
	u8_t rl_idx = ftr->rl_idx;

	if (ftr->lrpa_used) {
		memcpy(&cc->local_rpa[0], &pdu_tx->connect_ind.init_addr[0],
		       BDADDR_SIZE);
	} else {
		memset(&cc->local_rpa[0], 0x0, BDADDR_SIZE);
	}

	if (rl_idx != FILTER_IDX_NONE) {
		/* Store identity address */
		ll_rl_id_addr_get(rl_idx, &cc->peer_addr_type,
				  &cc->peer_addr[0]);
		/* Mark it as identity address from RPA (0x02, 0x03) */
		cc->peer_addr_type += 2;

		/* Store peer RPA */
		memcpy(&cc->peer_rpa[0], &peer_addr[0], BDADDR_SIZE);
	} else {
		memset(&cc->peer_rpa[0], 0x0, BDADDR_SIZE);
#else
	if (1) {
#endif /* CONFIG_BT_CTLR_PRIVACY */
		cc->peer_addr_type = peer_addr_type;
		memcpy(cc->peer_addr, &peer_addr[0], BDADDR_SIZE);
	}

	cc->interval = lll->interval;
	cc->latency = lll->latency;
	cc->timeout = scan->lll.conn_timeout;
	cc->sca = lll_conn_sca_local_get();

	lll->handle = ll_conn_handle_get(conn);
	rx->handle = lll->handle;

#if defined(CONFIG_BT_CTLR_TX_PWR_DYNAMIC_CONTROL)
	lll->tx_pwr_lvl = RADIO_TXP_DEFAULT;
#endif /* CONFIG_BT_CTLR_TX_PWR_DYNAMIC_CONTROL */

	/* Use Channel Selection Algorithm #2 if peer too supports it */
	if (IS_ENABLED(CONFIG_BT_CTLR_CHAN_SEL_2)) {
		struct node_rx_pdu *rx_csa;
		struct node_rx_cs *cs;

		/* pick the rx node instance stored within the connection
		 * rx node.
		 */
		rx_csa = (void *)ftr->extra;

		/* Enqueue the connection event */
		ll_rx_put(link, rx);

		/* use the rx node for CSA event */
		rx = (void *)rx_csa;
		link = rx->link;

		rx->handle = lll->handle;
		rx->type = NODE_RX_TYPE_CHAN_SEL_ALGO;

		cs = (void *)rx_csa->pdu;

		if (chan_sel) {
			u16_t aa_ls = ((u16_t)lll->access_addr[1] << 8) |
				      lll->access_addr[0];
			u16_t aa_ms = ((u16_t)lll->access_addr[3] << 8) |
				      lll->access_addr[2];

			lll->data_chan_sel = 1;
			lll->data_chan_id = aa_ms ^ aa_ls;

			cs->csa = 0x01;
		} else {
			cs->csa = 0x00;
		}
	}

	ll_rx_put(link, rx);
	ll_rx_sched();

	/* TODO: active_to_start feature port */
	conn->evt.ticks_active_to_start = 0U;
	conn->evt.ticks_xtal_to_start =
		HAL_TICKER_US_TO_TICKS(EVENT_OVERHEAD_XTAL_US);
	conn->evt.ticks_preempt_to_start =
		HAL_TICKER_US_TO_TICKS(EVENT_OVERHEAD_PREEMPT_MIN_US);
	conn->evt.ticks_slot =
		HAL_TICKER_US_TO_TICKS(EVENT_OVERHEAD_START_US +
				       ftr->us_radio_rdy + 328 + EVENT_IFS_US +
				       328);

	ticks_slot_offset = MAX(conn->evt.ticks_active_to_start,
				conn->evt.ticks_xtal_to_start);

	if (IS_ENABLED(CONFIG_BT_CTLR_LOW_LAT)) {
		ticks_slot_overhead = ticks_slot_offset;
	} else {
		ticks_slot_overhead = 0U;
	}

	conn_interval_us = lll->interval * 1250;
	conn_offset_us = ftr->us_radio_end;
	conn_offset_us += HAL_TICKER_TICKS_TO_US(1);
	conn_offset_us -= EVENT_OVERHEAD_START_US;
	conn_offset_us -= ftr->us_radio_rdy;

#if (CONFIG_BT_CTLR_ULL_HIGH_PRIO == CONFIG_BT_CTLR_ULL_LOW_PRIO)
	/* disable ticker job, in order to chain stop and start to avoid RTC
	 * being stopped if no tickers active.
	 */
	mayfly_enable(TICKER_USER_ID_ULL_HIGH, TICKER_USER_ID_ULL_LOW, 0);
#endif

	/* Stop Scanner */
	ticker_id_scan = TICKER_ID_SCAN_BASE + ull_scan_handle_get(scan);
	ticker_status = ticker_stop(TICKER_INSTANCE_ID_CTLR,
				    TICKER_USER_ID_ULL_HIGH,
				    ticker_id_scan, ticker_op_stop_scan_cb,
				    (void *)(u32_t)ticker_id_scan);
	ticker_op_stop_scan_cb(ticker_status, (void *)(u32_t)ticker_id_scan);

	/* Scanner stop can expire while here in this ISR.
	 * Deferred attempt to stop can fail as it would have
	 * expired, hence ignore failure.
	 */
	ticker_stop(TICKER_INSTANCE_ID_CTLR, TICKER_USER_ID_ULL_HIGH,
		    TICKER_ID_SCAN_STOP, NULL, NULL);

	/* Start master */
	ticker_id_conn = TICKER_ID_CONN_BASE + ll_conn_handle_get(conn);
	ticker_status = ticker_start(TICKER_INSTANCE_ID_CTLR,
				     TICKER_USER_ID_ULL_HIGH,
				     ticker_id_conn,
				     ftr->ticks_anchor - ticks_slot_offset,
				     HAL_TICKER_US_TO_TICKS(conn_offset_us),
				     HAL_TICKER_US_TO_TICKS(conn_interval_us),
				     HAL_TICKER_REMAINDER(conn_interval_us),
				     TICKER_NULL_LAZY,
				     (conn->evt.ticks_slot +
				      ticks_slot_overhead),
				     ull_master_ticker_cb, conn, ticker_op_cb,
				     (void *)__LINE__);
	LL_ASSERT((ticker_status == TICKER_STATUS_SUCCESS) ||
		  (ticker_status == TICKER_STATUS_BUSY));

#if (CONFIG_BT_CTLR_ULL_HIGH_PRIO == CONFIG_BT_CTLR_ULL_LOW_PRIO)
	/* enable ticker job, irrespective of disabled in this function so
	 * first connection event can be scheduled as soon as possible.
	 */
	mayfly_enable(TICKER_USER_ID_ULL_HIGH, TICKER_USER_ID_ULL_LOW, 1);
#endif
}

void ull_master_ticker_cb(u32_t ticks_at_expire, u32_t remainder, u16_t lazy,
			  void *param)
{
	static memq_link_t link;
	static struct mayfly mfy = {0, 0, &link, NULL, lll_master_prepare};
	static struct lll_prepare_param p;
	struct ll_conn *conn = param;
	u32_t err;
	u8_t ref;

	DEBUG_RADIO_PREPARE_M(1);

	/* If this is a must-expire callback, LLCP state machine does not need
	 * to know. Will be called with lazy > 0 when scheduled in air.
	 */
	if (!IS_ENABLED(CONFIG_BT_CTLR_CONN_META) ||
	    (lazy != TICKER_LAZY_MUST_EXPIRE)) {
		int ret;

		/* Handle any LL Control Procedures */
		ret = ull_conn_llcp(conn, ticks_at_expire, lazy);
		if (ret) {
			return;
		}
	}

	/* Increment prepare reference count */
	ref = ull_ref_inc(&conn->ull);
	LL_ASSERT(ref);

	/* De-mux 1 tx node from FIFO */
	ull_conn_tx_demux(1);

	/* Enqueue towards LLL */
	ull_conn_tx_lll_enqueue(conn, 1);

	/* Append timing parameters */
	p.ticks_at_expire = ticks_at_expire;
	p.remainder = remainder;
	p.lazy = lazy;
	p.param = &conn->lll;
	mfy.param = &p;

	/* Kick LLL prepare */
	err = mayfly_enqueue(TICKER_USER_ID_ULL_HIGH, TICKER_USER_ID_LLL,
			     0, &mfy);
	LL_ASSERT(!err);

	/* De-mux remaining tx nodes from FIFO */
	ull_conn_tx_demux(UINT8_MAX);

	/* Enqueue towards LLL */
	ull_conn_tx_lll_enqueue(conn, UINT8_MAX);

	DEBUG_RADIO_PREPARE_M(1);
}

static void ticker_op_stop_scan_cb(u32_t status, void *params)
{
	/* TODO: */
}

static void ticker_op_cb(u32_t status, void *params)
{
	ARG_UNUSED(params);

	LL_ASSERT(status == TICKER_STATUS_SUCCESS);
}

/** @brief Prepare access address as per BT Spec.
 *
 * - It shall have no more than six consecutive zeros or ones.
 * - It shall not be the advertising channel packets' Access Address.
 * - It shall not be a sequence that differs from the advertising channel
 *   packets Access Address by only one bit.
 * - It shall not have all four octets equal.
 * - It shall have no more than 24 transitions.
 * - It shall have a minimum of two transitions in the most significant six
 *   bits.
 *
 * LE Coded PHY requirements:
 * - It shall have at least three ones in the least significant 8 bits.
 * - It shall have no more than eleven transitions in the least significant 16
 *   bits.
 */
static inline void access_addr_get(u8_t access_addr[])
{
#if defined(CONFIG_BT_CTLR_PHY_CODED)
	u8_t transitions_lsb16;
	u8_t ones_count_lsb8;
#endif /* CONFIG_BT_CTLR_PHY_CODED */
	u8_t consecutive_cnt;
	u8_t consecutive_bit;
	u32_t adv_aa_check;
	u32_t aa;
	u8_t transitions;
	u8_t bit_idx;
	u8_t retry;

	retry = 3U;
again:
	LL_ASSERT(retry);
	retry--;

	lll_trng_get(access_addr, 4);
	aa = sys_get_le32(access_addr);

	bit_idx = 31U;
	transitions = 0U;
	consecutive_cnt = 1U;
#if defined(CONFIG_BT_CTLR_PHY_CODED)
	ones_count_lsb8 = 0U;
	transitions_lsb16 = 0U;
#endif /* CONFIG_BT_CTLR_PHY_CODED */
	consecutive_bit = (aa >> bit_idx) & 0x01;
	while (bit_idx--) {
#if defined(CONFIG_BT_CTLR_PHY_CODED)
		u8_t transitions_lsb16_prev = transitions_lsb16;
#endif /* CONFIG_BT_CTLR_PHY_CODED */
		u8_t consecutive_cnt_prev = consecutive_cnt;
		u8_t transitions_prev = transitions;
		u8_t bit;

		bit = (aa >> bit_idx) & 0x01;
		if (bit == consecutive_bit) {
			consecutive_cnt++;
		} else {
			consecutive_cnt = 1U;
			consecutive_bit = bit;
			transitions++;

#if defined(CONFIG_BT_CTLR_PHY_CODED)
			if (bit_idx < 15) {
				transitions_lsb16++;
			}
#endif /* CONFIG_BT_CTLR_PHY_CODED */
		}

#if defined(CONFIG_BT_CTLR_PHY_CODED)
		if ((bit_idx < 8) && consecutive_bit) {
			ones_count_lsb8++;
		}
#endif /* CONFIG_BT_CTLR_PHY_CODED */

		/* It shall have no more than six consecutive zeros or ones. */
		/* It shall have a minimum of two transitions in the most
		 * significant six bits.
		 */
		if ((consecutive_cnt > 6) ||
#if defined(CONFIG_BT_CTLR_PHY_CODED)
		    (!consecutive_bit && (((bit_idx < 6) &&
					   (ones_count_lsb8 < 1)) ||
					  ((bit_idx < 5) &&
					   (ones_count_lsb8 < 2)) ||
					  ((bit_idx < 4) &&
					   (ones_count_lsb8 < 3)))) ||
#endif /* CONFIG_BT_CTLR_PHY_CODED */
		    ((consecutive_cnt < 6) &&
		     (((bit_idx < 29) && (transitions < 1)) ||
		      ((bit_idx < 28) && (transitions < 2))))) {
			if (consecutive_bit) {
				consecutive_bit = 0U;
				aa &= ~BIT(bit_idx);
#if defined(CONFIG_BT_CTLR_PHY_CODED)
				if (bit_idx < 8) {
					ones_count_lsb8--;
				}
#endif /* CONFIG_BT_CTLR_PHY_CODED */
			} else {
				consecutive_bit = 1U;
				aa |= BIT(bit_idx);
#if defined(CONFIG_BT_CTLR_PHY_CODED)
				if (bit_idx < 8) {
					ones_count_lsb8++;
				}
#endif /* CONFIG_BT_CTLR_PHY_CODED */
			}

			if (transitions != transitions_prev) {
				consecutive_cnt = consecutive_cnt_prev;
				transitions = transitions_prev;
			} else {
				consecutive_cnt = 1U;
				transitions++;
			}

#if defined(CONFIG_BT_CTLR_PHY_CODED)
			if (bit_idx < 15) {
				if (transitions_lsb16 !=
				    transitions_lsb16_prev) {
					transitions_lsb16 =
						transitions_lsb16_prev;
				} else {
					transitions_lsb16++;
				}
			}
#endif /* CONFIG_BT_CTLR_PHY_CODED */
		}

		/* It shall have no more than 24 transitions
		 * It shall have no more than eleven transitions in the least
		 * significant 16 bits.
		 */
		if ((transitions > 24) ||
#if defined(CONFIG_BT_CTLR_PHY_CODED)
		    (transitions_lsb16 > 11) ||
#endif /* CONFIG_BT_CTLR_PHY_CODED */
		    0) {
			if (consecutive_bit) {
				aa &= ~(BIT(bit_idx + 1) - 1);
			} else {
				aa |= (BIT(bit_idx + 1) - 1);
			}

			break;
		}
	}

	/* It shall not be the advertising channel packets Access Address.
	 * It shall not be a sequence that differs from the advertising channel
	 * packets Access Address by only one bit.
	 */
	adv_aa_check = aa ^ PDU_AC_ACCESS_ADDR;
	if (util_ones_count_get((u8_t *)&adv_aa_check,
				sizeof(adv_aa_check)) <= 1) {
		goto again;
	}

	/* It shall not have all four octets equal. */
	if (!((aa & 0xFFFF) ^ (aa >> 16)) &&
	    !((aa & 0xFF) ^ (aa >> 24))) {
		goto again;
	}

	sys_put_le32(aa, access_addr);
}

static inline void conn_release(struct ll_scan_set *scan)
{
	struct lll_conn *lll = scan->lll.conn;
	struct node_rx_pdu *cc;
	struct ll_conn *conn;
	memq_link_t *link;

	LL_ASSERT(!lll->link_tx_free);
	link = memq_deinit(&lll->memq_tx.head, &lll->memq_tx.tail);
	LL_ASSERT(link);
	lll->link_tx_free = link;

	conn = (void *)HDR_LLL2EVT(lll);

	cc = (void *)&conn->llcp_terminate.node_rx;
	link = cc->hdr.link;
	LL_ASSERT(link);

	ll_rx_link_release(link);

	ll_conn_release(conn);
	scan->lll.conn = NULL;
}
