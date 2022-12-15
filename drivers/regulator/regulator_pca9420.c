/*
 * Copyright (c) 2021 NXP
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT nxp_pca9420

#include <errno.h>

#include <zephyr/kernel.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/regulator.h>
#include <zephyr/dt-bindings/regulator/pca9420.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/linear_range.h>
#include <zephyr/sys/util.h>

LOG_MODULE_REGISTER(pca9420, CONFIG_REGULATOR_LOG_LEVEL);

/** Register memory map. See datasheet for more details. */
/** General purpose registers */
/** @brief Top level system ctrl 0 */
#define PCA9420_TOP_CNTL0     0x09U

/** Regulator status indication registers */
/** @brief Mode configuration for mode 0_0 */
#define PCA9420_MODECFG_0_0          0x22U
/** @brief Mode configuration for mode 0_1 */
#define PCA9420_MODECFG_0_1          0x23U
/** @brief Mode configuration for mode 0_2 */
#define PCA9420_MODECFG_0_2          0x24U
/** @brief Mode configuration for mode 0_3 */
#define PCA9420_MODECFG_0_3          0x25U

/** @brief VIN input current limit selection */
#define PCA9420_TOP_CNTL0_VIN_ILIM_SEL_POS 5U
#define PCA9420_TOP_CNTL0_VIN_ILIM_SEL_MASK 0xE0U
#define PCA9420_TOP_CNTL0_VIN_ILIM_SEL_DISABLED 0x7U

/*
 * @brief Mode control selection mask. When this bit is set, the external
 * PMIC pins MODESEL0 and MODESEL1 can be used to select the active mode
 */
#define PCA9420_MODECFG_0_MODE_CTRL_SEL_MASK 0x40U

/*
 * @brief Mode configuration upon falling edge applied to ON pin. If set,
 * the device will switch to mode 0 when a valid falling edge is applied.
 * to the ON pin
 */
/** @brief Mode output voltage mask */
#define PCA9420_MODECFG_0_SW1_OUT_MASK       0x3FU
#define PCA9420_MODECFG_0_SW1_OUT_POS        0U
/** @brief SW2_OUT offset and voltage level mask */
#define PCA9420_MODECFG_1_SW2_OUT_MASK       0x3FU
#define PCA9420_MODECFG_1_SW2_OUT_POS        0U
/** @brief LDO1_OUT voltage level mask */
#define PCA9420_MODECFG_2_LDO1_OUT_MASK      0xF0U
#define PCA9420_MODECFG_2_LDO1_OUT_POS       4U
/** @brief SW1 Enable */
#define PCA9420_MODECFG_2_SW1_EN_MASK	     0x08U
#define PCA9420_MODECFG_2_SW1_EN_VAL	     0x08U
/** @brief SW2 Enable */
#define PCA9420_MODECFG_2_SW2_EN_MASK	     0x04U
#define PCA9420_MODECFG_2_SW2_EN_VAL	     0x04U
/** @brief LDO1 Enable */
#define PCA9420_MODECFG_2_LDO1_EN_MASK	     0x02U
#define PCA9420_MODECFG_2_LDO1_EN_VAL	     0x02U
/** @brief LDO2 Enable */
#define PCA9420_MODECFG_2_LDO2_EN_MASK	     0x01U
#define PCA9420_MODECFG_2_LDO2_EN_VAL	     0x01U
/** @brief LDO2_OUT offset and voltage level mask */
#define PCA9420_MODECFG_3_LDO2_OUT_MASK      0x3FU
#define PCA9420_MODECFG_3_LDO2_OUT_POS       0U

/** VIN ILIM resolution, uA/LSB */
#define PCA9420_VIN_ILIM_UA_LSB 170000
/** VIN ILIM minimum value, uA */
#define PCA9420_VIN_ILIM_MIN_UA 85000

struct regulator_pca9420_desc {
	uint8_t enable_reg;
	uint8_t enable_mask;
	uint8_t enable_val;
	uint8_t vsel_reg;
	uint8_t vsel_mask;
	uint8_t vsel_pos;
	uint8_t num_ranges;
	const struct linear_range *ranges;
};

