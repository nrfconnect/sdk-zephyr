/*
 * Copyright (c) 2016 Nordic Semiconductor ASA
 * Copyright (c) 2016 Vinayak Kariappa Chettimada
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <soc.h>
#include <errno.h>
#include <atomic.h>
#include <device.h>
#include <clock_control.h>
#include <misc/__assert.h>
#include <nrf_clock.h>
#if defined(CONFIG_USB) && defined(CONFIG_SOC_NRF52840)
#include <nrf_power.h>
#include <drivers/clock_control/nrf_clock_control.h>
#endif

static u8_t m16src_ref;
static u8_t m16src_grd;
static u8_t k32src_initialized;

static int m16src_start(struct device *dev, clock_control_subsys_t sub_system)
{
	bool blocking;
	u32_t imask;
	u32_t stat;

	/* If the clock is already started then just increment refcount.
	 * If the start and stop don't happen in pairs, a rollover will
	 * be caught and in that case system should assert.
	 */

	/* Test for reference increment from zero and resource guard not taken.
	 */
	imask = irq_lock();

	if (m16src_ref++) {
		irq_unlock(imask);
		goto hf_already_started;
	}

	if (m16src_grd) {
		m16src_ref--;
		irq_unlock(imask);
		return -EAGAIN;
	}

	m16src_grd = 1U;

	irq_unlock(imask);

	/* If blocking then spin-wait in CPU sleep until 16MHz clock settles. */
	blocking = POINTER_TO_UINT(sub_system);
	if (blocking) {
		u32_t intenset;

		irq_disable(DT_NORDIC_NRF_CLOCK_0_IRQ_0);

		NRF_CLOCK->EVENTS_HFCLKSTARTED = 0;

		intenset = NRF_CLOCK->INTENSET;
		nrf_clock_int_enable(NRF_CLOCK_INT_HF_STARTED_MASK);

		nrf_clock_task_trigger(NRF_CLOCK_TASK_HFCLKSTART);

		while (NRF_CLOCK->EVENTS_HFCLKSTARTED == 0) {
			__WFE();
			__SEV();
			__WFE();
		}

		NRF_CLOCK->EVENTS_HFCLKSTARTED = 0;

		if (!(intenset & CLOCK_INTENSET_HFCLKSTARTED_Msk)) {
			nrf_clock_int_disable(NRF_CLOCK_INT_HF_STARTED_MASK);
		}

		NVIC_ClearPendingIRQ(DT_NORDIC_NRF_CLOCK_0_IRQ_0);

		irq_enable(DT_NORDIC_NRF_CLOCK_0_IRQ_0);
	} else {
		NRF_CLOCK->EVENTS_HFCLKSTARTED = 0;

		nrf_clock_task_trigger(NRF_CLOCK_TASK_HFCLKSTART);
	}

	/* release resource guard */
	m16src_grd = 0U;

hf_already_started:
	/* rollover should not happen as start and stop shall be
	 * called in pairs.
	 */
	__ASSERT_NO_MSG(m16src_ref);

	stat = NRF_CLOCK_HFCLK_HIGH_ACCURACY | CLOCK_HFCLKSTAT_STATE_Msk;
	if ((NRF_CLOCK->HFCLKSTAT & stat) == stat) {
		return 0;
	} else {
		return -EINPROGRESS;
	}
}

static int m16src_stop(struct device *dev, clock_control_subsys_t sub_system)
{
	u32_t imask;

	ARG_UNUSED(sub_system);

	/* Test for started resource, if so, decrement reference and acquire
	 * resource guard.
	 */
	imask = irq_lock();

	if (!m16src_ref) {
		irq_unlock(imask);
		return -EALREADY;
	}

	if (--m16src_ref) {
		irq_unlock(imask);
		return -EBUSY;
	}

	if (m16src_grd) {
		m16src_ref++;
		irq_unlock(imask);
		return -EAGAIN;
	}

	m16src_grd = 1U;

	irq_unlock(imask);

	/* re-entrancy and mult-context safe, and reference count is zero, */

	nrf_clock_task_trigger(NRF_CLOCK_TASK_HFCLKSTOP);

	/* release resource guard */
	m16src_grd = 0U;

	return 0;
}

