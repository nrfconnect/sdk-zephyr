/*
 * Copyright (c) 2018 Workaround GmbH
 * Copyright (c) 2018 Allterco Robotics
 * Copyright (c) 2018 Linaro Limited
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Source file for the STM32 RTC driver
 *
 */

#include <time.h>

#include <clock_control/stm32_clock_control.h>
#include <clock_control.h>
#include <misc/util.h>
#include <kernel.h>
#include <soc.h>
#include <counter.h>

#include <logging/log.h>

LOG_MODULE_REGISTER(counter_rtc_stm32, CONFIG_COUNTER_LOG_LEVEL);

#define T_TIME_OFFSET 946684800

#if defined(CONFIG_SOC_SERIES_STM32L4X)
#define RTC_EXTI_LINE	LL_EXTI_LINE_18
#elif defined(CONFIG_SOC_SERIES_STM32F4X) \
	|| defined(CONFIG_SOC_SERIES_STM32F3X)	\
	|| defined(CONFIG_SOC_SERIES_STM32F7X)
#define RTC_EXTI_LINE	LL_EXTI_LINE_17
#endif

struct rtc_stm32_config {
	struct counter_config_info counter_info;
	struct stm32_pclken pclken;
	LL_RTC_InitTypeDef ll_rtc_config;
};

struct rtc_stm32_data {
	counter_alarm_callback_t callback;
	u32_t ticks;
	void *user_data;
	bool absolute;
};


#define DEV_DATA(dev) ((struct rtc_stm32_data *)(dev)->driver_data)
#define DEV_CFG(dev)	\
((const struct rtc_stm32_config * const)(dev)->config->config_info)


static void rtc_stm32_irq_config(struct device *dev);


static int rtc_stm32_start(struct device *dev)
{
	ARG_UNUSED(dev);

	LL_RCC_EnableRTC();

	return 0;
}


static int rtc_stm32_stop(struct device *dev)
{
	ARG_UNUSED(dev);

	LL_RCC_DisableRTC();

	return 0;
}


static u32_t rtc_stm32_read(struct device *dev)
{
	struct tm now = { 0 };
	time_t ts;
	u32_t rtc_date, rtc_time, ticks;

	ARG_UNUSED(dev);

	/* Read time and date registers */
	rtc_time = LL_RTC_TIME_Get(RTC);
	rtc_date = LL_RTC_DATE_Get(RTC);

	/* Convert calendar datetime to UNIX timestamp */
	/* RTC start time: 1st, Jan, 2000 */
	/* time_t start:   1st, Jan, 1900 */
	now.tm_year = 100 +
			__LL_RTC_CONVERT_BCD2BIN(__LL_RTC_GET_YEAR(rtc_date));
	/* tm_mon allowed values are 0-11 */
	now.tm_mon = __LL_RTC_CONVERT_BCD2BIN(__LL_RTC_GET_MONTH(rtc_date)) - 1;
	now.tm_mday = __LL_RTC_CONVERT_BCD2BIN(__LL_RTC_GET_DAY(rtc_date));

	now.tm_hour = __LL_RTC_CONVERT_BCD2BIN(__LL_RTC_GET_HOUR(rtc_time));
	now.tm_min = __LL_RTC_CONVERT_BCD2BIN(__LL_RTC_GET_MINUTE(rtc_time));
	now.tm_sec = __LL_RTC_CONVERT_BCD2BIN(__LL_RTC_GET_SECOND(rtc_time));

	ts = mktime(&now);

	/* Return number of seconds since RTC init */
	ts -= T_TIME_OFFSET;

	__ASSERT(sizeof(time_t) == 8, "unexpected time_t definition");
	ticks = counter_us_to_ticks(dev, ts * USEC_PER_SEC);

	return ticks;
}

