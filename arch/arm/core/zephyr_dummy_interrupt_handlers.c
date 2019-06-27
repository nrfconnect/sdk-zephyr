/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <stdint.h>
#include <stddef.h>

void dummy_handler(void)
{
	/* Hang on unexpected faults and interrupts as it's considered
	 * a bug in the program.
	 */
	while (1) {
		;
	}
}

/* Weakly assign all interrupt handlers to dummy_handler */
#define ALIAS_DUMMY __attribute__((weak, alias("dummy_handler")))

void __nmi(void) ALIAS_DUMMY;
void __hard_fault(void) ALIAS_DUMMY;
#if defined(CONFIG_ARMV7_M_ARMV8_M_MAINLINE)
void __mpu_fault(void) ALIAS_DUMMY;
void __bus_fault(void) ALIAS_DUMMY;
void __usage_fault(void) ALIAS_DUMMY;
void __debug_monitor(void) ALIAS_DUMMY;
#if defined(CONFIG_ARM_SECURE_FIRMWARE)
void __secure_fault(void) ALIAS_DUMMY;
#endif /* CONFIG_ARM_SECURE_FIRMWARE */
#endif /* CONFIG_ARMV7_M_ARMV8_M_MAINLINE */
void __svc(void) ALIAS_DUMMY;
void __pendsv(void) ALIAS_DUMMY;
void __sys_tick(void) ALIAS_DUMMY;
void z_clock_isr(void) ALIAS_DUMMY;