struct regulator_pca9420_data {
	struct onoff_sync_service srv;
};

struct regulator_pca9420_common_config {
	struct i2c_dt_spec i2c;
	int32_t vin_ilim_ua;
	uint8_t modesel_reg;
	uint8_t modesel_mask;
	const uint16_t *allowed_modes;
	int num_modes;
	uint16_t initial_mode;
};

struct regulator_pca9420_config {
	int32_t max_ua;
	bool enable_inverted;
	bool boot_on;
	const struct regulator_pca9420_desc *desc;
	const struct device *parent;
};

static const struct linear_range buck1_ranges[] = {
	LINEAR_RANGE_INIT(500000, 25000U, 0x0U, 0x28U),
	LINEAR_RANGE_INIT(1500000, 0U, 0x29U, 0x3E),
	LINEAR_RANGE_INIT(1800000, 0U, 0x3FU, 0x3FU),
};

static const struct linear_range buck2_ranges[] = {
	LINEAR_RANGE_INIT(1500000, 25000U, 0x0U, 0x18U),
	LINEAR_RANGE_INIT(2100000, 0U, 0x19U, 0x1F),
	LINEAR_RANGE_INIT(2700000, 25000U, 0x20U, 0x38U),
	LINEAR_RANGE_INIT(3300000, 0U, 0x39U, 0x3F),
};

static const struct linear_range ldo1_ranges[] = {
	LINEAR_RANGE_INIT(1700000, 25000U, 0x0U, 0x9U),
	LINEAR_RANGE_INIT(1900000, 0U, 0x9U, 0xFU),
};

static const struct linear_range ldo2_ranges[] = {
	LINEAR_RANGE_INIT(1500000, 25000U, 0x0U, 0x18U),
	LINEAR_RANGE_INIT(2100000, 0U, 0x19U, 0x1FU),
	LINEAR_RANGE_INIT(2700000, 25000U, 0x20U, 0x38U),
	LINEAR_RANGE_INIT(3300000, 0U, 0x39U, 0x3FU),
};

static const struct regulator_pca9420_desc buck1_desc = {
	.enable_reg = PCA9420_MODECFG_0_2,
	.enable_mask = PCA9420_MODECFG_2_SW1_EN_MASK,
	.enable_val = PCA9420_MODECFG_2_SW1_EN_VAL,
	.vsel_mask = PCA9420_MODECFG_0_SW1_OUT_MASK,
	.vsel_pos = PCA9420_MODECFG_0_SW1_OUT_POS,
	.vsel_reg = PCA9420_MODECFG_0_0,
	.ranges = buck1_ranges,
	.num_ranges = ARRAY_SIZE(buck1_ranges),
};

static const struct regulator_pca9420_desc buck2_desc = {
	.enable_reg = PCA9420_MODECFG_0_2,
	.enable_mask = PCA9420_MODECFG_2_SW2_EN_MASK,
	.enable_val = PCA9420_MODECFG_2_SW2_EN_VAL,
	.vsel_mask = PCA9420_MODECFG_1_SW2_OUT_MASK,
	.vsel_pos = PCA9420_MODECFG_1_SW2_OUT_POS,
	.vsel_reg = PCA9420_MODECFG_0_1,
	.ranges = buck2_ranges,
	.num_ranges = ARRAY_SIZE(buck2_ranges),
};

static const struct regulator_pca9420_desc ldo1_desc = {
	.enable_reg = PCA9420_MODECFG_0_2,
	.enable_mask = PCA9420_MODECFG_2_LDO1_EN_MASK,
	.enable_val = PCA9420_MODECFG_2_LDO1_EN_VAL,
	.vsel_mask = PCA9420_MODECFG_2_LDO1_OUT_MASK,
	.vsel_pos = PCA9420_MODECFG_2_LDO1_OUT_POS,
	.vsel_reg = PCA9420_MODECFG_0_2,
	.ranges = ldo1_ranges,
	.num_ranges = ARRAY_SIZE(ldo1_ranges),
};

