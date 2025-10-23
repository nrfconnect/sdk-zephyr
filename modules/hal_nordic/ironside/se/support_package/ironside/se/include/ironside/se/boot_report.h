/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef IRONSIDE_SE_BOOT_REPORT_H_
#define IRONSIDE_SE_BOOT_REPORT_H_

#include <stdint.h>
#include <stddef.h>

#include <ironside/se/ctrlap.h>
#include <ironside/se/memory_map.h>
#include <ironside/se/uicr.h>
#include <ironside/se/versions.h>

#include <nrfx.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(IRONSIDE_SE_BOOT_REPORT_ADDRESS)
/** Boot report for the current processor. */
#define IRONSIDE_SE_BOOT_REPORT ((struct ironside_se_boot_report *)IRONSIDE_SE_BOOT_REPORT_ADDRESS)
#endif

/** Constant used to check if an Nordic IronSide SE boot report has been written. */
#define IRONSIDE_SE_BOOT_REPORT_MAGIC (0x4d69546fUL)

/** UICR had no errors. */
#define IRONSIDE_SE_UICR_SUCCESS          0
/** There was an unexpected error processing the UICR. */
#define IRONSIDE_SE_UICR_ERROR_UNEXPECTED 1
/** The UICR integrity check failed. */
#define IRONSIDE_SE_UICR_ERROR_INTEGRITY  2
/** The UICR content check failed. */
#define IRONSIDE_SE_UICR_ERROR_CONTENT    3
/** Failed to configure system based on UICR. */
#define IRONSIDE_SE_UICR_ERROR_CONFIG     4
/** Unsupported UICR format version. */
#define IRONSIDE_SE_UICR_ERROR_FORMAT     5

/** Error found in UICR.PROTECTEDMEM. */
#define IRONSIDE_SE_UICR_REGID_PROTECTEDMEM           offsetof(struct UICR, PROTECTEDMEM)
/** Error found in UICR.SECURESTORAGE. */
#define IRONSIDE_SE_UICR_REGID_SECURESTORAGE          offsetof(struct UICR, SECURESTORAGE)
/** Error found in UICR.PERIPHCONF. */
#define IRONSIDE_SE_UICR_REGID_PERIPHCONF             offsetof(struct UICR, PERIPHCONF)
/** Error found in UICR.MPCCONF. */
#define IRONSIDE_SE_UICR_REGID_MPCCONF                offsetof(struct UICR, MPCCONF)
/** Error found in UICR.SECONDARY.ADDRESS/SIZE4KB */
#define IRONSIDE_SE_UICR_REGID_SECONDARY              offsetof(struct UICR, SECONDARY)
/** Error found in UICR.SECONDARY.PROTECTEDMEM. */
#define IRONSIDE_SE_UICR_REGID_SECONDARY_PROTECTEDMEM offsetof(struct UICR, SECONDARY.PROTECTEDMEM)
/** Error found in UICR.SECONDARY.PERIPHCONF. */
#define IRONSIDE_SE_UICR_REGID_SECONDARY_PERIPHCONF   offsetof(struct UICR, SECONDARY.PERIPHCONF)
/** Error found in UICR.SECONDARY.MPCCONF. */
#define IRONSIDE_SE_UICR_REGID_SECONDARY_MPCCONF      offsetof(struct UICR, SECONDARY.MPCCONF)

/** Failed to mount a CRYPTO secure storage partition in MRAM. */
#define IRONSIDE_SE_UICR_SECURESTORAGE_ERROR_MOUNT_CRYPTO_FAILED 1
/** Failed to mount an ITS secure storage partition in MRAM. */
#define IRONSIDE_SE_UICR_SECURESTORAGE_ERROR_MOUNT_ITS_FAILED    2
/** The start address and total size of all ITS partitions are not aligned to 4 KB. */
#define IRONSIDE_SE_UICR_SECURESTORAGE_ERROR_MISALIGNED          3

/** There was an unexpected error processing UICR.PERIPHCONF. */
#define IRONSIDE_SE_UICR_PERIPHCONF_ERROR_UNEXPECTED        1
/** The address contained in a UICR.PERIPHCONF array entry is not permitted. */
#define IRONSIDE_SE_UICR_PERIPHCONF_ERROR_NOT_PERMITTED     2
/** The readback of the value for a UICR.PERIPHCONF array entry did not match. */
#define IRONSIDE_SE_UICR_PERIPHCONF_ERROR_READBACK_MISMATCH 3

/** Booted in secondary mode. */
#define IRONSIDE_SE_BOOT_MODE_FLAGS_SECONDARY_MASK 0x1

/** Booted normally by IronSide SE.*/
#define IRONSIDE_SE_BOOT_REASON_DEFAULT                 0
/** Booted because of a cpuconf service call by a different core. */
#define IRONSIDE_SE_BOOT_REASON_CPUCONF_CALL            1
/** Booted in secondary mode because of a bootmode service call. */
#define IRONSIDE_SE_BOOT_REASON_BOOTMODE_SECONDARY_CALL 2
/** Booted in secondary mode because of a boot error in the primary mode. */
#define IRONSIDE_SE_BOOT_REASON_BOOTERROR               3
/** Booted in secondary mode because of local domain reset reason trigger. */
#define IRONSIDE_SE_BOOT_REASON_TRIGGER_RESETREAS       4
/** Booted in secondary mode via the CTRL-AP. */
#define IRONSIDE_SE_BOOT_REASON_CTRLAP_SECONDARYMODE    5

