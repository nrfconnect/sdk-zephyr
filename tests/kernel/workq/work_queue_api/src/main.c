/*
 * Copyright (c) 2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @brief Workqueue Tests
 * @defgroup kernel_workqueue_tests Workqueue
 * @ingroup all_tests
 * @{
 * @}
 */

#include <ztest.h>
#include <irq_offload.h>

#define TIMEOUT 100
#define STACK_SIZE 512
#define NUM_OF_WORK 2

static K_THREAD_STACK_DEFINE(tstack, STACK_SIZE);
static struct k_work_q workq;
static struct k_work work[NUM_OF_WORK];
static struct k_delayed_work new_work;
static struct k_delayed_work delayed_work[NUM_OF_WORK], delayed_work_sleepy;
static struct k_sem sync_sema;

static void work_sleepy(struct k_work *w)
{
	k_sleep(TIMEOUT);
	k_sem_give(&sync_sema);
}

static void work_handler(struct k_work *w)
{
	k_sem_give(&sync_sema);
}

static void new_work_handler(struct k_work *w)
{
	k_sem_give(&sync_sema);
}

static void twork_submit(void *data)
{
	struct k_work_q *work_q = (struct k_work_q *)data;

	for (int i = 0; i < NUM_OF_WORK; i++) {
		/**TESTPOINT: init via k_work_init*/
		k_work_init(&work[i], work_handler);
		/**TESTPOINT: check pending after work init*/
		zassert_false(k_work_pending(&work[i]), NULL);
		if (work_q) {
			/**TESTPOINT: work submit to queue*/
			k_work_submit_to_queue(work_q, &work[i]);
		} else {
			/**TESTPOINT: work submit to system queue*/
			k_work_submit(&work[i]);
		}
	}
}

static void twork_submit_multipleq(void *data)
{
	struct k_work_q *work_q = (struct k_work_q *)data;

	/**TESTPOINT: init via k_work_init*/
	k_delayed_work_init(&new_work, new_work_handler);

	k_delayed_work_submit_to_queue(work_q, &new_work, TIMEOUT);

	zassert_equal(k_delayed_work_submit(&new_work, TIMEOUT),
		      -EADDRINUSE, NULL);

	k_sem_give(&sync_sema);
}

static void twork_resubmit(void *data)
{
	struct k_work_q *work_q = (struct k_work_q *)data;

	/**TESTPOINT: init via k_work_init*/
	k_delayed_work_init(&new_work, new_work_handler);

	k_delayed_work_submit_to_queue(work_q, &new_work, 0);

	/* This is done to test a neagtive case when k_delayed_work_cancel()
	 * fails in k_delayed_work_submit_to_queue API. Removing work from it
	 * queue make sure that k_delayed_work_cancel() fails when the Work is
	 * resubmitted.
	 */
	k_queue_remove(&(new_work.work_q->queue), &(new_work.work));

	zassert_equal(k_delayed_work_submit_to_queue(work_q, &new_work, 0),
		      -EINVAL, NULL);

	k_sem_give(&sync_sema);
}


static void tdelayed_work_submit(void *data)
{
	struct k_work_q *work_q = (struct k_work_q *)data;

	for (int i = 0; i < NUM_OF_WORK; i++) {
		/**TESTPOINT: init via k_delayed_work_init*/
		k_delayed_work_init(&delayed_work[i], work_handler);
		/**TESTPOINT: check pending after delayed work init*/
		zassert_false(k_work_pending((struct k_work *)&delayed_work[i]),
			      NULL);
		/**TESTPOINT: check remaining timeout before submit*/
		zassert_equal(k_delayed_work_remaining_get(&delayed_work[i]), 0,
			      NULL);
		if (work_q) {
			/**TESTPOINT: delayed work submit to queue*/
			zassert_true(k_delayed_work_submit_to_queue(work_q,
								    &delayed_work[i], TIMEOUT) == 0, NULL);
		} else {
			/**TESTPOINT: delayed work submit to system queue*/
			zassert_true(k_delayed_work_submit(&delayed_work[i],
							   TIMEOUT) == 0, NULL);
		}
		/**TESTPOINT: check remaining timeout after submit*/
		zassert_true(k_delayed_work_remaining_get(&delayed_work[i]) >=
			     TIMEOUT, NULL);
		/**TESTPOINT: check pending after delayed work submit*/
		zassert_true(k_work_pending((struct k_work *)&delayed_work[i])
			     == 0, NULL);
	}
}

