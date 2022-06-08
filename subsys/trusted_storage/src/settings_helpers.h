/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __PROTECTED_STORAGE_SETTINGS_HELPERS_H_
#define __PROTECTED_STORAGE_SETTINGS_HELPERS_H_

#include <psa/error.h>
#include <psa/storage_common.h>

#define TRUSTED_STORAGE_MAX_ASSET_SIZE CONFIG_TRUSTED_STORAGE_SETTINGS_MAX_DATA_SIZE

/* Max filename length aligned on Settings File backend max length */
#define TRUSTED_STORAGE_FILENAME_MAX_LENGTH		32

/* UID as uint64_t in hexadecimal representation length */
#define TRUSTED_STORAGE_FILENAME_UID_LENGTH		(sizeof(uint64_t) * 2)

/* Storage pattern: prefix, uid low, uid high, suffix */
#define TRUSTED_STORAGE_FILENAME_PATTERN "%s%08x%08x%s"

/* Suffix mask */
#define TRUSTED_STORAGE_FILENAME_SUFFIX_MASK		".xxxxx"
#define TRUSTED_STORAGE_FILENAME_SUFFIX_MAX_LENGTH			\
	(sizeof(TRUSTED_STORAGE_FILENAME_SUFFIX_MASK) - 1)

/* Prefix maximum length */
#define TRUSTED_STORAGE_FILENAME_PREFIX_MAX_LENGTH			\
	(TRUSTED_STORAGE_FILENAME_MAX_LENGTH -				\
	 (TRUSTED_STORAGE_FILENAME_UID_LENGTH +				\
	  TRUSTED_STORAGE_FILENAME_SUFFIX_MAX_LENGTH))

/* Common data & metadata possible suffix */
#define TRUSTED_STORAGE_FILENAME_SUFFIX_SIZE		".size"
#define TRUSTED_STORAGE_FILENAME_SUFFIX_FLAGS		".flags"
#define TRUSTED_STORAGE_FILENAME_SUFFIX_DATA		".data"

/* Object helpers on top of Settings API */

/*
 * Writes an object
 * Will write an object of object_size bytes size
 * Returns 0 or a negative number if an error occurs.
 */
int trusted_storage_set_object(const psa_storage_uid_t uid, const char *prefix,
			       char *suffix, const uint8_t *object_data,
			       const size_t object_size);

/* Gets an object of the exact object_size size
 * Returns 0, -ENOENT if object doesn't exist or a negative number
 * if an error occurs.
 */
int trusted_storage_get_object(const psa_storage_uid_t uid, const char *prefix,
			       const char *suffix, uint8_t *object_data,
			       const size_t object_size);

/*
 * Deletes an object
 * Returns 0, -ENOENT if object doesn't exist or a negative number
 * if an error occurs.
 */
int trusted_storage_remove_object(const psa_storage_uid_t uid,
				  const char *prefix, const char *suffix);

/* Trusted Storage helpers on top of Settings API */

/*
 * Read object data and check for trust with metadata
 * Returns 0 or a negative number if an error occurs.
 */
typedef psa_status_t (*trusted_storage_get_trusted_cb_t)(
					psa_storage_uid_t uid,
					const char *prefix, size_t data_size,
					size_t data_offset,
					size_t data_length, void *p_data,
					size_t *p_data_length,
					psa_storage_create_flags_t flags);

/*
 * Write object data and with trust metadata
 * Returns 0 or a negative number if an error occurs.
 */
typedef psa_status_t (*trusted_storage_set_trusted_cb_t)(
					psa_storage_uid_t uid,
					const char *prefix, size_t data_length,
					const void *p_data,
					psa_storage_create_flags_t flags);

/*
 * Remove object data and associated metadata
 * Returns 0 or a negative number if an error occurs.
 */
typedef psa_status_t (*trusted_storage_remove_trusted_cb_t)(
					psa_storage_uid_t uid,
					const char *prefix,
					psa_storage_create_flags_t flags);

/*
 * Get object information from storage
 * Returns 0 or a negative number if an error occurs.
 */
psa_status_t trusted_storage_get_info(psa_storage_uid_t uid, const char *prefix,
				      struct psa_storage_info_t *p_info);

/*
 * Get object data from storage and use callback to read and check trust
 * Returns 0 or a negative number if an error occurs.
 */
psa_status_t trusted_storage_get(psa_storage_uid_t uid, const char *prefix,
			size_t data_offset, size_t data_length,
			void *p_data, size_t *p_data_length,
			trusted_storage_get_trusted_cb_t get_trusted_cb);

/*
 * Set object data in storage and use callback to write with trust metadata
 * Returns 0 or a negative number if an error occurs.
 */
psa_status_t trusted_storage_set(psa_storage_uid_t uid, const char *prefix,
			size_t data_length, const void *p_data,
			psa_storage_create_flags_t create_flags,
			trusted_storage_set_trusted_cb_t set_trusted_cb);

/*
 * Remove object data and use callback to remove data with trust metadata
 * Returns 0 or a negative number if an error occurs.
 */
psa_status_t trusted_storage_remove(psa_storage_uid_t uid, const char *prefix,
			trusted_storage_remove_trusted_cb_t remove_trusted_cb);

#endif /* __PROTECTED_STORAGE_BACKEND_H_ */
