/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT nordic_npm6001_regulator

#include <errno.h>

#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/regulator.h>
#include <zephyr/dt-bindings/regulator/npm6001.h>
#include <zephyr/sys/__assert.h>
#include <zephyr/sys/linear_range.h>
#include <zephyr/sys/util.h>

/* nPM6001 voltage sources */
enum npm6001_sources {
	NPM6001_SOURCE_BUCK0,
	NPM6001_SOURCE_BUCK1,
	NPM6001_SOURCE_BUCK2,
	NPM6001_SOURCE_BUCK3,
	NPM6001_SOURCE_LDO0,
	NPM6001_SOURCE_LDO1,
};

/* nPM6001 regulator related registers */
#define NPM6001_TASKS_START_BUCK3    0x02U
#define NPM6001_TASKS_START_LDO0     0x03U
#define NPM6001_TASKS_START_LDO1     0x04U
#define NPM6001_TASKS_STOP_BUCK3     0x08U
#define NPM6001_TASKS_STOP_LDO0	     0x09U
#define NPM6001_TASKS_STOP_LDO1	     0x0AU
#define NPM6001_TASKS_UPDATE_VOUTPWM 0x0EU
#define NPM6001_EVENTS_THWARN	     0x1EU
#define NPM6001_EVENTS_BUCK0OC	     0x1FU
#define NPM6001_EVENTS_BUCK1OC	     0x20U
#define NPM6001_EVENTS_BUCK2OC	     0x21U
#define NPM6001_EVENTS_BUCK3OC	     0x22U
#define NPM6001_BUCK0VOUTULP	     0x3AU
#define NPM6001_BUCK1VOUTULP	     0x3CU
#define NPM6001_BUCK2VOUTULP	     0x40U
#define NPM6001_BUCK3SELDAC	     0x44U
#define NPM6001_BUCK3VOUT	     0x45U
#define NPM6001_LDO0VOUT	     0x46U
#define NPM6001_BUCK0CONFPWMMODE     0x4AU
#define NPM6001_BUCK1CONFPWMMODE     0x4BU
#define NPM6001_BUCK2CONFPWMMODE     0x4CU
#define NPM6001_BUCK3CONFPWMMODE     0x4DU
#define NPM6001_BUCKMODEPADCONF	     0x4EU
#define NPM6001_PADDRIVESTRENGTH     0x53U

/* nPM6001 LDO0VOUT values */
#define NPM6001_LDO0VOUT_SET1V8	 0x06U
#define NPM6001_LDO0VOUT_SET2V1	 0x0BU
#define NPM6001_LDO0VOUT_SET2V41 0x10U
#define NPM6001_LDO0VOUT_SET2V7	 0x15U
#define NPM6001_LDO0VOUT_SET3V0	 0x1AU
#define NPM6001_LDO0VOUT_SET3V3	 0x1EU

/* nPM6001 BUCKXCONFPWMMODE fields */
#define NPM6001_BUCKXCONFPWMMODE_SETFORCEPWM_MSK 0x8U
#define NPM6001_BUCKXCONFPWMMODE_SETFORCEPWM_POS 3
#define NPM6001_BUCKXCONFPWMMODE_SETFORCEPWM                                   \
	BIT(NPM6001_BUCKXCONFPWMMODE_SETFORCEPWM_POS)

#define NPM6001_BUCKMODEPADCONF_BUCKMODE0PADTYPE_CMOS  BIT(0)
#define NPM6001_BUCKMODEPADCONF_BUCKMODE1PADTYPE_CMOS  BIT(1)
#define NPM6001_BUCKMODEPADCONF_BUCKMODE2PADTYPE_CMOS  BIT(2)
#define NPM6001_BUCKMODEPADCONF_BUCKMODE0PULLD_ENABLED BIT(4)
#define NPM6001_BUCKMODEPADCONF_BUCKMODE1PULLD_ENABLED BIT(5)
#define NPM6001_BUCKMODEPADCONF_BUCKMODE2PULLD_ENABLED BIT(6)

/* nPM6001 PADDRIVESTRENGTH fields */
#define NPM6001_PADDRIVESTRENGTH_READY_HIGH BIT(2)
#define NPM6001_PADDRIVESTRENGTH_NINT_HIGH  BIT(3)
#define NPM6001_PADDRIVESTRENGTH_SDA_HIGH   BIT(5)

