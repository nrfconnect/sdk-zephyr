/*
 * Copyright (c) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <device.h>
#include <stdlib.h>
#include <drivers/kscan.h>
#include <zephyr.h>
#include <ztest.h>

#if defined(DT_KSCAN_0_NAME)
#define KSCAN_DEV_NAME DT_KSCAN_0_NAME
#endif

static void kb_callback(struct device *dev, u8_t row, u8_t col, bool pressed)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(row);
	ARG_UNUSED(col);
	ARG_UNUSED(pressed);
}

static int test_kb_callback(void)
{
	struct device *kscan_dev = device_get_binding(KSCAN_DEV_NAME);

	if (!kscan_dev) {
		TC_PRINT("Cannot get KBSCAN device\n");
		return TC_FAIL;
	}

	if (kscan_config(kscan_dev, kb_callback) != 0) {
		TC_PRINT("Unexpected error code received\n");
		return TC_FAIL;
	}

	return TC_PASS;
}

static int test_null_callback(void)
{
	struct device *kscan_dev = device_get_binding(KSCAN_DEV_NAME);

	if (!kscan_dev) {
		TC_PRINT("Cannot get KBSCAN device\n");
		return TC_FAIL;
	}

	if (kscan_config(kscan_dev, NULL) != -EINVAL) {
		TC_PRINT("Unexpected error code received\n");
		return TC_FAIL;
	}

	return TC_PASS;
}

static int test_disable_enable_callback(void)
{
	struct device *kscan_dev = device_get_binding(KSCAN_DEV_NAME);

	if (!kscan_dev) {
		TC_PRINT("Cannot get KBSCAN device\n");
		return TC_FAIL;
	}

	if (kscan_config(kscan_dev, kb_callback) != 0) {
		TC_PRINT("Unexpected error code received\n");
		return TC_FAIL;
	}

	if (kscan_disable_callback(kscan_dev) != 0) {
		TC_PRINT("Error while disabling callback\n");
		return TC_FAIL;
	}

	if (kscan_enable_callback(kscan_dev) != 0) {
		TC_PRINT("Error while enabling callback\n");
		return TC_FAIL;
	}

	return TC_PASS;
}

void test_init_callback(void)
{
	/* Configure kscan matrix with an appropriate callback */
	zassert_true(test_kb_callback() == TC_PASS, NULL);
	k_sleep(K_MSEC(1000));

	/* Configure kscan with a null callback */
	zassert_true(test_null_callback() == TC_PASS, NULL);
}

void test_control_callback(void)
{
	/* Disable/enable notifications to user */
	zassert_true(test_disable_enable_callback() == TC_PASS, NULL);
	k_sleep(K_MSEC(1000));
}
