/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/modem/pipe.h>
#include <zephyr/modem/stats.h>

#ifndef ZEPHYR_MODEM_BACKEND_UART_SLM_
#define ZEPHYR_MODEM_BACKEND_UART_SLM_

#ifdef __cplusplus
extern "C" {
#endif

struct modem_backend_uart_slm {

};

struct modem_backend_uart_slm_config {

};

struct modem_pipe *modem_backend_uart_slm_init(struct modem_backend_uart_slm *backend,
					       const struct modem_backend_uart_slm_config *config);

#ifdef __cplusplus
}
#endif

#endif /* ZEPHYR_MODEM_BACKEND_UART_ */
