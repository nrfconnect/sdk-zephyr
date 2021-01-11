/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>

/**
 * \brief This symbol is the entry point provided by the PSA API compliance
 *        test libraries
 */
extern void val_entry(void);


void psa_test(void)
{
#if !defined(CONFIG_TFM_PSA_TEST_CRYPTO) \
	&& !defined(CONFIG_TFM_PSA_TEST_PROTECTED_STORAGE) \
	&& !defined(CONFIG_TFM_PSA_TEST_INTERNAL_TRUSTED_STORAGE) \
	&& !defined(CONFIG_TFM_PSA_TEST_STORAGE) \
	&& !defined(CONFIG_TFM_PSA_TEST_INITIAL_ATTESTATION)
	printk("No PSA test suite set. Use Kconfig to enable a test suite.\n");
#else
	val_entry();
#endif
}
