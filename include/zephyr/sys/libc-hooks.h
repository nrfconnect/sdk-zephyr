/*
 * Copyright (c) 2018, Intel Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_INCLUDE_SYS_LIBC_HOOKS_H_
#define ZEPHYR_INCLUDE_SYS_LIBC_HOOKS_H_

#include <zephyr/toolchain.h>
#include <zephyr/app_memory/app_memdomain.h>
#include <stdio.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Private header for specifying accessory functions to the C library internals
 * that need to call into the kernel as system calls
 */

#if defined(CONFIG_NEWLIB_LIBC) || defined(CONFIG_ARCMWDT_LIBC) || defined(CONFIG_PICOLIBC)

/* syscall generation ignores preprocessor, ensure this is defined to ensure
 * we don't have compile errors
 */
__syscall int zephyr_read_stdin(char *buf, int nbytes);

__syscall int zephyr_write_stdout(const void *buf, int nbytes);

#else
/* Minimal libc */

__syscall int zephyr_fputc(int c, FILE * stream);

__syscall size_t zephyr_fwrite(const void *ZRESTRICT ptr, size_t size,
				size_t nitems, FILE *ZRESTRICT stream);
#endif /* CONFIG_NEWLIB_LIBC */

/* Handle deprecated malloc arena size configuration values */
#ifdef CONFIG_COMMON_LIBC_MALLOC
# if defined(CONFIG_MINIMAL_LIBC) && (CONFIG_MINIMAL_LIBC_MALLOC_ARENA_SIZE != -2)
#  undef CONFIG_COMMON_LIBC_MALLOC_ARENA_SIZE
#  define CONFIG_COMMON_LIBC_MALLOC_ARENA_SIZE CONFIG_MINIMAL_LIBC_MALLOC_ARENA_SIZE
#  warning Using deprecated setting CONFIG_MINIMAL_LIBC_MALLOC_ARENA_SIZE
# elif defined(CONFIG_PICOLIBC) && (CONFIG_PICOLIBC_HEAP_SIZE != -2)
#  undef CONFIG_COMMON_LIBC_MALLOC_ARENA_SIZE
#  define CONFIG_COMMON_LIBC_MALLOC_ARENA_SIZE CONFIG_PICOLIBC_HEAP_SIZE
#  warning Using deprecated setting CONFIG_PICOLIBC_HEAP_SIZE
# endif
#endif

#ifdef CONFIG_USERSPACE
#ifdef CONFIG_COMMON_LIBC_MALLOC

/* When using the common malloc implementation with CONFIG_USERSPACE, the
 * heap will be in a separate partition when there's an MPU or MMU
 * available.
 */
#if CONFIG_COMMON_LIBC_MALLOC_ARENA_SIZE != 0 && \
(defined(CONFIG_MPU) || defined(CONFIG_MMU))
#define Z_MALLOC_PARTITION_EXISTS 1
#endif

#elif defined(CONFIG_NEWLIB_LIBC)
/* If we are using newlib, the heap arena is in one of two areas:
 *  - If we have an MPU that requires power of two alignment, the heap bounds
 *    must be specified in Kconfig via CONFIG_NEWLIB_LIBC_ALIGNED_HEAP_SIZE.
 *  - Otherwise, the heap arena on most arches starts at a suitably
 *    aligned base addreess after the `_end` linker symbol, through to the end
 *    of system RAM.
 */
#if (!defined(CONFIG_MPU_REQUIRES_POWER_OF_TWO_ALIGNMENT) || \
     (defined(CONFIG_MPU_REQUIRES_POWER_OF_TWO_ALIGNMENT) && \
      CONFIG_NEWLIB_LIBC_ALIGNED_HEAP_SIZE))
#define Z_MALLOC_PARTITION_EXISTS 1
#endif

#endif /* CONFIG_NEWLIB_LIBC */

#ifdef Z_MALLOC_PARTITION_EXISTS
/* Memory partition containing the libc malloc arena. Configuration controls
 * whether this is available, and an arena size may need to be set.
 */
extern struct k_mem_partition z_malloc_partition;
#endif

#if defined(CONFIG_NEWLIB_LIBC) || defined(CONFIG_STACK_CANARIES) || \
defined(CONFIG_PICOLIBC) || defined(CONFIG_NEED_LIBC_MEM_PARTITION)
/* - All newlib globals will be placed into z_libc_partition.
 * - Minimal C library globals, if any, will be placed into
 *   z_libc_partition.
 * - Stack canary globals will be placed into z_libc_partition since
 *   it is not worth placing in its own partition.
 * - Some architectures may place the global pointer to the thread local
 *   storage in z_libc_partition since it is not worth placing in its
 *   own partition.
 */
#define Z_LIBC_PARTITION_EXISTS 1

/* C library globals, except the malloc arena */
extern struct k_mem_partition z_libc_partition;
#endif
#endif /* CONFIG_USERSPACE */

#include <syscalls/libc-hooks.h>

/* C library memory partitions */
#define Z_LIBC_DATA K_APP_DMEM(z_libc_partition)

#ifdef __cplusplus
}
#endif

#endif /* ZEPHYR_INCLUDE_SYS_LIBC_HOOKS_H_ */
