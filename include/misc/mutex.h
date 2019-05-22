/*
 * Copyright (c) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_INCLUDE_MISC_MUTEX_H_
#define ZEPHYR_INCLUDE_MISC_MUTEX_H_

/*
 * sys_mutex behaves almost exactly like k_mutex, with the added advantage
 * that a sys_mutex instance can reside in user memory.
 *
 * Further enhancements will support locking/unlocking uncontended sys_mutexes
 * with simple atomic ops instead of syscalls, similar to Linux's
 * FUTEX_LOCK_PI and FUTEX_UNLOCK_PI
 */

#ifdef CONFIG_USERSPACE
#include <atomic.h>
#include <zephyr/types.h>

struct sys_mutex {
	/* Currently unused, but will be used to store state for fast mutexes
	 * that can be locked/unlocked with atomic ops if there is no
	 * contention
	 */
	atomic_t val;
};

#define SYS_MUTEX_DEFINE(name) \
	struct sys_mutex name

/**
 * @brief Initialize a mutex.
 *
 * This routine initializes a mutex object, prior to its first use.
 *
 * Upon completion, the mutex is available and does not have an owner.
 *
 * This routine is only necessary to call when userspace is disabled
 * and the mutex was not created with SYS_MUTEX_DEFINE().
 *
 * @param mutex Address of the mutex.
 *
 * @return N/A
 */
static inline void sys_mutex_init(struct sys_mutex *mutex)
{
	ARG_UNUSED(mutex);

	/* Nothing to do, kernel-side data structures are initialized at
	 * boot
	 */
}

__syscall int z_sys_mutex_kernel_lock(struct sys_mutex *mutex, s32_t timeout);

__syscall int z_sys_mutex_kernel_unlock(struct sys_mutex *mutex);

/**
 * @brief Lock a mutex.
 *
 * This routine locks @a mutex. If the mutex is locked by another thread,
 * the calling thread waits until the mutex becomes available or until
 * a timeout occurs.
 *
 * A thread is permitted to lock a mutex it has already locked. The operation
 * completes immediately and the lock count is increased by 1.
 *
 * @param mutex Address of the mutex, which may reside in user memory
 * @param timeout Waiting period to lock the mutex (in milliseconds),
 *                or one of the special values K_NO_WAIT and K_FOREVER.
 *
 * @retval 0 Mutex locked.
 * @retval -EBUSY Returned without waiting.
 * @retval -EAGAIN Waiting period timed out.
 * @retval -EACCESS Caller has no access to provided mutex address
 * @retval -EINVAL Provided mutex not recognized by the kernel
 */
static inline int sys_mutex_lock(struct sys_mutex *mutex, s32_t timeout)
{
	/* For now, make the syscall unconditionally */
	return z_sys_mutex_kernel_lock(mutex, timeout);
}

/**
 * @brief Unlock a mutex.
 *
 * This routine unlocks @a mutex. The mutex must already be locked by the
 * calling thread.
 *
 * The mutex cannot be claimed by another thread until it has been unlocked by
 * the calling thread as many times as it was previously locked by that
 * thread.
 *
 * @param mutex Address of the mutex, which may reside in user memory
 * @retval -EACCESS Caller has no access to provided mutex address
 * @retval -EINVAL Provided mutex not recognized by the kernel or mutex wasn't
 *                 locked
 * @retval -EPERM Caller does not own the mutex
 */
static inline int sys_mutex_unlock(struct sys_mutex *mutex)
{
	/* For now, make the syscall unconditionally */
	return z_sys_mutex_kernel_unlock(mutex);
}

#include <syscalls/mutex.h>

#else
#include <kernel.h>
#include <kernel_structs.h>

struct sys_mutex {
	struct k_mutex kernel_mutex;
};

#define SYS_MUTEX_DEFINE(name) \
	struct sys_mutex name = { \
		.kernel_mutex = _K_MUTEX_INITIALIZER(name.kernel_mutex) \
	}

static inline void sys_mutex_init(struct sys_mutex *mutex)
{
	k_mutex_init(&mutex->kernel_mutex);
}

static inline int sys_mutex_lock(struct sys_mutex *mutex, s32_t timeout)
{
	return k_mutex_lock(&mutex->kernel_mutex, timeout);
}

static inline int sys_mutex_unlock(struct sys_mutex *mutex)
{
	if (mutex->kernel_mutex.lock_count == 0) {
		return -EINVAL;
	}

	if (mutex->kernel_mutex.owner != _current) {
		return -EPERM;
	}

	k_mutex_unlock(&mutex->kernel_mutex);
	return 0;
}

#endif /* CONFIG_USERSPACE */
#endif /* ZEPHYR_INCLUDE_MISC_MUTEX_H_ */
