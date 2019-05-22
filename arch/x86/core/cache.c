/*
 * Copyright (c) 2013-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
/**
 * @file
 * @brief Cache manipulation
 *
 * This module contains functions for manipulation caches.
 */

#include <kernel.h>
#include <arch/cpu.h>
#include <misc/util.h>
#include <toolchain.h>
#include <cache.h>
#include <cache_private.h>
#include <stdbool.h>

#if defined(CONFIG_CLFLUSH_INSTRUCTION_SUPPORTED) || \
	defined(CONFIG_CLFLUSH_DETECT)

#if (CONFIG_CACHE_LINE_SIZE == 0) && !defined(CONFIG_CACHE_LINE_SIZE_DETECT)
#error Cannot use this implementation with a cache line size of 0
#endif

/**
 *
 * @brief Flush cache lines to main memory
 *
 * No alignment is required for either <virt> or <size>, but since
 * sys_cache_flush() iterates on the cache lines, a cache line alignment for
 * both is optimal.
 *
 * The cache line size is specified either via the CONFIG_CACHE_LINE_SIZE
 * kconfig option or it is detected at runtime.
 *
 * @return N/A
 */

_sys_cache_flush_sig(_cache_flush_clflush)
{
	int end;

	size = ROUND_UP(size, sys_cache_line_size);
	end = virt + size;

	for (; virt < end; virt += sys_cache_line_size) {
		__asm__ volatile("clflush %0;\n\t" :  : "m"(virt));
	}

	__asm__ volatile("mfence;\n\t");
}

#endif /* CONFIG_CLFLUSH_INSTRUCTION_SUPPORTED || CLFLUSH_DETECT */

#if defined(CONFIG_CLFLUSH_DETECT) || defined(CONFIG_CACHE_LINE_SIZE_DETECT)

#include <init.h>

#if defined(CONFIG_CLFLUSH_DETECT)
_sys_cache_flush_t *sys_cache_flush;
static void init_cache_flush(void)
{
	if (z_is_clflush_available()) {
		sys_cache_flush = _cache_flush_clflush;
	} else {
		sys_cache_flush = z_cache_flush_wbinvd;
	}
}
#else
#define init_cache_flush() do { } while (false)

#if defined(CONFIG_CLFLUSH_INSTRUCTION_SUPPORTED)
FUNC_ALIAS(_cache_flush_clflush, sys_cache_flush, void);
#endif

#endif /* CONFIG_CLFLUSH_DETECT */


#if defined(CONFIG_CACHE_LINE_SIZE_DETECT)
size_t sys_cache_line_size;
static void init_cache_line_size(void)
{
	sys_cache_line_size = z_cache_line_size_get();
}
#else
#define init_cache_line_size() do { } while ((0))
#endif

static int init_cache(struct device *unused)
{
	ARG_UNUSED(unused);

	init_cache_flush();
	init_cache_line_size();

	return 0;
}

SYS_INIT(init_cache, PRE_KERNEL_1, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT);

#endif /* CONFIG_CLFLUSH_DETECT || CONFIG_CACHE_LINE_SIZE_DETECT */
