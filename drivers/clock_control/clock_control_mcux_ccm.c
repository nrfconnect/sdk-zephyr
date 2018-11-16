/*
 * Copyright (c) 2017, NXP
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <errno.h>
#include <soc.h>
#include <clock_control.h>
#include <dt-bindings/clock/imx_ccm.h>
#include <fsl_clock.h>

#define LOG_LEVEL CONFIG_CLOCK_CONTROL_LOG_LEVEL
#include <logging/log.h>
LOG_MODULE_REGISTER(clock_control);

static const clock_name_t lpspi_clocks[] = {
	kCLOCK_Usb1PllPfd1Clk,
	kCLOCK_Usb1PllPfd0Clk,
	kCLOCK_SysPllClk,
	kCLOCK_SysPllPfd2Clk,
};

static int mcux_ccm_on(struct device *dev,
			      clock_control_subsys_t sub_system)
{
	return 0;
}

static int mcux_ccm_off(struct device *dev,
			       clock_control_subsys_t sub_system)
{
	return 0;
}

static int mcux_ccm_get_subsys_rate(struct device *dev,
				    clock_control_subsys_t sub_system,
				    u32_t *rate)
{
	u32_t clock_name = (u32_t) sub_system;

	switch (clock_name) {
	case IMX_CCM_LPSPI_CLK:
	{
		u32_t lpspi_mux = CLOCK_GetMux(kCLOCK_LpspiMux);
		clock_name_t lpspi_clock = lpspi_clocks[lpspi_mux];

		*rate = CLOCK_GetFreq(lpspi_clock)
			/ (CLOCK_GetDiv(kCLOCK_LpspiDiv) + 1);
		break;
	}

	case IMX_CCM_LPUART_CLK:
		if (CLOCK_GetMux(kCLOCK_UartMux) == 0) {
			*rate = CLOCK_GetPllFreq(kCLOCK_PllUsb1) / 6
				/ (CLOCK_GetDiv(kCLOCK_UartDiv) + 1);
		} else {
			*rate = CLOCK_GetOscFreq()
				/ (CLOCK_GetDiv(kCLOCK_UartDiv) + 1);
		}

		break;
	}

	return 0;
}

static int mcux_ccm_init(struct device *dev)
{
	return 0;
}

static const struct clock_control_driver_api mcux_ccm_driver_api = {
	.on = mcux_ccm_on,
	.off = mcux_ccm_off,
	.get_rate = mcux_ccm_get_subsys_rate,
};

DEVICE_AND_API_INIT(mcux_ccm, DT_MCUX_CCM_NAME,
		    &mcux_ccm_init,
		    NULL, NULL,
		    PRE_KERNEL_1, CONFIG_KERNEL_INIT_PRIORITY_DEVICE,
		    &mcux_ccm_driver_api);
