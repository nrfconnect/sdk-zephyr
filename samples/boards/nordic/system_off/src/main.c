/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "retained.h"

#include <inttypes.h>
#include <stdio.h>

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/hwinfo.h>
#include <zephyr/drivers/comparator.h>
#include <zephyr/kernel.h>
#include <zephyr/pm/device.h>
#include <zephyr/sys/poweroff.h>
#include <zephyr/sys/util.h>

#define NON_WAKEUP_RESET_REASON (RESET_PIN | RESET_SOFTWARE | RESET_POR | RESET_DEBUG)
#include <zephyr/pm/device_runtime.h>
#include <hal/nrf_gpio.h>
#include <hal/nrf_memconf.h>

#if defined(CONFIG_GRTC_WAKEUP_ENABLE)
#include <zephyr/drivers/timer/nrf_grtc_timer.h>
#define DEEP_SLEEP_TIME_S 2
#endif
#if defined(CONFIG_GPIO_WAKEUP_ENABLE)
static const struct gpio_dt_spec sw0 = GPIO_DT_SPEC_GET(DT_ALIAS(sw0), gpios);
#endif
#if defined(CONFIG_LPCOMP_WAKEUP_ENABLE)
static const struct device *comp_dev = DEVICE_DT_GET(DT_NODELABEL(comp));
#endif

static const struct gpio_dt_spec sw1 = GPIO_DT_SPEC_GET(DT_ALIAS(sw1), gpios);
static const uint32_t port_sw1 = DT_PROP(DT_GPIO_CTLR_BY_IDX(DT_ALIAS(sw1), gpios, 0), port);

int print_reset_cause(uint32_t reset_cause)
{
	int32_t ret;
	uint32_t supported;

	ret = hwinfo_get_supported_reset_cause((uint32_t *)&supported);

	if (ret || !(reset_cause & supported)) {
		return -ENOTSUP;
	}

	if (reset_cause & RESET_DEBUG) {
		printf("Reset by debugger.\n");
	} else if (reset_cause & RESET_CLOCK) {
		printf("Wakeup from System OFF by GRTC.\n");
	} else if (reset_cause & RESET_LOW_POWER_WAKE) {
		printf("Wakeup from System OFF by GPIO.\n");
	} else  {
		printf("Other wake up cause 0x%08X.\n", reset_cause);
	}

	return 0;
}

int main(void)
{
	int rc;
	uint32_t reset_cause;
	const struct device *const cons = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));
	uint32_t nrf_pin_sw1 = 32 * port_sw1 + sw1.pin;
	bool do_poweroff = true;

	if (!device_is_ready(cons)) {
		printf("%s: device not ready.\n", cons->name);
		return 0;
	}

	/* TODO: this is always set and locks entering power off after gpio wakeup */
	// if (nrf_gpio_pin_latch_get(nrf_pin_sw1)) {
	// 	nrf_gpio_pin_latch_clear(nrf_pin_sw1);
	// 	do_poweroff = false;
	// }

	printf("\n%s system off demo\n", CONFIG_BOARD);
	hwinfo_get_reset_cause(&reset_cause);
	rc = print_reset_cause(reset_cause);

#if defined(CONFIG_SOC_NRF54H20_CPUAPP)
/* Temporary set gpio default if sense is set, prevent 300uA additional current after wakeup */
	for (int i = 0; i < 12; i++) {
		if (nrf_gpio_pin_sense_get(i) != GPIO_PIN_CNF_SENSE_Disabled) {
			nrf_gpio_cfg_default(i);
		}
	}
#endif

	if (rc < 0) {
		printf("Reset cause not supported.\n");
		return 0;
	}

	if (IS_ENABLED(CONFIG_APP_USE_RETAINED_MEM)) {
		bool retained_ok = retained_validate();

		if (reset_cause & NON_WAKEUP_RESET_REASON) {
			retained.boots = 0;
			retained.off_count = 0;
			retained.uptime_sum = 0;
			retained.uptime_latest = 0;
			retained_ok = true;
		}
		/* Increment for this boot attempt and update. */
		retained.boots += 1;
		retained_update();

		printf("Retained data: %s\n", retained_ok ? "valid" : "INVALID");
		printf("Boot count: %u\n", retained.boots);
		printf("Off count: %u\n", retained.off_count);
		printf("Active Ticks: %" PRIu64 "\n", retained.uptime_sum);
	} else {
		printf("Retained data not supported\n");
	}


	k_sleep(K_MSEC(4000));

#if defined(CONFIG_GRTC_WAKEUP_ENABLE)
	int err = z_nrf_grtc_wakeup_prepare(DEEP_SLEEP_TIME_S * USEC_PER_SEC);

	if (err < 0) {
		printf("Unable to prepare GRTC as a wake up source (err = %d).\n", err);
	} else {
		printf("Entering system off; wait %u seconds to restart\n", DEEP_SLEEP_TIME_S);
	}
#endif
#if defined(CONFIG_GPIO_WAKEUP_ENABLE)
	/* configure sw0 as input, interrupt as level active to allow wake-up */
	rc = gpio_pin_configure_dt(&sw0, GPIO_INPUT);
	if (rc < 0) {
		printf("Could not configure sw0 GPIO (%d)\n", rc);
		return 0;
	}

	rc = gpio_pin_interrupt_configure_dt(&sw0, GPIO_INT_LEVEL_ACTIVE);
	if (rc < 0) {
		printf("Could not configure sw0 GPIO interrupt (%d)\n", rc);
		return 0;
	}
#endif
#if defined(CONFIG_LPCOMP_WAKEUP_ENABLE)
	comparator_set_trigger(comp_dev, COMPARATOR_TRIGGER_BOTH_EDGES);
	comparator_trigger_is_pending(comp_dev);
	printf("Entering system off; change signal level at comparator input to restart\n");
#endif
	rc = gpio_pin_configure_dt(&sw1, GPIO_INPUT);
	if (rc < 0) {
		printf("Could not configure sw1 GPIO (%d)\n", rc);
		return 0;
	}

	rc = gpio_pin_interrupt_configure_dt(&sw1, GPIO_INT_LEVEL_ACTIVE);
	if (rc < 0) {
		printf("Could not configure sw0 GPIO interrupt (%d)\n", rc);
		return 0;
	}

	if (do_poweroff) {
		printf("Entering system off; press sw0 or sw1 to restart\n");
	} else {
		printf("Button sw1 pressed, not entering system off\n");
	}

	rc = pm_device_action_run(cons, PM_DEVICE_ACTION_SUSPEND);
	if (rc < 0) {
		printf("Could not suspend console (%d)\n", rc);
		return 0;
	}

	if (IS_ENABLED(CONFIG_APP_USE_RETAINED_MEM)) {
		/* Update the retained state */
		retained.off_count += 1;
		retained_update();
	}


	if (do_poweroff) {
#if CONFIG_SOC_NRF54H20_CPUAPP
		/* Local RAM0 (TCM) is currently not used so retention can be disabled. */
		nrf_memconf_ramblock_ret_mask_enable_set(NRF_MEMCONF, 0, RAMBLOCK_RET_MASK, false);
		nrf_memconf_ramblock_ret_mask_enable_set(NRF_MEMCONF, 1, RAMBLOCK_RET_MASK, false);
#endif
		sys_poweroff();
	} else {
		k_sleep(K_FOREVER);
	}

	hwinfo_clear_reset_cause();
	sys_poweroff();

	return 0;
}
