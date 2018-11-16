/*
 * Copyright (c) 2016 Freescale Semiconductor, Inc.
 * Copyright (c) 2018 Phytec Messtechnik GmbH
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "fxos8700.h"
#include <misc/util.h>
#include <misc/__assert.h>
#include <logging/log.h>
#include <stdlib.h>

#define LOG_LEVEL CONFIG_SENSOR_LOG_LEVEL
LOG_MODULE_REGISTER(FXOS8700);

int fxos8700_set_odr(struct device *dev, const struct sensor_value *val)
{
	const struct fxos8700_config *config = dev->config->config_info;
	struct fxos8700_data *data = dev->driver_data;
	s32_t dr = val->val1;

#ifdef CONFIG_FXOS8700_MODE_HYBRID
	/* ODR is halved in hybrid mode */
	if (val->val1 == 400 && val->val2 == 0) {
		dr = FXOS8700_CTRLREG1_DR_RATE_800;
	} else if (val->val1 == 200 && val->val2 == 0) {
		dr = FXOS8700_CTRLREG1_DR_RATE_400;
	} else if (val->val1 == 100 && val->val2 == 0) {
		dr = FXOS8700_CTRLREG1_DR_RATE_200;
	} else if (val->val1 == 50 && val->val2 == 0) {
		dr = FXOS8700_CTRLREG1_DR_RATE_100;
	} else if (val->val1 == 25 && val->val2 == 0) {
		dr = FXOS8700_CTRLREG1_DR_RATE_50;
	} else if (val->val1 == 6 && val->val2 == 250000) {
		dr = FXOS8700_CTRLREG1_DR_RATE_12_5;
	} else if (val->val1 == 3 && val->val2 == 125000) {
		dr = FXOS8700_CTRLREG1_DR_RATE_6_25;
	} else if (val->val1 == 0 && val->val2 == 781300) {
		dr = FXOS8700_CTRLREG1_DR_RATE_1_56;
	} else {
		return -EINVAL;
	}
#else
	if (val->val1 == 800 && val->val2 == 0) {
		dr = FXOS8700_CTRLREG1_DR_RATE_800;
	} else if (val->val1 == 400 && val->val2 == 0) {
		dr = FXOS8700_CTRLREG1_DR_RATE_400;
	} else if (val->val1 == 200 && val->val2 == 0) {
		dr = FXOS8700_CTRLREG1_DR_RATE_200;
	} else if (val->val1 == 100 && val->val2 == 0) {
		dr = FXOS8700_CTRLREG1_DR_RATE_100;
	} else if (val->val1 == 50 && val->val2 == 0) {
		dr = FXOS8700_CTRLREG1_DR_RATE_50;
	} else if (val->val1 == 12 && val->val2 == 500000) {
		dr = FXOS8700_CTRLREG1_DR_RATE_12_5;
	} else if (val->val1 == 6 && val->val2 == 250000) {
		dr = FXOS8700_CTRLREG1_DR_RATE_6_25;
	} else if (val->val1 == 1 && val->val2 == 562500) {
		dr = FXOS8700_CTRLREG1_DR_RATE_1_56;
	} else {
		return -EINVAL;
	}
#endif

	LOG_DBG("Set ODR to 0x%x", (u8_t)dr);

	return i2c_reg_update_byte(data->i2c, config->i2c_address,
				   FXOS8700_REG_CTRLREG1,
				   FXOS8700_CTRLREG1_DR_MASK, (u8_t)dr);
}

static int fxos8700_set_mt_ths(struct device *dev,
			       const struct sensor_value *val)
{
#ifdef CONFIG_FXOS8700_MOTION
	const struct fxos8700_config *config = dev->config->config_info;
	struct fxos8700_data *data = dev->driver_data;
	u64_t micro_ms2 = abs(val->val1 * 1000000LL + val->val2);
	u64_t ths = micro_ms2 / FXOS8700_FF_MT_THS_SCALE;

	if (ths > FXOS8700_FF_MT_THS_MASK) {
		LOG_ERR("Threshold value is out of range");
		return -EINVAL;
	}

	LOG_DBG("Set FF_MT_THS to %d", (u8_t)ths);

	return i2c_reg_update_byte(data->i2c, config->i2c_address,
				   FXOS8700_REG_FF_MT_THS,
				   FXOS8700_FF_MT_THS_MASK, (u8_t)ths);
#else
	return -ENOTSUP;
#endif
}

