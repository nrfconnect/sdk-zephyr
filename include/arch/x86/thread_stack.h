/*
 * Copyright (c) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef ZEPHYR_INCLUDE_ARCH_X86_THREAD_STACK_H
#define ZEPHYR_INCLUDE_ARCH_X86_THREAD_STACK_H

#include <arch/x86/mmustructs.h>

#ifdef CONFIG_X86_64
#define STACK_ALIGN 16UL
#else
#define STACK_ALIGN 4UL
#endif

#ifdef CONFIG_USERSPACE
/* We need a set of page tables for each thread in the system which runs in
 * user mode. For each thread, we have:
 *
 *   - On 32-bit
 *      - a toplevel PDPT
 *   - On 64-bit
 *      - a toplevel PML4
 *      - a set of PDPTs for the memory range covered by system RAM
 *   - On all modes:
 *      - a set of page directories for the memory range covered by system RAM
 *      - a set of page tbales for the memory range covered by system RAM
 *
 * Directories and tables for memory ranges outside of system RAM will be
 * shared and not thread-specific.
 *
 * NOTE: We are operating under the assumption that memory domain partitions
 * will not be configured which grant permission to address ranges outside
 * of system RAM.
 *
 * Each of these page tables will be programmed to reflect the memory
 * permission policy for that thread, which will be the union of:
 *
 *   - The boot time memory regions (text, rodata, and so forth)
 *   - The thread's stack buffer
 *   - Partitions in the memory domain configuration (if a member of a
 *     memory domain)
 *
 * The PDPT is fairly small singleton on x86 PAE (32 bytes) and also must
 * be aligned to 32 bytes, so we place it at the highest addresses of the
 * page reserved for the privilege elevation stack. On 64-bit all table
 * entities up to and including the PML4 are page-sized.
 *
 * The page directories and tables require page alignment so we put them as
 * additional fields in the stack object, using the below macros to compute how
 * many pages we need.
 */

/* Define a range [Z_X86_PT_START, Z_X86_PT_END) which is the memory range
 * covered by all the page tables needed for system RAM
 */
#define Z_X86_PT_START	((uintptr_t)ROUND_DOWN(DT_PHYS_RAM_ADDR, Z_X86_PT_AREA))
#define Z_X86_PT_END	((uintptr_t)ROUND_UP(DT_PHYS_RAM_ADDR + \
					     (DT_RAM_SIZE * 1024UL), \
					     Z_X86_PT_AREA))

/* Number of page tables needed to cover system RAM. Depends on the specific
 * bounds of system RAM, but roughly 1 page table per 2MB of RAM
 */
#define Z_X86_NUM_PT	((Z_X86_PT_END - Z_X86_PT_START) / Z_X86_PT_AREA)

/* Same semantics as above, but for the page directories needed to cover
 * system RAM.
 */
#define Z_X86_PD_START	((uintptr_t)ROUND_DOWN(DT_PHYS_RAM_ADDR, Z_X86_PD_AREA))
#define Z_X86_PD_END	((uintptr_t)ROUND_UP(DT_PHYS_RAM_ADDR + \
					     (DT_RAM_SIZE * 1024UL), \
					     Z_X86_PD_AREA))
/* Number of page directories needed to cover system RAM. Depends on the
 * specific bounds of system RAM, but roughly 1 page directory per 1GB of RAM
 */
#define Z_X86_NUM_PD	((Z_X86_PD_END - Z_X86_PD_START) / Z_X86_PD_AREA)

#ifdef CONFIG_X86_64
/* Same semantics as above, but for the page directory pointer tables needed
 * to cover system RAM. On 32-bit there is just one 4-entry PDPT.
 */
#define Z_X86_PDPT_START	((uintptr_t)ROUND_DOWN(DT_PHYS_RAM_ADDR, \
						       Z_X86_PDPT_AREA))
#define Z_X86_PDPT_END	((uintptr_t)ROUND_UP(DT_PHYS_RAM_ADDR + \
					     (DT_RAM_SIZE * 1024UL), \
					     Z_X86_PDPT_AREA))
/* Number of PDPTs needed to cover system RAM. Depends on the
 * specific bounds of system RAM, but roughly 1 PDPT per 512GB of RAM
 */
#define Z_X86_NUM_PDPT	((Z_X86_PDPT_END - Z_X86_PDPT_START) / Z_X86_PDPT_AREA)

/* All pages needed for page tables, using computed values plus one more for
 * the top-level PML4
 */
#define Z_X86_NUM_TABLE_PAGES   (Z_X86_NUM_PT + Z_X86_NUM_PD + \
				 Z_X86_NUM_PDPT + 1)
#else /* !CONFIG_X86_64 */
/* Number of pages we need to reserve in the stack for per-thread page tables */
#define Z_X86_NUM_TABLE_PAGES	(Z_X86_NUM_PT + Z_X86_NUM_PD)
#endif /* CONFIG_X86_64 */
#else /* !CONFIG_USERSPACE */
/* If we're not implementing user mode, then the MMU tables don't get changed
 * on context switch and we don't need any per-thread page tables
 */
#define Z_X86_NUM_TABLE_PAGES	0UL
#endif /* CONFIG_USERSPACE */

#define Z_X86_THREAD_PT_AREA	(Z_X86_NUM_TABLE_PAGES * MMU_PAGE_SIZE)

