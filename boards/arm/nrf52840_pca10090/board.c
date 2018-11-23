/*
 * Copyright (c) 2018 Nordic Semiconductor ASA.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gpio.h>
#include <init.h>
#include <logging/log.h>
#include <zephyr.h>

LOG_MODULE_REGISTER(board_control_pca10090);

/* GPIOs on Port 0 */
#define INTERFACE0_U5 13 /* MCU interface pins 0 - 2 */
#define INTERFACE1_U6 24 /* MCU interface pins 3 - 5 */
#define BUTTON1_U12 6
#define BUTTON2_U12 26
#define SWITCH2_U9 8

/* GPIOs on Port 1 */
#define INTERFACE2_U21 10 /* COEX interface pins 6 - 8 */
#define UART0_VCOM_U14 14 /* Route nRF9160 UART0 to VCOM0 */
#define UART1_VCOM_U7 12  /* Route nRF9160 UART1 to VCOM2 */
#define LED1_U8 5
#define LED2_U8 7
#define LED3_U11 1
#define LED4_U11 3
#define SWITCH1_U9 9

/* MCU interface pins
 *
 * | nRF9160 |				 | nRF52840 |
 * | P0.17   | -- MCU Interface Pin 0 -- | P0.17    |
 * | P0.18   | -- MCU Interface Pin 1 -- | P0.20    |
 * | P0.19   | -- MCU Interface Pin 2 -- | P0.15    |
 * | P0.21   | -- MCU Interface Pin 3 -- | P0.22    |
 * | P0.22   | -- MCU Interface Pin 4 -- | P1.04    |
 * | P0.23   | -- MCU Interface Pin 5 -- | P1.02    |
 *
 * 	The rest are COEX pins.
 */

static const u32_t pins_on_p0[][2] = {
	/* default: Arduino headers */
	{ INTERFACE0_U5, IS_ENABLED(CONFIG_BOARD_PCA10090_INTERFACE_0_MCU) },
	{ INTERFACE1_U6, IS_ENABLED(CONFIG_BOARD_PCA10090_INTERFACE_1_MCU) },
	/* default: physical button */
	{ BUTTON1_U12, IS_ENABLED(CONFIG_BOARD_PCA10090_BUTTON_ARDUINO) },
	{ BUTTON2_U12, IS_ENABLED(CONFIG_BOARD_PCA10090_BUTTON_ARDUINO) },
	/* default: physical switch */
	{ SWITCH2_U9, IS_ENABLED(CONFIG_BOARD_PCA10090_SWITCH_ARDUINO) },
};

static const u32_t pins_on_p1[][2] = {
	/* default: COEX header */
	{ INTERFACE2_U21, IS_ENABLED(CONFIG_BOARD_PCA10090_INTERFACE_2_MCU) },
	/* default: VCOM0 */
	{ UART0_VCOM_U14, IS_ENABLED(CONFIG_BOARD_PCA10090_UART0_ARDUINO) },
	/* default: Arduino headers */
	{ UART1_VCOM_U7, IS_ENABLED(CONFIG_BOARD_PCA10090_UART1_VCOM) },
	/* default: physical LED */
	{ LED1_U8, IS_ENABLED(CONFIG_BOARD_PCA10090_LED_ARDUINO) },
	{ LED2_U8, IS_ENABLED(CONFIG_BOARD_PCA10090_LED_ARDUINO) },
	{ LED3_U11, IS_ENABLED(CONFIG_BOARD_PCA10090_LED_ARDUINO) },
	{ LED4_U11, IS_ENABLED(CONFIG_BOARD_PCA10090_LED_ARDUINO) },
	/* default: physical switch */
	{ SWITCH1_U9, IS_ENABLED(CONFIG_BOARD_PCA10090_SWITCH_ARDUINO) },
};