static int k32src_start(struct device *dev, clock_control_subsys_t sub_system)
{
	u32_t lf_clk_src;
	u32_t imask;
	u32_t stat;

#if defined(CONFIG_CLOCK_CONTROL_NRF_K32SRC_BLOCKING)
	u32_t intenset;
#endif /* CONFIG_CLOCK_CONTROL_NRF_K32SRC_BLOCKING */

	/* If the LF clock is already started, but wasn't initialized with
	 * this function, allow it to run once. This is needed because if a
	 * soft reset is triggered while watchdog is active, the LF clock will
	 * already be running, but won't be configured yet (watchdog forces LF
	 * clock to be running).
	 *
	 * That is, a hardware check won't work here, because even if the LF
	 * clock is already running it might not be initialized. We need an
	 * initialized flag.
	 */

	imask = irq_lock();

	if (k32src_initialized) {
		irq_unlock(imask);
		goto lf_already_started;
	}

	k32src_initialized = 1U;

	irq_unlock(imask);

	/* Clear events if any */
	NRF_CLOCK->EVENTS_LFCLKSTARTED = 0;

	/* Set LF Clock Source */
	lf_clk_src = POINTER_TO_UINT(sub_system);
	NRF_CLOCK->LFCLKSRC = lf_clk_src;

#if defined(CONFIG_CLOCK_CONTROL_NRF_K32SRC_BLOCKING)
	irq_disable(DT_NORDIC_NRF_CLOCK_0_IRQ_0);

	intenset = NRF_CLOCK->INTENSET;
	nrf_clock_int_enable(NRF_CLOCK_INT_LF_STARTED_MASK);

	/* Start and spin-wait until clock settles */
	nrf_clock_task_trigger(NRF_CLOCK_TASK_LFCLKSTART);

	while (NRF_CLOCK->EVENTS_LFCLKSTARTED == 0) {
		__WFE();
		__SEV();
		__WFE();
	}

	NRF_CLOCK->EVENTS_LFCLKSTARTED = 0;

	if (!(intenset & CLOCK_INTENSET_LFCLKSTARTED_Msk)) {
		nrf_clock_int_disable(NRF_CLOCK_INT_LF_STARTED_MASK);
	}

	NVIC_ClearPendingIRQ(DT_NORDIC_NRF_CLOCK_0_IRQ_0);

	irq_enable(DT_NORDIC_NRF_CLOCK_0_IRQ_0);

#else /* !CONFIG_CLOCK_CONTROL_NRF_K32SRC_BLOCKING */
	/* NOTE: LFCLK will initially start running from the LFRC if LFXO is
	 *       selected.
	 */
	nrf_clock_int_enable(NRF_CLOCK_INT_LF_STARTED_MASK);
	nrf_clock_task_trigger(NRF_CLOCK_TASK_LFCLKSTART);
#endif /* !CONFIG_CLOCK_CONTROL_NRF_K32SRC_BLOCKING */

#if NRF_CLOCK_HAS_CALIBRATION
	/* If RC selected, calibrate and start timer for consecutive
	 * calibrations.
	 */
	nrf_clock_int_disable(NRF_CLOCK_INT_DONE_MASK |
			      NRF_CLOCK_INT_CTTO_MASK);
	NRF_CLOCK->EVENTS_DONE = 0;
	NRF_CLOCK->EVENTS_CTTO = 0;

	if ((lf_clk_src & CLOCK_LFCLKSRC_SRC_Msk) == CLOCK_LFCLKSRC_SRC_RC) {
		int err;

		/* Set the Calibration Timer Initial Value */
		NRF_CLOCK->CTIV = 16;	/* 4s in 0.25s units */

		/* Enable DONE and CTTO IRQs */
		nrf_clock_int_enable(NRF_CLOCK_INT_DONE_MASK |
				     NRF_CLOCK_INT_CTTO_MASK);

		/* If non-blocking LF clock start, then start HF clock in ISR */
		if ((NRF_CLOCK->LFCLKSTAT & CLOCK_LFCLKSTAT_STATE_Msk) == 0) {
			nrf_clock_int_enable(NRF_CLOCK_INT_LF_STARTED_MASK);
			goto lf_already_started;
		}

		/* Start HF clock, if already started then explicitly
		 * assert IRQ.
		 * NOTE: The INTENSET is used as state flag to start
		 * calibration in ISR.
		 */
		nrf_clock_int_enable(NRF_CLOCK_INT_HF_STARTED_MASK);

		err = m16src_start(dev, false);
		if (!err) {
			NVIC_SetPendingIRQ(DT_NORDIC_NRF_CLOCK_0_IRQ_0);
		} else {
			__ASSERT_NO_MSG(err == -EINPROGRESS);
		}
	}
#endif /* NRF_CLOCK_HAS_CALIBRATION */

lf_already_started:
	stat = (NRF_CLOCK->LFCLKSRCCOPY & CLOCK_LFCLKSRCCOPY_SRC_Msk) |
	       CLOCK_LFCLKSTAT_STATE_Msk;
	if ((NRF_CLOCK->LFCLKSTAT & stat) == stat) {
		return 0;
	} else {
		return -EINPROGRESS;
	}
}

