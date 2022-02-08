/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/types.h>
#include <ztest.h>
#include "kconfig.h"

#include <bluetooth/hci.h>
#include <sys/byteorder.h>
#include <sys/slist.h>
#include <sys/util.h>
#include "hal/ccm.h"

#include "util/util.h"
#include "util/mem.h"
#include "util/memq.h"
#include "util/dbuf.h"

#include "pdu.h"
#include "ll.h"
#include "ll_settings.h"

#include "lll.h"
#include "lll_df_types.h"
#include "lll_conn.h"

#include "ull_tx_queue.h"
#include "ull_conn_types.h"
#include "ull_llcp.h"
#include "ull_conn_internal.h"
#include "ull_llcp_internal.h"

#include "helper_pdu.h"
#include "helper_util.h"

struct ll_conn conn;

static void setup(void)
{
	test_setup(&conn);
}

/* Tests of successful execution of CTE Request Procedure */

/* +-----+                     +-------+            +-----+
 * | UT  |                     | LL_A  |            | LT  |
 * +-----+                     +-------+            +-----+
 *    |                            |                   |
 *    | Start initiation           |                   |
 *    | CTE Reqest Proc.           |                   |
 *    |--------------------------->|                   |
 *    |                            |                   |
 *    |                            | LL_LE_CTE_REQ     |
 *    |                            |------------------>|
 *    |                            |                   |
 *    |                            |    LL_LE_CTE_RSP  |
 *    |                            |<------------------|
 *    |                            |                   |
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *    |                            |                   |
 *    | LE Connection IQ Report    |                   |
 *    |<---------------------------|                   |
 *    |                            |                   |
 *    |                            |                   |
 */
void test_cte_req_central_local(void)
{
	uint8_t err;
	struct node_tx *tx;

	struct pdu_data_llctrl_cte_req local_cte_req = {
		.cte_type_req = BT_HCI_LE_AOA_CTE,
		.min_cte_len_req = BT_HCI_LE_CTE_LEN_MIN,
	};
	struct pdu_data_llctrl_cte_rsp remote_cte_rsp = {};
	struct node_rx_pdu *ntf;

	/* Role */
	test_set_role(&conn, BT_HCI_ROLE_PERIPHERAL);

	/* Connect */
	ull_cp_state_set(&conn, ULL_CP_CONNECTED);

	/* Initiate an CTE Request Procedure */
	err = ull_cp_cte_req(&conn, local_cte_req.min_cte_len_req, local_cte_req.cte_type_req);
	zassert_equal(err, BT_HCI_ERR_SUCCESS, NULL);

	/* Prepare */
	event_prepare(&conn);

	/* Tx Queue should have one LL Control PDU */
	lt_rx(LL_CTE_REQ, &conn, &tx, &local_cte_req);
	lt_rx_q_is_empty(&conn);

	/* Rx */
	lt_tx(LL_CTE_RSP, &conn, &remote_cte_rsp);

	/* Done */
	event_done(&conn);

	/* Receive notification of sampled CTE response */
	ut_rx_pdu(LL_CTE_RSP, &ntf, &remote_cte_rsp);

	/* There should not be a host notifications */
	ut_rx_q_is_empty();

	/* Release tx node */
	ull_cp_release_tx(&conn, tx);

	zassert_equal(ctx_buffers_free(), CONFIG_BT_CTLR_LLCP_PROC_CTX_BUF_NUM,
				  "Free CTX buffers %d", ctx_buffers_free());
}

/* +-----+                     +-------+            +-----+
 * | UT  |                     | LL_A  |            | LT  |
 * +-----+                     +-------+            +-----+
 *    |                            |                   |
 *    | Start initiator            |                   |
 *    | CTE Reqest Proc.           |                   |
 *    |--------------------------->|                   |
 *    |                            |                   |
 *    |                            | LL_LE_CTE_REQ     |
 *    |                            |------------------>|
 *    |                            |                   |
 *    |                            |    LL_LE_CTE_RSP  |
 *    |                            |<------------------|
 *    |                            |                   |
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *    |                            |                   |
 *    | LE Connection IQ Report    |                   |
 *    |<---------------------------|                   |
 *    |                            |                   |
 *    |                            |                   |
 */
void test_cte_req_peripheral_local(void)
{
	uint8_t err;
	struct node_tx *tx;

	struct pdu_data_llctrl_cte_req local_cte_req = {
		.cte_type_req = BT_HCI_LE_AOA_CTE,
		.min_cte_len_req = BT_HCI_LE_CTE_LEN_MIN,
	};

	struct pdu_data_llctrl_cte_rsp remote_cte_rsp = {};
	struct node_rx_pdu *ntf;

	/* Role */
	test_set_role(&conn, BT_HCI_ROLE_PERIPHERAL);

	/* Connect */
	ull_cp_state_set(&conn, ULL_CP_CONNECTED);

	/* Initiate an CTE Request Procedure */
	err = ull_cp_cte_req(&conn, local_cte_req.min_cte_len_req, local_cte_req.cte_type_req);
	zassert_equal(err, BT_HCI_ERR_SUCCESS, NULL);

	/* Prepare */
	event_prepare(&conn);

	/* Tx Queue should have one LL Control PDU */
	lt_rx(LL_CTE_REQ, &conn, &tx, &local_cte_req);
	lt_rx_q_is_empty(&conn);

	/* Rx */
	lt_tx(LL_CTE_RSP, &conn, &remote_cte_rsp);

	/* Done */
	event_done(&conn);

	/* Receive notification of sampled CTE response */
	ut_rx_pdu(LL_CTE_RSP, &ntf, &remote_cte_rsp);

	/* Release tx node */
	ull_cp_release_tx(&conn, tx);

	/* There should not be a host notifications */
	ut_rx_q_is_empty();

	zassert_equal(ctx_buffers_free(), CONFIG_BT_CTLR_LLCP_PROC_CTX_BUF_NUM,
				  "Free CTX buffers %d", ctx_buffers_free());
}

/* +-----+                     +-------+            +-----+
 * | UT  |                     | LL_A  |            | LT  |
 * +-----+                     +-------+            +-----+
 *    |                            |                   |
 *    | Start responder            |                   |
 *    | CTE Reqest Proc.           |                   |
 *    |--------------------------->|                   |
 *    |                            |                   |
 *    |                            | LL_LE_CTE_REQ     |
 *    |                            |<------------------|
 *    |                            |                   |
 *    |                            |    LL_LE_CTE_RSP  |
 *    |                            |------------------>|
 *    |                            |                   |
 *    |                            |                   |
 */
