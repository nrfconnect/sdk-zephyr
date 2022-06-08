/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdint.h>

#include <psa/crypto.h>

#include "auth_crypt_psa.h"

#define AEAD_TAG_SIZE		16
#define AEAD_PSA_AUTH_ALG						\
		PSA_ALG_AEAD_WITH_SHORTENED_TAG(PSA_ALG_GCM,		\
						AEAD_TAG_SIZE)
#define AEAD_PSA_KEY_TYPE	PSA_KEY_TYPE_AES

psa_status_t trusted_storage_auth_crypt_psa_init(void)
{
	return psa_crypto_init();
}

size_t trusted_storage_auth_crypt_psa_get_encrypted_size(size_t data_size)
{
	return PSA_AEAD_ENCRYPT_OUTPUT_SIZE(AEAD_PSA_KEY_TYPE,
					    AEAD_PSA_AUTH_ALG,
					    data_size);
}

static psa_status_t trusted_storage_auth_crypt_psa_crypt(
					psa_key_usage_t key_usage,
					const void *key_buf, size_t key_len,
					const void *nonce_buf, size_t nonce_len,
					const void *add_buf, size_t add_len,
					const void *input_buf, size_t input_len,
					void *output_buf, size_t output_size,
					size_t *output_len)
{
	psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;
	mbedtls_svc_key_id_t key = MBEDTLS_SVC_KEY_ID_INIT;
	psa_status_t status;

	psa_set_key_usage_flags(&attributes, key_usage);
	psa_set_key_algorithm(&attributes, AEAD_PSA_AUTH_ALG);
	psa_set_key_type(&attributes, AEAD_PSA_KEY_TYPE);

	status = psa_import_key(&attributes, key_buf, key_len, &key);
	if (status != PSA_SUCCESS) {
		return status;
	}

	if (key_usage == PSA_KEY_USAGE_ENCRYPT)
		status = psa_aead_encrypt(key, AEAD_PSA_AUTH_ALG, nonce_buf,
				nonce_len, add_buf, add_len, input_buf,
				input_len, output_buf, output_size, output_len);
	else
		status = psa_aead_decrypt(key, AEAD_PSA_AUTH_ALG, nonce_buf,
				nonce_len, add_buf, add_len, input_buf,
				input_len, output_buf, output_size, output_len);

	psa_destroy_key(key);

	return status;
}

psa_status_t trusted_storage_auth_crypt_psa_encrypt(const void *key_buf,
					size_t key_len, const void *nonce_buf,
					size_t nonce_len, const void *add_buf,
					size_t add_len, const void *input_buf,
					size_t input_len, void *output_buf,
					size_t output_size, size_t *output_len)
{
	return trusted_storage_auth_crypt_psa_crypt(PSA_KEY_USAGE_ENCRYPT,
						    key_buf, key_len,
						    nonce_buf, nonce_len,
						    add_buf, add_len,
						    input_buf, input_len,
						    output_buf, output_size,
						    output_len);
}

psa_status_t trusted_storage_auth_crypt_psa_decrypt(const void *key_buf,
					size_t key_len, const void *nonce_buf,
					size_t nonce_len, const void *add_buf,
					size_t add_len, const void *input_buf,
					size_t input_len, void *output_buf,
					size_t output_size, size_t *output_len)
{
	return trusted_storage_auth_crypt_psa_crypt(PSA_KEY_USAGE_DECRYPT,
						    key_buf, key_len,
						    nonce_buf, nonce_len,
						    add_buf, add_len,
						    input_buf, input_len,
						    output_buf, output_size,
						    output_len);
}
