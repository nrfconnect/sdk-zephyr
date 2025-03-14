/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief ARM Cortex-M debug monitor functions interface based on DWT
 *
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <cortex_m/dwt.h>

/**
 * @brief Assess whether a debug monitor event should be treated as an error
 *
 * This routine checks the status of a debug monitor ()exception, and
 * evaluates whether this needs to be considered as a processor error.
 *
 * @return true if the DM exception is a processor error, otherwise false
 */
bool z_arm_debug_monitor_event_error_check(void)
{
#if defined(CONFIG_NULL_POINTER_EXCEPTION_DETECTION_DWT)
	/* Assess whether this debug exception was triggered
	 * as a result of a null pointer (R/W) dereferencing.
	 */
	if (SCB->DFSR & SCB_DFSR_DWTTRAP_Msk) {
		/* Debug event generated by the DWT unit */
		if (DWT->FUNCTION0 & DWT_FUNCTION_MATCHED_Msk) {
			/* DWT Comparator match used for */
			printk("Null-pointer exception?\n");
		}
		__ASSERT((DWT->FUNCTION0 & DWT_FUNCTION_MATCHED_Msk) == 0,
			 "MATCHED flag should have been cleared on read.");

		return true;
	}
	if (SCB->DFSR & SCB_DFSR_BKPT_Msk) {
		/* Treat BKPT events as an error as well (since they
		 * would mean the system would be stuck in an infinite loop).
		 */
		return true;
	}
#endif /* CONFIG_NULL_POINTER_EXCEPTION_DETECTION_DWT */
	return false;
}

#if defined(CONFIG_NULL_POINTER_EXCEPTION_DETECTION_DWT)

/* The area (0x0 - <size>) monitored by DWT needs to be a power of 2,
 * so we add a build assert that catches it.
 */
BUILD_ASSERT(!(CONFIG_CORTEX_M_NULL_POINTER_EXCEPTION_PAGE_SIZE &
	       (CONFIG_CORTEX_M_NULL_POINTER_EXCEPTION_PAGE_SIZE - 1)),
	     "the size of the partition must be power of 2");

int z_arm_debug_enable_null_pointer_detection(void)
{

	z_arm_dwt_init();
	z_arm_dwt_enable_debug_monitor();

	/* Enable null pointer detection by monitoring R/W access to the
	 * memory area 0x0 - <size> that is (or was intentionally left)
	 * unused by the system.
	 */

#if defined(CONFIG_ARMV8_M_MAINLINE)

	/* ASSERT that we have the comparators needed for the implementation */
	if (((DWT->CTRL & DWT_CTRL_NUMCOMP_Msk) >> DWT_CTRL_NUMCOMP_Pos) < 2) {
		__ASSERT(0, "on board DWT does not support the feature\n");
		return -EINVAL;
	}

	/* Use comparators 0, 1, R/W access check */
	DWT->COMP0 = 0;
	DWT->COMP1 = CONFIG_CORTEX_M_NULL_POINTER_EXCEPTION_PAGE_SIZE - 1;

	DWT->FUNCTION0 = ((0x4 << DWT_FUNCTION_MATCH_Pos) & DWT_FUNCTION_MATCH_Msk) |
			 ((0x1 << DWT_FUNCTION_ACTION_Pos) & DWT_FUNCTION_ACTION_Msk) |
			 ((0x0 << DWT_FUNCTION_DATAVSIZE_Pos) & DWT_FUNCTION_DATAVSIZE_Msk);
	DWT->FUNCTION1 = ((0x7 << DWT_FUNCTION_MATCH_Pos) & DWT_FUNCTION_MATCH_Msk) |
			 ((0x1 << DWT_FUNCTION_ACTION_Pos) & DWT_FUNCTION_ACTION_Msk) |
			 ((0x0 << DWT_FUNCTION_DATAVSIZE_Pos) & DWT_FUNCTION_DATAVSIZE_Msk);
#elif defined(CONFIG_ARMV7_M_ARMV8_M_MAINLINE)

	/* ASSERT that we have the comparator needed for the implementation */
	if (((DWT->CTRL & DWT_CTRL_NUMCOMP_Msk) >> DWT_CTRL_NUMCOMP_Pos) < 1) {
		__ASSERT(0, "on board DWT does not support the feature\n");
		return -EINVAL;
	}

	/* Use comparator 0, R/W access check */
	DWT->COMP0 = 0;

	DWT->FUNCTION0 = (0x7 << DWT_FUNCTION_FUNCTION_Pos) & DWT_FUNCTION_FUNCTION_Msk;

	/* Set mask according to the desired size */
	DWT->MASK0 = 32 - __builtin_clzl(CONFIG_CORTEX_M_NULL_POINTER_EXCEPTION_PAGE_SIZE - 1);
#endif

	return 0;
}

#endif /* CONFIG_NULL_POINTER_EXCEPTION_DETECTION_DWT */
