/*
 * Copyright (c) 2022 SILA Embedded Solutions GmbH
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT rohm_bd8lb600fs

#include <errno.h>

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/init.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/gpio/gpio_utils.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/byteorder.h>

LOG_MODULE_REGISTER(gpio_bd8lb600fs, CONFIG_GPIO_LOG_LEVEL);

#define OUTPUT_OFF_WITH_OPEN_LOAD_DETECTION  0b11
#define OUTPUT_ON			     0b10
#define WAIT_TIME_RESET_ACTIVE_IN_US	     1000
#define WAIT_TIME_RESET_INACTIVE_TO_CS_IN_US 10

struct bd8lb600fs_config {
	/* gpio_driver_config needs to be first */
	struct gpio_driver_config common;

	struct spi_dt_spec bus;
	const struct gpio_dt_spec gpio_reset;
	int gpios_count;
};

struct bd8lb600fs_drv_data {
	/* gpio_driver_data needs to be first */
	struct gpio_driver_data data;
	uint32_t state;	    /* each bit is one output channel, bit 0 = channel 1, ... */
	uint32_t configured; /* each bit defines if the output channel is configured, see state */
	struct k_mutex lock;
	int instance_count_actual;
	int gpios_count_actual;
};

static int write_state(const struct device *dev, uint32_t state)
{
	const struct bd8lb600fs_config *config = dev->config;
	struct bd8lb600fs_drv_data *drv_data = dev->data;

	LOG_DBG("%s: writing state 0x%08X to BD8LB600FS", dev->name, state);

	uint16_t state_converted = 0;
	uint8_t buffer_tx[8];
	const struct spi_buf tx_buf = {
		.buf = buffer_tx,
		.len = drv_data->instance_count_actual * sizeof(state_converted),
	};
	const struct spi_buf_set tx = {
		.buffers = &tx_buf,
		.count = 1,
	};

	memset(buffer_tx, 0x00, sizeof(buffer_tx));

	for (size_t j = 0; j < drv_data->instance_count_actual; ++j) {
		int instance_position = (drv_data->instance_count_actual - j - 1) * 2;

		state_converted = 0;

		for (size_t i = 0; i < 8; ++i) {
			if ((state & BIT(i + j*8)) == 0) {
				state_converted |= OUTPUT_OFF_WITH_OPEN_LOAD_DETECTION << (i * 2);
			} else {
				state_converted |= OUTPUT_ON << (i * 2);
			}
		}

		LOG_DBG("%s: configuration for instance %zu: %04X (position %i)",
			dev->name,
			j,
			state_converted,
			instance_position);
		sys_put_be16(state_converted, buffer_tx + instance_position);
	}

	LOG_HEXDUMP_DBG(buffer_tx, ARRAY_SIZE(buffer_tx), "configuration written out");

	int result = spi_write_dt(&config->bus, &tx);

	if (result != 0) {
		LOG_ERR("spi_write failed with error %i", result);
		return result;
	}

	return 0;
}

static int bd8lb600fs_pin_configure(const struct device *dev, gpio_pin_t pin, gpio_flags_t flags)
{
	struct bd8lb600fs_drv_data *drv_data = dev->data;

	/* cannot execute a bus operation in an ISR context */
	if (k_is_in_isr()) {
		return -EWOULDBLOCK;
	}

	if (pin >= drv_data->gpios_count_actual) {
		LOG_ERR("invalid pin number %i", pin);
		return -EINVAL;
	}

	if ((flags & GPIO_INPUT) != 0) {
		LOG_ERR("cannot configure pin as input");
		return -ENOTSUP;
	}

	if ((flags & GPIO_OUTPUT) == 0) {
		LOG_ERR("pin must be configured as an output");
		return -ENOTSUP;
	}

	if ((flags & GPIO_SINGLE_ENDED) == 0) {
		LOG_ERR("pin must be configured as single ended");
		return -ENOTSUP;
	}

	if ((flags & GPIO_LINE_OPEN_DRAIN) == 0) {
		LOG_ERR("pin must be configured as open drain");
		return -ENOTSUP;
	}

	if ((flags & GPIO_PULL_UP) != 0) {
		LOG_ERR("pin cannot have a pull up configured");
		return -ENOTSUP;
	}

	if ((flags & GPIO_PULL_DOWN) != 0) {
		LOG_ERR("pin cannot have a pull down configured");
		return -ENOTSUP;
	}

	k_mutex_lock(&drv_data->lock, K_FOREVER);

	if ((flags & GPIO_OUTPUT_INIT_LOW) != 0) {
		WRITE_BIT(drv_data->state, pin, 0);
	} else if ((flags & GPIO_OUTPUT_INIT_HIGH) != 0) {
		WRITE_BIT(drv_data->state, pin, 1);
	}

	WRITE_BIT(drv_data->configured, pin, 1);

	int result = write_state(dev, drv_data->state);

	k_mutex_unlock(&drv_data->lock);

	return result;
}

static int bd8lb600fs_port_get_raw(const struct device *dev, uint32_t *value)
{
	LOG_ERR("input pins are not available");
	return -ENOTSUP;
}

