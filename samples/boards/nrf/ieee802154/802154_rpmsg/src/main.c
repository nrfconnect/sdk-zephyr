/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <inttypes.h>
#include <kernel.h>
#include <stdbool.h>
#include <sys/__assert.h>
#include <zephyr.h>

#include <nrf_802154_serialization_error.h>

void nrf_802154_serialization_error(const nrf_802154_ser_err_data_t *err)
{
	(void)err;
	__ASSERT(false, "802.15.4 serialization error");
}

void nrf_802154_sl_fault_handler(uint32_t module_id, int32_t line, const char *p_error)
{
	if (p_error == NULL) {
		p_error = "error unknown";
	}
	
	printk("nrf_802154_sl: ASSERTION FAILED: Module %" PRIu32 ":%" PRId32 " expr: '%s'",
		module_id, line, p_error);

	k_panic();
	__disable_irq();

	while (true) {
	}
}