void test_cte_req_central_remote(void)
{
	struct node_tx *tx;

	struct pdu_data_llctrl_cte_req local_cte_req = {
		.cte_type_req = BT_HCI_LE_AOA_CTE,
		.min_cte_len_req = BT_HCI_LE_CTE_LEN_MIN,
	};

	struct pdu_data_llctrl_cte_rsp remote_cte_rsp = {};

	/* Role */
	test_set_role(&conn, BT_HCI_ROLE_CENTRAL);

	/* Connect */
	ull_cp_state_set(&conn, ULL_CP_CONNECTED);

	/* Enable response for CTE request */
	ull_cp_cte_rsp_enable(&conn, true, BT_HCI_LE_CTE_LEN_MAX,
			      (BT_HCI_LE_AOA_CTE | BT_HCI_LE_AOD_CTE_1US | BT_HCI_LE_AOD_CTE_2US));

	/* Prepare */
	event_prepare(&conn);

	/* Tx */
	lt_tx(LL_CTE_REQ, &conn, &local_cte_req);

	/* Done */
	event_done(&conn);

	/* Prepare */
	event_prepare(&conn);

	/* Tx Queue should have one LL Control PDU */
	lt_rx(LL_CTE_RSP, &conn, &tx, &remote_cte_rsp);
	lt_rx_q_is_empty(&conn);

	/* TX Ack */
	event_tx_ack(&conn, tx);

	/* Done */
	event_done(&conn);

	/* Release tx node */
	ull_cp_release_tx(&conn, tx);

	/* There should not be a host notifications */
	ut_rx_q_is_empty();

	zassert_equal(ctx_buffers_free(), CONFIG_BT_CTLR_LLCP_PROC_CTX_BUF_NUM,
				  "Free CTX buffers %d", ctx_buffers_free());
}

/* +-----+                     +-------+            +-----+
 * | UT  |                     | LL_A  |            | LT  |
 * +-----+                     +-------+            +-----+
 *    |                            |                   |
 *    | Start responder            |                   |
 *    | CTE Reqest Proc   .        |                   |
 *    |--------------------------->|                   |
 *    |                            |                   |
 *    |                            | LL_LE_CTE_REQ     |
 *    |                            |<------------------|
 *    |                            |                   |
 *    |                            |    LL_LE_CTE_RSP  |
 *    |                            |------------------>|
 *    |                            |                   |
 *    |                            |                   |
 */
void test_cte_req_peripheral_remote(void)
{
	struct node_tx *tx;

	struct pdu_data_llctrl_cte_req local_cte_req = {
		.cte_type_req = BT_HCI_LE_AOA_CTE,
		.min_cte_len_req = BT_HCI_LE_CTE_LEN_MIN,
	};

	struct pdu_data_llctrl_cte_rsp remote_cte_rsp = {};

	/* Role */
	test_set_role(&conn, BT_HCI_ROLE_PERIPHERAL);

	/* Connect */
	ull_cp_state_set(&conn, ULL_CP_CONNECTED);

	/* Enable response for CTE request */
	ull_cp_cte_rsp_enable(&conn, true, BT_HCI_LE_CTE_LEN_MAX,
			      (BT_HCI_LE_AOA_CTE | BT_HCI_LE_AOD_CTE_1US | BT_HCI_LE_AOD_CTE_2US));

	/* Prepare */
	event_prepare(&conn);

	/* Tx */
	lt_tx(LL_CTE_REQ, &conn, &local_cte_req);

	/* Done */
	event_done(&conn);

	/* Prepare */
	event_prepare(&conn);

	/* Tx Queue should have one LL Control PDU */
	lt_rx(LL_CTE_RSP, &conn, &tx, &remote_cte_rsp);
	lt_rx_q_is_empty(&conn);

	/* TX Ack */
	event_tx_ack(&conn, tx);

	/* Done */
	event_done(&conn);

	/* Release tx node */
	ull_cp_release_tx(&conn, tx);

	/* There should not be a host notifications */
	ut_rx_q_is_empty();

	zassert_equal(ctx_buffers_free(), CONFIG_BT_CTLR_LLCP_PROC_CTX_BUF_NUM,
				  "Free CTX buffers %d", ctx_buffers_free());
}

/* Tests of expected failures during execution of CTE Request Procedure */

/* +-----+                     +-------+                         +-----+
 * | UT  |                     | LL_A  |                         | LT  |
 * +-----+                     +-------+                         +-----+
 *    |                            |                                |
 *    | Start initiation           |                                |
 *    | CTE Reqest Proc.           |                                |
 *    |--------------------------->|                                |
 *    |                            |                                |
 *    |                            | LL_LE_CTE_REQ                  |
 *    |                            |------------------------------->|
 *    |                            |                                |
 *    |                            | LL_REJECT_EXT_IND              |
 *    |                            | BT_HCI_ERR_UNSUPP_LL_PARAM_VAL |
 *    |                            | or BT_HCI_ERR_INVALID_LL_PARAM |
 *    |                            |<-------------------------------|
 *    |                            |                                |
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *    |                            |                                |
 *    | LE CTE Request Failed      |                                |
 *    |<---------------------------|                                |
 *    |                            |                                |
 *    |                            |                                |
 */
void test_cte_req_rejected_inv_ll_param_central_local(void)
{
	uint8_t err;
	struct node_tx *tx;

	struct pdu_data_llctrl_cte_req local_cte_req = {
		.cte_type_req = BT_HCI_LE_AOD_CTE_1US,
		.min_cte_len_req = BT_HCI_LE_CTE_LEN_MIN,
	};
	struct pdu_data_llctrl_reject_ext_ind remote_reject_ext_ind = {
		.reject_opcode = PDU_DATA_LLCTRL_TYPE_CTE_REQ,
		.error_code = BT_HCI_ERR_UNSUPP_LL_PARAM_VAL,
	};
	struct node_rx_pdu *ntf;

	/* Role */
	test_set_role(&conn, BT_HCI_ROLE_CENTRAL);

	/* Connect */
	ull_cp_state_set(&conn, ULL_CP_CONNECTED);

	/* Initiate an CTE Request Procedure */
	err = ull_cp_cte_req(&conn, local_cte_req.min_cte_len_req, local_cte_req.cte_type_req);
	zassert_equal(err, BT_HCI_ERR_SUCCESS, NULL);

	/* Prepare */
	event_prepare(&conn);

	/* Tx Queue should have one LL Control PDU */
	lt_rx(LL_CTE_REQ, &conn, &tx, &local_cte_req);
	lt_rx_q_is_empty(&conn);

	/* Rx */
	lt_tx(LL_REJECT_EXT_IND, &conn, &remote_reject_ext_ind);

	/* Done */
	event_done(&conn);

	/* Receive notification of sampled CTE response */
	ut_rx_pdu(LL_REJECT_EXT_IND, &ntf, &remote_reject_ext_ind);

	/* There should not be a host notifications */
	ut_rx_q_is_empty();

	/* Release tx node */
	ull_cp_release_tx(&conn, tx);

	zassert_equal(ctx_buffers_free(), CONFIG_BT_CTLR_LLCP_PROC_CTX_BUF_NUM,
				  "Free CTX buffers %d", ctx_buffers_free());
}