struct regulator_npm6001_pconfig {
	struct i2c_dt_spec i2c;
	uint8_t buck_pad_val;
	uint8_t pad_val;
};

struct regulator_npm6001_config {
	struct regulator_common_config common;
	const struct device *p;
	uint8_t source;
};

struct regulator_npm6001_data {
	struct regulator_common_data data;
};

struct regulator_npm6001_vmap {
	uint8_t reg_val;
	int32_t volt_uv;
};

static const struct linear_range buck0_range =
	LINEAR_RANGE_INIT(1800000, 100000U, 0x0U, 0xFU);

static const struct linear_range buck1_range =
	LINEAR_RANGE_INIT(700000, 50000U, 0x0U, 0xEU);

static const struct linear_range buck2_range =
	LINEAR_RANGE_INIT(1200000, 50000U, 0xAU, 0xEU);

static const struct linear_range buck3_range =
	LINEAR_RANGE_INIT(500000, 25000U, 0x0U, 0x70U);

static const struct regulator_npm6001_vmap ldo0_voltages[] = {
	{ NPM6001_LDO0VOUT_SET1V8, 1800000 },
	{ NPM6001_LDO0VOUT_SET2V1, 2100000 },
	{ NPM6001_LDO0VOUT_SET2V41, 2410000 },
	{ NPM6001_LDO0VOUT_SET2V7, 2700000 },
	{ NPM6001_LDO0VOUT_SET3V0, 3000000 },
	{ NPM6001_LDO0VOUT_SET3V3, 3300000 },
};

static int regulator_npm6001_ldo0_list_voltage(const struct device *dev,
					       unsigned int idx,
					       int32_t *volt_uv)
{
	if (idx >= ARRAY_SIZE(ldo0_voltages)) {
		return -EINVAL;
	}

	*volt_uv = ldo0_voltages[idx].volt_uv;

	return 0;
}

static int regulator_npm6001_buck012_set_voltage(
	const struct device *dev, int32_t min_uv, int32_t max_uv,
	const struct linear_range *range, uint8_t vout_reg, uint8_t conf_reg)
{
	const struct regulator_npm6001_config *config = dev->config;
	const struct regulator_npm6001_pconfig *pconfig = config->p->config;
	uint8_t conf, buf[3];
	uint16_t idx;
	int ret;

	ret = linear_range_get_win_index(range, min_uv, max_uv, &idx);
	if (ret < 0) {
		return ret;
	}

	/* force PWM mode when updating voltage */
	ret = i2c_reg_read_byte_dt(&pconfig->i2c, conf_reg, &conf);
	if (ret < 0) {
		return ret;
	}

	if ((conf & NPM6001_BUCKXCONFPWMMODE_SETFORCEPWM) == 0U) {
		ret = i2c_reg_write_byte_dt(
			&pconfig->i2c, conf_reg,
			conf | NPM6001_BUCKXCONFPWMMODE_SETFORCEPWM);
		if (ret < 0) {
			return ret;
		}
	}

	/* write voltage in both ULP/PWM registers */
	buf[0] = vout_reg;
	buf[1] = (uint8_t)idx;
	buf[2] = (uint8_t)idx;

	ret = i2c_write_dt(&pconfig->i2c, buf, sizeof(buf));
	if (ret < 0) {
		return ret;
	}

	ret = i2c_reg_write_byte_dt(&pconfig->i2c, NPM6001_TASKS_UPDATE_VOUTPWM,
				    1U);
	if (ret < 0) {
		return ret;
	}

	/* restore HYS mode if enabled before */
	if ((conf & NPM6001_BUCKXCONFPWMMODE_SETFORCEPWM) == 0U) {
		ret = i2c_reg_write_byte_dt(&pconfig->i2c, conf_reg, conf);
		if (ret < 0) {
			return ret;
		}
	}

	return 0;
}

