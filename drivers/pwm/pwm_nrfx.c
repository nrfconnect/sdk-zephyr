/*
 * Copyright (c) 2018, Cue Health Inc
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <nrfx_pwm.h>
#include <drivers/pwm.h>
#include <hal/nrf_gpio.h>
#include <stdbool.h>

#define LOG_LEVEL CONFIG_PWM_LOG_LEVEL
#include <logging/log.h>
LOG_MODULE_REGISTER(pwm_nrfx);

#define PWM_NRFX_CH_POLARITY_MASK       BIT(15)
#define PWM_NRFX_CH_PULSE_CYCLES_MASK   BIT_MASK(15)
#define PWM_NRFX_CH_VALUE_NORMAL        PWM_NRFX_CH_POLARITY_MASK
#define PWM_NRFX_CH_VALUE_INVERTED      (0)
#define PWM_NRFX_CH_PIN_MASK            ~NRFX_PWM_PIN_INVERTED

struct pwm_nrfx_config {
	nrfx_pwm_t pwm;
	nrfx_pwm_config_t initial_config;
	nrf_pwm_sequence_t seq;
};

struct pwm_nrfx_data {
	u32_t period_cycles;
	u16_t current[NRF_PWM_CHANNEL_COUNT];
	u16_t countertop;
	u8_t  prescaler;
};


static int pwm_period_check_and_set(const struct pwm_nrfx_config *config,
				    struct pwm_nrfx_data *data,
				    u32_t channel,
				    u32_t period_cycles)
{
	u8_t i;
	u8_t prescaler;
	u32_t countertop;

	/* If any other channel (other than the one being configured) is set up
	 * with a non-zero pulse cycle, the period that is currently set cannot
	 * be changed, as this would influence the output for this channel.
	 */
	for (i = 0; i < NRF_PWM_CHANNEL_COUNT; ++i) {
		if (i != channel) {
			u16_t channel_pulse_cycle =
				data->current[i]
				& PWM_NRFX_CH_PULSE_CYCLES_MASK;
			if (channel_pulse_cycle > 0) {
				LOG_ERR("Incompatible period.");
				return -EINVAL;
			}
		}
	}

	/* Try to find a prescaler that will allow setting the requested period
	 * after prescaling as the countertop value for the PWM peripheral.
	 */
	prescaler = 0;
	countertop = period_cycles;
	do {
		if (countertop <= PWM_COUNTERTOP_COUNTERTOP_Msk) {
			data->period_cycles = period_cycles;
			data->prescaler     = prescaler;
			data->countertop    = (u16_t)countertop;

			nrf_pwm_configure(config->pwm.p_registers,
					  data->prescaler,
					  config->initial_config.count_mode,
					  data->countertop);
			return 0;
		}

		countertop >>= 1;
		++prescaler;
	} while (prescaler <= PWM_PRESCALER_PRESCALER_Msk);

	LOG_ERR("Prescaler for period_cycles %u not found.", period_cycles);
	return -EINVAL;
}

static u8_t pwm_channel_map(const uint8_t *output_pins, u32_t pwm)
{
	u8_t i;

	/* Find pin, return channel number */
	for (i = 0U; i < NRF_PWM_CHANNEL_COUNT; i++) {
		if (output_pins[i] != NRFX_PWM_PIN_NOT_USED
		    && (pwm == (output_pins[i] & PWM_NRFX_CH_PIN_MASK))) {
			return i;
		}
	}

	/* Return NRF_PWM_CHANNEL_COUNT to show that PWM pin was not found. */
	return NRF_PWM_CHANNEL_COUNT;
}

static bool pwm_channel_is_active(u8_t channel,
				  const struct pwm_nrfx_data *data)
{
	u16_t pulse_cycle =
		data->current[channel] & PWM_NRFX_CH_PULSE_CYCLES_MASK;

	return (pulse_cycle > 0 && pulse_cycle < data->countertop);
}

