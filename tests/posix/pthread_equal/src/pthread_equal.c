/*
 * Copyright (c) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <pthread.h>
#include <ztest.h>

#define STACKSZ 1024

pthread_t thread;

K_THREAD_STACK_ARRAY_DEFINE(stacks, 1, STACKSZ);

void *thread_top(void *p1)
{
	thread = pthread_self();
	pthread_exit(NULL);
	return NULL;
}

void test_pthread_equal(void)
{
	int ret = 1;
	pthread_attr_t attr;
	struct sched_param schedparam;
	pthread_t newthread;

	pthread_attr_init(&attr);
	schedparam.priority = 2;
	pthread_attr_setschedparam(&attr, &schedparam);
	pthread_attr_setstack(&attr, &stacks[0][0], STACKSZ);

	ret = pthread_create(&newthread, &attr, thread_top,
			(void *)0);

	/*TESTPOINT: Check if thread is created*/
	zassert_false(ret, "attempt to create thread failed\n");

	pthread_join(newthread, NULL);

	/*TESTPOINT: Check if threads are equal*/
	zassert_true(pthread_equal(newthread, thread),
			"thread IDs should be equal! exiting...\n");

	/*TESTPOINT: Check case when threads are not equal*/
	zassert_false(pthread_equal(newthread, k_current_get()),
			"thread IDs cannot be equal! exiting...\n");
}

void test_main(void)
{
	ztest_test_suite(test_pthreads_equal,
			ztest_unit_test(test_pthread_equal));
	ztest_run_test_suite(test_pthreads_equal);
}
