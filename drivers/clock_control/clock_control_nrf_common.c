/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 * SPDX-License-Identifier: Apache-2.0
 */

#include "clock_control_nrf_common.h"
#include <zephyr/irq.h>
#include <zephyr/device.h>
#include <nrfx.h>

#if defined(CONFIG_CLOCK_CONTROL_NRFX) && !defined(CONFIG_CLOCK_CONTROL_NRF)

#if IS_ENABLED(CONFIG_NRFX_POWER)
#include <nrfx_power.h>
#endif

#define DT_DRV_COMPAT nordic_nrf_clock

static bool irq_connected;

/* This function should be treated as static.
 * static keyword is not used so that it can be accessed by interrupt oriented tests.
 */
void common_irq_handler(void)
{
#if IS_ENABLED(CONFIG_NRFX_POWER)
	nrfx_power_irq_handler();
#endif

	STRUCT_SECTION_FOREACH(clock_control_nrf_irq_handler, irq) {
		irq->handler();
	}
}

void common_connect_irq(void)
{
	if (irq_connected) {
		return;
	}
	irq_connected = true;

#if NRF_LFRC_HAS_CALIBRATION
	IRQ_CONNECT(LFRC_IRQn, DT_INST_IRQ(0, priority), nrfx_isr, common_irq_handler, 0);
	irq_enable(LFRC_IRQn);
#endif

	IRQ_CONNECT(DT_INST_IRQN(0), DT_INST_IRQ(0, priority), nrfx_isr, common_irq_handler, 0);
	irq_enable(DT_INST_IRQN(0));
}

static int set_off_state(uint32_t *flags, uint32_t ctx)
{
	int err = 0;
	unsigned int key = irq_lock();
	uint32_t current_ctx = COMMON_GET_CTX(*flags);

	if ((current_ctx != 0) && (current_ctx != ctx)) {
		err = -EPERM;
	} else {
		*flags = CLOCK_CONTROL_STATUS_OFF;
	}

	irq_unlock(key);

	return err;
}

static int set_starting_state(uint32_t *flags, uint32_t ctx)
{
	int err = 0;
	unsigned int key = irq_lock();
	uint32_t current_ctx = COMMON_GET_CTX(*flags);

	if ((*flags & (COMMON_STATUS_MASK)) == CLOCK_CONTROL_STATUS_OFF) {
		*flags = CLOCK_CONTROL_STATUS_STARTING | ctx;
	} else if (current_ctx != ctx) {
		err = -EPERM;
	} else {
		err = -EALREADY;
	}

	irq_unlock(key);

	return err;
}

void common_set_on_state(uint32_t *flags)
{
	unsigned int key = irq_lock();

	*flags = CLOCK_CONTROL_STATUS_ON | COMMON_GET_CTX(*flags);
	irq_unlock(key);
}

void common_blocking_start_callback(const struct device *dev, clock_control_subsys_t subsys,
				    void *user_data)
{
	ARG_UNUSED(subsys);
	ARG_UNUSED(dev);

	struct k_sem *sem = user_data;

	k_sem_give(sem);
}

int common_async_start(const struct device *dev, clock_control_cb_t cb, void *user_data,
		       uint32_t ctx)
{
	int err;

	err = set_starting_state(&((common_clock_data_t *)dev->data)->flags, ctx);
	if (err < 0) {
		return err;
	}

	((common_clock_data_t *)dev->data)->cb = cb;
	((common_clock_data_t *)dev->data)->user_data = user_data;

	((common_clock_config_t *)dev->config)->start();

	return 0;
}

int common_stop(const struct device *dev, uint32_t ctx)
{
	int err;

	err = set_off_state(&((common_clock_data_t *)dev->data)->flags, ctx);
	if (err < 0) {
		return err;
	}

	((common_clock_config_t *)dev->config)->stop();

	return 0;
}

void common_onoff_started_callback(const struct device *dev, clock_control_subsys_t sys,
				   void *user_data)
{
	ARG_UNUSED(sys);

	onoff_notify_fn notify = user_data;

	notify(&((common_clock_data_t *)dev->data)->mgr, 0);
}

void common_clkstarted_handle(const struct device *dev)
{
	clock_control_cb_t callback = ((common_clock_data_t *)dev->data)->cb;

	((common_clock_data_t *)dev->data)->cb = NULL;
	common_set_on_state(&((common_clock_data_t *)dev->data)->flags);

	if (callback) {
		callback(dev, NULL, ((common_clock_data_t *)dev->data)->user_data);
	}
}

#endif /* defined(CONFIG_CLOCK_CONTROL_NRFX) && !defined(CONFIG_CLOCK_CONTROL_NRF) */
