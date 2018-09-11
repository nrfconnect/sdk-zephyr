/*
 * Copyright (c) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef _KERNEL_TRACE_H
#define _KERNEL_TRACE_H

#include <kernel.h>

/* Below IDs are used with systemview, not final to the zephyr tracing API */
#define SYS_TRACE_ID_OFFSET                  (32u)

#define SYS_TRACE_ID_MUTEX_INIT              (1u + SYS_TRACE_ID_OFFSET)
#define SYS_TRACE_ID_MUTEX_UNLOCK            (2u + SYS_TRACE_ID_OFFSET)
#define SYS_TRACE_ID_MUTEX_LOCK              (3u + SYS_TRACE_ID_OFFSET)
#define SYS_TRACE_ID_SEMA_INIT               (4u + SYS_TRACE_ID_OFFSET)
#define SYS_TRACE_ID_SEMA_GIVE               (5u + SYS_TRACE_ID_OFFSET)
#define SYS_TRACE_ID_SEMA_TAKE               (6u + SYS_TRACE_ID_OFFSET)

#if CONFIG_TRACING
void z_sys_trace_idle(void);
void z_sys_trace_isr_enter(void);
void z_sys_trace_isr_exit_to_scheduler(void);
void z_sys_trace_thread_switched_in(void);
#endif

#ifdef CONFIG_SEGGER_SYSTEMVIEW
#include "tracing_sysview.h"
#else

/**
 * @brief Called before a thread has been selected to run
 */
#define sys_trace_thread_switched_out()

/**
 * @brief Called after a thread has been selected to run
 */
#define sys_trace_thread_switched_in()

/**
 * @brief Called when setting priority of a thread
 */
#define sys_trace_thread_priority_set(thread)

/**
 * @brief Called when a thread is being created
 */
#define sys_trace_thread_create(thread)

/**
 * @brief Called when a thread is being aborted
 *
 */
#define sys_trace_thread_abort(thread)

/**
 * @brief Called when a thread is being suspended
 * @param thread Thread structure
 */
#define sys_trace_thread_suspend(thread)

/**
 * @brief Called when a thread is being resumed from suspension
 * @param thread Thread structure
 */
#define sys_trace_thread_resume(thread)

/**
 * @brief Called when a thread is ready to run
 * @param thread Thread structure
 */
#define sys_trace_thread_ready(thread)

/**
 * @brief Called when a thread is pending
 * @param thread Thread structure
 */
#define sys_trace_thread_pend(thread)

/**
 * @brief Provide information about specific thread
 * @param thread Thread structure
 */
#define sys_trace_thread_info(thread)

/**
 * @brief Called when entering an ISR
 */
#define sys_trace_isr_enter()

/**
 * @brief Called when exiting an ISR
 */
#define sys_trace_isr_exit()

/**
 * @brief Called when exiting an ISR and switching to scheduler
 */
#define sys_trace_isr_exit_to_scheduler()

/**
 * @brief Can be called with any id signifying a new call
 * @param id ID of the operation that was started
 */
#define sys_trace_void(id)

/**
 * @brief Can be called with any id signifying ending a call
 * @param id ID of the operation that was completed
 */
#define sys_trace_end_call(id)



#define z_sys_trace_idle()

#define z_sys_trace_isr_enter()

#define z_sys_trace_isr_exit_to_scheduler()

#define z_sys_trace_thread_switched_in()

#endif
#endif