static int regulator_npm6001_buck3_set_voltage(const struct device *dev,
					       int32_t min_uv, int32_t max_uv)
{
	const struct regulator_npm6001_config *config = dev->config;
	const struct regulator_npm6001_pconfig *pconfig = config->p->config;
	uint16_t idx;
	uint8_t conf;
	int ret;

	ret = linear_range_get_win_index(&buck3_range, min_uv, max_uv, &idx);
	if (ret < 0) {
		return ret;
	}

	/* force PWM mode when updating voltage */
	ret = i2c_reg_read_byte_dt(&pconfig->i2c, NPM6001_BUCK3CONFPWMMODE,
				   &conf);
	if (ret < 0) {
		return ret;
	}

	if ((conf & NPM6001_BUCKXCONFPWMMODE_SETFORCEPWM) == 0U) {
		ret = i2c_reg_write_byte_dt(
			&pconfig->i2c, NPM6001_BUCK3CONFPWMMODE,
			conf | NPM6001_BUCKXCONFPWMMODE_SETFORCEPWM);
		if (ret < 0) {
			return ret;
		}
	}

	ret = i2c_reg_write_byte_dt(&pconfig->i2c, NPM6001_BUCK3VOUT,
				    (uint8_t)idx);
	if (ret < 0) {
		return ret;
	}

	/* restore HYS mode if enabled before */
	if ((conf & NPM6001_BUCKXCONFPWMMODE_SETFORCEPWM) == 0U) {
		ret = i2c_reg_write_byte_dt(&pconfig->i2c,
					    NPM6001_BUCK3CONFPWMMODE, conf);
		if (ret < 0) {
			return ret;
		}
	}

	return 0;
}

static int regulator_npm6001_ldo0_set_voltage(const struct device *dev,
					      int32_t min_uv, int32_t max_uv)
{
	const struct regulator_npm6001_config *config = dev->config;
	const struct regulator_npm6001_pconfig *pconfig = config->p->config;
	uint8_t val;
	size_t i;

	for (i = 0U; i < ARRAY_SIZE(ldo0_voltages); i++) {
		if ((min_uv <= ldo0_voltages[i].volt_uv) &&
		    (max_uv >= ldo0_voltages[i].volt_uv)) {
			val = ldo0_voltages[i].reg_val;
			break;
		}
	}

	if (i == ARRAY_SIZE(ldo0_voltages)) {
		return -EINVAL;
	}

	return i2c_reg_write_byte_dt(&pconfig->i2c, NPM6001_LDO0VOUT, val);
}

static int
regulator_npm6001_buck0123_get_voltage(const struct device *dev,
				       const struct linear_range *range,
				       uint8_t vout_reg, int32_t *volt_uv)
{
	const struct regulator_npm6001_config *config = dev->config;
	const struct regulator_npm6001_pconfig *pconfig = config->p->config;
	uint8_t idx;
	int ret;

	ret = i2c_reg_read_byte_dt(&pconfig->i2c, vout_reg, &idx);
	if (ret < 0) {
		return ret;
	}

	return linear_range_get_value(range, idx, volt_uv);
}

static int regulator_npm6001_ldo0_get_voltage(const struct device *dev,
					      int32_t *volt_uv)
{
	const struct regulator_npm6001_config *config = dev->config;
	const struct regulator_npm6001_pconfig *pconfig = config->p->config;
	uint8_t val;
	int ret;

	ret = i2c_reg_read_byte_dt(&pconfig->i2c, NPM6001_LDO0VOUT, &val);
	if (ret < 0) {
		return ret;
	}

	for (size_t i = 0U; i < ARRAY_SIZE(ldo0_voltages); i++) {
		if (val == ldo0_voltages[i].reg_val) {
			*volt_uv = ldo0_voltages[i].volt_uv;
			return 0;
		}
	}

	__ASSERT(NULL, "Unexpected voltage");

	return -EINVAL;
}

static unsigned int regulator_npm6001_count_voltages(const struct device *dev)
{
	const struct regulator_npm6001_config *config = dev->config;

	switch (config->source) {
	case NPM6001_SOURCE_BUCK0:
		return linear_range_values_count(&buck0_range);
	case NPM6001_SOURCE_BUCK1:
		return linear_range_values_count(&buck1_range);
	case NPM6001_SOURCE_BUCK2:
		return linear_range_values_count(&buck2_range);
	case NPM6001_SOURCE_BUCK3:
		return linear_range_values_count(&buck3_range);
	case NPM6001_SOURCE_LDO0:
		return 6U;
	case NPM6001_SOURCE_LDO1:
		return 1U;
	default:
		__ASSERT(NULL, "Unexpected source");
	}

	return 0;
}

