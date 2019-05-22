/*
 * Copyright (c) 2017 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <ztest.h>
#include <tc_util.h>
#include <kernel_structs.h>
#include <irq_offload.h>
#include <kswap.h>

#if defined(CONFIG_X86) && defined(CONFIG_X86_MMU)
#define STACKSIZE (8192)
#else
#define  STACKSIZE (2048 + CONFIG_TEST_EXTRA_STACKSIZE)
#endif
#define MAIN_PRIORITY 7
#define PRIORITY 5

static K_THREAD_STACK_DEFINE(alt_stack, STACKSIZE);

#if defined(CONFIG_STACK_SENTINEL) && !defined(CONFIG_ARCH_POSIX)
#define OVERFLOW_STACKSIZE (STACKSIZE / 2)
static k_thread_stack_t *overflow_stack =
		alt_stack + (STACKSIZE - OVERFLOW_STACKSIZE);
#else
#if defined(CONFIG_USERSPACE) && defined(CONFIG_ARC)
/* for ARC, privilege stack is merged into defined stack */
#define OVERFLOW_STACKSIZE (STACKSIZE + CONFIG_PRIVILEGED_STACK_SIZE)
#else
#define OVERFLOW_STACKSIZE STACKSIZE
#endif
#endif

static struct k_thread alt_thread;
volatile int rv;

static volatile int crash_reason;

/* On some architectures, k_thread_abort(_current) will return instead
 * of z_swap'ing away.
 *
 * On ARM the PendSV exception is queued and immediately fires upon
 * completing the exception path; the faulting thread is never run
 * again.
 *
 * On Xtensa/asm2 and x86_64 the handler is running in interrupt
 * context and on the interrupt stack and needs to return through the
 * interrupt exit code.
 *
 * In both cases the thread is guaranteed never to run again once we
 * return from the z_SysFatalErrorHandler().
 */
#if !(defined(CONFIG_ARM) || defined(CONFIG_XTENSA_ASM2) \
	|| defined(CONFIG_ARC) || defined(CONFIG_X86_64))
#define ERR_IS_NORETURN 1
#endif

#ifdef ERR_IS_NORETURN
FUNC_NORETURN
#endif
void z_SysFatalErrorHandler(unsigned int reason, const NANO_ESF *pEsf)
{
	TC_PRINT("Caught system error -- reason %d\n", reason);
	crash_reason = reason;

	k_thread_abort(_current);
#ifdef ERR_IS_NORETURN
	CODE_UNREACHABLE;
#endif
}

void alt_thread1(void)
{
#if defined(CONFIG_X86) || defined(CONFIG_X86_64)
	__asm__ volatile ("ud2");
#elif defined(CONFIG_NIOS2)
	__asm__ volatile ("trap");
#elif defined(CONFIG_ARC)
	__asm__ volatile ("swi");
#else
	/* Triggers usage fault on ARM, illegal instruction on RISCV32
	 * and xtensa
	 */
	{
		int illegal = 0;
		((void(*)(void))&illegal)();
	}
#endif
	rv = TC_FAIL;
}


void alt_thread2(void)
{
	unsigned int key;

	key = irq_lock();
	k_oops();
	TC_ERROR("SHOULD NEVER SEE THIS\n");
	rv = TC_FAIL;
	irq_unlock(key);
}

void alt_thread3(void)
{
	unsigned int key;

	key = irq_lock();
	k_panic();
	TC_ERROR("SHOULD NEVER SEE THIS\n");
	rv = TC_FAIL;
	irq_unlock(key);
}

#ifndef CONFIG_ARCH_POSIX
#ifdef CONFIG_STACK_SENTINEL
void blow_up_stack(void)
{
	char buf[OVERFLOW_STACKSIZE];

	TC_PRINT("posting %zu bytes of junk to stack...\n", sizeof(buf));
	(void)memset(buf, 0xbb, sizeof(buf));
}
#else
/* stack sentinel doesn't catch it in time before it trashes the entire kernel
 */
int stack_smasher(int val)
{
	return stack_smasher(val * 2) + stack_smasher(val * 3);
}

void blow_up_stack(void)
{
	stack_smasher(37);
}
#endif

void stack_sentinel_timer(void)
{
	u32_t cur_tick;

	/* Test that stack overflow check due to timer interrupt works */
	blow_up_stack();
	TC_PRINT("waiting for tick advance...\n");

	/* This test has tickless kernel disabled, z_tick_get_32() returns
	 * the current tick count without trying to offset it by checking
	 * time elapsed in the driver since last update
	 */
	cur_tick = z_tick_get_32();

	while (cur_tick == z_tick_get_32()) {
		/* spin */
	}

	TC_ERROR("should never see this\n");
	rv = TC_FAIL;
}

void stack_sentinel_swap(void)
{
	unsigned int key = irq_lock();

	/* Test that stack overflow check due to swap works */
	blow_up_stack();
	TC_PRINT("swapping...\n");
	z_swap_unlocked();
	TC_ERROR("should never see this\n");
	rv = TC_FAIL;
	irq_unlock(key);
}

void stack_hw_overflow(void)
{
	/* Test that HW stack overflow check works */
	blow_up_stack();
	TC_ERROR("should never see this\n");
	rv = TC_FAIL;
}