static bool any_other_channel_is_active(u8_t channel,
					const struct pwm_nrfx_data *data)
{
	u8_t i;

	for (i = 0; i < NRF_PWM_CHANNEL_COUNT; ++i) {
		if (i != channel && pwm_channel_is_active(i, data)) {
			return true;
		}
	}

	return false;
}

static int pwm_nrfx_pin_set(struct device *dev, u32_t pwm,
			    u32_t period_cycles, u32_t pulse_cycles,
			    pwm_flags_t flags)
{
	/* We assume here that period_cycles will always be 16MHz
	 * peripheral clock. Since pwm_nrfx_get_cycles_per_sec() function might
	 * be removed, see ISSUE #6958.
	 * TODO: Remove this comment when issue has been resolved.
	 */
	const struct pwm_nrfx_config *config = dev->config->config_info;
	struct pwm_nrfx_data *data = dev->driver_data;
	u8_t channel;

	if (flags) {
		/* PWM polarity not supported (yet?) */
		return -ENOTSUP;
	}

	/* Check if PWM pin is one of the predefiend DTS config pins.
	 * Return its array index (channel number),
	 * or NRF_PWM_CHANNEL_COUNT if not initialized through DTS.
	 */
	channel = pwm_channel_map(config->initial_config.output_pins, pwm);
	if (channel == NRF_PWM_CHANNEL_COUNT) {
		LOG_ERR("PWM pin %d not enabled through DTS configuration.",
			pwm);
		return -EINVAL;
	}

	/* If this PWM is in center-aligned mode, pulse and period lengths
	 * are effectively doubled by the up-down count, so halve them here
	 * to compensate.
	 */
	if (config->initial_config.count_mode == NRF_PWM_MODE_UP_AND_DOWN) {
		period_cycles /= 2;
		pulse_cycles /= 2;
	}

	/* Check if period_cycle is either matching currently used, or
	 * possible to use with our prescaler options.
	 */
	if (period_cycles != data->period_cycles) {
		int ret = pwm_period_check_and_set(config, data, channel,
						   period_cycles);
		if (ret) {
			return ret;
		}
	}

	/* Limit pulse cycles to period cycles (meaning 100% duty), bigger
	 * values might not fit after prescaling into the 15-bit field that
	 * is filled below.
	 */
	pulse_cycles = MIN(pulse_cycles, period_cycles);

	/* Store new pulse value bit[14:0], and polarity bit[15] for channel. */
	data->current[channel] = (
		(data->current[channel] & PWM_NRFX_CH_POLARITY_MASK)
		| (pulse_cycles >> data->prescaler));

	LOG_DBG("pin %u, pulse %u, period %u, prescaler: %u.",
		pwm, pulse_cycles, period_cycles, data->prescaler);

	/* If this channel turns out to not need to be driven by the PWM
	 * peripheral (it is off or fully on - duty 0% or 100%), set properly
	 * the GPIO configuration for its output pin. This will provide
	 * the correct output state for this channel when the PWM peripheral
	 * is disabled after all channels appear to be inactive.
	 */
	if (!pwm_channel_is_active(channel, data)) {
		/* If pulse 0% and pin not inverted, set LOW.
		 * If pulse 100% and pin inverted, set LOW.
		 * If pulse 0% and pin inverted, set HIGH.
		 * If pulse 100% and pin not inverted, set HIGH.
		 */
		bool channel_inverted_state =
			config->initial_config.output_pins[channel]
			& NRFX_PWM_PIN_INVERTED;

		bool pulse_0_and_not_inverted =
			(pulse_cycles == 0U)
			&& !channel_inverted_state;
		bool pulse_100_and_inverted =
			(pulse_cycles == period_cycles)
			&& channel_inverted_state;

		if (pulse_0_and_not_inverted || pulse_100_and_inverted) {
			nrf_gpio_pin_clear(pwm);
		} else {
			nrf_gpio_pin_set(pwm);
		}

		if (!any_other_channel_is_active(channel, data)) {
			nrfx_pwm_stop(&config->pwm, false);
		}
	} else {
		/* Since we are playing the sequence in a loop, the
		 * sequence only has to be started when its not already
		 * playing. The new channel values will be used
		 * immediately when they are written into the seq array.
		 */
		if (nrfx_pwm_is_stopped(&config->pwm)) {
			nrfx_pwm_simple_playback(&config->pwm,
				&config->seq,
				1,
				NRFX_PWM_FLAG_LOOP);
		}
	}

	return 0;
}

