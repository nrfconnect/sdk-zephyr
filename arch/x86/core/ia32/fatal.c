/*
 * Copyright (c) 2013-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Kernel fatal error handler
 */

#include <kernel.h>
#include <kernel_internal.h>
#include <drivers/interrupt_controller/sysapic.h>
#include <arch/x86/ia32/segmentation.h>
#include <arch/syscall.h>
#include <ia32/exception.h>
#include <inttypes.h>
#include <exc_handle.h>
#include <logging/log.h>
LOG_MODULE_DECLARE(os);

__weak void z_debug_fatal_hook(const z_arch_esf_t *esf) { ARG_UNUSED(esf); }

void z_x86_spurious_irq(const z_arch_esf_t *esf)
{
	int vector = z_irq_controller_isr_vector_get();

	if (vector >= 0) {
		LOG_ERR("IRQ vector: %d", vector);
	}

	z_x86_fatal_error(K_ERR_SPURIOUS_IRQ, esf);
}

void arch_syscall_oops(void *ssf_ptr)
{
	struct _x86_syscall_stack_frame *ssf =
		(struct _x86_syscall_stack_frame *)ssf_ptr;
	z_arch_esf_t oops = {
		.eip = ssf->eip,
		.cs = ssf->cs,
		.eflags = ssf->eflags
	};

	if (oops.cs == USER_CODE_SEG) {
		oops.esp = ssf->esp;
	}

	z_x86_fatal_error(K_ERR_KERNEL_OOPS, &oops);
}

extern void (*_kernel_oops_handler)(void);
NANO_CPU_INT_REGISTER(_kernel_oops_handler, NANO_SOFT_IRQ,
		      Z_X86_OOPS_VECTOR / 16, Z_X86_OOPS_VECTOR, 3);

#if CONFIG_EXCEPTION_DEBUG
FUNC_NORETURN static void generic_exc_handle(unsigned int vector,
					     const z_arch_esf_t *pEsf)
{
	z_x86_unhandled_cpu_exception(vector, pEsf);
}

#define _EXC_FUNC(vector) \
FUNC_NORETURN void handle_exc_##vector(const z_arch_esf_t *pEsf) \
{ \
	generic_exc_handle(vector, pEsf); \
}

