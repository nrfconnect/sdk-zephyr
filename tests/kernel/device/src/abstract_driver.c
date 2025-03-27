/*
 * Copyright (c) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/ztest.h>
#include <zephyr/device.h>
#include "abstract_driver.h"

#define MY_DRIVER_A	"my_driver_A"
#define MY_DRIVER_B	"my_driver_B"

/* define individual driver A */
static int my_driver_A_do_this(const struct device *dev, int foo, int bar)
{
	return foo + bar;
}

static void my_driver_A_do_that(const struct device *dev, unsigned int *baz)
{
	*baz = 1;
}

static DEVICE_API(abstract, my_driver_A_api_funcs) = {
	.do_this = my_driver_A_do_this,
	.do_that = my_driver_A_do_that,
};

int common_driver_init(const struct device *dev)
{
	return 0;
}

/* define individual driver B */
static int my_driver_B_do_this(const struct device *dev, int foo, int bar)
{
	return foo - bar;
}

static void my_driver_B_do_that(const struct device *dev, unsigned int *baz)
{
	*baz = 2;
}

static DEVICE_API(abstract, my_driver_B_api_funcs) = {
	.do_this = my_driver_B_do_this,
	.do_that = my_driver_B_do_that,
};

/**
 * @cond INTERNAL_HIDDEN
 */
DEVICE_DEFINE(my_driver_A, MY_DRIVER_A, &common_driver_init,
		NULL, NULL, NULL,
		POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,
		&my_driver_A_api_funcs);

DEVICE_DEFINE(my_driver_B, MY_DRIVER_B, &common_driver_init,
		NULL, NULL, NULL,
		POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,
		&my_driver_B_api_funcs);

/**
 * @endcond
 */
