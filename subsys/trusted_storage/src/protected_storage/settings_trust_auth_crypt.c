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

#include "settings_backend.h"
#include "settings_trust_auth_crypt.h"

#include "../auth_crypt_backend.h"

/* Protected Storage wrapper on top on Trusted Storage AEAD with PSA Crypto */

#if defined(CONFIG_PROTECTED_STORAGE_SETTINGS_TRUST_AUTH_CRYPT_PSA)

#include "../auth_crypt_psa.h"

psa_status_t psa_ps_get_settings_auth_crypt_init(void)
{
	return trusted_storage_auth_crypt_psa_init();
}

size_t psa_ps_get_settings_auth_crypt_get_encrypted_size(size_t data_size)
{
	return trusted_storage_auth_crypt_psa_get_encrypted_size(data_size);
}

psa_status_t psa_ps_get_settings_auth_crypt_encrypt(const void *key_buf,
					size_t key_len, const void *nonce_buf,
					size_t nonce_len, const void *add_buf,
					size_t add_len, const void *input_buf,
					size_t input_len, void *output_buf,
					size_t output_size, size_t *output_len)
{
	return trusted_storage_auth_crypt_psa_encrypt(key_buf, key_len, nonce_buf,
						      nonce_len, add_buf, add_len,
						      input_buf, input_len,
						      output_buf, output_size,
						      output_len);
}

psa_status_t psa_ps_get_settings_auth_crypt_decrypt(const void *key_buf,
					size_t key_len, const void *nonce_buf,
					size_t nonce_len, const void *add_buf,
					size_t add_len, const void *input_buf,
					size_t input_len, void *output_buf,
					size_t output_size, size_t *output_len)
{
	return trusted_storage_auth_crypt_psa_decrypt(key_buf, key_len, nonce_buf,
						      nonce_len, add_buf, add_len,
						      input_buf, input_len,
						      output_buf, output_size,
						      output_len);
}
#endif

static int psa_ps_settings_auth_crypt_init(const struct device *dev)
{
	psa_status_t status;

	ARG_UNUSED(dev);

	status = trusted_storage_auth_crypt_backend_init(
			psa_ps_get_settings_auth_crypt_init);
	if (status != PSA_SUCCESS) {
		return -EIO;
	}

	return 0;
}

SYS_INIT(psa_ps_settings_auth_crypt_init, APPLICATION,
	 CONFIG_APPLICATION_INIT_PRIORITY);

psa_status_t psa_ps_get_settings_trusted(psa_storage_uid_t uid,
				const char *prefix, size_t data_size,
				size_t data_offset,
				size_t data_length, void *p_data,
				size_t *p_data_length,
				psa_storage_create_flags_t create_flags)
{
	return trusted_storage_auth_crypt_backend_get(uid, prefix, data_size,
			data_offset, data_length, p_data,
			p_data_length, create_flags,
			psa_ps_get_settings_auth_crypt_get_encrypted_size,
			psa_ps_get_settings_auth_crypt_decrypt);
}

psa_status_t psa_ps_set_settings_trusted(psa_storage_uid_t uid,
				const char *prefix, size_t data_length,
				const void *p_data,
				psa_storage_create_flags_t create_flags)
{
	return trusted_storage_auth_crypt_backend_set(uid, prefix, data_length,
				p_data, create_flags,
				psa_ps_get_settings_auth_crypt_encrypt);
}

psa_status_t psa_ps_remove_settings_trusted(psa_storage_uid_t uid,
				const char *prefix,
				psa_storage_create_flags_t create_flags)
{
	return trusted_storage_auth_crypt_backend_remove(uid, prefix,
							 create_flags);
}