/* +-----+                     +-------+                         +-----+
 * | UT  |                     | LL_A  |                         | LT  |
 * +-----+                     +-------+                         +-----+
 *    |                            |                                |
 *    | Start initiation           |                                |
 *    | CTE Reqest Proc.           |                                |
 *    |--------------------------->|                                |
 *    |                            |                                |
 *    |                            | LL_LE_CTE_REQ                  |
 *    |                            |------------------------------->|
 *    |                            |                                |
 *    |                            | LL_REJECT_EXT_IND              |
 *    |                            | BT_HCI_ERR_UNSUPP_LL_PARAM_VAL |
 *    |                            | or BT_HCI_ERR_INVALID_LL_PARAM |
 *    |                            |<-------------------------------|
 *    |                            |                                |
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *    |                            |                                |
 *    | LE CTE Request Failed      |                                |
 *    |<---------------------------|                                |
 *    |                            |                                |
 *    |                            |                                |
 */
void test_cte_req_rejected_inv_ll_param_peripheral_local(void)
{
	uint8_t err;
	struct node_tx *tx;

	struct pdu_data_llctrl_cte_req local_cte_req = {
		.cte_type_req = BT_HCI_LE_AOD_CTE_1US,
		.min_cte_len_req = BT_HCI_LE_CTE_LEN_MIN,
	};
	struct pdu_data_llctrl_reject_ext_ind remote_reject_ext_ind = {
		.reject_opcode = PDU_DATA_LLCTRL_TYPE_CTE_REQ,
		.error_code = BT_HCI_ERR_UNSUPP_LL_PARAM_VAL,
	};
	struct node_rx_pdu *ntf;

	/* Role */
	test_set_role(&conn, BT_HCI_ROLE_PERIPHERAL);

	/* Connect */
	ull_cp_state_set(&conn, ULL_CP_CONNECTED);

	/* Initiate an CTE Request Procedure */
	err = ull_cp_cte_req(&conn, local_cte_req.min_cte_len_req, local_cte_req.cte_type_req);
	zassert_equal(err, BT_HCI_ERR_SUCCESS, NULL);

	/* Prepare */
	event_prepare(&conn);

	/* Tx Queue should have one LL Control PDU */
	lt_rx(LL_CTE_REQ, &conn, &tx, &local_cte_req);
	lt_rx_q_is_empty(&conn);

	/* Rx */
	lt_tx(LL_REJECT_EXT_IND, &conn, &remote_reject_ext_ind);

	/* Done */
	event_done(&conn);

	/* Receive notification of sampled CTE response */
	ut_rx_pdu(LL_REJECT_EXT_IND, &ntf, &remote_reject_ext_ind);

	/* Release tx node */
	ull_cp_release_tx(&conn, tx);

	/* There should not be a host notifications */
	ut_rx_q_is_empty();

	zassert_equal(ctx_buffers_free(), CONFIG_BT_CTLR_LLCP_PROC_CTX_BUF_NUM,
				  "Free CTX buffers %d", ctx_buffers_free());
}

/* +-----+                     +-------+                         +-----+
 * | UT  |                     | LL_A  |                         | LT  |
 * +-----+                     +-------+                         +-----+
 *    |                            |                                |
 *    | Start initiation           |                                |
 *    | CTE Reqest Proc.           |                                |
 *    |--------------------------->|                                |
 *    |                            |                                |
 *    |                            | LL_LE_CTE_REQ                  |
 *    |                            |<-------------------------------|
 *    |                            |                                |
 *    |                            | LL_REJECT_EXT_IND              |
 *    |                            | BT_HCI_ERR_UNSUPP_LL_PARAM_VAL |
 *    |                            |------------------------------->|
 *    |                            |                                |
 */
void test_cte_req_reject_inv_ll_param_central_remote(void)
{
	struct node_tx *tx;

	struct pdu_data_llctrl_cte_req local_cte_req = {
		.cte_type_req = BT_HCI_LE_AOD_CTE_2US,
		.min_cte_len_req = BT_HCI_LE_CTE_LEN_MIN,
	};

	struct pdu_data_llctrl_reject_ext_ind remote_reject_ext_ind = {
		.reject_opcode = PDU_DATA_LLCTRL_TYPE_CTE_REQ,
		.error_code = BT_HCI_ERR_UNSUPP_LL_PARAM_VAL,
	};

	/* Role */
	test_set_role(&conn, BT_HCI_ROLE_CENTRAL);

	/* Connect */
	ull_cp_state_set(&conn, ULL_CP_CONNECTED);

	/* Enable response for CTE request */
	ull_cp_cte_rsp_enable(&conn, true, BT_HCI_LE_CTE_LEN_MAX,
			      (BT_HCI_LE_AOA_CTE_RSP | BT_HCI_LE_AOD_CTE_RSP_1US));

	/* Prepare */
	event_prepare(&conn);

	/* Tx */
	lt_tx(LL_CTE_REQ, &conn, &local_cte_req);

	/* Done */
	event_done(&conn);

	/* Prepare */
	event_prepare(&conn);

	/* Tx Queue should have one LL Control PDU */
	lt_rx(LL_REJECT_EXT_IND, &conn, &tx, &remote_reject_ext_ind);
	lt_rx_q_is_empty(&conn);

	/* TX Ack */
	event_tx_ack(&conn, tx);

	/* Done */
	event_done(&conn);

	/* Release tx node */
	ull_cp_release_tx(&conn, tx);

	/* There should not be a host notifications */
	ut_rx_q_is_empty();

	zassert_equal(ctx_buffers_free(), CONFIG_BT_CTLR_LLCP_PROC_CTX_BUF_NUM,
				  "Free CTX buffers %d", ctx_buffers_free());
}

/* +-----+                     +-------+                         +-----+
 * | UT  |                     | LL_A  |                         | LT  |
 * +-----+                     +-------+                         +-----+
 *    |                            |                                |
 *    | Start initiation           |                                |
 *    | CTE Reqest Proc.           |                                |
 *    |--------------------------->|                                |
 *    |                            |                                |
 *    |                            | LL_LE_CTE_REQ                  |
 *    |                            |<-------------------------------|
 *    |                            |                                |
 *    |                            | LL_REJECT_EXT_IND              |
 *    |                            | BT_HCI_ERR_UNSUPP_LL_PARAM_VAL |
 *    |                            |------------------------------->|
 *    |                            |                                |
 */
