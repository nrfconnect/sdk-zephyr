/*
 * Copyright (c) 2016 Open-RnD Sp. z o.o.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @brief Driver for UART port on STM32 family processor.
 *
 */

#ifndef ZEPHYR_DRIVERS_SERIAL_UART_STM32_H_
#define ZEPHYR_DRIVERS_SERIAL_UART_STM32_H_

/* device config */
struct uart_stm32_config {
	struct uart_device_config uconf;
	/* clock subsystem driving this peripheral */
	struct stm32_pclken pclken;
};

/* driver data */
struct uart_stm32_data {
	/* Baud rate */
	u32_t baud_rate;
	/* clock device */
	struct device *clock;
#ifdef CONFIG_UART_INTERRUPT_DRIVEN
	uart_irq_callback_user_data_t user_cb;
	void *user_data;
#endif
};

#endif	/* ZEPHYR_DRIVERS_SERIAL_UART_STM32_H_ */