static const struct regulator_pca9420_desc ldo2_desc = {
	.enable_reg = PCA9420_MODECFG_0_2,
	.enable_mask = PCA9420_MODECFG_2_LDO2_EN_MASK,
	.enable_val = PCA9420_MODECFG_2_LDO2_EN_VAL,
	.vsel_reg = PCA9420_MODECFG_0_3,
	.vsel_mask = PCA9420_MODECFG_3_LDO2_OUT_MASK,
	.vsel_pos = PCA9420_MODECFG_3_LDO2_OUT_POS,
	.ranges = ldo2_ranges,
	.num_ranges = ARRAY_SIZE(ldo2_ranges),
};

static int regulator_pca9420_is_supported_voltage(const struct device *dev,
						  int32_t min_uv,
						  int32_t max_uv);

/**
 * Reads a register from the PMIC
 * Returns 0 on success, or errno on error
 */
static int regulator_pca9420_read_register(const struct i2c_dt_spec *i2c,
					   uint8_t reg, uint8_t *out)
{
	int ret;

	ret = i2c_reg_read_byte_dt(i2c, reg, out);
	LOG_DBG("READ 0x%x: 0x%x", reg, *out);
	return ret;
}

/**
 * Modifies a register within the PMIC
 * Returns 0 on success, or errno on error
 */
static int regulator_pca9420_modify_register(const struct i2c_dt_spec *i2c,
					     uint8_t reg, uint8_t reg_mask,
					     uint8_t reg_val)
{
	uint8_t reg_current;
	int rc;

	rc = regulator_pca9420_read_register(i2c, reg, &reg_current);
	if (rc) {
		return rc;
	}

	reg_current &= ~reg_mask;
	reg_current |= (reg_val & reg_mask);
	LOG_DBG("WRITE 0x%02X to 0x%02X at I2C addr 0x%02X", reg_current,
		reg, i2c->addr);
	return i2c_reg_write_byte_dt(i2c, reg, reg_current);
}

/*
 * Internal helper function- gets the voltage from a regulator, with an
 * offset applied to the vsel_reg. Useful to support reading voltages
 * in another target mode
 */
static int regulator_pca9420_get_voltage_offset(const struct device *dev,
						uint32_t off, int32_t *voltage)
{
	const struct regulator_pca9420_config *config = dev->config;
	const struct regulator_pca9420_common_config *cconfig = config->parent->config;

	int ret;
	uint8_t raw_reg;

	ret = regulator_pca9420_read_register(&cconfig->i2c,
					      config->desc->vsel_reg + off,
					      &raw_reg);
	if (ret < 0) {
		return ret;
	}

	raw_reg = (raw_reg & config->desc->vsel_mask) >> config->desc->vsel_pos;

	return linear_range_group_get_value(config->desc->ranges,
					    config->desc->num_ranges, raw_reg,
					    voltage);
}

/**
 * Internal helper function- sets the voltage for a regulator, with an
 * offset applied to the vsel_reg. Useful to support setting voltages in
 * another target mode.
 */
static int regulator_set_voltage_offset(const struct device *dev,
					int32_t min_uv, int32_t max_uv,
					uint32_t off)
{
	const struct regulator_pca9420_config *config = dev->config;
	const struct regulator_pca9420_common_config *cconfig = config->parent->config;
	uint16_t idx;
	int ret;

	ret = linear_range_group_get_win_index(config->desc->ranges,
					       config->desc->num_ranges, min_uv,
					       max_uv, &idx);
	if (ret < 0) {
		return ret;
	}

	idx <<= config->desc->vsel_pos;

	return regulator_pca9420_modify_register(&cconfig->i2c,
						 config->desc->vsel_reg + off,
						 config->desc->vsel_mask,
						 (uint8_t)idx);
}


/**
 * Part of the extended regulator consumer API
 * Returns the number of supported voltages
 */
static int regulator_pca9420_count_voltages(const struct device *dev)
{
	const struct regulator_pca9420_config *config = dev->config;

	return linear_range_group_values_count(config->desc->ranges,
					       config->desc->num_ranges);
}