static int pwm_nrfx_get_cycles_per_sec(struct device *dev, u32_t pwm,
				       u64_t *cycles)
{
	/* TODO: Since this function might be removed, we will always return
	 * 16MHz from this function and handle the conversion with prescaler,
	 * etc, in the pin set function. See issue #6958.
	 */
	*cycles = 16ul * 1000ul * 1000ul;

	return 0;
}

static const struct pwm_driver_api pwm_nrfx_drv_api_funcs = {
	.pin_set = pwm_nrfx_pin_set,
	.get_cycles_per_sec = pwm_nrfx_get_cycles_per_sec,
};

static int pwm_nrfx_init(struct device *dev)
{
	const struct pwm_nrfx_config *config = dev->config->config_info;

	nrfx_err_t result = nrfx_pwm_init(&config->pwm,
					  &config->initial_config,
					  NULL,
					  NULL);
	if (result != NRFX_SUCCESS) {
		LOG_ERR("Failed to initialize device: %s", dev->config->name);
		return -EBUSY;
	}

	return 0;
}

#ifdef CONFIG_DEVICE_POWER_MANAGEMENT

static void pwm_nrfx_uninit(struct device *dev)
{
	const struct pwm_nrfx_config *config = dev->config->config_info;

	nrfx_pwm_uninit(&config->pwm);
}

static int pwm_nrfx_set_power_state(u32_t new_state,
				    u32_t current_state,
				    struct device *dev)
{
	int err = 0;

	switch (new_state) {
	case DEVICE_PM_ACTIVE_STATE:
		err = pwm_nrfx_init(dev);
		break;
	case DEVICE_PM_LOW_POWER_STATE:
	case DEVICE_PM_SUSPEND_STATE:
	case DEVICE_PM_FORCE_SUSPEND_STATE:
	case DEVICE_PM_OFF_STATE:
		if (current_state == DEVICE_PM_ACTIVE_STATE) {
			pwm_nrfx_uninit(dev);
		}
		break;
	default:
		assert(false);
		break;
	}
	return err;
}

static int pwm_nrfx_pm_control(struct device *dev,
			       u32_t ctrl_command,
			       void *context,
			       u32_t *current_state)
{
	int err = 0;

	if (ctrl_command == DEVICE_PM_SET_POWER_STATE) {
		u32_t new_state = *((const u32_t *)context);

		if (new_state != (*current_state)) {
			err = pwm_nrfx_set_power_state(new_state,
						       *current_state,
						       dev);
			if (!err) {
				(*current_state) = new_state;
			}
		}
	} else {
		assert(ctrl_command == DEVICE_PM_GET_POWER_STATE);
		*((u32_t *)context) = (*current_state);
	}

	return err;
}

#define PWM_NRFX_PM_CONTROL(idx)					\
	static int pwm_##idx##_nrfx_pm_control(struct device *dev,	\
					       u32_t ctrl_command,	\
					       void *context,		\
					       device_pm_cb cb,		\
					       void *arg)		\
	{								\
		static u32_t current_state = DEVICE_PM_ACTIVE_STATE;	\
		int ret = 0;                                            \
		ret = pwm_nrfx_pm_control(dev, ctrl_command, context,	\
					   &current_state);		\
		if (cb) {                                               \
			cb(dev, ret, context, arg);                     \
		}                                                       \
		return ret;                                             \
	}