static int regulator_npm6001_list_voltage(const struct device *dev,
					  unsigned int idx, int32_t *volt_uv)
{
	const struct regulator_npm6001_config *config = dev->config;

	switch (config->source) {
	case NPM6001_SOURCE_BUCK0:
		return linear_range_get_value(&buck0_range, idx, volt_uv);
	case NPM6001_SOURCE_BUCK1:
		return linear_range_get_value(&buck1_range, idx, volt_uv);
	case NPM6001_SOURCE_BUCK2:
		return linear_range_get_value(
			&buck2_range, idx + buck2_range.min_idx, volt_uv);
	case NPM6001_SOURCE_BUCK3:
		return linear_range_get_value(&buck3_range, idx, volt_uv);
	case NPM6001_SOURCE_LDO0:
		return regulator_npm6001_ldo0_list_voltage(dev, idx, volt_uv);
	case NPM6001_SOURCE_LDO1:
		*volt_uv = 1800000;
		break;
	default:
		__ASSERT(NULL, "Unexpected source");
	}

	return 0;
}

static int regulator_npm6001_set_voltage(const struct device *dev,
					 int32_t min_uv, int32_t max_uv)
{
	const struct regulator_npm6001_config *config = dev->config;

	switch (config->source) {
	case NPM6001_SOURCE_BUCK0:
		return regulator_npm6001_buck012_set_voltage(
			dev, min_uv, max_uv, &buck0_range, NPM6001_BUCK0VOUTULP,
			NPM6001_BUCK0CONFPWMMODE);
	case NPM6001_SOURCE_BUCK1:
		return regulator_npm6001_buck012_set_voltage(
			dev, min_uv, max_uv, &buck1_range, NPM6001_BUCK1VOUTULP,
			NPM6001_BUCK1CONFPWMMODE);
	case NPM6001_SOURCE_BUCK2:
		return regulator_npm6001_buck012_set_voltage(
			dev, min_uv, max_uv, &buck2_range, NPM6001_BUCK2VOUTULP,
			NPM6001_BUCK2CONFPWMMODE);
	case NPM6001_SOURCE_BUCK3:
		return regulator_npm6001_buck3_set_voltage(dev, min_uv, max_uv);
	case NPM6001_SOURCE_LDO0:
		return regulator_npm6001_ldo0_set_voltage(dev, min_uv, max_uv);
	case NPM6001_SOURCE_LDO1:
		return -ENOTSUP;
	default:
		__ASSERT(NULL, "Unexpected source");
	}

	return 0;
}

static int regulator_npm6001_get_voltage(const struct device *dev,
					 int32_t *volt_uv)
{
	const struct regulator_npm6001_config *config = dev->config;

	switch (config->source) {
	case NPM6001_SOURCE_BUCK0:
		return regulator_npm6001_buck0123_get_voltage(
			dev, &buck0_range, NPM6001_BUCK0VOUTULP, volt_uv);
	case NPM6001_SOURCE_BUCK1:
		return regulator_npm6001_buck0123_get_voltage(
			dev, &buck1_range, NPM6001_BUCK1VOUTULP, volt_uv);
	case NPM6001_SOURCE_BUCK2:
		return regulator_npm6001_buck0123_get_voltage(
			dev, &buck2_range, NPM6001_BUCK2VOUTULP, volt_uv);
	case NPM6001_SOURCE_BUCK3:
		return regulator_npm6001_buck0123_get_voltage(
			dev, &buck3_range, NPM6001_BUCK3VOUT, volt_uv);
	case NPM6001_SOURCE_LDO0:
		return regulator_npm6001_ldo0_get_voltage(dev, volt_uv);
	case NPM6001_SOURCE_LDO1:
		*volt_uv = 1800000U;
		break;
	default:
		__ASSERT(NULL, "Unexpected source");
	}

	return 0;
}

