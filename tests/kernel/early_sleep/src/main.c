/*
 * Copyright (c) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * @file
 * @brief Test early sleep functionality
 *
 * This test verifies that k_sleep() can be used to put the calling thread to
 * sleep for a specified number of ticks during system initialization.  In this
 * test we are calling k_sleep() at POST_KERNEL and APPLICATION level
 * initialization sequence.
 *
 * Note: We can not call k_sleep() during PRE_KERNEL1 or PRE_KERNEL2 level
 * because the core kernel objects and devices initialization happens at these
 * levels.
 */

#include <init.h>
#include <arch/cpu.h>
#include <sys_clock.h>
#include <stdbool.h>
#include <tc_util.h>
#include <ztest.h>

#define THREAD_STACK		(384 + CONFIG_TEST_EXTRA_STACKSIZE)

#define TEST_TICKS_TO_SLEEP	50

/* Helper thread data */
static K_THREAD_STACK_DEFINE(helper_tstack, THREAD_STACK);
static struct k_thread helper_tdata;
static k_tid_t  helper_ttid;

/* time that the thread was actually sleeping */
static int actual_sleep_ticks;
static int actual_post_kernel_sleep_ticks;
static int actual_app_sleep_ticks;

static bool test_failure = true;

static void helper_thread(void *p1, void *p2, void *p3)
{
	test_failure = false;
}

static int ticks_to_sleep(int ticks)
{
	u32_t start_time;
	u32_t stop_time;

	start_time = k_cycle_get_32();
	k_sleep(__ticks_to_ms(ticks));
	stop_time = k_cycle_get_32();

	return (stop_time - start_time) / sys_clock_hw_cycles_per_tick;
}


static int test_early_sleep_post_kernel(struct device *unused)
{
	ARG_UNUSED(unused);
	actual_post_kernel_sleep_ticks = ticks_to_sleep(TEST_TICKS_TO_SLEEP);
	return 0;
}

SYS_INIT(test_early_sleep_post_kernel,
		POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEVICE);

static int test_early_sleep_app(struct device *unused)
{
	ARG_UNUSED(unused);
	actual_app_sleep_ticks = ticks_to_sleep(TEST_TICKS_TO_SLEEP);
	return 0;
}

SYS_INIT(test_early_sleep_app, APPLICATION, CONFIG_KERNEL_INIT_PRIORITY_DEVICE);

/**
 * @brief Test early sleep
 *
 * @see k_sleep()
 */
static void test_early_sleep(void)
{
	TC_PRINT("Testing early sleeping\n");

	/*
	 * Main thread(test_main) priority is 0 but ztest thread runs at
	 * priority -1. To run the test smoothly make both main and ztest
	 * threads run at same priority level.
	 */
	k_thread_priority_set(k_current_get(), 0);

	TC_PRINT("msec per tick: %lld.%03lld, ticks to sleep: %d\n",
			__ticks_to_ms(1000) / 1000, __ticks_to_ms(1000) % 1000,
			TEST_TICKS_TO_SLEEP);

	/* Create a lower priority thread */
	helper_ttid = k_thread_create(&helper_tdata,
				   helper_tstack, THREAD_STACK,
				   helper_thread, NULL, NULL, NULL,
				   k_thread_priority_get(k_current_get()) + 1,
				   K_INHERIT_PERMS, 0);

	TC_PRINT("k_sleep() ticks at POST_KERNEL level: %d\n",
					actual_post_kernel_sleep_ticks);
	zassert_true((actual_post_kernel_sleep_ticks + 1) >
					TEST_TICKS_TO_SLEEP, NULL);

	TC_PRINT("k_sleep() ticks at APPLICATION level: %d\n",
					actual_app_sleep_ticks);
	zassert_true((actual_app_sleep_ticks + 1) >
					TEST_TICKS_TO_SLEEP, NULL);

	actual_sleep_ticks = ticks_to_sleep(TEST_TICKS_TO_SLEEP);
	TC_PRINT("k_sleep() ticks on running system: %d\n",
					actual_sleep_ticks);
	zassert_true((actual_sleep_ticks + 1) >
					TEST_TICKS_TO_SLEEP, NULL);

	zassert_false(test_failure, "Lower priority thread not ran!!");
}

/*test case main entry*/
void test_main(void)
{
	ztest_test_suite(test_earlysleep,
			ztest_unit_test(test_early_sleep));
	ztest_run_test_suite(test_earlysleep);
}
