/*
 * Copyright (c) 2017 Intel Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <zephyr.h>
#include <soc_power.h>
#include <nrf_power.h>

#define LOG_LEVEL CONFIG_SOC_LOG_LEVEL
#include <logging/log.h>
LOG_MODULE_DECLARE(soc);

#if defined(CONFIG_SYS_POWER_DEEP_SLEEP)
/* System_OFF is deepest Power state available, On exiting from this
 * state CPU including all peripherals reset
 */
static void _system_off(void)
{
	nrf_power_system_off();
}
#endif

static void _issue_low_power_command(void)
{
	__WFE();
	__SEV();
	__WFE();
}

/* Name: _low_power_mode
 * Parameter : Low Power Task ID
 *
 * Set Notdic SOC specific Low Power Task and invoke
 * _WFE event to put the nordic SOC into Low Power State.
 */
static void _low_power_mode(enum power_states state)
{
	switch (state) {
		/* CONSTANT LATENCY TASK */
	case SYS_POWER_STATE_CPU_LPS:
		nrf_power_task_trigger(NRF_POWER_TASK_CONSTLAT);
		break;
		/* LOW POWER TASK */
	case SYS_POWER_STATE_CPU_LPS_1:
		nrf_power_task_trigger(NRF_POWER_TASK_LOWPWR);
		break;

	default:
		/* Unsupported State */
		LOG_ERR("Unsupported State");
		break;
	}

	/* Issue __WFE*/
	_issue_low_power_command();
}

/* Invoke Low Power/System Off specific Tasks */
void sys_set_power_state(enum power_states state)
{
	switch (state) {
	case SYS_POWER_STATE_CPU_LPS:
		_low_power_mode(SYS_POWER_STATE_CPU_LPS);
		break;
	case SYS_POWER_STATE_CPU_LPS_1:
		_low_power_mode(SYS_POWER_STATE_CPU_LPS_1);
		break;
#if defined(CONFIG_SYS_POWER_DEEP_SLEEP)
	case SYS_POWER_STATE_DEEP_SLEEP:
		_system_off();
		break;
#endif
	default:
		/* Unsupported State */
		LOG_ERR("Unsupported State");
		break;
	}
}

/* Handle SOC specific activity after Low Power Mode Exit */
void sys_power_state_post_ops(enum power_states state)
{
	switch (state) {
	case SYS_POWER_STATE_CPU_LPS:
	case SYS_POWER_STATE_CPU_LPS_1:
		/* Enable interrupts */
		__set_BASEPRI(0);
		break;
#if defined(CONFIG_SYS_POWER_DEEP_SLEEP)
	case SYS_POWER_STATE_DEEP_SLEEP:
		break;
#endif
	default:
		/* Unsupported State */
		LOG_ERR("Unsupported State");
		break;
	}
}

bool sys_is_valid_power_state(enum power_states state)
{
	switch (state) {
	case SYS_POWER_STATE_CPU_LPS:
	case SYS_POWER_STATE_CPU_LPS_1:
#if defined(CONFIG_SYS_POWER_DEEP_SLEEP)
	case SYS_POWER_STATE_DEEP_SLEEP:
#endif
		return true;
		break;
	default:
		LOG_DBG("Unsupported State");
		break;
	}

	return false;
}

/* Overrides the weak ARM implementation:
   Set general purpose retention register and reboot */
void sys_arch_reboot(int type)
{
	nrf_power_gpregret_set((uint8_t)type);
	NVIC_SystemReset();
}
