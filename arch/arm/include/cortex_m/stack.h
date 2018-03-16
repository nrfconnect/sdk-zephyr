/*
 * Copyright (c) 2013-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Stack helpers for Cortex-M CPUs
 *
 * Stack helper functions.
 */

#ifndef _ARM_CORTEXM_STACK__H_
#define _ARM_CORTEXM_STACK__H_

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _ASMLANGUAGE

/* nothing */

#else

#include "arch/arm/cortex_m/cmsis.h"

extern K_THREAD_STACK_DEFINE(_interrupt_stack, CONFIG_ISR_STACK_SIZE);

/**
 *
 * @brief Setup interrupt stack
 *
 * On Cortex-M, the interrupt stack is registered in the MSP (main stack
 * pointer) register, and switched to automatically when taking an exception.
 *
 * @return N/A
 */
static ALWAYS_INLINE void _InterruptStackSetup(void)
{
#ifdef CONFIG_MPU_REQUIRES_POWER_OF_TWO_ALIGNMENT
	u32_t msp = (u32_t)(K_THREAD_STACK_BUFFER(_interrupt_stack) +
			    CONFIG_ISR_STACK_SIZE - MPU_GUARD_ALIGN_AND_SIZE);
#else
	u32_t msp = (u32_t)(K_THREAD_STACK_BUFFER(_interrupt_stack) +
			    CONFIG_ISR_STACK_SIZE);
#endif

	__set_MSP(msp);
}

#endif /* _ASMLANGUAGE */

#ifdef __cplusplus
}
#endif

#endif /* _ARM_CORTEXM_STACK__H_ */
