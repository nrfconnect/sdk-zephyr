/*
 * Copyright (c) 2018 Google LLC.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <init.h>
#include <pinmux.h>

static int board_pinmux_init(struct device *dev)
{
	struct device *muxa = device_get_binding(DT_PINMUX_SAM0_A_LABEL);
	struct device *muxb = device_get_binding(DT_PINMUX_SAM0_B_LABEL);

	ARG_UNUSED(dev);

#if DT_UART_SAM0_SERCOM0_BASE_ADDRESS
	/* SERCOM0 on RX=PA11, TX=PA10 */
	pinmux_pin_set(muxa, 11, PINMUX_FUNC_C);
	pinmux_pin_set(muxa, 10, PINMUX_FUNC_C);
#endif

#if DT_UART_SAM0_SERCOM5_BASE_ADDRESS
	/* SERCOM5 on RX=PB23, TX=PB22 */
	pinmux_pin_set(muxb, 23, PINMUX_FUNC_D);
	pinmux_pin_set(muxb, 22, PINMUX_FUNC_D);
#endif

#if DT_UART_SAM0_SERCOM1_BASE_ADDRESS
#error Pin mapping is not configured
#endif
#if DT_UART_SAM0_SERCOM2_BASE_ADDRESS
#error Pin mapping is not configured
#endif
#if DT_UART_SAM0_SERCOM3_BASE_ADDRESS
#error Pin mapping is not configured
#endif
#if DT_UART_SAM0_SERCOM4_BASE_ADDRESS
#error Pin mapping is not configured
#endif

#if DT_SPI_SAM0_SERCOM4_BASE_ADDRESS
	/* SPI SERCOM4 on MISO=PA12/pad 0, MOSI=PB10/pad 2, SCK=PB11/pad 3 */
	pinmux_pin_set(muxa, 12, PINMUX_FUNC_D);
	pinmux_pin_set(muxb, 10, PINMUX_FUNC_D);
	pinmux_pin_set(muxb, 11, PINMUX_FUNC_D);
#endif

#if DT_SPI_SAM0_SERCOM0_BASE_ADDRESS
#error Pin mapping is not configured
#endif
#if DT_SPI_SAM0_SERCOM1_BASE_ADDRESS
#error Pin mapping is not configured
#endif
#if DT_SPI_SAM0_SERCOM2_BASE_ADDRESS
#error Pin mapping is not configured
#endif
#if DT_SPI_SAM0_SERCOM3_BASE_ADDRESS
#error Pin mapping is not configured
#endif
#if DT_SPI_SAM0_SERCOM5_BASE_ADDRESS
#error Pin mapping is not configured
#endif

#ifdef CONFIG_USB_DC_SAM0
	/* USB DP on PA25, USB DM on PA24 */
	pinmux_pin_set(muxa, 25, PINMUX_FUNC_G);
	pinmux_pin_set(muxa, 24, PINMUX_FUNC_G);
#endif

	return 0;
}

SYS_INIT(board_pinmux_init, PRE_KERNEL_1, CONFIG_PINMUX_INIT_PRIORITY);
