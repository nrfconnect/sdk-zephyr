/*
 * Copyright (c) 2013-2015 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Non-random number generator based on system timer
 *
 * This module provides a non-random implementation of sys_rand32_get(), which
 * is not meant to be used in a final product as a truly random number
 * generator. It was provided to allow testing on a platform that does not (yet)
 * provide a random number generator.
 */

#include <random/rand32.h>
#include <drivers/timer/system_timer.h>
#include <kernel.h>
#include <sys/atomic.h>
#include <string.h>

#if defined(__GNUC__)

/*
 * Symbols used to ensure a rapid series of calls to random number generator
 * return different values.
 */
static atomic_val_t _rand32_counter;

#define _RAND32_INC 1000000013

/**
 *
 * @brief Get a 32 bit random number
 *
 * The non-random number generator returns values that are based off the
 * target's clock counter, which means that successive calls will return
 * different values.
 *
 * @return a 32-bit number
 */

u32_t sys_rand32_get(void)
{
	return k_cycle_get_32() + atomic_add(&_rand32_counter, _RAND32_INC);
}

/**
 *
 * @brief Fill destination buffer with random numbers
 *
 * The non-random number generator returns values that are based off the
 * target's clock counter, which means that successive calls will return
 * different values.
 *
 * @param dst destination buffer to fill
 * @param outlen size of destination buffer to fill
 *
 * @return N/A
 */

void sys_rand_get(void *dst, size_t outlen)
{
	u32_t len = 0;
	u32_t blocksize = 4;
	u32_t ret;
	u32_t *udst = (u32_t *)dst;

	while (len < outlen) {
		ret = sys_rand32_get();
		if ((outlen-len) < sizeof(ret)) {
			blocksize = len;
			(void)memcpy(udst, &ret, blocksize);
		} else {
			(*udst++) = ret;
		}
		len += blocksize;
	}
}
#endif /* __GNUC__ */