static void tdelayed_work_cancel(void *data)
{
	struct k_work_q *work_q = (struct k_work_q *)data;
	int ret;

	k_delayed_work_init(&delayed_work_sleepy, work_sleepy);
	k_delayed_work_init(&delayed_work[0], work_handler);
	k_delayed_work_init(&delayed_work[1], work_handler);

	if (work_q) {
		ret = k_delayed_work_submit_to_queue(work_q,
						     &delayed_work_sleepy, TIMEOUT);
		ret |= k_delayed_work_submit_to_queue(work_q, &delayed_work[0],
						      TIMEOUT);
		ret |= k_delayed_work_submit_to_queue(work_q, &delayed_work[1],
						      TIMEOUT);
	} else {
		ret = k_delayed_work_submit(&delayed_work_sleepy, TIMEOUT);
		ret |= k_delayed_work_submit(&delayed_work[0], TIMEOUT);
		ret |= k_delayed_work_submit(&delayed_work[1], TIMEOUT);
	}
	/*
	 * t0: delayed submit three work items, all with delay=TIMEOUT
	 * >t0: cancel delayed_work[0], expected cancellation success
	 * >t0+TIMEOUT: handling delayed_work_sleepy, which do k_sleep TIMEOUT
	 *              pending delayed_work[1], check pending flag, expected 1
	 *              cancel delayed_work[1], expected 0
	 * >t0+2*TIMEOUT: delayed_work_sleepy completed
	 *                delayed_work[1] completed
	 *                cancel delayed_work_sleepy, expected 0
	 */
	zassert_true(ret == 0, NULL);
	/**TESTPOINT: delayed work cancel when countdown*/
	ret = k_delayed_work_cancel(&delayed_work[0]);
	zassert_true(ret == 0, NULL);
	/**TESTPOINT: check pending after delayed work cancel*/
	zassert_false(k_work_pending((struct k_work *)&delayed_work[0]), NULL);
	if (!k_is_in_isr()) {
		/*wait for handling work_sleepy*/
		k_sleep(TIMEOUT);
		/**TESTPOINT: check pending when work pending*/
		zassert_true(k_work_pending((struct k_work *)&delayed_work[1]),
			     NULL);
		/**TESTPOINT: delayed work cancel when pending*/
		ret = k_delayed_work_cancel(&delayed_work[1]);
		zassert_equal(ret, 0, NULL);
		k_sem_give(&sync_sema);
		/*wait for completed work_sleepy and delayed_work[1]*/
		k_sleep(TIMEOUT);
		/**TESTPOINT: check pending when work completed*/
		zassert_false(k_work_pending(
				      (struct k_work *)&delayed_work_sleepy), NULL);
		/**TESTPOINT: delayed work cancel when completed*/
		ret = k_delayed_work_cancel(&delayed_work_sleepy);
		zassert_equal(ret, 0, NULL);
	}
	/*work items not cancelled: delayed_work[1], delayed_work_sleepy*/
}

/*test cases*/
/**
 * @brief Test work queue start before submit
 *
 * @ingroup kernel_workqueue_tests
 *
 * @see k_work_q_start()
 */
void test_workq_start_before_submit(void)
{
	k_sem_init(&sync_sema, 0, NUM_OF_WORK);
	k_work_q_start(&workq, tstack, STACK_SIZE,
		       CONFIG_MAIN_THREAD_PRIORITY);
}

/**
 * @brief Test work submission to work queue
 *
 * @ingroup kernel_workqueue_tests
 *
 * @see k_work_init(), k_work_pending(), k_work_submit_to_queue(),
 * k_work_submit()
 */
void test_work_submit_to_queue_thread(void)
{
	k_sem_reset(&sync_sema);
	twork_submit(&workq);
	for (int i = 0; i < NUM_OF_WORK; i++) {
		k_sem_take(&sync_sema, K_FOREVER);
	}
}

