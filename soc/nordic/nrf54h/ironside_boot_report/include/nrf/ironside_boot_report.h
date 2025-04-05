/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_SOC_NORDIC_NRF54H_IRONSIDE_BOOT_REPORT_INCLUDE_NRF_IRONSIDE_BOOT_REPORT_H_
#define ZEPHYR_SOC_NORDIC_NRF54H_IRONSIDE_BOOT_REPORT_INCLUDE_NRF_IRONSIDE_BOOT_REPORT_H_

#include <stdint.h>
#include <stddef.h>


/** @brief UICR error description contained in the boot report. */
struct ironside_boot_report_uicr_error {
	/** The type of error. A value of 0 indicates no error */
	uint32_t error_type;
	/** Error descrpitions specific to each type of UICR error */
	union {
		/** MPCCONF error */
		struct {
			/** The MPC error type */
			uint32_t err;
			/** Address to the MPC instance that triggered the error */
			uint32_t mpc_instance_address;
			/** The index of the MPCCONF entry that triggered the error */
			int32_t index;
		} mpcconf;
		/** PERIPHCONF error */
		struct {
			/** The SPU error type */
			uint32_t err;
			/** Address to the SPU instance that triggered the error */
			uint32_t spu_instance_address;
			/** Address to the peripheral index of the SPU that triggered the error */
			uint32_t peripheral;
			/** The index of the PERIPHCONF entry that triggered the error */
			int32_t index;
		} periphconf;
		/** ITS error */
		struct {
			uint32_t err;
		} its;
	} description;
};

/** @brief IRONside boot report. */
struct ironside_boot_report {
	/** Magic value used to identify valid boot report */
	uint32_t magic;
	/** Firmware version of IRONside SE. 8bit MAJOR.MINOR.PATCH.TWEAK */
	uint32_t ironside_version_int;
	/** Human readable extraversion of IRONside SE */
	char ironside_extraversion[12];
	/** Firmware version of IRONside SE recovery firmware. 8bit MAJOR.MINOR.PATCH.TWEAK */
	uint32_t ironside_recovery_version_int;
	/** Human readable extraversion of IRONside SE */
	char ironside_recovery_extraversion[12];
	/** Copy of SICR.UROT.UPDATE.STATUS.*/
	uint32_t ironside_update_status;
	/** See @ref ironside_boot_report_uicr_error */
	struct ironside_boot_report_uicr_error uicr_error_description;
	/** Data passed from booting local domain to local domain being booted */
	uint8_t local_domain_context[32];
	/** CSPRNG data */
	uint8_t random_data[32];
	/** Reserved for Future Use */
	uint32_t rfu[64];
};

/**
 * @brief Get a pointer to the IRONside boot report.
 *
 * @param[out] report Will be set to point to the IRONside boot report.
 *
 * @return non-negative value if success, negative value otherwise.
 * @retval -EFAULT if the magic field in the report is incorrect.
 * @retval -EINVAL if @ref report is NULL.
 */
int ironside_boot_report_get(const struct ironside_boot_report **report);

#endif /* ZEPHYR_SOC_NORDIC_NRF54H_IRONSIDE_BOOT_REPORT_INCLUDE_NRF_IRONSIDE_BOOT_REPORT_H_ */
