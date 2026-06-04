/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/device.h>
#include <zephyr/kernel.h>

#if DT_NODE_HAS_STATUS(DT_NODELABEL(spi00), okay)

static const struct device *const spi_bus = DEVICE_DT_GET(DT_NODELABEL(spi00));

int main(void)
{
	if (!device_is_ready(spi_bus)) {
		return -ENODEV;
	}

	return 0;
}

#else
#error "This test requires an enabled spi00 (nordic,nrf-spim) devicetree node"
#endif