static int fxos8700_attr_set(struct device *dev,
			     enum sensor_channel chan,
			     enum sensor_attribute attr,
			     const struct sensor_value *val)
{
	if (chan != SENSOR_CHAN_ALL) {
		return -ENOTSUP;
	}

	if (attr == SENSOR_ATTR_SAMPLING_FREQUENCY) {
		return fxos8700_set_odr(dev, val);
	} else if (attr == SENSOR_ATTR_SLOPE_TH) {
		return fxos8700_set_mt_ths(dev, val);
	} else {
		return -ENOTSUP;
	}

	return 0;
}

static int fxos8700_sample_fetch(struct device *dev, enum sensor_channel chan)
{
	const struct fxos8700_config *config = dev->config->config_info;
	struct fxos8700_data *data = dev->driver_data;
	u8_t buffer[FXOS8700_MAX_NUM_BYTES];
	u8_t num_bytes;
	s16_t *raw;
	int ret = 0;
	int i;

	if (chan != SENSOR_CHAN_ALL) {
		LOG_ERR("Unsupported sensor channel");
		return -ENOTSUP;
	}

	k_sem_take(&data->sem, K_FOREVER);

	/* Read all the channels in one I2C transaction. The number of bytes to
	 * read and the starting register address depend on the mode
	 * configuration (accel-only, mag-only, or hybrid).
	 */
	num_bytes = config->num_channels * FXOS8700_BYTES_PER_CHANNEL_NORMAL;

	__ASSERT(num_bytes <= sizeof(buffer), "Too many bytes to read");

	if (i2c_burst_read(data->i2c, config->i2c_address, config->start_addr,
			   buffer, num_bytes)) {
		LOG_ERR("Could not fetch sample");
		ret = -EIO;
		goto exit;
	}

	/* Parse the buffer into raw channel data (16-bit integers). To save
	 * RAM, store the data in raw format and wait to convert to the
	 * normalized sensor_value type until later.
	 */
	__ASSERT(config->start_channel + config->num_channels
			<= ARRAY_SIZE(data->raw),
			"Too many channels");

	raw = &data->raw[config->start_channel];

	for (i = 0; i < num_bytes; i += 2) {
		*raw++ = (buffer[i] << 8) | (buffer[i+1]);
	}

#ifdef CONFIG_FXOS8700_TEMP
	if (i2c_reg_read_byte(data->i2c, config->i2c_address, FXOS8700_REG_TEMP,
			      &data->temp)) {
		LOG_ERR("Could not fetch temperature");
		ret = -EIO;
		goto exit;
	}
#endif

exit:
	k_sem_give(&data->sem);

	return ret;
}

static void fxos8700_accel_convert(struct sensor_value *val, s16_t raw,
				   enum fxos8700_range range)
{
	u8_t frac_bits;
	s64_t micro_ms2;

	/* The range encoding is convenient to compute the number of fractional
	 * bits:
	 * - 2g mode (range = 0) has 14 fractional bits
	 * - 4g mode (range = 1) has 13 fractional bits
	 * - 8g mode (range = 2) has 12 fractional bits
	 */
	frac_bits = 14 - range;

	/* Convert units to micro m/s^2. Intermediate results before the shift
	 * are 40 bits wide.
	 */
	micro_ms2 = (raw * SENSOR_G) >> frac_bits;

	/* The maximum possible value is 8g, which in units of micro m/s^2
	 * always fits into 32-bits. Cast down to s32_t so we can use a
	 * faster divide.
	 */
	val->val1 = (s32_t) micro_ms2 / 1000000;
	val->val2 = (s32_t) micro_ms2 % 1000000;
}

static void fxos8700_magn_convert(struct sensor_value *val, s16_t raw)
{
	s32_t micro_g;

	/* Convert units to micro Gauss. Raw magnetic data always has a
	 * resolution of 0.1 uT/LSB, which is equivalent to 0.001 G/LSB.
	 */
	micro_g = raw * 1000;

	val->val1 = micro_g / 1000000;
	val->val2 = micro_g % 1000000;
}

