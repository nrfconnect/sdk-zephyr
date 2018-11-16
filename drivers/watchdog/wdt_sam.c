/*
 * Copyright (C) 2017 Intel Deutschland GmbH
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @brief Watchdog (WDT) Driver for Atmel SAM MCUs
 *
 * Note:
 * - Once the watchdog disable bit is set, it cannot be cleared till next
 *   power reset, i.e, the watchdog cannot be started once stopped.
 * - Since the MCU boots with WDT enabled,the  CONFIG_WDT_SAM_DISABLE_AT_BOOT
 *   is set default at boot and watchdog module is disabled in the MCU for
 *   systems that don't need watchdog functionality.
 * - If the application needs to use the watchdog in the system, then
 *   CONFIG_WDT_SAM_DISABLE_AT_BOOT must be unset in the app's config file
 */

#include <watchdog.h>
#include <soc.h>

#define LOG_LEVEL CONFIG_WDT_LOG_LEVEL
#include <logging/log.h>
LOG_MODULE_REGISTER(wdt_sam);

#define SAM_PRESCALAR   128
#define WDT_MAX_VALUE   4095

/* Device constant configuration parameters */
struct wdt_sam_dev_cfg {
	Wdt *regs;
};

static struct device DEVICE_NAME_GET(wdt_sam);

struct wdt_sam_dev_data {
	wdt_callback_t cb;
	u32_t mode;
	bool timeout_valid;
	bool mode_set;
};

static struct wdt_sam_dev_data wdt_sam_data = { 0 };

#define DEV_CFG(dev) \
	((const struct wdt_sam_dev_cfg *const)(dev)->config->config_info)

static void wdt_sam_isr(struct device *dev)
{
	u32_t wdt_sr;
	Wdt *const wdt = DEV_CFG(dev)->regs;
	struct wdt_sam_dev_data *data = dev->driver_data;

	/* Clear status bit to acknowledge interrupt by dummy read. */
	wdt_sr = wdt->WDT_SR;

	data->cb(dev, 0);
}

/**
 * @brief Calculates the watchdog counter value (WDV)
 *        to be installed in the watchdog timer
 *
 * @param timeout Timeout value in milliseconds.
 * @param slow clock on board in Hz.
 */
int wdt_sam_convert_timeout(u32_t timeout, u32_t sclk)
{
	u32_t max, min;

	timeout = timeout * 1000;
	min =  (SAM_PRESCALAR * 1000000) / sclk;
	max = min * WDT_MAX_VALUE;
	if ((timeout < min) || (timeout > max)) {
		LOG_ERR("Invalid timeout value allowed range:"
			"%d ms to %d ms", min / 1000, max / 1000);
		return -EINVAL;
	}

	return WDT_MR_WDV(timeout / min);
}

static int wdt_sam_disable(struct device *dev)
{
	Wdt *const wdt = DEV_CFG(dev)->regs;
	struct wdt_sam_dev_data *data = dev->driver_data;

	/* since Watchdog mode register is 'write-once', we can't disable if
	 * someone has already set the mode register
	 */
	if (data->mode_set) {
		return -EPERM;
	}

	/* do we handle -EFAULT here */

	/* Watchdog Mode register is 'write-once' only register.
	 * Once disabled, it cannot be enabled until the device is reset
	 */
	wdt->WDT_MR |= WDT_MR_WDDIS;
	data->mode_set = true;

	return 0;
}

static int wdt_sam_setup(struct device *dev, u8_t options)
{

	Wdt *const wdt = DEV_CFG(dev)->regs;
	struct wdt_sam_dev_data *data = dev->driver_data;

	if (!data->timeout_valid) {
		LOG_ERR("No valid timeouts installed");
		return -EINVAL;
	}

	/* since Watchdog mode register is 'write-once', we can't set if
	 * someone has already set the mode register
	 */
	if (data->mode_set) {
		return -EPERM;
	}

	if ((options & WDT_OPT_PAUSE_IN_SLEEP) == WDT_OPT_PAUSE_IN_SLEEP) {
		data->mode |= WDT_MR_WDIDLEHLT;
	}

	if ((options & WDT_OPT_PAUSE_HALTED_BY_DBG) ==
	    WDT_OPT_PAUSE_HALTED_BY_DBG) {
		data->mode |= WDT_MR_WDDBGHLT;
	}

	wdt->WDT_MR = data->mode;
	data->mode_set = true;

	return 0;
}

