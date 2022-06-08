/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __TRUSTED_STORAGE_AUTH_CRYPT_BACKEND_H_
#define __TRUSTED_STORAGE_AUTH_CRYPT_BACKEND_H_

#include <psa/error.h>
#include <psa/storage_common.h>

/* Initialize the Auth/Crypt crypto backend */
typedef psa_status_t (*trusted_storage_auth_crypt_init_cb_t)(void);

/* Get encrypted buffer size */
typedef size_t (*trusted_storage_auth_crypt_get_encrypted_size_cb_t)(
					size_t data_size);

/* Decrypt encrypted buffer with provided auth data */
typedef psa_status_t (*trusted_storage_auth_crypt_decrypt_cb_t)(
					const void *key_buf, size_t key_len,
					const void *nonce_buf, size_t nonce_len,
					const void *add_buf, size_t add_len,
					const void *input_buf, size_t input_len,
					void *output_buf,
					size_t output_size,
					size_t *output_len);

/* Encrypt clear buffer with provided auth data */
typedef psa_status_t (*trusted_storage_auth_crypt_encrypt_cb_t)(
					const void *key_buf, size_t key_len,
					const void *nonce_buf, size_t nonce_len,
					const void *add_buf, size_t add_len,
					const void *input_buf, size_t input_len,
					void *output_buf,
					size_t output_size,
					size_t *output_len);

/* Initialize the Auth/Crypt backend */
psa_status_t trusted_storage_auth_crypt_backend_init(
				trusted_storage_auth_crypt_init_cb_t init_cb);

/*
 * Load & validate the data within the trust implementation
 * Object flags has already been checked by the caller.
 * Returns 0 or a negative PSA error value if an error occurs.
 */
psa_status_t trusted_storage_auth_crypt_backend_get(psa_storage_uid_t uid,
			const char *prefix, size_t data_size,
			size_t data_offset, size_t data_length,
			void *p_data, size_t *p_data_length,
			psa_storage_create_flags_t create_flags,
			trusted_storage_auth_crypt_get_encrypted_size_cb_t
				get_encrypted_size_cb,
			trusted_storage_auth_crypt_encrypt_cb_t crypt_cb);

/*
 * Stores & authenticates the data within the trust implementation
 * Returns 0 or a negative PSA error value if an error occurs.
 */
psa_status_t trusted_storage_auth_crypt_backend_set(psa_storage_uid_t uid,
			const char *prefix, size_t data_length,
			const void *p_data,
			psa_storage_create_flags_t create_flags,
			trusted_storage_auth_crypt_encrypt_cb_t crypt_cb);

/*
 * Removes data and metadata stored by the trust implementation
 * Returns 0 or a negative PSA error value if an error occurs.
 */
psa_status_t trusted_storage_auth_crypt_backend_remove(psa_storage_uid_t uid,
				const char *prefix,
				psa_storage_create_flags_t create_flags);

#endif /* __TRUSTED_STORAGE_AUTH_CRYPT_BACKEND_H_ */
