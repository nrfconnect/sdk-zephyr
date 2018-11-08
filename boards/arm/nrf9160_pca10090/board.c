/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <init.h>
#include <hal/nrf_clock.h>

static int pca10090_switch_to_xtal(struct device *dev)
{
	nrf_clock_task_trigger(NRF_CLOCK_TASK_HFCLKSTART);
	return 0;
}

SYS_INIT(pca10090_switch_to_xtal, POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT);