void check_stack_overflow(void *handler, u32_t flags)
{
	crash_reason = -1;
#ifdef CONFIG_STACK_SENTINEL
	/* When testing stack sentinel feature, the overflow stack is a
	 * smaller section of alt_stack near the end.
	 * In this way when it gets overflowed by blow_up_stack() we don't
	 * corrupt anything else and prevent the test case from completing.
	 */
	k_thread_create(&alt_thread, overflow_stack, OVERFLOW_STACKSIZE,
#else
	k_thread_create(&alt_thread, alt_stack,
			K_THREAD_STACK_SIZEOF(alt_stack),
#endif /* CONFIG_STACK_SENTINEL */
			(k_thread_entry_t)handler,
			NULL, NULL, NULL, K_PRIO_PREEMPT(PRIORITY), flags,
			K_NO_WAIT);

	zassert_equal(crash_reason, _NANO_ERR_STACK_CHK_FAIL,
		      "bad reason code got %d expected %d\n",
		      crash_reason, _NANO_ERR_STACK_CHK_FAIL);
	zassert_not_equal(rv, TC_FAIL, "thread was not aborted");
}
#endif /* !CONFIG_ARCH_POSIX */

/**
 * @brief Test the kernel fatal error handling works correctly
 * @details Manually trigger the crash with various ways and check
 * that the kernel is handling that properly or not. Also the crash reason
 * should match. Check for stack sentinel feature by overflowing the
 * thread's stack and check for the exception.
 *
 * @ingroup kernel_common_tests
 */
void test_fatal(void)
{
	rv = TC_PASS;

	/*
	 * Main thread(test_main) priority was 10 but ztest thread runs at
	 * priority -1. To run the test smoothly make both main and ztest
	 * threads run at same priority level.
	 */
	k_thread_priority_set(_current, K_PRIO_PREEMPT(MAIN_PRIORITY));

#ifndef CONFIG_ARCH_POSIX
	TC_PRINT("test alt thread 1: generic CPU exception\n");
	k_thread_create(&alt_thread, alt_stack,
			K_THREAD_STACK_SIZEOF(alt_stack),
			(k_thread_entry_t)alt_thread1,
			NULL, NULL, NULL, K_PRIO_COOP(PRIORITY), 0,
			K_NO_WAIT);
	zassert_not_equal(rv, TC_FAIL, "thread was not aborted");
#else
	/*
	 * We want the native OS to handle segfaults so we can debug it
	 * with the normal linux tools
	 */
	TC_PRINT("test alt thread 1: skipped for POSIX ARCH\n");
#endif

	TC_PRINT("test alt thread 2: initiate kernel oops\n");
	k_thread_create(&alt_thread, alt_stack,
			K_THREAD_STACK_SIZEOF(alt_stack),
			(k_thread_entry_t)alt_thread2,
			NULL, NULL, NULL, K_PRIO_COOP(PRIORITY), 0,
			K_NO_WAIT);
	k_thread_abort(&alt_thread);
	zassert_equal(crash_reason, _NANO_ERR_KERNEL_OOPS,
		      "bad reason code got %d expected %d\n",
		      crash_reason, _NANO_ERR_KERNEL_OOPS);
	zassert_not_equal(rv, TC_FAIL, "thread was not aborted");

	TC_PRINT("test alt thread 3: initiate kernel panic\n");
	k_thread_create(&alt_thread, alt_stack,
			K_THREAD_STACK_SIZEOF(alt_stack),
			(k_thread_entry_t)alt_thread3,
			NULL, NULL, NULL, K_PRIO_COOP(PRIORITY), 0,
			K_NO_WAIT);
	k_thread_abort(&alt_thread);
	zassert_equal(crash_reason, _NANO_ERR_KERNEL_PANIC,
		      "bad reason code got %d expected %d\n",
		      crash_reason, _NANO_ERR_KERNEL_PANIC);
	zassert_not_equal(rv, TC_FAIL, "thread was not aborted");

#ifndef CONFIG_ARCH_POSIX

#ifdef CONFIG_STACK_SENTINEL
	TC_PRINT("test stack sentinel overflow - timer irq\n");
	check_stack_overflow(stack_sentinel_timer, 0);

	TC_PRINT("test stack sentinel overflow - swap\n");
	check_stack_overflow(stack_sentinel_swap, 0);
#endif /* CONFIG_STACK_SENTINEL */

#ifdef CONFIG_HW_STACK_PROTECTION
	/* HW based stack overflow detection.
	 * Do this twice to show that HW-based solutions work more than
	 * once.
	 */

	TC_PRINT("test stack HW-based overflow - supervisor 1\n");
	check_stack_overflow(stack_hw_overflow, 0);

	TC_PRINT("test stack HW-based overflow - supervisor 2\n");
	check_stack_overflow(stack_hw_overflow, 0);
#endif /* CONFIG_HW_STACK_PROTECTION */

#ifdef CONFIG_USERSPACE

	TC_PRINT("test stack HW-based overflow - user 1\n");
	check_stack_overflow(stack_hw_overflow, K_USER);

	TC_PRINT("test stack HW-based overflow - user 2\n");
	check_stack_overflow(stack_hw_overflow, K_USER);
#endif /* CONFIG_USERSPACE */

#endif /* !CONFIG_ARCH_POSIX */
}

/*test case main entry*/
void test_main(void)
{
	ztest_test_suite(fatal,
			ztest_unit_test(test_fatal));
	ztest_run_test_suite(fatal);
}
