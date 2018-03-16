/*
 * Copyright (c) 2017 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __PTHREAD_H__
#define __PTHREAD_H__

#include <kernel.h>
#include <time.h>
#include "sys/types.h"
#include "posix_sched.h"
#include "unistd.h"

enum pthread_state {
	/* The thread is running and joinable. */
	PTHREAD_JOINABLE = 0,
	/* The thread is running and detached. */
	PTHREAD_DETACHED,
	/* A joinable thread exited and its return code is available. */
	PTHREAD_EXITED,
	/* The thread structure is unallocated and available for reuse. */
	PTHREAD_TERMINATED
};

struct posix_thread {
	struct k_thread thread;

	/* Exit status */
	void *retval;

	/* Pthread cancellation */
	int cancel_state;
	int cancel_pending;
	struct k_sem cancel_lock_sem;
	pthread_mutex_t cancel_lock;

	/* Pthread State */
	enum pthread_state state;
	pthread_mutex_t state_lock;
	struct k_sem state_lock_sem;
	pthread_cond_t state_cond;
};

/* Pthread detach/joinable */
#define PTHREAD_CREATE_JOINABLE     0
#define PTHREAD_CREATE_DETACHED     1

/* Pthread cancellation */
#define _PTHREAD_CANCEL_POS	0
#define PTHREAD_CANCEL_ENABLE	(0 << _PTHREAD_CANCEL_POS)
#define PTHREAD_CANCEL_DISABLE	(1 << _PTHREAD_CANCEL_POS)

/**
 * @brief Declare a pthread condition variable
 *
 * Declaration API for a pthread condition variable.  This is not a
 * POSIX API, it's provided to better conform with Zephyr's allocation
 * strategies for kernel objects.
 *
 * @param name Symbol name of the condition variable
 */
#define PTHREAD_COND_DEFINE(name)					\
	struct pthread_cond name = {					\
		.wait_q = SYS_DLIST_STATIC_INIT(&name.wait_q),		\
	}

/**
 * @brief POSIX threading compatibility API
 *
 * See IEEE 1003.1
 */
static inline int pthread_cond_init(pthread_cond_t *cv,
				    const pthread_condattr_t *att)
{
	ARG_UNUSED(att);
	sys_dlist_init(&cv->wait_q);
	return 0;
}

/**
 * @brief POSIX threading compatibility API
 *
 * See IEEE 1003.1
 */
static inline int pthread_cond_destroy(pthread_cond_t *cv)
{
	return 0;
}

/**
 * @brief POSIX threading compatibility API
 *
 * See IEEE 1003.1
 */
int pthread_cond_signal(pthread_cond_t *cv);

/**
 * @brief POSIX threading compatibility API
 *
 * See IEEE 1003.1
 */
int pthread_cond_broadcast(pthread_cond_t *cv);

/**
 * @brief POSIX threading compatibility API
 *
 * See IEEE 1003.1
 */
int pthread_cond_wait(pthread_cond_t *cv, pthread_mutex_t *mut);

/**
 * @brief POSIX threading compatibility API
 *
 * See IEEE 1003.1
 */
int pthread_cond_timedwait(pthread_cond_t *cv, pthread_mutex_t *mut,
			   const struct timespec *to);

/**
 * @brief POSIX threading compatibility API
 *
 * See IEEE 1003.1.
 *
 * Note that pthread attribute structs are currently noops in Zephyr.
 */
static inline int pthread_condattr_init(pthread_condattr_t *att)
{
	return 0;
}

/**
 * @brief POSIX threading compatibility API
 *
 * See IEEE 1003.1
 *
 * Note that pthread attribute structs are currently noops in Zephyr.
 */
static inline int pthread_condattr_destroy(pthread_condattr_t *att)
{
	return 0;
}

/**
 * @brief Declare a pthread mutex
 *
 * Declaration API for a pthread mutex.  This is not a POSIX API, it's
 * provided to better conform with Zephyr's allocation strategies for
 * kernel objects.
 *
 * @param name Symbol name of the mutex
 */
