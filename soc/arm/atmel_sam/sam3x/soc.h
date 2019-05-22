/*
 * Copyright (c) 2016 Intel Corporation.
 * Copyright (c) 2013-2015 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/** @file
 * @brief Register access macros for the Atmel SAM3X MCU.
 *
 * This file provides register access macros for the Atmel SAM3X MCU, HAL
 * drivers for core peripherals as well as symbols specific to Atmel SAM family.
 */

#ifndef _ATMEL_SAM3X_SOC_H_
#define _ATMEL_SAM3X_SOC_H_

#ifndef _ASMLANGUAGE

#define DONT_USE_CMSIS_INIT
#define DONT_USE_PREDEFINED_CORE_HANDLERS
#define DONT_USE_PREDEFINED_PERIPHERALS_HANDLERS

#if defined CONFIG_SOC_PART_NUMBER_SAM3X4C
#include <sam3x4c.h>
#elif defined CONFIG_SOC_PART_NUMBER_SAM3X4E
#include <sam3x4e.h>
#elif defined CONFIG_SOC_PART_NUMBER_SAM3X8C
#include <sam3x8c.h>
#elif defined CONFIG_SOC_PART_NUMBER_SAM3X8E
#include <sam3x8e.h>
#elif defined CONFIG_SOC_PART_NUMBER_SAM3X8H
#include <sam3x8h.h>
#else
#error Library does not support the specified device.
#endif

/* Add include for DTS generated information */
#include <generated_dts_board.h>

#define ID_UART0   ID_UART
#define UART0      UART

#include "soc_pinmap.h"

#include "../common/soc_pmc.h"
#include "../common/soc_gpio.h"

#endif /* _ASMLANGUAGE */

/** Processor Clock (HCLK) Frequency */
#define SOC_ATMEL_SAM_HCLK_FREQ_HZ CONFIG_SYS_CLOCK_HW_CYCLES_PER_SEC
/** Master Clock (MCK) Frequency */
#define SOC_ATMEL_SAM_MCK_FREQ_HZ SOC_ATMEL_SAM_HCLK_FREQ_HZ

#endif /* _ATMEL_SAM3X_SOC_H_ */
