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

#include "settings_helpers.h"

/* Helper to fill filename with a suffix */
#define TRUSTED_STORAGE_FILENAME_FILL(filename, prefix, uid, suffix)	\
	snprintf((filename),						\
		 TRUSTED_STORAGE_FILENAME_MAX_LENGTH + 1,		\
		 TRUSTED_STORAGE_FILENAME_PATTERN,			\
		 (prefix),						\
		 (unsigned int)((uid) >> 32),				\
		 (unsigned int)((uid) & 0xffffffff),			\
		 (suffix))

/*
 * Writes an object
 * Will write an object of object_size bytes size
 * Returns 0 or a negative number if an error occurs.
 */
int trusted_storage_set_object(const psa_storage_uid_t uid, const char *prefix,
			       char *suffix, const uint8_t *object_data,
			       const size_t object_size)
{
	char path[TRUSTED_STORAGE_FILENAME_MAX_LENGTH + 1];

	if (object_size == 0 || object_data == NULL ||
	    prefix == NULL || suffix == NULL) {
		return -EINVAL;
	}

	TRUSTED_STORAGE_FILENAME_FILL(path, prefix, uid, suffix);

	return settings_save_one(path, object_data, object_size);
}

struct load_object_info {
	uint8_t *data;
	size_t size;
	int ret;
};

/*
 * Reads the object content
 * if object size is larger only read the provided size
 * is object is smaller, return with error
 */
static int trusted_storage_load_object(const char *key, size_t len,
				       settings_read_cb read_cb,
				       void *cb_arg, void *param)
{
	struct load_object_info *info = param;

	if (len < info->size) {
		info->ret = -EINVAL;
	} else {
		info->ret = read_cb(cb_arg, info->data, info->size);
	}

	/*
	 * This returned value isn't necessarely kept
	 * so also keep it in the load_object_info structure
	 */
	return info->ret;
}

/* Gets an object of the exact object_size size
 * Returns 0, -ENOENT if object doesn't exist or a negative number
 * if an error occurs.
 */
int trusted_storage_get_object(const psa_storage_uid_t uid, const char *prefix,
			       const char *suffix, uint8_t *object_data,
			       const size_t object_size)
{
	char path[TRUSTED_STORAGE_FILENAME_MAX_LENGTH + 1];
	struct load_object_info info;
	int ret;

	if (object_size == 0 || object_data == NULL ||
	    prefix == NULL || suffix == NULL) {
		return -EINVAL;
	}

	TRUSTED_STORAGE_FILENAME_FILL(path, prefix, uid, suffix);

	info.data = object_data;
	info.size = object_size;
	/* Set a fallback error if trusted_storage_load_object isn't called */
	info.ret = -ENOENT;

	ret = settings_load_subtree_direct(path, trusted_storage_load_object,
					   &info);
	if (ret < 0) {
		return ret;
	}

	if (info.ret < 0) {
		return info.ret;
	}

	return 0;
}

/*
 * Deletes an object
 * Returns 0, -ENOENT if object doesn't exist or a negative number
 * if an error occurs.
 */
int trusted_storage_remove_object(const psa_storage_uid_t uid,
				  const char *prefix, const char *suffix)
{
	char path[TRUSTED_STORAGE_FILENAME_MAX_LENGTH + 1];

	if (prefix == NULL || suffix == NULL) {
		return -EINVAL;
	}

	TRUSTED_STORAGE_FILENAME_FILL(path, prefix, uid, suffix);

	return settings_delete(path);
}

psa_status_t trusted_storage_get_info(psa_storage_uid_t uid, const char *prefix,
				      struct psa_storage_info_t *p_info)
{
	psa_storage_create_flags_t data_flags;
	size_t data_size;
	int ret;

	if (p_info == NULL) {
		return PSA_ERROR_INVALID_ARGUMENT;
	}

