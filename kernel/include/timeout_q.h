/*
 * Copyright (c) 2015 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_KERNEL_INCLUDE_TIMEOUT_Q_H_
#define ZEPHYR_KERNEL_INCLUDE_TIMEOUT_Q_H_

/**
 * @file
 * @brief timeout queue for threads on kernel objects
 */

#include <kernel.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef CONFIG_SYS_CLOCK_EXISTS

static inline void _init_timeout(struct _timeout *t, _timeout_func_t fn)
{
	t->dticks = _INACTIVE;
}

void _add_timeout(struct _timeout *to, _timeout_func_t fn, s32_t ticks);

int _abort_timeout(struct _timeout *to);

static inline void _init_thread_timeout(struct _thread_base *thread_base)
{
	_init_timeout(&thread_base->timeout, NULL);
}

extern void z_thread_timeout(struct _timeout *to);

static inline void _add_thread_timeout(struct k_thread *th, s32_t ticks)
{
	_add_timeout(&th->base.timeout, z_thread_timeout, ticks);
}

static inline int _abort_thread_timeout(struct k_thread *thread)
{
	return _abort_timeout(&thread->base.timeout);
}

s32_t _get_next_timeout_expiry(void);

void z_set_timeout_expiry(s32_t ticks, bool idle);

s32_t z_timeout_remaining(struct _timeout *timeout);

#else

/* Stubs when !CONFIG_SYS_CLOCK_EXISTS */
#define _init_thread_timeout(t) do {} while (0)
#define _add_thread_timeout(th, to) do {} while (0 && (void *)to && (void *)th)
#define _abort_thread_timeout(t) (0)
#define _get_next_timeout_expiry() (K_FOREVER)
#define z_set_timeout_expiry(t, i) do {} while (0)

#endif

#ifdef __cplusplus
}
#endif

#endif /* ZEPHYR_KERNEL_INCLUDE_TIMEOUT_Q_H_ */