#ifdef CONFIG_FXOS8700_TEMP
static void fxos8700_temp_convert(struct sensor_value *val, s8_t raw)
{
	s32_t micro_c;

	/* Convert units to micro Celsius. Raw temperature data always has a
	 * resolution of 0.96 deg C/LSB.
	 */
	micro_c = raw * 960 * 1000;

	val->val1 = micro_c / 1000000;
	val->val2 = micro_c % 1000000;
}
#endif

static int fxos8700_channel_get(struct device *dev, enum sensor_channel chan,
				struct sensor_value *val)
{
	const struct fxos8700_config *config = dev->config->config_info;
	struct fxos8700_data *data = dev->driver_data;
	int start_channel;
	int num_channels;
	s16_t *raw;
	int ret;
	int i;

	k_sem_take(&data->sem, K_FOREVER);

	/* Start with an error return code by default, then clear it if we find
	 * a supported sensor channel.
	 */
	ret = -ENOTSUP;

	/* If we're in an accelerometer-enabled mode (accel-only or hybrid),
	 * then convert raw accelerometer data to the normalized sensor_value
	 * type.
	 */
	if (config->mode != FXOS8700_MODE_MAGN) {
		switch (chan) {
		case SENSOR_CHAN_ACCEL_X:
			start_channel = FXOS8700_CHANNEL_ACCEL_X;
			num_channels = 1;
			break;
		case SENSOR_CHAN_ACCEL_Y:
			start_channel = FXOS8700_CHANNEL_ACCEL_Y;
			num_channels = 1;
			break;
		case SENSOR_CHAN_ACCEL_Z:
			start_channel = FXOS8700_CHANNEL_ACCEL_Z;
			num_channels = 1;
			break;
		case SENSOR_CHAN_ACCEL_XYZ:
			start_channel = FXOS8700_CHANNEL_ACCEL_X;
			num_channels = 3;
			break;
		default:
			start_channel = 0;
			num_channels = 0;
			break;
		}

		raw = &data->raw[start_channel];
		for (i = 0; i < num_channels; i++) {
			fxos8700_accel_convert(val++, *raw++, config->range);
		}

		if (num_channels > 0) {
			ret = 0;
		}
	}

	/* If we're in an magnetometer-enabled mode (mag-only or hybrid), then
	 * convert raw magnetometer data to the normalized sensor_value type.
	 */
	if (config->mode != FXOS8700_MODE_ACCEL) {
		switch (chan) {
		case SENSOR_CHAN_MAGN_X:
			start_channel = FXOS8700_CHANNEL_MAGN_X;
			num_channels = 1;
			break;
		case SENSOR_CHAN_MAGN_Y:
			start_channel = FXOS8700_CHANNEL_MAGN_Y;
			num_channels = 1;
			break;
		case SENSOR_CHAN_MAGN_Z:
			start_channel = FXOS8700_CHANNEL_MAGN_Z;
			num_channels = 1;
			break;
		case SENSOR_CHAN_MAGN_XYZ:
			start_channel = FXOS8700_CHANNEL_MAGN_X;
			num_channels = 3;
			break;
		default:
			start_channel = 0;
			num_channels = 0;
			break;
		}

		raw = &data->raw[start_channel];
		for (i = 0; i < num_channels; i++) {
			fxos8700_magn_convert(val++, *raw++);
		}

		if (num_channels > 0) {
			ret = 0;
		}
#ifdef CONFIG_FXOS8700_TEMP
		if (chan == SENSOR_CHAN_DIE_TEMP) {
			fxos8700_temp_convert(val, data->temp);
			ret = 0;
		}
#endif
	}

	if (ret != 0) {
		LOG_ERR("Unsupported sensor channel");
	}

	k_sem_give(&data->sem);

	return ret;
}

int fxos8700_get_power(struct device *dev, enum fxos8700_power *power)
{
	const struct fxos8700_config *config = dev->config->config_info;
	struct fxos8700_data *data = dev->driver_data;
	u8_t val = *power;

	if (i2c_reg_read_byte(data->i2c, config->i2c_address,
			      FXOS8700_REG_CTRLREG1,
			      &val)) {
		LOG_ERR("Could not get power setting");
		return -EIO;
	}
	val &= FXOS8700_M_CTRLREG1_MODE_MASK;
	*power = val;

	return 0;
}