static int bd8lb600fs_port_set_masked_raw(const struct device *dev, uint32_t mask, uint32_t value)
{
	struct bd8lb600fs_drv_data *drv_data = dev->data;

	/* cannot execute a bus operation in an ISR context */
	if (k_is_in_isr()) {
		return -EWOULDBLOCK;
	}

	k_mutex_lock(&drv_data->lock, K_FOREVER);
	drv_data->state = (drv_data->state & ~mask) | (mask & value);

	int result = write_state(dev, drv_data->state);

	k_mutex_unlock(&drv_data->lock);

	return result;
}

static int bd8lb600fs_port_set_bits_raw(const struct device *dev, uint32_t mask)
{
	return bd8lb600fs_port_set_masked_raw(dev, mask, mask);
}

static int bd8lb600fs_port_clear_bits_raw(const struct device *dev, uint32_t mask)
{
	return bd8lb600fs_port_set_masked_raw(dev, mask, 0);
}

static int bd8lb600fs_port_toggle_bits(const struct device *dev, uint32_t mask)
{
	struct bd8lb600fs_drv_data *drv_data = dev->data;

	/* cannot execute a bus operation in an ISR context */
	if (k_is_in_isr()) {
		return -EWOULDBLOCK;
	}

	k_mutex_lock(&drv_data->lock, K_FOREVER);
	drv_data->state ^= mask;

	int result = write_state(dev, drv_data->state);

	k_mutex_unlock(&drv_data->lock);

	return result;
}

static const struct gpio_driver_api api_table = {
	.pin_configure = bd8lb600fs_pin_configure,
	.port_get_raw = bd8lb600fs_port_get_raw,
	.port_set_masked_raw = bd8lb600fs_port_set_masked_raw,
	.port_set_bits_raw = bd8lb600fs_port_set_bits_raw,
	.port_clear_bits_raw = bd8lb600fs_port_clear_bits_raw,
	.port_toggle_bits = bd8lb600fs_port_toggle_bits,
};

static int bd8lb600fs_init(const struct device *dev)
{
	const struct bd8lb600fs_config *config = dev->config;
	struct bd8lb600fs_drv_data *drv_data = dev->data;

	if (!spi_is_ready_dt(&config->bus)) {
		LOG_ERR("SPI bus %s not ready", config->bus.bus->name);
		return -ENODEV;
	}

	if (!gpio_is_ready_dt(&config->gpio_reset)) {
		LOG_ERR("%s: reset GPIO is not ready", dev->name);
		return -ENODEV;
	}

	int result = k_mutex_init(&drv_data->lock);

	if (result != 0) {
		LOG_ERR("unable to initialize mutex");
		return result;
	}

	drv_data->instance_count_actual = config->gpios_count / 8;

	if (config->gpios_count % 8 != 0) {
		LOG_ERR("%s: number of GPIOs %i is not a multiple of 8",
			dev->name, config->gpios_count);
		return -EINVAL;
	}

	if (drv_data->instance_count_actual > 4) {
		LOG_ERR("%s: only a maximum of 4 devices are supported for the daisy chaining",
			dev->name);
		return -EINVAL;
	}

	drv_data->gpios_count_actual = drv_data->instance_count_actual * 8;

	result = gpio_pin_configure_dt(&config->gpio_reset, GPIO_OUTPUT_ACTIVE);

	if (result != 0) {
		LOG_ERR("failed to initialize GPIO for reset");
		return result;
	}

	k_busy_wait(WAIT_TIME_RESET_ACTIVE_IN_US);
	gpio_pin_set_dt(&config->gpio_reset, 0);
	k_busy_wait(WAIT_TIME_RESET_INACTIVE_TO_CS_IN_US);

	return 0;
}

#define BD8LB600FS_INIT(inst)                                                                      \
	static const struct bd8lb600fs_config bd8lb600fs_##inst##_config = {                       \
		.common =                                                                          \
			{                                                                          \
				.port_pin_mask = GPIO_PORT_PIN_MASK_FROM_DT_INST(inst),            \
			},                                                                         \
		.bus = SPI_DT_SPEC_INST_GET(                                                       \
			inst, SPI_OP_MODE_MASTER | SPI_MODE_CPHA | SPI_WORD_SET(8), 0),            \
		.gpio_reset = GPIO_DT_SPEC_GET_BY_IDX(DT_DRV_INST(inst), reset_gpios, 0),          \
		.gpios_count = DT_INST_PROP(inst, ngpios), \
	};                                                                                         \
                                                                                                   \
	static struct bd8lb600fs_drv_data bd8lb600fs_##inst##_drvdata = {                          \
		.state = 0x00,                                                                     \
		.configured = 0x00,                                                                \
	};                                                                                         \
                                                                                                   \
	/* This has to be initialized after the SPI peripheral. */                                 \
	DEVICE_DT_INST_DEFINE(inst, bd8lb600fs_init, NULL, &bd8lb600fs_##inst##_drvdata,           \
			      &bd8lb600fs_##inst##_config, POST_KERNEL,                            \
			      CONFIG_GPIO_BD8LB600FS_INIT_PRIORITY, &api_table);

DT_INST_FOREACH_STATUS_OKAY(BD8LB600FS_INIT)
