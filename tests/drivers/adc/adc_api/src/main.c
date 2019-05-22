/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 * Copyright (c) 2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @addtogroup t_driver_adc
 * @{
 * @defgroup t_adc_basic test_adc_basic_operations
 * @}
 */


#include <zephyr.h>
#include <ztest.h>

extern void test_adc_sample_one_channel(void);
extern void test_adc_sample_two_channels(void);
extern void test_adc_asynchronous_call(void);
extern void test_adc_sample_with_interval(void);
extern void test_adc_repeated_samplings(void);
extern void test_adc_invalid_request(void);
extern struct device *get_adc_device(void);
extern struct k_poll_signal async_sig;

void test_main(void)
{
	k_object_access_grant(get_adc_device(), k_current_get());
#ifdef CONFIG_ADC_ASYNC
	k_object_access_grant(&async_sig, k_current_get());
	k_poll_signal_init(&async_sig);
	k_thread_system_pool_assign(k_current_get());
#endif

	ztest_test_suite(adc_basic_test,
			 ztest_user_unit_test(test_adc_sample_one_channel),
			 ztest_user_unit_test(test_adc_sample_two_channels),
			 ztest_user_unit_test(test_adc_asynchronous_call),
			 ztest_unit_test(test_adc_sample_with_interval),
			 ztest_unit_test(test_adc_repeated_samplings),
			 ztest_user_unit_test(test_adc_invalid_request));
	ztest_run_test_suite(adc_basic_test);
}
