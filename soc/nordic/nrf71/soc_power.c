/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/pm/pm.h>
#include <hal/nrf_lfxo.h>

#if defined(CONFIG_PM)

/*
 * Switch LFXO between Pierce (the default, most accurate mode) and Pixo
 * (needed to reliably reach the SoC's lowest System ON idle current) on
 * every idle entry/exit. Pierce mode's periodic oscillator-amplitude
 * check, run in the MMI, otherwise forces periodic exits from the
 * chip's lowest-power state; Pixo mode has no such check.
 *
 * Only STATUS/MODE/EVENTS_MODECHANGED are used here, all public HAL
 * struct members for this SoC -- no manual register addresses needed.
 */
static void lfxo_switch_mode(uint32_t target_mode)
{
	while ((NRF_LFXO->STATUS & LFXO_STATUS_RUNNING_Msk) !=
	       (LFXO_STATUS_RUNNING_Running << LFXO_STATUS_RUNNING_Pos)) {
	}

	if ((NRF_LFXO->STATUS & LFXO_STATUS_MODE_Msk) != (target_mode << LFXO_STATUS_MODE_Pos)) {
		NRF_LFXO->EVENTS_MODECHANGED = 0;
		NRF_LFXO->MODE =
			(NRF_LFXO->MODE & ~LFXO_MODE_MODE_Msk) | (target_mode << LFXO_MODE_MODE_Pos);

		while (NRF_LFXO->EVENTS_MODECHANGED == 0) {
		}
	}
}

void pm_state_set(enum pm_state state, uint8_t substate_id)
{
	ARG_UNUSED(substate_id);

	if (state == PM_STATE_RUNTIME_IDLE) {
		lfxo_switch_mode(LFXO_MODE_MODE_Pixo);
	}

	k_cpu_idle();
}

void pm_state_exit_post_ops(enum pm_state state, uint8_t substate_id)
{
	ARG_UNUSED(substate_id);

	if (state == PM_STATE_RUNTIME_IDLE) {
		lfxo_switch_mode(LFXO_MODE_MODE_Pierce);
	}

	irq_unlock(0);
}

#endif /* CONFIG_PM */
