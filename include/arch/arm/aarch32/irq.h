/*
 * Copyright (c) 2013-2014 Wind River Systems, Inc.
 * Copyright (c) 2019 Nordic Semiconductor ASA.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Cortex-M public interrupt handling
 *
 * ARM-specific kernel interrupt handling interface. Included by arm/arch.h.
 */

#ifndef ZEPHYR_INCLUDE_ARCH_ARM_AARCH32_IRQ_H_
#define ZEPHYR_INCLUDE_ARCH_ARM_AARCH32_IRQ_H_

#include <irq.h>
#include <sw_isr_table.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _ASMLANGUAGE
GTEXT(z_arm_int_exit);
GTEXT(arch_irq_enable)
GTEXT(arch_irq_disable)
GTEXT(arch_irq_is_enabled)
#else
extern void arch_irq_enable(unsigned int irq);
extern void arch_irq_disable(unsigned int irq);
extern int arch_irq_is_enabled(unsigned int irq);

extern void z_arm_int_exit(void);

#if defined(CONFIG_ARMV7_R)
static ALWAYS_INLINE void z_arm_int_lib_init(void)
{
}
#else
extern void z_arm_int_lib_init(void);
#endif

/* macros convert value of it's argument to a string */
#define DO_TOSTR(s) #s
#define TOSTR(s) DO_TOSTR(s)

/* concatenate the values of the arguments into one */
#define DO_CONCAT(x, y) x ## y
#define CONCAT(x, y) DO_CONCAT(x, y)

/* internal routine documented in C file, needed by IRQ_CONNECT() macro */
extern void z_arm_irq_priority_set(unsigned int irq, unsigned int prio,
				   u32_t flags);


/* Flags for use with IRQ_CONNECT() */
#ifdef CONFIG_ZERO_LATENCY_IRQS
/**
 * Set this interrupt up as a zero-latency IRQ. It has a fixed hardware
 * priority level (discarding what was supplied in the interrupt's priority
 * argument), and will run even if irq_lock() is active. Be careful!
 */
#define IRQ_ZERO_LATENCY	BIT(0)
#endif


/* All arguments must be computable by the compiler at build time.
 *
 * Z_ISR_DECLARE will populate the .intList section with the interrupt's
 * parameters, which will then be used by gen_irq_tables.py to create
 * the vector table and the software ISR table. This is all done at
 * build-time.
 *
 * We additionally set the priority in the interrupt controller at
 * runtime.
 */
#define ARCH_IRQ_CONNECT(irq_p, priority_p, isr_p, isr_param_p, flags_p) \
({ \
	Z_ISR_DECLARE(irq_p, 0, isr_p, isr_param_p); \
	z_arm_irq_priority_set(irq_p, priority_p, flags_p); \
	irq_p; \
})

#define ARCH_IRQ_DIRECT_CONNECT(irq_p, priority_p, isr_p, flags_p) \
({ \
	Z_ISR_DECLARE(irq_p, ISR_FLAG_DIRECT, isr_p, NULL); \
	z_arm_irq_priority_set(irq_p, priority_p, flags_p); \
	irq_p; \
})

#ifdef CONFIG_SYS_POWER_MANAGEMENT
extern void _arch_isr_direct_pm(void);
#define ARCH_ISR_DIRECT_PM() _arch_isr_direct_pm()
#else
#define ARCH_ISR_DIRECT_PM() do { } while (false)
#endif

#define ARCH_ISR_DIRECT_HEADER() arch_isr_direct_header()
#define ARCH_ISR_DIRECT_FOOTER(swap) arch_isr_direct_footer(swap)

/* arch/arm/core/aarch32/exc_exit.S */
extern void z_arm_int_exit(void);

#ifdef CONFIG_TRACING
extern void sys_trace_isr_enter(void);
extern void sys_trace_isr_exit(void);
#endif

static inline void arch_isr_direct_header(void)
{
#ifdef CONFIG_TRACING
	sys_trace_isr_enter();
#endif
}

