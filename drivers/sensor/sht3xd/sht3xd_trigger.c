/*
 * Copyright (c) 2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <device.h>
#include <misc/util.h>
#include <kernel.h>
#include <sensor.h>

#include "sht3xd.h"

#define LOG_LEVEL CONFIG_SENSOR_LOG_LEVEL
#include <logging/log.h>
LOG_MODULE_DECLARE(SHT3XD);

static u16_t sht3xd_temp_processed_to_raw(const struct sensor_value *val)
{
	u64_t uval;

	/* ret = (val + 45) * (2^16 - 1) / 175 */
	uval = (u64_t)(val->val1 + 45) * 1000000U + val->val2;
	return ((uval * 0xFFFF) / 175) / 1000000;
}

static int sht3xd_rh_processed_to_raw(const struct sensor_value *val)
{
	u64_t uval;

	/* ret = val * (2^16 -1) / 100 */
	uval = (u64_t)val->val1 * 1000000U + val->val2;
	return ((uval * 0xFFFF) / 100) / 1000000;
}

int sht3xd_attr_set(struct device *dev,
		    enum sensor_channel chan,
		    enum sensor_attribute attr,
		    const struct sensor_value *val)
{
	struct sht3xd_data *data = dev->driver_data;
	u16_t set_cmd, clear_cmd, reg_val, temp, rh;

	if (attr == SENSOR_ATTR_LOWER_THRESH) {
		if (chan == SENSOR_CHAN_AMBIENT_TEMP) {
			data->t_low = sht3xd_temp_processed_to_raw(val);
		} else if (chan == SENSOR_CHAN_HUMIDITY) {
			data->rh_low = sht3xd_rh_processed_to_raw(val);
		} else {
			return -ENOTSUP;
		}

		set_cmd = SHT3XD_CMD_WRITE_TH_LOW_SET;
		clear_cmd = SHT3XD_CMD_WRITE_TH_LOW_CLEAR;
		temp = data->t_low;
		rh = data->rh_low;
	} else if (attr == SENSOR_ATTR_UPPER_THRESH) {
		if (chan == SENSOR_CHAN_AMBIENT_TEMP) {
			data->t_high = sht3xd_temp_processed_to_raw(val);
		} else if (chan == SENSOR_CHAN_HUMIDITY) {
			data->rh_high = sht3xd_rh_processed_to_raw(val);
		} else {
			return -ENOTSUP;
		}

		set_cmd = SHT3XD_CMD_WRITE_TH_HIGH_SET;
		clear_cmd = SHT3XD_CMD_WRITE_TH_HIGH_CLEAR;
		temp = data->t_high;
		rh = data->rh_high;
	} else {
		return -ENOTSUP;
	}

	reg_val = (rh & 0xFE00) | ((temp & 0xFF80) >> 7);

	if (sht3xd_write_reg(dev, set_cmd, reg_val) < 0 ||
	    sht3xd_write_reg(dev, clear_cmd, reg_val) < 0) {
		LOG_DBG("Failed to write threshold value!");
		return -EIO;
	}

	return 0;
}

static void sht3xd_gpio_callback(struct device *dev,
				 struct gpio_callback *cb, u32_t pins)
{
	struct sht3xd_data *data =
		CONTAINER_OF(cb, struct sht3xd_data, alert_cb);
	const struct sht3xd_config *cfg = data->dev->config->config_info;

	ARG_UNUSED(pins);

	gpio_pin_disable_callback(dev, cfg->alert_pin);

#if defined(CONFIG_SHT3XD_TRIGGER_OWN_THREAD)
	k_sem_give(&data->gpio_sem);
#elif defined(CONFIG_SHT3XD_TRIGGER_GLOBAL_THREAD)
	k_work_submit(&data->work);
#endif
}

static void sht3xd_thread_cb(void *arg)
{
	struct device *dev = arg;
	struct sht3xd_data *data = dev->driver_data;
	const struct sht3xd_config *cfg = dev->config->config_info;

	if (data->handler != NULL) {
		data->handler(dev, &data->trigger);
	}

	gpio_pin_enable_callback(data->alert_gpio, cfg->alert_pin);
}

