/*
 * Copyright (c) 2016 Firmwave
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <device.h>
#include <i2c.h>
#include <misc/byteorder.h>
#include <misc/util.h>
#include <kernel.h>
#include <sensor.h>
#include <misc/__assert.h>
#include <logging/log.h>

#define LOG_LEVEL CONFIG_SENSOR_LOG_LEVEL
LOG_MODULE_REGISTER(TMP112);

#define TMP112_I2C_ADDRESS		CONFIG_TMP112_I2C_ADDR

#define TMP112_REG_TEMPERATURE		0x00
#define TMP112_D0_BIT			BIT(0)

#define TMP112_REG_CONFIG		0x01
#define TMP112_EM_BIT			BIT(4)
#define TMP112_CR0_BIT			BIT(6)
#define TMP112_CR1_BIT			BIT(7)

/* scale in micro degrees Celsius */
#define TMP112_TEMP_SCALE		62500

struct tmp112_data {
	struct device *i2c;
	s16_t sample;
};

static int tmp112_reg_read(struct tmp112_data *drv_data,
			   u8_t reg, u16_t *val)
{
	if (i2c_burst_read(drv_data->i2c, TMP112_I2C_ADDRESS,
			   reg, (u8_t *) val, 2) < 0) {
		return -EIO;
	}

	*val = sys_be16_to_cpu(*val);

	return 0;
}

static int tmp112_reg_write(struct tmp112_data *drv_data,
			    u8_t reg, u16_t val)
{
	u16_t val_be = sys_cpu_to_be16(val);

	return i2c_burst_write(drv_data->i2c, TMP112_I2C_ADDRESS,
			       reg, (u8_t *)&val_be, 2);
}

static int tmp112_reg_update(struct tmp112_data *drv_data, u8_t reg,
			     u16_t mask, u16_t val)
{
	u16_t old_val = 0;
	u16_t new_val;

	if (tmp112_reg_read(drv_data, reg, &old_val) < 0) {
		return -EIO;
	}

	new_val = old_val & ~mask;
	new_val |= val & mask;

	return tmp112_reg_write(drv_data, reg, new_val);
}

static int tmp112_attr_set(struct device *dev,
			   enum sensor_channel chan,
			   enum sensor_attribute attr,
			   const struct sensor_value *val)
{
	struct tmp112_data *drv_data = dev->driver_data;
	s64_t value;
	u16_t cr;

	if (chan != SENSOR_CHAN_AMBIENT_TEMP) {
		return -ENOTSUP;
	}

	switch (attr) {
	case SENSOR_ATTR_FULL_SCALE:
		/* the sensor supports two ranges -55 to 128 and -55 to 150 */
		/* the value contains the upper limit */
		if (val->val1 == 128) {
			value = 0x0000;
		} else if (val->val1 == 150) {
			value = TMP112_EM_BIT;
		} else {
			return -ENOTSUP;
		}

		if (tmp112_reg_update(drv_data, TMP112_REG_CONFIG,
				      TMP112_EM_BIT, value) < 0) {
			LOG_DBG("Failed to set attribute!");
			return -EIO;
		}

		return 0;

	case SENSOR_ATTR_SAMPLING_FREQUENCY:
		/* conversion rate in mHz */
		cr = val->val1 * 1000 + val->val2 / 1000;

		/* the sensor supports 0.25Hz, 1Hz, 4Hz and 8Hz */
		/* conversion rate */
		switch (cr) {
		case 250:
			value = 0x0000;
			break;

		case 1000:
			value = TMP112_CR0_BIT;
			break;

		case 4000:
			value = TMP112_CR1_BIT;
			break;

		case 8000:
			value = TMP112_CR0_BIT | TMP112_CR1_BIT;
			break;

		default:
			return -ENOTSUP;
		}

		if (tmp112_reg_update(drv_data, TMP112_REG_CONFIG,
				      TMP112_CR0_BIT | TMP112_CR1_BIT,
				      value) < 0) {
			LOG_DBG("Failed to set attribute!");
			return -EIO;
		}

		return 0;

	default:
		return -ENOTSUP;
	}

	return 0;
}

static int tmp112_sample_fetch(struct device *dev, enum sensor_channel chan)
{
	struct tmp112_data *drv_data = dev->driver_data;
	u16_t val;

	__ASSERT_NO_MSG(chan == SENSOR_CHAN_ALL || chan == SENSOR_CHAN_AMBIENT_TEMP);

	if (tmp112_reg_read(drv_data, TMP112_REG_TEMPERATURE, &val) < 0) {
		return -EIO;
	}

	if (val & TMP112_D0_BIT) {
		drv_data->sample = arithmetic_shift_right((s16_t)val, 3);
	} else {
		drv_data->sample = arithmetic_shift_right((s16_t)val, 4);
	}

	return 0;
}

static int tmp112_channel_get(struct device *dev,
		enum sensor_channel chan,
		struct sensor_value *val)
{
	struct tmp112_data *drv_data = dev->driver_data;
	s32_t uval;

	if (chan != SENSOR_CHAN_AMBIENT_TEMP) {
		return -ENOTSUP;
	}

	uval = (s32_t)drv_data->sample * TMP112_TEMP_SCALE;
	val->val1 = uval / 1000000;
	val->val2 = uval % 1000000;

	return 0;
}

static const struct sensor_driver_api tmp112_driver_api = {
	.attr_set = tmp112_attr_set,
	.sample_fetch = tmp112_sample_fetch,
	.channel_get = tmp112_channel_get,
};

int tmp112_init(struct device *dev)
{
	struct tmp112_data *drv_data = dev->driver_data;

	drv_data->i2c = device_get_binding(CONFIG_TMP112_I2C_MASTER_DEV_NAME);
	if (drv_data->i2c == NULL) {
		LOG_DBG("Failed to get pointer to %s device!",
			    CONFIG_TMP112_I2C_MASTER_DEV_NAME);
		return -EINVAL;
	}

	dev->driver_api = &tmp112_driver_api;

	return 0;
}

static struct tmp112_data tmp112_driver;

DEVICE_INIT(tmp112, CONFIG_TMP112_NAME, tmp112_init, &tmp112_driver,
	    NULL, POST_KERNEL, CONFIG_SENSOR_INIT_PRIORITY);
