/*
 * Copyright (c) 2019 Intel Corp.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_ARCH_X86_INCLUDE_INTEL64_KERNEL_ARCH_DATA_H_
#define ZEPHYR_ARCH_X86_INCLUDE_INTEL64_KERNEL_ARCH_DATA_H_

#include <arch/x86/mmustructs.h>

#ifndef _ASMLANGUAGE

/* linker symbols defining the bounds of the kernel part loaded in locore */

extern char _locore_start[], _locore_end[];

/*
 * Per-CPU bootstrapping parameters. See locore.S and cpu.c.
 */

struct x86_cpuboot {
	volatile int ready;	/* CPU has started */
	u16_t tr;		/* selector for task register */
	struct x86_tss64 *gs_base; /* Base address for GS segment */
	u64_t sp;		/* initial stack pointer */
	arch_cpustart_t fn;	/* kernel entry function */
	void *arg;		/* argument for above function */
#ifdef CONFIG_X86_MMU
	struct x86_page_tables *ptables; /* Runtime page tables to install */
#endif /* CONFIG_X86_MMU */
};

typedef struct x86_cpuboot x86_cpuboot_t;

extern u8_t x86_cpu_loapics[];	/* CPU logical ID -> local APIC ID */

#endif /* _ASMLANGUAGE */

#endif /* ZEPHYR_ARCH_X86_INCLUDE_INTEL64_KERNEL_ARCH_DATA_H_ */
