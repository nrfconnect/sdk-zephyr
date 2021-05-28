/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 *   This file implements the thermometer abstraction that uses Zephyr sensor
 *   API for the die termomether.
 *
 */

#include "platform/nrf_802154_temperature.h"

#include <device.h>
#include <drivers/sensor.h>
#include <kernel.h>
#include <stdbool.h>

/** @brief Value of the last temperature measurement. */
static int8_t m_temperature;

void nrf_802154_temperature_init(void)
{
	/* Intentionally empty. */
}

void nrf_802154_temperature_deinit(void)
{
	/* Intentionally empty. */
}

int8_t nrf_802154_temperature_get(void)
{
	return m_temperature;
}

/** @brief Thread handler for temperature update. */
static void temperature_update_thread(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	static struct sensor_value  temperature_value;
	static const struct device *temperature_dev;

	temperature_dev = DEVICE_DT_GET(DT_NODELABEL(temp));

	if (!device_is_ready(temperature_dev)) {
		__ASSERT_NO_MSG(false);
	}

	while (true) {
		sensor_sample_fetch(temperature_dev);
		sensor_channel_get(temperature_dev, SENSOR_CHAN_DIE_TEMP, &temperature_value);

		if (m_temperature != temperature_value.val1) {
			m_temperature = temperature_value.val1;

			nrf_802154_temperature_changed();
		}

		k_sleep(K_MSEC(CONFIG_NRF_802154_TEMPERATURE_UPDATE_PERIOD));
	}
}

K_THREAD_DEFINE(temperature_update_tid,
		CONFIG_NRF_802154_TEMPERATURE_UPDATE_STACK_SIZE,
		temperature_update_thread,
		NULL,
		NULL,
		NULL,
		CONFIG_NRF_802154_TEMPERATURE_UPDATE_PRIO,
		0,
		0);
