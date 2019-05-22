/*
 * Copyright (c) 2018 blik GmbH
 * Copyright (c) 2018, NXP
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <counter.h>
#include <fsl_rtc.h>
#include <logging/log.h>

LOG_MODULE_REGISTER(mcux_rtc, CONFIG_COUNTER_LOG_LEVEL);

struct mcux_rtc_data {
	counter_alarm_callback_t alarm_callback;
	counter_top_callback_t top_callback;
	void *alarm_user_data;
	void *top_user_data;
};

struct mcux_rtc_config {
	struct counter_config_info info;
	RTC_Type *base;
	void (*irq_config_func)(struct device *dev);
};

static int mcux_rtc_start(struct device *dev)
{
	const struct counter_config_info *info = dev->config->config_info;
	const struct mcux_rtc_config *config =
		CONTAINER_OF(info, struct mcux_rtc_config, info);

	RTC_StartTimer(config->base);
	RTC_EnableInterrupts(config->base,
			     kRTC_AlarmInterruptEnable |
			     kRTC_TimeOverflowInterruptEnable |
			     kRTC_TimeInvalidInterruptEnable);

	return 0;
}

static int mcux_rtc_stop(struct device *dev)
{
	const struct counter_config_info *info = dev->config->config_info;
	const struct mcux_rtc_config *config =
		CONTAINER_OF(info, struct mcux_rtc_config, info);

	RTC_DisableInterrupts(config->base,
			      kRTC_AlarmInterruptEnable |
			      kRTC_TimeOverflowInterruptEnable |
			      kRTC_TimeInvalidInterruptEnable);
	RTC_StopTimer(config->base);

	/* clear out any set alarms */
	config->base->TAR = 0;

	return 0;
}

static u32_t mcux_rtc_read(struct device *dev)
{
	const struct counter_config_info *info = dev->config->config_info;
	const struct mcux_rtc_config *config =
		CONTAINER_OF(info, struct mcux_rtc_config, info);

	u32_t ticks = config->base->TSR;

	/*
	 * Read TSR seconds twice in case it glitches during an update.
	 * This can happen when a read occurs at the time the register is
	 * incrementing.
	 */
	if (config->base->TSR == ticks) {
		return ticks;
	}

	ticks = config->base->TSR;

	return ticks;
}

static int mcux_rtc_set_alarm(struct device *dev, u8_t chan_id,
			      const struct counter_alarm_cfg *alarm_cfg)
{
	const struct counter_config_info *info = dev->config->config_info;
	const struct mcux_rtc_config *config =
		CONTAINER_OF(info, struct mcux_rtc_config, info);
	struct mcux_rtc_data *data = dev->driver_data;

	u32_t ticks = alarm_cfg->ticks;
	u32_t current = mcux_rtc_read(dev);

	LOG_DBG("Current time is %d ticks", current);

	if (chan_id != 0U) {
		LOG_ERR("Invalid channel id");
		return -EINVAL;
	}

	if (data->alarm_callback != NULL) {
		return -EBUSY;
	}

	if (!alarm_cfg->absolute) {
		ticks += current;
	}

	if (ticks < current) {
		LOG_ERR("Alarm cannot be earlier than current time");
		return -EINVAL;
	}

	data->alarm_callback = alarm_cfg->callback;
	data->alarm_user_data = alarm_cfg->user_data;

	config->base->TAR = ticks;
	LOG_DBG("Alarm set to %d ticks", ticks);

	return 0;
}

static int mcux_rtc_cancel_alarm(struct device *dev, u8_t chan_id)
{
	struct mcux_rtc_data *data = dev->driver_data;

	if (chan_id != 0U) {
		LOG_ERR("Invalid channel id");
		return -EINVAL;
	}

	data->alarm_callback = NULL;

	return 0;
}

static int mcux_rtc_set_top_value(struct device *dev, u32_t ticks,
				  counter_top_callback_t callback,
				  void *user_data)
{
	const struct counter_config_info *info = dev->config->config_info;
	struct mcux_rtc_data *data = dev->driver_data;

	if (ticks != info->max_top_value) {
		LOG_ERR("Wrap can only be set to 0x%x", info->max_top_value);
		return -ENOTSUP;
	}

	data->top_callback = callback;
	data->top_user_data = user_data;

	return 0;
}

static u32_t mcux_rtc_get_pending_int(struct device *dev)
{
	const struct counter_config_info *info = dev->config->config_info;
	const struct mcux_rtc_config *config =
		CONTAINER_OF(info, struct mcux_rtc_config, info);

	return RTC_GetStatusFlags(config->base) & RTC_SR_TAF_MASK;
}

