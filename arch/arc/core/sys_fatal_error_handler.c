/*
 * Copyright (c) 2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief ARCv2 system fatal error handler
 *
 * This module provides the z_SysFatalErrorHandler() routine for ARCv2 BSPs.
 */

#include <kernel.h>
#include <toolchain.h>
#include <linker/sections.h>
#include <kernel_structs.h>
#include <misc/printk.h>

/**
 *
 * @brief Fatal error handler
 *
 * This routine implements the corrective action to be taken when the system
 * detects a fatal error.
 *
 * This sample implementation attempts to abort the current thread and allow
 * the system to continue executing, which may permit the system to continue
 * functioning with degraded capabilities.
 *
 * System designers may wish to enhance or substitute this sample
 * implementation to take other actions, such as logging error (or debug)
 * information to a persistent repository and/or rebooting the system.
 *
 * @param reason the fatal error reason
 * @param pEsf pointer to exception stack frame
 *
 * @return N/A
 */
__weak void z_SysFatalErrorHandler(unsigned int reason,
						const NANO_ESF *pEsf)
{
	ARG_UNUSED(pEsf);

#if !defined(CONFIG_SIMPLE_FATAL_ERROR_HANDLER)
#if defined(CONFIG_STACK_SENTINEL)
	if (reason == _NANO_ERR_STACK_CHK_FAIL) {
		goto hang_system;
	}
#endif
	if (reason == _NANO_ERR_KERNEL_PANIC) {
		goto hang_system;
	}

	if (z_is_thread_essential()) {
		printk("Fatal fault in essential thread! Spinning...\n");
		goto hang_system;
	}

	printk("Fatal fault in thread %p! Aborting.\n", _current);

	k_thread_abort(_current);

	return;

hang_system:
#else
	ARG_UNUSED(reason);
#endif

	for (;;) {
		k_cpu_idle();
	}
}