	/* Get size & flags */
	ret = trusted_storage_get_object(uid, prefix,
					 TRUSTED_STORAGE_FILENAME_SUFFIX_FLAGS,
					 (void *)&data_flags,
					 sizeof(data_flags));
	if (ret == -ENOENT) {
		return PSA_ERROR_DOES_NOT_EXIST;
	} else if (ret == -ENODATA) {
		return PSA_ERROR_DATA_CORRUPT;
	} else if (ret < 0) {
		return PSA_ERROR_STORAGE_FAILURE;
	}

	ret = trusted_storage_get_object(uid, prefix,
					 TRUSTED_STORAGE_FILENAME_SUFFIX_SIZE,
					 (void *)&data_size,
					 sizeof(data_size));
	if (ret == -ENOENT) {
		return PSA_ERROR_DOES_NOT_EXIST;
	} else if (ret == -ENODATA) {
		return PSA_ERROR_DATA_CORRUPT;
	} else if (ret < 0) {
		return PSA_ERROR_STORAGE_FAILURE;
	}

	p_info->capacity = TRUSTED_STORAGE_MAX_ASSET_SIZE;
	p_info->size = data_size;
	p_info->flags = data_flags;

	return PSA_SUCCESS;
}

psa_status_t trusted_storage_get(psa_storage_uid_t uid, const char *prefix,
			size_t data_offset, size_t data_length,
			void *p_data, size_t *p_data_length,
			trusted_storage_get_trusted_cb_t get_trusted_cb)
{
	psa_storage_create_flags_t data_flags;
	size_t data_size;
	int ret;

	if (data_length == 0 || p_data == NULL || p_data_length == NULL) {
		return PSA_ERROR_INVALID_ARGUMENT;
	}

	if ((data_offset + data_length) > TRUSTED_STORAGE_MAX_ASSET_SIZE) {
		return PSA_ERROR_NOT_SUPPORTED;
	}

	/* Get flags then size */
	ret = trusted_storage_get_object(uid, prefix,
					 TRUSTED_STORAGE_FILENAME_SUFFIX_FLAGS,
					 (void *)&data_flags,
					 sizeof(data_flags));
	if (ret == -ENOENT) {
		return PSA_ERROR_DOES_NOT_EXIST;
	} else if (ret == -ENODATA) {
		return PSA_ERROR_DATA_CORRUPT;
	} else if (ret < 0) {
		return PSA_ERROR_STORAGE_FAILURE;
	}

	ret = trusted_storage_get_object(uid, prefix,
					 TRUSTED_STORAGE_FILENAME_SUFFIX_SIZE,
					 (void *)&data_size,
					 sizeof(data_size));
	if (ret == -ENOENT || ret == -ENODATA) {
		return PSA_ERROR_DATA_CORRUPT;
	} else if (ret < 0) {
		return PSA_ERROR_STORAGE_FAILURE;
	}

	if ((data_offset + data_length) > data_size) {
		return PSA_ERROR_DATA_CORRUPT;
	}

	ret = get_trusted_cb(uid, prefix, data_size, data_offset, data_length,
			     p_data, p_data_length, data_flags);
	if (ret != PSA_SUCCESS) {
		return ret;
	}

	return PSA_SUCCESS;
}

