/*
 * Copyright (c) 2016-2019 Nordic Semiconductor ASA
 * Copyright (c) 2016 Vinayak Kariappa Chettimada
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/types.h>
#include <toolchain.h>
#include <soc.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_vs.h>

#include "hal/ccm.h"
#include "hal/radio.h"

#include "util/memq.h"

#include "pdu.h"

#include "ll.h"
#include "lll.h"

#if defined(CONFIG_BT_LL_SW_SPLIT)
#include "util/util.h"

#include "lll_scan.h"
#include "ull_scan_types.h"
#include "ull_scan_internal.h"
#include "lll_adv.h"
#include "ull_adv_types.h"
#include "ull_adv_internal.h"
#include "lll_conn.h"
#include "ull_conn_types.h"
#include "ull_conn_internal.h"

u8_t ll_tx_pwr_lvl_get(u8_t handle_type,
		       u16_t handle, u8_t type, s8_t *tx_pwr_lvl)
{
	switch (handle_type) {
#if defined(CONFIG_BT_BROADCASTER) &&\
	defined(CONFIG_BT_CTLR_TX_PWR_DYNAMIC_CONTROL)
		case (BT_HCI_VS_LL_HANDLE_TYPE_ADV): {
			struct ll_adv_set *adv;

#if !defined(CONFIG_BT_CTLR_ADV_EXT)
			/* Ignore handle if AE not enabled */
			handle = 0;
#endif /* CONFIG_BT_CTLR_ADV_EXT */
			/* Allow the app to get Tx power
			 * when advertising is off
			 */
			adv = ull_adv_set_get(handle);
			if (!adv) {
				return BT_HCI_ERR_UNKNOWN_CONN_ID;
			}
			*tx_pwr_lvl = adv->lll.tx_pwr_lvl;
			break;
		}
#endif /* CONFIG_BT_BROADCASTER && CONFIG_BT_CTLR_TX_PWR_DYNAMIC_CONTROL */
#if defined(CONFIG_BT_OBSERVER) &&\
	defined(CONFIG_BT_CTLR_TX_PWR_DYNAMIC_CONTROL)
		case (BT_HCI_VS_LL_HANDLE_TYPE_SCAN): {
			struct ll_scan_set *scan;

			/* Ignore handle in case of scanner
			 * as for mesh extensions and scanning
			 * sets this control  is handled
			 * at a lower-level in the stack.
			 */
			handle = 0;
			/* Allow the app to get Tx power
			 * when scanning is off
			 */
			scan = ull_scan_set_get(handle);
			if (!scan) {
				return BT_HCI_ERR_UNKNOWN_CONN_ID;
			}
			*tx_pwr_lvl = scan->lll.tx_pwr_lvl;
			break;
		}
#endif /* CONFIG_BT_OBSERVER && CONFIG_BT_CTLR_TX_PWR_DYNAMIC_CONTROL*/
#if defined(CONFIG_BT_CONN)
		case (BT_HCI_VS_LL_HANDLE_TYPE_CONN): {
			struct ll_conn *conn;

			conn = ll_connected_get(handle);
			if (!conn) {
				return BT_HCI_ERR_UNKNOWN_CONN_ID;
			}

			if (type) {
#if defined(CONFIG_BT_CTLR_TX_PWR_DYNAMIC_CONTROL)
				/* Level desired is maximum available */
				*tx_pwr_lvl = lll_radio_tx_pwr_max_get();
#else  /* !CONFIG_BT_CTLR_TX_PWR_DYNAMIC_CONTROL */
				/* Return default if not multiple TXP */
				*tx_pwr_lvl = RADIO_TXP_DEFAULT;
#endif /* CONFIG_BT_CTLR_TX_PWR_DYNAMIC_CONTROL */
			} else {
#if defined(CONFIG_BT_CTLR_TX_PWR_DYNAMIC_CONTROL)
				/* Current level is requested */
				*tx_pwr_lvl = conn->lll.tx_pwr_lvl;
#else  /* !CONFIG_BT_CTLR_TX_PWR_DYNAMIC_CONTROL */
				/* Return default if not multiple TXP */
				*tx_pwr_lvl = RADIO_TXP_DEFAULT;
#endif /* CONFIG_BT_CTLR_TX_PWR_DYNAMIC_CONTROL */
			}
			break;
		}
