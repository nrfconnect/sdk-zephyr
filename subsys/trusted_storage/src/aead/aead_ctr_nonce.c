/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <psa/crypto.h>
#include <zephyr/device.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/util.h>

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>

#include "aead_nonce.h"

/*
 * Nonce is a 128 bits counter.
 */

#define NONCE_MAX_LENGTH 16

static struct aead_ctr_nonce {
	uint64_t low;
	uint64_t high;
} g_nonce;

/* Return an incrementing nonce */
psa_status_t trusted_storage_get_nonce(uint8_t *nonce, size_t nonce_len)
{
	if (nonce == NULL) {
		return PSA_ERROR_INVALID_ARGUMENT;
	}

	if (nonce_len > NONCE_MAX_LENGTH) {
		return PSA_ERROR_NOT_SUPPORTED;
	}

	if (nonce_len == 0) {
		return PSA_SUCCESS;
	}

	/* Incrementing is implemented by using 2 64bit numbers */
	g_nonce.low++;

	/* On overflow */
	if (g_nonce.low == 0) {
		++g_nonce.high;
	}

	/* Copy low first */
	memcpy(nonce, &g_nonce.low, MIN(nonce_len, sizeof(g_nonce.low)));

	/* Then high if there's free space */
	if (nonce_len > sizeof(g_nonce.low)) {
		memcpy(nonce + sizeof(g_nonce.low), &g_nonce.high,
		       MIN(nonce_len - sizeof(g_nonce.low), sizeof(g_nonce.high)));
	}

	return PSA_SUCCESS;
}

static int trusted_storage_nonce_init(void)
{
	psa_status_t status;

	status = psa_crypto_init();
	if (status != PSA_SUCCESS) {
		return -EIO;
	}

	/* Initialize nonce to a random value. */
	status = psa_generate_random((void *)&g_nonce, sizeof(g_nonce));
	if (status != PSA_SUCCESS) {
		return -EIO;
	}

	return 0;
}

SYS_INIT(trusted_storage_nonce_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