#if defined(CONFIG_USB) && defined(CONFIG_SOC_NRF52840)
static inline void power_event_cb(nrf_power_event_t event)
{
	extern void usb_dc_nrfx_power_event_callback(nrf_power_event_t event);

	usb_dc_nrfx_power_event_callback(event);
}
#endif

/* Note: this function has public linkage, and MUST have this
 * particular name.  The platform architecture itself doesn't care,
 * but there is a test (tests/kernel/arm_irq_vector_table) that needs
 * to find it to it can set it in a custom vector table.  Should
 * probably better abstract that at some point (e.g. query and reset
 * it by pointer at runtime, maybe?) so we don't have this leaky
 * symbol.
 */
void nrf_power_clock_isr(void *arg)
{
	u8_t pof, hf_intenset, hf, lf_intenset, lf;
#if NRF_CLOCK_HAS_CALIBRATION
	u8_t ctto, done;
	struct device *dev = arg;
#endif
#if defined(CONFIG_USB) && defined(CONFIG_SOC_NRF52840)
	bool usb_detected, usb_pwr_rdy, usb_removed;
#endif

	pof = (NRF_POWER->EVENTS_POFWARN != 0);

	hf_intenset = ((NRF_CLOCK->INTENSET &
		       CLOCK_INTENSET_HFCLKSTARTED_Msk) != 0);
	hf = (NRF_CLOCK->EVENTS_HFCLKSTARTED != 0);

	lf_intenset = ((NRF_CLOCK->INTENSET &
		       CLOCK_INTENSET_LFCLKSTARTED_Msk) != 0);
	lf = (NRF_CLOCK->EVENTS_LFCLKSTARTED != 0);

#if NRF_CLOCK_HAS_CALIBRATION
	done = (NRF_CLOCK->EVENTS_DONE != 0);
	ctto = (NRF_CLOCK->EVENTS_CTTO != 0);
#endif
#if defined(CONFIG_USB) && defined(CONFIG_SOC_NRF52840)
	usb_detected = nrf_power_event_check(NRF_POWER_EVENT_USBDETECTED);
	usb_pwr_rdy = nrf_power_event_check(NRF_POWER_EVENT_USBPWRRDY);
	usb_removed = nrf_power_event_check(NRF_POWER_EVENT_USBREMOVED);
#endif

	__ASSERT_NO_MSG(pof || hf || hf_intenset || lf
#if NRF_CLOCK_HAS_CALIBRATION
			|| done || ctto
#endif
#if defined(CONFIG_USB) && defined(CONFIG_SOC_NRF52840)
			|| usb_detected || usb_pwr_rdy || usb_removed
#endif
	);

	if (pof) {
		NRF_POWER->EVENTS_POFWARN = 0;
	}

	if (hf) {
		NRF_CLOCK->EVENTS_HFCLKSTARTED = 0;
	}

	if (hf_intenset && (hf || ((NRF_CLOCK->HFCLKSTAT &
				    (CLOCK_HFCLKSTAT_STATE_Msk |
				     CLOCK_HFCLKSTAT_SRC_Msk)) ==
				   (CLOCK_HFCLKSTAT_STATE_Msk |
				    CLOCK_HFCLKSTAT_SRC_Msk)))){
		/* INTENSET is used as state flag to start calibration,
		 * hence clear it here.
		 */
		NRF_CLOCK->INTENCLR = CLOCK_INTENCLR_HFCLKSTARTED_Msk;

#if defined(CONFIG_SOC_SERIES_NRF52X)
		/* NOTE: Errata [192] CLOCK: LFRC frequency offset after
		 * calibration.
		 * Calibration start, workaround.
		 */
		*(volatile u32_t *)0x40000C34 = 0x00000002;
#endif /* CONFIG_SOC_SERIES_NRF52X */

#if NRF_CLOCK_HAS_CALIBRATION
		/* Start Calibration */
		NRF_CLOCK->TASKS_CAL = 1;
#endif
	}

	if (lf) {
		NRF_CLOCK->EVENTS_LFCLKSTARTED = 0;

		if (lf_intenset) {
			/* INTENSET is used as state flag to start calibration,
			 * hence clear it here.
			 */
			NRF_CLOCK->INTENCLR = CLOCK_INTENCLR_LFCLKSTARTED_Msk;

#if NRF_CLOCK_HAS_CALIBRATION
			/* Start HF Clock if LF RC is used. */
			if ((NRF_CLOCK->LFCLKSRCCOPY & CLOCK_LFCLKSRCCOPY_SRC_Msk) ==
			    CLOCK_LFCLKSRCCOPY_SRC_RC) {
				ctto = 1U;
			}
#endif
		}
	}

#if NRF_CLOCK_HAS_CALIBRATION
	if (done) {
		int err;

#if defined(CONFIG_SOC_SERIES_NRF52X)
		/* NOTE: Errata [192] CLOCK: LFRC frequency offset after
		 * calibration.
		 * Calibration done, workaround.
		 */
		*(volatile u32_t *)0x40000C34 = 0x00000000;
#endif /* CONFIG_SOC_SERIES_NRF52X */

		NRF_CLOCK->EVENTS_DONE = 0;

		/* Calibration done, stop 16M Xtal. */
		err = m16src_stop(dev, NULL);
		__ASSERT_NO_MSG(!err || err == -EBUSY);

		/* Start timer for next calibration. */
		NRF_CLOCK->TASKS_CTSTART = 1;
	}

	if (ctto) {
		int err;

		NRF_CLOCK->EVENTS_CTTO = 0;

		/* Start HF clock, if already started
		 * then explicitly assert IRQ; we use the INTENSET
		 * as a state flag to start calibration.
		 */
		NRF_CLOCK->INTENSET = CLOCK_INTENSET_HFCLKSTARTED_Msk;

		err = m16src_start(dev, false);
		if (!err) {
			NVIC_SetPendingIRQ(DT_NORDIC_NRF_CLOCK_0_IRQ_0);
		} else {
			__ASSERT_NO_MSG(err == -EINPROGRESS);
		}
	}
#endif /* NRF_CLOCK_HAS_CALIBRATION */

#if defined(CONFIG_USB) && defined(CONFIG_SOC_NRF52840)
	if (usb_detected) {
		nrf_power_event_clear(NRF_POWER_EVENT_USBDETECTED);
		power_event_cb(NRF_POWER_EVENT_USBDETECTED);
	}

	if (usb_pwr_rdy) {
		nrf_power_event_clear(NRF_POWER_EVENT_USBPWRRDY);
		power_event_cb(NRF_POWER_EVENT_USBPWRRDY);
	}

	if (usb_removed) {
		nrf_power_event_clear(NRF_POWER_EVENT_USBREMOVED);
		power_event_cb(NRF_POWER_EVENT_USBREMOVED);
	}
#endif
}

