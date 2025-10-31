/* Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/pm/device.h>
#include <zephyr/pm/device_runtime.h>
#include <zephyr/pm/pm.h>
#include <zephyr/sys/poweroff.h>
#include <zephyr/drivers/timer/nrf_grtc_timer.h>

int main(void)
{

	if (IS_ENABLED(CONFIG_CONSOLE)) {
		printf("%s system off demo. Ready for system off.\n", CONFIG_BOARD_QUALIFIERS);
	}

	if (IS_ENABLED(CONFIG_PM_DEVICE) && IS_ENABLED(CONFIG_SERIAL)) {
		static const struct device *cons = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));
		int err;
		enum pm_device_state state;

		if (cons) {
			do {
				err = pm_device_state_get(cons, &state);
			} while ((err == 0) && (state == PM_DEVICE_STATE_ACTIVE));
		}
	}

	pm_state_force(0u, &(struct pm_state_info){PM_STATE_SOFT_OFF, 0, 0});
	k_sleep(K_FOREVER);

	return 0;
}
