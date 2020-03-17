/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file SoC configuration macros for the
 *       Nordic Semiconductor nRF53 family processors.
 */

#ifndef _NORDICSEMI_NRF53_SOC_H_
#define _NORDICSEMI_NRF53_SOC_H_

#ifndef _ASMLANGUAGE

#include <nrfx.h>

/* Add include for DTS generated information */
#include <devicetree.h>

#endif /* !_ASMLANGUAGE */

#if defined(CONFIG_SOC_NRF5340_CPUAPP)
#define FLASH_PAGE_ERASE_MAX_TIME_US  89700UL
#define FLASH_PAGE_MAX_CNT  256UL
#elif defined(CONFIG_SOC_NRF5340_CPUNET)
#define FLASH_PAGE_ERASE_MAX_TIME_US  44850UL
#define FLASH_PAGE_MAX_CNT  128UL
#endif

#ifdef CONFIG_SOC_NRF5340_CPUAPP
bool nrf53_has_erratum19(void);
#endif

#endif /* _NORDICSEMI_NRF53_SOC_H_ */
