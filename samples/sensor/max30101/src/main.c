/*
 * Copyright (c) 2017, NXP
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <sensor.h>
#include <stdio.h>

void main(void)
{
	struct sensor_value green;
	struct device *dev = device_get_binding(DT_MAX30101_NAME);

	if (dev == NULL) {
		printf("Could not get max30101 device\n");
		return;
	}

	while (1) {
		sensor_sample_fetch(dev);
		sensor_channel_get(dev, SENSOR_CHAN_GREEN, &green);

		/* Print green LED data*/
		printf("GREEN=%d\n", green.val1);

		k_sleep(20);
	}
}
