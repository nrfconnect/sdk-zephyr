/*
 * Copyright (c) 2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nrf_acl.h"
#include <ztest.h>

/**
 * @brief Test Asserts
 *
 * This test verifies various assert macros provided by ztest.
 *
 */
static void test_assert(void)
{
	nrf_acl_access_control_set(NRF_ACL0, 0x1000, 0x1000, NRF_ACL_PERMISSIONS_NO_READ_NO_WRITE);
	zassert_equal(0x1000, nrf_acl_access_control_size_get(NRF_ACL0), "Incorrect size read out");
	zassert_equal(0x1000, nrf_acl_access_control_address_get(NRF_ACL0), "Incorrect address read out");
	zassert_equal(NRF_ACL_PERMISSIONS_NO_READ_NO_WRITE,
				  nrf_acl_access_control_perm_get(NRF_ACL0), "Incorrect permissions read out");
}

void test_main(void)
{
	ztest_test_suite(framework_tests,
		ztest_unit_test(test_assert)
	);
	ztest_run_test_suite(framework_tests);
}