/**
 * Part of the extended regulator consumer API
 * Counts the number of modes supported by a regulator
 */
static int regulator_pca9420_count_modes(const struct device *dev)
{
	const struct regulator_pca9420_config *config = dev->config;
	const struct regulator_pca9420_common_config *cconfig = config->parent->config;

	return cconfig->num_modes;
}

/**
 * Part of the extended regulator consumer API
 * Returns the supported voltage in uV for a given selector value
 */
static int32_t regulator_pca9420_list_voltages(const struct device *dev,
					       unsigned int selector)
{
	const struct regulator_pca9420_config *config = dev->config;
	int32_t value;

	if (linear_range_group_get_value(config->desc->ranges,
					 config->desc->num_ranges, selector,
					 &value) < 0) {
		return 0;
	}

	return value;
}

/**
 * Part of the extended regulator consumer API
 * Returns true if the regulator supports a voltage in the given range.
 */
static int regulator_pca9420_is_supported_voltage(const struct device *dev,
						  int32_t min_uv,
						  int32_t max_uv)
{
	const struct regulator_pca9420_config *config = dev->config;
	uint16_t idx;

	return linear_range_group_get_win_index(config->desc->ranges,
						config->desc->num_ranges,
						min_uv, max_uv, &idx);
}

/**
 * Part of the extended regulator consumer API
 * Sets the output voltage to the closest supported voltage value
 */
static int regulator_pca9420_set_voltage(const struct device *dev,
					 int32_t min_uv, int32_t max_uv)
{
	return regulator_set_voltage_offset(dev, min_uv, max_uv, 0);
}


/**
 * Part of the extended regulator consumer API
 * Gets the current output voltage in uV
 */
static int32_t regulator_pca9420_get_voltage(const struct device *dev)
{
	int32_t voltage = 0;

	(void)regulator_pca9420_get_voltage_offset(dev, 0, &voltage);

	return voltage;
}

/**
 * Part of the extended regulator consumer API
 * Gets the set current limit for the regulator
 */
static int regulator_pca9420_get_current_limit(const struct device *dev)
{
	const struct regulator_pca9420_config *config = dev->config;
	const struct regulator_pca9420_common_config *cconfig = config->parent->config;

	if (cconfig->vin_ilim_ua == 0U) {
		return config->max_ua;
	}

	return MIN(config->max_ua, cconfig->vin_ilim_ua);
}

/*
 * Part of the extended regulator consumer API.
 * sets the target voltage for a given regulator mode. This mode does
 * not need to be the active mode. This API can be used to configure
 * voltages for a mode, then the regulator can be switched to that mode
 * with the regulator_pca9420_set_mode api
 */
static int regulator_pca9420_set_mode_voltage(const struct device *dev,
					      uint32_t mode, int32_t min_uv,
					      int32_t max_uv)
{
	const struct regulator_pca9420_config *config = dev->config;
	const struct regulator_pca9420_common_config *cconfig = config->parent->config;
	uint8_t i, sel_off;

	if (cconfig->num_modes == 0) {
		return -ENOTSUP;
	}

	/* Search for mode ID in allowed modes. */
	for (i = 0 ; i < cconfig->num_modes; i++) {
		if (cconfig->allowed_modes[i] == mode) {
			break;
		}
	}
	if (i == cconfig->num_modes) {
		/* Mode was not found */
		return -EINVAL;
	}
	sel_off = ((mode & PCA9420_MODE_OFFSET_MASK) >> PCA9420_MODE_OFFSET_SHIFT);
	return regulator_set_voltage_offset(dev, min_uv, max_uv, sel_off);
}

/*
 * Part of the extended regulator consumer API.
 * Disables the regulator in a given mode. Does not implement the
 * onoff service, as this is incompatible with multiple mode operation
 */
