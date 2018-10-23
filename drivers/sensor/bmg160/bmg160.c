/* Bosch BMG160 gyro driver
 *
 * Copyright (c) 2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Datasheet:
 * http://ae-bst.resource.bosch.com/media/_tech/media/datasheets/BST-BMG160-DS000-09.pdf
 */

#include <init.h>
#include <sensor.h>
#include <misc/byteorder.h>
#include <kernel.h>
#include <logging/log.h>

#include "bmg160.h"

#define LOG_LEVEL CONFIG_SENSOR_LOG_LEVEL
LOG_MODULE_REGISTER(BMG160);

struct bmg160_device_data bmg160_data;

static inline int bmg160_bus_config(struct device *dev)
{
	const struct bmg160_device_config *dev_cfg = dev->config->config_info;
	struct bmg160_device_data *bmg160 = dev->driver_data;
	u32_t i2c_cfg;

	i2c_cfg = I2C_MODE_MASTER | I2C_SPEED_SET(dev_cfg->i2c_speed);

	return i2c_configure(bmg160->i2c, i2c_cfg);
}

int bmg160_read(struct device *dev, u8_t reg_addr, u8_t *data,
		u8_t len)
{
	const struct bmg160_device_config *dev_cfg = dev->config->config_info;
	struct bmg160_device_data *bmg160 = dev->driver_data;
	int ret = 0;

	bmg160_bus_config(dev);

	k_sem_take(&bmg160->sem, K_FOREVER);

	if (i2c_burst_read(bmg160->i2c, dev_cfg->i2c_addr,
			   reg_addr, data, len) < 0) {
		ret = -EIO;
	}

	k_sem_give(&bmg160->sem);

	return ret;
}

int bmg160_read_byte(struct device *dev, u8_t reg_addr, u8_t *byte)
{
	return bmg160_read(dev, reg_addr, byte, 1);
}

static int bmg160_write(struct device *dev, u8_t reg_addr, u8_t *data,
			u8_t len)
{
	const struct bmg160_device_config *dev_cfg = dev->config->config_info;
	struct bmg160_device_data *bmg160 = dev->driver_data;
	int ret = 0;

	bmg160_bus_config(dev);

	k_sem_take(&bmg160->sem, K_FOREVER);

	if (i2c_burst_write(bmg160->i2c, dev_cfg->i2c_addr,
			    reg_addr, data, len) < 0) {
		ret = -EIO;
	}

	k_sem_give(&bmg160->sem);

	return ret;
}

int bmg160_write_byte(struct device *dev, u8_t reg_addr, u8_t byte)
{
	return bmg160_write(dev, reg_addr, &byte, 1);
}

int bmg160_update_byte(struct device *dev, u8_t reg_addr, u8_t mask,
		       u8_t value)
{
	const struct bmg160_device_config *dev_cfg = dev->config->config_info;
	struct bmg160_device_data *bmg160 = dev->driver_data;
	int ret = 0;

	bmg160_bus_config(dev);

	k_sem_take(&bmg160->sem, K_FOREVER);

	if (i2c_reg_update_byte(bmg160->i2c, dev_cfg->i2c_addr,
				reg_addr, mask, value) < 0) {
		ret = -EIO;
	}

	k_sem_give(&bmg160->sem);

	return ret;
}

/* Allowed range values, in degrees/sec. */
static const s16_t bmg160_gyro_range_map[] = {2000, 1000, 500, 250, 125};
#define BMG160_GYRO_RANGE_MAP_SIZE	ARRAY_SIZE(bmg160_gyro_range_map)

/* Allowed sampling frequencies, in Hz */
static const s16_t bmg160_sampling_freq_map[] = {2000, 1000, 400, 200, 100};
#define BMG160_SAMPLING_FREQ_MAP_SIZE	ARRAY_SIZE(bmg160_sampling_freq_map)