#define Z_EXC_FUNC_CODE(vector) \
	_EXC_FUNC(vector) \
	_EXCEPTION_CONNECT_CODE(handle_exc_##vector, vector)

#define Z_EXC_FUNC_NOCODE(vector) \
	_EXC_FUNC(vector) \
	_EXCEPTION_CONNECT_NOCODE(handle_exc_##vector, vector)

/* Necessary indirection to ensure 'vector' is expanded before we expand
 * the handle_exc_##vector
 */
#define EXC_FUNC_NOCODE(vector) \
	Z_EXC_FUNC_NOCODE(vector)

#define EXC_FUNC_CODE(vector) \
	Z_EXC_FUNC_CODE(vector)

EXC_FUNC_NOCODE(IV_DIVIDE_ERROR);
EXC_FUNC_NOCODE(IV_NON_MASKABLE_INTERRUPT);
EXC_FUNC_NOCODE(IV_OVERFLOW);
EXC_FUNC_NOCODE(IV_BOUND_RANGE);
EXC_FUNC_NOCODE(IV_INVALID_OPCODE);
EXC_FUNC_NOCODE(IV_DEVICE_NOT_AVAILABLE);
#ifndef CONFIG_X86_ENABLE_TSS
EXC_FUNC_NOCODE(IV_DOUBLE_FAULT);
#endif
EXC_FUNC_CODE(IV_INVALID_TSS);
EXC_FUNC_CODE(IV_SEGMENT_NOT_PRESENT);
EXC_FUNC_CODE(IV_STACK_FAULT);
EXC_FUNC_CODE(IV_GENERAL_PROTECTION);
EXC_FUNC_NOCODE(IV_X87_FPU_FP_ERROR);
EXC_FUNC_CODE(IV_ALIGNMENT_CHECK);
EXC_FUNC_NOCODE(IV_MACHINE_CHECK);
#endif

_EXCEPTION_CONNECT_CODE(z_x86_page_fault_handler, IV_PAGE_FAULT);

#ifdef CONFIG_X86_ENABLE_TSS
static __noinit volatile z_arch_esf_t _df_esf;

/* Very tiny stack; just enough for the bogus error code pushed by the CPU
 * and a frame pointer push by the compiler. All df_handler_top does is
 * shuffle some data around with 'mov' statements and then 'iret'.
 */
static __noinit char _df_stack[8];

static FUNC_NORETURN __used void df_handler_top(void);

#ifdef CONFIG_X86_KPTI
extern char z_trampoline_stack_end[];
#endif

Z_GENERIC_SECTION(.tss)
struct task_state_segment _main_tss = {
	.ss0 = DATA_SEG,
#ifdef CONFIG_X86_KPTI
	/* Stack to land on when we get a soft/hard IRQ in user mode.
	 * In a special kernel page that, unlike all other kernel pages,
	 * is marked present in the user page table.
	 */
	.esp0 = (u32_t)&z_trampoline_stack_end
#endif
};

/* Special TSS for handling double-faults with a known good stack */
Z_GENERIC_SECTION(.tss)
struct task_state_segment _df_tss = {
	.esp = (u32_t)(_df_stack + sizeof(_df_stack)),
	.cs = CODE_SEG,
	.ds = DATA_SEG,
	.es = DATA_SEG,
	.ss = DATA_SEG,
	.eip = (u32_t)df_handler_top,
	.cr3 = (u32_t)&z_x86_kernel_ptables
};

static __used void df_handler_bottom(void)
{
	/* We're back in the main hardware task on the interrupt stack */
	int reason = K_ERR_CPU_EXCEPTION;

	/* Restore the top half so it is runnable again */
	_df_tss.esp = (u32_t)(_df_stack + sizeof(_df_stack));
	_df_tss.eip = (u32_t)df_handler_top;

	LOG_ERR("Double Fault");
#ifdef CONFIG_THREAD_STACK_INFO
	if (z_x86_check_stack_bounds(_df_esf.esp, 0, _df_esf.cs)) {
		reason = K_ERR_STACK_CHK_FAIL;
	}
#endif
	z_x86_fatal_error(reason, (z_arch_esf_t *)&_df_esf);
}

static FUNC_NORETURN __used void df_handler_top(void)
{
	/* State of the system when the double-fault forced a task switch
	 * will be in _main_tss. Set up a z_arch_esf_t and copy system state into
	 * it
	 */
	_df_esf.esp = _main_tss.esp;
	_df_esf.ebp = _main_tss.ebp;
	_df_esf.ebx = _main_tss.ebx;
	_df_esf.esi = _main_tss.esi;
	_df_esf.edi = _main_tss.edi;
	_df_esf.edx = _main_tss.edx;
	_df_esf.eax = _main_tss.eax;
	_df_esf.ecx = _main_tss.ecx;
	_df_esf.errorCode = 0;
	_df_esf.eip = _main_tss.eip;
	_df_esf.cs = _main_tss.cs;
	_df_esf.eflags = _main_tss.eflags;

	/* Restore the main IA task to a runnable state */
	_main_tss.esp = (u32_t)(ARCH_THREAD_STACK_BUFFER(_interrupt_stack) +
				CONFIG_ISR_STACK_SIZE);
	_main_tss.cs = CODE_SEG;
	_main_tss.ds = DATA_SEG;
	_main_tss.es = DATA_SEG;
	_main_tss.ss = DATA_SEG;
	_main_tss.eip = (u32_t)df_handler_bottom;
	_main_tss.cr3 = (u32_t)&z_x86_kernel_ptables;
	_main_tss.eflags = 0U;

	/* NT bit is set in EFLAGS so we will task switch back to _main_tss
	 * and run df_handler_bottom
	 */
	__asm__ volatile ("iret");
	CODE_UNREACHABLE;
}

/* Configure a task gate descriptor in the IDT for the double fault
 * exception
 */
_X86_IDT_TSS_REGISTER(DF_TSS, -1, -1, IV_DOUBLE_FAULT, 0);

#endif /* CONFIG_X86_ENABLE_TSS */