psa_status_t trusted_storage_set(psa_storage_uid_t uid, const char *prefix,
			size_t data_length, const void *p_data,
			psa_storage_create_flags_t create_flags,
			trusted_storage_set_trusted_cb_t set_trusted_cb)
{
	psa_status_t status = PSA_ERROR_GENERIC_ERROR;
	psa_storage_create_flags_t data_flags;
	int ret;

	if (data_length == 0 || p_data == NULL) {
		return PSA_ERROR_INVALID_ARGUMENT;
	}

	if (create_flags != 0 &&
	    create_flags != PSA_STORAGE_FLAG_WRITE_ONCE) {
		return PSA_ERROR_NOT_SUPPORTED;
	}

	if (data_length > TRUSTED_STORAGE_MAX_ASSET_SIZE) {
		return PSA_ERROR_NOT_SUPPORTED;
	}

	/* Get flags */
	ret = trusted_storage_get_object(uid, prefix,
					 TRUSTED_STORAGE_FILENAME_SUFFIX_FLAGS,
					 (void *)&data_flags,
					 sizeof(data_flags));
	if (ret == -ENODATA) {
		return PSA_ERROR_DATA_CORRUPT;
	} else if (ret < 0 && ret != -ENOENT) {
		return PSA_ERROR_STORAGE_FAILURE;
	}

	if (ret == 0 && (data_flags & PSA_STORAGE_FLAG_WRITE_ONCE) != 0) {
		return PSA_ERROR_NOT_PERMITTED;
	}

	/* Write new size & flags */
	ret = trusted_storage_set_object(uid, prefix,
					 TRUSTED_STORAGE_FILENAME_SUFFIX_SIZE,
					 (void *)&data_length,
					 sizeof(data_length));
	if (ret < 0) {
		status = PSA_ERROR_STORAGE_FAILURE;
		goto cleanup_objects;
	}

	ret = trusted_storage_set_object(uid, prefix,
					 TRUSTED_STORAGE_FILENAME_SUFFIX_FLAGS,
					 (void *)&create_flags,
					 sizeof(create_flags));
	if (ret < 0) {
		status = PSA_ERROR_STORAGE_FAILURE;
		goto cleanup_objects;
	}

	/* Write data */
	status = set_trusted_cb(uid, prefix, data_length, p_data, create_flags);
	if (status != PSA_SUCCESS) {
		/*
		 * On error, objects create by the trust implementation
		 * are expected to be already removed
		 */
		goto cleanup_objects;
	}

	return PSA_SUCCESS;

cleanup_objects:
	/* Remove all object if an error occurs */
	trusted_storage_remove_object(uid, prefix,
				      TRUSTED_STORAGE_FILENAME_SUFFIX_SIZE);
	trusted_storage_remove_object(uid, prefix,
				      TRUSTED_STORAGE_FILENAME_SUFFIX_FLAGS);

	return status;
}

psa_status_t trusted_storage_remove(psa_storage_uid_t uid, const char *prefix,
			trusted_storage_remove_trusted_cb_t remove_trusted_cb)
{
	psa_storage_create_flags_t data_flags;
	int ret;

	/* Get flags */
	ret = trusted_storage_get_object(uid, prefix,
					 TRUSTED_STORAGE_FILENAME_SUFFIX_FLAGS,
					 (void *)&data_flags,
					 sizeof(data_flags));
	if (ret == -ENOENT) {
		return PSA_ERROR_DOES_NOT_EXIST;
	} else if (ret == -ENODATA) {
		return PSA_ERROR_DATA_CORRUPT;
	} else if (ret < 0) {
		return PSA_ERROR_STORAGE_FAILURE;
	}

	if (ret == 0 && (data_flags & PSA_STORAGE_FLAG_WRITE_ONCE) != 0) {
		return PSA_ERROR_NOT_PERMITTED;
	}

	ret = trusted_storage_remove_object(uid, prefix,
					TRUSTED_STORAGE_FILENAME_SUFFIX_SIZE);
	if (ret == -ENOENT || ret == -ENODATA) {
		return PSA_ERROR_DATA_CORRUPT;
	} else if (ret < 0) {
		return PSA_ERROR_STORAGE_FAILURE;
	}

	ret = trusted_storage_remove_object(uid, prefix,
					TRUSTED_STORAGE_FILENAME_SUFFIX_FLAGS);
	if (ret == -ENOENT || ret == -ENODATA) {
		return PSA_ERROR_DATA_CORRUPT;
	} else if (ret < 0) {
		return PSA_ERROR_STORAGE_FAILURE;
	}

	return remove_trusted_cb(uid, prefix, data_flags);
}
