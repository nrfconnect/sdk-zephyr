/*
 *
 * Copyright (c) 2018, NXP

 * SPDX-License-Identifier: Apache-2.0
 */

#include <watchdog.h>
#include <clock_control.h>
#include <fsl_wdog.h>

#define LOG_LEVEL CONFIG_WDT_LOG_LEVEL
#include <logging/log.h>
LOG_MODULE_REGISTER(wdt_mcux_wdog);

#define MIN_TIMEOUT 4

struct mcux_wdog_config {
	WDOG_Type *base;
	char *clock_name;
	clock_control_subsys_t clock_subsys;
	void (*irq_config_func)(struct device *dev);
};

struct mcux_wdog_data {
	wdt_callback_t callback;
	wdog_config_t wdog_config;
	bool timeout_valid;
};

static int mcux_wdog_setup(struct device *dev, u8_t options)
{
	const struct mcux_wdog_config *config = dev->config->config_info;
	struct mcux_wdog_data *data = dev->driver_data;
	WDOG_Type *base = config->base;

	if (!data->timeout_valid) {
		LOG_ERR("No valid timeouts installed");
		return -EINVAL;
	}

	data->wdog_config.workMode.enableStop =
		(options & WDT_OPT_PAUSE_IN_SLEEP) == 0;

	data->wdog_config.workMode.enableDebug =
		(options & WDT_OPT_PAUSE_HALTED_BY_DBG) == 0;

	WDOG_Init(base, &data->wdog_config);
	LOG_DBG("Setup the watchdog");

	return 0;
}

static int mcux_wdog_disable(struct device *dev)
{
	const struct mcux_wdog_config *config = dev->config->config_info;
	struct mcux_wdog_data *data = dev->driver_data;
	WDOG_Type *base = config->base;

	WDOG_Deinit(base);
	data->timeout_valid = false;
	LOG_DBG("Disabled the watchdog");

	return 0;
}

static int mcux_wdog_install_timeout(struct device *dev,
				     const struct wdt_timeout_cfg *cfg)
{
	const struct mcux_wdog_config *config = dev->config->config_info;
	struct mcux_wdog_data *data = dev->driver_data;
	struct device *clock_dev;
	u32_t clock_freq;

	if (data->timeout_valid) {
		LOG_ERR("No more timeouts can be installed");
		return -ENOMEM;
	}

	clock_dev = device_get_binding(config->clock_name);
	if (clock_dev == NULL) {
		return -EINVAL;
	}

	if (clock_control_get_rate(clock_dev, config->clock_subsys,
				   &clock_freq)) {
		return -EINVAL;
	}

	WDOG_GetDefaultConfig(&data->wdog_config);

	data->wdog_config.timeoutValue = clock_freq * cfg->window.max / 1000;

	if (cfg->window.min) {
		data->wdog_config.enableWindowMode = true;
		data->wdog_config.windowValue =
			clock_freq * cfg->window.min / 1000;
	} else {
		data->wdog_config.enableWindowMode = false;
		data->wdog_config.windowValue = 0;
	}

	if ((data->wdog_config.timeoutValue < MIN_TIMEOUT) ||
	    (data->wdog_config.timeoutValue < data->wdog_config.windowValue)) {
		LOG_ERR("Invalid timeout");
		return -EINVAL;
	}

	data->wdog_config.clockSource = kWDOG_LpoClockSource;
	data->wdog_config.enableInterrupt = cfg->callback != NULL;
	data->callback = cfg->callback;
	data->timeout_valid = true;

	return 0;
}

static int mcux_wdog_feed(struct device *dev, int channel_id)
{
	const struct mcux_wdog_config *config = dev->config->config_info;
	WDOG_Type *base = config->base;

	if (channel_id != 0) {
		LOG_ERR("Invalid channel id");
		return -EINVAL;
	}

	WDOG_Refresh(base);
	LOG_DBG("Fed the watchdog");

	return 0;
}

static void mcux_wdog_isr(void *arg)
{
	struct device *dev = (struct device *)arg;
	const struct mcux_wdog_config *config = dev->config->config_info;
	struct mcux_wdog_data *data = dev->driver_data;
	WDOG_Type *base = config->base;
	u32_t flags;

	flags = WDOG_GetStatusFlags(base);
	WDOG_ClearStatusFlags(base, flags);

	if (data->callback) {
		data->callback(dev, 0);
	}
}

static int mcux_wdog_init(struct device *dev)
{
	const struct mcux_wdog_config *config = dev->config->config_info;

	config->irq_config_func(dev);

	return 0;
}

static const struct wdt_driver_api mcux_wdog_api = {
	.setup = mcux_wdog_setup,
	.disable = mcux_wdog_disable,
	.install_timeout = mcux_wdog_install_timeout,
	.feed = mcux_wdog_feed,
};

static void mcux_wdog_config_func_0(struct device *dev);

static const struct mcux_wdog_config mcux_wdog_config_0 = {
	.base = (WDOG_Type *) DT_WDT_0_BASE_ADDRESS,
	.clock_name = DT_WDT_0_CLOCK_NAME,
	.clock_subsys = (clock_control_subsys_t) DT_WDT_0_CLOCK_SUBSYS,
	.irq_config_func = mcux_wdog_config_func_0,
};

static struct mcux_wdog_data mcux_wdog_data_0;

DEVICE_AND_API_INIT(mcux_wdog_0, CONFIG_WDT_0_NAME, &mcux_wdog_init,
		    &mcux_wdog_data_0, &mcux_wdog_config_0,
		    POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEVICE,
		    &mcux_wdog_api);

static void mcux_wdog_config_func_0(struct device *dev)
{
	IRQ_CONNECT(DT_WDT_0_IRQ, DT_WDT_0_IRQ_PRI,
		    mcux_wdog_isr, DEVICE_GET(mcux_wdog_0), 0);

	irq_enable(DT_WDT_0_IRQ);
}