void test_cte_req_reject_inv_ll_param_peripheral_remote(void)
{
	struct node_tx *tx;

	struct pdu_data_llctrl_cte_req local_cte_req = {
		.cte_type_req = BT_HCI_LE_AOD_CTE_2US,
		.min_cte_len_req = BT_HCI_LE_CTE_LEN_MIN,
	};

	struct pdu_data_llctrl_reject_ext_ind remote_reject_ext_ind = {
		.reject_opcode = PDU_DATA_LLCTRL_TYPE_CTE_REQ,
		.error_code = BT_HCI_ERR_UNSUPP_LL_PARAM_VAL,
	};

	/* Role */
	test_set_role(&conn, BT_HCI_ROLE_PERIPHERAL);

	/* Connect */
	ull_cp_state_set(&conn, ULL_CP_CONNECTED);

	/* Enable response for CTE request */
	ull_cp_cte_rsp_enable(&conn, true, BT_HCI_LE_CTE_LEN_MAX,
			      (BT_HCI_LE_AOA_CTE | BT_HCI_LE_AOD_CTE_1US));

	/* Prepare */
	event_prepare(&conn);

	/* Tx */
	lt_tx(LL_CTE_REQ, &conn, &local_cte_req);

	/* Done */
	event_done(&conn);

	/* Prepare */
	event_prepare(&conn);

	/* Tx Queue should have one LL Control PDU */
	lt_rx(LL_REJECT_EXT_IND, &conn, &tx, &remote_reject_ext_ind);
	lt_rx_q_is_empty(&conn);

	/* TX Ack */
	event_tx_ack(&conn, tx);

	/* Done */
	event_done(&conn);

	/* Release tx node */
	ull_cp_release_tx(&conn, tx);

	/* There should not be a host notifications */
	ut_rx_q_is_empty();

	zassert_equal(ctx_buffers_free(), CONFIG_BT_CTLR_LLCP_PROC_CTX_BUF_NUM,
				  "Free CTX buffers %d", ctx_buffers_free());
}

/* Tests related with PHY update procedure and CTE request procedure "collision" */

#define PREFER_S2_CODING 0U
#define HOST_INITIATED 1U
#define PHY_UPDATE_INSTANT_DELTA 6U
#define PHY_PREFER_ANY (PHY_1M | PHY_2M | PHY_CODED)
/* Arbitrary value used for setting effective maximum number of TX/RX octets */
#define PDU_PDU_MAX_OCTETS (PDU_DC_PAYLOAD_SIZE_MIN * 3)

static void check_pref_phy_state(const struct ll_conn *conn, uint8_t phy_tx, uint8_t phy_rx)
{
	zassert_equal(conn->phy_pref_rx, phy_rx,
		      "Preferred RX PHY mismatch %d (actual) != %d (expected)", conn->phy_pref_rx,
		      phy_rx);
	zassert_equal(conn->phy_pref_tx, phy_tx,
		      "Preferred TX PHY mismatch %d (actual) != %d (expected)", conn->phy_pref_tx,
		      phy_tx);
}

static void check_current_phy_state(const struct ll_conn *conn, uint8_t phy_tx, uint8_t flags,
				    uint8_t phy_rx)
{
	zassert_equal(conn->lll.phy_rx, phy_rx,
		      "Current RX PHY mismatch %d (actual) != %d (expected)", conn->lll.phy_rx,
		      phy_rx);
	zassert_equal(conn->lll.phy_tx, phy_tx,
		      "Current TX PHY mismatch %d (actual) != %d (expected)", conn->lll.phy_tx,
		      phy_tx);
	zassert_equal(conn->lll.phy_flags, flags,
		      "Current Flags mismatch %d (actual) != %d (expected)", conn->lll.phy_flags,
		      flags);
}

static bool is_instant_reached(struct ll_conn *conn, uint16_t instant)
{
	/* Check if instant is in the past.
	 *
	 * NOTE: If conn_event > instant then subtract operation will result in value greather than
	 *       0x7FFF for uint16_t type. This is based on modulo 65536 math. The 0x7FFF is
	 *       maximum positive difference between actual value of connection event counter and
	 *       instant.
	 */
	return (uint16_t)(event_counter(conn) - instant) <= (uint16_t)0x7FFF;
}

static uint16_t pu_event_counter(struct ll_conn *conn)
{
	struct lll_conn *lll;
	uint16_t event_counter;

	lll = &conn->lll;

	/* Calculate current event counter */
	event_counter = lll->event_counter + lll->latency_prepare;

	return event_counter;
}

static void phy_update_setup(void)
{
	test_setup(&conn);

	/* Emulate initial conn state */
	conn.phy_pref_rx = PHY_PREFER_ANY;
	conn.phy_pref_tx = PHY_PREFER_ANY;
	conn.lll.phy_flags = PREFER_S2_CODING;
	conn.lll.phy_tx_time = PHY_1M;
	conn.lll.phy_rx = PHY_1M;
	conn.lll.phy_tx = PHY_1M;

	/* Init DLE data */
	ull_conn_default_tx_octets_set(251);
	ull_conn_default_tx_time_set(2120);
	ull_dle_init(&conn, PHY_1M);
	/* Emulate different remote numbers to trigger update of effective max TX octets and time.
	 * Numbers are taken arbitrary.
	 */
	conn.lll.dle.remote.max_tx_octets = PDU_PDU_MAX_OCTETS;
	conn.lll.dle.remote.max_rx_octets = PDU_PDU_MAX_OCTETS;
	conn.lll.dle.remote.max_tx_time = PDU_DC_MAX_US(conn.lll.dle.remote.max_tx_octets, PHY_1M);
	conn.lll.dle.remote.max_rx_time = PDU_DC_MAX_US(conn.lll.dle.remote.max_rx_octets, PHY_1M);
	ull_dle_update_eff(&conn);
}

static void run_local_cte_req(struct pdu_data_llctrl_cte_req *cte_req)
{
	struct pdu_data_llctrl_cte_rsp remote_cte_rsp = {};
	struct node_tx *tx = NULL;
	struct node_rx_pdu *ntf;

	/* The CTE request should already be in local control procedures queue */

	/* Prepare */
	event_prepare(&conn);

	/* Tx Queue should have one LL Control PDU */
	lt_rx(LL_CTE_REQ, &conn, &tx, cte_req);
	lt_rx_q_is_empty(&conn);

	/* Rx */
	lt_tx(LL_CTE_RSP, &conn, &remote_cte_rsp);

	/* Done */
	event_done(&conn);

	/* Receive notification of sampled CTE response */
	ut_rx_pdu(LL_CTE_RSP, &ntf, &remote_cte_rsp);

	/* There should not be a host notifications */
	ut_rx_q_is_empty();

	/* Release tx node */
	ull_cp_release_tx(&conn, tx);
}

void wait_for_phy_update_instant(uint8_t instant)
{
	/* */
	while (!is_instant_reached(&conn, instant)) {
		/* Prepare */
		event_prepare(&conn);

		/* Tx Queue should NOT have a LL Control PDU */
		lt_rx_q_is_empty(&conn);

		/* Done */
		event_done(&conn);

		check_current_phy_state(&conn, PHY_1M, PREFER_S2_CODING, PHY_1M);

		/* There should NOT be a host notification */
		ut_rx_q_is_empty();
	}
}

