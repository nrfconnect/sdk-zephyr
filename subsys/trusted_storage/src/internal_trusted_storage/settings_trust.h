/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __INTERNAL_TRUSTED_STORAGE_SETTINGS_TRUST_H_
#define __INTERNAL_TRUSTED_STORAGE_SETTINGS_TRUST_H_

#include <psa/error.h>

psa_status_t psa_its_get_settings_auth_crypt_init(void);

size_t psa_its_get_settings_auth_crypt_get_encrypted_size(size_t data_size);

psa_status_t psa_its_get_settings_auth_crypt_encrypt(const void *key_buf,
					size_t key_len, const void *nonce_buf,
					size_t nonce_len, const void *add_buf,
					size_t add_len, const void *input_buf,
					size_t input_len, void *output_buf,
					size_t output_size, size_t *output_len);

psa_status_t psa_its_get_settings_auth_crypt_decrypt(const void *key_buf,
					size_t key_len, const void *nonce_buf,
					size_t nonce_len, const void *add_buf,
					size_t add_len, const void *input_buf,
					size_t input_len, void *output_buf,
					size_t output_size, size_t *output_len);

#endif /* __INTERNAL_TRUSTED_STORAGE_SETTINGS_TRUST_H_ */