static int clock_control_init(struct device *dev)
{
	/* TODO: Initialization will be called twice, once for 32KHz and then
	 * for 16 MHz clock. The vector is also shared for other power related
	 * features. Hence, design a better way to init IRQISR when adding
	 * power peripheral driver and/or new SoC series.
	 * NOTE: Currently the operations here are idempotent.
	 */
	IRQ_CONNECT(DT_NORDIC_NRF_CLOCK_0_IRQ_0,
		    DT_NORDIC_NRF_CLOCK_0_IRQ_0_PRIORITY,
		    nrf_power_clock_isr, 0, 0);

	irq_enable(DT_NORDIC_NRF_CLOCK_0_IRQ_0);

	return 0;
}

static const struct clock_control_driver_api _m16src_clock_control_api = {
	.on = m16src_start,
	.off = m16src_stop,
	.get_rate = NULL,
};

DEVICE_AND_API_INIT(clock_nrf5_m16src,
		    DT_NORDIC_NRF_CLOCK_0_LABEL "_16M",
		    clock_control_init, NULL, NULL, PRE_KERNEL_1,
		    CONFIG_KERNEL_INIT_PRIORITY_DEVICE,
		    &_m16src_clock_control_api);

static const struct clock_control_driver_api _k32src_clock_control_api = {
	.on = k32src_start,
	.off = NULL,
	.get_rate = NULL,
};

DEVICE_AND_API_INIT(clock_nrf5_k32src,
		    DT_NORDIC_NRF_CLOCK_0_LABEL "_32K",
		    clock_control_init, NULL, NULL, PRE_KERNEL_1,
		    CONFIG_KERNEL_INIT_PRIORITY_DEVICE,
		    &_k32src_clock_control_api);

#if defined(CONFIG_USB) && defined(CONFIG_SOC_NRF52840)

void nrf5_power_usb_power_int_enable(bool enable)
{
	u32_t mask;


	mask = NRF_POWER_INT_USBDETECTED_MASK |
	       NRF_POWER_INT_USBREMOVED_MASK |
	       NRF_POWER_INT_USBPWRRDY_MASK;

	if (enable) {
		nrf_power_int_enable(mask);
		irq_enable(DT_NORDIC_NRF_CLOCK_0_IRQ_0);
	} else {
		nrf_power_int_disable(mask);
	}
}

#endif
