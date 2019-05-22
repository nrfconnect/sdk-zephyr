/*
 * Copyright (c) 2018 Linaro Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief LP3943 LED driver
 *
 * Limitations:
 * - Blink period and brightness value are controlled by two sets of PSCx/PWMx
 *   registers. This driver partitions the available LEDs into two groups as
 *   0 to 7 and 8 to 15 and assigns PSC0/PWM0 to LEDs from 0 to 7 and PSC1/PWM1
 *   to LEDs from 8 to 15. So, it is not possible to set unique blink period
 *   and brightness value for LEDs in a group, changing either of these
 *   values for a LED will affect other LEDs also.
 */

#include <i2c.h>
#include <led.h>
#include <misc/util.h>
#include <zephyr.h>

#define LOG_LEVEL CONFIG_LED_LOG_LEVEL
#include <logging/log.h>
LOG_MODULE_REGISTER(lp3943);

#ifdef CONFIG_HAS_DTS_I2C
#define CONFIG_LP3943_DEV_NAME			DT_TI_LP3943_0_LABEL
#define CONFIG_LP3943_I2C_ADDRESS		DT_TI_LP3943_0_BASE_ADDRESS
#define CONFIG_LP3943_I2C_MASTER_DEV_NAME	DT_TI_LP3943_0_BUS_NAME
#endif

#include "led_context.h"

/* LP3943 Registers */
#define LP3943_INPUT_1 0x00
#define LP3943_INPUT_2 0x01
#define LP3943_PSC0 0x02
#define LP3943_PWM0 0x03
#define LP3943_PSC1 0x04
#define LP3943_PWM1 0x05
#define LP3943_LS0 0x06
#define LP3943_LS1 0x07
#define LP3943_LS2 0x08
#define LP3943_LS3 0x09

#define LP3943_MASK 0x03

enum lp3943_modes {
	LP3943_OFF,
	LP3943_ON,
	LP3943_DIM0,
	LP3943_DIM1,
};

struct lp3943_data {
	struct device *i2c;
	struct led_data dev_data;
};

static int lp3943_get_led_reg(u32_t *led, u8_t *reg)
{
	switch (*led) {
	case 0:
	case 1:
	case 2:
		/* Fall through */
	case 3:
		*reg = LP3943_LS0;
		break;
	case 4:
	case 5:
	case 6:
		/* Fall through */
	case 7:
		*reg = LP3943_LS1;
		*led -= 4U;
		break;
	case 8:
	case 9:
	case 10:
		/* Fall through */
	case 11:
		*reg = LP3943_LS2;
		*led -= 8U;
		break;
	case 12:
	case 13:
	case 14:
		/* Fall through */
	case 15:
		*reg = LP3943_LS3;
		*led -= 12U;
		break;
	default:
		LOG_ERR("Invalid LED specified");
		return -EINVAL;
	}

	return 0;
}

static int lp3943_set_dim_states(struct lp3943_data *data, u32_t led, u8_t mode)
{
	int ret;
	u8_t reg;

	ret = lp3943_get_led_reg(&led, &reg);
	if (ret) {
		return ret;
	}

	/* Set DIMx states for the LEDs */
	if (i2c_reg_update_byte(data->i2c, CONFIG_LP3943_I2C_ADDRESS, reg,
				LP3943_MASK << (led << 1),
				mode << (led << 1))) {
		LOG_ERR("LED reg update failed");
		return -EIO;
	}

	return 0;
}

static int lp3943_led_blink(struct device *dev, u32_t led,
			    u32_t delay_on, u32_t delay_off)
{
	struct lp3943_data *data = dev->driver_data;
	struct led_data *dev_data = &data->dev_data;
	int ret;
	u16_t period;
	u8_t reg, val, mode;

	period = delay_on + delay_off;

	if (period < dev_data->min_period || period > dev_data->max_period) {
		return -EINVAL;
	}

	/* Use DIM0 for LEDs 0 to 7 and DIM1 for LEDs 8 to 15 */
	if (led < 8) {
		mode = LP3943_DIM0;
	} else {
		mode = LP3943_DIM1;
	}

	if (mode == LP3943_DIM0) {
		reg = LP3943_PSC0;
	} else {
		reg = LP3943_PSC1;
	}

	val = (period * 255U) / dev_data->max_period;
	if (i2c_reg_write_byte(data->i2c, CONFIG_LP3943_I2C_ADDRESS,
			       reg, val)) {
		LOG_ERR("LED write failed");
		return -EIO;
	}

	ret = lp3943_set_dim_states(data, led, mode);
	if (ret) {
		return ret;
	}

	return 0;
}

