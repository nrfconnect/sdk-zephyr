/*
 * Copyright (c) 2025 Nordic Semiconductor ASA.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef IRONSIDE_SE_UICR_H_
#define IRONSIDE_SE_UICR_H_

#include <stdint.h>

#include <nrfx.h>

#ifdef __cplusplus
extern "C" {
#endif

/** UICR structure defined by IronSide SE. */
#define IRONSIDE_SE_UICR ((struct UICR *)NRF_APPLICATION_UICR_NS_BASE)

/* UICR_VERSION: Version of the UICR format */

/* List of supported versions */
/* UICR version 2.0 */
#define UICR_VERSION_2_0 0x00020000UL
/* UICR version 2.1 */
#define UICR_VERSION_2_1 0x00020001UL
/* Maximum UICR version supported by this header. */
#define UICR_VERSION_MAX UICR_VERSION_2_1

/* Common enum values */
/* Default erased value for all UICR fields */
#define UICR_MAGIC_ERASE_VALUE (0xBD2328A8UL)
/* Common disabled value */
#define UICR_DISABLED          UICR_MAGIC_ERASE_VALUE
/* Common enabled value.
 * Note that any value other than UICR_DISABLED is interpreted as UICR_ENABLED.
 */
#define UICR_ENABLED           (0xFFFFFFFFUL)
/* Common unprotected value */
#define UICR_UNPROTECTED       UICR_MAGIC_ERASE_VALUE
/* Common protected value
 * Note that any value other than UICR_UNPROTECTED is interpreted as UICR_PROTECTED.
 */
#define UICR_PROTECTED         UICR_ENABLED

/* Common values for enumeration choices. */
#define UICR_ENUM_CHOICE_0 UICR_MAGIC_ERASE_VALUE
#define UICR_ENUM_CHOICE_1 (0x1730C77FUL)

/**
 * @brief Access port protection.
 */
struct UICR_APPROTECT {
	/* APPLICATION access port protection */
	volatile uint32_t APPLICATION;
	/* RADIOCORE access port protection */
	volatile uint32_t RADIOCORE;
	volatile const uint32_t RESERVED;
	/* CoreSight access port protection */
	volatile uint32_t CORESIGHT;
};

/* APPROTECT APPLICATION enum values */
#define UICR_APPROTECT_APPLICATION_UNPROTECTED UICR_MAGIC_ERASE_VALUE
#define UICR_APPROTECT_APPLICATION_PROTECTED   UICR_PROTECTED

/* APPROTECT RADIOCORE enum values */
#define UICR_APPROTECT_RADIOCORE_UNPROTECTED UICR_MAGIC_ERASE_VALUE
#define UICR_APPROTECT_RADIOCORE_PROTECTED   UICR_PROTECTED

/* APPROTECT CORESIGHT enum values */
#define UICR_APPROTECT_CORESIGHT_UNPROTECTED UICR_MAGIC_ERASE_VALUE
#define UICR_APPROTECT_CORESIGHT_PROTECTED   UICR_PROTECTED

/**
 * @brief Protected Memory region configuration
 */
struct UICR_PROTECTEDMEM {
	/* Enable the Protected Memory region */
	volatile uint32_t ENABLE;
	/* Protected Memory region size in 4 kiB blocks. */
	volatile uint32_t SIZE4KB;
};

/**
 * @brief Start a local watchdog timer ahead of the CPU boot.
 */
struct UICR_WDTSTART {
	/* Enable watchdog timer start. */
	volatile uint32_t ENABLE;
	/* Watchdog timer instance. */
	volatile uint32_t INSTANCE;
	/* Initial CRV (Counter Reload Value) register value. */
	volatile uint32_t CRV;
};

/* WDTSTART enum values */
/* Start WDT0 in the domain of the processor being booted. */
#define UICR_WDTSTART_INSTANCE_WDT0 UICR_ENUM_CHOICE_0
/* Start WDT1 in the domain of the processor being booted. */
#define UICR_WDTSTART_INSTANCE_WDT1 UICR_ENUM_CHOICE_1

/* WDTSTART CRV validation */
/* Minimum CRV value. */
#define UICR_WDTSTART_CRV_CRV_MIN (0xFUL)
/* Maximum CRV value. */
#define UICR_WDTSTART_CRV_CRV_MAX (0xFFFFFFFFUL)

/**
 * @brief Secure Storage partition sizes
 */