#if defined(CONFIG_HW_STACK_PROTECTION) || defined(CONFIG_USERSPACE)
#define Z_X86_STACK_BASE_ALIGN	MMU_PAGE_SIZE
#else
#define Z_X86_STACK_BASE_ALIGN	STACK_ALIGN
#endif

#ifdef CONFIG_USERSPACE
/* If user mode enabled, expand any stack size to fill a page since that is
 * the access control granularity and we don't want other kernel data to
 * unintentionally fall in the latter part of the page
 */
#define Z_X86_STACK_SIZE_ALIGN	MMU_PAGE_SIZE
#else
#define Z_X86_STACK_SIZE_ALIGN	STACK_ALIGN
#endif

#ifndef _ASMLANGUAGE

#ifndef CONFIG_X86_64
struct z_x86_kernel_stack_data {
	/* For 32-bit, a single four-entry page directory pointer table, that
	 * needs to be aligned to 32 bytes.
	 *
	 * 64-bit all the page table entities up to and including the PML4
	 * are page-aligned and we just reserve room for them in
	 * Z_X86_THREAD_PT_AREA.
	 */
	struct x86_page_tables ptables;
} __aligned(0x20);
#endif /* !CONFIG_X86_64 */

/* With both hardware stack protection and userspace enabled, stacks are
 * arranged as follows:
 *
 * High memory addresses
 * +-----------------------------------------+
 * | Thread stack (varies)                   |
 * +-----------------------------------------+
 * | PDPT (32 bytes, 32-bit only)            |
 * | Privilege elevation stack               |
 * |      (4064 or 4096 bytes)               |
 * +-----------------------------------------+
 * | Guard page (4096 bytes)                 |
 * +-----------------------------------------+
 * | User page tables (Z_X86_THREAD_PT_AREA) |
 * +-----------------------------------------+
 * Low Memory addresses
 *
 * Privilege elevation stacks are fixed-size. All the pages containing the
 * thread stack are marked as user-accessible. The guard page is marked
 * read-only to catch stack overflows in supervisor mode.
 *
 * If a thread starts in supervisor mode, the page containing the PDPT and/or
 * privilege elevation stack is also marked read-only.
 *
 * If a thread starts in, or drops down to user mode, the privilege stack page
 * will be marked as present, supervior-only. The page tables will be
 * initialized and used as the active page tables when that thread is active.
 *
 * If KPTI is not enabled, the _main_tss.esp0 field will always be updated
 * updated to point to the top of the privilege elevation stack. Otherwise
 * _main_tss.esp0 always points to the trampoline stack, which handles the
 * page table switch to the kernel PDPT and transplants context to the
 * privileged mode stack.
 */
struct z_x86_thread_stack_header {
#ifdef CONFIG_USERSPACE
	char page_tables[Z_X86_THREAD_PT_AREA];
#endif

#ifdef CONFIG_HW_STACK_PROTECTION
	char guard_page[MMU_PAGE_SIZE];
#endif

#ifdef CONFIG_USERSPACE
#ifdef CONFIG_X86_64
	char privilege_stack[MMU_PAGE_SIZE];
#else
	char privilege_stack[MMU_PAGE_SIZE -
		sizeof(struct z_x86_kernel_stack_data)];

	struct z_x86_kernel_stack_data kernel_data;
#endif /* CONFIG_X86_64 */
#endif /* CONFIG_USERSPACE */
} __packed __aligned(Z_X86_STACK_BASE_ALIGN);

#define ARCH_THREAD_STACK_RESERVED \
	sizeof(struct z_x86_thread_stack_header)

#define ARCH_THREAD_STACK_DEFINE(sym, size) \
	struct _k_thread_stack_element __noinit \
		__aligned(Z_X86_STACK_BASE_ALIGN) \
		sym[ROUND_UP((size), Z_X86_STACK_SIZE_ALIGN) + \
			ARCH_THREAD_STACK_RESERVED]

#define ARCH_THREAD_STACK_LEN(size) \
		(ROUND_UP((size), \
			  MAX(Z_X86_STACK_BASE_ALIGN, \
			      Z_X86_STACK_SIZE_ALIGN)) + \
		ARCH_THREAD_STACK_RESERVED)

#define ARCH_THREAD_STACK_ARRAY_DEFINE(sym, nmemb, size) \
	struct _k_thread_stack_element __noinit \
		__aligned(Z_X86_STACK_BASE_ALIGN) \
		sym[nmemb][ARCH_THREAD_STACK_LEN(size)]

#define ARCH_THREAD_STACK_MEMBER(sym, size) \
	struct _k_thread_stack_element __aligned(Z_X86_STACK_BASE_ALIGN) \
		sym[ROUND_UP((size), Z_X86_STACK_SIZE_ALIGN) + \
			ARCH_THREAD_STACK_RESERVED]

#define ARCH_THREAD_STACK_SIZEOF(sym) \
	(sizeof(sym) - ARCH_THREAD_STACK_RESERVED)

#define ARCH_THREAD_STACK_BUFFER(sym) \
	((char *)((sym) + ARCH_THREAD_STACK_RESERVED))

#endif /* !_ASMLANGUAGE */
#endif /* ZEPHYR_INCLUDE_ARCH_X86_THREAD_STACK_H */
