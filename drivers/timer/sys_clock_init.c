/*
 * Copyright (c) 2015 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Initialize system clock driver
 *
 * Initializing the timer driver is done in this module to reduce code
 * duplication.
 */

#include <kernel.h>
#include <init.h>
#include <drivers/system_timer.h>

/* Weak-linked noop defaults for optional driver interfaces: */

int __weak z_clock_driver_init(struct device *device)
{
	return 0;
}

int __weak z_clock_device_ctrl(struct device *device,
				 u32_t ctrl_command, void *context)
{
	return 0;
}

void __weak z_clock_set_timeout(s32_t ticks, bool idle)
{
}

void __weak z_clock_idle_exit(void)
{
}

void __weak sys_clock_disable(void)
{
}

SYS_DEVICE_DEFINE("sys_clock", z_clock_driver_init, z_clock_device_ctrl,
		PRE_KERNEL_2, CONFIG_SYSTEM_CLOCK_INIT_PRIORITY);
