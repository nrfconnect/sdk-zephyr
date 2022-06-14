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
#include "settings_trust.h"

#include "../auth_crypt_backend.h"

static int psa_its_settings_auth_crypt_init(const struct device *dev)
{
	psa_status_t status;

	ARG_UNUSED(dev);

	status = trusted_storage_auth_crypt_backend_init(
			psa_its_get_settings_auth_crypt_init);
	if (status != PSA_SUCCESS) {
		return -EIO;
	}

	return 0;
}

SYS_INIT(psa_its_settings_auth_crypt_init, APPLICATION,
	 CONFIG_APPLICATION_INIT_PRIORITY);

psa_status_t psa_its_get_settings_trusted(psa_storage_uid_t uid,
				const char *prefix, size_t data_size,
				size_t data_offset,
				size_t data_length, void *p_data,
				size_t *p_data_length,
				psa_storage_create_flags_t create_flags)
{
	return trusted_storage_auth_crypt_backend_get(uid, prefix, data_size,
			data_offset, data_length, p_data,
			p_data_length, create_flags,
			psa_its_get_settings_auth_crypt_get_encrypted_size,
			psa_its_get_settings_auth_crypt_decrypt);
}

psa_status_t psa_its_set_settings_trusted(psa_storage_uid_t uid,
				const char *prefix, size_t data_length,
				const void *p_data,
				psa_storage_create_flags_t create_flags)
{
	return trusted_storage_auth_crypt_backend_set(uid, prefix, data_length,
				p_data, create_flags,
				psa_its_get_settings_auth_crypt_encrypt);
}

psa_status_t psa_its_remove_settings_trusted(psa_storage_uid_t uid,
				const char *prefix,
				psa_storage_create_flags_t create_flags)
{
	return trusted_storage_auth_crypt_backend_remove(uid, prefix,
							 create_flags);
}
