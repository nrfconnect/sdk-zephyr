/*
 * Copyright 2024, NXP
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _SOC__H_
#define _SOC__H_

#include <zephyr/sys/util.h>

#ifndef _ASMLANGUAGE

#include <fsl_common.h>

/* Add include for DTS generated information */
#include <zephyr/devicetree.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef CONFIG_MEMC_MCUX_FLEXSPI
uint32_t flexspi_clock_set_freq(uint32_t clock_name, uint32_t rate);
#endif

#ifdef __cplusplus
}
#endif

#endif /* !_ASMLANGUAGE */

#endif /* _SOC__H_ */
