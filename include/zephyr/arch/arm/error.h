/*
 * Copyright (c) 2013-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief ARM AArch32 public error handling
 *
 * ARM AArch32-specific kernel error handling interface. Included by
 * arm/arch.h.
 */

#ifndef ZEPHYR_INCLUDE_ARCH_ARM_ERROR_H_
#define ZEPHYR_INCLUDE_ARCH_ARM_ERROR_H_

#include <zephyr/arch/arm/syscall.h>
#include <zephyr/arch/arm/exception.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(CONFIG_ARMV6_M_ARMV8_M_BASELINE)
/* ARMv6 will hard-fault if SVC is called with interrupts locked. Just
 * force them unlocked, the thread is in an undefined state anyway
 *
 * On ARMv7m we won't get a HardFault, but if interrupts were locked the
 * thread will continue executing after the exception and forbid PendSV to
 * schedule a new thread until they are unlocked which is not what we want.
 * Force them unlocked as well.
 */
#define ARCH_EXCEPT(reason_p) \
register uint32_t r0 __asm__("r0") = reason_p; \
do { \
	__asm__ volatile ( \
		"cpsie i\n\t" \
		"svc %[id]\n\t" \
		: \
		: "r" (r0), [id] "i" (_SVC_CALL_RUNTIME_EXCEPT) \
		: "memory"); \
} while (false)
#elif defined(CONFIG_ARMV7_M_ARMV8_M_MAINLINE)
#define ARCH_EXCEPT(reason_p) do { \
	__asm__ volatile ( \
		"eors.n r0, r0\n\t" \
		"msr BASEPRI, r0\n\t" \
		"mov r0, %[reason]\n\t" \
		"svc %[id]\n\t" \
		: \
		: [reason] "i" (reason_p), [id] "i" (_SVC_CALL_RUNTIME_EXCEPT) \
		: "memory"); \
} while (false)
#elif defined(CONFIG_ARMV7_R) || defined(CONFIG_AARCH32_ARMV8_R) \
	|| defined(CONFIG_ARMV7_A)
/*
 * In order to support using svc for an exception while running in an
 * isr, stack $lr_svc before calling svc.  While exiting the isr,
 * z_check_stack_sentinel is called.  $lr_svc contains the return address.
 * If the sentinel is wrong, it calls svc to cause an oops.  This svc
 * call will overwrite $lr_svc, losing the return address from the
 * z_check_stack_sentinel call if it is not stacked before the svc.
 */
#define ARCH_EXCEPT(reason_p) \
register uint32_t r0 __asm__("r0") = reason_p; \
do { \
	__asm__ volatile ( \
		"push {lr}\n\t" \
		"cpsie i\n\t" \
		"svc %[id]\n\t" \
		"pop {lr}\n\t" \
		: \
		: "r" (r0), [id] "i" (_SVC_CALL_RUNTIME_EXCEPT) \
		: "memory"); \
} while (false)
#else
#error Unknown ARM architecture
#endif /* CONFIG_ARMV6_M_ARMV8_M_BASELINE */

#ifdef __cplusplus
}
#endif

#endif /* ZEPHYR_INCLUDE_ARCH_ARM_ERROR_H_ */