#ifdef CONFIG_SHT3XD_TRIGGER_OWN_THREAD
static void sht3xd_thread(int dev_ptr, int unused)
{
	struct device *dev = INT_TO_POINTER(dev_ptr);
	struct sht3xd_data *data = dev->driver_data;

	ARG_UNUSED(unused);

	while (1) {
		k_sem_take(&data->gpio_sem, K_FOREVER);
		sht3xd_thread_cb(dev);
	}
}
#endif

#ifdef CONFIG_SHT3XD_TRIGGER_GLOBAL_THREAD
static void sht3xd_work_cb(struct k_work *work)
{
	struct sht3xd_data *data =
		CONTAINER_OF(work, struct sht3xd_data, work);

	sht3xd_thread_cb(data->dev);
}
#endif

int sht3xd_trigger_set(struct device *dev,
		       const struct sensor_trigger *trig,
		       sensor_trigger_handler_t handler)
{
	struct sht3xd_data *data = dev->driver_data;
	const struct sht3xd_config *cfg = dev->config->config_info;

	if (trig->type != SENSOR_TRIG_THRESHOLD) {
		return -ENOTSUP;
	}

	gpio_pin_disable_callback(data->alert_gpio, cfg->alert_pin);
	data->handler = handler;
	data->trigger = *trig;
	gpio_pin_enable_callback(data->alert_gpio, cfg->alert_pin);

	return 0;
}

int sht3xd_init_interrupt(struct device *dev)
{
	struct sht3xd_data *data = dev->driver_data;
	const struct sht3xd_config *cfg = dev->config->config_info;
	struct device *gpio = device_get_binding(cfg->alert_gpio_name);
	int rc;

	/* setup gpio interrupt */
	if (gpio == NULL) {
		LOG_DBG("Failed to get pointer to %s device!",
			cfg->alert_gpio_name);
		return -EINVAL;
	}
	data->alert_gpio = gpio;

	rc = gpio_pin_configure(gpio, cfg->alert_pin,
				GPIO_DIR_IN | GPIO_INT |
				GPIO_INT_EDGE | GPIO_INT_DOUBLE_EDGE |
				GPIO_INT_ACTIVE_HIGH | GPIO_INT_DEBOUNCE);
	if (rc != 0) {
		LOG_DBG("Failed to configure alert pin %u!", cfg->alert_pin);
		return -EIO;
	}

	gpio_init_callback(&data->alert_cb, sht3xd_gpio_callback,
			   BIT(cfg->alert_pin));
	rc = gpio_add_callback(gpio, &data->alert_cb);
	if (rc < 0) {
		LOG_DBG("Failed to set gpio callback!");
		return -EIO;
	}

	/* set alert thresholds to match reamsurement ranges */
	data->t_low = 0U;
	data->rh_low = 0U;
	data->t_high = 0xFFFF;
	data->rh_high = 0xFFFF;
	if (sht3xd_write_reg(dev, SHT3XD_CMD_WRITE_TH_HIGH_SET, 0xFFFF)
	    < 0) {
		LOG_DBG("Failed to write threshold high set value!");
		return -EIO;
	}

	if (sht3xd_write_reg(dev, SHT3XD_CMD_WRITE_TH_HIGH_CLEAR,
			     0xFFFF) < 0) {
		LOG_DBG("Failed to write threshold high clear value!");
		return -EIO;
	}

	if (sht3xd_write_reg(dev, SHT3XD_CMD_WRITE_TH_LOW_SET, 0) < 0) {
		LOG_DBG("Failed to write threshold low set value!");
		return -EIO;
	}

	if (sht3xd_write_reg(dev, SHT3XD_CMD_WRITE_TH_LOW_SET, 0) < 0) {
		LOG_DBG("Failed to write threshold low clear value!");
		return -EIO;
	}

#if defined(CONFIG_SHT3XD_TRIGGER_OWN_THREAD)
	k_sem_init(&data->gpio_sem, 0, UINT_MAX);

	k_thread_create(&data->thread, data->thread_stack,
			CONFIG_SHT3XD_THREAD_STACK_SIZE,
			(k_thread_entry_t)sht3xd_thread, dev,
			0, NULL, K_PRIO_COOP(CONFIG_SHT3XD_THREAD_PRIORITY),
			0, 0);
#elif defined(CONFIG_SHT3XD_TRIGGER_GLOBAL_THREAD)
	data->work.handler = sht3xd_work_cb;
#endif

	return 0;
}
