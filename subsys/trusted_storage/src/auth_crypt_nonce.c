/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <zephyr.h>
#include <zephyr/device.h>
#include <init.h>
#include <sys/util.h>

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>

#include "auth_crypt_nonce.h"

/*
 * NONCE is an incrementing 128bits number
 */

#define NONCE_MAX_LENGTH		16

static uint64_t g_nonce_low;
static uint64_t g_nonce_high;

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
	g_nonce_low++;

	/* On overflow */
	if (g_nonce_low == 0) {
		++g_nonce_high;
	}

	/* Copy low first */
	memcpy(nonce, &g_nonce_low, MIN(nonce_len, sizeof(g_nonce_low)));

	/* Then high is there's free space */
	if (nonce_len > sizeof(g_nonce_low)) {
		memcpy(nonce + sizeof(g_nonce_low), &g_nonce_high,
		       MIN(nonce_len - sizeof(g_nonce_low),
			   sizeof(g_nonce_high)));
	}

	return PSA_SUCCESS;
}

#if defined(CONFIG_TRUSTED_STORAGE_AUTH_CRYPT_NONCE_SEED_COUNTER_PSA)
#include <psa/crypto.h>

static int trusted_storage_nonce_init(const struct device *dev)
{
	psa_status_t status;

	ARG_UNUSED(dev);

	status = psa_crypto_init();
	if (status != PSA_SUCCESS) {
		return -EIO;
	}

	/* Set nonce to an initial random value */
	status = psa_generate_random((void *)&g_nonce_low,
				     sizeof(g_nonce_low));
	if (status != PSA_SUCCESS) {
		return -EIO;
	}

	status = psa_generate_random((void *)&g_nonce_high,
				     sizeof(g_nonce_high));
	if (status != PSA_SUCCESS) {
		return -EIO;
	}

	return 0;
}

SYS_INIT(trusted_storage_nonce_init, APPLICATION,
	 CONFIG_APPLICATION_INIT_PRIORITY);
#endif