struct UICR_SECURESTORAGE_SIZES {
	/* Size of the APPLICATION partition in 1 kiB blocks */
	volatile uint32_t APPLICATIONSIZE1KB;
	/* Size of the RADIOCORE partition in 1 kiB blocks */
	volatile uint32_t RADIOCORESIZE1KB;
};

/**
 * @brief Secure Storage configuration
 */
struct UICR_SECURESTORAGE {
	/* Enable the Secure Storage */
	volatile uint32_t ENABLE;
	/* Start address of the Secure Storage region */
	volatile uint32_t ADDRESS;
	/* Secure Storage partitions for the Cryptographic service */
	volatile struct UICR_SECURESTORAGE_SIZES CRYPTO;
	/* Secure Storage partitions for the Internal Trusted Storage service */
	volatile struct UICR_SECURESTORAGE_SIZES ITS;
};

/**
 * @brief Global domain peripheral configuration
 */
struct UICR_PERIPHCONF {
	/* Enable the global domain peripheral configuration */
	volatile uint32_t ENABLE;
	/* Start address of the array of peripheral configuration entries*/
	volatile uint32_t ADDRESS;
	/* Maximum number of peripheral configuration entries */
	volatile uint32_t MAXCOUNT;
};

/**
 * @brief Global domain MPC configuration
 */
struct UICR_MPCCONF {
	/* Enable the global domain MPC configuration */
	volatile uint32_t ENABLE;
	/* Start address of the array of MPC configuration entries*/
	volatile uint32_t ADDRESS;
	/* Maximum number of MPC configuration entries */
	volatile uint32_t MAXCOUNT;
};

/**
 * @brief Automatic triggers for reset into secondary firmware
 */
struct UICR_SECONDARY_TRIGGER {
	/* Enable automatic triggers for reset into secondary firmware*/
	volatile uint32_t ENABLE;
	/* Reset reasons that trigger automatic reset into secondary firmware*/
	volatile uint32_t RESETREAS;
	volatile const uint32_t RESERVED;
};

/* SECONDARY TRIGGER RESETREAS field access macros */
#define UICR_SECONDARY_TRIGGER_RESETREAS_APPLICATIONWDT0_Pos (0UL)
#define UICR_SECONDARY_TRIGGER_RESETREAS_APPLICATIONWDT0_Msk                                       \
	(0x1UL << UICR_SECONDARY_TRIGGER_RESETREAS_APPLICATIONWDT0_Pos)
#define UICR_SECONDARY_TRIGGER_RESETREAS_APPLICATIONWDT1_Pos (1UL)
#define UICR_SECONDARY_TRIGGER_RESETREAS_APPLICATIONWDT1_Msk                                       \
	(0x1UL << UICR_SECONDARY_TRIGGER_RESETREAS_APPLICATIONWDT1_Pos)
#define UICR_SECONDARY_TRIGGER_RESETREAS_APPLICATIONLOCKUP_Pos (3UL)
#define UICR_SECONDARY_TRIGGER_RESETREAS_APPLICATIONLOCKUP_Msk                                     \
	(0x1UL << UICR_SECONDARY_TRIGGER_RESETREAS_APPLICATIONLOCKUP_Pos)
#define UICR_SECONDARY_TRIGGER_RESETREAS_RADIOCOREWDT0_Pos (5UL)
#define UICR_SECONDARY_TRIGGER_RESETREAS_RADIOCOREWDT0_Msk                                         \
	(0x1UL << UICR_SECONDARY_TRIGGER_RESETREAS_RADIOCOREWDT0_Pos)
#define UICR_SECONDARY_TRIGGER_RESETREAS_RADIOCOREWDT1_Pos (6UL)
#define UICR_SECONDARY_TRIGGER_RESETREAS_RADIOCOREWDT1_Msk                                         \
	(0x1UL << UICR_SECONDARY_TRIGGER_RESETREAS_RADIOCOREWDT1_Pos)
#define UICR_SECONDARY_TRIGGER_RESETREAS_RADIOCORELOCKUP_Pos (8UL)
#define UICR_SECONDARY_TRIGGER_RESETREAS_RADIOCORELOCKUP_Msk                                       \
	(0x1UL << UICR_SECONDARY_TRIGGER_RESETREAS_RADIOCORELOCKUP_Pos)

/**
 * @brief Secondary firmware configuration
 */