static inline void arch_isr_direct_footer(int maybe_swap)
{
#ifdef CONFIG_TRACING
	sys_trace_isr_exit();
#endif
	if (maybe_swap) {
		z_arm_int_exit();
	}
}

#define ARCH_ISR_DIRECT_DECLARE(name) \
	static inline int name##_body(void); \
	__attribute__ ((interrupt ("IRQ"))) void name(void) \
	{ \
		int check_reschedule; \
		ISR_DIRECT_HEADER(); \
		check_reschedule = name##_body(); \
		ISR_DIRECT_FOOTER(check_reschedule); \
	} \
	static inline int name##_body(void)

#if defined(CONFIG_DYNAMIC_DIRECT_INTERRUPTS)

extern void z_arm_irq_direct_dynamic_dispatch_reschedule(void);
extern void z_arm_irq_direct_dynamic_dispatch_no_reschedule(void);

/**
 * @brief Macro to register an ISR Dispatcher (with or without re-scheduling
 * request) for dynamic direct interrupts.
 *
 * This macro registers the ISR dispatcher function for dynamic direct
 * interrupts for a particular IRQ line, allowing the use of dynamic
 * direct ISRs in the kernel for that interrupt source.
 * The dispatcher function is invoked when the hardware
 * interrupt occurs and then triggers the (software) Interrupt Service Routine
 * (ISR) that is registered dynamically (i.e. at run-time) into the software
 * ISR table stored in SRAM. The ISR must be connected with
 * irq_connect_dynamic() and enabled via irq_enable() before the dynamic direct
 * interrupt can be serviced. This ISR dispatcher must be configured by the
 * user to trigger thread re-secheduling upon return, using the @param resch
 * parameter.
 *
 * These ISRs are designed for performance-critical interrupt handling and do
 * not go through all of the common interrupt handling code.
 *
 * With respect to their declaration, dynamic 'direct' interrupts are regular
 * Zephyr interrupts; their signature must match void isr(void* parameter), as,
 * unlike regular direct interrupts, they are not placed directly into the
 * ROM hardware vector table but instead they are installed in the software
 * ISR table.
 *
 * The major differences with regular Zephyr interrupts are the following:
 * - Similar to direct interrupts, the call into the OS to exit power
 *   management idle state is optional. Normal interrupts always do this
 *   before the ISR is run, but with dynamic direct ones when and if it runs
 *   is controlled by the placement of
 *   a ISR_DIRECT_PM() macro, or omitted entirely.
 * - Similar to direct interrupts, scheduling decisions are optional. Unlike
 *   direct interrupts, the decisions must be made at build time.
 *   They are controlled by @param resch to this macro.
 *
 * @param irq_p IRQ line number.
 * @param priority_p Interrupt priority.
 * @param flags_p Architecture-specific IRQ configuration flags.
 * @param resch Set flag to 'reschedule' to request thread
 *              re-scheduling upon ISR function. Set flag
 *              'no_reschedule' to skip thread re-scheduling
 *
 * Note: the function is an ARM Cortex-M only API.
 *
 * @return Interrupt vector assigned to this interrupt.
 */
#define ARM_IRQ_DIRECT_DYNAMIC_CONNECT(irq_p, priority_p, flags_p, resch) \
	IRQ_DIRECT_CONNECT(irq_p, priority_p, \
		CONCAT(z_arm_irq_direct_dynamic_dispatch_, resch), flags_p)

#endif /* CONFIG_DYNAMIC_DIRECT_INTERRUPTS */

/* Spurious interrupt handler. Throws an error if called */
extern void z_irq_spurious(void *unused);

#ifdef CONFIG_GEN_SW_ISR_TABLE
/* Architecture-specific common entry point for interrupts from the vector
 * table. Most likely implemented in assembly. Looks up the correct handler
 * and parameter from the _sw_isr_table and executes it.
 */
extern void _isr_wrapper(void);
#endif

#endif /* _ASMLANGUAGE */

#ifdef __cplusplus
}
#endif

#endif /* ZEPHYR_INCLUDE_ARCH_ARM_AARCH32_IRQ_H_ */
