/*
 * Copyright (c) 2018 Intel Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Public interface for spinlocks
 */

#ifndef ZEPHYR_INCLUDE_SPINLOCK_H_
#define ZEPHYR_INCLUDE_SPINLOCK_H_

#include <zephyr/sys/atomic.h>
#include <zephyr/sys/__assert.h>
#include <zephyr/sys/time_units.h>
#include <stdbool.h>
#include <zephyr/arch/cpu.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Spinlock APIs
 * @defgroup spinlock_apis Spinlock APIs
 * @ingroup kernel_apis
 * @{
 */

struct z_spinlock_key {
	int key;
};

/**
 * @brief Kernel Spin Lock
 *
 * This struct defines a spin lock record on which CPUs can wait with
 * k_spin_lock().  Any number of spinlocks may be defined in
 * application code.
 */
struct k_spinlock {
#ifdef CONFIG_SMP
	atomic_t locked;
#endif

#ifdef CONFIG_SPIN_VALIDATE
	/* Stores the thread that holds the lock with the locking CPU
	 * ID in the bottom two bits.
	 */
	uintptr_t thread_cpu;
#ifdef CONFIG_SPIN_LOCK_TIME_LIMIT
	/* Stores the time (in cycles) when a lock was taken
	 */
	uint32_t lock_time;
#endif /* CONFIG_SPIN_LOCK_TIME_LIMIT */
#endif /* CONFIG_SPIN_VALIDATE */

#if defined(CONFIG_CPP) && !defined(CONFIG_SMP) && \
	!defined(CONFIG_SPIN_VALIDATE)
	/* If CONFIG_SMP and CONFIG_SPIN_VALIDATE are both not defined
	 * the k_spinlock struct will have no members. The result
	 * is that in C sizeof(k_spinlock) is 0 and in C++ it is 1.
	 *
	 * This size difference causes problems when the k_spinlock
	 * is embedded into another struct like k_msgq, because C and
	 * C++ will have different ideas on the offsets of the members
	 * that come after the k_spinlock member.
	 *
	 * To prevent this we add a 1 byte dummy member to k_spinlock
	 * when the user selects C++ support and k_spinlock would
	 * otherwise be empty.
	 */
	char dummy;
#endif
};

/* There's a spinlock validation framework available when asserts are
 * enabled.  It adds a relatively hefty overhead (about 3k or so) to
 * kernel code size, don't use on platforms known to be small.
 */
#ifdef CONFIG_SPIN_VALIDATE
bool z_spin_lock_valid(struct k_spinlock *l);
bool z_spin_unlock_valid(struct k_spinlock *l);
void z_spin_lock_set_owner(struct k_spinlock *l);
BUILD_ASSERT(CONFIG_MP_MAX_NUM_CPUS <= 4, "Too many CPUs for mask");

# ifdef CONFIG_KERNEL_COHERENCE
bool z_spin_lock_mem_coherent(struct k_spinlock *l);
# endif /* CONFIG_KERNEL_COHERENCE */

#endif /* CONFIG_SPIN_VALIDATE */

/**
 * @brief Spinlock key type
 *
 * This type defines a "key" value used by a spinlock implementation
 * to store the system interrupt state at the time of a call to
 * k_spin_lock().  It is expected to be passed to a matching
 * k_spin_unlock().
 *
 * This type is opaque and should not be inspected by application
 * code.
 */
typedef struct z_spinlock_key k_spinlock_key_t;

/**
 * @brief Lock a spinlock
 *
 * This routine locks the specified spinlock, returning a key handle
 * representing interrupt state needed at unlock time.  Upon
 * returning, the calling thread is guaranteed not to be suspended or
 * interrupted on its current CPU until it calls k_spin_unlock().  The
 * implementation guarantees mutual exclusion: exactly one thread on
 * one CPU will return from k_spin_lock() at a time.  Other CPUs
 * trying to acquire a lock already held by another CPU will enter an
 * implementation-defined busy loop ("spinning") until the lock is
 * released.
 *
 * Separate spin locks may be nested. It is legal to lock an
 * (unlocked) spin lock while holding a different lock.  Spin locks
 * are not recursive, however: an attempt to acquire a spin lock that
 * the CPU already holds will deadlock.
 *
 * In circumstances where only one CPU exists, the behavior of
 * k_spin_lock() remains as specified above, though obviously no
 * spinning will take place.  Implementations may be free to optimize
 * in uniprocessor contexts such that the locking reduces to an
 * interrupt mask operation.
 *
 * @param l A pointer to the spinlock to lock
 * @return A key value that must be passed to k_spin_unlock() when the
 *         lock is released.
 */
static ALWAYS_INLINE k_spinlock_key_t k_spin_lock(struct k_spinlock *l)
{
	ARG_UNUSED(l);
	k_spinlock_key_t k;

	/* Note that we need to use the underlying arch-specific lock
	 * implementation.  The "irq_lock()" API in SMP context is
	 * actually a wrapper for a global spinlock!
	 */
	k.key = arch_irq_lock();

#ifdef CONFIG_SPIN_VALIDATE
	__ASSERT(z_spin_lock_valid(l), "Recursive spinlock %p", l);
# ifdef CONFIG_KERNEL_COHERENCE
	__ASSERT_NO_MSG(z_spin_lock_mem_coherent(l));
# endif
#endif

#ifdef CONFIG_SMP
	while (!atomic_cas(&l->locked, 0, 1)) {
		arch_spin_relax();
	}
#endif

#ifdef CONFIG_SPIN_VALIDATE
	z_spin_lock_set_owner(l);
#if defined(CONFIG_SPIN_LOCK_TIME_LIMIT) && (CONFIG_SPIN_LOCK_TIME_LIMIT != 0)
	l->lock_time = sys_clock_cycle_get_32();
#endif /* CONFIG_SPIN_LOCK_TIME_LIMIT */
#endif/* CONFIG_SPIN_VALIDATE */
	return k;
}

/**
 * @brief Unlock a spin lock
 *
 * This releases a lock acquired by k_spin_lock().  After this
 * function is called, any CPU will be able to acquire the lock.  If
 * other CPUs are currently spinning inside k_spin_lock() waiting for
 * this lock, exactly one of them will return synchronously with the
 * lock held.
 *
 * Spin locks must be properly nested.  A call to k_spin_unlock() must
 * be made on the lock object most recently locked using
 * k_spin_lock(), using the key value that it returned.  Attempts to
 * unlock mis-nested locks, or to unlock locks that are not held, or
 * to passing a key parameter other than the one returned from
 * k_spin_lock(), are illegal.  When CONFIG_SPIN_VALIDATE is set, some
 * of these errors can be detected by the framework.
 *
 * @param l A pointer to the spinlock to release
 * @param key The value returned from k_spin_lock() when this lock was
 *        acquired
 */
static ALWAYS_INLINE void k_spin_unlock(struct k_spinlock *l,
					k_spinlock_key_t key)
{
	ARG_UNUSED(l);
#ifdef CONFIG_SPIN_VALIDATE
	__ASSERT(z_spin_unlock_valid(l), "Not my spinlock %p", l);

#if defined(CONFIG_SPIN_LOCK_TIME_LIMIT) && (CONFIG_SPIN_LOCK_TIME_LIMIT != 0)
	uint32_t delta = sys_clock_cycle_get_32() - l->lock_time;

	__ASSERT(delta < CONFIG_SPIN_LOCK_TIME_LIMIT,
		 "Spin lock %p held %u cycles, longer than limit of %u cycles",
		 l, delta, CONFIG_SPIN_LOCK_TIME_LIMIT);
#endif /* CONFIG_SPIN_LOCK_TIME_LIMIT */
#endif /* CONFIG_SPIN_VALIDATE */

#ifdef CONFIG_SMP
	/* Strictly we don't need atomic_clear() here (which is an
	 * exchange operation that returns the old value).  We are always
	 * setting a zero and (because we hold the lock) know the existing
	 * state won't change due to a race.  But some architectures need
	 * a memory barrier when used like this, and we don't have a
	 * Zephyr framework for that.
	 */
	atomic_clear(&l->locked);
#endif
	arch_irq_unlock(key.key);
}

/* Internal function: releases the lock, but leaves local interrupts
 * disabled
 */
static ALWAYS_INLINE void k_spin_release(struct k_spinlock *l)
{
	ARG_UNUSED(l);
#ifdef CONFIG_SPIN_VALIDATE
	__ASSERT(z_spin_unlock_valid(l), "Not my spinlock %p", l);
#endif
#ifdef CONFIG_SMP
	atomic_clear(&l->locked);
#endif
}

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* ZEPHYR_INCLUDE_SPINLOCK_H_ */