static int rtc_stm32_set_alarm(struct device *dev, u8_t chan_id,
				const struct counter_alarm_cfg *alarm_cfg)
{
	struct tm alarm_tm;
	time_t alarm_val;
	LL_RTC_AlarmTypeDef rtc_alarm;
	struct rtc_stm32_data *data = DEV_DATA(dev);

	u32_t now = rtc_stm32_read(dev);
	u32_t ticks = alarm_cfg->ticks;

	if (data->callback != NULL) {
		LOG_DBG("Alarm busy\n");
		return -EBUSY;
	}


	data->callback = alarm_cfg->callback;
	data->user_data = alarm_cfg->user_data;
	data->absolute = alarm_cfg->absolute;

	if (!alarm_cfg->absolute) {
		ticks += now;
	}

	LOG_DBG("Set Alarm: %d\n", ticks);

	alarm_val = (time_t)(counter_ticks_to_us(dev, ticks) / USEC_PER_SEC);

	gmtime_r(&alarm_val, &alarm_tm);

	/* Apply ALARM_A */
	rtc_alarm.AlarmTime.TimeFormat = LL_RTC_TIME_FORMAT_AM_OR_24;
	rtc_alarm.AlarmTime.Hours = alarm_tm.tm_hour;
	rtc_alarm.AlarmTime.Minutes = alarm_tm.tm_min;
	rtc_alarm.AlarmTime.Seconds = alarm_tm.tm_sec;

	rtc_alarm.AlarmMask = LL_RTC_ALMA_MASK_NONE;
	rtc_alarm.AlarmDateWeekDaySel = LL_RTC_ALMA_DATEWEEKDAYSEL_DATE;
	rtc_alarm.AlarmDateWeekDay = alarm_tm.tm_mday;

	LL_RTC_DisableWriteProtection(RTC);
	LL_RTC_ALMA_Disable(RTC);
	LL_RTC_EnableWriteProtection(RTC);

	if (LL_RTC_ALMA_Init(RTC, LL_RTC_FORMAT_BIN, &rtc_alarm) != SUCCESS) {
		return -EIO;
	}

	LL_RTC_DisableWriteProtection(RTC);
	LL_RTC_ALMA_Enable(RTC);
	LL_RTC_ClearFlag_ALRA(RTC);
	LL_RTC_EnableIT_ALRA(RTC);
	LL_RTC_EnableWriteProtection(RTC);

	return 0;
}


static int rtc_stm32_cancel_alarm(struct device *dev, u8_t chan_id)
{
	LL_RTC_DisableWriteProtection(RTC);
	LL_RTC_ClearFlag_ALRA(RTC);
	LL_RTC_DisableIT_ALRA(RTC);
	LL_RTC_ALMA_Disable(RTC);
	LL_RTC_EnableWriteProtection(RTC);

	DEV_DATA(dev)->callback = NULL;

	return 0;
}


static u32_t rtc_stm32_get_pending_int(struct device *dev)
{
	return LL_RTC_IsActiveFlag_ALRA(RTC) != 0;
}


static u32_t rtc_stm32_get_top_value(struct device *dev)
{
	const struct counter_config_info *info = dev->config->config_info;

	return info->max_top_value;
}


static int rtc_stm32_set_top_value(struct device *dev, u32_t ticks,
				counter_top_callback_t callback,
				void *user_data)
{
	const struct counter_config_info *info = dev->config->config_info;

	ARG_UNUSED(dev);
	ARG_UNUSED(callback);
	ARG_UNUSED(user_data);

	if (ticks != info->max_top_value) {
		return -ENOTSUP;
	} else {
		return 0;
	}


}


static u32_t rtc_stm32_get_max_relative_alarm(struct device *dev)
{
	const struct counter_config_info *info = dev->config->config_info;

	return info->max_top_value;
}


void rtc_stm32_isr(void *arg)
{
	struct device *const dev = (struct device *)arg;
	struct rtc_stm32_data *data = DEV_DATA(dev);
	counter_alarm_callback_t alarm_callback = data->callback;

	u32_t now = rtc_stm32_read(dev);

	if (LL_RTC_IsActiveFlag_ALRA(RTC) != 0) {

		LL_RTC_DisableWriteProtection(RTC);
		LL_RTC_ClearFlag_ALRA(RTC);
		LL_RTC_DisableIT_ALRA(RTC);
		LL_RTC_ALMA_Disable(RTC);
		LL_RTC_EnableWriteProtection(RTC);

		if (alarm_callback != NULL) {
			data->callback = NULL;
			alarm_callback(dev, 0, now, data->user_data);
		}
	}

	LL_EXTI_ClearFlag_0_31(RTC_EXTI_LINE);
}


