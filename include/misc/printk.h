/* printk.h - low-level debug output */

/*
 * Copyright (c) 2010-2012, 2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef ZEPHYR_INCLUDE_MISC_PRINTK_H_
#define ZEPHYR_INCLUDE_MISC_PRINTK_H_

#include <toolchain.h>
#include <stddef.h>
#include <stdarg.h>
#include <inttypes.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 *
 * @brief Print kernel debugging message.
 *
 * This routine prints a kernel debugging message to the system console.
 * Output is send immediately, without any mutual exclusion or buffering.
 *
 * A basic set of conversion specifier characters are supported:
 *   - signed decimal: \%d, \%i
 *   - unsigned decimal: \%u
 *   - unsigned hexadecimal: \%x (\%X is treated as \%x)
 *   - pointer: \%p
 *   - string: \%s
 *   - character: \%c
 *   - percent: \%\%
 *
 * Field width (with or without leading zeroes) are supported.
 * Length attributes such as 'h' and 'l' are supported. However,
 * integral values with %lld and %lli are only printed if they fit in 32 bits,
 * otherwise 'ERR' is printed. Full 64-bit values may be printed with %llx.
 * Flags and precision attributes are not supported.
 *
 * @param fmt Format string.
 * @param ... Optional list of format arguments.
 *
 * @return N/A
 */
#ifdef CONFIG_PRINTK
extern __printf_like(1, 2) void printk(const char *fmt, ...);
extern __printf_like(1, 0) void vprintk(const char *fmt, va_list ap);
extern __printf_like(3, 4) int snprintk(char *str, size_t size,
					const char *fmt, ...);
extern __printf_like(3, 0) int vsnprintk(char *str, size_t size,
					  const char *fmt, va_list ap);

extern __printf_like(3, 0) void z_vprintk(int (*out)(int f, void *c), void *ctx,
					 const char *fmt, va_list ap);
#else
static inline __printf_like(1, 2) void printk(const char *fmt, ...)
{
	ARG_UNUSED(fmt);
}

static inline __printf_like(1, 0) void vprintk(const char *fmt, va_list ap)
{
	ARG_UNUSED(fmt);
	ARG_UNUSED(ap);
}

static inline __printf_like(3, 4) int snprintk(char *str, size_t size,
					       const char *fmt, ...)
{
	ARG_UNUSED(str);
	ARG_UNUSED(size);
	ARG_UNUSED(fmt);
	return 0;
}

static inline __printf_like(3, 0) int vsnprintk(char *str, size_t size,
						const char *fmt, va_list ap)
{
	ARG_UNUSED(str);
	ARG_UNUSED(size);
	ARG_UNUSED(fmt);
	ARG_UNUSED(ap);

	return 0;
}
#endif

#ifdef __cplusplus
}
#endif

#endif
