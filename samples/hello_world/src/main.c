/*
 * Copyright (c) 2012-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <zephyr/kernel.h>

/* NCS API for Constant Latency mode */
#include <nrf_sys_event.h>

int main(void)
{
	/* Request Constant Latency mode for cross-power-domain UART operation
	 * UARTE21 (PERI domain) accessing P2 (MCU domain) requires this mode
	 */
	nrf_sys_event_request_global_constlat();

	printf("Hello World! %s\n", CONFIG_BOARD_TARGET);
	printf("UARTE21 on P2.08/P2.07 with Constant Latency mode\n");

	/* In a real application, release when UART is no longer needed:
	 * nrf_sys_event_release_global_constlat();
	 */

	return 0;
}