static int regulator_npm6001_set_mode(const struct device *dev,
				      regulator_mode_t mode)
{
	const struct regulator_npm6001_config *config = dev->config;
	const struct regulator_npm6001_pconfig *pconfig = config->p->config;
	uint8_t conf_reg;

	if (mode > NPM6001_MODE_PWM) {
		return -ENOTSUP;
	}

	switch (config->source) {
	case NPM6001_SOURCE_BUCK0:
		conf_reg = NPM6001_BUCK0CONFPWMMODE;
		break;
	case NPM6001_SOURCE_BUCK1:
		conf_reg = NPM6001_BUCK1CONFPWMMODE;
		break;
	case NPM6001_SOURCE_BUCK2:
		conf_reg = NPM6001_BUCK2CONFPWMMODE;
		break;
	case NPM6001_SOURCE_BUCK3:
		conf_reg = NPM6001_BUCK3CONFPWMMODE;
		break;
	default:
		return -ENOTSUP;
	}

	return i2c_reg_update_byte_dt(
		&pconfig->i2c, conf_reg,
		NPM6001_BUCKXCONFPWMMODE_SETFORCEPWM_MSK,
		mode << NPM6001_BUCKXCONFPWMMODE_SETFORCEPWM_POS);
}

static int regulator_npm6001_get_mode(const struct device *dev,
				      regulator_mode_t *mode)
{
	const struct regulator_npm6001_config *config = dev->config;
	const struct regulator_npm6001_pconfig *pconfig = config->p->config;
	uint8_t conf_reg, conf;
	int ret;

	switch (config->source) {
	case NPM6001_SOURCE_BUCK0:
		conf_reg = NPM6001_BUCK0CONFPWMMODE;
		break;
	case NPM6001_SOURCE_BUCK1:
		conf_reg = NPM6001_BUCK1CONFPWMMODE;
		break;
	case NPM6001_SOURCE_BUCK2:
		conf_reg = NPM6001_BUCK2CONFPWMMODE;
		break;
	case NPM6001_SOURCE_BUCK3:
		conf_reg = NPM6001_BUCK3CONFPWMMODE;
		break;
	default:
		return -ENOTSUP;
	}

	ret = i2c_reg_read_byte_dt(&pconfig->i2c, conf_reg, &conf);
	if (ret < 0) {
		return ret;
	}

	*mode = (conf & NPM6001_BUCKXCONFPWMMODE_SETFORCEPWM_MSK) >>
		NPM6001_BUCKXCONFPWMMODE_SETFORCEPWM_POS;

	return 0;
}

static int regulator_npm6001_enable(const struct device *dev)
{
	const struct regulator_npm6001_config *config = dev->config;
	const struct regulator_npm6001_pconfig *pconfig = config->p->config;
	uint8_t start_reg;

	switch (config->source) {
	case NPM6001_SOURCE_BUCK3:
		start_reg = NPM6001_TASKS_START_BUCK3;
		break;
	case NPM6001_SOURCE_LDO0:
		start_reg = NPM6001_TASKS_START_LDO0;
		break;
	case NPM6001_SOURCE_LDO1:
		start_reg = NPM6001_TASKS_START_LDO1;
		break;
	default:
		return 0;
	}

	/* TASKS_START_(BUCK3|LDO0|LDO1) */
	return i2c_reg_write_byte_dt(&pconfig->i2c, start_reg, 1U);
}

static int regulator_npm6001_disable(const struct device *dev)
{
	const struct regulator_npm6001_config *config = dev->config;
	const struct regulator_npm6001_pconfig *pconfig = config->p->config;
	uint8_t stop_reg;

	switch (config->source) {
	case NPM6001_SOURCE_BUCK3:
		stop_reg = NPM6001_TASKS_STOP_BUCK3;
		break;
	case NPM6001_SOURCE_LDO0:
		stop_reg = NPM6001_TASKS_STOP_LDO0;
		break;
	case NPM6001_SOURCE_LDO1:
		stop_reg = NPM6001_TASKS_STOP_LDO1;
		break;
	default:
		return 0;
	}

	return i2c_reg_write_byte_dt(&pconfig->i2c, stop_reg, 1U);
}