#define PTHREAD_MUTEX_DEFINE(name)		\
	K_SEM_DEFINE(name##_psem, 1, 1);	\
	struct pthread_mutex name = {		\
		.sem = &name##_psem,		\
	}

/**
 * @brief POSIX threading compatibility API
 *
 * See IEEE 1003.1
 */
static inline int pthread_mutex_init(pthread_mutex_t *m,
				     const pthread_mutexattr_t *att)
{
	ARG_UNUSED(att);

	k_sem_init(m->sem, 1, 1);

	return 0;
}

/**
 * @brief POSIX threading compatibility API
 *
 * See IEEE 1003.1
 */
static inline int pthread_mutex_destroy(pthread_mutex_t *m)
{
	ARG_UNUSED(m);

	return 0;
}

/**
 * @brief POSIX threading compatibility API
 *
 * See IEEE 1003.1
 */
static inline int pthread_mutex_lock(pthread_mutex_t *m)
{
	return k_sem_take(m->sem, K_FOREVER);
}

/**
 * @brief POSIX threading compatibility API
 *
 * See IEEE 1003.1
 */
static inline int pthread_mutex_timedlock(pthread_mutex_t *m,
					  const struct timespec *to)
{
	int ret = k_sem_take(m->sem, _ts_to_ms(to));

	return ret == 0 ? ret : ETIMEDOUT;
}

/**
 * @brief POSIX threading compatibility API
 *
 * See IEEE 1003.1
 */
int pthread_mutex_trylock(pthread_mutex_t *m);

/**
 * @brief POSIX threading compatibility API
 *
 * See IEEE 1003.1
 */
static inline int pthread_mutex_unlock(pthread_mutex_t *m)
{
	k_sem_give(m->sem);
	return 0;
}

/**
 * @brief POSIX threading compatibility API
 *
 * See IEEE 1003.1
 *
 * Note that pthread attribute structs are currently noops in Zephyr.
 */
static inline int pthread_mutexattr_init(pthread_mutexattr_t *m)
{
	ARG_UNUSED(m);

	return 0;
}

/**
 * @brief POSIX threading compatibility API
 *
 * See IEEE 1003.1
 *
 * Note that pthread attribute structs are currently noops in Zephyr.
 */
static inline int pthread_mutexattr_destroy(pthread_mutexattr_t *m)
{
	ARG_UNUSED(m);

	return 0;
}

/* FIXME: these are going to be tricky to implement.  Zephyr has (for
 * good reason) deprecated its own "initializer" macros in favor of a
 * static "declaration" macros instead.  Using such a macro inside a
 * gcc compound expression to declare and object then reference it
 * would work, but gcc limits such expressions to function context
 * (because they may need to generate code that runs at assignment
 * time) and much real-world use of these initializers is for static
 * variables.  The best trick I can think of would be to declare it in
 * a special section and then initialize that section at runtime
 * startup, which sort of defeats the purpose of having these be
 * static...
 *
 * Instead, see the nonstandard PTHREAD_*_DEFINE macros instead, which
 * work similarly but conform to Zephyr's paradigms.
 */
/* #define PTHREAD_MUTEX_INITIALIZER */
/* #define PTHREAD_COND_INITIALIZER */

/**
 * @brief Declare a pthread barrier
 *
 * Declaration API for a pthread barrier.  This is not a
 * POSIX API, it's provided to better conform with Zephyr's allocation
 * strategies for kernel objects.
 *
 * @param name Symbol name of the barrier
 * @param count Thread count, same as the "count" argument to
 *             pthread_barrier_init()
 */
#define PTHREAD_BARRIER_DEFINE(name, count)			\
	struct pthread_barrier name = {				\
		.wait_q = SYS_DLIST_STATIC_INIT(&name.wait_q),	\
		.max = count,					\
	}

/**
 * @brief POSIX threading compatibility API
 *
 * See IEEE 1003.1
 */
int pthread_barrier_wait(pthread_barrier_t *b);

/**
 * @brief POSIX threading compatibility API
 *
 * See IEEE 1003.1
 */
static inline int pthread_barrier_init(pthread_barrier_t *b,
				       const pthread_barrierattr_t *attr,
				       unsigned int count)
{
	ARG_UNUSED(attr);

	b->max = count;
	b->count = 0;
	sys_dlist_init(&b->wait_q);

	return 0;
}

/**
 * @brief POSIX threading compatibility API
 *
 * See IEEE 1003.1
 */
static inline int pthread_barrier_destroy(pthread_barrier_t *b)
{
	ARG_UNUSED(b);

	return 0;
}

/**
 * @brief POSIX threading compatibility API
 *
 * See IEEE 1003.1
 *
 * Note that pthread attribute structs are currently noops in Zephyr.
 */
static inline int pthread_barrierattr_init(pthread_barrierattr_t *b)
{
	ARG_UNUSED(b);

	return 0;
}

/**
 * @brief POSIX threading compatibility API
 *
 * See IEEE 1003.1
 *
 * Note that pthread attribute structs are currently noops in Zephyr.
 */
static inline int pthread_barrierattr_destroy(pthread_barrierattr_t *b)
{
	ARG_UNUSED(b);

	return 0;
}

/* Predicates and setters for various pthread attribute values that we
 * don't support (or always support: the "process shared" attribute
 * can only be true given the way Zephyr implements these
 * objects). Leave these undefined for simplicity instead of defining
 * stubs to return an error that would have to be logged and
 * interpreted just to figure out that we didn't support it in the
 * first place. These APIs are very rarely used even in production
 * Unix code.  Leave the declarations here so they can be easily
 * uncommented and implemented as needed.

int pthread_condattr_getclock(const pthread_condattr_t * clockid_t *);
int pthread_condattr_getpshared(const pthread_condattr_t * int *);
int pthread_condattr_setclock(pthread_condattr_t *, clockid_t);
int pthread_condattr_setpshared(pthread_condattr_t *, int);
int pthread_mutex_consistent(pthread_mutex_t *);
int pthread_mutex_getprioceiling(const pthread_mutex_t * int *);
int pthread_mutex_setprioceiling(pthread_mutex_t *, int int *);
int pthread_mutexattr_getprioceiling(const pthread_mutexattr_t *, int *);
int pthread_mutexattr_getprotocol(const pthread_mutexattr_t * int *);
int pthread_mutexattr_getpshared(const pthread_mutexattr_t * int *);
int pthread_mutexattr_getrobust(const pthread_mutexattr_t * int *);
int pthread_mutexattr_gettype(const pthread_mutexattr_t * int *);
int pthread_mutexattr_setprioceiling(pthread_mutexattr_t *, int);
int pthread_mutexattr_setprotocol(pthread_mutexattr_t *, int);
int pthread_mutexattr_setpshared(pthread_mutexattr_t *, int);
int pthread_mutexattr_setrobust(pthread_mutexattr_t *, int);
int pthread_mutexattr_settype(pthread_mutexattr_t *, int);
int pthread_barrierattr_getpshared(const pthread_barrierattr_t *, int *);
int pthread_barrierattr_setpshared(pthread_barrierattr_t *, int);
*/

/* Base Pthread related APIs */

/**
 * @brief Obtain ID of the calling thread.
 *
 * The results of calling this API from threads not created with
 * pthread_create() are undefined.
 *
 * See IEEE 1003.1
 */
static inline pthread_t pthread_self(void)
{
	return (pthread_t)k_current_get();
}


/**
 * @brief Compare thread IDs.
 *
 * See IEEE 1003.1
 */
static inline int pthread_equal(pthread_t pt1, pthread_t pt2)
{
	return (pt1 == pt2);
}

/**
 * @brief Destroy the read-write lock attributes object.
 *
 * See IEEE 1003.1
 */
static inline int pthread_rwlockattr_destroy(pthread_rwlockattr_t *attr)
{
	return 0;
}

/**
 * @brief initialize the read-write lock attributes object.
 *
 * See IEEE 1003.1
 */
static inline int pthread_rwlockattr_init(pthread_rwlockattr_t *attr)
{
	return 0;
}

int pthread_attr_getstacksize(const pthread_attr_t *attr, size_t *stacksize);
int pthread_attr_setschedpolicy(pthread_attr_t *attr, int policy);
int pthread_attr_getschedpolicy(const pthread_attr_t *attr, int *policy);
int pthread_attr_setdetachstate(pthread_attr_t *attr, int detachstate);
int pthread_attr_getdetachstate(const pthread_attr_t *attr, int *detachstate);
int pthread_attr_init(pthread_attr_t *attr);
int pthread_attr_destroy(pthread_attr_t *attr);
int pthread_attr_getschedparam(const pthread_attr_t *attr,
			       struct sched_param *schedparam);
int pthread_getschedparam(pthread_t pthread, int *policy,
			  struct sched_param *param);
int pthread_attr_getstack(const pthread_attr_t *attr,
			  void **stackaddr, size_t *stacksize);
int pthread_attr_setstack(pthread_attr_t *attr, void *stackaddr,
			  size_t stacksize);
void pthread_exit(void *retval);
int pthread_join(pthread_t thread, void **status);
int pthread_cancel(pthread_t pthread);
int pthread_detach(pthread_t thread);
int pthread_create(pthread_t *newthread, const pthread_attr_t *attr,
		   void *(*threadroutine)(void *), void *arg);
int pthread_setcancelstate(int state, int *oldstate);
int pthread_attr_setschedparam(pthread_attr_t *attr,
			       const struct sched_param *schedparam);
int pthread_setschedparam(pthread_t pthread, int policy,
			  const struct sched_param *param);
int pthread_rwlock_destroy(pthread_rwlock_t *rwlock);
int pthread_rwlock_init(pthread_rwlock_t *rwlock,
			const pthread_rwlockattr_t *attr);
int pthread_rwlock_rdlock(pthread_rwlock_t *rwlock);
int pthread_rwlock_timedrdlock(pthread_rwlock_t *rwlock,
			       const struct timespec *abstime);
int pthread_rwlock_timedwrlock(pthread_rwlock_t *rwlock,
			       const struct timespec *abstime);
int pthread_rwlock_tryrdlock(pthread_rwlock_t *rwlock);
int pthread_rwlock_trywrlock(pthread_rwlock_t *rwlock);
int pthread_rwlock_unlock(pthread_rwlock_t *rwlock);
int pthread_rwlock_wrlock(pthread_rwlock_t *rwlock);

#endif /* __PTHREAD_H__ */
