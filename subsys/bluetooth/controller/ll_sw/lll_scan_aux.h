/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define AUX_OFFSET_IS_VALID(_offset_us, _win_size_us, _pdu_us) \
		(((_offset_us) + (_win_size_us)) >= ((_pdu_us) + (EVENT_MAFS_US)))

int lll_scan_aux_init(void);
int lll_scan_aux_reset(void);
void lll_scan_aux_prepare(void *param);

extern uint8_t ull_scan_aux_lll_handle_get(struct lll_scan_aux *lll);
extern void *ull_scan_aux_lll_parent_get(struct lll_scan_aux *lll,
					 uint8_t *is_lll_scan);