static u32_t mcux_rtc_get_top_value(struct device *dev)
{
	const struct counter_config_info *info = dev->config->config_info;

	return info->max_top_value;
}

static u32_t mcux_rtc_get_max_relative_alarm(struct device *dev)
{
	const struct counter_config_info *info = dev->config->config_info;

	return info->max_top_value;
}

static void mcux_rtc_isr(void *arg)
{
	struct device *dev = arg;
	const struct counter_config_info *info = dev->config->config_info;
	const struct mcux_rtc_config *config =
		CONTAINER_OF(info, struct mcux_rtc_config, info);
	struct mcux_rtc_data *data = dev->driver_data;
	u32_t current = mcux_rtc_read(dev);

	LOG_DBG("Current time is %d ticks", current);

	if ((RTC_GetStatusFlags(config->base) & RTC_SR_TAF_MASK) &&
	    (data->alarm_callback)) {
		data->alarm_callback(dev, 0, current, data->alarm_user_data);
	}

	if ((RTC_GetStatusFlags(config->base) & RTC_SR_TOF_MASK) &&
	    (data->top_callback)) {
		data->top_callback(dev, data->top_user_data);
	}

	/*
	 * Clear any conditions to ack the IRQ
	 *
	 * callback may have already reset the alarm flag if a new
	 * alarm value was programmed to the TAR
	 */
	RTC_StopTimer(config->base);
	if (RTC_GetStatusFlags(config->base) & RTC_SR_TAF_MASK) {
		RTC_ClearStatusFlags(config->base, kRTC_AlarmFlag);
	} else if (RTC_GetStatusFlags(config->base) & RTC_SR_TIF_MASK) {
		RTC_ClearStatusFlags(config->base, kRTC_TimeInvalidFlag);
	} else if (RTC_GetStatusFlags(config->base) & RTC_SR_TOF_MASK) {
		RTC_ClearStatusFlags(config->base, kRTC_TimeOverflowFlag);
	}
	RTC_StartTimer(config->base);
}

static int mcux_rtc_init(struct device *dev)
{
	const struct counter_config_info *info = dev->config->config_info;
	const struct mcux_rtc_config *config =
		CONTAINER_OF(info, struct mcux_rtc_config, info);
	rtc_config_t rtc_config;

	RTC_GetDefaultConfig(&rtc_config);
	RTC_Init(config->base, &rtc_config);

	/* Enable 32kHz oscillator and wait for 1ms to settle */
	config->base->CR |= 0x100;
	k_busy_wait(USEC_PER_MSEC);

	config->irq_config_func(dev);

	return 0;
}

static const struct counter_driver_api mcux_rtc_driver_api = {
	.start = mcux_rtc_start,
	.stop = mcux_rtc_stop,
	.read = mcux_rtc_read,
	.set_alarm = mcux_rtc_set_alarm,
	.cancel_alarm = mcux_rtc_cancel_alarm,
	.set_top_value = mcux_rtc_set_top_value,
	.get_pending_int = mcux_rtc_get_pending_int,
	.get_top_value = mcux_rtc_get_top_value,
	.get_max_relative_alarm = mcux_rtc_get_max_relative_alarm,
};

static struct mcux_rtc_data mcux_rtc_data_0;

static void mcux_rtc_irq_config_0(struct device *dev);

static struct mcux_rtc_config mcux_rtc_config_0 = {
	.base = (RTC_Type *)DT_RTC_MCUX_0_BASE_ADDRESS,
	.irq_config_func = mcux_rtc_irq_config_0,
	.info = {
		.max_top_value = UINT32_MAX,
		.freq = DT_NXP_KINETIS_RTC_0_CLOCK_FREQUENCY /
				DT_NXP_KINETIS_RTC_0_PRESCALER,
		.count_up = true,
		.channels = 1,
	},
};

DEVICE_AND_API_INIT(rtc, DT_RTC_MCUX_0_NAME, &mcux_rtc_init,
		    &mcux_rtc_data_0, &mcux_rtc_config_0.info,
		    POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEVICE,
		    &mcux_rtc_driver_api);

static void mcux_rtc_irq_config_0(struct device *dev)
{
	IRQ_CONNECT(DT_RTC_MCUX_0_IRQ, DT_RTC_MCUX_0_IRQ_PRI,
		    mcux_rtc_isr, DEVICE_GET(rtc), 0);
	irq_enable(DT_RTC_MCUX_0_IRQ);
}
