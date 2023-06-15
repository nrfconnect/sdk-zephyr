/*
 * Copyright (c) 2022 Google LLC
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/input/input.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(zephyr_gpio_keys, CONFIG_INPUT_LOG_LEVEL);

#define DT_DRV_COMPAT zephyr_gpio_keys

struct gpio_keys_callback {
	struct gpio_callback gpio_cb;
	uint32_t zephyr_code;
	int8_t pin_state;
};

struct gpio_keys_pin_config {
	/** GPIO specification from devicetree */
	struct gpio_dt_spec spec;
	/** Zephyr code from devicetree */
	uint32_t zephyr_code;
};
struct gpio_keys_config {
	/** Debounce interval in milliseconds from devicetree */
	uint32_t debounce_interval_ms;
	const int num_keys;
	const struct gpio_keys_pin_config *pin_cfg;
};

struct gpio_keys_pin_data {
	const struct device *dev;
	struct gpio_keys_callback cb_data;
	struct k_work_delayable work;
	int8_t pin_state;
};

struct gpio_keys_data {
	struct gpio_keys_pin_data *pin_data;
};

/**
 * Handle debounced gpio pin state.
 */
static void gpio_keys_change_deferred(struct k_work *work)
{
	struct gpio_keys_pin_data *pin_data = CONTAINER_OF(work, struct gpio_keys_pin_data, work);
	const struct device *dev = pin_data->dev;
	struct gpio_keys_data *data = dev->data;
	int key_index = pin_data - &data->pin_data[0];
	const struct gpio_keys_config *cfg = dev->config;
	const struct gpio_keys_pin_config *pin_cfg = &cfg->pin_cfg[key_index];

	const int new_pressed = gpio_pin_get(pin_cfg->spec.port, pin_cfg->spec.pin);

	LOG_DBG("gpio_change_deferred %s pin_state=%d, new_pressed=%d, key_index=%d", dev->name,
		pin_data->cb_data.pin_state, new_pressed, key_index);

	/* If gpio changed, report the event */
	if (new_pressed != pin_data->cb_data.pin_state) {
		pin_data->cb_data.pin_state = new_pressed;
		LOG_DBG("Report event %s %d, code=%d", dev->name, new_pressed,
			pin_cfg->zephyr_code);
		input_report_key(dev, pin_cfg->zephyr_code, new_pressed, true, K_FOREVER);
	}
}

static void gpio_keys_change_call_deferred(struct gpio_keys_pin_data *data, uint32_t msec)
{
	int __maybe_unused rv = k_work_reschedule(&data->work, K_MSEC(msec));

	__ASSERT(rv >= 0, "Set wake mask work queue error");
}

static void gpio_keys_interrupt(const struct device *dev, struct gpio_callback *cbdata,
				uint32_t pins)
{
	ARG_UNUSED(dev); /* This is a pointer to GPIO device, use dev pointer in
			  * cbdata for pointer to gpio_debounce device node
			  */
	struct gpio_keys_pin_data *pin_data =
		CONTAINER_OF(cbdata, struct gpio_keys_pin_data, cb_data);
	const struct gpio_keys_config *cfg = pin_data->dev->config;

	for (int i = 0; i < cfg->num_keys; i++) {
		if (pins & BIT(cfg->pin_cfg[i].spec.pin)) {
			gpio_keys_change_call_deferred(pin_data, cfg->debounce_interval_ms);
		}
	}
}

static int gpio_keys_interrupt_configure(const struct gpio_dt_spec *gpio_spec,
					 struct gpio_keys_callback *cb, uint32_t zephyr_code)
{
	int retval;
	gpio_flags_t flags;

	gpio_init_callback(&cb->gpio_cb, gpio_keys_interrupt, BIT(gpio_spec->pin));

	retval = gpio_add_callback(gpio_spec->port, &cb->gpio_cb);
	if (retval < 0) {
		LOG_ERR("Could not set gpio callback");
		return retval;
	}

	cb->zephyr_code = zephyr_code;
	cb->pin_state = -1;
	flags = GPIO_INT_EDGE_BOTH & ~GPIO_INT_MODE_DISABLED;

	LOG_DBG("%s [0x%p, %d]", __func__, gpio_spec->port, gpio_spec->pin);

	retval = z_impl_gpio_pin_interrupt_configure(gpio_spec->port, gpio_spec->pin, flags);

	return retval;
}

static int gpio_keys_init(const struct device *dev)
{
	struct gpio_keys_data *data = dev->data;
	const struct gpio_keys_config *cfg = dev->config;
	int ret;

	for (int i = 0; i < cfg->num_keys; i++) {
		const struct gpio_dt_spec *gpio = &cfg->pin_cfg[i].spec;

		if (!gpio_is_ready_dt(gpio)) {
			LOG_ERR("%s is not ready", gpio->port->name);
			return -ENODEV;
		}

		ret = gpio_pin_configure_dt(gpio, GPIO_INPUT);
		if (ret != 0) {
			LOG_ERR("Pin %d configuration failed: %d", i, ret);
			return ret;
		}

		data->pin_data[i].dev = dev;
		k_work_init_delayable(&data->pin_data[i].work, gpio_keys_change_deferred);

		ret = gpio_keys_interrupt_configure(&cfg->pin_cfg[i].spec,
						    &data->pin_data[i].cb_data,
						    cfg->pin_cfg[i].zephyr_code);
		if (ret != 0) {
			LOG_ERR("Pin %d interrupt configuration failed: %d", i, ret);
			return ret;
		}
	}

	return 0;
}

#define GPIO_KEYS_CFG_DEF(node_id)                                                                 \
	{                                                                                          \
		.spec = GPIO_DT_SPEC_GET(node_id, gpios),                                          \
		.zephyr_code = DT_PROP(node_id, zephyr_code),                                      \
	}

#define GPIO_KEYS_INIT(i)                                                                          \
	static const struct gpio_keys_pin_config gpio_keys_pin_config_##i[] = {                    \
		DT_INST_FOREACH_CHILD_STATUS_OKAY_SEP(i, GPIO_KEYS_CFG_DEF, (,))};                 \
	static struct gpio_keys_config gpio_keys_config_##i = {                                    \
		.debounce_interval_ms = DT_INST_PROP(i, debounce_interval_ms),                     \
		.num_keys = ARRAY_SIZE(gpio_keys_pin_config_##i),                                  \
		.pin_cfg = gpio_keys_pin_config_##i,                                               \
	};                                                                                         \
	static struct gpio_keys_pin_data                                                           \
		gpio_keys_pin_data_##i[ARRAY_SIZE(gpio_keys_pin_config_##i)];                      \
	static struct gpio_keys_data gpio_keys_data_##i = {                                        \
		.pin_data = gpio_keys_pin_data_##i,                                                \
	};                                                                                         \
	DEVICE_DT_INST_DEFINE(i, &gpio_keys_init, NULL, &gpio_keys_data_##i,                       \
			      &gpio_keys_config_##i, POST_KERNEL, CONFIG_INPUT_INIT_PRIORITY,      \
			      NULL);

DT_INST_FOREACH_STATUS_OKAY(GPIO_KEYS_INIT)
