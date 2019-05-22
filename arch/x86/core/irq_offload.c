/*
 * Copyright (c) 2015 Intel corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file IRQ offload - x86 implementation
 */

#include <kernel.h>
#include <irq_offload.h>

extern void (*_irq_sw_handler)(void);
NANO_CPU_INT_REGISTER(_irq_sw_handler, NANO_SOFT_IRQ,
		      CONFIG_IRQ_OFFLOAD_VECTOR / 16,
		      CONFIG_IRQ_OFFLOAD_VECTOR, 0);

static irq_offload_routine_t offload_routine;
static void *offload_param;

/* Called by asm stub */
void z_irq_do_offload(void)
{
	offload_routine(offload_param);
}

void irq_offload(irq_offload_routine_t routine, void *parameter)
{
	unsigned int key;

	/*
	 * Lock interrupts here to prevent any concurrency issues with
	 * the two globals
	 */
	key = irq_lock();
	offload_routine = routine;
	offload_param = parameter;

	__asm__ volatile("int %[vector]" : :
			 [vector] "i" (CONFIG_IRQ_OFFLOAD_VECTOR));

	irq_unlock(key);
}