struct UICR_SECONDARY {
	/* Enable booting of secondary firmware. */
	volatile uint32_t ENABLE;
	/* Processor to boot for the secondary firmware. */
	volatile uint32_t PROCESSOR;
	/* Automatic triggers for reset into secondary firmware */
	volatile struct UICR_SECONDARY_TRIGGER TRIGGER;
	/* Start address of the secondary firmware.
	 * This value is used as the initial value of the secure VTOR
	 * (Vector Table Offset Register) after CPU reset.
	 */
	volatile uint32_t ADDRESS;
	/* Protected Memory region for the secondary firmware.*/
	volatile struct UICR_PROTECTEDMEM PROTECTEDMEM;
	/* Start a local watchdog timer ahead of the CPU boot.   */
	volatile struct UICR_WDTSTART WDTSTART;
	/* Global domain peripheral configuration used when booting the secondary firmware.*/
	volatile struct UICR_PERIPHCONF PERIPHCONF;
	/* Global domain MPC configuration used when booting the secondary firmware*/
	volatile struct UICR_MPCCONF MPCCONF;
};

/* SECONDARY enum values */
/* Boot the application core. */
#define UICR_SECONDARY_PROCESSOR_APPLICATION UICR_ENUM_CHOICE_0
/* Boot the radio core. */
#define UICR_SECONDARY_PROCESSOR_RADIOCORE   UICR_ENUM_CHOICE_1

/* SECONDARY field access macros */
#define UICR_SECONDARY_ADDRESS_ADDRESS_Msk (0xFFFFF000UL)

/* LOCK enum values */
/* NVR page 0 can be written, and is not integrity checked by Nordic IronSide SE*/
#define UICR_LOCK_PALL_UNLOCKED UICR_MAGIC_ERASE_VALUE
/* NVR page 0 is read-only, and is integrity checked by Nordic IronSide SE on boot*/
#define UICR_LOCK_PALL_LOCKED   (0xFFFFFFFFUL)

/* ERASEPROTECT enum values */
/* Erase protection disabled */
#define UICR_ERASEPROTECT_PALL_UNPROTECTED UICR_MAGIC_ERASE_VALUE
/* Erase protection enabled */
#define UICR_ERASEPROTECT_PALL_PROTECTED   UICR_PROTECTED

/* POLICY_PERIPHCONFSTAGE enum values */
/* PERIPHCONF API stage is set to initialization stage at application boot. */
#define UICR_POLICY_PERIPHCONFSTAGE_INIT   UICR_ENUM_CHOICE_0
/* PERIPHCONF API stage is set to normal stage at application boot. */
#define UICR_POLICY_PERIPHCONFSTAGE_NORMAL UICR_ENUM_CHOICE_1

/**
 * @brief User information configuration region.
 *
 * Any fields named "RESERVED" or similar are reserved for future extensions
 * by the IronSide SE and should not be used for other data.
 */
struct UICR {
	/* Version of the UICR format */
	volatile uint32_t VERSION;
	volatile const uint32_t RESERVED;
	/* Lock the UICR from modification */
	volatile uint32_t LOCK;
	volatile const uint32_t RESERVED1;
	/* AP protection */
	volatile struct UICR_APPROTECT APPROTECT;
	/* ERASEALL protection */
	volatile uint32_t ERASEPROTECT;
	/* Protected memory region */
	volatile struct UICR_PROTECTEDMEM PROTECTEDMEM;
	/* Start a local watchdog timer ahead of the CPU boot. */
	volatile struct UICR_WDTSTART WDTSTART;
	volatile const uint32_t RESERVED2;
	/* Secure Storage configuration */
	volatile struct UICR_SECURESTORAGE SECURESTORAGE;
	volatile const uint32_t RESERVED3[5];
	/* Global domain peripheral configuration */
	volatile struct UICR_PERIPHCONF PERIPHCONF;
	/* Global domain MPC configuration */
	volatile struct UICR_MPCCONF MPCCONF;
	/* Secondary firmware configuration */
	volatile struct UICR_SECONDARY SECONDARY;
	volatile const uint32_t RESERVED4[78];
	/* PERIPHCONF API stage at application boot. */
	volatile uint32_t POLICY_PERIPHCONFSTAGE;
#if !defined(UICR_DEF_OMIT_CUSTOMER)
	/* Reserved for customer. */
	volatile uint32_t CUSTOMER[320];
	volatile const uint32_t RESERVED5[44];
#endif
};

#ifdef __cplusplus
}
#endif
#endif /* IRONSIDE_SE_UICR_H_ */
