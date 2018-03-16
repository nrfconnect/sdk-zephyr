/*
 * Copyright (c) 2015 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <ztest.h>

void test_intmath(void)
{
	/*
	 * Declaring volatile so the compiler doesn't try to optimize any
	 * of the math away at build time
	 */
	volatile u64_t bignum, ba, bb;
	volatile u32_t num, a, b;

	ba = 0x00000012ABCDEF12ULL;
	bb = 0x0000001000000111ULL;
	bignum = ba * bb;
	zassert_true((bignum == 0xbcdf0509369bf232ULL), "64-bit multiplication failed");

	a = 30000;
	b = 5872;
	num = a * b;
	zassert_true((num == 176160000), "32-bit multiplication failed");

	a = 234424432;
	b = 98982;
	num = a / b;
	zassert_true((num == 2368), "32-bit division failed");
}