static int regulator_npm6001_get_error_flags(const struct device *dev,
					     regulator_error_flags_t *flags)
{
	const struct regulator_npm6001_config *config = dev->config;
	const struct regulator_npm6001_pconfig *pconfig = config->p->config;
	uint8_t oc_reg, val;
	int ret;

	*flags = 0U;

	/* read thermal warning */
	ret = i2c_reg_read_byte_dt(&pconfig->i2c, NPM6001_EVENTS_THWARN, &val);
	if (ret < 0) {
		return ret;
	}

	if (val != 0U) {
		/* clear thermal warning */
		ret = i2c_reg_write_byte_dt(&pconfig->i2c,
					    NPM6001_EVENTS_THWARN, 0U);
		if (ret < 0) {
			return ret;
		}

		*flags |= REGULATOR_ERROR_OVER_TEMP;
	}

	/* read overcurrent event */
	switch (config->source) {
	case NPM6001_SOURCE_BUCK0:
		oc_reg = NPM6001_EVENTS_BUCK0OC;
		break;
	case NPM6001_SOURCE_BUCK1:
		oc_reg = NPM6001_EVENTS_BUCK1OC;
		break;
	case NPM6001_SOURCE_BUCK2:
		oc_reg = NPM6001_EVENTS_BUCK2OC;
		break;
	case NPM6001_SOURCE_BUCK3:
		oc_reg = NPM6001_EVENTS_BUCK3OC;
		break;
	default:
		return 0;
	}

	ret = i2c_reg_read_byte_dt(&pconfig->i2c, oc_reg, &val);
	if (ret < 0) {
		return ret;
	}

	if (val != 0U) {
		/* clear overcurrent event */
		ret = i2c_reg_write_byte_dt(&pconfig->i2c, oc_reg, 0U);
		if (ret < 0) {
			return ret;
		}

		*flags |= REGULATOR_ERROR_OVER_CURRENT;
	}

	return 0;
}

static int regulator_npm6001_init(const struct device *dev)
{
	const struct regulator_npm6001_config *config = dev->config;

	regulator_common_data_init(dev);

	if (!device_is_ready(config->p)) {
		return -ENODEV;
	}

	return regulator_common_init_enable(dev);
}

static int regulator_npm6001_common_init(const struct device *dev)
{
	const struct regulator_npm6001_pconfig *config = dev->config;
	int ret;

	if (!device_is_ready(config->i2c.bus)) {
		return -ENODEV;
	}

	/* always select BUCK3 DAC (does not increase power consumption) */
	ret = i2c_reg_write_byte_dt(&config->i2c, NPM6001_BUCK3SELDAC, 1U);
	if (ret < 0) {
		return ret;
	}

	/* configure pad properties */
	ret = i2c_reg_write_byte_dt(&config->i2c, NPM6001_BUCKMODEPADCONF,
				    config->buck_pad_val);
	if (ret < 0) {
		return ret;
	}

	ret = i2c_reg_write_byte_dt(&config->i2c, NPM6001_PADDRIVESTRENGTH,
				    config->pad_val);
	if (ret < 0) {
		return ret;
	}

	return 0;
}

static const struct regulator_driver_api api = {
	.enable = regulator_npm6001_enable,
	.disable = regulator_npm6001_disable,
	.count_voltages = regulator_npm6001_count_voltages,
	.list_voltage = regulator_npm6001_list_voltage,
	.set_voltage = regulator_npm6001_set_voltage,
	.get_voltage = regulator_npm6001_get_voltage,
	.set_mode = regulator_npm6001_set_mode,
	.get_mode = regulator_npm6001_get_mode,
	.get_error_flags = regulator_npm6001_get_error_flags,
};

#define REGULATOR_NPM6001_DEFINE(node_id, id, _source, parent)                 \
	static struct regulator_npm6001_data data_##id;                        \
                                                                               \
	static const struct regulator_npm6001_config config_##id = {           \
		.common = REGULATOR_DT_COMMON_CONFIG_INIT(node_id),            \
		.p = parent,                                                   \
		.source = _source,                                             \
	};                                                                     \
                                                                               \
	DEVICE_DT_DEFINE(node_id, regulator_npm6001_init, NULL, &data_##id,    \
			 &config_##id, POST_KERNEL,                            \
			 CONFIG_REGULATOR_NPM6001_INIT_PRIORITY, &api);