static int wdt_sam_install_timeout(struct device *dev,
				   const struct wdt_timeout_cfg *cfg)
{
	u32_t wdt_mode = 0;
	int timeout_value;

	struct wdt_sam_dev_data *data = dev->driver_data;

	if (data->timeout_valid) {
		LOG_ERR("No more timeouts can be installed");
		return -ENOMEM;
	}

	if (cfg->window.min != 0) {
		return -EINVAL;
	}

	/*
	 * Convert time to cycles. SAM3X SoC doesn't supports window
	 * timeout config. So the api expects the timeout to be filled
	 * in the max field of the timeout config.
	 */
	timeout_value = wdt_sam_convert_timeout(cfg->window.max,
						(u32_t) CHIP_FREQ_XTAL_32K);

	if (timeout_value < 0) {
		return -EINVAL;
	}

	switch (cfg->flags) {
	case WDT_FLAG_RESET_SOC:
		/*A Watchdog fault (underflow or error) activates all resets */
		wdt_mode = WDT_MR_WDRSTEN;  /* WDT reset enable */
		break;

	case WDT_FLAG_RESET_NONE:
		/* A Watchdog fault (underflow or error) asserts interrupt. */
		if (cfg->callback) {
			wdt_mode = WDT_MR_WDFIEN;   /* WDT fault interrupt. */
			data->cb = cfg->callback;
		} else {
			LOG_ERR("Invalid(NULL) ISR callback passed\n");
			return -EINVAL;
		}
		break;

		/* Processor only reset mode not available in same70 series */
#ifdef WDT_MR_WDRPROC
	case WDT_FLAG_RESET_CPU_CORE:
		/*A Watchdog fault activates the processor reset*/
		LOG_DBG("Configuring reset CPU only mode\n");
		wdt_mode = WDT_MR_WDRSTEN   |   /* WDT reset enable */
			   WDT_MR_WDRPROC;      /* WDT reset processor only*/
		break;
#endif
	default:
		LOG_ERR("Unsupported watchdog config Flag\n");
		return -ENOTSUP;
	}

	data->mode = wdt_mode |
		     WDT_MR_WDV(timeout_value) |
		     WDT_MR_WDD(timeout_value);

	data->timeout_valid = true;

	return 0;
}

static int wdt_sam_feed(struct device *dev, int channel_id)
{
	/*
	 * On watchdog restart the Watchdog counter is immediately
	 * reloaded/feeded with the 12-bit watchdog counter
	 * value from WDT_MR and restarted
	 */
	Wdt *const wdt = DEV_CFG(dev)->regs;

	wdt->WDT_CR |= WDT_CR_KEY_PASSWD | WDT_CR_WDRSTT;

	return 0;
}

static const struct wdt_driver_api wdt_sam_api = {
	.setup = wdt_sam_setup,
	.disable = wdt_sam_disable,
	.install_timeout = wdt_sam_install_timeout,
	.feed = wdt_sam_feed,
};

static const struct wdt_sam_dev_cfg wdt_sam_cfg = {
	.regs = (Wdt *)DT_WDT_SAM_BASE_ADDRESS,
};

static void wdt_sam_irq_config(void)
{
	IRQ_CONNECT(DT_WDT_SAM_IRQ,
		    DT_WDT_SAM_IRQ_PRIORITY, wdt_sam_isr,
		    DEVICE_GET(wdt_sam), 0);
	irq_enable(DT_WDT_SAM_IRQ);
}

static int wdt_sam_init(struct device *dev)
{
#ifdef CONFIG_WDT_SAM_DISABLE_AT_BOOT
	wdt_sam_disable(dev);
#endif

	wdt_sam_irq_config();
	return 0;
}

DEVICE_AND_API_INIT(wdt_sam, CONFIG_WDT_0_NAME, wdt_sam_init,
		    &wdt_sam_data, &wdt_sam_cfg, PRE_KERNEL_1,
		    CONFIG_KERNEL_INIT_PRIORITY_DEVICE, &wdt_sam_api);
