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

	z_nrf_grtc_wakeup_prepare(1000);

	if (0) {
		/*nrf_memconf_ramblock_ret_mask_enable_set(NRF_MEMCONF, 0, RAMBLOCK_RET_MASK, false);*/
		/*nrf_memconf_ramblock_ret_mask_enable_set(NRF_MEMCONF, 1, RAMBLOCK_RET_MASK, false);*/
		k_sleep(K_FOREVER);
	} else {
		/*nrf_memconf_ramblock_ret_mask_enable_set(NRF_MEMCONF, 0, RAMBLOCK_RET_MASK, false);*/
		/*nrf_memconf_ramblock_ret_mask_enable_set(NRF_MEMCONF, 1, RAMBLOCK_RET_MASK, false);*/

		sys_poweroff();
	}

	return 0;
}
