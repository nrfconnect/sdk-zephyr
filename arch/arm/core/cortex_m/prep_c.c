/*
 * Copyright (c) 2013-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Full C support initialization
 *
 *
 * Initialization of full C support: zero the .bss, copy the .data if XIP,
 * call z_cstart().
 *
 * Stack is available in this module, but not the global data/bss until their
 * initialization is performed.
 */

#include <kernel.h>
#include <zephyr/types.h>
#include <toolchain.h>
#include <linker/linker-defs.h>
#include <kernel_internal.h>
#include <arch/arm/cortex_m/cmsis.h>
#include <cortex_m/stack.h>

#if defined(__GNUC__)
/*
 * GCC can detect if memcpy is passed a NULL argument, however one of
 * the cases of relocate_vector_table() it is valid to pass NULL, so we
 * supress the warning for this case.  We need to do this before
 * string.h is included to get the declaration of memcpy.
 */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnonnull"
#endif

#include <string.h>

static inline void switch_sp_to_psp(void)
{
	__set_CONTROL(__get_CONTROL() | CONTROL_SPSEL_Msk);
	/*
	 * When changing the stack pointer, software must use an ISB instruction
	 * immediately after the MSR instruction. This ensures that instructions
	 * after the ISB instruction execute using the new stack pointer.
	 */
	__ISB();
}

static inline void set_and_switch_to_psp(void)
{
	u32_t process_sp;

	process_sp = (u32_t)&_interrupt_stack + CONFIG_ISR_STACK_SIZE;
	__set_PSP(process_sp);
	switch_sp_to_psp();
}

void lock_interrupts(void)
{
#if defined(CONFIG_ARMV6_M_ARMV8_M_BASELINE)
	__disable_irq();
#elif defined(CONFIG_ARMV7_M_ARMV8_M_MAINLINE)
	__set_BASEPRI(_EXC_IRQ_DEFAULT_PRIO);
#else
#error Unknown ARM architecture
#endif /* CONFIG_ARMV6_M_ARMV8_M_BASELINE */
}

#ifdef CONFIG_INIT_STACKS
static inline void init_stacks(void)
{
	memset(&_interrupt_stack, 0xAA, CONFIG_ISR_STACK_SIZE);
}
#endif

#ifdef CONFIG_CPU_CORTEX_M_HAS_VTOR

#ifdef CONFIG_XIP
#define VECTOR_ADDRESS ((uintptr_t)_vector_start)
#else
#define VECTOR_ADDRESS CONFIG_SRAM_BASE_ADDRESS
#endif
static inline void relocate_vector_table(void)
{
	SCB->VTOR = VECTOR_ADDRESS & SCB_VTOR_TBLOFF_Msk;
	__DSB();
	__ISB();
}

#else

#if defined(CONFIG_SW_VECTOR_RELAY)
Z_GENERIC_SECTION(.vt_pointer_section) void *_vector_table_pointer;
#endif

#define VECTOR_ADDRESS 0

void __weak relocate_vector_table(void)
{
#if defined(CONFIG_XIP) && (CONFIG_FLASH_BASE_ADDRESS != 0) || \
    !defined(CONFIG_XIP) && (CONFIG_SRAM_BASE_ADDRESS != 0)
	size_t vector_size = (size_t)_vector_end - (size_t)_vector_start;
	(void)memcpy(VECTOR_ADDRESS, _vector_start, vector_size);
#elif defined(CONFIG_SW_VECTOR_RELAY)
	_vector_table_pointer = _vector_start;
#endif
}

#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#endif /* CONFIG_CPU_CORTEX_M_HAS_VTOR */

#ifdef CONFIG_FLOAT
static inline void enable_floating_point(void)
{
	/*
	 * Upon reset, the Co-Processor Access Control Register is 0x00000000.
	 * Enable CP10 and CP11 Co-Processors to enable access to floating
	 * point registers.
	 */
#if defined(CONFIG_USERSPACE)
	/* Full access */
	SCB->CPACR |= CPACR_CP10_FULL_ACCESS | CPACR_CP11_FULL_ACCESS;
#else
	/* Privileged access only */
	SCB->CPACR |= CPACR_CP10_PRIV_ACCESS | CPACR_CP11_PRIV_ACCESS;
#endif /* CONFIG_USERSPACE */
	/*
	 * Upon reset, the FPU Context Control Register is 0xC0000000
	 * (both Automatic and Lazy state preservation is enabled).
	 */
#if !defined(CONFIG_FP_SHARING)
	/* Default mode is Unshared FP registers mode. We disable the
	 * automatic stacking of FP registers (automatic setting of
	 * FPCA bit in the CONTROL register), upon exception entries,
	 * as the FP registers are to be used by a single context (and
	 * the use of FP registers in ISRs is not supported). This
	 * configuration improves interrupt latency and decreases the
	 * stack memory requirement for the (single) thread that makes
	 * use of the FP co-processor.
	 */
	FPU->FPCCR &= (~(FPU_FPCCR_ASPEN_Msk | FPU_FPCCR_LSPEN_Msk));
#else
	/*
	 * Disable lazy state preservation so the volatile FP registers are
	 * always saved on exception.
	 */
	FPU->FPCCR = FPU_FPCCR_ASPEN_Msk; /* FPU_FPCCR_LSPEN = 0 */
#endif /* CONFIG_FP_SHARING */

	/* Make the side-effects of modifying the FPCCR be realized
	 * immediately.
	 */
	__DSB();
	__ISB();

	/* Initialize the Floating Point Status and Control Register. */
	__set_FPSCR(0);

	/*
	 * Note:
	 * The use of the FP register bank is enabled, however the FP context
	 * will be activated (FPCA bit on the CONTROL register) in the presence
	 * of floating point instructions.
	 */
}
#else
static inline void enable_floating_point(void)
{
}
#endif

extern FUNC_NORETURN void z_cstart(void);
/**
 *
 * @brief Prepare to and run C code
 *
 * This routine prepares for the execution of and runs C code.
 *
 * @return N/A
 */

extern void z_IntLibInit(void);

#ifdef CONFIG_BOOT_TIME_MEASUREMENT
	extern u64_t __start_time_stamp;
#endif
void _PrepC(void)
{
#ifdef CONFIG_INIT_STACKS
	init_stacks();
#endif
	/*
	 * Set PSP and use it to boot without using MSP, so that it
	 * gets set to _interrupt_stack during initialization.
	 */
	set_and_switch_to_psp();
	relocate_vector_table();
	enable_floating_point();
	z_bss_zero();
	z_data_copy();
#ifdef CONFIG_BOOT_TIME_MEASUREMENT
	__start_time_stamp = 0U;
#endif
	z_IntLibInit();
	z_cstart();
	CODE_UNREACHABLE;
}
