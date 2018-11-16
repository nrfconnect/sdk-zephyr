/*
 * Copyright (c) 2015-2016 Intel Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __TEST_GPIO_H__
#define __TEST_GPIO_H__

#include <zephyr.h>
#include <gpio.h>
#include <misc/util.h>
#include <ztest.h>

#if defined(CONFIG_BOARD_QUARK_SE_C1000_DEVBOARD)
#define DEV_NAME DT_GPIO_QMSI_0_NAME
#define PIN_OUT 15 /* GPIO15_I2S_RXD */
#define PIN_IN 16 /* GPIO16_I2S_RSCK */
#elif defined(CONFIG_BOARD_QUARK_SE_C1000_DEVBOARD_SS)
#define DEV_NAME DT_GPIO_QMSI_SS_0_NAME
#define PIN_OUT 4  /* GPIO_SS_AIN_12 */
#define PIN_IN 5  /* GPIO_SS_AIN_13 */
#elif defined(CONFIG_BOARD_ARDUINO_101)
#define DEV_NAME DT_GPIO_QMSI_0_NAME
#define PIN_OUT 16  /* IO8 */
#define PIN_IN 19  /* IO4 */
#elif defined(CONFIG_BOARD_ARDUINO_101_SSS)
#define DEV_NAME DT_GPIO_QMSI_SS_0_NAME
#define PIN_OUT 2  /* AD0 */
#define PIN_IN 3  /* AD1 */
#elif defined(CONFIG_BOARD_QUARK_D2000_CRB)
#define DEV_NAME DT_GPIO_QMSI_0_NAME
#define PIN_OUT 8  /* DIO7 */
#define PIN_IN 9  /* DIO8 */
#elif defined(CONFIG_BOARD_ESP32)
#define DEV_NAME CONFIG_GPIO_ESP32_0_NAME
#define PIN_OUT 4  /* DIO4 */
#define PIN_IN 2  /* DIO2 */
#elif defined(CONFIG_BOARD_ARDUINO_ZERO)
#define DEV_NAME DT_GPIO_SAM0_PORTA_LABEL
#define PIN_OUT 20  /* PA20 / pin 6 */
#define PIN_IN 15  /* PA15 / pin 5 */
#elif defined(CONFIG_BOARD_NRF52840_PCA10056)
#define DEV_NAME DT_GPIO_P1_DEV_NAME
#define PIN_OUT 1  /* P1.01 */
#define PIN_IN 2  /* P1.02 */
#elif defined(CONFIG_BOARD_NRF52_PCA10040)
#define DEV_NAME DT_GPIO_P0_DEV_NAME
#define PIN_OUT 11  /* P0.11 */
#define PIN_IN 12  /* P0.12 */
#elif defined(CONFIG_BOARD_NRF51_PCA10028)
#define DEV_NAME DT_GPIO_P0_DEV_NAME
#define PIN_OUT 12  /* P0.12 */
#define PIN_IN 13  /* P0.13 */
#else
#error Unsupported board
#endif

#define MAX_INT_CNT 3
struct drv_data {
	struct gpio_callback gpio_cb;
	int mode;
	int index;
};

void test_gpio_pin_read_write(void);
void test_gpio_callback_edge_high(void);
void test_gpio_callback_edge_low(void);
void test_gpio_callback_level_high(void);
void test_gpio_callback_level_low(void);
void test_gpio_callback_add_remove(void);
void test_gpio_callback_enable_disable(void);

#endif /* __TEST_GPIO_H__ */
