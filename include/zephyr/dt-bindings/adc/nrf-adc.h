/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2022 Nordic Semiconductor ASA
 */

#ifndef ZEPHYR_INCLUDE_DT_BINDINGS_ADC_NRF_ADC_H_
#define ZEPHYR_INCLUDE_DT_BINDINGS_ADC_NRF_ADC_H_

#include <zephyr/dt-bindings/dt-util.h>

#define NRF_ADC_AIN0 BIT(0)
#define NRF_ADC_AIN1 BIT(1)
#define NRF_ADC_AIN2 BIT(2)
#define NRF_ADC_AIN3 BIT(3)
#define NRF_ADC_AIN4 BIT(4)
#define NRF_ADC_AIN5 BIT(5)
#define NRF_ADC_AIN6 BIT(6)
#define NRF_ADC_AIN7 BIT(7)

#define NRF_SAADC_AIN0     1
#define NRF_SAADC_AIN1     2
#define NRF_SAADC_AIN2     3
#define NRF_SAADC_AIN3     4
#define NRF_SAADC_AIN4     5
#define NRF_SAADC_AIN5     6
#define NRF_SAADC_AIN6     7
#define NRF_SAADC_AIN7     8
#define NRF_SAADC_VDD      9
#define NRF_SAADC_VDDHDIV5 13

#endif /* ZEPHYR_INCLUDE_DT_BINDINGS_ADC_NRF_ADC_H_ */
