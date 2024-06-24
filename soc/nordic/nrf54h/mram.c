/*
 * Copyright (c) 2024 Nordic Semiconductor ASA.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/toolchain.h>

#include <services/nrfs_mram.h>
#include <nrfs_backend_ipc_service.h>

LOG_MODULE_REGISTER(mram, CONFIG_SOC_LOG_LEVEL);

static void mram_latency_handler(nrfs_mram_latency_evt_t const *p_evt, void *context)
{
	ARG_UNUSED(context);

	switch (p_evt->type) {
	case NRFS_MRAM_LATENCY_REQ_APPLIED:
		LOG_DBG("MRAM latency not allowed setting applied");
		break;
	case NRFS_MRAM_LATENCY_REQ_REJECTED:
		LOG_ERR("MRAM latency not allowed setting rejected");
		k_panic();
		break;
	default:
		LOG_WRN("Unexpected event: %d", p_evt->type);
	}
}

/* Turn off mram automatic suspend as it causes delays in time depended code sections. */
static int turn_off_suspend_mram(void)
{
	nrfs_err_t err;

	/* Wait for ipc initialization */
	nrfs_backend_wait_for_connection(K_FOREVER);

	err = nrfs_mram_init(mram_latency_handler);
	if (err != NRFS_SUCCESS) {
		LOG_ERR("MRAM service init failed: %d", err);
		return -EIO;
	}

	LOG_DBG("MRAM service initialized, disallow latency");

	err = nrfs_mram_set_latency(MRAM_LATENCY_NOT_ALLOWED, NULL);
	if (err != NRFS_SUCCESS) {
		LOG_ERR("MRAM: set latency failed (%d)", err);
		return -EIO;
	}

	return 0;
}

SYS_INIT(turn_off_suspend_mram, APPLICATION, 90);