void check_phy_update_and_cte_req_complete(bool is_local, struct pdu_data_llctrl_cte_req *cte_req,
					   struct pdu_data_llctrl_phy_req *phy_req,
					   uint8_t ctx_num_at_end)
{
	struct pdu_data_llctrl_length_rsp length_ntf = {
		PDU_PDU_MAX_OCTETS, PDU_DC_MAX_US(PDU_PDU_MAX_OCTETS, phy_req->tx_phys),
		PDU_PDU_MAX_OCTETS, PDU_DC_MAX_US(PDU_PDU_MAX_OCTETS, phy_req->rx_phys)
	};
	struct node_rx_pu pu = { .status = BT_HCI_ERR_SUCCESS };
	struct pdu_data_llctrl_cte_rsp remote_cte_rsp = {};
	struct node_tx *tx = NULL;
	struct node_rx_pdu *ntf;

	/* Prepare */
	event_prepare(&conn);

	if (!is_local && cte_req != NULL) {
		/* Handle remote PHY update request completion and local CTE reques in the same
		 * event.
		 */

		/* Tx Queue should have one LL Control PDU */
		lt_rx(LL_CTE_REQ, &conn, &tx, cte_req);
		lt_rx_q_is_empty(&conn);

		/* Rx */
		lt_tx(LL_CTE_RSP, &conn, &remote_cte_rsp);
	} else {
		/* Tx Queue should NOT have a LL Control PDU */
		lt_rx_q_is_empty(&conn);
	}

	/* Done */
	event_done(&conn);

	/* There should be two host notifications, one pu and one dle */
	ut_rx_node(NODE_PHY_UPDATE, &ntf, &pu);
	ut_rx_pdu(LL_LENGTH_RSP, &ntf, &length_ntf);

	/* Release Ntf */
	ull_cp_release_ntf(ntf);

	if (!is_local && cte_req != NULL) {
		/* Receive notification of sampled CTE response */
		ut_rx_pdu(LL_CTE_RSP, &ntf, &remote_cte_rsp);

		/* Release Ntf */
		ull_cp_release_ntf(ntf);

		/* Release tx node */
		ull_cp_release_tx(&conn, tx);
	}

	/* There should not be a host notifications */
	ut_rx_q_is_empty();

	check_current_phy_state(&conn, phy_req->tx_phys, PREFER_S2_CODING, phy_req->tx_phys);
	if (is_local) {
		check_pref_phy_state(&conn, phy_req->rx_phys, phy_req->tx_phys);
	} else {
		check_pref_phy_state(&conn, PHY_PREFER_ANY, PHY_PREFER_ANY);
	}

	/* There is still queued CTE REQ so number of contexts is smaller by 1 than max */
	zassert_equal(ctx_buffers_free(), ctx_num_at_end, "Free CTX buffers %d",
		      ctx_buffers_free());
}

/**
 * @brief The function executes PHY update procedure in central role.
 *
 * The main goal for the function is to run and evaluate the PHY update control procedure.
 * In case the PHY request is remote request and there is a local CTE request then
 * after PHY update completion CTE request is executed in the same event.
 * In this situation the function processes verification of CTE request completion also.
 *
 * @param is_local        Flag informing if PHY request is local or remote.
 * @param cte_req         Parameters of CTE request procedure. If it is NULL there were no CTE
 *                        request.
 * @param phy_req         Parameters of PHY update reques.
 * @param events_at_start Number of connection events at function start.
 * @param ctx_num_at_end  Expected number of free procedure contexts at function end.
 */
static void run_phy_update_central(bool is_local, struct pdu_data_llctrl_cte_req *cte_req,
				   struct pdu_data_llctrl_phy_req *phy_req, uint8_t events_at_start,
				   uint8_t ctx_num_at_end)
{
	struct pdu_data_llctrl_phy_req rsp = { .rx_phys = PHY_PREFER_ANY,
					       .tx_phys = PHY_PREFER_ANY };
	struct pdu_data_llctrl_phy_upd_ind ind = { .instant = events_at_start +
							      PHY_UPDATE_INSTANT_DELTA,
						   .c_to_p_phy = phy_req->tx_phys,
						   .p_to_c_phy = phy_req->rx_phys };
	struct node_tx *tx = NULL;
	struct pdu_data *pdu;
	uint16_t instant;

	/* Prepare */
	event_prepare(&conn);

	if (is_local) {
		/* Tx Queue should have one LL Control PDU */
		lt_rx(LL_PHY_REQ, &conn, &tx, phy_req);
		lt_rx_q_is_empty(&conn);

		/* TX Ack */
		event_tx_ack(&conn, tx);

		/* Rx */
		lt_tx(LL_PHY_RSP, &conn, &rsp);

		ind.instant += 1;
	}

	/* Done */
	event_done(&conn);

	/* Check that data tx was paused */
	zassert_equal(conn.tx_q.pause_data, 1U, "Data tx is not paused");

	if (tx != NULL) {
		/* Release Tx */
		ull_cp_release_tx(&conn, tx);
	}
	/* Prepare */
	event_prepare(&conn);

	/* Tx Queue should have one LL Control PDU */
	lt_rx(LL_PHY_UPDATE_IND, &conn, &tx, &ind);
	lt_rx_q_is_empty(&conn);

	/* TX Ack */
	event_tx_ack(&conn, tx);

	/* Check that data tx is no lonnger paused */
	zassert_equal(conn.tx_q.pause_data, 0U, "Data tx is paused");

	/* Done */
	event_done(&conn);

	/* Save Instant */
	pdu = (struct pdu_data *)tx->pdu;
	instant = sys_le16_to_cpu(pdu->llctrl.phy_upd_ind.instant);

	/* Release Tx */
	ull_cp_release_tx(&conn, tx);

	wait_for_phy_update_instant(instant);

	check_phy_update_and_cte_req_complete(is_local, cte_req, phy_req, ctx_num_at_end);
}

/**
 * @brief The function executes PHY update procedure in peripheral role.
 *
 * The main goal for the function is to run and evaluate the PHY update control procedure.
 * In case the PHY request is remote request and there is a local CTE request then
 * after PHY update completion CTE request is executed in the same event.
 * In this situation the function processes verification of CTE request completion also.
 *
 * @param is_local        Flag informing if PHY request is local or remote.
 * @param cte_req         Parameters of CTE request procedure. If it is NULL there were no CTE
 *                        request.
 * @param phy_req         Parameters of PHY update reques.
 * @param events_at_start Number of connection events at function start.
 * @param ctx_num_at_end  Expected number of free procedure contexts at function end.
 */
