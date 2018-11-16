/* Driver for MS5837 pressure sensor
 *
 * Copyright (c) 2018 Jan Van Winkel <jan.van_winkel@dxplore.eu>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <init.h>
#include <kernel.h>
#include <misc/byteorder.h>
#include <sensor.h>
#include <misc/__assert.h>

#include "ms5837.h"

#define LOG_LEVEL CONFIG_SENSOR_LOG_LEVEL
#include <logging/log.h>
LOG_MODULE_REGISTER(MS5837);

static int ms5837_get_measurement(struct device *i2c_master,
				  const u8_t i2c_address, u32_t *val,
				  u8_t cmd, const u8_t delay)
{
	u8_t adc_read_cmd = MS5837_CMD_CONV_READ_ADC;
	int err;

	*val = 0;

	err = i2c_write(i2c_master, &cmd, 1, i2c_address);
	if (err < 0) {
		return err;
	}

	k_sleep(delay);

	err = i2c_burst_read(i2c_master, i2c_address, adc_read_cmd,
			((u8_t *)val) + 1, 3);
	if (err < 0) {
		return err;
	}

	*val = sys_be32_to_cpu(*val);

	return 0;
}

static void ms5837_compensate(struct ms5837_data *data,
			      const s32_t adc_temperature,
			      const s32_t adc_pressure)
{
	s64_t dT;
	s64_t OFF;
	s64_t SENS;
	s64_t temp_sq;
	s32_t Ti;
	s32_t OFFi;
	s32_t SENSi;

	/* first order compensation as per datasheet
	 * (https://www.te.com/usa-en/product-CAT-BLPS0017.html) section
	 * PRESSURE AND TEMPERATURE CALCULATION
	 */

	dT = adc_temperature - ((u32_t)(data->t_ref) << 8);
	data->temperature = 2000 + (dT * data->tempsens) / (1ll << 23);
	OFF = ((s64_t)(data->off_t1) << 16) + (dT * data->tco) / (1ll << 7);
	SENS = ((s64_t)(data->sens_t1) << 15) + (dT * data->tcs) / (1ll << 8);

	/* Second order compensation as per datasheet
	 * (https://www.te.com/usa-en/product-CAT-BLPS0017.html) section
	 * SECOND ORDER TEMPERATURE COMPENSATION
	 */

	temp_sq = (data->temperature - 2000) * (data->temperature - 2000);
	if (data->temperature < 2000) {
		Ti = (3ll * dT * dT) / (1ll << 23);
		OFFi = (3ll * temp_sq) / 1ll;
		SENSi = (5ll * temp_sq) / (1ll << 3);
		if (data->temperature < -1500) {
			temp_sq = (data->temperature + 1500) *
				  (data->temperature + 1500);
			OFFi += 7ll * temp_sq;
			SENSi += 5ll * temp_sq;
		}
	} else {
		Ti = (1ll * dT * dT) / (1ll << 37);
		OFFi = temp_sq / (1ll << 4);
		SENSi = 0;
	}

	OFF -= OFFi;
	SENS -= SENSi;

	data->temperature -= Ti;
	data->pressure =
	    (((SENS * adc_pressure) / (1ll << 21)) - OFF) / (1ll << 13);
}

static int ms5837_sample_fetch(struct device *dev, enum sensor_channel channel)
{
	struct ms5837_data *data = dev->driver_data;
	const struct ms5837_config *cfg = dev->config->config_info;
	int err;
	u32_t adc_pressure;
	u32_t adc_temperature;

	__ASSERT_NO_MSG(channel == SENSOR_CHAN_ALL);

	err = ms5837_get_measurement(data->i2c_master, cfg->i2c_address,
				    &adc_pressure,
				    data->presure_conv_cmd,
				    data->presure_conv_delay);
	if (err < 0) {
		return err;
	}

	err = ms5837_get_measurement(data->i2c_master, cfg->i2c_address,
				     &adc_temperature,
				     data->temperature_conv_cmd,
				     data->temperature_conv_delay);
	if (err < 0) {
		return err;
	}

	ms5837_compensate(data, adc_temperature, adc_pressure);

	return 0;
}

static int ms5837_channel_get(struct device *dev, enum sensor_channel chan,
			      struct sensor_value *val)
{
	struct ms5837_data *data = dev->driver_data;

