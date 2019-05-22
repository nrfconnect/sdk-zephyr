/*
 * Copyright (c) 2015 Intel Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>

#include <kernel.h>
#include <device.h>
#include <shared_irq.h>
#include <init.h>
#include <sys_io.h>

#ifdef CONFIG_IOAPIC
#include <drivers/ioapic.h>
#endif

/**
 *  @brief Register a device ISR
 *  @param dev Pointer to device structure for SHARED_IRQ driver instance.
 *  @param isr_func Pointer to the ISR function for the device.
 *  @param isr_dev Pointer to the device that will service the interrupt.
 */
static int isr_register(struct device *dev, isr_t isr_func,
				 struct device *isr_dev)
{
	struct shared_irq_runtime *clients = dev->driver_data;
	const struct shared_irq_config *config = dev->config->config_info;
	u32_t i;

	for (i = 0U; i < config->client_count; i++) {
		if (!clients->client[i].isr_dev) {
			clients->client[i].isr_dev = isr_dev;
			clients->client[i].isr_func = isr_func;
			return 0;
		}
	}
	return -EIO;
}

/**
 *  @brief Enable ISR for device
 *  @param dev Pointer to device structure for SHARED_IRQ driver instance.
 *  @param isr_dev Pointer to the device that will service the interrupt.
 */
static inline int enable(struct device *dev, struct device *isr_dev)
{
	struct shared_irq_runtime *clients = dev->driver_data;
	const struct shared_irq_config *config = dev->config->config_info;
	u32_t i;

	for (i = 0U; i < config->client_count; i++) {
		if (clients->client[i].isr_dev == isr_dev) {
			clients->client[i].enabled = 1U;
			irq_enable(config->irq_num);
			return 0;
		}
	}
	return -EIO;
}

static int last_enabled_isr(struct shared_irq_runtime *clients, int count)
{
	u32_t i;

	for (i = 0U; i < count; i++) {
		if (clients->client[i].enabled) {
			return 0;
		}
	}
	return 1;
}
/**
 *  @brief Disable ISR for device
 *  @param dev Pointer to device structure for SHARED_IRQ driver instance.
 *  @param isr_dev Pointer to the device that will service the interrupt.
 */
static inline int disable(struct device *dev, struct device *isr_dev)
{
	struct shared_irq_runtime *clients = dev->driver_data;
	const struct shared_irq_config *config = dev->config->config_info;
	u32_t i;

	for (i = 0U; i < config->client_count; i++) {
		if (clients->client[i].isr_dev == isr_dev) {
			clients->client[i].enabled = 0U;
			if (last_enabled_isr(clients, config->client_count)) {
				irq_disable(config->irq_num);
			}
			return 0;
		}
	}
	return -EIO;
}

void shared_irq_isr(struct device *dev)
{
	struct shared_irq_runtime *clients = dev->driver_data;
	const struct shared_irq_config *config = dev->config->config_info;
	u32_t i;

	for (i = 0U; i < config->client_count; i++) {
		if (clients->client[i].isr_dev) {
			clients->client[i].isr_func(clients->client[i].isr_dev);
		}
	}
}

static const struct shared_irq_driver_api api_funcs = {
	.isr_register = isr_register,
	.enable = enable,
	.disable = disable,
};


int shared_irq_initialize(struct device *dev)
{
	const struct shared_irq_config *config = dev->config->config_info;
	config->config();
	return 0;
}

#if CONFIG_SHARED_IRQ_0
void shared_irq_config_0_irq(void);

const struct shared_irq_config shared_irq_config_0 = {
	.irq_num = DT_SHARED_IRQ_SHAREDIRQ0_IRQ_0,
	.client_count = CONFIG_SHARED_IRQ_NUM_CLIENTS,
	.config = shared_irq_config_0_irq
};

struct shared_irq_runtime shared_irq_0_runtime;

DEVICE_AND_API_INIT(shared_irq_0, DT_SHARED_IRQ_SHAREDIRQ0_LABEL,
		shared_irq_initialize, &shared_irq_0_runtime,
		&shared_irq_config_0, POST_KERNEL,
		CONFIG_SHARED_IRQ_INIT_PRIORITY, &api_funcs);

void shared_irq_config_0_irq(void)
{
	IRQ_CONNECT(DT_SHARED_IRQ_SHAREDIRQ0_IRQ_0,
		    DT_SHARED_IRQ_SHAREDIRQ0_IRQ_0_PRIORITY,
		    shared_irq_isr, DEVICE_GET(shared_irq_0),
		    DT_SHARED_IRQ_SHAREDIRQ0_IRQ_0_SENSE);
}

#endif /* CONFIG_SHARED_IRQ_0 */

#if CONFIG_SHARED_IRQ_1
void shared_irq_config_1_irq(void);

const struct shared_irq_config shared_irq_config_1 = {
	.irq_num = DT_SHARED_IRQ_SHAREDIRQ1_IRQ_0,
	.client_count = CONFIG_SHARED_IRQ_NUM_CLIENTS,
	.config = shared_irq_config_1_irq
};

struct shared_irq_runtime shared_irq_1_runtime;

DEVICE_AND_API_INIT(shared_irq_1, DT_SHARED_IRQ_SHAREDIRQ1_LABEL,
		shared_irq_initialize, &shared_irq_1_runtime,
		&shared_irq_config_1, POST_KERNEL,
		CONFIG_SHARED_IRQ_INIT_PRIORITY, &api_funcs);

void shared_irq_config_1_irq(void)
{
	IRQ_CONNECT(DT_SHARED_IRQ_SHAREDIRQ1_IRQ_0,
		    DT_SHARED_IRQ_SHAREDIRQ1_IRQ_0_PRIORITY,
		    shared_irq_isr, DEVICE_GET(shared_irq_1),
		    DT_SHARED_IRQ_SHAREDIRQ1_IRQ_0_SENSE);
}

#endif /* CONFIG_SHARED_IRQ_1 */
