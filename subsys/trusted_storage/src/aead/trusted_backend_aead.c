/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/sys/util.h>
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(internal_trusted_trusted_aead, CONFIG_PSA_TRUSTED_STORAGE_LOG_LEVEL);

#include <string.h>

#include "../trusted_backend.h"
#include "../storage_backend.h"
#include "aead_key.h"
#include "aead_nonce.h"
#include "aead_crypt.h"

/* Common data and metadata suffixes. */
#define TRUSTED_STORAGE_AEAD_BACKEND_FILENAME_SUFFIX_SIZE  ".size"
#define TRUSTED_STORAGE_AEAD_BACKEND_FILENAME_SUFFIX_FLAGS ".flags"
#define TRUSTED_STORAGE_AEAD_BACKEND_FILENAME_SUFFIX_DATA  ".data"
#define TRUSTED_STORAGE_AEAD_BACKEND_FILENAME_SUFFIX_NONCE ".nonce"

#define TRUSTED_STORAGE_MAX_ASSET_SIZE CONFIG_TRUSTED_STORAGE_TRUSTED_AEAD_BACKEND_MAX_DATA_SIZE

/*
 * AEAD based Authenticated Encrypted trust implementation
 *
 * Actual implementation uses:
 * - Hexadecimal UID imported as KEY
 * - UID+Flags+Size as additional parameter
 * - Nonce is a number that is incremented for each encryption.
 * - Tag is left at the end of output data
 */

#define AEAD_TAG_SIZE   16
#define AEAD_NONCE_SIZE 12

/* Max storage size for the encrypted or decrypted output */
#define AEAD_MAX_BUF_SIZE ROUND_UP(TRUSTED_STORAGE_MAX_ASSET_SIZE + AEAD_TAG_SIZE, AEAD_TAG_SIZE)

/* Additional data structure */
struct aead_additional_data {
	psa_storage_uid_t uid;
	psa_storage_create_flags_t flags;
	size_t size;
};

/* Temporary AEAD encryption/decryption buffers */
static uint8_t aead_buf[AEAD_MAX_BUF_SIZE];
static uint8_t data_buf[TRUSTED_STORAGE_MAX_ASSET_SIZE];

psa_status_t trusted_get_info(const psa_storage_uid_t uid, const char *prefix,
			      struct psa_storage_info_t *p_info)
{
	psa_storage_create_flags_t data_flags;
	size_t data_size;
	psa_status_t status;

	if (p_info == NULL) {
		return PSA_ERROR_INVALID_ARGUMENT;
	}

	/* Get size & flags */
	status = storage_get_object(uid, prefix, TRUSTED_STORAGE_AEAD_BACKEND_FILENAME_SUFFIX_FLAGS,
				    (void *)&data_flags, sizeof(data_flags));
	if (status != PSA_SUCCESS) {
		return status;
	}

	status = storage_get_object(uid, prefix, TRUSTED_STORAGE_AEAD_BACKEND_FILENAME_SUFFIX_SIZE,
				    (void *)&data_size, sizeof(data_size));
	if (status != PSA_SUCCESS) {
		return status;
	}

	p_info->capacity = TRUSTED_STORAGE_MAX_ASSET_SIZE;
	p_info->size = data_size;
	p_info->flags = data_flags;

	return PSA_SUCCESS;
}