static void run_phy_update_peripheral(bool is_local, struct pdu_data_llctrl_cte_req *cte_req,
				      struct pdu_data_llctrl_phy_req *phy_req,
				      uint8_t events_at_start, uint8_t ctx_num_at_end)
{
	struct pdu_data_llctrl_phy_req rsp = { .rx_phys = PHY_PREFER_ANY,
					       .tx_phys = PHY_PREFER_ANY };
	struct pdu_data_llctrl_phy_upd_ind ind = { .c_to_p_phy = phy_req->rx_phys,
						   .p_to_c_phy = phy_req->tx_phys };
	struct node_tx *tx;
	uint16_t instant;

	/* Prepare */
	event_prepare(&conn);

	if (is_local) {
		/* Tx Queue should have one LL Control PDU */
		lt_rx(LL_PHY_REQ, &conn, &tx, phy_req);
		lt_rx_q_is_empty(&conn);

		/* TX Ack */
		event_tx_ack(&conn, tx);
	}

	/* Done */
	event_done(&conn);

	if (is_local) {
		/* Release Tx */
		ull_cp_release_tx(&conn, tx);
	} else {
		/* Check that data tx was paused */
		zassert_equal(conn.tx_q.pause_data, 1U, "Data tx is not paused");
	}

	/* Prepare */
	event_prepare(&conn);

	instant = event_counter(&conn) + PHY_UPDATE_INSTANT_DELTA;
	ind.instant = instant;

	if (is_local) {
		/* Tx Queue should NOT have a LL Control PDU */
		lt_rx_q_is_empty(&conn);

		/* Tx Queue should have one LL Control PDU */
		lt_tx(LL_PHY_UPDATE_IND, &conn, &ind);
	} else {
		/* Tx Queue should have one LL Control PDU */
		lt_rx(LL_PHY_RSP, &conn, &tx, &rsp);
		lt_rx_q_is_empty(&conn);

		/* Rx */
		lt_tx(LL_PHY_UPDATE_IND, &conn, &ind);

		/* We are sending RSP, so data tx should be paused until after tx ack */
		zassert_equal(conn.tx_q.pause_data, 1U, "Data tx is not paused");

		/* TX Ack */
		event_tx_ack(&conn, tx);

		/* Check that data tx is no longer paused */
		zassert_equal(conn.tx_q.pause_data, 0U, "Data tx is paused");
	}

	/* Done */
	event_done(&conn);

	if (!is_local) {
		/* Release Tx */
		ull_cp_release_tx(&conn, tx);
	}

	wait_for_phy_update_instant(instant);

	check_phy_update_and_cte_req_complete(is_local, cte_req, phy_req, ctx_num_at_end);
}

static void test_local_cte_req_wait_for_phy_update_complete_and_disable(uint8_t role)
{
	uint8_t err;

	struct pdu_data_llctrl_cte_req local_cte_req = {
		.cte_type_req = BT_HCI_LE_AOA_CTE,
		.min_cte_len_req = BT_HCI_LE_CTE_LEN_MIN,
	};
	struct pdu_data_llctrl_phy_req phy_req = { .rx_phys = PHY_CODED, .tx_phys = PHY_CODED };

	phy_update_setup();

	/* Role */
	test_set_role(&conn, role);

	/* Connect */
	ull_cp_state_set(&conn, ULL_CP_CONNECTED);

	/* Initiate an PHY Update Procedure */
	err = ull_cp_phy_update(&conn, PHY_CODED, PREFER_S2_CODING, PHY_CODED, HOST_INITIATED);
	zassert_equal(err, BT_HCI_ERR_SUCCESS, NULL);

	/* Initiate an CTE Request Procedure */
	err = ull_cp_cte_req(&conn, local_cte_req.min_cte_len_req, local_cte_req.cte_type_req);
	zassert_equal(err, BT_HCI_ERR_SUCCESS, NULL);

	if (role == BT_HCI_ROLE_CENTRAL) {
		run_phy_update_central(true, NULL, &phy_req, pu_event_counter(&conn),
				       CONFIG_BT_CTLR_LLCP_PROC_CTX_BUF_NUM - 1);
	} else {
		run_phy_update_peripheral(true, NULL, &phy_req, pu_event_counter(&conn),
					  CONFIG_BT_CTLR_LLCP_PROC_CTX_BUF_NUM - 1);
	}

	/* In this test CTE request is local procedure. Local procedures are handled after remote
	 * procedures, hence PHY update will be handled one event after completion of CTE request.
	 */

	/* Prepare */
	event_prepare(&conn);

	/* Tx Queue should not have any LL Control PDU */
	lt_rx_q_is_empty(&conn);

	/* Done */
	event_done(&conn);

	/* There should not be a host notifications */
	ut_rx_q_is_empty();

	zassert_equal(ctx_buffers_free(), CONFIG_BT_CTLR_LLCP_PROC_CTX_BUF_NUM,
		      "Free CTX buffers %d", ctx_buffers_free());
}

void test_central_local_cte_req_wait_for_phy_update_complete_and_disable(void)
{
	test_local_cte_req_wait_for_phy_update_complete_and_disable(BT_HCI_ROLE_CENTRAL);
}

void test_peripheral_local_cte_req_wait_for_phy_update_complete_and_disable(void)
{
	test_local_cte_req_wait_for_phy_update_complete_and_disable(BT_HCI_ROLE_PERIPHERAL);
}

static void test_local_cte_req_wait_for_phy_update_complete(uint8_t role)
{
	struct pdu_data_llctrl_cte_req local_cte_req = {
		.cte_type_req = BT_HCI_LE_AOA_CTE,
		.min_cte_len_req = BT_HCI_LE_CTE_LEN_MIN,
	};
	struct pdu_data_llctrl_phy_req phy_req = { .rx_phys = PHY_2M, .tx_phys = PHY_2M };

	uint8_t err;

	phy_update_setup();

	/* Role */
	test_set_role(&conn, role);

	/* Connect */
	ull_cp_state_set(&conn, ULL_CP_CONNECTED);

	/* Initiate an PHY Update Procedure */
	err = ull_cp_phy_update(&conn, phy_req.rx_phys, PREFER_S2_CODING, phy_req.tx_phys,
				HOST_INITIATED);
	zassert_equal(err, BT_HCI_ERR_SUCCESS, NULL);

	/* Initiate an CTE Request Procedure */
	err = ull_cp_cte_req(&conn, local_cte_req.min_cte_len_req, local_cte_req.cte_type_req);
	zassert_equal(err, BT_HCI_ERR_SUCCESS, NULL);

	if (role == BT_HCI_ROLE_CENTRAL) {
		run_phy_update_central(true, &local_cte_req, &phy_req, pu_event_counter(&conn),
				       CONFIG_BT_CTLR_LLCP_PROC_CTX_BUF_NUM - 1);
	} else {
		run_phy_update_peripheral(true, &local_cte_req, &phy_req, pu_event_counter(&conn),
					  CONFIG_BT_CTLR_LLCP_PROC_CTX_BUF_NUM - 1);
	}

	/* PHY update was completed. Handle CTE request */
	run_local_cte_req(&local_cte_req);

	zassert_equal(ctx_buffers_free(), CONFIG_BT_CTLR_LLCP_PROC_CTX_BUF_NUM,
		      "Free CTX buffers %d", ctx_buffers_free());
}

