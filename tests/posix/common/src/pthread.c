/*
 * Copyright (c) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <ztest.h>
#include <kernel.h>
#include <pthread.h>
#include <semaphore.h>

#define N_THR_E 3
#define N_THR_T 4
#define BOUNCES 64
#define STACKS (1024 + CONFIG_TEST_EXTRA_STACKSIZE)
#define THREAD_PRIORITY 3
#define ONE_SECOND 1

/* Macros to test invalid states */
#define PTHREAD_CANCEL_INVALID -1
#define SCHED_INVALID -1
#define PRIO_INVALID -1

K_THREAD_STACK_ARRAY_DEFINE(stack_e, N_THR_E, STACKS);
K_THREAD_STACK_ARRAY_DEFINE(stack_t, N_THR_T, STACKS);

void *thread_top_exec(void *p1);
void *thread_top_term(void *p1);

PTHREAD_MUTEX_DEFINE(lock);

PTHREAD_COND_DEFINE(cvar0);

PTHREAD_COND_DEFINE(cvar1);

PTHREAD_BARRIER_DEFINE(barrier, N_THR_E);

sem_t main_sem;

static int bounce_failed;
static int bounce_done[N_THR_E];

static int curr_bounce_thread;

static int barrier_failed;
static int barrier_done[N_THR_E];
static int barrier_return[N_THR_E];

/* First phase bounces execution between two threads using a condition
 * variable, continuously testing that no other thread is mucking with
 * the protected state.  This ends with all threads going back to
 * sleep on the condition variable and being woken by main() for the
 * second phase.
 *
 * Second phase simply lines up all the threads on a barrier, verifies
 * that none run until the last one enters, and that all run after the
 * exit.
 *
 * Test success is signaled to main() using a traditional semaphore.
 */

void *thread_top_exec(void *p1)
{
	int i, j, id = (int) p1;
	int policy;
	struct sched_param schedparam;

	pthread_getschedparam(pthread_self(), &policy, &schedparam);
	printk("Thread %d starting with scheduling policy %d & priority %d\n",
		 id, policy, schedparam.sched_priority);
	/* Try a double-lock here to exercise the failing case of
	 * trylock.  We don't support RECURSIVE locks, so this is
	 * guaranteed to fail.
	 */
	pthread_mutex_lock(&lock);

	if (!pthread_mutex_trylock(&lock)) {
		printk("pthread_mutex_trylock inexplicably succeeded\n");
		bounce_failed = 1;
	}

	pthread_mutex_unlock(&lock);

	for (i = 0; i < BOUNCES; i++) {

		pthread_mutex_lock(&lock);

		/* Wait for the current owner to signal us, unless we
		 * are the very first thread, in which case we need to
		 * wait a bit to be sure the other threads get
		 * scheduled and wait on cvar0.
		 */
		if (!(id == 0 && i == 0)) {
			pthread_cond_wait(&cvar0, &lock);
		} else {
			pthread_mutex_unlock(&lock);
			usleep(USEC_PER_MSEC * 500U);
			pthread_mutex_lock(&lock);
		}

		/* Claim ownership, then try really hard to give someone
		 * else a shot at hitting this if they are racing.
		 */
		curr_bounce_thread = id;
		for (j = 0; j < 1000; j++) {
			if (curr_bounce_thread != id) {
				printk("Racing bounce threads\n");
				bounce_failed = 1;
				sem_post(&main_sem);
				pthread_mutex_unlock(&lock);
				return NULL;
			}
			sched_yield();
		}

		/* Next one's turn, go back to the top and wait.  */
		pthread_cond_signal(&cvar0);
		pthread_mutex_unlock(&lock);
	}

	/* Signal we are complete to main(), then let it wake us up.  Note
	 * that we are using the same mutex with both cvar0 and cvar1,
	 * which is non-standard but kosher per POSIX (and it works fine
	 * in our implementation
	 */
	pthread_mutex_lock(&lock);
	bounce_done[id] = 1;
	sem_post(&main_sem);
	pthread_cond_wait(&cvar1, &lock);
	pthread_mutex_unlock(&lock);

	/* Now just wait on the barrier.  Make sure no one else finished
	 * before we wait on it, then signal that we're done
	 */
	for (i = 0; i < N_THR_E; i++) {
		if (barrier_done[i]) {
			printk("Barrier exited early\n");
			barrier_failed = 1;
			sem_post(&main_sem);
		}
	}
	barrier_return[id] = pthread_barrier_wait(&barrier);
	barrier_done[id] = 1;
	sem_post(&main_sem);
	pthread_exit(p1);

	return NULL;
}

int bounce_test_done(void)
{
	int i;

	if (bounce_failed) {
		return 1;
	}

	for (i = 0; i < N_THR_E; i++) {
		if (!bounce_done[i]) {
			return 0;
		}
	}

	return 1;
}