psa_status_t trusted_get(const psa_storage_uid_t uid, const char *prefix, size_t data_offset,
			 size_t data_length, void *p_data, size_t *p_data_length)
{
	psa_status_t status = PSA_ERROR_CORRUPTION_DETECTED;
	psa_storage_create_flags_t data_flags;
	struct aead_additional_data additional_data;
	uint8_t key_buf[AEAD_KEY_SIZE + 1];
	uint8_t nonce[AEAD_NONCE_SIZE];
	size_t object_data_size;
	size_t aead_out_size;
	size_t data_size;

	if (data_length == 0 || p_data == NULL || p_data_length == NULL) {
		return PSA_ERROR_INVALID_ARGUMENT;
	}

	if ((data_offset + data_length) > TRUSTED_STORAGE_MAX_ASSET_SIZE) {
		return PSA_ERROR_NOT_SUPPORTED;
	}

	/* Get flags then size */
	status = storage_get_object(uid, prefix, TRUSTED_STORAGE_AEAD_BACKEND_FILENAME_SUFFIX_FLAGS,
				    (void *)&data_flags, sizeof(data_flags));
	if (status != PSA_SUCCESS) {
		return status;
	}

	status = storage_get_object(uid, prefix, TRUSTED_STORAGE_AEAD_BACKEND_FILENAME_SUFFIX_SIZE,
				    (void *)&data_size, sizeof(data_size));
	if (status != PSA_SUCCESS) {
		return status;
	}

	/* Calculate the exact output size of encrypted buffer */
	object_data_size = trusted_storage_aead_get_encrypted_size(data_size);

	if (object_data_size > AEAD_MAX_BUF_SIZE) {
		return PSA_ERROR_NOT_SUPPORTED;
	}

	status = storage_get_object(uid, prefix, TRUSTED_STORAGE_AEAD_BACKEND_FILENAME_SUFFIX_NONCE,
				    nonce, sizeof(nonce));
	if (status != PSA_SUCCESS) {
		return status;
	}

	status = storage_get_object(uid, prefix, TRUSTED_STORAGE_AEAD_BACKEND_FILENAME_SUFFIX_DATA,
				    aead_buf, object_data_size);
	if (status != PSA_SUCCESS) {
		return status;
	}

	status = trusted_storage_get_key(uid, key_buf, AEAD_KEY_SIZE);
	if (status != PSA_SUCCESS) {
		return status;
	}

	additional_data.uid = uid;
	additional_data.flags = data_flags;
	additional_data.size = data_length;

	status = trusted_storage_aead_decrypt(key_buf, AEAD_KEY_SIZE, nonce, AEAD_NONCE_SIZE,
					      (void *)&additional_data, sizeof(additional_data),
					      aead_buf, object_data_size, data_buf,
					      TRUSTED_STORAGE_MAX_ASSET_SIZE, &aead_out_size);

	/* Clean up */
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

psa_status_t trusted_set(const psa_storage_uid_t uid, const char *prefix, size_t data_length,
			 const void *p_data, psa_storage_create_flags_t create_flags)
{
	psa_status_t status = PSA_ERROR_CORRUPTION_DETECTED;
	uint8_t key_buf[AEAD_KEY_SIZE + 1];
	uint8_t nonce[AEAD_NONCE_SIZE];
	size_t aead_out_size;
	struct aead_additional_data additional_data;
	psa_storage_create_flags_t data_flags;

	if (data_length == 0 || p_data == NULL) {
		return PSA_ERROR_INVALID_ARGUMENT;
	}

	if (create_flags != PSA_STORAGE_FLAG_NONE && create_flags != PSA_STORAGE_FLAG_WRITE_ONCE) {
		return PSA_ERROR_NOT_SUPPORTED;
	}

	if (data_length > TRUSTED_STORAGE_MAX_ASSET_SIZE) {
		return PSA_ERROR_NOT_SUPPORTED;
	}

	/* Get flags */
	status = storage_get_object(uid, prefix, TRUSTED_STORAGE_AEAD_BACKEND_FILENAME_SUFFIX_FLAGS,
				    (void *)&data_flags, sizeof(data_flags));
	if (status != PSA_SUCCESS && status != PSA_ERROR_DOES_NOT_EXIST) {
		return status;
	}

	/* Do not allow to write new values if WRITE_ONCE flag is set */
	if (status == PSA_SUCCESS && (data_flags & PSA_STORAGE_FLAG_WRITE_ONCE) != 0) {
		return PSA_ERROR_NOT_PERMITTED;
	}

	/* Write new size & flags */
	status = storage_set_object(uid, prefix, TRUSTED_STORAGE_AEAD_BACKEND_FILENAME_SUFFIX_SIZE,
				    (void *)&data_length, sizeof(data_length));
	if (status != PSA_SUCCESS) {
		goto cleanup_objects;
	}

	status = storage_set_object(uid, prefix, TRUSTED_STORAGE_AEAD_BACKEND_FILENAME_SUFFIX_FLAGS,
				    (void *)&create_flags, sizeof(create_flags));
	if (status != PSA_SUCCESS) {
		goto cleanup_objects;
	}

	/* Get AEAD key */
	status = trusted_storage_get_key(uid, key_buf, AEAD_KEY_SIZE);
	if (status != PSA_SUCCESS) {
		goto cleanup_objects;
	}

	/* Get new nonce at each set */
	status = trusted_storage_get_nonce(nonce, AEAD_NONCE_SIZE);
	if (status != PSA_SUCCESS) {
		goto cleanup_objects;
	}

	additional_data.uid = uid;
	additional_data.flags = create_flags;
	additional_data.size = data_length;

	status = trusted_storage_aead_encrypt(key_buf, AEAD_KEY_SIZE, nonce, AEAD_NONCE_SIZE,
					      (void *)&additional_data, sizeof(additional_data),
					      p_data, data_length, aead_buf, AEAD_MAX_BUF_SIZE,
					      &aead_out_size);

	memset(key_buf, 0, AEAD_KEY_SIZE);

	if (status != PSA_SUCCESS) {
		goto cleanup;
	}

	/* Write nonce */
	status = storage_set_object(uid, prefix, TRUSTED_STORAGE_AEAD_BACKEND_FILENAME_SUFFIX_NONCE,
				    nonce, AEAD_NONCE_SIZE);
	if (status != PSA_SUCCESS) {
		goto cleanup_objects;
	}

	/* Write data (with embedded tag) */
	status = storage_set_object(uid, prefix, TRUSTED_STORAGE_AEAD_BACKEND_FILENAME_SUFFIX_DATA,
				    aead_buf, aead_out_size);
	if (status != PSA_SUCCESS) {
		goto cleanup_objects;
	}

	return PSA_SUCCESS;

cleanup_objects:
	/* Remove all object if an error occurs */
	storage_remove_object(uid, prefix, TRUSTED_STORAGE_AEAD_BACKEND_FILENAME_SUFFIX_NONCE);
	storage_remove_object(uid, prefix, TRUSTED_STORAGE_AEAD_BACKEND_FILENAME_SUFFIX_DATA);
	storage_remove_object(uid, prefix, TRUSTED_STORAGE_AEAD_BACKEND_FILENAME_SUFFIX_FLAGS);

cleanup:
	memset(&additional_data, 0, sizeof(additional_data));
	memset(nonce, 0, sizeof(nonce));
	memset(aead_buf, 0, AEAD_MAX_BUF_SIZE);

	return status;
}

psa_status_t trusted_remove(const psa_storage_uid_t uid, const char *prefix)
{
	psa_status_t status = PSA_ERROR_CORRUPTION_DETECTED;
	psa_storage_create_flags_t data_flags;

	/* Get flags */
	status = storage_get_object(uid, prefix, TRUSTED_STORAGE_AEAD_BACKEND_FILENAME_SUFFIX_FLAGS,
				    (void *)&data_flags, sizeof(data_flags));
	if (status != PSA_SUCCESS) {
		return status;
	}

	if (status == PSA_SUCCESS && (data_flags & PSA_STORAGE_FLAG_WRITE_ONCE) != 0) {
		return PSA_ERROR_NOT_PERMITTED;
	}

	status = storage_remove_object(uid, prefix,
				       TRUSTED_STORAGE_AEAD_BACKEND_FILENAME_SUFFIX_SIZE);
	if (status != PSA_SUCCESS) {
		return status;
	}

	return storage_remove_object(uid, prefix,
				     TRUSTED_STORAGE_AEAD_BACKEND_FILENAME_SUFFIX_FLAGS);
}

uint32_t trusted_get_support(void)
{
	return 0;
}

psa_status_t trusted_create(const psa_storage_uid_t uid, size_t capacity,
			    psa_storage_create_flags_t create_flags)
{

	ARG_UNUSED(uid);
	ARG_UNUSED(capacity);
	ARG_UNUSED(create_flags);
	return PSA_ERROR_NOT_SUPPORTED;
}

psa_status_t trusted_set_extended(const psa_storage_uid_t uid, size_t data_offset,
				  size_t data_length, const void *p_data)
{
	ARG_UNUSED(uid);
	ARG_UNUSED(data_offset);
	ARG_UNUSED(data_length);
	ARG_UNUSED(p_data);
	return PSA_ERROR_NOT_SUPPORTED;
}