int fxos8700_set_power(struct device *dev, enum fxos8700_power power)
{
	const struct fxos8700_config *config = dev->config->config_info;
	struct fxos8700_data *data = dev->driver_data;

	return i2c_reg_update_byte(data->i2c, config->i2c_address,
				   FXOS8700_REG_CTRLREG1,
				   FXOS8700_CTRLREG1_ACTIVE_MASK,
				   power);
}

static int fxos8700_init(struct device *dev)
{
	const struct fxos8700_config *config = dev->config->config_info;
	struct fxos8700_data *data = dev->driver_data;
	struct sensor_value odr = {.val1 = 6, .val2 = 250000};

	/* Get the I2C device */
	data->i2c = device_get_binding(config->i2c_name);
	if (data->i2c == NULL) {
		LOG_ERR("Could not find I2C device");
		return -EINVAL;
	}

	/*
	 * Read the WHOAMI register to make sure we are talking to FXOS8700 or
	 * compatible device and not some other type of device that happens to
	 * have the same I2C address.
	 */
	if (i2c_reg_read_byte(data->i2c, config->i2c_address,
			      FXOS8700_REG_WHOAMI, &data->whoami)) {
		LOG_ERR("Could not get WHOAMI value");
		return -EIO;
	}

	switch (data->whoami) {
	case WHOAMI_ID_MMA8451:
	case WHOAMI_ID_MMA8652:
	case WHOAMI_ID_MMA8653:
		if (config->mode != FXOS8700_MODE_ACCEL) {
			LOG_ERR("Device 0x%x supports only "
				    "accelerometer mode",
				    data->whoami);
			return -EIO;
		}
		break;
	case WHOAMI_ID_FXOS8700:
		LOG_DBG("Device ID 0x%x", data->whoami);
		break;
	default:
		LOG_ERR("Unknown Device ID 0x%x", data->whoami);
		return -EIO;
	}

	/* Reset the sensor. Upon issuing a software reset command over the I2C
	 * interface, the sensor immediately resets and does not send any
	 * acknowledgment (ACK) of the written byte to the master. Therefore,
	 * do not check the return code of the I2C transaction.
	 */
	i2c_reg_write_byte(data->i2c, config->i2c_address,
			   FXOS8700_REG_CTRLREG2, FXOS8700_CTRLREG2_RST_MASK);

	/* The sensor requires us to wait 1 ms after a software reset before
	 * attempting further communications.
	 */
	k_busy_wait(USEC_PER_MSEC);

	if (fxos8700_set_odr(dev, &odr)) {
		LOG_ERR("Could not set default data rate");
		return -EIO;
	}

	if (i2c_reg_update_byte(data->i2c, config->i2c_address,
				FXOS8700_REG_CTRLREG2,
				FXOS8700_CTRLREG2_MODS_MASK,
				config->power_mode)) {
		LOG_ERR("Could not set power scheme");
		return -EIO;
	}

	/* Set the mode (accel-only, mag-only, or hybrid) */
	if (i2c_reg_update_byte(data->i2c, config->i2c_address,
				FXOS8700_REG_M_CTRLREG1,
				FXOS8700_M_CTRLREG1_MODE_MASK,
				config->mode)) {
		LOG_ERR("Could not set mode");
		return -EIO;
	}

	/* Set hybrid autoincrement so we can read accel and mag channels in
	 * one I2C transaction.
	 */
	if (i2c_reg_update_byte(data->i2c, config->i2c_address,
				FXOS8700_REG_M_CTRLREG2,
				FXOS8700_M_CTRLREG2_AUTOINC_MASK,
				FXOS8700_M_CTRLREG2_AUTOINC_MASK)) {
		LOG_ERR("Could not set hybrid autoincrement");
		return -EIO;
	}

	/* Set the full-scale range */
	if (i2c_reg_update_byte(data->i2c, config->i2c_address,
				FXOS8700_REG_XYZ_DATA_CFG,
				FXOS8700_XYZ_DATA_CFG_FS_MASK,
				config->range)) {
		LOG_ERR("Could not set range");
		return -EIO;
	}

	k_sem_init(&data->sem, 0, UINT_MAX);

#if CONFIG_FXOS8700_TRIGGER
	if (fxos8700_trigger_init(dev)) {
		LOG_ERR("Could not initialize interrupts");
		return -EIO;
	}
#endif

	/* Set active */
	if (fxos8700_set_power(dev, FXOS8700_POWER_ACTIVE)) {
		LOG_ERR("Could not set active");
		return -EIO;
	}
	k_sem_give(&data->sem);

	LOG_DBG("Init complete");

	return 0;
}

