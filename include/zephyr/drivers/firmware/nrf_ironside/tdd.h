/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_INCLUDE_ZEPHYR_DRIVERS_FIRMWARE_NRF_IRONSIDE_TDD_H_
#define ZEPHYR_INCLUDE_ZEPHYR_DRIVERS_FIRMWARE_NRF_IRONSIDE_TDD_H_

#include <zephyr/drivers/firmware/nrf_ironside/call.h>

#include <nrfx.h>

#define IRONSIDE_TDD_ERROR_EINVAL (22) /* Invalid argument */


#define IRONSIDE_CALL_ID_TDD_V0 3

#define IRONSIDE_TDD_SERVICE_REQ_CONFIG_IDX 0
#define IRONSIDE_TDD_SERVICE_RSP_RETCODE_IDX 0

enum ironside_tdd_config {
	/** Don't touch the configuration */
	IRONSIDE_TDD_CONFIG_NO_CHANGE = 0, /* This allows future extensions to the TDD service */
	/** Turn off the TDD */
	IRONSIDE_TDD_CONFIG_OFF = 1, 
	/** Turn on the TDD with default configuration */
	IRONSIDE_TDD_CONFIG_ON_DEFAULT = 2,
	/** Turn on the TDD with default configuration, but skip TPIU pin configuration */
	IRONSIDE_TDD_CONFIG_ON_DEFAULT_NO_PINS = 3,
};

/**
 * @brief Control the Trace and Debug Domain (TDD).
 *
 * @param config The configuration to be applied.
 *
 * @retval 0 on success.
 * @retval -IRONSIDE_TDD_ERROR_EINVAL on invalid argument.
 */
int ironside_tdd(const enum ironside_tdd_config config);

#endif /* ZEPHYR_INCLUDE_ZEPHYR_DRIVERS_FIRMWARE_NRF_IRONSIDE_TDD_H_ */