#define REGULATOR_NPM6001_DEFINE_COND(inst, child, source, parent)             \
	COND_CODE_1(DT_NODE_EXISTS(DT_INST_CHILD(inst, child)),                \
		    (REGULATOR_NPM6001_DEFINE(DT_INST_CHILD(inst, child),      \
					      child##inst, source, parent)),   \
		    ())

#define REGULATOR_NPM6001_DEFINE_ALL(inst)                                     \
	static const struct regulator_npm6001_pconfig config_##inst = {        \
		.i2c = I2C_DT_SPEC_GET(DT_INST_PARENT(inst)),                  \
		.buck_pad_val =                                                \
			((DT_INST_ENUM_IDX(inst,                               \
					   nordic_buck_mode0_input_type) *     \
			  NPM6001_BUCKMODEPADCONF_BUCKMODE0PADTYPE_CMOS) |     \
			 (DT_INST_ENUM_IDX(inst,                               \
					   nordic_buck_mode1_input_type) *     \
			  NPM6001_BUCKMODEPADCONF_BUCKMODE1PADTYPE_CMOS) |     \
			 (DT_INST_ENUM_IDX(inst,                               \
					   nordic_buck_mode2_input_type) *     \
			  NPM6001_BUCKMODEPADCONF_BUCKMODE2PADTYPE_CMOS) |     \
			 (DT_INST_PROP(inst, nordic_buck_mode0_pull_down) *    \
			  NPM6001_BUCKMODEPADCONF_BUCKMODE0PULLD_ENABLED) |    \
			 (DT_INST_PROP(inst, nordic_buck_mode1_pull_down) *    \
			  NPM6001_BUCKMODEPADCONF_BUCKMODE1PULLD_ENABLED) |    \
			 (DT_INST_PROP(inst, nordic_buck_mode2_pull_down) *    \
			  NPM6001_BUCKMODEPADCONF_BUCKMODE2PULLD_ENABLED)),    \
		.pad_val = ((DT_INST_PROP(inst, nordic_ready_high_drive) *     \
			     NPM6001_PADDRIVESTRENGTH_READY_HIGH) |            \
			    (DT_INST_PROP(inst, nordic_nint_high_drive) *      \
			     NPM6001_PADDRIVESTRENGTH_NINT_HIGH) |             \
			    (DT_INST_PROP(inst, nordic_sda_high_drive) *       \
			     NPM6001_PADDRIVESTRENGTH_SDA_HIGH)),              \
	};                                                                     \
                                                                               \
	DEVICE_DT_INST_DEFINE(inst, regulator_npm6001_common_init, NULL, NULL, \
			      &config_##inst, POST_KERNEL,                     \
			      CONFIG_REGULATOR_NPM6001_COMMON_INIT_PRIORITY,   \
			      NULL);                                           \
                                                                               \
	REGULATOR_NPM6001_DEFINE_COND(inst, buck0, NPM6001_SOURCE_BUCK0,       \
				      DEVICE_DT_INST_GET(inst))                \
	REGULATOR_NPM6001_DEFINE_COND(inst, buck1, NPM6001_SOURCE_BUCK1,       \
				      DEVICE_DT_INST_GET(inst))                \
	REGULATOR_NPM6001_DEFINE_COND(inst, buck2, NPM6001_SOURCE_BUCK2,       \
				      DEVICE_DT_INST_GET(inst))                \
	REGULATOR_NPM6001_DEFINE_COND(inst, buck3, NPM6001_SOURCE_BUCK3,       \
				      DEVICE_DT_INST_GET(inst))                \
	REGULATOR_NPM6001_DEFINE_COND(inst, ldo0, NPM6001_SOURCE_LDO0,         \
				      DEVICE_DT_INST_GET(inst))                \
	REGULATOR_NPM6001_DEFINE_COND(inst, ldo1, NPM6001_SOURCE_LDO1,         \
				      DEVICE_DT_INST_GET(inst))

DT_INST_FOREACH_STATUS_OKAY(REGULATOR_NPM6001_DEFINE_ALL)
