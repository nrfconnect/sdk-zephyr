/*
 * Copyright (c) 2018 Alexander Wachter.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <device.h>
#include <i2c.h>
#include <kernel.h>
#include <misc/byteorder.h>
#include <misc/util.h>
#include <sensor.h>
#include <misc/__assert.h>
#include <logging/log.h>

#include "iAQcore.h"

#define LOG_LEVEL CONFIG_SENSOR_LOG_LEVEL
LOG_MODULE_REGISTER(IAQ_CORE);

static int iaqcore_sample_fetch(struct device *dev, enum sensor_channel chan)
{
	struct iaq_core_data *drv_data = dev->driver_data;
	struct iaq_registers buf;
	struct i2c_msg msg;
	int ret, tries;

	__ASSERT_NO_MSG(chan == SENSOR_CHAN_ALL);

	msg.buf = (u8_t *)&buf;
	msg.len = sizeof(struct iaq_registers);
	msg.flags = I2C_MSG_READ | I2C_MSG_STOP;

	for (tries = 0; tries < CONFIG_IAQ_CORE_MAX_READ_RETRIES; tries++) {

		ret = i2c_transfer(drv_data->i2c, &msg, 1,
				   DT_AMS_IAQCORE_0_BASE_ADDRESS);
		if (ret < 0) {
			LOG_ERR("Failed to read registers data [%d].", ret);
			return -EIO;
		}

		drv_data->status = buf.status;

		if (buf.status == 0x00) {
			drv_data->co2 = sys_be16_to_cpu(buf.co2_pred);
			drv_data->voc = sys_be16_to_cpu(buf.voc);
			drv_data->status = buf.status;
			drv_data->resistance = sys_be32_to_cpu(buf.resistance);

			return 0;
		}

		k_sleep(100);
	}

	if (drv_data->status == 0x01) {
		LOG_INF("Sensor data not available");
	}

	if (drv_data->status == 0x80) {
		LOG_ERR("Sensor Error");
	}

	return -EIO;
}

static int iaqcore_channel_get(struct device *dev,
			       enum sensor_channel chan,
			       struct sensor_value *val)
{
	struct iaq_core_data *drv_data = dev->driver_data;

	switch (chan) {
	case SENSOR_CHAN_CO2:
		val->val1 = drv_data->co2;
		val->val2 = 0;
		break;
	case SENSOR_CHAN_VOC:
		val->val1 = drv_data->voc;
		val->val2 = 0;
		break;
	case SENSOR_CHAN_RESISTANCE:
		val->val1 = drv_data->resistance;
		val->val2 = 0;
		break;
	default:
		return -ENOTSUP;
	}

	return 0;
}

static const struct sensor_driver_api iaq_core_driver_api = {
	.sample_fetch = iaqcore_sample_fetch,
	.channel_get = iaqcore_channel_get,
};

static int iaq_core_init(struct device *dev)
{
	struct iaq_core_data *drv_data = dev->driver_data;

	drv_data->i2c = device_get_binding(DT_AMS_IAQCORE_0_BUS_NAME);
	if (drv_data->i2c == NULL) {
		LOG_ERR("Failed to get pointer to %s device!",
			    DT_AMS_IAQCORE_0_BUS_NAME);
		return -EINVAL;
	}

	return 0;
}

static struct iaq_core_data iaq_core_driver;

DEVICE_AND_API_INIT(iaq_core, DT_AMS_IAQCORE_0_LABEL, iaq_core_init,
		    &iaq_core_driver, NULL, POST_KERNEL,
		    CONFIG_SENSOR_INIT_PRIORITY, &iaq_core_driver_api);
