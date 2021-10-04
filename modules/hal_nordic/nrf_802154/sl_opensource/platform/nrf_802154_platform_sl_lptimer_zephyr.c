/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "platform/nrf_802154_platform_sl_lptimer.h"

#include <assert.h>
#include <kernel.h>

#include "drivers/timer/nrf_rtc_timer.h"

#include "platform/nrf_802154_clock.h"
#include "nrf_802154_sl_utils.h"

static volatile bool m_clock_ready;
static bool m_compare_int_lock_key;
static int32_t m_rtc_channel;
static uint32_t m_critical_section_cnt;

void rtc_irq_handler(int32_t id, uint64_t expire_time, void *user_data)
{
	(void)user_data;
	(void)expire_time;

	assert(id == m_rtc_channel);

	uint64_t curr_time = z_nrf_rtc_timer_read();

	nrf_802154_sl_timer_handler(curr_time);
}

void nrf_802154_clock_lfclk_ready(void)
{
	m_clock_ready = true;
}

void nrf_802154_platform_sl_lp_timer_init(void)
{
	m_critical_section_cnt = 0UL;

	/* Setup low frequency clock. */
	nrf_802154_clock_lfclk_start();

	while (!m_clock_ready) {
		/* Intentionally empty */
	}

	m_rtc_channel = z_nrf_rtc_timer_chan_alloc();
	if (m_rtc_channel < 0) {
		assert(false);
		return;
	}

	(void)z_nrf_rtc_timer_compare_int_lock(m_rtc_channel);
}

void nrf_802154_platform_sl_lp_timer_deinit(void)
{
	(void)z_nrf_rtc_timer_compare_int_lock(m_rtc_channel);

	z_nrf_rtc_timer_chan_free(m_rtc_channel);

	nrf_802154_clock_lfclk_stop();
}

uint64_t nrf_802154_platform_sl_lptimer_current_lpticks_get(void)
{
	return z_nrf_rtc_timer_read();
}

uint64_t nrf_802154_platform_sl_lptimer_us_to_lpticks_convert(uint64_t us, bool round_up)
{
	return NRF_802154_SL_US_TO_RTC_TICKS(us, round_up);
}

uint64_t nrf_802154_platform_sl_lptimer_lpticks_to_us_convert(uint64_t lpticks)
{
	/* Calculations are performed on uint64_t as it is safe to assume
	 * overflow will not occur in any foreseeable future.
	 */
	return NRF_802154_SL_RTC_TICKS_TO_US(lpticks);
}

void nrf_802154_platform_sl_lptimer_schedule_at(uint64_t fire_lpticks)
{
	/* This function is not required to be reentrant, hence no critical section. */
	z_nrf_rtc_timer_set(m_rtc_channel, fire_lpticks, rtc_irq_handler, NULL);
}

void nrf_802154_platform_sl_lptimer_disable(void)
{
	z_nrf_rtc_timer_abort(m_rtc_channel);
}

void nrf_802154_platform_sl_lptimer_critical_section_enter(void)
{
	nrf_802154_sl_mcu_critical_state_t state;

	nrf_802154_sl_mcu_critical_enter(state);

	m_critical_section_cnt++;

	if (m_critical_section_cnt == 1UL) {
		m_compare_int_lock_key = z_nrf_rtc_timer_compare_int_lock(m_rtc_channel);
	}

	nrf_802154_sl_mcu_critical_exit(state);
}

void nrf_802154_platform_sl_lptimer_critical_section_exit(void)
{
	nrf_802154_sl_mcu_critical_state_t state;

	nrf_802154_sl_mcu_critical_enter(state);

	assert(m_critical_section_cnt > 0UL);

	if (m_critical_section_cnt == 1UL) {
		z_nrf_rtc_timer_compare_int_unlock(m_rtc_channel, m_compare_int_lock_key);
	}

	m_critical_section_cnt--;

	nrf_802154_sl_mcu_critical_exit(state);
}