void test_central_local_cte_req_wait_for_phy_update_complete(void)
{
	test_local_cte_req_wait_for_phy_update_complete(BT_HCI_ROLE_CENTRAL);
}

void test_peripheral_local_cte_req_wait_for_phy_update_complete(void)
{
	test_local_cte_req_wait_for_phy_update_complete(BT_HCI_ROLE_PERIPHERAL);
}

static void test_local_phy_update_wait_for_cte_req_complete(uint8_t role)
{
	struct pdu_data_llctrl_phy_req phy_req = { .rx_phys = PHY_CODED, .tx_phys = PHY_CODED };
	struct pdu_data_llctrl_cte_req local_cte_req = {
		.cte_type_req = BT_HCI_LE_AOA_CTE,
		.min_cte_len_req = BT_HCI_LE_CTE_LEN_MIN,
	};
	uint8_t err;

	phy_update_setup();

	/* Role */
	test_set_role(&conn, role);

	/* Connect */
	ull_cp_state_set(&conn, ULL_CP_CONNECTED);

	/* Initiate an CTE Request Procedure */
	err = ull_cp_cte_req(&conn, local_cte_req.min_cte_len_req, local_cte_req.cte_type_req);
	zassert_equal(err, BT_HCI_ERR_SUCCESS, NULL);

	/* Initiate an PHY Update Procedure */
	err = ull_cp_phy_update(&conn, PHY_CODED, PREFER_S2_CODING, PHY_CODED, HOST_INITIATED);
	zassert_equal(err, BT_HCI_ERR_SUCCESS, NULL);

	/* Handle CTE request */
	run_local_cte_req(&local_cte_req);

	zassert_equal(ctx_buffers_free(), CONFIG_BT_CTLR_LLCP_PROC_CTX_BUF_NUM - 1,
		      "Free CTX buffers %d", ctx_buffers_free());

	if (role == BT_HCI_ROLE_CENTRAL) {
		run_phy_update_central(true, NULL, &phy_req, pu_event_counter(&conn),
				       CONFIG_BT_CTLR_LLCP_PROC_CTX_BUF_NUM);
	} else {
		run_phy_update_peripheral(true, NULL, &phy_req, pu_event_counter(&conn),
					  CONFIG_BT_CTLR_LLCP_PROC_CTX_BUF_NUM);
	}
}

void test_central_local_phy_update_wait_for_cte_req_complete(void)
{
	test_local_phy_update_wait_for_cte_req_complete(BT_HCI_ROLE_CENTRAL);
}

void test_peripheral_local_phy_update_wait_for_cte_req_complete(void)
{
	test_local_phy_update_wait_for_cte_req_complete(BT_HCI_ROLE_PERIPHERAL);
}

static void run_remote_cte_req(struct pdu_data_llctrl_cte_req *cte_req)
{
	struct pdu_data_llctrl_cte_rsp remote_cte_rsp = {};
	struct node_tx *tx;

	/* The CTE response should already be enabled and request PDU should already be
	 * received.
	 */

	/* Prepare */
	event_prepare(&conn);

	/* Tx Queue should have one LL Control PDU */
	lt_rx(LL_CTE_RSP, &conn, &tx, &remote_cte_rsp);
	lt_rx_q_is_empty(&conn);

	/* TX Ack */
	event_tx_ack(&conn, tx);

	/* Done */
	event_done(&conn);

	/* Release tx node */
	ull_cp_release_tx(&conn, tx);

	/* There should not be a host notifications */
	ut_rx_q_is_empty();
}

static void test_phy_update_wait_for_remote_cte_req_complete(uint8_t role)
{
	struct pdu_data_llctrl_cte_req local_cte_req = {
		.cte_type_req = BT_HCI_LE_AOA_CTE,
		.min_cte_len_req = BT_HCI_LE_CTE_LEN_MIN,
	};
	struct pdu_data_llctrl_phy_req phy_req = { .rx_phys = PHY_CODED, .tx_phys = PHY_CODED };
	uint8_t err;

	phy_update_setup();

	/* Role */
	test_set_role(&conn, role);

	/* Connect */
	ull_cp_state_set(&conn, ULL_CP_CONNECTED);

	/* Enable response for CTE request */
	ull_cp_cte_rsp_enable(&conn, true, BT_HCI_LE_CTE_LEN_MAX,
			      (BT_HCI_LE_AOA_CTE | BT_HCI_LE_AOD_CTE_1US | BT_HCI_LE_AOD_CTE_2US));

	/* Prepare */
	event_prepare(&conn);

	/* Tx */
	lt_tx(LL_CTE_REQ, &conn, &local_cte_req);

	/* Done */
	event_done(&conn);

	/* Initiate an PHY Update Procedure */
	err = ull_cp_phy_update(&conn, PHY_CODED, PREFER_S2_CODING, PHY_CODED, HOST_INITIATED);
	zassert_equal(err, BT_HCI_ERR_SUCCESS, NULL);

	run_remote_cte_req(&local_cte_req);

	/* There should not be a host notifications */
	ut_rx_q_is_empty();

	zassert_equal(ctx_buffers_free(), CONFIG_BT_CTLR_LLCP_PROC_CTX_BUF_NUM - 1,
		      "Free CTX buffers %d", ctx_buffers_free());

	if (role == BT_HCI_ROLE_CENTRAL) {
		run_phy_update_central(true, NULL, &phy_req, pu_event_counter(&conn),
				       CONFIG_BT_CTLR_LLCP_PROC_CTX_BUF_NUM);
	} else {
		run_phy_update_peripheral(true, NULL, &phy_req, pu_event_counter(&conn),
					  CONFIG_BT_CTLR_LLCP_PROC_CTX_BUF_NUM);
	}
}

void test_central_phy_update_wait_for_remote_cte_req_complete(void)
{
	test_phy_update_wait_for_remote_cte_req_complete(BT_HCI_ROLE_CENTRAL);
}

void test_peripheral_phy_update_wait_for_remote_cte_req_complete(void)
{
	test_phy_update_wait_for_remote_cte_req_complete(BT_HCI_ROLE_PERIPHERAL);
}

