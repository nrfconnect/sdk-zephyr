/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef IRONSIDE_SE_CTRLAP_H_
#define IRONSIDE_SE_CTRLAP_H_

#ifdef __cplusplus
extern "C" {
#endif

/* Boot command opcodes used in the BOOTMODE.OPCODE field. */

/* Command "DEBUGWAIT" - start the application CPU with CPUCONF.CPUWAIT = 1. */
#define IRONSIDE_SE_BOOTMODE_OPCODE_DEBUGWAIT 0x2

/* Command "ERASEALL" - eraseall except SDFW and SCFW */
#define IRONSIDE_SE_BOOTMODE_OPCODE_ERASEALL 0x1

/* Boot command error codes used in the BOOTSTATUS.CMDERROR field.
 * Each command defines what the numbers in the available range signify.
 */

/** The command executed successfully (shared by all commands). */
#define IRONSIDE_SE_CMDERROR_SUCCESS 0x0

/** The ERASEALL command was prevented by UICR ERASEPROTECT being enabled
 *  or by the integrity check failing on a locked UICR.
 */
#define IRONSIDE_SE_CMDERROR_ERASEALL_PROTECTED 0x1

/** Value reserved for conditions that should never happen (shared by all commands). */
#define IRONSIDE_SE_CMDERROR_UNEXPECTED 0x7

/* Boot error codes used in the BOOTSTATUS.BOOTERROR field. */

/** The boot had no errors. */
#define IRONSIDE_SE_BOOT_ERROR_SUCCESS                                      0x0
/** The reset vector for the application firmware was not programmed. */
#define IRONSIDE_SE_BOOT_ERROR_NO_APPLICATION_FIRMWARE                      0x1
/** The IronSide SE was unable to parse the SysCtrl ROM report. */
#define IRONSIDE_SE_BOOT_ERROR_ROM_REPORT_INVALID                           0x2
/** The SysCtrl ROM booted the system in current limited mode due to an issue in the BICR. */
#define IRONSIDE_SE_BOOT_ERROR_ROM_REPORT_CURRENT_LIMITED                   0x3
/** The IronSide SE detected an issue with the HFXO configuration in the BICR. */
#define IRONSIDE_SE_BOOT_ERROR_BICR_HFXO_INVALID                            0x4
/** The IronSide SE detected an issue with the LFXO configuration in the BICR. */
#define IRONSIDE_SE_BOOT_ERROR_BICR_LFXO_INVALID                            0x5
/** The IronSide SE failed to boot the SysCtrl Firmware. */
#define IRONSIDE_SE_BOOT_ERROR_SYSCTRL_START_FAILED                         0x6
/** The UICR integrity check failed. */
#define IRONSIDE_SE_BOOT_ERROR_UICR_INTEGRITY_FAILED                        0x7
/** The UICR content is not valid */
#define IRONSIDE_SE_BOOT_ERROR_UICR_CONTENT_INVALID                         0x8
/** Integrity check of PROTECTEDMEM failed. */
#define IRONSIDE_SE_BOOT_ERROR_UICR_PROTECTEDMEM_INTEGRITY_FAILED           0x9
/** Failed to configure system based on UICR. */
#define IRONSIDE_SE_BOOT_ERROR_UICR_CONFIG_FAILED                           0xA
/** The IronSide SE failed to mount its own storage. */
#define IRONSIDE_SE_BOOT_ERROR_SECDOM_STORAGE_MOUNT_FAILED                  0xB
/** Failed to initialize DVFS service */
#define IRONSIDE_SE_BOOT_ERROR_DVFS_INIT_FAILED                             0xC
/** Failed to boot secondary application firmware; configuration missing from UICR. */
#define IRONSIDE_SE_BOOT_ERROR_NO_SECONDARY_APPLICATION_FIRMWARE            0xD
/** Integrity check of secondary PROTECTEDMEM failed. */
#define IRONSIDE_SE_BOOT_ERROR_UICR_SECONDARY_PROTECTEDMEM_INTEGRITY_FAILED 0xE
/** Unsupported UICR format version. */
#define IRONSIDE_SE_BOOT_ERROR_UICR_FORMAT_UNSUPPORTED                      0xF
/** Failed to initialize counter service */
#define IRONSIDE_SE_BOOT_ERROR_COUNTER_SERVICE_INIT_FAILED                  0x10
/** Value reserved for conditions that should never happen. */
#define IRONSIDE_SE_BOOT_ERROR_UNEXPECTED                                   0xFF

#ifdef __cplusplus
}
#endif
#endif /* IRONSIDE_SE_CTRLAP_H_ */
