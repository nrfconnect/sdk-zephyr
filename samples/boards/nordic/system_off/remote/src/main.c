/* Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/pm/device.h>
#include <zephyr/pm/device_runtime.h>
#include <zephyr/sys/poweroff.h>
#include <hal/nrf_memconf.h>
#include <zephyr/drivers/timer/nrf_grtc_timer.h>

int main(void)
{

	if (IS_ENABLED(CONFIG_CONSOLE)) {
		printf("%s system off demo. Ready for system off.\n", CONFIG_BOARD);
	}

	sys_poweroff();

	return 0;
}