#endif /* CONFIG_BT_CONN */
		default: {
			return BT_HCI_ERR_UNKNOWN_CMD;
		}
	}

	return BT_HCI_ERR_SUCCESS;
}


u8_t ll_tx_pwr_lvl_set(u8_t handle_type, u16_t handle,
		       s8_t *tx_pwr_lvl)
{
#if defined(CONFIG_BT_CTLR_TX_PWR_DYNAMIC_CONTROL)
	if (*tx_pwr_lvl == BT_HCI_VS_LL_TX_POWER_LEVEL_NO_PREF) {
		/* If no preference selected, then use default Tx power */
		*tx_pwr_lvl = RADIO_TXP_DEFAULT;
	}

	/**
	 * Check that desired Tx power matches the achievable transceiver
	 * Tx power capabilities by flooring - if selected power matches than
	 * is used, otherwise next smaller power available is used.
	 */
	*tx_pwr_lvl = lll_radio_tx_pwr_floor(*tx_pwr_lvl);
#endif /* CONFIG_BT_CTLR_TX_PWR_DYNAMIC_CONTROL */

	switch (handle_type) {
#if defined(CONFIG_BT_CTLR_TX_PWR_DYNAMIC_CONTROL)
#if defined(CONFIG_BT_BROADCASTER)
		case (BT_HCI_VS_LL_HANDLE_TYPE_ADV): {
			struct ll_adv_set *adv;

#if !defined(CONFIG_BT_CTLR_ADV_EXT)
			/* Ignore handle if AE not enabled */
			handle = 0;
#endif /* CONFIG_BT_CTLR_ADV_EXT */
			/* Allow the app to set Tx power
			 * prior to advertising
			 */
			adv = ull_adv_set_get(handle);
			if (!adv) {
				return BT_HCI_ERR_UNKNOWN_CONN_ID;
			}
			adv->lll.tx_pwr_lvl = *tx_pwr_lvl;
			break;
		}
#endif /* CONFIG_BT_BROADCASTER */
#if defined(CONFIG_BT_OBSERVER)
		case (BT_HCI_VS_LL_HANDLE_TYPE_SCAN): {
			struct ll_scan_set *scan;

			/* Ignore handle in case of scanner
			 * as for mesh extensions and scanning
			 * sets this control is handled
			 * at a lower-level in the stack.
			 */
			handle = 0;
			/* Allow the app to set Tx power
			 * prior to scanning
			 */
			scan = ull_scan_set_get(handle);
			if (!scan) {
				return BT_HCI_ERR_UNKNOWN_CONN_ID;
			}
			scan->lll.tx_pwr_lvl = *tx_pwr_lvl;
			break;
		}
#endif /* CONFIG_BT_OBSERVER */
#if defined(CONFIG_BT_CONN)
		case (BT_HCI_VS_LL_HANDLE_TYPE_CONN): {
			struct ll_conn *conn;

			conn = ll_connected_get(handle);
			if (!conn) {
				return BT_HCI_ERR_UNKNOWN_CONN_ID;
			}
			conn->lll.tx_pwr_lvl = *tx_pwr_lvl;
			break;
		}
#endif /* CONFIG_BT_CONN */
#endif /* CONFIG_BT_CTLR_TX_PWR_DYNAMIC_CONTROL */
		default: {
			return BT_HCI_ERR_UNKNOWN_CMD;
		}
	}

	return BT_HCI_ERR_SUCCESS;
}
#endif /* CONFIG_BT_LL_SW_SPLIT */

void ll_tx_pwr_get(s8_t *min, s8_t *max)
{
#if defined(CONFIG_BT_CTLR_TX_PWR_DYNAMIC_CONTROL)
	*min = lll_radio_tx_pwr_min_get();
	*max = lll_radio_tx_pwr_max_get();
#else
	*min = RADIO_TXP_DEFAULT;
	*max = RADIO_TXP_DEFAULT;
#endif /* CONFIG_BT_CTLR_TX_PWR_DYNAMIC_CONTROL */
}
