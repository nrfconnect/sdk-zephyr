/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef IRONSIDE_SE_API_H_
#define IRONSIDE_SE_API_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <nrfx.h>

#include <ironside/se/memory_map.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @name Update service error codes.
 * @{
 */

/** Caller does not have access to the provided update candidate buffer. */
#define IRONSIDE_SE_UPDATE_ERROR_NOT_PERMITTED     (1)
/** Failed to write the update metadata to SICR. */
#define IRONSIDE_SE_UPDATE_ERROR_SICR_WRITE_FAILED (2)
/** Update is placed outside of valid range */
#define IRONSIDE_SE_UPDATE_ERROR_INVALID_ADDRESS   (3)

/**
 * @}
 */

/** Length of the update manifest in bytes */
#define IRONSIDE_SE_UPDATE_MANIFEST_LENGTH  (256)
/** Length of the update public key in bytes. */
#define IRONSIDE_SE_UPDATE_PUBKEY_LENGTH    (32)
/** Length of the update signature in bytes. */
#define IRONSIDE_SE_UPDATE_SIGNATURE_LENGTH (64)

/** @brief IronSide SE update blob. */
struct ironside_se_update_blob {
	uint8_t manifest[IRONSIDE_SE_UPDATE_MANIFEST_LENGTH];
	uint8_t pubkey[IRONSIDE_SE_UPDATE_PUBKEY_LENGTH];
	uint8_t signature[IRONSIDE_SE_UPDATE_SIGNATURE_LENGTH];
	uint32_t firmware[];
};

/**
 * @brief Request a firmware upgrade of the IronSide SE.
 *
 * This invokes the IronSide SE update service. The device must be restarted for the update
 * to be installed. Check the update status in the application boot report to see if the update
 * was successfully installed.
 *
 * @param update Pointer to update blob
 *
 * @retval 0 on a successful request (although the update itself may still fail).
 * @retval -IRONSIDE_SE_UPDATE_ERROR_NOT_PERMITTED if missing access to the update candidate.
 * @retval -IRONSIDE_SE_UPDATE_ERROR_SICR_WRITE_FAILED if writing update parameters to SICR failed.
 * @returns Positive error status if reported by IronSide call (see error codes in @ref call.h).
 *
 */
int ironside_se_update(const struct ironside_se_update_blob *update);

/**
 * @name CPUCONF service error codes.
 * @{
 */

/** An invalid or unsupported processor ID was specified. */
#define IRONSIDE_SE_CPUCONF_ERROR_WRONG_CPU         (1)
/** The boot message is too large to fit in the buffer. */
#define IRONSIDE_SE_CPUCONF_ERROR_MESSAGE_TOO_LARGE (2)

/**
 * @}
 */

/* Maximum size of the CPUCONF message parameter. */
#define IRONSIDE_SE_CPUCONF_REQ_MSG_MAX_SIZE (4 * sizeof(uint32_t))

/**
 * @brief Boot a local domain CPU
 *
 * @param cpu The CPU to be booted
 * @param vector_table Pointer to the vector table used to boot the CPU.
 * @param cpu_wait When this is true, the CPU will WAIT even if the CPU has clock.
 * @param msg A message that can be placed in cpu's boot report.
 * @param msg_size Size of the message in bytes.
 *
 * @note cpu_wait is only intended to be enabled for debug purposes
 * and it is only supported that a debugger resumes the CPU.
 *
 * @note the call always sends IRONSIDE_SE_CPUCONF_REQ_MSG_MAX_SIZE message bytes.
 * If the given msg_size is less than that, the remaining bytes are set to zero.
 *
 * @retval 0 on success or if the CPU has already booted.
 * @retval -IRONSIDE_SE_CPUCONF_ERROR_WRONG_CPU if cpu is unrecognized.
 * @retval -IRONSIDE_SE_CPUCONF_ERROR_MESSAGE_TOO_LARGE if msg_size is greater than
 * IRONSIDE_SE_CPUCONF_REQ_MSG_MAX_SIZE.
 * @returns Positive error status if reported by IronSide call (see error codes in @ref call.h).
 */
int ironside_se_cpuconf(NRF_PROCESSORID_Type cpu, const void *vector_table, bool cpu_wait,
			const uint8_t *msg, size_t msg_size);

/** Invalid configuration enum. */
#define IRONSIDE_SE_TDD_ERROR_INVALID_CONFIG (1)

enum ironside_se_tdd_config {
	/** Turn off the TDD */
	IRONSIDE_SE_TDD_CONFIG_OFF = 1,
	/** Turn on the TDD with default configuration */
	IRONSIDE_SE_TDD_CONFIG_ON_DEFAULT = 2,
};

/**
 * @brief Control the Trace and Debug Domain (TDD).
 *
 * @param config The configuration to be applied.
 *
 * @retval 0 on success.
 * @retval -IRONSIDE_SE_SE_TDD_SERVICE_ERROR_INVALID_CONFIG if the configuration is invalid.
 * @returns Positive error status if reported by IronSide call (see error codes in @ref call.h).
 */
int ironside_se_tdd_configure(const enum ironside_se_tdd_config config);

/** Supported DVFS operational points. */
enum ironside_se_dvfs_oppoint {
	IRONSIDE_SE_DVFS_OPP_HIGH = 0,
	IRONSIDE_SE_DVFS_OPP_MEDLOW = 1,
	IRONSIDE_SE_DVFS_OPP_LOW = 2
};

/**
 * @brief Number of DVFS oppoints supported by IronSide.
 *
 * This is the number of different DVFS oppoints that can be set on IronSide.
 * The oppoints are defined in the `ironside_dvfs_oppoint` enum.
 */
#define IRONSIDE_SE_DVFS_OPPOINT_COUNT (3)

/**
 * @name IronSide DVFS service error codes.
 * @{
 */

/** The requested DVFS oppoint is not allowed. */
#define IRONSIDE_SE_DVFS_ERROR_WRONG_OPPOINT    (1)
/** Failed to change the DVFS oppoint due to other ongoing operations. */
#define IRONSIDE_SE_DVFS_ERROR_BUSY             (2)
/** Currently unused. */
#define IRONSIDE_SE_DVFS_ERROR_OPPOINT_DATA     (3)
/** The caller does not have permission to change the DVFS oppoint. */
#define IRONSIDE_SE_DVFS_ERROR_PERMISSION       (4)
/** The requested DVFS oppoint is already set, no change needed. */
#define IRONSIDE_SE_DVFS_ERROR_NO_CHANGE_NEEDED (5)
/** The operation timed out, possibly due to a hardware issue. */
#define IRONSIDE_SE_DVFS_ERROR_TIMEOUT          (6)

/**
 * @}
 */

/**
 * @brief Change the current DVFS oppoint.
 *
 * Requests a change of the current DVFS oppoint to the specified value.
 * It will block until the change is applied.
 *
 * @param dvfs_oppoint The new DVFS oppoint to set.
 *
 * @retval 0 on success.
 * @retval -IRONSIDE_SE_DVFS_ERROR_WRONG_OPPOINT if the requested DVFS oppoint is not allowed.
 * @retval -IRONSIDE_SE_DVFS_ERROR_BUSY if hardware is busy.
 * @retval -IRONSIDE_SE_DVFS_ERROR_PERMISSION if the caller does not have permission to change the
 * DVFS oppoint.
 * @retval -IRONSIDE_SE_DVFS_ERROR_NO_CHANGE_NEEDED if the requested DVFS oppoint is already set.
 * @retval -IRONSIDE_SE_DVFS_ERROR_TIMEOUT if the operation timed out, possibly due to a hardware
 * issue.
 * @returns Positive error status if reported by IronSide call (see error codes in @ref call.h).
 */
int ironside_se_dvfs_req_oppoint(enum ironside_se_dvfs_oppoint dvfs_oppoint);

/**
 * @brief Check if the given oppoint is valid.
 *
 * @param dvfs_oppoint The oppoint to check.
 * @return true if the oppoint is valid, false otherwise.
 */
static inline bool ironside_se_dvfs_is_oppoint_valid(enum ironside_se_dvfs_oppoint dvfs_oppoint)
{
	if (dvfs_oppoint != IRONSIDE_SE_DVFS_OPP_HIGH &&
	    dvfs_oppoint != IRONSIDE_SE_DVFS_OPP_MEDLOW &&
	    dvfs_oppoint != IRONSIDE_SE_DVFS_OPP_LOW) {
		return false;
	}

	return true;
}

/**
 * @name Boot mode service error codes.
 * @{
 */

/** Invalid/unsupported boot mode transition. */
#define IRONSIDE_SE_BOOTMODE_ERROR_UNSUPPORTED_MODE  (1)
/** Failed to reboot into the boot mode due to other activity preventing a reset. */
#define IRONSIDE_SE_BOOTMODE_ERROR_BUSY              (2)
/** The boot message is too large to fit in the buffer. */
#define IRONSIDE_SE_BOOTMODE_ERROR_MESSAGE_TOO_LARGE (3)

/**
 * @}
 */

/* Maximum size of the message parameter. */
#define IRONSIDE_SE_BOOTMODE_REQ_MSG_MAX_SIZE (4 * sizeof(uint32_t))

/**
 * @brief Request a reboot into the secondary firmware boot mode.
 *
 * This invokes the IronSide boot mode service to restart the system into the secondary boot
 * mode. In this mode, the secondary configuration defined in UICR is applied instead of the
 * primary one. The system immediately reboots without a reply if the request succeeds.
 *
 * The given message data is passed to the boot report of the CPU booted in the secondary boot mode.
 *
 * @note This function does not return if the request is successful.
 * @note The device will boot into the secondary firmware instead of primary firmware.
 * @note The request does not fail if the secondary firmware is not defined.
 *
 * @param msg A message that can be placed in the cpu's boot report.
 * @param msg_size Size of the message in bytes.
 *
 * @retval 0 on success.
 * @retval -IRONSIDE_SE_BOOTMODE_ERROR_UNSUPPORTED_MODE if the secondary boot mode is unsupported.
 * @retval -IRONSIDE_SE_BOOTMODE_ERROR_BUSY if the reboot was blocked.
 * @retval -IRONSIDE_SE_BOOTMODE_ERROR_MESSAGE_TOO_LARGE if msg_size is greater than
 * IRONSIDE_SE_BOOTMODE_REQ_MSG_MAX_SIZE.
 * @returns Positive error status if reported by IronSide call (see error codes in @ref call.h).
 */
int ironside_se_bootmode_secondary_reboot(const uint8_t *msg, size_t msg_size);

/**
 * @name Counter service error codes.
 * @{
 */

/** Counter value is lower than current value (monotonic violation). */
#define IRONSIDE_SE_COUNTER_ERROR_TOO_LOW         (1)
/** Invalid counter ID. */
#define IRONSIDE_SE_COUNTER_ERROR_INVALID_ID      (2)
/** Counter is locked and cannot be modified. */
#define IRONSIDE_SE_COUNTER_ERROR_LOCKED          (3)
/** Invalid parameter. */
#define IRONSIDE_SE_COUNTER_ERROR_INVALID_PARAM   (4)
/** Storage operation failed. */
#define IRONSIDE_SE_COUNTER_ERROR_STORAGE_FAILURE (5)

/**
 * @}
 */

/** Maximum value for a counter */
#define IRONSIDE_SE_COUNTER_MAX_VALUE UINT32_MAX

/** Number of counters */
#define IRONSIDE_SE_COUNTER_NUM 4

/**
 * @brief Counter identifiers.
 */
enum ironside_se_counter {
	IRONSIDE_SE_COUNTER_0 = 0,
	IRONSIDE_SE_COUNTER_1,
	IRONSIDE_SE_COUNTER_2,
	IRONSIDE_SE_COUNTER_3
};

/**
 * @brief Set a counter value.
 *
 * This sets the specified counter to the given value. The counter is monotonic,
 * so the new value must be greater than or equal to the current value.
 * If the counter is locked, the operation will fail.
 *
 * @note Counters are automatically initialized to 0 during the first boot in LCS ROT.
 *       The monotonic constraint applies to all subsequent writes.
 *
 * @param counter_id Counter identifier.
 * @param value New counter value.
 *
 * @retval 0 on success.
 * @retval -IRONSIDE_SE_COUNTER_ERROR_INVALID_ID if counter_id is invalid.
 * @retval -IRONSIDE_SE_COUNTER_ERROR_TOO_LOW if value is lower than current value.
 * @retval -IRONSIDE_SE_COUNTER_ERROR_LOCKED if counter is locked.
 * @retval -IRONSIDE_SE_COUNTER_ERROR_STORAGE_FAILURE if storage operation failed.
 * @returns Positive error status if reported by IronSide call (see error codes in @ref call.h).
 */
int ironside_se_counter_set(enum ironside_se_counter counter_id, uint32_t value);

/**
 * @brief Get a counter value.
 *
 * This retrieves the current value of the specified counter.
 *
 * @note Counters are automatically initialized to 0 during the first boot in LCS ROT,
 *       so this function will always succeed for valid counter IDs.
 *
 * @param counter_id Counter identifier.
 * @param value Pointer to store the counter value.
 *
 * @retval 0 on success.
 * @retval -IRONSIDE_SE_COUNTER_ERROR_INVALID_ID if counter_id is invalid.
 * @retval -IRONSIDE_SE_COUNTER_ERROR_INVALID_PARAM if value is NULL.
 * @retval -IRONSIDE_SE_COUNTER_ERROR_STORAGE_FAILURE if storage operation failed.
 * @returns Positive error status if reported by IronSide call (see error codes in @ref call.h).
 */
int ironside_se_counter_get(enum ironside_se_counter counter_id, uint32_t *value);

/**
 * @brief Lock a counter for the current boot.
 *
 * This locks the specified counter, preventing any further modifications until the next reboot.
 * The lock state is not persistent and will be cleared on reboot.
 *
 * @note The intended use case is for a bootloader to lock a counter before transferring control
 *       to the next boot stage, preventing that image from modifying the counter value.
 *
 * @param counter_id Counter identifier.
 *
 * @retval 0 on success.
 * @retval -IRONSIDE_SE_COUNTER_ERROR_INVALID_ID if counter_id is invalid.
 * @returns Positive error status if reported by IronSide call (see error codes in @ref call.h).
 */
int ironside_se_counter_lock(enum ironside_se_counter counter_id);

#ifdef __cplusplus
}
#endif
#endif /* IRONSIDE_SE_API_H_ */