static int regulator_pca9420_mode_disable(const struct device *dev,
					  uint32_t mode)
{
	const struct regulator_pca9420_config *config = dev->config;
	const struct regulator_pca9420_common_config *cconfig = config->parent->config;
	uint8_t i, sel_off, dis_val;

	if (cconfig->num_modes == 0) {
		return -ENOTSUP;
	}

	/* Search for mode ID in allowed modes. */
	for (i = 0 ; i < cconfig->num_modes; i++) {
		if (cconfig->allowed_modes[i] == mode) {
			break;
		}
	}
	if (i == cconfig->num_modes) {
		/* Mode was not found */
		return -EINVAL;
	}
	sel_off = ((mode & PCA9420_MODE_OFFSET_MASK) >> PCA9420_MODE_OFFSET_SHIFT);
	dis_val = config->enable_inverted ? config->desc->enable_val : 0;
	return regulator_pca9420_modify_register(
		&cconfig->i2c, config->desc->enable_reg + sel_off,
		config->desc->enable_mask, dis_val);
}

/*
 * Part of the extended regulator consumer API.
 * Enables the regulator in a given mode. Does not implement the
 * onoff service, as this is incompatible with multiple mode operation
 */
static int regulator_pca9420_mode_enable(const struct device *dev,
					 uint32_t mode)
{
	const struct regulator_pca9420_config *config = dev->config;
	const struct regulator_pca9420_common_config *cconfig = config->parent->config;
	uint8_t i, sel_off, en_val;

	if (cconfig->num_modes == 0) {
		return -ENOTSUP;
	}

	/* Search for mode ID in allowed modes. */
	for (i = 0 ; i < cconfig->num_modes; i++) {
		if (cconfig->allowed_modes[i] == mode) {
			break;
		}
	}
	if (i == cconfig->num_modes) {
		/* Mode was not found */
		return -EINVAL;
	}
	sel_off = ((mode & PCA9420_MODE_OFFSET_MASK) >> PCA9420_MODE_OFFSET_SHIFT);
	en_val = config->enable_inverted ? 0 : config->desc->enable_val;
	return regulator_pca9420_modify_register(
		&cconfig->i2c, config->desc->enable_reg + sel_off,
		config->desc->enable_mask, en_val);
}

/*
 * Part of the extended regulator consumer API.
 * gets the target voltage for a given regulator mode. This mode does
 * not need to be the active mode. This API can be used to read voltages
 * from a regulator mode other than the default.
 */
static int32_t regulator_pca9420_get_mode_voltage(const struct device *dev,
						  uint32_t mode)
{
	const struct regulator_pca9420_config *config = dev->config;
	const struct regulator_pca9420_common_config *cconfig = config->parent->config;
	uint8_t i, sel_off;
	int32_t voltage;

	if (cconfig->num_modes == 0) {
		return -ENOTSUP;
	}

	/* Search for mode ID in allowed modes. */
	for (i = 0 ; i < cconfig->num_modes; i++) {
		if (cconfig->allowed_modes[i] == mode) {
			break;
		}
	}
	if (i == cconfig->num_modes) {
		/* Mode was not found */
		return -EINVAL;
	}

	sel_off = ((mode & PCA9420_MODE_OFFSET_MASK) >> PCA9420_MODE_OFFSET_SHIFT);

	(void)regulator_pca9420_get_voltage_offset(dev, sel_off, &voltage);

	return voltage;
}

/*
 * Part of the extended regulator consumer API
 * switches the regulator to a given mode. This API will apply a mode for
 * the regulator.
 */
static int regulator_pca9420_set_mode(const struct device *dev, uint32_t mode)
{
	const struct regulator_pca9420_config *config = dev->config;
	const struct regulator_pca9420_common_config *cconfig = config->parent->config;
	int rc;
	uint8_t i, sel_off;

	if (cconfig->num_modes == 0) {
		return -ENOTSUP;
	}

	/* Search for mode ID in allowed modes. */
	for (i = 0 ; i < cconfig->num_modes; i++) {
		if (cconfig->allowed_modes[i] == mode) {
			break;
		}
	}
	if (i == cconfig->num_modes) {
		/* Mode was not found */
		return -EINVAL;
	}
	sel_off = ((mode & PCA9420_MODE_OFFSET_MASK) >> PCA9420_MODE_OFFSET_SHIFT);
	/* Configure mode */
	if (mode & PCA9420_MODE_FLAG_MODESEL_MULTI_REG) {
		/* Select mode with offset calculation */
		rc = regulator_pca9420_modify_register(
			&cconfig->i2c,
			cconfig->modesel_reg + sel_off,
			mode & PCA9420_MODE_SELECTOR_MASK,
			cconfig->modesel_mask);
	} else {
		/* Select mode without offset to modesel_reg */
		rc = regulator_pca9420_modify_register(
			&cconfig->i2c, cconfig->modesel_reg,
			mode & PCA9420_MODE_SELECTOR_MASK,
			cconfig->modesel_mask);
	}
	return rc;
}

