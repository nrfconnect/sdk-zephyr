/*
 * Copyright (c) 2017-2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#if defined(CONFIG_BT_CTLR_ADV_SET)
#define BT_CTLR_ADV_SET CONFIG_BT_CTLR_ADV_SET
#else /* CONFIG_BT_CTLR_ADV_SET */
#define BT_CTLR_ADV_SET 1
#endif /* CONFIG_BT_CTLR_ADV_SET */

#include "lll_adv_pdu.h"

struct lll_adv_aux {
	struct lll_hdr hdr;
	struct lll_adv *adv;

	uint32_t ticks_offset;

	struct lll_adv_pdu data;

#if defined(CONFIG_BT_CTLR_TX_PWR_DYNAMIC_CONTROL)
	int8_t tx_pwr_lvl;
#endif /* CONFIG_BT_CTLR_TX_PWR_DYNAMIC_CONTROL */
};

struct lll_adv_iso {
	struct lll_hdr hdr;
};

struct lll_adv_sync {
	struct lll_hdr hdr;
	struct lll_adv *adv;
#if defined(CONFIG_BT_CTLR_ADV_ISO)
	struct lll_adv_iso *adv_iso;
#endif /* CONFIG_BT_CTLR_ADV_ISO */

	uint8_t access_addr[4];
	uint8_t crc_init[3];

	uint16_t latency_prepare;
	uint16_t latency_event;
	uint16_t event_counter;

	uint8_t data_chan_map[5];
	uint8_t data_chan_count:6;
	uint16_t data_chan_id;

	uint32_t ticks_offset;

	struct lll_adv_pdu data;

#if defined(CONFIG_BT_CTLR_TX_PWR_DYNAMIC_CONTROL)
	int8_t tx_pwr_lvl;
#endif /* CONFIG_BT_CTLR_TX_PWR_DYNAMIC_CONTROL */

#if IS_ENABLED(CONFIG_BT_CTLR_DF_ADV_CTE_TX)
	/* This flag is used only by LLL. It holds information if CTE
	 * transmission was started by LLL.
	 */
	uint8_t cte_started:1;
#endif /* CONFIG_BT_CTLR_DF_ADV_CTE_TX */
};

struct lll_adv {
	struct lll_hdr hdr;

#if defined(CONFIG_BT_PERIPHERAL)
	/* NOTE: conn context has to be after lll_hdr */
	struct lll_conn *conn;
	uint8_t is_hdcd:1;
#endif /* CONFIG_BT_PERIPHERAL */

	uint8_t chan_map:3;
	uint8_t chan_map_curr:3;
	uint8_t filter_policy:2;

#if defined(CONFIG_BT_CTLR_ADV_EXT)
	uint8_t phy_p:3;
	uint8_t phy_s:3;
#endif /* CONFIG_BT_CTLR_ADV_EXT */

#if defined(CONFIG_BT_CTLR_SCAN_REQ_NOTIFY)
	uint8_t scan_req_notify:1;
#endif

#if defined(CONFIG_BT_HCI_MESH_EXT)
	uint8_t is_mesh:1;
#endif /* CONFIG_BT_HCI_MESH_EXT */

#if defined(CONFIG_BT_CTLR_PRIVACY)
	uint8_t  rl_idx;
#endif /* CONFIG_BT_CTLR_PRIVACY */

	struct lll_adv_pdu adv_data;
	struct lll_adv_pdu scan_rsp;

#if defined(CONFIG_BT_CTLR_ADV_EXT)
	struct lll_adv_aux *aux;

#if defined(CONFIG_BT_CTLR_ADV_PERIODIC)
	struct lll_adv_sync *sync;
#endif /* CONFIG_BT_CTLR_ADV_PERIODIC */
#endif /* CONFIG_BT_CTLR_ADV_EXT */

#if defined(CONFIG_BT_CTLR_TX_PWR_DYNAMIC_CONTROL)
	int8_t tx_pwr_lvl;
#endif /* CONFIG_BT_CTLR_TX_PWR_DYNAMIC_CONTROL */

#if defined(CONFIG_BT_CTLR_ADV_EXT)
	struct node_rx_hdr *node_rx_adv_term;
#endif /* CONFIG_BT_CTLR_ADV_EXT */
};

int lll_adv_init(void);
int lll_adv_reset(void);
void lll_adv_prepare(void *param);

extern uint16_t ull_adv_lll_handle_get(struct lll_adv *lll);
