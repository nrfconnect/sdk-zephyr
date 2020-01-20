/*
 * Copyright 2018 Foundries.io Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <init.h>
#include <drivers/pinmux.h>
#include <drivers/gpio.h>
#include <fsl_port.h>

#ifdef CONFIG_BT_CTLR_DEBUG_PINS
struct device *vega_debug_portb;
struct device *vega_debug_portc;
struct device *vega_debug_portd;
#endif

static int rv32m1_vega_pinmux_init(struct device *dev)
{
	ARG_UNUSED(dev);

#ifdef CONFIG_PINMUX_RV32M1_PORTA
	__unused struct device *porta =
		device_get_binding(CONFIG_PINMUX_RV32M1_PORTA_NAME);
#endif
#ifdef CONFIG_PINMUX_RV32M1_PORTB
	__unused struct device *portb =
		device_get_binding(CONFIG_PINMUX_RV32M1_PORTB_NAME);
#endif
#ifdef CONFIG_PINMUX_RV32M1_PORTC
	__unused struct device *portc =
		device_get_binding(CONFIG_PINMUX_RV32M1_PORTC_NAME);
#endif
#ifdef CONFIG_PINMUX_RV32M1_PORTD
	__unused struct device *portd =
		device_get_binding(CONFIG_PINMUX_RV32M1_PORTD_NAME);
#endif
#ifdef CONFIG_PINMUX_RV32M1_PORTE
	__unused struct device *porte =
		device_get_binding(CONFIG_PINMUX_RV32M1_PORTE_NAME);
#endif

#ifdef CONFIG_UART_RV32M1_LPUART_0
	/* LPUART0 RX, TX */
	pinmux_pin_set(portc, 7, PORT_PCR_MUX(kPORT_MuxAlt3));
	pinmux_pin_set(portc, 8, PORT_PCR_MUX(kPORT_MuxAlt3));
#endif

#ifdef CONFIG_UART_RV32M1_LPUART_1
	/* LPUART1 RX, TX */
	pinmux_pin_set(portc, 29, PORT_PCR_MUX(kPORT_MuxAlt2));
	pinmux_pin_set(portc, 30, PORT_PCR_MUX(kPORT_MuxAlt2));
#endif

#if CONFIG_I2C_0
	/* LPI2C0 SCL, SDA - Arduino header */
	pinmux_pin_set(portc, 10, PORT_PCR_MUX(kPORT_MuxAlt4));
	pinmux_pin_set(portc, 9, PORT_PCR_MUX(kPORT_MuxAlt4));
#endif

#if CONFIG_I2C_3
	/* LPI2C3 SCL, SDA - FXOS8700 */
	pinmux_pin_set(porte, 30, PORT_PCR_MUX(kPORT_MuxAlt3));
	pinmux_pin_set(porte, 29, PORT_PCR_MUX(kPORT_MuxAlt3));
#endif

	/* FXOS8700 INT1, INT2, RST */
	pinmux_pin_set(porte, 1, PORT_PCR_MUX(kPORT_MuxAsGpio));
	pinmux_pin_set(porte, 22, PORT_PCR_MUX(kPORT_MuxAsGpio));
	pinmux_pin_set(porte, 27, PORT_PCR_MUX(kPORT_MuxAsGpio));

#ifdef CONFIG_SPI_0
	/* LPSPI0 SCK, SOUT, PCS2, SIN */
	pinmux_pin_set(portb,  4, PORT_PCR_MUX(kPORT_MuxAlt2));
	pinmux_pin_set(portb,  5, PORT_PCR_MUX(kPORT_MuxAlt2));
	pinmux_pin_set(portb,  6, PORT_PCR_MUX(kPORT_MuxAlt2));
	pinmux_pin_set(portb,  7, PORT_PCR_MUX(kPORT_MuxAlt2));
#endif

#if CONFIG_SPI_1
	/* LPSPI1 SCK, SIN, SOUT, CS */
	pinmux_pin_set(portb, 20, PORT_PCR_MUX(kPORT_MuxAlt2));
	pinmux_pin_set(portb, 21, PORT_PCR_MUX(kPORT_MuxAlt2));
	pinmux_pin_set(portb, 24, PORT_PCR_MUX(kPORT_MuxAlt2));
	pinmux_pin_set(portb, 22, PORT_PCR_MUX(kPORT_MuxAlt2));
#endif

#ifdef CONFIG_PWM_2
	/* RGB LEDs as PWM */
	pinmux_pin_set(porta, 22, PORT_PCR_MUX(kPORT_MuxAlt6));
	pinmux_pin_set(porta, 23, PORT_PCR_MUX(kPORT_MuxAlt6));
	pinmux_pin_set(porta, 24, PORT_PCR_MUX(kPORT_MuxAlt6));
#else
	/* RGB LEDs as GPIO */
	pinmux_pin_set(porta, 22, PORT_PCR_MUX(kPORT_MuxAsGpio));
	pinmux_pin_set(porta, 23, PORT_PCR_MUX(kPORT_MuxAsGpio));
	pinmux_pin_set(porta, 24, PORT_PCR_MUX(kPORT_MuxAsGpio));
#endif

#ifdef CONFIG_BT_CTLR_DEBUG_PINS

	pinmux_pin_set(portb, 29, PORT_PCR_MUX(kPORT_MuxAsGpio));

	pinmux_pin_set(portc, 28, PORT_PCR_MUX(kPORT_MuxAsGpio));
	pinmux_pin_set(portc, 29, PORT_PCR_MUX(kPORT_MuxAsGpio));
	pinmux_pin_set(portc, 30, PORT_PCR_MUX(kPORT_MuxAsGpio));

	pinmux_pin_set(portd, 0, PORT_PCR_MUX(kPORT_MuxAsGpio));
	pinmux_pin_set(portd, 1, PORT_PCR_MUX(kPORT_MuxAsGpio));
	pinmux_pin_set(portd, 2, PORT_PCR_MUX(kPORT_MuxAsGpio));
	pinmux_pin_set(portd, 3, PORT_PCR_MUX(kPORT_MuxAsGpio));
	pinmux_pin_set(portd, 4, PORT_PCR_MUX(kPORT_MuxAsGpio));
	pinmux_pin_set(portd, 5, PORT_PCR_MUX(kPORT_MuxAsGpio));

	struct device *gpio_dev = device_get_binding(DT_ALIAS_GPIO_B_LABEL);

	gpio_pin_configure(gpio_dev, 29, GPIO_DIR_OUT);

	gpio_dev = device_get_binding(DT_ALIAS_GPIO_C_LABEL);
	gpio_pin_configure(gpio_dev, 28, GPIO_DIR_OUT);
	gpio_pin_configure(gpio_dev, 29, GPIO_DIR_OUT);
	gpio_pin_configure(gpio_dev, 30, GPIO_DIR_OUT);

	gpio_dev = device_get_binding(DT_ALIAS_GPIO_D_LABEL);
	gpio_pin_configure(gpio_dev, 0, GPIO_DIR_OUT);
	gpio_pin_configure(gpio_dev, 1, GPIO_DIR_OUT);
	gpio_pin_configure(gpio_dev, 2, GPIO_DIR_OUT);
	gpio_pin_configure(gpio_dev, 3, GPIO_DIR_OUT);
	gpio_pin_configure(gpio_dev, 4, GPIO_DIR_OUT);
	gpio_pin_configure(gpio_dev, 5, GPIO_DIR_OUT);
#endif

	return 0;
}

SYS_INIT(rv32m1_vega_pinmux_init, PRE_KERNEL_1, CONFIG_PINMUX_INIT_PRIORITY);
