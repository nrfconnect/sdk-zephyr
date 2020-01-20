/*
 * Copyright (c) 2010-2015 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Thread support primitives
 *
 * This module provides core thread related primitives for the IA-32
 * processor architecture.
 */

#include <kernel.h>
#include <ksched.h>
#include <arch/x86/mmustructs.h>

/* forward declaration */

/* Initial thread stack frame, such that everything is laid out as expected
 * for when z_swap() switches to it for the first time.
 */
struct _x86_initial_frame {
	u32_t swap_retval;
	u32_t ebp;
	u32_t ebx;
	u32_t esi;
	u32_t edi;
	void *thread_entry;
	u32_t eflags;
	k_thread_entry_t entry;
	void *p1;
	void *p2;
	void *p3;
};

#ifdef CONFIG_X86_USERSPACE
/* Implemented in userspace.S */
extern void z_x86_syscall_entry_stub(void);

/* Syscalls invoked by 'int 0x80'. Installed in the IDT at DPL=3 so that
 * userspace can invoke it.
 */
NANO_CPU_INT_REGISTER(z_x86_syscall_entry_stub, -1, -1, 0x80, 3);
#endif /* CONFIG_X86_USERSPACE */

#if defined(CONFIG_FLOAT) && defined(CONFIG_FP_SHARING)

extern int z_float_disable(struct k_thread *thread);

int arch_float_disable(struct k_thread *thread)
{
#if defined(CONFIG_LAZY_FP_SHARING)
	return z_float_disable(thread);
#else
	return -ENOSYS;
#endif /* CONFIG_LAZY_FP_SHARING */
}
#endif /* CONFIG_FLOAT && CONFIG_FP_SHARING */

void arch_new_thread(struct k_thread *thread, k_thread_stack_t *stack,
		     size_t stack_size, k_thread_entry_t entry,
		     void *parameter1, void *parameter2, void *parameter3,
		     int priority, unsigned int options)
{
	char *stack_buf;
	char *stack_high;
	void *swap_entry;
	struct _x86_initial_frame *initial_frame;

	Z_ASSERT_VALID_PRIO(priority, entry);
	stack_buf = Z_THREAD_STACK_BUFFER(stack);
	z_new_thread_init(thread, stack_buf, stack_size, priority, options);

#if CONFIG_X86_STACK_PROTECTION
	struct z_x86_thread_stack_header *header =
		(struct z_x86_thread_stack_header *)stack;

	/* Set guard area to read-only to catch stack overflows */
	z_x86_mmu_set_flags(&z_x86_kernel_ptables, &header->guard_page,
			    MMU_PAGE_SIZE, MMU_ENTRY_READ, Z_X86_MMU_RW,
			    true);
#endif

#ifdef CONFIG_USERSPACE
	swap_entry = z_x86_userspace_prepare_thread(thread);
#else
	swap_entry = z_thread_entry;
#endif

	stack_high = (char *)STACK_ROUND_DOWN(stack_buf + stack_size);

	/* Create an initial context on the stack expected by z_swap() */
	initial_frame = (struct _x86_initial_frame *)
		(stack_high - sizeof(struct _x86_initial_frame));
	/* z_thread_entry() arguments */
	initial_frame->entry = entry;
	initial_frame->p1 = parameter1;
	initial_frame->p2 = parameter2;
	initial_frame->p3 = parameter3;
	initial_frame->eflags = EFLAGS_INITIAL;
#ifdef _THREAD_WRAPPER_REQUIRED
	initial_frame->edi = (u32_t)swap_entry;
	initial_frame->thread_entry = z_x86_thread_entry_wrapper;
#else
	initial_frame->thread_entry = swap_entry;
#endif /* _THREAD_WRAPPER_REQUIRED */

	/* Remaining _x86_initial_frame members can be garbage, z_thread_entry()
	 * doesn't care about their state when execution begins
	 */
	thread->callee_saved.esp = (unsigned long)initial_frame;
#if defined(CONFIG_LAZY_FP_SHARING)
	thread->arch.excNestCount = 0;
#endif /* CONFIG_LAZY_FP_SHARING */
	thread->arch.flags = 0;
}