static int bmg160_is_val_valid(s16_t val, const s16_t *val_map,
			       u16_t map_size)
{
	int i;

	for (i = 0; i < map_size; i++) {
		if (val == val_map[i]) {
			return i;
		}
	}

	return -1;
}

static int bmg160_attr_set(struct device *dev, enum sensor_channel chan,
			   enum sensor_attribute attr,
			   const struct sensor_value *val)
{
	struct bmg160_device_data *bmg160 = dev->driver_data;
	int idx;
	u16_t range_dps;

	if (chan != SENSOR_CHAN_GYRO_XYZ) {
		return -ENOTSUP;
	}

	switch (attr) {
	case SENSOR_ATTR_FULL_SCALE:
		range_dps = sensor_rad_to_degrees(val);

		idx = bmg160_is_val_valid(range_dps,
					  bmg160_gyro_range_map,
					  BMG160_GYRO_RANGE_MAP_SIZE);
		if (idx < 0) {
			return -ENOTSUP;
		}

		if (bmg160_write_byte(dev, BMG160_REG_RANGE, idx) < 0) {
			return -EIO;
		}

		bmg160->scale = BMG160_RANGE_TO_SCALE(range_dps);

		return 0;

	case SENSOR_ATTR_SAMPLING_FREQUENCY:
		idx = bmg160_is_val_valid(val->val1,
					  bmg160_sampling_freq_map,
					  BMG160_SAMPLING_FREQ_MAP_SIZE);
		if (idx < 0) {
			return -ENOTSUP;
		}

		/*
		 * The sampling frequencies values start at 1, i.e. a
		 * sampling frequency of 2000Hz translates to BW value
		 * of 1. Hence the 1 added to the index received.
		 */
		if (bmg160_write_byte(dev, BMG160_REG_BW, idx + 1) < 0) {
			return -EIO;
		}

		return 0;

#ifdef CONFIG_BMG160_TRIGGER
	case SENSOR_ATTR_SLOPE_TH:
	case SENSOR_ATTR_SLOPE_DUR:
		return bmg160_slope_config(dev, attr, val);
#endif
	default:
		return -ENOTSUP;
	}
}

static int bmg160_sample_fetch(struct device *dev, enum sensor_channel chan)
{
	struct bmg160_device_data *bmg160 = dev->driver_data;
	union {
		u8_t raw[7];
		struct {
			u16_t x_axis;
			u16_t y_axis;
			u16_t z_axis;
			u8_t temp;
		};
	} buf __aligned(2);

	/* do a burst read, to fetch all axis data */
	if (bmg160_read(dev, BMG160_REG_RATE_X, buf.raw, sizeof(buf)) < 0) {
		return -EIO;
	}

	bmg160->raw_gyro_xyz[0] = sys_le16_to_cpu(buf.x_axis);
	bmg160->raw_gyro_xyz[1] = sys_le16_to_cpu(buf.y_axis);
	bmg160->raw_gyro_xyz[2] = sys_le16_to_cpu(buf.z_axis);
	bmg160->raw_temp	= buf.temp;

	return 0;
}

static void bmg160_to_fixed_point(struct bmg160_device_data *bmg160,
				  enum sensor_channel chan, s16_t raw,
				  struct sensor_value *val)
{
	if (chan == SENSOR_CHAN_DIE_TEMP) {
		val->val1 = 23 + (raw / 2);
		val->val2 = (raw % 2) * 500000;
	} else {
		s32_t converted_val = raw * bmg160->scale;

		val->val1 = converted_val / 1000000;
		val->val2 = converted_val % 1000000;
	}
}

static int bmg160_channel_get(struct device *dev, enum sensor_channel chan,
			      struct sensor_value *val)
{
	struct bmg160_device_data *bmg160 = dev->driver_data;
	s16_t raw_val;
	int i;