static int regulator_pca9420_enable(const struct device *dev,
				    struct onoff_client *cli)
{
	k_spinlock_key_t key;
	int rc;
	uint8_t en_val;
	struct regulator_pca9420_data *data = dev->data;
	const struct regulator_pca9420_config *config = dev->config;
	const struct regulator_pca9420_common_config *cconfig = config->parent->config;

	LOG_DBG("Enabling regulator");
	rc = onoff_sync_lock(&data->srv, &key);
	if (rc) {
		/* Request has already enabled PMIC */
		return onoff_sync_finalize(&data->srv, key, cli, rc, true);
	}
	en_val = config->enable_inverted ? 0 : config->desc->enable_val;
	rc = regulator_pca9420_modify_register(&cconfig->i2c,
					       config->desc->enable_reg,
					       config->desc->enable_mask,
					       en_val);
	if (rc != 0) {
		return onoff_sync_finalize(&data->srv, key, NULL, rc, false);
	}
	return onoff_sync_finalize(&data->srv, key, cli, rc, true);
}

static int regulator_pca9420_disable(const struct device *dev)
{
	struct regulator_pca9420_data *data = dev->data;
	const struct regulator_pca9420_config *config = dev->config;
	const struct regulator_pca9420_common_config *cconfig = config->parent->config;
	k_spinlock_key_t key;
	uint8_t dis_val;
	int rc;

	LOG_DBG("Disabling regulator");
	rc = onoff_sync_lock(&data->srv, &key);
	if (rc == 0) {
		rc = -EINVAL;
		return onoff_sync_finalize(&data->srv, key, NULL, rc, false);
	} else if (rc == 1) {
		/* Disable regulator */
		dis_val = config->enable_inverted ? config->desc->enable_val : 0;
		rc = regulator_pca9420_modify_register(
			&cconfig->i2c, config->desc->enable_reg,
			config->desc->enable_mask, dis_val);
	}
	return onoff_sync_finalize(&data->srv, key, NULL, rc, false);
}

static int regulator_pca9420_init(const struct device *dev)
{
	const struct regulator_pca9420_config *config = dev->config;
	const struct regulator_pca9420_common_config *cconfig = config->parent->config;
	int rc  = 0;

	if (!device_is_ready(config->parent)) {
		return -ENODEV;
	}

	if (config->boot_on) {
		rc = regulator_pca9420_enable(dev, NULL);
	}
	if (cconfig->initial_mode) {
		rc = regulator_pca9420_set_mode(dev, cconfig->initial_mode);
	}
	return rc;
}

static int regulator_pca9420_common_init(const struct device *dev)
{
	const struct regulator_pca9420_common_config *config = dev->config;
	uint8_t reg_val = PCA9420_TOP_CNTL0_VIN_ILIM_SEL_DISABLED;

	if (!device_is_ready(config->i2c.bus)) {
		return -ENODEV;
	}

	/* configure VIN current limit */
	if (config->vin_ilim_ua != 0U) {
		reg_val = (config->vin_ilim_ua - PCA9420_VIN_ILIM_MIN_UA) /
			  PCA9420_VIN_ILIM_UA_LSB;
	}

	return regulator_pca9420_modify_register(
		&config->i2c, PCA9420_TOP_CNTL0,
		PCA9420_TOP_CNTL0_VIN_ILIM_SEL_MASK,
		reg_val << PCA9420_TOP_CNTL0_VIN_ILIM_SEL_POS);
}