int barrier_test_done(void)
{
	int i;

	if (barrier_failed) {
		return 1;
	}

	for (i = 0; i < N_THR_E; i++) {
		if (!barrier_done[i]) {
			return 0;
		}
	}

	return 1;
}

void *thread_top_term(void *p1)
{
	pthread_t self;
	int oldstate, policy, ret;
	int val = (u32_t) p1;
	struct sched_param param, getschedparam;

	param.sched_priority = N_THR_T - (s32_t) p1;

	self = pthread_self();

	/* Change priority of thread */
	zassert_false(pthread_setschedparam(self, SCHED_RR, &param),
		      "Unable to set thread priority!");

	zassert_false(pthread_getschedparam(self, &policy, &getschedparam),
			"Unable to get thread priority!");

	printk("Thread %d starting with a priority of %d\n", (s32_t) p1,
			getschedparam.sched_priority);

	if (val % 2) {
		ret = pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &oldstate);
		zassert_false(ret, "Unable to set cancel state!");
	}

	if (val >= 2) {
		ret = pthread_detach(self);
		if (val == 2) {
			zassert_equal(ret, EINVAL, "re-detached thread!");
		}
	}

	printk("Cancelling thread %d\n", (s32_t) p1);
	pthread_cancel(self);
	printk("Thread %d could not be cancelled\n", (s32_t) p1);
	sleep(ONE_SECOND);
	pthread_exit(p1);
	return NULL;
}

void test_posix_pthread_execution(void)
{
	int i, ret, min_prio, max_prio;
	int dstate, policy;
	pthread_attr_t attr[N_THR_E] = {};
	struct sched_param schedparam, getschedparam;
	pthread_t newthread[N_THR_E];
	int schedpolicy = SCHED_FIFO;
	void *retval, *stackaddr;
	size_t stacksize;
	int serial_threads = 0;

	sem_init(&main_sem, 0, 1);
	schedparam.sched_priority = CONFIG_NUM_COOP_PRIORITIES - 1;
	min_prio = sched_get_priority_min(schedpolicy);
	max_prio = sched_get_priority_max(schedpolicy);

	ret = (min_prio < 0 || max_prio < 0 ||
			schedparam.sched_priority < min_prio ||
			schedparam.sched_priority > max_prio);

	/* TESTPOINT: Check if scheduling priority is valid */
	zassert_false(ret,
			"Scheduling priority outside valid priority range");

	/* TESTPOINTS: Try setting attributes before init */
	ret = pthread_attr_setschedparam(&attr[0], &schedparam);
	zassert_equal(ret, EINVAL, "uninitialized attr set!");

	ret = pthread_attr_setdetachstate(&attr[0], PTHREAD_CREATE_JOINABLE);
	zassert_equal(ret, EINVAL, "uninitialized attr set!");

	ret = pthread_attr_setschedpolicy(&attr[0], schedpolicy);
	zassert_equal(ret, EINVAL, "uninitialized attr set!");

	/* TESTPOINT: Try setting attribute with empty stack */
	ret = pthread_attr_setstack(&attr[0], 0, STACKS);
	zassert_equal(ret, EACCES, "empty stack set!");

	/* TESTPOINTS: Try getting attributes before init */
	ret = pthread_attr_getschedparam(&attr[0], &getschedparam);
	zassert_equal(ret, EINVAL, "uninitialized attr retrieved!");

	ret = pthread_attr_getdetachstate(&attr[0], &dstate);
	zassert_equal(ret, EINVAL, "uninitialized attr retrieved!");

	ret = pthread_attr_getschedpolicy(&attr[0], &policy);
	zassert_equal(ret, EINVAL, "uninitialized attr retrieved!");

	ret = pthread_attr_getstack(&attr[0], &stackaddr, &stacksize);
	zassert_equal(ret, EINVAL, "uninitialized attr retrieved!");

	ret = pthread_attr_getstacksize(&attr[0], &stacksize);
	zassert_equal(ret, EINVAL, "uninitialized attr retrieved!");

	/* TESTPOINT: Try destroying attr before init */
	ret = pthread_attr_destroy(&attr[0]);
	zassert_equal(ret, EINVAL, "uninitialized attr destroyed!");

	/* TESTPOINT: Try creating thread before attr init */
	ret = pthread_create(&newthread[0], &attr[0],
				thread_top_exec, NULL);
	zassert_equal(ret, EINVAL, "thread created before attr init!");

	for (i = 0; i < N_THR_E; i++) {
		ret = pthread_attr_init(&attr[i]);
		if (ret != 0) {
			zassert_false(pthread_attr_destroy(&attr[i]),
				      "Unable to destroy pthread object attrib");
			zassert_false(pthread_attr_init(&attr[i]),
				      "Unable to create pthread object attrib");
		}

		/* TESTPOINTS: Retrieve set stack attributes and compare */
		pthread_attr_setstack(&attr[i], &stack_e[i][0], STACKS);
		pthread_attr_getstack(&attr[i], &stackaddr, &stacksize);
		zassert_equal_ptr(attr[i].stack, stackaddr,
				"stack attribute addresses do not match!");
		zassert_equal(STACKS, stacksize, "stack sizes do not match!");

		pthread_attr_getstacksize(&attr[i], &stacksize);
		zassert_equal(STACKS, stacksize, "stack sizes do not match!");

		pthread_attr_setschedpolicy(&attr[i], schedpolicy);
		pthread_attr_getschedpolicy(&attr[i], &policy);
		zassert_equal(schedpolicy, policy,
				"scheduling policies do not match!");

		pthread_attr_setschedparam(&attr[i], &schedparam);
		pthread_attr_getschedparam(&attr[i], &getschedparam);
		zassert_equal(schedparam.sched_priority,
			      getschedparam.sched_priority,
			      "scheduling priorities do not match!");

		ret = pthread_create(&newthread[i], &attr[i], thread_top_exec,
				(void *)i);

		/* TESTPOINT: Check if thread is created successfully */
		zassert_false(ret, "Number of threads exceed max limit");
	}

	while (!bounce_test_done()) {
		sem_wait(&main_sem);
	}

	/* TESTPOINT: Check if bounce test passes */
	zassert_false(bounce_failed, "Bounce test failed");

	printk("Bounce test OK\n");

	/* Wake up the worker threads */
	pthread_mutex_lock(&lock);
	pthread_cond_broadcast(&cvar1);
	pthread_mutex_unlock(&lock);

	while (!barrier_test_done()) {
		sem_wait(&main_sem);
	}

	/* TESTPOINT: Check if barrier test passes */
	zassert_false(barrier_failed, "Barrier test failed");

	for (i = 0; i < N_THR_E; i++) {
		pthread_join(newthread[i], &retval);
	}

	for (i = 0; i < N_THR_E; i++) {
		if (barrier_return[i] == PTHREAD_BARRIER_SERIAL_THREAD) {
			++serial_threads;
		}
	}

	/* TESTPOINT: Check only one PTHREAD_BARRIER_SERIAL_THREAD returned. */
	zassert_true(serial_threads == 1, "Bungled barrier return value(s)");

	printk("Barrier test OK\n");
}