	switch (chan) {
	case SENSOR_CHAN_GYRO_X:
	case SENSOR_CHAN_GYRO_Y:
	case SENSOR_CHAN_GYRO_Z:
		raw_val = bmg160->raw_gyro_xyz[chan - SENSOR_CHAN_GYRO_X];
		bmg160_to_fixed_point(bmg160, chan, raw_val, val);
		return 0;

	case SENSOR_CHAN_GYRO_XYZ:
		/* return all channel values, in one read */
		for (i = 0; i < 3; i++, val++) {
			raw_val = bmg160->raw_gyro_xyz[i];
			bmg160_to_fixed_point(bmg160, chan, raw_val, val);
		}

		return 0;

	case SENSOR_CHAN_DIE_TEMP:
		bmg160_to_fixed_point(bmg160, chan, bmg160->raw_temp, val);
		return 0;

	default:
		return -ENOTSUP;
	}
}

static const struct sensor_driver_api bmg160_api = {
	.attr_set = bmg160_attr_set,
#ifdef CONFIG_BMG160_TRIGGER
	.trigger_set = bmg160_trigger_set,
#endif
	.sample_fetch = bmg160_sample_fetch,
	.channel_get = bmg160_channel_get,
};

int bmg160_init(struct device *dev)
{
	const struct bmg160_device_config *cfg = dev->config->config_info;
	struct bmg160_device_data *bmg160 = dev->driver_data;
	u8_t chip_id = 0;
	u16_t range_dps;

	bmg160->i2c = device_get_binding((char *)cfg->i2c_port);
	if (!bmg160->i2c) {
		LOG_DBG("I2C master controller not found!");
		return -EINVAL;
	}

	k_sem_init(&bmg160->sem, 1, UINT_MAX);

	if (bmg160_read_byte(dev, BMG160_REG_CHIPID, &chip_id) < 0) {
		LOG_DBG("Failed to read chip id.");
		return -EIO;
	}

	if (chip_id != BMG160_CHIP_ID) {
		LOG_DBG("Unsupported chip detected (0x%x)!", chip_id);
		return -ENODEV;
	}

	/* reset the chip */
	bmg160_write_byte(dev, BMG160_REG_BGW_SOFTRESET, BMG160_RESET);

	k_busy_wait(1000); /* wait for the chip to come up */

	if (bmg160_write_byte(dev, BMG160_REG_RANGE,
			      BMG160_DEFAULT_RANGE) < 0) {
		LOG_DBG("Failed to set range.");
		return -EIO;
	}

	range_dps = bmg160_gyro_range_map[BMG160_DEFAULT_RANGE];

	bmg160->scale = BMG160_RANGE_TO_SCALE(range_dps);

	if (bmg160_write_byte(dev, BMG160_REG_BW, BMG160_DEFAULT_ODR) < 0) {
		LOG_DBG("Failed to set sampling frequency.");
		return -EIO;
	}

	/* disable interrupts */
	if (bmg160_write_byte(dev, BMG160_REG_INT_EN0, 0) < 0) {
		LOG_DBG("Failed to disable all interrupts.");
		return -EIO;
	}

#ifdef CONFIG_BMG160_TRIGGER
	bmg160_trigger_init(dev);
#endif

	return 0;
}

const struct bmg160_device_config bmg160_config = {
	.i2c_port = CONFIG_BMG160_I2C_PORT_NAME,
	.i2c_addr = CONFIG_BMG160_I2C_ADDR,
	.i2c_speed = BMG160_BUS_SPEED,
#ifdef CONFIG_BMG160_TRIGGER
	.gpio_port = CONFIG_BMG160_GPIO_PORT_NAME,
	.int_pin = CONFIG_BMG160_INT_PIN,
#endif
};

DEVICE_AND_API_INIT(bmg160, CONFIG_BMG160_DRV_NAME, bmg160_init, &bmg160_data,
		    &bmg160_config, POST_KERNEL, CONFIG_SENSOR_INIT_PRIORITY,
		    &bmg160_api);
