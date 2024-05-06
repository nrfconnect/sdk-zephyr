/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/sys/poweroff.h>
#include <zephyr/toolchain.h>
#include <zephyr/pm/policy.h>
#include <zephyr/arch/common/pm_s2ram.h>
#include <hal/nrf_resetinfo.h>
#include <hal/nrf_lrcconf.h>
#include <hal/nrf_memconf.h>
#include <zephyr/cache.h>

#if defined(NRF_APPLICATION)
#define RAMBLOCK_CONTROL_BIT_ICACHE 1
#define RAMBLOCK_CONTROL_BIT_DCACHE 2
#define RAMBLOCK_POWER_ID	    0
#define RAMBLOCK_CONTROL_OFF	    0
#elif defined(NRF_RADIOCORE)
#define RAMBLOCK_CONTROL_BIT_ICACHE 2
#define RAMBLOCK_CONTROL_BIT_DCACHE 3
#define RAMBLOCK_POWER_ID	    0
#define RAMBLOCK_CONTROL_OFF	    0
#else
#error "Unsupported domain."
#endif

static void suspend_common(void)
{
	if (IS_ENABLED(CONFIG_DCACHE)) {
		/* Flush, disable and power down DCACHE */
		sys_cache_data_flush_all();
		sys_cache_data_disable();
		nrf_memconf_ramblock_control_enable_set(NRF_MEMCONF, RAMBLOCK_POWER_ID,
							RAMBLOCK_CONTROL_BIT_DCACHE, false);
	}

	if (IS_ENABLED(CONFIG_ICACHE)) {
		/* Flush, disable and power down ICACHE */
		sys_cache_instr_disable();
		nrf_memconf_ramblock_control_enable_set(NRF_MEMCONF, RAMBLOCK_POWER_ID,
							RAMBLOCK_CONTROL_BIT_ICACHE, false);
	}

	/* Disable retention */
	nrf_lrcconf_retain_set(NRF_LRCCONF010, NRF_LRCCONF_POWER_DOMAIN_0, false);
	nrf_lrcconf_poweron_force_set(NRF_LRCCONF010, NRF_LRCCONF_POWER_DOMAIN_0, false);

	k_cpu_idle();
}

void z_sys_poweroff(void)
{
	nrf_resetinfo_resetreas_local_set(NRF_RESETINFO, 0);
	nrf_resetinfo_restore_valid_set(NRF_RESETINFO, false);

	nrf_lrcconf_retain_set(NRF_LRCCONF010, NRF_LRCCONF_POWER_MAIN, false);
	nrf_lrcconf_poweron_force_set(NRF_LRCCONF010, NRF_LRCCONF_POWER_MAIN, false);

	nrf_lrcconf_task_trigger(NRF_LRCCONF010, NRF_LRCCONF_TASK_SYSTEMOFFREADY);

	suspend_common();

	CODE_UNREACHABLE;
}

__weak void z_pm_soc_resume(void)
{
}

/* Resume domain after local suspend to RAM. */
void z_pm_sys_resume(void)
{
	if (IS_ENABLED(CONFIG_ICACHE)) {
		/* Power up and re-enable ICACHE */
		nrf_memconf_ramblock_control_enable_set(NRF_MEMCONF, RAMBLOCK_POWER_ID,
							RAMBLOCK_CONTROL_BIT_ICACHE, true);
		sys_cache_instr_enable();
	}

	if (IS_ENABLED(CONFIG_DCACHE)) {
		/* Power up and re-enable DCACHE */
		nrf_memconf_ramblock_control_enable_set(NRF_MEMCONF, RAMBLOCK_POWER_ID,
							RAMBLOCK_CONTROL_BIT_DCACHE, true);
		sys_cache_data_enable();
	}

	/* Re-enable domain retention. */
	nrf_lrcconf_retain_set(NRF_LRCCONF010, NRF_LRCCONF_POWER_DOMAIN_0, true);
	nrf_lrcconf_poweron_force_set(NRF_LRCCONF010, NRF_LRCCONF_POWER_MAIN, true);
	nrf_lrcconf_poweron_force_set(NRF_LRCCONF010, NRF_LRCCONF_POWER_DOMAIN_0, true);


	z_pm_soc_resume();
}

/* Function called during local domain suspend to RAM. */
int z_pm_sys_suspend(void)
{
	/* Set intormation which is used on domain wakeup to determine if resume from RAM shall
	 * be performed.
	 */
	nrf_resetinfo_resetreas_local_set(NRF_RESETINFO,
					  NRF_RESETINFO_RESETREAS_LOCAL_UNRETAINED_MASK);
	nrf_resetinfo_restore_valid_set(NRF_RESETINFO, true);
	nrf_lrcconf_poweron_force_set(NRF_LRCCONF010, NRF_LRCCONF_POWER_MAIN, false);

	suspend_common();

	/*
	 * We might reach this point is k_cpu_idle returns (there is a pre sleep hook that
	 * can abort sleeping.
	 */
	return -EBUSY;
}

void pm_s2ram_mark_set(void)
{
	/* empty */
}

bool pm_s2ram_mark_check_and_clear(void)
{
	bool unretained_wake;
	bool restore_valid;

	unretained_wake = nrf_resetinfo_resetreas_local_get(NRF_RESETINFO) &
			  NRF_RESETINFO_RESETREAS_LOCAL_UNRETAINED_MASK;
	nrf_resetinfo_resetreas_local_set(NRF_RESETINFO, 0);

	restore_valid = nrf_resetinfo_restore_valid_check(NRF_RESETINFO);
	nrf_resetinfo_restore_valid_set(NRF_RESETINFO, false);

	return (unretained_wake & restore_valid) ? true : false;
}

#if CONFIG_PM

__weak bool z_pm_do_suspend(void)
{
	return true;
}

void do_suspend(void)
{
	if (!z_pm_do_suspend()) {
		return;
	}

	/*
	 * Save the CPU context (including the return address),set the SRAM
	 * marker and power off the system.
	 */
	(void)pm_s2ram_suspend(z_pm_sys_suspend);

	/*
	 * On resuming or error we return exactly *HERE*
	 */

	z_pm_sys_resume();
}

void pm_state_set(enum pm_state state, uint8_t substate_id)
{
	if (state != PM_STATE_SUSPEND_TO_RAM) {
		return;
	}

	do_suspend();
}

void pm_state_exit_post_ops(enum pm_state state, uint8_t substate_id)
{
	if (state != PM_STATE_SUSPEND_TO_RAM) {
		return;
	}

	irq_unlock(0);
}
#endif
