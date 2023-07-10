/*
 * Copyright (c) Copyright (c) 2022 BrainCo Inc.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/device.h>
#include <zephyr/init.h>
#include <soc.h>

static int gd32l23x_init(void)
{
	SystemInit();

	return 0;
}

SYS_INIT(gd32l23x_init, PRE_KERNEL_1, 0);