static int lp3943_led_set_brightness(struct device *dev, u32_t led,
				     u8_t value)
{
	struct lp3943_data *data = dev->driver_data;
	struct led_data *dev_data = &data->dev_data;
	int ret;
	u8_t reg, val, mode;

	if (value < dev_data->min_brightness ||
			value > dev_data->max_brightness) {
		return -EINVAL;
	}

	/* Use DIM0 for LEDs 0 to 7 and DIM1 for LEDs 8 to 15 */
	if (led < 8) {
		mode = LP3943_DIM0;
	} else {
		mode = LP3943_DIM1;
	}

	if (mode == LP3943_DIM0) {
		reg = LP3943_PWM0;
	} else {
		reg = LP3943_PWM1;
	}

	val = (value * 255U) / dev_data->max_brightness;
	if (i2c_reg_write_byte(data->i2c, CONFIG_LP3943_I2C_ADDRESS,
			       reg, val)) {
		LOG_ERR("LED write failed");
		return -EIO;
	}

	ret = lp3943_set_dim_states(data, led, mode);
	if (ret) {
		return ret;
	}

	return 0;
}

static inline int lp3943_led_on(struct device *dev, u32_t led)
{
	struct lp3943_data *data = dev->driver_data;
	int ret;
	u8_t reg, mode;

	ret = lp3943_get_led_reg(&led, &reg);
	if (ret) {
		return ret;
	}

	/* Set LED state to ON */
	mode = LP3943_ON;
	if (i2c_reg_update_byte(data->i2c, CONFIG_LP3943_I2C_ADDRESS, reg,
				LP3943_MASK << (led << 1),
				mode << (led << 1))) {
		LOG_ERR("LED reg update failed");
		return -EIO;
	}

	return 0;
}

static inline int lp3943_led_off(struct device *dev, u32_t led)
{
	struct lp3943_data *data = dev->driver_data;
	int ret;
	u8_t reg;

	ret = lp3943_get_led_reg(&led, &reg);
	if (ret) {
		return ret;
	}

	/* Set LED state to OFF */
	if (i2c_reg_update_byte(data->i2c, CONFIG_LP3943_I2C_ADDRESS, reg,
				LP3943_MASK << (led << 1), 0)) {
		LOG_ERR("LED reg update failed");
		return -EIO;
	}

	return 0;
}

static int lp3943_led_init(struct device *dev)
{
	struct lp3943_data *data = dev->driver_data;
	struct led_data *dev_data = &data->dev_data;

	data->i2c = device_get_binding(CONFIG_LP3943_I2C_MASTER_DEV_NAME);
	if (data->i2c == NULL) {
		LOG_DBG("Failed to get I2C device");
		return -EINVAL;
	}

	/* Hardware specific limits */
	dev_data->min_period = 0U;
	dev_data->max_period = 1600U;
	dev_data->min_brightness = 0U;
	dev_data->max_brightness = 100U;

	return 0;
}

static struct lp3943_data lp3943_led_data;

static const struct led_driver_api lp3943_led_api = {
	.blink = lp3943_led_blink,
	.set_brightness = lp3943_led_set_brightness,
	.on = lp3943_led_on,
	.off = lp3943_led_off,
};

DEVICE_AND_API_INIT(lp3943_led, CONFIG_LP3943_DEV_NAME,
		    &lp3943_led_init, &lp3943_led_data,
		    NULL, POST_KERNEL, CONFIG_LED_INIT_PRIORITY,
		    &lp3943_led_api);
