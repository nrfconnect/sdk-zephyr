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

#include "settings_helpers.h"
#include "auth_crypt_backend.h"
#include "auth_crypt_nonce.h"

/* AEAD NONCE suffix */
#define TRUSTED_STORAGE_FILENAME_SUFFIX_NONCE		".nonce"

/*
 * AEAD based Authenticated Encrypted trust implementation
 *
 * Actual implementation uses:
 * - Hexadecimal UID imported as KEY
 * - UID+Flags+Size as additional parameter
 * - NONCE is an incrementing number at each encryption
 * - TAG is left at the end of output data
 */

#define AEAD_TAG_SIZE		16
#define AEAD_NONCE_SIZE		12

/* Max storage size for the encrypted or decrypted output */
#define AEAD_MAX_BUF_SIZE ROUND_UP(TRUSTED_STORAGE_MAX_ASSET_SIZE +	\
				   AEAD_TAG_SIZE, AEAD_TAG_SIZE)

/* 128bits AEAD key pattern: uid low, uid */
#define AEAD_KEY_PATTERN	"%08x%08x"
#define AEAD_KEY_SIZE		(sizeof(uint64_t) * 2)

/* Additional data structure */
struct aead_additional_data {
	psa_storage_uid_t uid;
	psa_storage_create_flags_t flags;
	size_t size;
};

/* Temporary AEAD encryption/decryption buffers */
static uint8_t aead_buf[AEAD_MAX_BUF_SIZE];
static uint8_t data_buf[TRUSTED_STORAGE_MAX_ASSET_SIZE];

psa_status_t trusted_storage_auth_crypt_backend_init(
				trusted_storage_auth_crypt_init_cb_t init_cb)
{
	psa_status_t status;

	if (init_cb) {
		status = init_cb();
		if (status != PSA_SUCCESS) {
			return -EIO;
		}
	}

	return 0;
}

psa_status_t trusted_storage_auth_crypt_backend_get(psa_storage_uid_t uid,
			const char *prefix, size_t data_size,
			size_t data_offset, size_t data_length,
			void *p_data, size_t *p_data_length,
			psa_storage_create_flags_t create_flags,
			trusted_storage_auth_crypt_get_encrypted_size_cb_t
				get_encrypted_size_cb,
			trusted_storage_auth_crypt_encrypt_cb_t crypt_cb)
{
	struct aead_additional_data additional_data;
	uint8_t key_buf[AEAD_KEY_SIZE + 1];
	uint8_t nonce[AEAD_NONCE_SIZE];
	size_t object_data_size;
	size_t aead_out_size;
	psa_status_t status;
	int ret;

	/* Calculate the exact output size of encrypted buffer */
	object_data_size = get_encrypted_size_cb(data_size);

	if (object_data_size > AEAD_MAX_BUF_SIZE) {
		return PSA_ERROR_NOT_SUPPORTED;
	}

	ret = trusted_storage_get_object(uid, prefix,
					 TRUSTED_STORAGE_FILENAME_SUFFIX_NONCE,
					 nonce, sizeof(nonce));
	if (ret == -ENOENT || ret == -ENODATA) {
		return PSA_ERROR_DATA_CORRUPT;
	} else if (ret < 0) {
		return PSA_ERROR_STORAGE_FAILURE;
	}

	ret = trusted_storage_get_object(uid, prefix,
					 TRUSTED_STORAGE_FILENAME_SUFFIX_DATA,
					 aead_buf, object_data_size);
	if (ret == -ENOENT || ret == -ENODATA) {
		return PSA_ERROR_DATA_CORRUPT;
	} else if (ret < 0) {
		return PSA_ERROR_STORAGE_FAILURE;
	}

	/* Key is ASCII version of UID */
	snprintf(key_buf, AEAD_KEY_SIZE + 1, AEAD_KEY_PATTERN,
		 (unsigned int)((uid) >> 32),
		 (unsigned int)((uid) & 0xffffffff));

	additional_data.uid = uid;
	additional_data.flags = create_flags;
	additional_data.size = data_length;

	status = crypt_cb(key_buf, AEAD_KEY_SIZE, nonce, AEAD_NONCE_SIZE,
			 (void *)&additional_data, sizeof(additional_data),
			 aead_buf, object_data_size,
			 data_buf, TRUSTED_STORAGE_MAX_ASSET_SIZE,
			 &aead_out_size);

	memset(key_buf, 0, AEAD_KEY_SIZE);
	memset(nonce, 0, sizeof(nonce));
	memset(&additional_data, 0, sizeof(additional_data));
	memset(aead_buf, 0, AEAD_MAX_BUF_SIZE);

	if (status != PSA_SUCCESS) {
		return status;
	}

	if ((data_offset + data_length) > aead_out_size) {
		memset(data_buf, 0, TRUSTED_STORAGE_MAX_ASSET_SIZE);
		return PSA_ERROR_INVALID_SIGNATURE;
	}

	memcpy(p_data, data_buf + data_offset, data_length);

	memset(data_buf, 0, TRUSTED_STORAGE_MAX_ASSET_SIZE);

	return PSA_SUCCESS;
}

