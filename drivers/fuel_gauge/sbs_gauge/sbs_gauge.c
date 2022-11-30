/*
 * Copyright (c) 2022 Leica Geosystems AG
 *
 * Copyright 2022 Google LLC
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT sbs_sbs_gauge_new_api

#include "sbs_gauge.h"

#include <zephyr/drivers/fuel_gauge.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/byteorder.h>

LOG_MODULE_REGISTER(sbs_gauge);

static int sbs_cmd_reg_read(const struct device *dev, uint8_t reg_addr, uint16_t *val)
{
	const struct sbs_gauge_config *cfg;
	uint8_t i2c_data[2];
	int status;

	cfg = dev->config;
	status = i2c_burst_read_dt(&cfg->i2c, reg_addr, i2c_data, ARRAY_SIZE(i2c_data));
	if (status < 0) {
		LOG_ERR("Unable to read register");
		return status;
	}

	*val = sys_get_le16(i2c_data);

	return 0;
}

static int sbs_gauge_get_prop(const struct device *dev, struct fuel_gauge_get_property *prop)
{
	int rc = 0;
	uint16_t val = 0;

	switch (prop->property_type) {
	case FUEL_GAUGE_AVG_CURRENT:
		rc = sbs_cmd_reg_read(dev, SBS_GAUGE_CMD_AVG_CURRENT, &val);
		prop->value.avg_current = val * 1000;
		break;
	case FUEL_GAUGE_CYCLE_COUNT:
		rc = sbs_cmd_reg_read(dev, SBS_GAUGE_CMD_CYCLE_COUNT, &val);
		prop->value.cycle_count = val;
		break;
	case FUEL_GAUGE_CURRENT:
		rc = sbs_cmd_reg_read(dev, SBS_GAUGE_CMD_CURRENT, &val);
		prop->value.current = val * 1000;
		break;
	case FUEL_GAUGE_FULL_CHARGE_CAPACITY:
		rc = sbs_cmd_reg_read(dev, SBS_GAUGE_CMD_FULL_CAPACITY, &val);
		prop->value.full_charge_capacity = val * 1000;
		break;
	case FUEL_GAUGE_REMAINING_CAPACITY:
		rc = sbs_cmd_reg_read(dev, SBS_GAUGE_CMD_REM_CAPACITY, &val);
		prop->value.remaining_capacity = val * 1000;
		break;
	case FUEL_GAUGE_RUNTIME_TO_EMPTY:
		rc = sbs_cmd_reg_read(dev, SBS_GAUGE_CMD_RUNTIME2EMPTY, &val);
		prop->value.runtime_to_empty = val;
		break;
	case FUEL_GAUGE_RUNTIME_TO_FULL:
		rc = sbs_cmd_reg_read(dev, SBS_GAUGE_CMD_AVG_TIME2FULL, &val);
		prop->value.runtime_to_empty = val;
		break;
	case FUEL_GAUGE_STATE_OF_CHARGE:
		rc = sbs_cmd_reg_read(dev, SBS_GAUGE_CMD_ASOC, &val);
		prop->value.state_of_charge = val;
		break;
	case FUEL_GAUGE_TEMPERATURE:
		rc = sbs_cmd_reg_read(dev, SBS_GAUGE_CMD_TEMP, &val);
		prop->value.temperature = val;
		break;
	case FUEL_GAUGE_VOLTAGE:
		rc = sbs_cmd_reg_read(dev, SBS_GAUGE_CMD_VOLTAGE, &val);
		prop->value.voltage = val * 1000;
		break;
	default:
		rc = -ENOTSUP;
	}

	prop->status = rc;

	return rc;
}

static int sbs_gauge_get_props(const struct device *dev, struct fuel_gauge_get_property *props,
			       size_t len)
{
	int ret = 0;

	for (int i = 0; i < len; i++) {
		ret |= sbs_gauge_get_prop(dev, props + i);
	}

	return ret;
}

/**
 * @brief initialize the fuel gauge
 *
 * @return 0 for success
 */
static int sbs_gauge_init(const struct device *dev)
{
	const struct sbs_gauge_config *cfg;

	cfg = dev->config;

	if (!device_is_ready(cfg->i2c.bus)) {
		LOG_ERR("Bus device is not ready");
		return -ENODEV;
	}

	return 0;
}

static const struct battery_driver_api sbs_gauge_driver_api = {
	.get_property = &sbs_gauge_get_props,
};

/* FIXME: fix init priority */
#define SBS_GAUGE_INIT(index)                                                                      \
                                                                                                   \
	static const struct sbs_gauge_config sbs_gauge_config_##index = {                          \
		.i2c = I2C_DT_SPEC_INST_GET(index),                                                \
	};                                                                                         \
                                                                                                   \
	DEVICE_DT_INST_DEFINE(index, &sbs_gauge_init, NULL, NULL, &sbs_gauge_config_##index,       \
			      POST_KERNEL, 90, &sbs_gauge_driver_api);

DT_INST_FOREACH_STATUS_OKAY(SBS_GAUGE_INIT)
