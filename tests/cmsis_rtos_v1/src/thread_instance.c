/*
 * Copyright (c) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <ztest.h>
#include <kernel.h>
#include <cmsis_os.h>

#ifdef CONFIG_COVERAGE
#define STACKSZ		(512 + CONFIG_TEST_EXTRA_STACKSIZE)
#else
#define STACKSZ		(512U)
#endif

void thread_inst_check(void const *argument)
{
	osThreadId id = osThreadGetId();

	zassert_true(id != NULL, "Failed getting ThreadId");
}

osThreadDef(thread_inst_check, osPriorityNormal, 3, STACKSZ);

void test_thread_instances(void)
{
	osThreadId id1, id2, id3, id4;

	id1 = osThreadCreate(osThread(thread_inst_check), NULL);
	zassert_true(id1 != NULL, "Failed creating thread_inst_check");

	id2 = osThreadCreate(osThread(thread_inst_check), NULL);
	zassert_true(id2 != NULL, "Failed creating thread_inst_check");

	id3 = osThreadCreate(osThread(thread_inst_check), NULL);
	zassert_true(id3 != NULL, "Failed creating thread_inst_check");

	id4 = osThreadCreate(osThread(thread_inst_check), NULL);
	zassert_true(id4 == NULL, "Something wrong with thread instances");
}
