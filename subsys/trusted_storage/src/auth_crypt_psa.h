/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __TRUSTED_STORAGE_AUTH_CRYPT_PSA_H_
#define __TRUSTED_STORAGE_AUTH_CRYPT_PSA_H_

#include <psa/error.h>

psa_status_t trusted_storage_auth_crypt_psa_init(void);

size_t trusted_storage_auth_crypt_psa_get_encrypted_size(size_t data_size);

psa_status_t trusted_storage_auth_crypt_psa_encrypt(const void *key_buf,
					size_t key_len, const void *nonce_buf,
					size_t nonce_len, const void *add_buf,
					size_t add_len, const void *input_buf,
					size_t input_len, void *output_buf,
					size_t output_size, size_t *output_len);

psa_status_t trusted_storage_auth_crypt_psa_decrypt(const void *key_buf,
					size_t key_len, const void *nonce_buf,
					size_t nonce_len, const void *add_buf,
					size_t add_len, const void *input_buf,
					size_t input_len, void *output_buf,
					size_t output_size, size_t *output_len);

#endif /* __TRUSTED_STORAGE_AUTH_CRYPT_PSA_H_ */
