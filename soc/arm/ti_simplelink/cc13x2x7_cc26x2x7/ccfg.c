/*
 * Copyright (c) 2022 Vaishnav Achath
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifdef CONFIG_CC13X2_CC26X2_BOOST_MODE
#define CCFG_FORCE_VDDR_HH 1
#endif

#ifdef CONFIG_CC13X2_CC26X2_BOOTLOADER_ENABLE
#define SET_CCFG_BL_CONFIG_BOOTLOADER_ENABLE 0xC5
#else
#define SET_CCFG_BL_CONFIG_BOOTLOADER_ENABLE 0x00
#endif /* CONFIG_CC13X2_CC26X2_BOOTLOADER_ENABLE */

#ifdef CONFIG_CC13X2_CC26X2_BOOTLOADER_BACKDOOR_ENABLE
#define SET_CCFG_BL_CONFIG_BL_ENABLE 0xC5
#define SET_CCFG_BL_CONFIG_BL_PIN_NUMBER CONFIG_CC13X2_CC26X2_BOOTLOADER_BACKDOOR_PIN
#define SET_CCFG_BL_CONFIG_BL_LEVEL CONFIG_CC13X2_CC26X2_BOOTLOADER_BACKDOOR_LEVEL
#else
#define SET_CCFG_BL_CONFIG_BL_ENABLE 0x00
#endif /* CONFIG_CC13X2_CC26X2_BOOTLOADER_BACKDOOR_ENABLE */

/* TI recommends setting CCFG values and then including the TI provided ccfg.c */
#include <startup_files/ccfg.c>
