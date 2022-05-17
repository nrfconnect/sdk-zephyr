/*
 * Copyright (c) 2020 Seagate Technology LLC
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT pwm_leds

/**
 * @file
 * @brief PWM driven LEDs
 */

#include <drivers/led.h>
#include <drivers/pwm.h>
#include <device.h>
#include <pm/device.h>
#include <zephyr.h>
#include <sys/math_extras.h>

#include <logging/log.h>
LOG_MODULE_REGISTER(led_pwm, CONFIG_LED_LOG_LEVEL);

struct led_pwm {
	const struct device *dev;
	uint32_t channel;
	uint32_t period;
	pwm_flags_t flags;
};

struct led_pwm_config {
	int num_leds;
	const struct led_pwm *led;
};

static int led_pwm_blink(const struct device *dev, uint32_t led,
			 uint32_t delay_on, uint32_t delay_off)
{
	const struct led_pwm_config *config = dev->config;
	const struct led_pwm *led_pwm;
	uint32_t period_usec, pulse_usec;

	if (led >= config->num_leds) {
		return -EINVAL;
	}

	/*
	 * Convert delays (in ms) into PWM period and pulse (in us) and check
	 * for overflows.
	 */
	if (u32_add_overflow(delay_on, delay_off, &period_usec) ||
	    u32_mul_overflow(period_usec, 1000, &period_usec) ||
	    u32_mul_overflow(delay_on, 1000, &pulse_usec)) {
		return -EINVAL;
	}

	led_pwm = &config->led[led];

	return pwm_set_usec(led_pwm->dev, led_pwm->channel, period_usec,
			    pulse_usec, led_pwm->flags);
}

static int led_pwm_set_brightness(const struct device *dev,
				  uint32_t led, uint8_t value)
{
	const struct led_pwm_config *config = dev->config;
	const struct led_pwm *led_pwm;
	uint32_t pulse;

	if (led >= config->num_leds || value > 100) {
		return -EINVAL;
	}

	led_pwm = &config->led[led];

	pulse = led_pwm->period * value / 100;

	return pwm_set_nsec(led_pwm->dev, led_pwm->channel, led_pwm->period,
			    pulse, led_pwm->flags);
}

static int led_pwm_on(const struct device *dev, uint32_t led)
{
	return led_pwm_set_brightness(dev, led, 100);
}

static int led_pwm_off(const struct device *dev, uint32_t led)
{
	return led_pwm_set_brightness(dev, led, 0);
}

static int led_pwm_init(const struct device *dev)
{
	const struct led_pwm_config *config = dev->config;
	int i;

	if (!config->num_leds) {
		LOG_ERR("%s: no LEDs found (DT child nodes missing)",
			dev->name);
		return -ENODEV;
	}

	for (i = 0; i < config->num_leds; i++) {
		const struct led_pwm *led = &config->led[i];

		if (!device_is_ready(led->dev)) {
			LOG_ERR("%s: pwm device not ready", dev->name);
			return -ENODEV;
		}
	}

	return 0;
}

#ifdef CONFIG_PM_DEVICE
static int led_pwm_pm_action(const struct device *dev,
			     enum pm_device_action action)
{
	const struct led_pwm_config *config = dev->config;

	/* switch all underlying PWM devices to the new state */
	for (size_t i = 0; i < config->num_leds; i++) {
		int err;
		const struct led_pwm *led_pwm = &config->led[i];

		LOG_DBG("PWM %p running pm action %" PRIu32, led_pwm->dev,
				action);

		err = pm_device_action_run(led_pwm->dev, action);
		if (err && (err != -EALREADY)) {
			LOG_ERR("Cannot switch PWM %p power state", led_pwm->dev);
		}
	}

	return 0;
}
#endif /* CONFIG_PM_DEVICE */

static const struct led_driver_api led_pwm_api = {
	.on		= led_pwm_on,
	.off		= led_pwm_off,
	.blink		= led_pwm_blink,
	.set_brightness	= led_pwm_set_brightness,
};

#define LED_PWM(led_node_id)						\
{									\
	.dev		= DEVICE_DT_GET(DT_PWMS_CTLR(led_node_id)),	\
	.channel	= DT_PWMS_CHANNEL(led_node_id),			\
	.period		= DT_PHA_OR(led_node_id, pwms, period, 100000),	\
	.flags		= DT_PHA_OR(led_node_id, pwms, flags,		\
				    PWM_POLARITY_NORMAL),		\
},

#define LED_PWM_DEVICE(id)					\
								\
static const struct led_pwm led_pwm_##id[] = {			\
	DT_INST_FOREACH_CHILD(id, LED_PWM)			\
};								\
								\
static const struct led_pwm_config led_pwm_config_##id = {	\
	.num_leds	= ARRAY_SIZE(led_pwm_##id),		\
	.led		= led_pwm_##id,				\
};								\
								\
PM_DEVICE_DT_INST_DEFINE(id, led_pwm_pm_action);		\
								\
DEVICE_DT_INST_DEFINE(id, &led_pwm_init,			\
		      PM_DEVICE_DT_INST_GET(id), NULL,		\
		      &led_pwm_config_##id, POST_KERNEL,	\
		      CONFIG_LED_INIT_PRIORITY, &led_pwm_api);

DT_INST_FOREACH_STATUS_OKAY(LED_PWM_DEVICE)