static int rtc_stm32_init(struct device *dev)
{
	struct device *clk = device_get_binding(STM32_CLOCK_CONTROL_NAME);
	const struct rtc_stm32_config *cfg = DEV_CFG(dev);

	__ASSERT_NO_MSG(clk);

	DEV_DATA(dev)->callback = NULL;

	clock_control_on(clk, (clock_control_subsys_t *) &cfg->pclken);

	LL_PWR_EnableBkUpAccess();
	LL_RCC_ForceBackupDomainReset();
	LL_RCC_ReleaseBackupDomainReset();

#if defined(CONFIG_COUNTER_RTC_STM32_CLOCK_LSI)

	LL_RCC_LSI_Enable();
	while (LL_RCC_LSI_IsReady() != 1)
		;
	LL_RCC_SetRTCClockSource(LL_RCC_RTC_CLKSOURCE_LSI);

#else /* CONFIG_COUNTER_RTC_STM32_CLOCK_LSE */

#if !defined(CONFIG_SOC_SERIES_STM32F4X) &&	\
	!defined(CONFIG_SOC_SERIES_STM32F2X)

	LL_RCC_LSE_SetDriveCapability(
		CONFIG_COUNTER_RTC_STM32_LSE_DRIVE_STRENGTH);

#endif /* !CONFIG_SOC_SERIES_STM32F4X && !CONFIG_SOC_SERIES_STM32F2X */

	LL_RCC_LSE_Enable();

	/* Wait until LSE is ready */
	while (LL_RCC_LSE_IsReady() != 1)
		;

	LL_RCC_SetRTCClockSource(LL_RCC_RTC_CLKSOURCE_LSE);

#endif /* CONFIG_COUNTER_RTC_STM32_CLOCK_SRC */

	LL_RCC_EnableRTC();

	if (LL_RTC_DeInit(RTC) != SUCCESS) {
		return -EIO;
	}

	if (LL_RTC_Init(RTC, ((LL_RTC_InitTypeDef *)
			      &cfg->ll_rtc_config)) != SUCCESS) {
		return -EIO;
	}

#ifdef RTC_CR_BYPSHAD
	LL_RTC_DisableWriteProtection(RTC);
	LL_RTC_EnableShadowRegBypass(RTC);
	LL_RTC_EnableWriteProtection(RTC);
#endif /* RTC_CR_BYPSHAD */

	LL_EXTI_EnableIT_0_31(RTC_EXTI_LINE);
	LL_EXTI_EnableRisingTrig_0_31(RTC_EXTI_LINE);

	rtc_stm32_irq_config(dev);

	return 0;
}

static struct rtc_stm32_data rtc_data;

static const struct rtc_stm32_config rtc_config = {
	.counter_info = {
		.max_top_value = UINT32_MAX,
		.freq = 1,
		.count_up = true,
		.channels = 1,
	},
	.pclken = {
		.enr = DT_RTC_0_CLOCK_BITS,
		.bus = DT_RTC_0_CLOCK_BUS,
	},
	.ll_rtc_config = {
		.HourFormat = LL_RTC_HOURFORMAT_24HOUR,
#if defined(CONFIG_COUNTER_RTC_STM32_CLOCK_LSI)
		/* prescaler values for LSI @ 32 KHz */
		.AsynchPrescaler = 0x7F,
		.SynchPrescaler = 0x00F9,
#else /* CONFIG_COUNTER_RTC_STM32_CLOCK_LSE */
		/* prescaler values for LSE @ 32768 Hz */
		.AsynchPrescaler = 0x7F,
		.SynchPrescaler = 0x00FF,
#endif
	},
};


static const struct counter_driver_api rtc_stm32_driver_api = {
		.start = rtc_stm32_start,
		.stop = rtc_stm32_stop,
		.read = rtc_stm32_read,
		.set_alarm = rtc_stm32_set_alarm,
		.cancel_alarm = rtc_stm32_cancel_alarm,
		.set_top_value = rtc_stm32_set_top_value,
		.get_pending_int = rtc_stm32_get_pending_int,
		.get_top_value = rtc_stm32_get_top_value,
		.get_max_relative_alarm = rtc_stm32_get_max_relative_alarm,
};

DEVICE_AND_API_INIT(rtc_stm32, DT_RTC_0_NAME, &rtc_stm32_init,
		    &rtc_data, &rtc_config, PRE_KERNEL_1,
		    CONFIG_KERNEL_INIT_PRIORITY_DEVICE, &rtc_stm32_driver_api);

static void rtc_stm32_irq_config(struct device *dev)
{
	IRQ_CONNECT(DT_RTC_0_IRQ, DT_RTC_0_IRQ_PRI,
		    rtc_stm32_isr, DEVICE_GET(rtc_stm32), 0);
	irq_enable(DT_RTC_0_IRQ);
}
