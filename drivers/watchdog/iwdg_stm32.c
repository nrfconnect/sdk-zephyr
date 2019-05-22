/*
 * Copyright (c) 2016 Open-RnD Sp. z o.o.
 * Copyright (c) 2017 RnDity Sp. z o.o.
 * Copyright (c) 2018 qianfan Zhao
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <watchdog.h>
#include <soc.h>
#include <errno.h>
#include <assert.h>

#include "iwdg_stm32.h"

/* Minimal timeout in microseconds. */
#define IWDG_TIMEOUT_MIN	100
/* Maximal timeout in microseconds. */
#define IWDG_TIMEOUT_MAX	26214400

#define IS_IWDG_TIMEOUT(__TIMEOUT__)		\
	(((__TIMEOUT__) >= IWDG_TIMEOUT_MIN) &&	\
	 ((__TIMEOUT__) <= IWDG_TIMEOUT_MAX))

/*
 * Status register need 5 RC LSI divided by prescaler clock to be updated.
 * With higher prescaler (256U), and according to HSI variation,
 * we need to wait at least 6 cycles so 48 ms.
 */

#define IWDG_DEFAULT_TIMEOUT	48u

/**
 * @brief Calculates prescaler & reload values.
 *
 * @param timeout Timeout value in microseconds.
 * @param prescaler Pointer to prescaler value.
 * @param reload Pointer to reload value.
 */
static void iwdg_stm32_convert_timeout(u32_t timeout,
				       u32_t *prescaler,
				       u32_t *reload)
{

	u16_t divider = 0U;
	u8_t shift = 0U;

	/* Convert timeout to seconds. */
	float m_timeout = (float)timeout / 1000000 * LSI_VALUE;

	do {
		divider = 4 << shift;
		shift++;
	} while ((m_timeout / divider) > 0xFFF);

	/*
	 * Value of the 'shift' variable corresponds to the
	 * defines of LL_IWDG_PRESCALER_XX type.
	 */
	*prescaler = --shift;
	*reload = (u32_t)(m_timeout / divider) - 1;
}

static int iwdg_stm32_setup(struct device *dev, u8_t options)
{
	IWDG_TypeDef *iwdg = IWDG_STM32_STRUCT(dev);

	/* Deactivate running when debugger is attached. */
	if (options & WDT_OPT_PAUSE_HALTED_BY_DBG) {
#if defined(CONFIG_SOC_SERIES_STM32F0X)
		LL_APB1_GRP2_EnableClock(LL_APB1_GRP2_PERIPH_DBGMCU);
#elif defined(CONFIG_SOC_SERIES_STM32L0X)
		LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_DBGMCU);
#endif
		LL_DBGMCU_APB1_GRP1_FreezePeriph(LL_DBGMCU_APB1_GRP1_IWDG_STOP);
	}

	if (options & WDT_OPT_PAUSE_IN_SLEEP) {
		return -ENOTSUP;
	}

	LL_IWDG_Enable(iwdg);
	return 0;
}

static int iwdg_stm32_disable(struct device *dev)
{
	/* watchdog cannot be stopped once started */
	ARG_UNUSED(dev);

	return -EPERM;
}

static int iwdg_stm32_install_timeout(struct device *dev,
				      const struct wdt_timeout_cfg *config)
{
	IWDG_TypeDef *iwdg = IWDG_STM32_STRUCT(dev);
	u32_t timeout = config->window.max * USEC_PER_MSEC;
	u32_t prescaler = 0U;
	u32_t reload = 0U;
	u32_t tickstart;

	if (config->callback != NULL) {
		return -ENOTSUP;
	}

	iwdg_stm32_convert_timeout(timeout, &prescaler, &reload);

	if (!(IS_IWDG_TIMEOUT(timeout) && IS_IWDG_PRESCALER(prescaler) &&
	    IS_IWDG_RELOAD(reload))) {
		/* One of the parameters provided is invalid */
		return -EINVAL;
	}

	tickstart = k_uptime_get_32();

	while (LL_IWDG_IsReady(iwdg) == 0) {
		/* Wait untill WVU, RVU, PVU are reset before updating  */
		if ((k_uptime_get_32() - tickstart) > IWDG_DEFAULT_TIMEOUT) {
			return -ENODEV;
		}
	}

	LL_IWDG_EnableWriteAccess(iwdg);

	LL_IWDG_SetPrescaler(iwdg, prescaler);
	LL_IWDG_SetReloadCounter(iwdg, reload);

	return 0;
}

static int iwdg_stm32_feed(struct device *dev, int channel_id)
{
	IWDG_TypeDef *iwdg = IWDG_STM32_STRUCT(dev);

	ARG_UNUSED(channel_id);
	LL_IWDG_ReloadCounter(iwdg);

	return 0;
}

static const struct wdt_driver_api iwdg_stm32_api = {
	.setup = iwdg_stm32_setup,
	.disable = iwdg_stm32_disable,
	.install_timeout = iwdg_stm32_install_timeout,
	.feed = iwdg_stm32_feed,
};

static int iwdg_stm32_init(struct device *dev)
{
#ifdef CONFIG_IWDG_STM32_START_AT_BOOT
	IWDG_TypeDef *iwdg = IWDG_STM32_STRUCT(dev);
	struct wdt_timeout_cfg config = {
		.window.max = CONFIG_IWDG_STM32_TIMEOUT * USEC_PER_MSEC,
	};

	LL_IWDG_Enable(iwdg);
	iwdg_stm32_install_timeout(dev, &config);
#endif

	/*
	 * The ST production value for the option bytes where WDG_SW bit is
	 * present is 0x00FF55AA, namely the Software watchdog mode is
	 * enabled by default.
	 * If the IWDG is started by either hardware option or software access,
	 * the LSI oscillator is forced ON and cannot be disabled.
	 *
	 * t_IWDG(ms) = t_LSI(ms) x 4 x 2^(IWDG_PR[2:0]) x (IWDG_RLR[11:0] + 1)
	 */

	return 0;
}

static struct iwdg_stm32_data iwdg_stm32_dev_data = {
	.Instance = (IWDG_TypeDef *)DT_ST_STM32_WATCHDOG_0_BASE_ADDRESS
};

DEVICE_AND_API_INIT(iwdg_stm32, DT_ST_STM32_WATCHDOG_0_LABEL,
		    iwdg_stm32_init, &iwdg_stm32_dev_data, NULL,
		    POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEVICE,
		    &iwdg_stm32_api);