#else

#define PWM_NRFX_PM_CONTROL(idx)

#endif /* CONFIG_DEVICE_POWER_MANAGEMENT */

#define PWM_NRFX_IS_INVERTED(dev_idx, ch_idx)				      \
	IS_ENABLED(DT_NORDIC_NRF_PWM_PWM_##dev_idx##_CH##ch_idx##_INVERTED)

#define PWM_NRFX_OUTPUT_PIN(dev_idx, ch_idx)				      \
	(DT_NORDIC_NRF_PWM_PWM_##dev_idx##_CH##ch_idx##_PIN |		      \
	 (PWM_NRFX_IS_INVERTED(dev_idx, ch_idx) ? NRFX_PWM_PIN_INVERTED : 0))

#define PWM_NRFX_DEFAULT_VALUE(dev_idx, ch_idx)				      \
	(PWM_NRFX_IS_INVERTED(dev_idx, ch_idx) ?			      \
	 PWM_NRFX_CH_VALUE_INVERTED : PWM_NRFX_CH_VALUE_NORMAL)

#define PWM_NRFX_COUNT_MODE(dev_idx)                                          \
	(IS_ENABLED(DT_NORDIC_NRF_PWM_PWM_##dev_idx##_CENTER_ALIGNED) ?	      \
	 NRF_PWM_MODE_UP_AND_DOWN : NRF_PWM_MODE_UP)

#define PWM_NRFX_DEVICE(idx)						      \
	static struct pwm_nrfx_data pwm_nrfx_##idx##_data = {		      \
		.current = {						      \
			PWM_NRFX_DEFAULT_VALUE(idx, 0),			      \
			PWM_NRFX_DEFAULT_VALUE(idx, 1),			      \
			PWM_NRFX_DEFAULT_VALUE(idx, 2),			      \
			PWM_NRFX_DEFAULT_VALUE(idx, 3),			      \
		}							      \
	};								      \
	static const struct pwm_nrfx_config pwm_nrfx_##idx##config = {	      \
		.pwm = NRFX_PWM_INSTANCE(idx),				      \
		.initial_config = {					      \
			.output_pins = {				      \
				PWM_NRFX_OUTPUT_PIN(idx, 0),		      \
				PWM_NRFX_OUTPUT_PIN(idx, 1),		      \
				PWM_NRFX_OUTPUT_PIN(idx, 2),		      \
				PWM_NRFX_OUTPUT_PIN(idx, 3),		      \
			},						      \
			.base_clock = NRF_PWM_CLK_1MHz,			      \
			.count_mode = PWM_NRFX_COUNT_MODE(idx),		      \
			.top_value = 1000,				      \
			.load_mode = NRF_PWM_LOAD_INDIVIDUAL,		      \
			.step_mode = NRF_PWM_STEP_TRIGGERED,		      \
		},							      \
		.seq.values.p_raw = pwm_nrfx_##idx##_data.current,	      \
		.seq.length = NRF_PWM_CHANNEL_COUNT			      \
	};								      \
	PWM_NRFX_PM_CONTROL(idx)					      \
	DEVICE_DEFINE(pwm_nrfx_##idx,					      \
		      DT_NORDIC_NRF_PWM_PWM_##idx##_LABEL,		      \
		      pwm_nrfx_init, pwm_##idx##_nrfx_pm_control,	      \
		      &pwm_nrfx_##idx##_data,				      \
		      &pwm_nrfx_##idx##config,				      \
		      POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEVICE,	      \
		      &pwm_nrfx_drv_api_funcs)