/**
 * @brief Test submission of work to multiple queues
 *
 * @ingroup kernel_workqueue_tests
 *
 * @see k_delayed_work_init(), k_delayed_work_submit_to_queue(),
 * k_delayed_work_submit()
 */
void test_work_submit_to_multipleq(void)
{
	k_sem_reset(&sync_sema);
	twork_submit_multipleq(&workq);
	for (int i = 0; i < NUM_OF_WORK; i++) {
		k_sem_take(&sync_sema, K_FOREVER);
	}
}

/**
 * @brief Test work queue resubmission
 *
 * @ingroup kernel_workqueue_tests
 *
 * @see k_queue_remove(), k_delayed_work_init(),
 * k_delayed_work_submit_to_queue()
 */
void test_work_resubmit_to_queue(void)
{
	k_sem_reset(&sync_sema);
	twork_resubmit(&workq);
	k_sem_take(&sync_sema, K_FOREVER);
}

/**
 * @brief Test work submission to queue from ISR context
 *
 * @ingroup kernel_workqueue_tests
 *
 * @see k_work_init(), k_work_pending(), k_work_submit_to_queue(), k_work_submit()
 */
void test_work_submit_to_queue_isr(void)
{
	k_sem_reset(&sync_sema);
	irq_offload(twork_submit, (void *)&workq);
	for (int i = 0; i < NUM_OF_WORK; i++) {
		k_sem_take(&sync_sema, K_FOREVER);
	}
}

/**
 * @brief Test work submission to queue
 *
 * @ingroup kernel_workqueue_tests
 *
 * @see k_work_init(), k_work_pending(), k_work_submit_to_queue(), k_work_submit()
 */
void test_work_submit_thread(void)
{
	k_sem_reset(&sync_sema);
	twork_submit(NULL);
	for (int i = 0; i < NUM_OF_WORK; i++) {
		k_sem_take(&sync_sema, K_FOREVER);
	}
}

/**
 * @brief Test work submission from ISR context
 *
 * @ingroup kernel_workqueue_tests
 *
 * @see k_work_init(), k_work_pending(), k_work_submit_to_queue(), k_work_submit()
 */
void test_work_submit_isr(void)
{
	k_sem_reset(&sync_sema);
	irq_offload(twork_submit, NULL);
	for (int i = 0; i < NUM_OF_WORK; i++) {
		k_sem_take(&sync_sema, K_FOREVER);
	}
}

/**
 * @brief Test delayed work submission to queue
 *
 * @ingroup kernel_workqueue_tests
 *
 * @see k_delayed_work_init(), k_work_pending(),
 * k_delayed_work_remaining_get(), k_delayed_work_submit_to_queue(),
 * k_delayed_work_submit()
 */
void test_delayed_work_submit_to_queue_thread(void)
{
	k_sem_reset(&sync_sema);
	tdelayed_work_submit(&workq);
	for (int i = 0; i < NUM_OF_WORK; i++) {
		k_sem_take(&sync_sema, K_FOREVER);
	}
}

/**
 * @brief Test delayed work submission to queue in ISR context
 *
 * @ingroup kernel_workqueue_tests
 *
 * @see k_delayed_work_init(), k_work_pending(),
 * k_delayed_work_remaining_get(), k_delayed_work_submit_to_queue(),
 * k_delayed_work_submit()
 */
void test_delayed_work_submit_to_queue_isr(void)
{
	k_sem_reset(&sync_sema);
	irq_offload(tdelayed_work_submit, (void *)&workq);
	for (int i = 0; i < NUM_OF_WORK; i++) {
		k_sem_take(&sync_sema, K_FOREVER);
	}
}

/**
 * @brief Test delayed work submission
 *
 * @ingroup kernel_workqueue_tests
 *
 * @see k_delayed_work_init(), k_work_pending(),
 * k_delayed_work_remaining_get(), k_delayed_work_submit_to_queue(),
 * k_delayed_work_submit()
 */