void test_posix_pthread_termination(void)
{
	s32_t i, ret;
	int oldstate, policy;
	pthread_attr_t attr[N_THR_T];
	struct sched_param schedparam;
	pthread_t newthread[N_THR_T];
	void *retval;

	/* Creating 4 threads with lowest application priority */
	for (i = 0; i < N_THR_T; i++) {
		ret = pthread_attr_init(&attr[i]);
		if (ret != 0) {
			zassert_false(pthread_attr_destroy(&attr[i]),
				      "Unable to destroy pthread object attrib");
			zassert_false(pthread_attr_init(&attr[i]),
				      "Unable to create pthread object attrib");
		}

		if (i == 2) {
			pthread_attr_setdetachstate(&attr[i],
						    PTHREAD_CREATE_DETACHED);
		}

		schedparam.sched_priority = 2;
		pthread_attr_setschedparam(&attr[i], &schedparam);
		pthread_attr_setstack(&attr[i], &stack_t[i][0], STACKS);
		ret = pthread_create(&newthread[i], &attr[i], thread_top_term,
				     (void *)i);

		zassert_false(ret, "Not enough space to create new thread");
	}

	/* TESTPOINT: Try setting invalid cancel state to current thread */
	ret = pthread_setcancelstate(PTHREAD_CANCEL_INVALID, &oldstate);
	zassert_equal(ret, EINVAL, "invalid cancel state set!");

	/* TESTPOINT: Try setting invalid policy */
	ret = pthread_setschedparam(newthread[0], SCHED_INVALID, &schedparam);
	zassert_equal(ret, EINVAL, "invalid policy set!");

	/* TESTPOINT: Try setting invalid priority */
	schedparam.sched_priority = PRIO_INVALID;
	ret = pthread_setschedparam(newthread[0], SCHED_RR, &schedparam);
	zassert_equal(ret, EINVAL, "invalid priority set!");

	for (i = 0; i < N_THR_T; i++) {
		pthread_join(newthread[i], &retval);
	}

	/* TESTPOINT: Test for deadlock */
	ret = pthread_join(pthread_self(), &retval);
	zassert_equal(ret, EDEADLK, "thread joined with self inexplicably!");

	/* TESTPOINT: Try canceling a terminated thread */
	ret = pthread_cancel(newthread[N_THR_T/2]);
	zassert_equal(ret, ESRCH, "cancelled a terminated thread!");

	/* TESTPOINT: Try getting scheduling info from terminated thread */
	ret = pthread_getschedparam(newthread[N_THR_T/2], &policy, &schedparam);
	zassert_equal(ret, ESRCH, "got attr from terminated thread!");
}
