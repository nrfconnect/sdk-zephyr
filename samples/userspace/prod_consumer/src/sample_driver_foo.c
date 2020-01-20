/*
 * Copyright (c) 2019 Intel Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "sample_driver.h"
#include <string.h>
#include <kernel.h>
#include <logging/log.h>

LOG_MODULE_REGISTER(sample_driver);

/*
 * Fake sample driver for demonstration purposes
 *
 * This is fake driver for demonstration purposes, showing how an application
 * can make system calls to interact with it.
 *
 * The driver sets up a timer which is used to fake interrupts.
 */

#define DEV_DATA(dev) \
	((struct sample_driver_foo_dev_data *const)(dev)->driver_data)

struct sample_driver_foo_dev_data {
	sample_driver_callback_t cb;
	void *cb_context;
	struct k_timer timer; /* to fake 'interrupts' */
	u32_t count;
};

static int sample_driver_foo_write(struct device *dev, void *buf)
{
	LOG_DBG("%s(%p, %p)", __func__, dev, buf);

	return 0;
}

static int sample_driver_foo_set_callback(struct device *dev,
				       sample_driver_callback_t cb,
				       void *context)
{
	struct sample_driver_foo_dev_data *data = DEV_DATA(dev);
	int key = irq_lock();

	data->cb_context = context;
	data->cb = cb;
	irq_unlock(key);

	return 0;
}

static int sample_driver_foo_state_set(struct device *dev, bool active)
{
	struct sample_driver_foo_dev_data *data = DEV_DATA(dev);

	LOG_DBG("%s(%p, %d)", __func__, dev, active);

	data->timer.user_data = dev;
	if (active) {
		k_timer_start(&data->timer, K_MSEC(1000), K_MSEC(1000));
	} else {
		k_timer_stop(&data->timer);
	}

	return 0;
}

static struct sample_driver_api sample_driver_foo_api = {
	.write = sample_driver_foo_write,
	.set_callback = sample_driver_foo_set_callback,
	.state_set = sample_driver_foo_state_set
};

static void sample_driver_foo_isr(void *param)
{
	struct device *dev = param;
	struct sample_driver_foo_dev_data *data = DEV_DATA(dev);

	char data_payload[SAMPLE_DRIVER_MSG_SIZE];

	LOG_INF("%s: param=%p count=%u", __func__, param,
		data->count);

	/* Just for demonstration purposes; the data payload is full of junk */
	if (data->cb) {
		data->cb(dev, data->cb_context, data_payload);
	}

	data->count++;
}

static void sample_driver_timer_cb(struct k_timer *timer)
{
	sample_driver_foo_isr(timer->user_data);
}

static int sample_driver_foo_init(struct device *dev)
{
	struct sample_driver_foo_dev_data *data = DEV_DATA(dev);

	k_timer_init(&data->timer, sample_driver_timer_cb, NULL);

	LOG_DBG("initialized foo sample driver %p", dev);

	return 0;
}

static struct sample_driver_foo_dev_data sample_driver_foo_dev_data_0;

DEVICE_AND_API_INIT(sample_driver_foo_0, SAMPLE_DRIVER_NAME_0,
		    &sample_driver_foo_init,
		    &sample_driver_foo_dev_data_0, NULL,
		    POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEVICE,
		    &sample_driver_foo_api);
