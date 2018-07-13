/*
 * Copyright (c) 2016 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _kernel_structs__h_
#define _kernel_structs__h_

#include <kernel.h>

#if !defined(_ASMLANGUAGE)
#include <atomic.h>
#include <misc/dlist.h>
#include <misc/rb.h>
#include <string.h>
#endif

#define K_NUM_PRIORITIES \
	(CONFIG_NUM_COOP_PRIORITIES + CONFIG_NUM_PREEMPT_PRIORITIES + 1)

#define K_NUM_PRIO_BITMAPS ((K_NUM_PRIORITIES + 31) >> 5)

/*
 * Bitmask definitions for the struct k_thread.thread_state field.
 *
 * Must be before kerneL_arch_data.h because it might need them to be already
 * defined.
 */

/* states: common uses low bits, arch-specific use high bits */

/* Not a real thread */
#define _THREAD_DUMMY (1 << 0)

/* Thread is waiting on an object */
#define _THREAD_PENDING (1 << 1)

/* Thread has not yet started */
#define _THREAD_PRESTART (1 << 2)

/* Thread has terminated */
#define _THREAD_DEAD (1 << 3)

/* Thread is suspended */
#define _THREAD_SUSPENDED (1 << 4)

/* Thread is present in the ready queue */
#define _THREAD_QUEUED (1 << 6)

/* end - states */

#ifdef CONFIG_STACK_SENTINEL
/* Magic value in lowest bytes of the stack */
#define STACK_SENTINEL 0xF0F0F0F0
#endif

/* lowest value of _thread_base.preempt at which a thread is non-preemptible */
#define _NON_PREEMPT_THRESHOLD 0x0080

/* highest value of _thread_base.preempt at which a thread is preemptible */
#define _PREEMPT_THRESHOLD (_NON_PREEMPT_THRESHOLD - 1)

#include <kernel_arch_data.h>

#if !defined(_ASMLANGUAGE)

struct _ready_q {
#ifndef CONFIG_SMP
	/* always contains next thread to run: cannot be NULL */
	struct k_thread *cache;
#endif

#if defined(CONFIG_SCHED_DUMB)
	sys_dlist_t runq;
#elif defined(CONFIG_SCHED_SCALABLE)
	struct _priq_rb runq;
#elif defined(CONFIG_SCHED_MULTIQ)
	struct _priq_mq runq;
#endif
};

typedef struct _ready_q _ready_q_t;

struct _cpu {
	/* nested interrupt count */
	u32_t nested;

	/* interrupt stack pointer base */
	char *irq_stack;

	/* currently scheduled thread */
	struct k_thread *current;

	/* one assigned idle thread per CPU */
	struct k_thread *idle_thread;

	u8_t id;

#ifdef CONFIG_SMP
	/* True when _current is allowed to context switch */
	u8_t swap_ok;
#endif
};

typedef struct _cpu _cpu_t;

struct _kernel {
	/* For compatibility with pre-SMP code, union the first CPU
	 * record with the legacy fields so code can continue to use
	 * the "_kernel.XXX" expressions and assembly offsets.
	 */
	union {
		struct _cpu cpus[CONFIG_MP_NUM_CPUS];
#ifndef CONFIG_SMP
		struct {
			/* nested interrupt count */
			u32_t nested;

			/* interrupt stack pointer base */
			char *irq_stack;

			/* currently scheduled thread */
			struct k_thread *current;
		};
#endif
	};

#ifdef CONFIG_SYS_CLOCK_EXISTS
	/* queue of timeouts */
	sys_dlist_t timeout_q;
#endif

#ifdef CONFIG_SYS_POWER_MANAGEMENT
	s32_t idle; /* Number of ticks for kernel idling */
#endif

	/*
	 * ready queue: can be big, keep after small fields, since some
	 * assembly (e.g. ARC) are limited in the encoding of the offset
	 */
	struct _ready_q ready_q;

#ifdef CONFIG_FP_SHARING
	/*
	 * A 'current_sse' field does not exist in addition to the 'current_fp'
	 * field since it's not possible to divide the IA-32 non-integer
	 * registers into 2 distinct blocks owned by differing threads.  In
	 * other words, given that the 'fxnsave/fxrstor' instructions
	 * save/restore both the X87 FPU and XMM registers, it's not possible
	 * for a thread to only "own" the XMM registers.
	 */

	/* thread that owns the FP regs */
	struct k_thread *current_fp;
#endif

#if defined(CONFIG_THREAD_MONITOR)
	struct k_thread *threads; /* singly linked list of ALL threads */
#endif

	/* arch-specific part of _kernel */
	struct _kernel_arch arch;
};

typedef struct _kernel _kernel_t;

extern struct _kernel _kernel;

#ifdef CONFIG_SMP
#define _current_cpu (_arch_curr_cpu())
#define _current (_arch_curr_cpu()->current)
#else
#define _current_cpu (&_kernel.cpus[0])
#define _current _kernel.current
#endif

#define _ready_q _kernel.ready_q
#define _timeout_q _kernel.timeout_q
#define _threads _kernel.threads

#include <kernel_arch_func.h>

#if CONFIG_USE_SWITCH
/* This is a arch function traditionally, but when the switch-based
 * _Swap() is in use it's a simple inline provided by the kernel.
 */
static ALWAYS_INLINE void
_set_thread_return_value(struct k_thread *thread, unsigned int value)
{
	thread->swap_retval = value;
}
#endif

static ALWAYS_INLINE void
_set_thread_return_value_with_data(struct k_thread *thread,
				   unsigned int value,
				   void *data)
{
	_set_thread_return_value(thread, value);
	thread->base.swap_data = data;
}

extern void _init_thread_base(struct _thread_base *thread_base,
			      int priority, u32_t initial_state,
			      unsigned int options);

static ALWAYS_INLINE void _new_thread_init(struct k_thread *thread,
					    char *pStack, size_t stackSize,
					    int prio, unsigned int options)
{
#if !defined(CONFIG_INIT_STACKS) && !defined(CONFIG_THREAD_STACK_INFO)
	ARG_UNUSED(pStack);
	ARG_UNUSED(stackSize);
#endif

#ifdef CONFIG_INIT_STACKS
	memset(pStack, 0xaa, stackSize);
#endif
#ifdef CONFIG_STACK_SENTINEL
	/* Put the stack sentinel at the lowest 4 bytes of the stack area.
	 * We periodically check that it's still present and kill the thread
	 * if it isn't.
	 */
	*((u32_t *)pStack) = STACK_SENTINEL;
#endif /* CONFIG_STACK_SENTINEL */
	/* Initialize various struct k_thread members */
	_init_thread_base(&thread->base, prio, _THREAD_PRESTART, options);

	/* static threads overwrite it afterwards with real value */
	thread->init_data = NULL;
	thread->fn_abort = NULL;

#ifdef CONFIG_THREAD_CUSTOM_DATA
	/* Initialize custom data field (value is opaque to kernel) */
	thread->custom_data = NULL;
#endif

#if defined(CONFIG_USERSPACE)
	thread->mem_domain_info.mem_domain = NULL;
#endif /* CONFIG_USERSPACE */

#if defined(CONFIG_THREAD_STACK_INFO)
	thread->stack_info.start = (u32_t)pStack;
	thread->stack_info.size = (u32_t)stackSize;
#endif /* CONFIG_THREAD_STACK_INFO */
}

#endif /* _ASMLANGUAGE */

#endif /* _kernel_structs__h_ */
