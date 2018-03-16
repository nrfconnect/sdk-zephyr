/*
 * Copyright (c) 2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <ztest.h>
#include <stdio.h>
#ifdef KERNEL
__kernel static struct k_thread ztest_thread;
#endif

enum {
	TEST_PHASE_SETUP,
	TEST_PHASE_TEST,
	TEST_PHASE_TEARDOWN,
	TEST_PHASE_FRAMEWORK
} phase = TEST_PHASE_FRAMEWORK;

static int test_status;


static int cleanup_test(struct unit_test *test)
{
	int ret = TC_PASS;
	int mock_status;

	mock_status = _cleanup_mock();

#ifdef KERNEL
	/* we need to remove the ztest_thread information from the timeout_q.
	 * Because we reuse the same k_thread structure this would
	 * causes some problems.
	 */
	k_thread_abort(&ztest_thread);
#endif

	if (!ret && mock_status == 1) {
		PRINT("Test %s failed: Unused mock parameter values\n",
		      test->name);
		ret = TC_FAIL;
	} else if (!ret && mock_status == 2) {
		PRINT("Test %s failed: Unused mock return values\n",
		      test->name);
		ret = TC_FAIL;
	}

	return ret;
}

static void run_test_functions(struct unit_test *test)
{
	phase = TEST_PHASE_SETUP;
	test->setup();
	phase = TEST_PHASE_TEST;
	test->test();
	phase = TEST_PHASE_TEARDOWN;
	test->teardown();
	phase = TEST_PHASE_FRAMEWORK;
}

#ifndef KERNEL
#include <setjmp.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>

#define FAIL_FAST 0

static jmp_buf test_fail;
static jmp_buf test_pass;
static jmp_buf stack_fail;

void ztest_test_fail(void)
{
	raise(SIGABRT);
}

void ztest_test_pass(void)
{
	longjmp(test_pass, 1);
}

static void handle_signal(int sig)
{
	static const char *const phase_str[] = {
		"setup",
		"unit test",
		"teardown",
	};

	PRINT("    %s", strsignal(sig));
	switch (phase) {
	case TEST_PHASE_SETUP:
	case TEST_PHASE_TEST:
	case TEST_PHASE_TEARDOWN:
		PRINT(" at %s function\n", phase_str[phase]);
		longjmp(test_fail, 1);
	case TEST_PHASE_FRAMEWORK:
		PRINT("\n");
		longjmp(stack_fail, 1);
	}
}

static void init_testing(void)
{
	signal(SIGABRT, handle_signal);
	signal(SIGSEGV, handle_signal);

	if (setjmp(stack_fail)) {
		PRINT("Test suite crashed.");
		exit(1);
	}
}

static int run_test(struct unit_test *test)
{
	int ret = TC_PASS;

	TC_START(test->name);

	if (setjmp(test_fail)) {
		ret = TC_FAIL;
		goto out;
	}

	if (setjmp(test_pass)) {
		ret = TC_PASS;
		goto out;
	}

	run_test_functions(test);
out:
	ret |= cleanup_test(test);
	_TC_END_RESULT(ret, test->name);

	return ret;
}

#else /* KERNEL */

/* Zephyr's probably going to cause all tests to fail if one test fails, so
 * skip the rest of tests if one of them fails
 */
#ifdef CONFIG_ZTEST_FAIL_FAST
#define FAIL_FAST 1
#else
#define FAIL_FAST 0
#endif

#if CONFIG_ZTEST_STACKSIZE & (STACK_ALIGN - 1)
    #error "CONFIG_ZTEST_STACKSIZE must be a multiple of the stack alignment"
#endif

K_THREAD_STACK_DEFINE(ztest_thread_stack, CONFIG_ZTEST_STACKSIZE +
		      CONFIG_TEST_EXTRA_STACKSIZE);

static int test_result;
__kernel static struct k_sem test_end_signal;

void ztest_test_fail(void)
{
	test_result = -1;
	k_sem_give(&test_end_signal);
	k_thread_abort(k_current_get());
}

void ztest_test_pass(void)
{
	test_result = 0;
	k_sem_give(&test_end_signal);
	k_thread_abort(k_current_get());
}

static void init_testing(void)
{
	k_sem_init(&test_end_signal, 0, 1);
	k_object_access_all_grant(&test_end_signal);
}

static void test_cb(void *a, void *dummy2, void *dummy)
{
	struct unit_test *test = (struct unit_test *)a;

	ARG_UNUSED(dummy2);
	ARG_UNUSED(dummy);

	test_result = 1;
	run_test_functions(test);
	test_result = 0;

	k_sem_give(&test_end_signal);
}

static int run_test(struct unit_test *test)
{
	int ret = TC_PASS;

	TC_START(test->name);
	k_thread_create(&ztest_thread, ztest_thread_stack,
			K_THREAD_STACK_SIZEOF(ztest_thread_stack),
			(k_thread_entry_t) test_cb, (struct unit_test *)test,
			NULL, NULL, -1, test->thread_options | K_INHERIT_PERMS,
			0);
	/*
	 * There is an implicit expectation here that the thread that was
	 * spawned is still higher priority than the current thread.
	 *
	 * If that is not the case, it will have given the semaphore, which
	 * will have caused the current thread to run, _if_ the test case
	 * thread is preemptible, since it is higher priority. If there is
	 * another test case to be run after the current one finishes, the
	 * thread_stack will be reused for that new test case while the current
	 * test case has not finished running yet (it has given the semaphore,
	 * but has _not_ gone back to _thread_entry() and completed it's "abort
	 * phase": this will corrupt the kernel ready queue.
	 */
	k_sem_take(&test_end_signal, K_FOREVER);
	if (test_result) {
		ret = TC_FAIL;
	}

	if (!test_result || !FAIL_FAST) {
		ret |= cleanup_test(test);
	}

	_TC_END_RESULT(ret, test->name);

	return ret;
}

#endif /* !KERNEL */

void _ztest_run_test_suite(const char *name, struct unit_test *suite)
{
	int fail = 0;

	if (test_status < 0) {
		return;
	}

	init_testing();

	PRINT("Running test suite %s\n", name);
	PRINT_LINE;
	while (suite->test) {
		fail += run_test(suite);
		suite++;

		if (fail && FAIL_FAST) {
			break;
		}
	}
	if (fail) {
		TC_END_REPORT(TC_FAIL);
	} else {
		TC_END_REPORT(TC_PASS);
	}
	test_status = (test_status || fail) ? 1 : 0;
}

void test_main(void);

#ifndef KERNEL
int main(void)
{
	_init_mock();
	test_main();

	return test_status;
}
#else
void main(void)
{
	_init_mock();
	test_main();
}
#endif
