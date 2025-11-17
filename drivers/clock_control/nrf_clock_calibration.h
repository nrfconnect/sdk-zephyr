/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef ZEPHYR_DRIVERS_CLOCK_CONTROL_NRF_CLOCK_CALIBRATION_H_
#define ZEPHYR_DRIVERS_CLOCK_CONTROL_NRF_CLOCK_CALIBRATION_H_

#include <zephyr/sys/onoff.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize LFCLK RC calibration.
 */
void z_nrf_clock_calibration_init(void);

/**
 * @brief Calibration done handler
 *
 * Must be called from clock event handler.
 */
void z_nrf_clock_calibration_done_handler(void);

/**
 * @brief Notify calibration module about LF clock start
 */
void z_nrf_clock_calibration_lfclk_started(void);

/**
 * @brief Notify calibration module about LF clock stop
 */
void z_nrf_clock_calibration_lfclk_stopped(void);


#ifdef __cplusplus
}
#endif

#endif /* ZEPHYR_DRIVERS_CLOCK_CONTROL_NRF_CLOCK_CALIBRATION_H_ */
