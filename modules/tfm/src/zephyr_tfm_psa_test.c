/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>

/**
 * \brief This symbol is the entry point provided by the PSA API compliance
 *        test libraries
 */
extern void val_entry(void);

/**
 * \brief This symbol is the TIMER1 IRQ handler used by the TFM PSA test.
 */
extern void TIMER1_Handler(void);

__attribute__((noreturn))
void main(void)
{
	IRQ_CONNECT(TIMER1_IRQn, NRFX_TIMER_DEFAULT_CONFIG_IRQ_PRIORITY,
		TIMER1_Handler, NULL, 0);

	val_entry();

	for (;;) {
	}
}