static void test_cte_req_wait_for_remote_phy_update_complete_and_disable(uint8_t role)
{
	uint8_t err;

	struct pdu_data_llctrl_cte_req local_cte_req = {
		.cte_type_req = BT_HCI_LE_AOA_CTE,
		.min_cte_len_req = BT_HCI_LE_CTE_LEN_MIN,
	};
	struct pdu_data_llctrl_phy_req phy_req = { .rx_phys = PHY_CODED, .tx_phys = PHY_CODED };

	phy_update_setup();

	/* Role */
	test_set_role(&conn, role);

	/* Connect */
	ull_cp_state_set(&conn, ULL_CP_CONNECTED);

	/* Prepare */
	event_prepare(&conn);

	/* Tx */
	lt_tx(LL_PHY_REQ, &conn, &phy_req);

	/* Done */
	event_done(&conn);

	/* Initiate an CTE Request Procedure */
	err = ull_cp_cte_req(&conn, local_cte_req.min_cte_len_req, local_cte_req.cte_type_req);
	zassert_equal(err, BT_HCI_ERR_SUCCESS, NULL);

	if (role == BT_HCI_ROLE_CENTRAL) {
		run_phy_update_central(false, NULL, &phy_req, pu_event_counter(&conn),
				       CONFIG_BT_CTLR_LLCP_PROC_CTX_BUF_NUM);
	} else {
		run_phy_update_peripheral(false, NULL, &phy_req, pu_event_counter(&conn),
					  CONFIG_BT_CTLR_LLCP_PROC_CTX_BUF_NUM);
	}

	/* There is no special handling of CTE REQ completion. It is done when instant happens just
	 * after remote PHY update completes.
	 */
}

void test_central_cte_req_wait_for_remote_phy_update_complete_and_disable(void)
{
	test_cte_req_wait_for_remote_phy_update_complete_and_disable(BT_HCI_ROLE_CENTRAL);
}

void test_peripheral_cte_req_wait_for_remote_phy_update_complete_and_disable(void)
{
	test_cte_req_wait_for_remote_phy_update_complete_and_disable(BT_HCI_ROLE_PERIPHERAL);
}

static void test_cte_req_wait_for_remote_phy_update_complete(uint8_t role)
{
	uint8_t err;

	struct pdu_data_llctrl_cte_req local_cte_req = {
		.cte_type_req = BT_HCI_LE_AOA_CTE,
		.min_cte_len_req = BT_HCI_LE_CTE_LEN_MIN,
	};
	struct pdu_data_llctrl_phy_req phy_req = { .rx_phys = PHY_2M, .tx_phys = PHY_2M };

	phy_update_setup();

	/* Role */
	test_set_role(&conn, role);

	/* Connect */
	ull_cp_state_set(&conn, ULL_CP_CONNECTED);

	/* Prepare */
	event_prepare(&conn);

	/* Tx */
	lt_tx(LL_PHY_REQ, &conn, &phy_req);

	/* Done */
	event_done(&conn);

	/* Initiate an CTE Request Procedure */
	err = ull_cp_cte_req(&conn, local_cte_req.min_cte_len_req, local_cte_req.cte_type_req);
	zassert_equal(err, BT_HCI_ERR_SUCCESS, NULL);

	if (role == BT_HCI_ROLE_CENTRAL) {
		run_phy_update_central(false, &local_cte_req, &phy_req, pu_event_counter(&conn),
				       CONFIG_BT_CTLR_LLCP_PROC_CTX_BUF_NUM);
	} else {
		run_phy_update_peripheral(false, &local_cte_req, &phy_req, pu_event_counter(&conn),
					  CONFIG_BT_CTLR_LLCP_PROC_CTX_BUF_NUM);
	}

	/* There is no special handling of CTE REQ completion here. It is done when instant happens
	 * just after remote PHY update completes.
	 */
}

void test_central_cte_req_wait_for_remote_phy_update_complete(void)
{
	test_cte_req_wait_for_remote_phy_update_complete(BT_HCI_ROLE_CENTRAL);
}

void test_peripheral_cte_req_wait_for_remote_phy_update_complete(void)
{
	test_cte_req_wait_for_remote_phy_update_complete(BT_HCI_ROLE_PERIPHERAL);
}

void test_main(void)
{
	ztest_test_suite(
		cte_req,
		ztest_unit_test_setup_teardown(test_cte_req_central_local, setup, unit_test_noop),
		ztest_unit_test_setup_teardown(test_cte_req_peripheral_local, setup,
					       unit_test_noop),
		ztest_unit_test_setup_teardown(test_cte_req_central_remote, setup, unit_test_noop),
		ztest_unit_test_setup_teardown(test_cte_req_peripheral_remote, setup,
					       unit_test_noop),
		ztest_unit_test_setup_teardown(test_cte_req_rejected_inv_ll_param_central_local,
					       setup, unit_test_noop),
		ztest_unit_test_setup_teardown(test_cte_req_rejected_inv_ll_param_peripheral_local,
					       setup, unit_test_noop),
		ztest_unit_test_setup_teardown(test_cte_req_reject_inv_ll_param_central_remote,
					       setup, unit_test_noop),
		ztest_unit_test_setup_teardown(test_cte_req_reject_inv_ll_param_peripheral_remote,
					       setup, unit_test_noop),
		ztest_unit_test_setup_teardown(
			test_central_local_cte_req_wait_for_phy_update_complete_and_disable, setup,
			unit_test_noop),
		ztest_unit_test_setup_teardown(
			test_central_local_cte_req_wait_for_phy_update_complete_and_disable, setup,
			unit_test_noop),
		ztest_unit_test_setup_teardown(
			test_peripheral_local_cte_req_wait_for_phy_update_complete, setup,
			unit_test_noop),
		ztest_unit_test_setup_teardown(
			test_central_local_phy_update_wait_for_cte_req_complete, setup,
			unit_test_noop),
		ztest_unit_test_setup_teardown(
			test_peripheral_local_phy_update_wait_for_cte_req_complete, setup,
			unit_test_noop),
		ztest_unit_test_setup_teardown(
			test_peripheral_local_cte_req_wait_for_phy_update_complete, setup,
			unit_test_noop),
		ztest_unit_test_setup_teardown(
			test_central_phy_update_wait_for_remote_cte_req_complete, setup,
			unit_test_noop),
		ztest_unit_test_setup_teardown(
			test_peripheral_phy_update_wait_for_remote_cte_req_complete, setup,
			unit_test_noop),
		ztest_unit_test_setup_teardown(
			test_central_cte_req_wait_for_remote_phy_update_complete_and_disable, setup,
			unit_test_noop),
		ztest_unit_test_setup_teardown(
			test_peripheral_cte_req_wait_for_remote_phy_update_complete_and_disable,
			setup, unit_test_noop),
		ztest_unit_test_setup_teardown(
			test_central_cte_req_wait_for_remote_phy_update_complete, setup,
			unit_test_noop),
		ztest_unit_test_setup_teardown(
			test_peripheral_cte_req_wait_for_remote_phy_update_complete, setup,
			unit_test_noop));
	ztest_run_test_suite(cte_req);
}
