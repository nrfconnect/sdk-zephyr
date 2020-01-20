/*
 * Copyright (c) 2019 Intel Corporation
 * SPDX-License-Identifier: Apache-2.0
 */

#include <kernel.h>
#include <kernel_arch_data.h>
#include <kernel_arch_func.h>
#include <kernel_structs.h>
#include <arch/x86/multiboot.h>
#include <arch/x86/mmustructs.h>
#include <drivers/interrupt_controller/loapic.h>

/*
 * Map of CPU logical IDs to CPU local APIC IDs. By default,
 * we assume this simple identity mapping, as found in QEMU.
 * The symbol is weak so that boards/SoC files can override.
 */

__weak u8_t x86_cpu_loapics[] = { 0, 1, 2, 3 };

extern char x86_ap_start[];  /* AP entry point in locore.S */

extern u8_t _exception_stack[];
extern u8_t _exception_stack1[];
extern u8_t _exception_stack2[];
extern u8_t _exception_stack3[];

Z_GENERIC_SECTION(.tss)
struct x86_tss64 tss0 = {
	.ist7 = (u64_t) _exception_stack + CONFIG_EXCEPTION_STACK_SIZE,
	.iomapb = 0xFFFF,
	.cpu = &(_kernel.cpus[0])
};

#if CONFIG_MP_NUM_CPUS > 1
Z_GENERIC_SECTION(.tss)
struct x86_tss64 tss1 = {
	.ist7 = (u64_t) _exception_stack1 + CONFIG_EXCEPTION_STACK_SIZE,
	.iomapb = 0xFFFF,
	.cpu = &(_kernel.cpus[1])
};
#endif

#if CONFIG_MP_NUM_CPUS > 2
Z_GENERIC_SECTION(.tss)
struct x86_tss64 tss2 = {
	.ist7 = (u64_t) _exception_stack2 + CONFIG_EXCEPTION_STACK_SIZE,
	.iomapb = 0xFFFF,
	.cpu = &(_kernel.cpus[2])
};
#endif

#if CONFIG_MP_NUM_CPUS > 3
Z_GENERIC_SECTION(.tss)
struct x86_tss64 tss3 = {
	.ist7 = (u64_t) _exception_stack3 + CONFIG_EXCEPTION_STACK_SIZE,
	.iomapb = 0xFFFF,
	.cpu = &(_kernel.cpus[3])
};
#endif

extern struct x86_page_tables z_x86_flat_ptables;

struct x86_cpuboot x86_cpuboot[] = {
	{
		.tr = X86_KERNEL_CPU0_TR,
		.gs_base = &tss0,
		.sp = (u64_t) _interrupt_stack + CONFIG_ISR_STACK_SIZE,
		.fn = z_x86_prep_c,
#ifdef CONFIG_X86_MMU
		.ptables = &z_x86_flat_ptables,
#endif
	},
#if CONFIG_MP_NUM_CPUS > 1
	{
		.tr = X86_KERNEL_CPU1_TR,
		.gs_base = &tss1
	},
#endif
#if CONFIG_MP_NUM_CPUS > 2
	{
		.tr = X86_KERNEL_CPU2_TR,
		.gs_base = &tss2
	},
#endif
#if CONFIG_MP_NUM_CPUS > 3
	{
		.tr = X86_KERNEL_CPU3_TR,
		.gs_base = &tss3
	},
#endif
};

/*
 * Send the INIT/STARTUP IPI sequence required to start up CPU 'cpu_num', which
 * will enter the kernel at fn(arg), running on the specified stack.
 */

void arch_start_cpu(int cpu_num, k_thread_stack_t *stack, int sz,
		    arch_cpustart_t fn, void *arg)
{
	u8_t vector = ((unsigned long) x86_ap_start) >> 12;
	u8_t apic_id = x86_cpu_loapics[cpu_num];

	x86_cpuboot[cpu_num].sp = (u64_t) Z_THREAD_STACK_BUFFER(stack) + sz;
	x86_cpuboot[cpu_num].fn = fn;
	x86_cpuboot[cpu_num].arg = arg;
#ifdef CONFIG_X86_MMU
	x86_cpuboot[cpu_num].ptables = &z_x86_kernel_ptables;
#endif /* CONFIG_X86_MMU */

	z_loapic_ipi(apic_id, LOAPIC_ICR_IPI_INIT, 0);
	k_busy_wait(10000);
	z_loapic_ipi(apic_id, LOAPIC_ICR_IPI_STARTUP, vector);

	while (x86_cpuboot[cpu_num].ready == 0) {
	}
}

/* Per-CPU initialization, C domain. On the first CPU, z_x86_prep_c is the
 * next step. For other CPUs it is probably smp_init_top().
 */
FUNC_NORETURN void z_x86_cpu_init(struct x86_cpuboot *cpuboot)
{
	x86_sse_init(NULL);

	z_loapic_enable();

#ifdef CONFIG_USERSPACE
	/* Set landing site for 'syscall' instruction */
	z_x86_msr_write(X86_LSTAR_MSR, (u64_t)z_x86_syscall_entry_stub);

	/* Set segment descriptors for syscall privilege transitions */
	z_x86_msr_write(X86_STAR_MSR, (u64_t)X86_STAR_UPPER << 32);

	/* Mask applied to RFLAGS when making a syscall */
	z_x86_msr_write(X86_FMASK_MSR, EFLAGS_SYSCALL);
#endif

	/* Enter kernel, never return */
	cpuboot->ready++;
	cpuboot->fn(cpuboot->arg);
}
