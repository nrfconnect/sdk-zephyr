/*
 * Copyright (c) 2016-2017 Nordic Semiconductor ASA
 * Copyright (c) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <soc.h>
#include <clock_control.h>
#include <drivers/clock_control/nrf5_clock_control.h>
#include <system_timer.h>
#include <sys_clock.h>
#include <nrf_rtc.h>
#include <spinlock.h>

#define RTC NRF_RTC1

#define COUNTER_MAX 0x00ffffff
#define CYC_PER_TICK (CONFIG_SYS_CLOCK_HW_CYCLES_PER_SEC	\
		      / CONFIG_SYS_CLOCK_TICKS_PER_SEC)
#define MAX_TICKS ((COUNTER_MAX - CYC_PER_TICK) / CYC_PER_TICK)

#define MIN_DELAY 32

static struct k_spinlock lock;

static u32_t last_count;

static u32_t counter_sub(u32_t a, u32_t b)
{
	return (a - b) & COUNTER_MAX;
}

static void set_comparator(u32_t cyc)
{
	nrf_rtc_cc_set(RTC, 0, cyc & COUNTER_MAX);
}

static u32_t counter(void)
{
	return nrf_rtc_counter_get(RTC);
}

/* Note: this function has public linkage, and MUST have this
 * particular name.  The platform architecture itself doesn't care,
 * but there is a test (tests/kernel/arm_irq_vector_table) that needs
 * to find it to it can set it in a custom vector table.  Should
 * probably better abstract that at some point (e.g. query and reset
 * it by pointer at runtime, maybe?) so we don't have this leaky
 * symbol.
 */
void rtc1_nrf5_isr(void *arg)
{
	ARG_UNUSED(arg);
	RTC->EVENTS_COMPARE[0] = 0;

	k_spinlock_key_t key = k_spin_lock(&lock);
	u32_t t = counter();
	u32_t dticks = counter_sub(t, last_count) / CYC_PER_TICK;

	last_count += dticks * CYC_PER_TICK;

	if (!IS_ENABLED(CONFIG_TICKLESS_KERNEL)) {
		u32_t next = last_count + CYC_PER_TICK;

		if (counter_sub(next, t) < MIN_DELAY) {
			next += CYC_PER_TICK;
		}
		set_comparator(next);
	}

	k_spin_unlock(&lock, key);
	z_clock_announce(dticks);
}

int z_clock_driver_init(struct device *device)
{
	struct device *clock;

	ARG_UNUSED(device);

	clock = device_get_binding(CONFIG_CLOCK_CONTROL_NRF5_K32SRC_DRV_NAME);
	if (!clock) {
		return -1;
	}

	clock_control_on(clock, (void *)CLOCK_CONTROL_NRF5_K32SRC);

	/* TODO: replace with counter driver to access RTC */
	nrf_rtc_prescaler_set(RTC, 0);
	nrf_rtc_cc_set(RTC, 0, CYC_PER_TICK);
	nrf_rtc_event_enable(RTC, RTC_EVTENSET_COMPARE0_Msk);
	nrf_rtc_int_enable(RTC, RTC_INTENSET_COMPARE0_Msk);

	/* Clear the event flag and possible pending interrupt */
	nrf_rtc_event_clear(RTC, NRF_RTC_EVENT_COMPARE_0);
	NVIC_ClearPendingIRQ(NRF5_IRQ_RTC1_IRQn);

	IRQ_CONNECT(NRF5_IRQ_RTC1_IRQn, 1, rtc1_nrf5_isr, 0, 0);
	irq_enable(NRF5_IRQ_RTC1_IRQn);

	nrf_rtc_task_trigger(RTC, NRF_RTC_TASK_CLEAR);
	nrf_rtc_task_trigger(RTC, NRF_RTC_TASK_START);

	if (!IS_ENABLED(TICKLESS_KERNEL)) {
		set_comparator(counter() + CYC_PER_TICK);
	}

	return 0;
}

void z_clock_set_timeout(s32_t ticks, bool idle)
{
	ARG_UNUSED(idle);

#ifdef CONFIG_TICKLESS_KERNEL
	ticks = (ticks == K_FOREVER) ? MAX_TICKS : ticks;
	ticks = max(min(ticks - 1, (s32_t)MAX_TICKS), 0);

	k_spinlock_key_t key = k_spin_lock(&lock);
	u32_t cyc, t = counter();

	/* Round up to next tick boundary */
	cyc = ticks * CYC_PER_TICK + counter_sub(t, last_count);
	cyc += (CYC_PER_TICK - 1);
	cyc = (cyc / CYC_PER_TICK) * CYC_PER_TICK;
	cyc += last_count;

	if (counter_sub(cyc, t) < MIN_DELAY) {
		cyc += CYC_PER_TICK;
	}

	set_comparator(cyc);
	k_spin_unlock(&lock, key);
#endif
}

u32_t z_clock_elapsed(void)
{
	if (!IS_ENABLED(CONFIG_TICKLESS_KERNEL)) {
		return 0;
	}

	k_spinlock_key_t key = k_spin_lock(&lock);
	u32_t ret = counter_sub(counter(), last_count) / CYC_PER_TICK;

	k_spin_unlock(&lock, key);
	return ret;
}

u32_t _timer_cycle_get_32(void)
{
	k_spinlock_key_t key = k_spin_lock(&lock);
	u32_t ret = counter_sub(counter(), last_count) + last_count;

	k_spin_unlock(&lock, key);
	return ret;
}
