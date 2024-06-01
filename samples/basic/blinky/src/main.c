/*
 * Copyright (c) 2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>

/* 1000 msec = 1 sec */
#define SLEEP_TIME_MS   1000

/* The devicetree node identifier for the "led0" alias. */
#define LED0_NODE DT_ALIAS(led0)
#define LED1_NODE DT_ALIAS(led1)
#define LED2_NODE DT_ALIAS(led2)

/*
 * Get button configuration from the devicetree sw0 alias. This is mandatory.
 */
/* #define SW0_NODE DT_ALIAS(button0)
#if !DT_NODE_HAS_STATUS(SW0_NODE, okay)
#error "Unsupported board: sw0 devicetree alias is not defined"
#endif */
/*
 * A build error on this line means your board is unsupported.
 * See the sample documentation for information on how to fix this.
 */

static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);
static const struct gpio_dt_spec led1= GPIO_DT_SPEC_GET(LED1_NODE, gpios);
static const struct gpio_dt_spec led2= GPIO_DT_SPEC_GET(LED2_NODE, gpios);

//static const struct gpio_dt_spec button = GPIO_DT_SPEC_GET_OR(SW0_NODE, gpios, {0});

int main(void)
{
	int ret;

	/* if (!gpio_is_ready_dt(&button)) {
		return 0;
	} */

	if (!gpio_is_ready_dt(&led)) {
		return 0;
	}
	
	if (!gpio_is_ready_dt(&led1)) {
		return 0;
	}

	if (!gpio_is_ready_dt(&led2)) {
		return 0;
	}

	ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);
	if (ret < 0) {
		return 0;
	}

	ret = gpio_pin_configure_dt(&led1, GPIO_OUTPUT_ACTIVE);
	if (ret < 0) {
		return 0;
	}

	ret = gpio_pin_configure_dt(&led2, GPIO_OUTPUT_ACTIVE);
	if (ret < 0) {
		return 0;
	}
	while (1) {
		ret = gpio_pin_toggle_dt(&led);
		if (ret < 0) {
			return 0;
		}
		k_msleep(SLEEP_TIME_MS);
		ret = gpio_pin_toggle_dt(&led);
		if (ret < 0) {
			return 0;
		}
		k_msleep(SLEEP_TIME_MS);

		ret = gpio_pin_toggle_dt(&led1);
		if (ret < 0) {
			return 0;
		}
		k_msleep(SLEEP_TIME_MS);

		ret = gpio_pin_toggle_dt(&led1);
		if (ret < 0) {
			return 0;
		}
		k_msleep(SLEEP_TIME_MS);

		ret = gpio_pin_toggle_dt(&led2);
		if (ret < 0) {
			return 0;
		}
		k_msleep(SLEEP_TIME_MS);

		ret = gpio_pin_toggle_dt(&led2);
		if (ret < 0) {
			return 0;
		}
		k_msleep(SLEEP_TIME_MS);
	}
	return 0;
}
