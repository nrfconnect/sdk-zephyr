/*
 * Copyright (c) 2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <ztest.h>

static void expect_one_parameter(int a)
{
	ztest_check_expected_value(a);
}

static void expect_two_parameters(int a, int b)
{
	ztest_check_expected_value(a);
	ztest_check_expected_value(b);
}

static void test_parameter_tests(void)
{
	ztest_expect_value(expect_one_parameter, a, 1);
	expect_one_parameter(1);

	ztest_expect_value(expect_two_parameters, a, 2);
	ztest_expect_value(expect_two_parameters, b, 3);
	expect_two_parameters(2, 3);
}

static int returns_int(void)
{
	return ztest_get_return_value();
}

static void test_return_value_tests(void)
{
	ztest_returns_value(returns_int, 5);
	zassert_equal(returns_int(), 5, NULL);
}

static void test_multi_value_tests(void)
{
	/* Setup expects for three mock calls */
	ztest_expect_value(expect_one_parameter,  a, 1);
	ztest_expect_value(expect_two_parameters, a, 2);
	ztest_expect_value(expect_two_parameters, b, 3);
	ztest_returns_value(returns_int, 5);

	/* Verify mock behavior */
	expect_one_parameter(1);
	zassert_equal(returns_int(), 5, NULL);
	expect_two_parameters(2, 3);
}

void test_main(void)
{
	ztest_test_suite(mock_framework_tests,
			 ztest_unit_test(test_parameter_tests),
			 ztest_unit_test(test_return_value_tests),
			 ztest_unit_test(test_multi_value_tests)
			 );

	ztest_run_test_suite(mock_framework_tests);
}
