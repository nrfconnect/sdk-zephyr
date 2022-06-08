/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __PROTECTED_STORAGE_SETTINGS_TRUST_AUTH_CRYPT_H_
#define __PROTECTED_STORAGE_SETTINGS_TRUST_AUTH_CRYPT_H_

#include <psa/error.h>

psa_status_t psa_ps_get_settings_auth_crypt_init(void);

psa_status_t psa_ps_get_settings_auth_crypt_get_random(void *buf,
						      size_t length);

size_t psa_ps_get_settings_auth_crypt_get_encrypted_size(size_t data_size);

psa_status_t psa_ps_get_settings_auth_crypt_encrypt(const void *key_buf,
					size_t key_len, const void *nonce_buf,
					size_t nonce_len, const void *add_buf,
					size_t add_len, const void *input_buf,
					size_t input_len, void *output_buf,
					size_t output_size, size_t *output_len);

psa_status_t psa_ps_get_settings_auth_crypt_decrypt(const void *key_buf,
					size_t key_len, const void *nonce_buf,
					size_t nonce_len, const void *add_buf,
					size_t add_len, const void *input_buf,
					size_t input_len, void *output_buf,
					size_t output_size, size_t *output_len);

#endif /* __PROTECTED_STORAGE_SETTINGS_TRUST_AUTH_CRYPT_H_ */
