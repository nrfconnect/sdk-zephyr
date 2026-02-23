/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @brief Header containing display scan specific declarations for the
 * Zephyr OS layer of the Wi-Fi driver.
 */

#ifndef __ZEPHYR_DISP_SCAN_H__
#define __ZEPHYR_DISP_SCAN_H__
#include <zephyr/device.h>
#include <zephyr/net/wifi_mgmt.h>

#include "osal_api.h"

#ifdef CONFIG_WIFI_NRF71
//#ifdef SCAN_DB_GDRAM

struct nrf_wifi_umac_event_new_scan_display_results {
        /** Header nrf_wifi_umac_hdr */
        struct nrf_wifi_umac_hdr umac_hdr;
        /** Number of scan results in the current event */
        unsigned char event_bss_count;
        /** Display scan results info umac_display_results */
        struct umac_display_results display_results[DISPLAY_BSS_TOHOST_PEREVNT];
} __NRF_WIFI_PKD;
#endif

int nrf_wifi_disp_scan_zep(const struct device *dev, struct wifi_scan_params *params,
			   scan_result_cb_t cb);

enum nrf_wifi_status nrf_wifi_disp_scan_res_get_zep(struct nrf_wifi_vif_ctx_zep *vif_ctx_zep);

void nrf_wifi_event_proc_disp_scan_res_zep(void *vif_ctx,
				struct nrf_wifi_umac_event_new_scan_display_results *scan_res,
				unsigned int event_len,
				bool is_last);

#ifdef CONFIG_WIFI_MGMT_RAW_SCAN_RESULTS
void nrf_wifi_rx_bcn_prb_resp_frm(void *vif_ctx,
				  void *frm,
				  unsigned short frequency,
				  signed short signal);
#endif /* CONFIG_WIFI_MGMT_RAW_SCAN_RESULTS */
#endif /* __ZEPHYR_DISP_SCAN_H__ */
