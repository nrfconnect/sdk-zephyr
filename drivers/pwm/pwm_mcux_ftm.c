/*
 * Copyright (c) 2017, NXP
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <drivers/clock_control.h>
#include <errno.h>
#include <drivers/pwm.h>
#include <soc.h>
#include <fsl_ftm.h>
#include <fsl_clock.h>

#define LOG_LEVEL CONFIG_PWM_LOG_LEVEL
#include <logging/log.h>
LOG_MODULE_REGISTER(pwm_mcux_ftm);

#define MAX_CHANNELS ARRAY_SIZE(FTM0->CONTROLS)

struct mcux_ftm_config {
	FTM_Type *base;
	char *clock_name;
	clock_control_subsys_t clock_subsys;
	ftm_clock_source_t ftm_clock_source;
	ftm_clock_prescale_t prescale;
	u8_t channel_count;
	ftm_pwm_mode_t mode;
};

struct mcux_ftm_data {
	u32_t clock_freq;
	u32_t period_cycles;
	ftm_chnl_pwm_signal_param_t channel[MAX_CHANNELS];
};

static int mcux_ftm_pin_set(struct device *dev, u32_t pwm,
			    u32_t period_cycles, u32_t pulse_cycles,
			    pwm_flags_t flags)
{
	const struct mcux_ftm_config *config = dev->config->config_info;
	struct mcux_ftm_data *data = dev->driver_data;
	u8_t duty_cycle;

	if ((period_cycles == 0U) || (pulse_cycles > period_cycles)) {
		LOG_ERR("Invalid combination: period_cycles=%d, "
			    "pulse_cycles=%d", period_cycles, pulse_cycles);
		return -EINVAL;
	}

	if (pwm >= config->channel_count) {
		LOG_ERR("Invalid channel");
		return -ENOTSUP;
	}

	duty_cycle = pulse_cycles * 100U / period_cycles;
	data->channel[pwm].dutyCyclePercent = duty_cycle;

	if ((flags & PWM_POLARITY_INVERTED) == 0) {
		data->channel[pwm].level = kFTM_HighTrue;
	} else {
		data->channel[pwm].level = kFTM_LowTrue;
	}

	LOG_DBG("pulse_cycles=%d, period_cycles=%d, duty_cycle=%d, flags=%d",
		pulse_cycles, period_cycles, duty_cycle, flags);

	if (period_cycles != data->period_cycles) {
		u32_t pwm_freq;
		status_t status;

		if (data->period_cycles != 0) {
			/* Only warn when not changing from zero */
			LOG_WRN("Changing period cycles from %d to %d"
				" affects all %d channels in %s",
				data->period_cycles, period_cycles,
				config->channel_count, dev->config->name);
		}

		data->period_cycles = period_cycles;

		pwm_freq = (data->clock_freq >> config->prescale) /
			   period_cycles;

		LOG_DBG("pwm_freq=%d, clock_freq=%d", pwm_freq,
			data->clock_freq);

		if (pwm_freq == 0U) {
			LOG_ERR("Could not set up pwm_freq=%d", pwm_freq);
			return -EINVAL;
		}

		FTM_StopTimer(config->base);

		status = FTM_SetupPwm(config->base, data->channel,
				      config->channel_count, config->mode,
				      pwm_freq, data->clock_freq);

		if (status != kStatus_Success) {
			LOG_ERR("Could not set up pwm");
			return -ENOTSUP;
		}
		FTM_SetSoftwareTrigger(config->base, true);
		FTM_StartTimer(config->base, config->ftm_clock_source);

	} else {
		FTM_UpdatePwmDutycycle(config->base, pwm, config->mode,
				       duty_cycle);
		FTM_UpdateChnlEdgeLevelSelect(config->base, pwm,
					      data->channel[pwm].level);
		FTM_SetSoftwareTrigger(config->base, true);
	}

	return 0;
}

static int mcux_ftm_get_cycles_per_sec(struct device *dev, u32_t pwm,
				       u64_t *cycles)
{
	const struct mcux_ftm_config *config = dev->config->config_info;
	struct mcux_ftm_data *data = dev->driver_data;

	*cycles = data->clock_freq >> config->prescale;

	return 0;
}

static int mcux_ftm_init(struct device *dev)
{
	const struct mcux_ftm_config *config = dev->config->config_info;
	struct mcux_ftm_data *data = dev->driver_data;
	ftm_chnl_pwm_signal_param_t *channel = data->channel;
	struct device *clock_dev;
	ftm_config_t ftm_config;
	int i;

	if (config->channel_count > ARRAY_SIZE(data->channel)) {
		LOG_ERR("Invalid channel count");
		return -EINVAL;
	}

	clock_dev = device_get_binding(config->clock_name);
	if (clock_dev == NULL) {
		LOG_ERR("Could not get clock device");
		return -EINVAL;
	}

	if (clock_control_get_rate(clock_dev, config->clock_subsys,
				   &data->clock_freq)) {
		LOG_ERR("Could not get clock frequency");
		return -EINVAL;
	}

	for (i = 0; i < config->channel_count; i++) {
		channel->chnlNumber = i;
		channel->level = kFTM_NoPwmSignal;
		channel->dutyCyclePercent = 0;
		channel->firstEdgeDelayPercent = 0;
		channel++;
	}

	FTM_GetDefaultConfig(&ftm_config);
	ftm_config.prescale = config->prescale;

	FTM_Init(config->base, &ftm_config);

	return 0;
}

static const struct pwm_driver_api mcux_ftm_driver_api = {
	.pin_set = mcux_ftm_pin_set,
	.get_cycles_per_sec = mcux_ftm_get_cycles_per_sec,
};

#define FTM_DEVICE(n) \
	static const struct mcux_ftm_config mcux_ftm_config_##n = { \
		.base = (FTM_Type *)DT_INST_##n##_NXP_KINETIS_FTM_BASE_ADDRESS,\
		.clock_name = DT_INST_##n##_NXP_KINETIS_FTM_CLOCK_CONTROLLER, \
		.clock_subsys = (clock_control_subsys_t) \
			DT_INST_##n##_NXP_KINETIS_FTM_CLOCK_NAME, \
		.ftm_clock_source = kFTM_FixedClock, \
		.prescale = kFTM_Prescale_Divide_16, \
		.channel_count = FSL_FEATURE_FTM_CHANNEL_COUNTn((FTM_Type *) \
			DT_INST_##n##_NXP_KINETIS_FTM_BASE_ADDRESS), \
		.mode = kFTM_EdgeAlignedPwm, \
	}; \
	static struct mcux_ftm_data mcux_ftm_data_##n; \
	DEVICE_AND_API_INIT(mcux_ftm_##n, DT_INST_##n##_NXP_KINETIS_FTM_LABEL, \
			    &mcux_ftm_init, &mcux_ftm_data_##n, \
			    &mcux_ftm_config_##n, \
			    POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEVICE, \
			    &mcux_ftm_driver_api)

#if DT_INST_0_NXP_KINETIS_FTM
FTM_DEVICE(0);
#endif /* DT_INST_0_NXP_KINETIS_FTM */

#if DT_INST_1_NXP_KINETIS_FTM
FTM_DEVICE(1);
#endif /* DT_INST_1_NXP_KINETIS_FTM */

#if DT_INST_2_NXP_KINETIS_FTM
FTM_DEVICE(2);
#endif /* DT_INST_2_NXP_KINETIS_FTM */

#if DT_INST_3_NXP_KINETIS_FTM
FTM_DEVICE(3);
#endif /* DT_INST_3_NXP_KINETIS_FTM */
