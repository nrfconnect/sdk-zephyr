/*
 * Copyright (c) 2021 Nordic Semiconductor ASA.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>

/* Run the PSA test suite */
void psa_test(void);

__attribute__((noreturn))
void main(void)
{
	psa_test();

	for (;;) {
	}
}