static const struct sensor_driver_api fxos8700_driver_api = {
	.sample_fetch = fxos8700_sample_fetch,
	.channel_get = fxos8700_channel_get,
	.attr_set = fxos8700_attr_set,
#if CONFIG_FXOS8700_TRIGGER
	.trigger_set = fxos8700_trigger_set,
#endif
};

static const struct fxos8700_config fxos8700_config = {
	.i2c_name = DT_FXOS8700_I2C_NAME,
	.i2c_address = DT_FXOS8700_I2C_ADDRESS,
#ifdef CONFIG_FXOS8700_MODE_ACCEL
	.mode = FXOS8700_MODE_ACCEL,
	.start_addr = FXOS8700_REG_OUTXMSB,
	.start_channel = FXOS8700_CHANNEL_ACCEL_X,
	.num_channels = FXOS8700_NUM_ACCEL_CHANNELS,
#elif CONFIG_FXOS8700_MODE_MAGN
	.mode = FXOS8700_MODE_MAGN,
	.start_addr = FXOS8700_REG_M_OUTXMSB,
	.start_channel = FXOS8700_CHANNEL_MAGN_X,
	.num_channels = FXOS8700_NUM_MAG_CHANNELS,
#else
	.mode = FXOS8700_MODE_HYBRID,
	.start_addr = FXOS8700_REG_OUTXMSB,
	.start_channel = FXOS8700_CHANNEL_ACCEL_X,
	.num_channels = FXOS8700_NUM_HYBRID_CHANNELS,
#endif
#if CONFIG_FXOS8700_PM_NORMAL
	.power_mode = FXOS8700_PM_NORMAL,
#elif CONFIG_FXOS8700_PM_LOW_NOISE_LOW_POWER
	.power_mode = FXOS8700_PM_LOW_NOISE_LOW_POWER,
#elif CONFIG_FXOS8700_PM_HIGH_RESOLUTION
	.power_mode = FXOS8700_PM_HIGH_RESOLUTION,
#else
	.power_mode = FXOS8700_PM_LOW_POWER,
#endif
#if CONFIG_FXOS8700_RANGE_8G
	.range = FXOS8700_RANGE_8G,
#elif CONFIG_FXOS8700_RANGE_4G
	.range = FXOS8700_RANGE_4G,
#else
	.range = FXOS8700_RANGE_2G,
#endif
#ifdef CONFIG_FXOS8700_TRIGGER
	.gpio_name = DT_FXOS8700_GPIO_NAME,
	.gpio_pin = DT_FXOS8700_GPIO_PIN,
#endif
#ifdef CONFIG_FXOS8700_PULSE
	.pulse_cfg = CONFIG_FXOS8700_PULSE_CFG,
	.pulse_ths[0] = CONFIG_FXOS8700_PULSE_THSX,
	.pulse_ths[1] = CONFIG_FXOS8700_PULSE_THSY,
	.pulse_ths[2] = CONFIG_FXOS8700_PULSE_THSZ,
	.pulse_tmlt = CONFIG_FXOS8700_PULSE_TMLT,
	.pulse_ltcy = CONFIG_FXOS8700_PULSE_LTCY,
	.pulse_wind = CONFIG_FXOS8700_PULSE_WIND,
#endif
};

static struct fxos8700_data fxos8700_data;

DEVICE_AND_API_INIT(fxos8700, DT_FXOS8700_NAME, fxos8700_init,
		    &fxos8700_data, &fxos8700_config,
		    POST_KERNEL, CONFIG_SENSOR_INIT_PRIORITY,
		    &fxos8700_driver_api);
