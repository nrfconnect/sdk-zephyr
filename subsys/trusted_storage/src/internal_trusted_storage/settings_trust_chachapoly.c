/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <zephyr.h>
#include <zephyr/device.h>
#include <init.h>
#include <sys/util.h>
#include <random/rand32.h>

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include <mbedtls/chachapoly.h>

#include "settings_trust.h"

psa_status_t psa_its_get_settings_auth_crypt_init(void)
{
	return PSA_SUCCESS;
}

size_t psa_its_get_settings_auth_crypt_get_encrypted_size(size_t data_size)
{
	/* 16 bytes TAG followed by encrypted data */
	return 16 + data_size;
}

psa_status_t psa_its_get_settings_auth_crypt_encrypt(const void *key_buf,
					size_t key_len, const void *nonce_buf,
					size_t nonce_len, const void *add_buf,
					size_t add_len, const void *input_buf,
					size_t input_len, void *output_buf,
					size_t output_size, size_t *output_len)
{
	mbedtls_chachapoly_context ctx;
	uint8_t *tag, *data;
	uint8_t key[32];
	int ret;

	if (nonce_len < 12 || output_size < (input_len + 16)) {
		return PSA_ERROR_INVALID_ARGUMENT;
	}

	/* 16 bytes TAG followed by encrypted data */
	tag = (uint8_t *)output_buf;
	data = tag + 16;

	memcpy(key, key_buf, MAX(key_len, 32));

	/* Double the key if less than 32 bytes */
	if (key_len < 32) {
		memcpy(key + key_len, key_buf, 32 - key_len);
	}

	mbedtls_chachapoly_init(&ctx);

	ret = mbedtls_chachapoly_setkey(&ctx, key);
	if (ret < 0) {
		goto cleanup;
	}

	ret = mbedtls_chachapoly_encrypt_and_tag(&ctx, input_len, nonce_buf,
						 add_buf, add_len, input_buf,
						 data, tag);
	if (ret < 0) {
		goto cleanup;
	}

	*output_len = 16 + input_len;

cleanup:
	mbedtls_chachapoly_free(&ctx);

	if (ret < 0) {
		return PSA_ERROR_GENERIC_ERROR;
	}

	return PSA_SUCCESS;
}

psa_status_t psa_its_get_settings_auth_crypt_decrypt(const void *key_buf,
					size_t key_len, const void *nonce_buf,
					size_t nonce_len, const void *add_buf,
					size_t add_len, const void *input_buf,
					size_t input_len, void *output_buf,
					size_t output_size, size_t *output_len)
{
	mbedtls_chachapoly_context ctx;
	const uint8_t *tag, *data;
	size_t data_len;
	uint8_t key[32];
	int ret;

	if (nonce_len < 12 || input_len < 16  ||
	    output_size < (input_len - 16)) {
		return PSA_ERROR_INVALID_ARGUMENT;
	}

	/* 16 bytes TAG followed by encrypted data */
	tag = (uint8_t *)input_buf;
	data = tag + 16;
	data_len = input_len - 16;

	memcpy(key, key_buf, MAX(key_len, 32));

	/* Double the key if less than 32 bytes */
	if (key_len < 32) {
		memcpy(key + key_len, key_buf, 32 - key_len);
	}

	mbedtls_chachapoly_init(&ctx);

	ret = mbedtls_chachapoly_setkey(&ctx, key);
	if (ret < 0) {
		goto cleanup;
	}

	ret = mbedtls_chachapoly_auth_decrypt(&ctx, data_len, nonce_buf,
					      add_buf, add_len,
					      tag, data, output_buf);
	if (ret < 0) {
		goto cleanup;
	}

	*output_len = data_len;

cleanup:
	mbedtls_chachapoly_free(&ctx);

	if (ret == MBEDTLS_ERR_CHACHAPOLY_AUTH_FAILED) {
		return PSA_ERROR_INVALID_SIGNATURE;
	} else if (ret < 0) {
		return PSA_ERROR_GENERIC_ERROR;
	}

	return PSA_SUCCESS;
}