	switch (chan) {
	case SENSOR_CHAN_AMBIENT_TEMP:
		val->val1 = data->temperature / 100;
		val->val2 = data->temperature % 100 * 10000;
		break;
	case SENSOR_CHAN_PRESS:
		val->val1 = data->pressure / 100;
		val->val2 = data->pressure % 100 * 10000;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int ms5837_attr_set(struct device *dev, enum sensor_channel chan,
			   enum sensor_attribute attr,
			   const struct sensor_value *val)
{
	struct ms5837_data *data = dev->driver_data;
	u8_t p_conv_cmd;
	u8_t t_conv_cmd;
	u8_t conv_delay;

	if (attr == SENSOR_ATTR_OVERSAMPLING) {

		switch (val->val1) {
		case 8192:
			p_conv_cmd = MS5837_CMD_CONV_P_8192;
			t_conv_cmd = MS5837_CMD_CONV_T_8192;
			conv_delay = 19;
			break;
		case 4096:
			p_conv_cmd = MS5837_CMD_CONV_P_4096;
			t_conv_cmd = MS5837_CMD_CONV_T_4096;
			conv_delay = 10;
			break;
		case 2048:
			p_conv_cmd = MS5837_CMD_CONV_P_2048;
			t_conv_cmd = MS5837_CMD_CONV_T_2048;
			conv_delay = 5;
			break;
		case 1024:
			p_conv_cmd = MS5837_CMD_CONV_P_1024;
			t_conv_cmd = MS5837_CMD_CONV_T_1024;
			conv_delay = 3;
			break;
		case 512:
			p_conv_cmd = MS5837_CMD_CONV_P_512;
			t_conv_cmd = MS5837_CMD_CONV_T_512;
			conv_delay = 2;
			break;
		case 256:
			p_conv_cmd = MS5837_CMD_CONV_P_256;
			t_conv_cmd = MS5837_CMD_CONV_T_256;
			conv_delay = 1;
			break;
		default:
			LOG_ERR("invalid oversampling rate %d", val->val1);
			return -EINVAL;
		}

		if (chan == SENSOR_CHAN_ALL) {
			data->presure_conv_cmd = p_conv_cmd;
			data->presure_conv_delay = conv_delay;
			data->temperature_conv_cmd = t_conv_cmd;
			data->temperature_conv_delay = conv_delay;
			return 0;
		}

		if (chan == SENSOR_CHAN_PRESS) {
			data->presure_conv_cmd = p_conv_cmd;
			data->presure_conv_delay = conv_delay;
			return 0;
		}

		if (chan == SENSOR_CHAN_AMBIENT_TEMP) {
			data->temperature_conv_cmd = t_conv_cmd;
			data->temperature_conv_delay = conv_delay;
			return 0;
		}

		return -ENOTSUP;
	}

	return -ENOTSUP;
}

static const struct sensor_driver_api ms5837_api_funcs = {
	.attr_set = ms5837_attr_set,
	.sample_fetch = ms5837_sample_fetch,
	.channel_get = ms5837_channel_get,
};

static int ms5837_read_prom(struct device *i2c_master, const u8_t i2c_address,
		const u8_t cmd, u16_t *val)
{
	int err;

	err = i2c_burst_read(i2c_master, i2c_address, cmd, (u8_t *)val, 2);
	if (err < 0) {
		return err;
	}

	*val = sys_be16_to_cpu(*val);

	return 0;
}

static int ms5837_init(struct device *dev)
{
	struct ms5837_data *data = dev->driver_data;
	const struct ms5837_config *cfg = dev->config->config_info;
	int err;
	u8_t cmd;

	data->pressure = 0;
	data->temperature = 0;

	data->presure_conv_cmd = MS5837_CMD_CONV_P_256;
	data->presure_conv_delay = 1;
	data->temperature_conv_cmd = MS5837_CMD_CONV_T_256;
	data->temperature_conv_delay = 1;

	data->i2c_master = device_get_binding(cfg->i2c_name);
	if (data->i2c_master == NULL) {
		LOG_ERR("i2c master %s not found",
			    DT_MS5837_I2C_MASTER_DEV_NAME);
		return -EINVAL;
	}

	cmd = MS5837_CMD_RESET;
	err = i2c_write(data->i2c_master, &cmd, 1, cfg->i2c_address);
	if (err < 0) {
		return err;
	}

	err = ms5837_read_prom(data->i2c_master, cfg->i2c_address,
			       MS5837_CMD_CONV_READ_SENS_T1,
			       &data->sens_t1);
	if (err < 0) {
		return err;
	}

	err = ms5837_read_prom(data->i2c_master, cfg->i2c_address,
			       MS5837_CMD_CONV_READ_OFF_T1,
			       &data->off_t1);
	if (err < 0) {
		return err;
	}

	err = ms5837_read_prom(data->i2c_master, cfg->i2c_address,
			       MS5837_CMD_CONV_READ_TCS,
			       &data->tcs);
	if (err < 0) {
		return err;
	}

	err = ms5837_read_prom(data->i2c_master, cfg->i2c_address,
			       MS5837_CMD_CONV_READ_TCO,
			       &data->tco);
	if (err < 0) {
		return err;
	}

	err = ms5837_read_prom(data->i2c_master, cfg->i2c_address,
			       MS5837_CMD_CONV_READ_T_REF,
			       &data->t_ref);
	if (err < 0) {
		return err;
	}

	err = ms5837_read_prom(data->i2c_master, cfg->i2c_address,
			       MS5837_CMD_CONV_READ_TEMPSENS,
			       &data->tempsens);
	if (err < 0) {
		return err;
	}

	return 0;
}

static struct ms5837_data ms5837_data;

static const struct ms5837_config ms5837_config = {
	.i2c_name = DT_MS5837_I2C_MASTER_DEV_NAME,
	.i2c_address = MS5837_ADDR
};

DEVICE_AND_API_INIT(ms5837, DT_MS5837_DEV_NAME, ms5837_init, &ms5837_data,
		    &ms5837_config, POST_KERNEL, CONFIG_SENSOR_INIT_PRIORITY,
		    &ms5837_api_funcs);
