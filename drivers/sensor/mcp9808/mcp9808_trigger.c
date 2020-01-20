/*
 * Copyright (c) 2019 Peter Bigot Consulting, LLC
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <kernel.h>
#include <drivers/i2c.h>
#include <sys/byteorder.h>
#include <sys/__assert.h>
#include <logging/log.h>
#include "mcp9808.h"

LOG_MODULE_DECLARE(MCP9808, CONFIG_SENSOR_LOG_LEVEL);

static int mcp9808_reg_write(struct device *dev, u8_t reg, u16_t val)
{
	const struct mcp9808_data *data = dev->driver_data;
	const struct mcp9808_config *cfg = dev->config->config_info;
	u8_t buf[3] = {
		reg,
		val >> 8,	/* big-endian register storage */
		val & 0xFF,
	};

	return i2c_write(data->i2c_master, buf, sizeof(buf), cfg->i2c_addr);
}

int mcp9808_attr_set(struct device *dev, enum sensor_channel chan,
		     enum sensor_attribute attr,
		     const struct sensor_value *val)
{
	u8_t reg_addr;
	int temp;

	__ASSERT_NO_MSG(chan == SENSOR_CHAN_AMBIENT_TEMP);

	switch (attr) {
	case SENSOR_ATTR_LOWER_THRESH:
		reg_addr = MCP9808_REG_LOWER_LIMIT;
		break;
	case SENSOR_ATTR_UPPER_THRESH:
		reg_addr = MCP9808_REG_UPPER_LIMIT;
		break;
	default:
		return -EINVAL;
	}

	/* Convert temperature to a signed scaled value, then write
	 * the 12-bit 2s complement-plus-sign-bit register value.
	 */
	temp = val->val1 * MCP9808_TEMP_SCALE_CEL;
	temp += (MCP9808_TEMP_SCALE_CEL * val->val2) / 1000000;

	return mcp9808_reg_write(dev, reg_addr,
				 mcp9808_temp_reg_from_signed(temp));
}

static inline void setup_int(struct device *dev,
			     bool enable)
{
	const struct mcp9808_data *data = dev->driver_data;
	const struct mcp9808_config *cfg = dev->config->config_info;

	if (enable) {
		gpio_pin_enable_callback(data->alert_gpio, cfg->alert_pin);
	} else {
		gpio_pin_disable_callback(data->alert_gpio, cfg->alert_pin);
	}
}

static void handle_int(struct device *dev)
{
	struct mcp9808_data *data = dev->driver_data;

	setup_int(dev, false);

#if defined(CONFIG_MCP9808_TRIGGER_OWN_THREAD)
	k_sem_give(&data->sem);
#elif defined(CONFIG_MCP9808_TRIGGER_GLOBAL_THREAD)
	k_work_submit(&data->work);
#endif
}

static void process_int(struct device *dev)
{
	struct mcp9808_data *data = dev->driver_data;

	if (data->trigger_handler) {
		data->trigger_handler(dev, &data->trig);
	}

	if (data->trigger_handler) {
		setup_int(dev, true);
	}
}

int mcp9808_trigger_set(struct device *dev,
			const struct sensor_trigger *trig,
			sensor_trigger_handler_t handler)
{
	struct mcp9808_data *data = dev->driver_data;
	const struct mcp9808_config *cfg = dev->config->config_info;
	int rv = 0;

	setup_int(dev, false);

	data->trig = *trig;
	data->trigger_handler = handler;

	if (handler != NULL) {
		u32_t val;

		setup_int(dev, true);

		rv = gpio_pin_read(data->alert_gpio, cfg->alert_pin, &val);
		if ((rv == 0) && (val == 0)) {
			handle_int(dev);
		}
	}

	return rv;
}

static void alert_cb(struct device *dev, struct gpio_callback *cb, u32_t pins)
{
	struct mcp9808_data *data =
		CONTAINER_OF(cb, struct mcp9808_data, alert_cb);

	ARG_UNUSED(pins);

	handle_int(data->dev);
}

#ifdef CONFIG_MCP9808_TRIGGER_OWN_THREAD

static void mcp9808_thread_main(int arg1, int arg2)
{
	struct device *dev = INT_TO_POINTER(arg1);
	struct mcp9808_data *data = dev->driver_data;

	ARG_UNUSED(arg2);

	while (true) {
		k_sem_take(&data->sem, K_FOREVER);
		process_int(dev);
	}
}

static K_THREAD_STACK_DEFINE(mcp9808_thread_stack, CONFIG_MCP9808_THREAD_STACK_SIZE);
static struct k_thread mcp9808_thread;
#else /* CONFIG_MCP9808_TRIGGER_GLOBAL_THREAD */

static void mcp9808_gpio_thread_cb(struct k_work *work)
{
	struct mcp9808_data *data =
		CONTAINER_OF(work, struct mcp9808_data, work);

	process_int(data->dev);
}

#endif /* CONFIG_MCP9808_TRIGGER_GLOBAL_THREAD */

int mcp9808_setup_interrupt(struct device *dev)
{
	struct mcp9808_data *data = dev->driver_data;
	const struct mcp9808_config *cfg = dev->config->config_info;
	struct device *gpio;
	int rc = mcp9808_reg_write(dev, MCP9808_REG_CRITICAL,
				   MCP9808_TEMP_ABS_MASK);
	if (rc == 0) {
		rc = mcp9808_reg_write(dev, MCP9808_REG_CONFIG,
				       MCP9808_CFG_ALERT_ENA);
	}

	data->dev = dev;

#ifdef CONFIG_MCP9808_TRIGGER_OWN_THREAD
	k_sem_init(&data->sem, 0, UINT_MAX);

	k_thread_create(&mcp9808_thread, mcp9808_thread_stack,
			CONFIG_MCP9808_THREAD_STACK_SIZE,
			(k_thread_entry_t)mcp9808_thread_main, dev, 0, NULL,
			K_PRIO_COOP(CONFIG_MCP9808_THREAD_PRIORITY), 0, K_NO_WAIT);
#else /* CONFIG_MCP9808_TRIGGER_GLOBAL_THREAD */
	data->work.handler = mcp9808_gpio_thread_cb;
#endif /* trigger type */

	gpio = device_get_binding(cfg->alert_controller);
	if (gpio != NULL) {
		data->alert_gpio = gpio;
	} else {
		rc = -ENOENT;
	}

	if (rc == 0) {
		rc = gpio_pin_configure(gpio, cfg->alert_pin,
					GPIO_DIR_IN | GPIO_INT | GPIO_INT_EDGE |
					GPIO_PUD_PULL_UP |
					GPIO_INT_ACTIVE_LOW | GPIO_INT_DEBOUNCE);
	}

	if (rc == 0) {
		gpio_init_callback(&data->alert_cb, alert_cb, BIT(cfg->alert_pin));

		rc = gpio_add_callback(gpio, &data->alert_cb);
	}

	return rc;
}
