/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_INCLUDE_ZEPHYR_DRIVERS_FIRMWARE_NRF_IRONSIDE_RECOVER_H_
#define ZEPHYR_INCLUDE_ZEPHYR_DRIVERS_FIRMWARE_NRF_IRONSIDE_RECOVER_H_

#include <stdint.h>
#include <stddef.h>

/* IronSide call identifiers with implicit versions.
 *
 * With the initial "version 0", the service ABI is allowed to break until the
 * first production release of IronSide SE.
 */
#define IRONSIDE_CALL_ID_RECOVER_SERVICE_V0 5

/* Index of the return code within the service buffer. */
#define IRONSIDE_RECOVER_SERVICE_RETCODE_IDX (0)

/**
 * @brief Request a reboot into recovery mode.
 *
 * This invokes the IronSide SE recovery service to restart the system into recovery mode.
 * The system will immediately begin the reboot process after this call completes successfully.
 * Recovery mode allows for system recovery operations such as firmware repair or
 * factory reset procedures.
 *
 * @note This function will not return if the reboot is successful.
 * @note The device will boot into recovery firmware instead of normal application firmware.
 *
 * @returns Positive non-0 error status if reported by IronSide call.
 */
int ironside_reboot_into_recovery(void);

#endif /* ZEPHYR_INCLUDE_ZEPHYR_DRIVERS_FIRMWARE_NRF_IRONSIDE_RECOVER_H_ */