#ifdef CONFIG_PWM_0
#ifndef DT_NORDIC_NRF_PWM_PWM_0_CH0_PIN
#define DT_NORDIC_NRF_PWM_PWM_0_CH0_PIN NRFX_PWM_PIN_NOT_USED
#endif
#ifndef DT_NORDIC_NRF_PWM_PWM_0_CH1_PIN
#define DT_NORDIC_NRF_PWM_PWM_0_CH1_PIN NRFX_PWM_PIN_NOT_USED
#endif
#ifndef DT_NORDIC_NRF_PWM_PWM_0_CH2_PIN
#define DT_NORDIC_NRF_PWM_PWM_0_CH2_PIN NRFX_PWM_PIN_NOT_USED
#endif
#ifndef DT_NORDIC_NRF_PWM_PWM_0_CH3_PIN
#define DT_NORDIC_NRF_PWM_PWM_0_CH3_PIN NRFX_PWM_PIN_NOT_USED
#endif
PWM_NRFX_DEVICE(0);
#endif

#ifdef CONFIG_PWM_1
#ifndef DT_NORDIC_NRF_PWM_PWM_1_CH0_PIN
#define DT_NORDIC_NRF_PWM_PWM_1_CH0_PIN NRFX_PWM_PIN_NOT_USED
#endif
#ifndef DT_NORDIC_NRF_PWM_PWM_1_CH1_PIN
#define DT_NORDIC_NRF_PWM_PWM_1_CH1_PIN NRFX_PWM_PIN_NOT_USED
#endif
#ifndef DT_NORDIC_NRF_PWM_PWM_1_CH2_PIN
#define DT_NORDIC_NRF_PWM_PWM_1_CH2_PIN NRFX_PWM_PIN_NOT_USED
#endif
#ifndef DT_NORDIC_NRF_PWM_PWM_1_CH3_PIN
#define DT_NORDIC_NRF_PWM_PWM_1_CH3_PIN NRFX_PWM_PIN_NOT_USED
#endif
PWM_NRFX_DEVICE(1);
#endif

#ifdef CONFIG_PWM_2
#ifndef DT_NORDIC_NRF_PWM_PWM_2_CH0_PIN
#define DT_NORDIC_NRF_PWM_PWM_2_CH0_PIN NRFX_PWM_PIN_NOT_USED
#endif
#ifndef DT_NORDIC_NRF_PWM_PWM_2_CH1_PIN
#define DT_NORDIC_NRF_PWM_PWM_2_CH1_PIN NRFX_PWM_PIN_NOT_USED
#endif
#ifndef DT_NORDIC_NRF_PWM_PWM_2_CH2_PIN
#define DT_NORDIC_NRF_PWM_PWM_2_CH2_PIN NRFX_PWM_PIN_NOT_USED
#endif
#ifndef DT_NORDIC_NRF_PWM_PWM_2_CH3_PIN
#define DT_NORDIC_NRF_PWM_PWM_2_CH3_PIN NRFX_PWM_PIN_NOT_USED
#endif
PWM_NRFX_DEVICE(2);
#endif

#ifdef CONFIG_PWM_3
#ifndef DT_NORDIC_NRF_PWM_PWM_3_CH0_PIN
#define DT_NORDIC_NRF_PWM_PWM_3_CH0_PIN NRFX_PWM_PIN_NOT_USED
#endif
#ifndef DT_NORDIC_NRF_PWM_PWM_3_CH1_PIN
#define DT_NORDIC_NRF_PWM_PWM_3_CH1_PIN NRFX_PWM_PIN_NOT_USED
#endif
#ifndef DT_NORDIC_NRF_PWM_PWM_3_CH2_PIN
#define DT_NORDIC_NRF_PWM_PWM_3_CH2_PIN NRFX_PWM_PIN_NOT_USED
#endif
#ifndef DT_NORDIC_NRF_PWM_PWM_3_CH3_PIN
#define DT_NORDIC_NRF_PWM_PWM_3_CH3_PIN NRFX_PWM_PIN_NOT_USED
#endif
PWM_NRFX_DEVICE(3);
#endif
