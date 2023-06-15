/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/regulator.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/sensor/npm1300_charger.h>
#include <zephyr/dt-bindings/regulator/npm1300.h>
#include <zephyr/sys/printk.h>
#include <getopt.h>

#define SLEEP_TIME_MS  100
#define UPDATE_TIME_MS 2000

static const struct gpio_dt_spec button1 = GPIO_DT_SPEC_GET(DT_ALIAS(sw0), gpios);

static const struct device *regulators = DEVICE_DT_GET(DT_NODELABEL(npm1300_ek_regulators));

static const struct device *charger = DEVICE_DT_GET(DT_NODELABEL(npm1300_ek_charger));

void configure_ui(void)
{
	int ret;

	if (!gpio_is_ready_dt(&button1)) {
		printk("Error: button device %s is not ready\n", button1.port->name);
		return;
	}

	ret = gpio_pin_configure_dt(&button1, GPIO_INPUT);
	if (ret != 0) {
		printk("Error %d: failed to configure %s pin %d\n", ret, button1.port->name,
		       button1.pin);
		return;
	}

	printk("Set up button at %s pin %d\n", button1.port->name, button1.pin);
}

void read_sensors(void)
{
	struct sensor_value volt;
	struct sensor_value current;
	struct sensor_value temp;
	struct sensor_value error;
	struct sensor_value status;

	sensor_sample_fetch(charger);
	sensor_channel_get(charger, SENSOR_CHAN_GAUGE_VOLTAGE, &volt);
	sensor_channel_get(charger, SENSOR_CHAN_GAUGE_AVG_CURRENT, &current);
	sensor_channel_get(charger, SENSOR_CHAN_GAUGE_TEMP, &temp);
	sensor_channel_get(charger, SENSOR_CHAN_NPM1300_CHARGER_STATUS, &status);
	sensor_channel_get(charger, SENSOR_CHAN_NPM1300_CHARGER_ERROR, &error);

	printk("V: %d.%03d ", volt.val1, volt.val2 / 1000);

	printk("I: %s%d.%04d ", ((current.val1 < 0) || (current.val2 < 0)) ? "-" : "",
	       abs(current.val1), abs(current.val2) / 100);

	printk("T: %d.%02d\n", temp.val1, temp.val2 / 10000);

	printk("Charger Status: %d, Error: %d\n", status.val1, error.val1);
}

int main(void)
{
	configure_ui();

	if (!device_is_ready(regulators)) {
		printk("Error: Regulator device is not ready\n");
		return 0;
	}

	if (!device_is_ready(charger)) {
		printk("Charger device not ready.\n");
		return 0;
	}

	while (1) {
		/* Cycle regulator control GPIOs when first button pressed */
		static bool last_button;
		static int dvs_state;
		bool button_state = gpio_pin_get_dt(&button1) == 1;

		if (button_state && !last_button) {
			dvs_state = (dvs_state + 1) % 4;
			regulator_parent_dvs_state_set(regulators, dvs_state);
		}

		/* Read and display charger status */
		static int count;

		if (++count > (UPDATE_TIME_MS / SLEEP_TIME_MS)) {
			read_sensors();
			count = 0;
		}

		last_button = button_state;
		k_msleep(SLEEP_TIME_MS);
	}
}
