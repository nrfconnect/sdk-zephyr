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

#include <psa/crypto.h>

#include "settings_backend.h"
#include "../settings_helpers.h"

/* SHA256 hash suffix */
#define TRUSTED_STORAGE_FILENAME_SUFFIX_HASH		".hash"

/*
 * SHA256 based integrity trust implementation
 *
 * Actual implementation uses:
 * - PSA Crypto SHA256 implementation
 */

static uint8_t object_data[TRUSTED_STORAGE_MAX_ASSET_SIZE];

static int psa_ps_settings_integrity_init(const struct device *dev)
{
	psa_status_t status;

	ARG_UNUSED(dev);

	status = psa_crypto_init();
	if (status != PSA_SUCCESS) {
		return -EIO;
	}

	return 0;
}

SYS_INIT(psa_ps_settings_integrity_init, APPLICATION,
	 CONFIG_APPLICATION_INIT_PRIORITY);

psa_status_t psa_ps_get_settings_trusted(psa_storage_uid_t uid,
				const char *prefix, size_t data_size,
				size_t data_offset,
				size_t data_length, void *p_data,
				size_t *p_data_length,
				psa_storage_create_flags_t create_flags)
{
	psa_status_t status = PSA_ERROR_CORRUPTION_DETECTED;
	uint8_t hash[PSA_HASH_LENGTH(PSA_ALG_SHA_256)];
	int ret;

	ARG_UNUSED(create_flags);

	ret = trusted_storage_get_object(uid, prefix,
					 TRUSTED_STORAGE_FILENAME_SUFFIX_HASH,
				hash, sizeof(hash));
	if (ret == -ENOENT || ret == -ENODATA) {
		return PSA_ERROR_DATA_CORRUPT;
	} else if (ret < 0) {
		return PSA_ERROR_STORAGE_FAILURE;
	}

	ret = trusted_storage_get_object(uid, prefix,
					 TRUSTED_STORAGE_FILENAME_SUFFIX_DATA,
					 object_data, data_size);
	if (ret == -ENOENT || ret == -ENODATA) {
		return PSA_ERROR_DATA_CORRUPT;
	} else if (ret < 0) {
		return PSA_ERROR_STORAGE_FAILURE;
	}

	status = psa_hash_compare(PSA_ALG_SHA_256, object_data, data_size,
				  hash, sizeof(hash));
	if (status != PSA_SUCCESS) {
		return status;
	}

	memcpy(p_data, object_data + data_offset, data_length);
	*p_data_length = data_length;

	memset(object_data, 0, sizeof(object_data));

	return PSA_SUCCESS;
}

psa_status_t psa_ps_set_settings_trusted(psa_storage_uid_t uid,
				const char *prefix, size_t data_length,
				const void *p_data,
				psa_storage_create_flags_t create_flags)
{
	psa_status_t status = PSA_ERROR_CORRUPTION_DETECTED;
	uint8_t hash[PSA_HASH_LENGTH(PSA_ALG_SHA_256)];
	size_t hash_length;
	int ret;

	ARG_UNUSED(create_flags);

	status = psa_hash_compute(PSA_ALG_SHA_256, p_data, data_length,
				  hash, sizeof(hash), &hash_length);
	if (status != PSA_SUCCESS) {
		return status;
	}

	/* Write hash */
	ret = trusted_storage_set_object(uid, prefix,
					 TRUSTED_STORAGE_FILENAME_SUFFIX_HASH,
					 hash, hash_length);
	if (ret < 0) {
		status = PSA_ERROR_STORAGE_FAILURE;
		goto cleanup_objects;
	}

	/* Write data */
	ret = trusted_storage_set_object(uid, prefix,
					 TRUSTED_STORAGE_FILENAME_SUFFIX_DATA,
					 p_data, data_length);
	if (ret < 0) {
		status = PSA_ERROR_STORAGE_FAILURE;
		goto cleanup_objects;
	}

	return PSA_SUCCESS;

cleanup_objects:
	/* Remove all object if an error occurs */
	trusted_storage_remove_object(uid, prefix,
				      TRUSTED_STORAGE_FILENAME_SUFFIX_HASH);
	trusted_storage_remove_object(uid, prefix,
				      TRUSTED_STORAGE_FILENAME_SUFFIX_DATA);

	return status;
}

psa_status_t psa_ps_remove_settings_trusted(psa_storage_uid_t uid,
				const char *prefix,
				psa_storage_create_flags_t create_flags)
{
	int ret;

	ARG_UNUSED(create_flags);

	ret = trusted_storage_remove_object(uid, prefix,
					TRUSTED_STORAGE_FILENAME_SUFFIX_HASH);
	if (ret == -ENOENT || ret == -ENODATA) {
		return PSA_ERROR_DATA_CORRUPT;
	} else if (ret < 0) {
		return PSA_ERROR_STORAGE_FAILURE;
	}

	ret = trusted_storage_remove_object(uid, prefix,
					TRUSTED_STORAGE_FILENAME_SUFFIX_DATA);
	if (ret == -ENOENT || ret == -ENODATA) {
		return PSA_ERROR_DATA_CORRUPT;
	} else if (ret < 0) {
		return PSA_ERROR_STORAGE_FAILURE;
	}

	return PSA_SUCCESS;
}
