/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <zephyr.h>
#include <init.h>
#include <settings/settings.h>
#include <sys/util.h>
#include <logging/log.h>

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>

#include "settings_backend.h"
#include "../settings_helpers.h"

LOG_MODULE_REGISTER(protected_storage_settings,
		    CONFIG_TRUSTED_STORAGE_LOG_LEVEL);

/* PSA Protected Storage with Zephyr Settings Backend */

#if defined(CONFIG_PROTECTED_STORAGE_SETTINGS_TRUST_NONE)
static uint8_t object_data[TRUSTED_STORAGE_MAX_ASSET_SIZE];

psa_status_t psa_ps_get_settings_trusted(psa_storage_uid_t uid,
				const char *prefix,
				size_t data_size, size_t data_offset,
				size_t data_length, void *p_data,
				size_t *p_data_length,
				psa_storage_create_flags_t create_flags)
{
	int ret;

	ARG_UNUSED(create_flags);

	ret = trusted_storage_get_object(uid, prefix,
					 TRUSTED_STORAGE_FILENAME_SUFFIX_DATA,
					 object_data, data_size);
	if (ret == -ENOENT || ret == -ENODATA) {
		return PSA_ERROR_DATA_CORRUPT;
	} else if (ret < 0) {
		return PSA_ERROR_STORAGE_FAILURE;
	}

	memcpy(p_data, object_data + data_offset, data_length);
	*p_data_length = data_length;

	memset(object_data, 0, sizeof(object_data));

	return PSA_SUCCESS;
}

psa_status_t psa_ps_set_settings_trusted(psa_storage_uid_t uid,
				const char *prefix,  size_t data_length,
				const void *p_data,
				psa_storage_create_flags_t create_flags)
{
	psa_status_t status;
	int ret;

	ARG_UNUSED(create_flags);

	ret = trusted_storage_set_object(uid, PS_STORAGE_FILENAME_PREFIX,
					 TRUSTED_STORAGE_FILENAME_SUFFIX_DATA,
					 p_data, data_length);
	if (ret < 0) {
		status = PSA_ERROR_STORAGE_FAILURE;
		goto cleanup_objects;
	}

	return PSA_SUCCESS;

cleanup_objects:
	trusted_storage_remove_object(uid, PS_STORAGE_FILENAME_PREFIX,
				      TRUSTED_STORAGE_FILENAME_SUFFIX_DATA);

	return status;
}

psa_status_t psa_ps_remove_settings_trusted(psa_storage_uid_t uid,
				const char *prefix,
				psa_storage_create_flags_t create_flags)
{
	int ret;

	ARG_UNUSED(create_flags);

	ret = trusted_storage_remove_object(uid, PS_STORAGE_FILENAME_PREFIX,
					    TRUSTED_STORAGE_FILENAME_SUFFIX_DATA);
	if (ret == -ENOENT || ret == -ENODATA) {
		return PSA_ERROR_DATA_CORRUPT;
	} else if (ret < 0) {
		return PSA_ERROR_STORAGE_FAILURE;
	}

	return PSA_SUCCESS;
}
#endif

psa_status_t psa_ps_get_info_backend(psa_storage_uid_t uid,
				     struct psa_storage_info_t *p_info)
{
	return trusted_storage_get_info(uid, PS_STORAGE_FILENAME_PREFIX,
					p_info);
}

psa_status_t psa_ps_get_backend(psa_storage_uid_t uid, size_t data_offset,
				size_t data_length, void *p_data,
				size_t *p_data_length)
{
	return trusted_storage_get(uid, PS_STORAGE_FILENAME_PREFIX, data_offset,
				   data_length, p_data, p_data_length,
				   psa_ps_get_settings_trusted);
}

psa_status_t psa_ps_set_backend(psa_storage_uid_t uid,
				size_t data_length,
				const void *p_data,
				psa_storage_create_flags_t create_flags)
{
	return trusted_storage_set(uid, PS_STORAGE_FILENAME_PREFIX,
				   data_length, p_data, create_flags,
				   psa_ps_set_settings_trusted);
}

psa_status_t psa_ps_remove_backend(psa_storage_uid_t uid)
{
	return trusted_storage_remove(uid, PS_STORAGE_FILENAME_PREFIX,
				      psa_ps_remove_settings_trusted);
}

uint32_t psa_ps_get_support_backend(void)
{
	return 0;
}

psa_status_t psa_ps_create_backend(psa_storage_uid_t uid,
				   size_t capacity,
				   psa_storage_create_flags_t create_flags)
{
	return PSA_ERROR_NOT_SUPPORTED;
}

psa_status_t psa_ps_set_extended_backend(psa_storage_uid_t uid,
					 size_t data_offset,
					 size_t data_length,
					 const void *p_data)
{
	return PSA_ERROR_NOT_SUPPORTED;
}

static int psa_ps_settings_init(const struct device *dev)
{
	int ret;

	ARG_UNUSED(dev);

	ret = settings_subsys_init();
	if (ret != 0) {
		LOG_ERR("%s failed (ret %d)", __func__, ret);
	}

	return ret;
}

SYS_INIT(psa_ps_settings_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
