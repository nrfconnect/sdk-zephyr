/*
 * Copyright (c) 2015, Wind River Systems, Inc.
 * Copyright (c) 2017, Oticon A/S
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * POSIX ARCH specific public inline "assembler" functions and macros
 */

/* Either public functions or macros or invoked by public functions */

#ifndef ZEPHYR_INCLUDE_ARCH_POSIX_ASM_INLINE_GCC_H_
#define ZEPHYR_INCLUDE_ARCH_POSIX_ASM_INLINE_GCC_H_

/*
 * The file must not be included directly
 * Include kernel.h instead
 */


#ifdef __cplusplus
extern "C" {
#endif

#ifndef _ASMLANGUAGE

#include <toolchain/common.h>
#include <zephyr/types.h>
#include <sys_io.h>
#include <arch/bits_portable.h>
#include "posix_soc_if.h"


#endif /* _ASMLANGUAGE */

#ifdef __cplusplus
}
#endif

#endif /* ZEPHYR_INCLUDE_ARCH_POSIX_ASM_INLINE_GCC_H_ */