/** Index for RESETREAS.DOMAIN[NRF_DOMAIN_APPLICATION]. */
#define IRONSIDE_SE_SECONDARY_RESETREAS_APPLICATION 0
/** Index for RESETREAS.DOMAIN[NRF_DOMAIN_RADIOCORE]. */
#define IRONSIDE_SE_SECONDARY_RESETREAS_RADIOCORE   1

/** Length of the local domain context buffer in bytes. */
#define IRONSIDE_SE_BOOT_REPORT_LOCAL_DOMAIN_CONTEXT_SIZE (16UL)
/** Length of the random data buffer in bytes. */
#define IRONSIDE_SE_BOOT_REPORT_RANDOM_DATA_SIZE          (32UL)
/** Length of the UUID buffer in bytes. */
#define IRONSIDE_SE_BOOT_REPORT_UUID_SIZE                 (16UL)

/** @brief Initialization/boot status description contained in the boot report. */
struct ironside_se_boot_report_init_status {
	/** Reserved for Future Use. */
	uint8_t rfu1[3];
	/** Boot error for the current boot (same as reported in BOOTSTATUS)*/
	uint8_t boot_error;
	/** Overall UICR status. */
	uint8_t uicr_status;
	/** Reserved for Future Use. */
	uint8_t rfu2;
	/** ID of the register that caused the error. Only relevant for
	 *  IRONSIDE_SE_UICR_ERROR_CONTENT and IRONSIDE_SE_UICR_ERROR_CONFIG.
	 */
	uint16_t uicr_regid;
	/** Additional description for IRONSIDE_SE_UICR_ERROR_CONFIG. */
	union {
		/** UICR.SECURESTORAGE error description. */
		struct {
			/** Reason that UICR.SECURESTORAGE configuration failed. */
			uint16_t status;
			/** Owner ID of the failing secure storage partition. Only relevant for
			 *  IRONSIDE_SE_UICR_SECURESTORAGE_ERROR_MOUNT_CRYPTO_FAILED and
			 *  IRONSIDE_SE_UICR_SECURESTORAGE_ERROR_MOUNT_ITS_FAILED.
			 */
			uint16_t owner_id;
		} securestorage;
		/** UICR.PERIPHCONF error description. */
		struct {
			/** Reason that UICR.PERIPHCONF configuration failed. */
			uint16_t status;
			/** Index of the failing entry in the UICR.PERIPHCONF array. */
			uint16_t index;
		} periphconf;
	} uicr_detail;
};

/** @brief Initialization/boot context description contained in the boot report. */
struct ironside_se_boot_report_init_context {
	/** Reserved for Future Use */
	uint8_t rfu[3];
	/** Reason the processor was started. */
	uint8_t boot_reason;

	union {
		/** Data passed from booting local domain to local domain being booted.
		 *
		 * Valid if the boot reason is one of the following:
		 * - IRONSIDE_SE_BOOT_REASON_CPUCONF_CALL
		 * - IRONSIDE_SE_BOOT_REASON_BOOTMODE_SECONDARY_CALL
		 */
		uint8_t local_domain_context[IRONSIDE_SE_BOOT_REPORT_LOCAL_DOMAIN_CONTEXT_SIZE];

		/** Initialiation error that triggered the boot.
		 *
		 * Valid if the boot reason is IRONSIDE_SE_BOOT_REASON_BOOTERROR.
		 */
		struct ironside_se_boot_report_init_status trigger_init_status;

		/** RESETREAS.DOMAIN that triggered the boot.
		 *
		 * Valid if the boot reason is IRONSIDE_SE_BOOT_REASON_TRIGGER_RESETREAS.
		 */
		uint32_t trigger_resetreas[4];
	};
};

/** @brief Random bytes provided in the boot report. */
struct ironside_se_boot_report_random {
	uint8_t data[IRONSIDE_SE_BOOT_REPORT_RANDOM_DATA_SIZE];
};

/** @brief IronSide SE boot report. */
struct ironside_se_boot_report {
	/** Magic value used to identify valid boot report */
	uint32_t magic;
	/** Firmware version of IronSide SE. 8bit MAJOR.MINOR.PATCH.SEQNUM */
	uint32_t ironside_se_version_int;
	/** Human readable extraversion of IronSide SE */
	char ironside_se_extraversion[12];
	/** Firmware version of IronSide SE recovery firmware. 8bit MAJOR.MINOR.PATCH.SEQNUM */
	uint32_t ironside_se_recovery_version_int;
	/** Human readable extraversion of IronSide SE recovery firmware */
	char ironside_se_recovery_extraversion[12];
	/** Copy of SICR.UROT.UPDATE.STATUS.*/
	uint32_t ironside_update_status;
	/** Initialization/boot status. */
	struct ironside_se_boot_report_init_status init_status;
	/** Reserved for Future Use */
	uint16_t rfu1;
	/** Flags describing the current boot mode. */
	uint16_t boot_mode_flags;
	/** Data describing the context under which the CPU was booted. */
	struct ironside_se_boot_report_init_context init_context;
	/** CSPRNG data */
	struct ironside_se_boot_report_random random;
#if TARGET_IRONSIDE_SE_VERSION > IRONSIDE_SE_V23_0_2_17
	/** Device Info data : 128-bit Universally Unique IDentifier (UUID) */
	uint8_t device_info_uuid[IRONSIDE_SE_BOOT_REPORT_UUID_SIZE];
#else
	uint32_t unused1[4];
#endif
	/** Reserved for Future Use */
	uint32_t rfu2[60];
};

#ifdef __cplusplus
}
#endif
#endif /* IRONSIDE_SE_BOOT_REPORT_H_ */