static int init(struct device *dev)
{
	int err = 0;
	struct device *p0;
	struct device *p1;

	LOG_DBG("Configuring..");

	p0 = device_get_binding(DT_GPIO_P0_DEV_NAME);
	__ASSERT(p0, "Unable to find GPIO %s", DT_GPIO_P0_DEV_NAME);

	p1 = device_get_binding(DT_GPIO_P1_DEV_NAME);
	__ASSERT(p1, "Unable to find GPIO %s", DT_GPIO_P1_DEV_NAME);

	for (size_t i = 0; i < ARRAY_SIZE(pins_on_p0); i++) {
		err = gpio_pin_configure(p0, pins_on_p0[i][0], GPIO_DIR_OUT);
		__ASSERT(err == 0, "Unable to configure pin %d",
			 pins_on_p0[i][0]);

		err = gpio_pin_write(p0, pins_on_p0[i][0], pins_on_p0[i][1]);
		__ASSERT(err == 0, "Unable to set pin %d on port 0 to %d",
			 pins_on_p0[i][0]);
	}

	for (size_t i = 0; i < ARRAY_SIZE(pins_on_p1); i++) {
		err = gpio_pin_configure(p1, pins_on_p1[i][0], GPIO_DIR_OUT);
		__ASSERT(err == 0, "Unable to configure pin %d",
			 pins_on_p1[i][0]);

		err = gpio_pin_write(p1, pins_on_p1[i][0], pins_on_p1[i][1]);
		__ASSERT(err == 0, "Unable to set pin %d on port 1 to %d",
			 pins_on_p1[i][0]);
	}

	/* Interface pins 0-2 */
	LOG_INF("Routing interface pins 0-2 to %s (U5 -> %d)",
		IS_ENABLED(CONFIG_BOARD_PCA10090_INTERFACE_0_MCU) ?
			"nRF52840" :
			"Arduino headers",
		IS_ENABLED(CONFIG_BOARD_PCA10090_INTERFACE_0_MCU));

	/* Interface pins 3-5 */
	LOG_INF("Routing interface pins 3-5 to %s (U6 -> %d)",
		IS_ENABLED(CONFIG_BOARD_PCA10090_INTERFACE_1_MCU) ?
			"nRF52840" :
			"TRACE header",
		IS_ENABLED(CONFIG_BOARD_PCA10090_INTERFACE_1_MCU));

	/* Interface pins 6-8 */
	LOG_INF("Routing interface pins 6-8 to %s (U21 -> %d)",
		IS_ENABLED(CONFIG_BOARD_PCA10090_INTERFACE_2_MCU) ?
			"nRF52840" :
			"COEX header",
		IS_ENABLED(CONFIG_BOARD_PCA10090_INTERFACE_2_MCU));

	LOG_INF("Routing nRF9160 UART0 to %s (U14 -> %d)",
		IS_ENABLED(CONFIG_BOARD_PCA10090_UART0_ARDUINO) ?
			"Arduino pin headers" :
			"VCOM0",
		IS_ENABLED(CONFIG_BOARD_PCA10090_UART0_ARDUINO));

	LOG_INF("Routing nRF9160 UART1 to %s (U7 -> %d)",
		IS_ENABLED(CONFIG_BOARD_PCA10090_UART1_ARDUINO) ?
			"Arduino pin headers" :
			"VCOM2",
		/* defaults to arduino pins */
		IS_ENABLED(CONFIG_BOARD_PCA10090_UART1_VCOM));

	LOG_INF("Routing nRF9160 LEDs to %s (U8, U11 -> %d)",
		IS_ENABLED(CONFIG_BOARD_PCA10090_LED_ARDUINO) ?
			"Arduino pin headers" :
			"physical LEDs",
		IS_ENABLED(CONFIG_BOARD_PCA10090_LED_ARDUINO));

	LOG_INF("Routing nRF9160 buttons to %s (U12 -> %d)",
		IS_ENABLED(CONFIG_BOARD_PCA10090_BUTTON_ARDUINO) ?
			"Arduino pin headers" :
			"physical buttons",
		IS_ENABLED(CONFIG_BOARD_PCA10090_BUTTON_ARDUINO));

	LOG_INF("Routing nRF9160 switches to %s (U9 -> %d)",
		IS_ENABLED(CONFIG_BOARD_PCA10090_SWITCH_ARDUINO) ?
			"Arduino pin headers" :
			"physical switches",
		IS_ENABLED(CONFIG_BOARD_PCA10090_SWITCH_ARDUINO));

	return 0;
}

SYS_INIT(init, POST_KERNEL, 0);
