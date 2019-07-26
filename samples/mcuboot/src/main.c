/*
 * Copyright (c) 2012-2014 Wind River Systems, Inc.
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <misc/printk.h>

void main(void)
{
	printk("Hello World! %s\n", CONFIG_BOARD);
}
