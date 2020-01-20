/*
 * Copyright (c) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <ztest.h>

#define NSEC_PER_MSEC (u64_t)(NSEC_PER_USEC * USEC_PER_MSEC)
/**
 * @brief Test delay during boot
 * @defgroup kernel_bootdelay_tests Init
 * @ingroup all_tests
 * @{
 */

/**
 * @brief This module verifies the delay specified during boot.
 */
void test_verify_bootdelay(void)
{
	u32_t current_cycles = k_cycle_get_32();

	/* compare this with the boot delay specified */
	zassert_true(k_cyc_to_ns_floor64(current_cycles) >=
			(NSEC_PER_MSEC * CONFIG_BOOT_DELAY),
			"boot delay not executed");
}

/**
 * @}
 */
