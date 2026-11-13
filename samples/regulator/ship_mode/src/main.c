/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/drivers/regulator.h>
#include <zephyr/logging/log.h>
#include <zephyr/kernel.h>

LOG_MODULE_REGISTER(ship_mode, LOG_LEVEL_INF);

static const struct device *regulator_parent = DEVICE_DT_GET(DT_ALIAS(regulator_parent));

int main(void)
{
	int ret;

	if (!device_is_ready(regulator_parent)) {
		LOG_ERR_DEVICE_NOT_READY(regulator_parent);
		return -ENODEV;
	}

	LOG_INF("%s entering ship mode", regulator_parent->name);

	/* Sleep added to drain LOG messages. */
	k_msleep(1000);

	ret = regulator_parent_ship_mode(regulator_parent);
	if (ret < 0) {
		LOG_ERR("%s failed to enter ship mode (ret = %i)", regulator_parent->name, ret);
		return ret;
	}

	/* Should never get here. */
	return 0;
}