void test_delayed_work_submit_thread(void)
{
	k_sem_reset(&sync_sema);
	tdelayed_work_submit(NULL);
	for (int i = 0; i < NUM_OF_WORK; i++) {
		k_sem_take(&sync_sema, K_FOREVER);
	}
}

/**
 * @brief Test delayed work submission from ISR context
 *
 * @ingroup kernel_workqueue_tests
 *
 * @see k_delayed_work_init(), k_work_pending(),
 * k_delayed_work_remaining_get(), k_delayed_work_submit_to_queue(),
 * k_delayed_work_submit()
 */
void test_delayed_work_submit_isr(void)
{
	k_sem_reset(&sync_sema);
	irq_offload(tdelayed_work_submit, NULL);
	for (int i = 0; i < NUM_OF_WORK; i++) {
		k_sem_take(&sync_sema, K_FOREVER);
	}
}

/**
 * @brief Test delayed work cancel from work queue
 *
 * @ingroup kernel_workqueue_tests
 *
 * @see k_delayed_work_cancel(), k_work_pending()
 */
void test_delayed_work_cancel_from_queue_thread(void)
{
	k_sem_reset(&sync_sema);
	tdelayed_work_cancel(&workq);
	/*wait for work items that could not be cancelled*/
	for (int i = 0; i < NUM_OF_WORK; i++) {
		k_sem_take(&sync_sema, K_FOREVER);
	}
}

/**
 * @brief Test delayed work cancel from work queue from ISR context
 *
 * @ingroup kernel_workqueue_tests
 *
 * @see k_delayed_work_cancel(), k_work_pending()
 */
void test_delayed_work_cancel_from_queue_isr(void)
{
	k_sem_reset(&sync_sema);
	irq_offload(tdelayed_work_cancel, &workq);
	/*wait for work items that could not be cancelled*/
	for (int i = 0; i < NUM_OF_WORK; i++) {
		k_sem_take(&sync_sema, K_FOREVER);
	}
}

/**
 * @brief Test delayed work cancel
 *
 * @ingroup kernel_workqueue_tests
 *
 * @see k_delayed_work_cancel(), k_work_pending()
 */
void test_delayed_work_cancel_thread(void)
{
	k_sem_reset(&sync_sema);
	tdelayed_work_cancel(NULL);
	/*wait for work items that could not be cancelled*/
	for (int i = 0; i < NUM_OF_WORK; i++) {
		k_sem_take(&sync_sema, K_FOREVER);
	}
}

/**
 * @brief Test delayed work cancel from ISR context
 *
 * @ingroup kernel_workqueue_tests
 *
 * @see k_delayed_work_cancel(), k_work_pending()
 */
void test_delayed_work_cancel_isr(void)
{
	k_sem_reset(&sync_sema);
	irq_offload(tdelayed_work_cancel, NULL);
	/*wait for work items that could not be cancelled*/
	for (int i = 0; i < NUM_OF_WORK; i++) {
		k_sem_take(&sync_sema, K_FOREVER);
	}
}


void test_main(void)
{
	ztest_test_suite(workqueue_api,
			 ztest_unit_test(test_workq_start_before_submit),/*keep first!*/
			 ztest_unit_test(test_work_submit_to_multipleq),
			 ztest_unit_test(test_work_resubmit_to_queue),
			 ztest_unit_test(test_work_submit_to_queue_thread),
			 ztest_unit_test(test_work_submit_to_queue_isr),
			 ztest_unit_test(test_work_submit_thread),
			 ztest_unit_test(test_work_submit_isr),
			 ztest_unit_test(test_delayed_work_submit_to_queue_thread),
			 ztest_unit_test(test_delayed_work_submit_to_queue_isr),
			 ztest_unit_test(test_delayed_work_submit_thread),
			 ztest_unit_test(test_delayed_work_submit_isr),
			 ztest_unit_test(test_delayed_work_cancel_from_queue_thread),
			 ztest_unit_test(test_delayed_work_cancel_from_queue_isr),
			 ztest_unit_test(test_delayed_work_cancel_thread),
			 ztest_unit_test(test_delayed_work_cancel_isr));
	ztest_run_test_suite(workqueue_api);
}