psa_status_t trusted_storage_auth_crypt_backend_set(psa_storage_uid_t uid,
			const char *prefix, size_t data_length,
			const void *p_data,
			psa_storage_create_flags_t create_flags,
			trusted_storage_auth_crypt_encrypt_cb_t crypt_cb)
{
	uint8_t key_buf[AEAD_KEY_SIZE + 1];
	uint8_t nonce[AEAD_NONCE_SIZE];
	struct aead_additional_data additional_data;
	size_t aead_out_size;
	psa_status_t status;
	int ret;

	/* Key is ASCII version of UID */
	snprintf(key_buf, AEAD_KEY_SIZE + 1, AEAD_KEY_PATTERN,
		 (unsigned int)((uid) >> 32),
		 (unsigned int)((uid) & 0xffffffff));

	/* Get new nonce at each set */
	trusted_storage_get_nonce(nonce, AEAD_NONCE_SIZE);

	additional_data.uid = uid;
	additional_data.flags = create_flags;
	additional_data.size = data_length;

	status = crypt_cb(key_buf, AEAD_KEY_SIZE, nonce, AEAD_NONCE_SIZE,
			  (void *)&additional_data, sizeof(additional_data),
			  p_data, data_length,
			  aead_buf, AEAD_MAX_BUF_SIZE,
			  &aead_out_size);

	memset(key_buf, 0, AEAD_KEY_SIZE);

	if (status != PSA_SUCCESS) {
		goto cleanup;
	}

	/* Write nonce */
	ret = trusted_storage_set_object(uid, prefix,
					 TRUSTED_STORAGE_FILENAME_SUFFIX_NONCE,
					 nonce, AEAD_NONCE_SIZE);
	if (ret < 0) {
		status = PSA_ERROR_STORAGE_FAILURE;
		goto cleanup_objects;
	}

	/* Write data (with embedded tag) */
	ret = trusted_storage_set_object(uid, prefix,
					 TRUSTED_STORAGE_FILENAME_SUFFIX_DATA,
					 aead_buf, aead_out_size);
	if (ret < 0) {
		status = PSA_ERROR_STORAGE_FAILURE;
		goto cleanup_objects;
	}

	return PSA_SUCCESS;

cleanup_objects:
	/* Remove all object if an error occurs */
	trusted_storage_remove_object(uid, prefix,
				      TRUSTED_STORAGE_FILENAME_SUFFIX_NONCE);
	trusted_storage_remove_object(uid, prefix,
				      TRUSTED_STORAGE_FILENAME_SUFFIX_DATA);

cleanup:
	memset(&additional_data, 0, sizeof(additional_data));
	memset(nonce, 0, sizeof(nonce));
	memset(aead_buf, 0, AEAD_MAX_BUF_SIZE);

	return status;
}

psa_status_t trusted_storage_auth_crypt_backend_remove(psa_storage_uid_t uid,
				const char *prefix,
				psa_storage_create_flags_t create_flags)
{
	int ret;

	ARG_UNUSED(create_flags);

	ret = trusted_storage_remove_object(uid, prefix,
					TRUSTED_STORAGE_FILENAME_SUFFIX_NONCE);
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