static const struct regulator_driver_api api = {
	.enable = regulator_pca9420_enable,
	.disable = regulator_pca9420_disable,
	.count_voltages = regulator_pca9420_count_voltages,
	.count_modes = regulator_pca9420_count_modes,
	.list_voltages = regulator_pca9420_list_voltages,
	.is_supported_voltage = regulator_pca9420_is_supported_voltage,
	.set_voltage = regulator_pca9420_set_voltage,
	.get_voltage = regulator_pca9420_get_voltage,
	.get_current_limit = regulator_pca9420_get_current_limit,
	.set_mode = regulator_pca9420_set_mode,
	.set_mode_voltage = regulator_pca9420_set_mode_voltage,
	.get_mode_voltage = regulator_pca9420_get_mode_voltage,
	.mode_disable = regulator_pca9420_mode_disable,
	.mode_enable = regulator_pca9420_mode_enable,
};

#define REGULATOR_PCA9420_DEFINE(node_id, id, name, _parent)                   \
	static struct regulator_pca9420_data data_##id;                        \
                                                                               \
	static const struct regulator_pca9420_config config_##id = {           \
		.max_ua = DT_PROP(node_id, regulator_max_microamp),            \
		.enable_inverted = DT_PROP(node_id, enable_inverted),          \
		.boot_on = DT_PROP(node_id, regulator_boot_on),                \
		.desc = &name ## _desc,                                        \
		.parent = _parent,                                             \
	};                                                                     \
                                                                               \
	DEVICE_DT_DEFINE(node_id, regulator_pca9420_init, NULL, &data_##id,    \
			 &config_##id, POST_KERNEL,                            \
			 CONFIG_REGULATOR_PCA9420_INIT_PRIORITY, &api);

#define REGULATOR_PCA9420_DEFINE_COND(inst, child, parent)                     \
	COND_CODE_1(DT_NODE_EXISTS(DT_INST_CHILD(inst, child)),                \
		    (REGULATOR_PCA9420_DEFINE(DT_INST_CHILD(inst, child),      \
					      child ## inst, child, parent)),  \
		    ())

#define REGULATOR_PCA9420_DEFINE_ALL(inst)                                     \
	static const uint16_t allowed_modes_##inst[] =                         \
		DT_INST_PROP_OR(inst, regulator_allowed_modes, {});            \
                                                                               \
	static const struct regulator_pca9420_common_config config_##inst = {  \
		.i2c = I2C_DT_SPEC_INST_GET(inst),                             \
		.vin_ilim_ua = DT_INST_PROP(inst, nxp_vin_ilim_microamp),      \
		.allowed_modes = allowed_modes_##inst,                         \
		.num_modes = ARRAY_SIZE(allowed_modes_##inst),                 \
		.initial_mode =                                                \
			DT_INST_PROP_OR(inst, regulator_initial_mode, 0),      \
		.modesel_reg = DT_INST_PROP_OR(inst, modesel_reg, 0),          \
		.modesel_mask = DT_INST_PROP_OR(inst, modesel_mask, 0),        \
	};                                                                     \
                                                                               \
	DEVICE_DT_INST_DEFINE(inst, regulator_pca9420_common_init, NULL, NULL, \
			      &config_##inst, POST_KERNEL,                     \
			      CONFIG_REGULATOR_PCA9420_COMMON_INIT_PRIORITY,   \
			      NULL);                                           \
                                                                               \
	REGULATOR_PCA9420_DEFINE_COND(inst, buck1, DEVICE_DT_INST_GET(inst))   \
	REGULATOR_PCA9420_DEFINE_COND(inst, buck2, DEVICE_DT_INST_GET(inst))   \
	REGULATOR_PCA9420_DEFINE_COND(inst, ldo1, DEVICE_DT_INST_GET(inst))    \
	REGULATOR_PCA9420_DEFINE_COND(inst, ldo2, DEVICE_DT_INST_GET(inst))

DT_INST_FOREACH_STATUS_OKAY(REGULATOR_PCA9420_DEFINE_ALL)
