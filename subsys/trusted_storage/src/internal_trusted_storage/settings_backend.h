/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __INTERNAL_TRUSTED_STORAGE_SETTINGS_H_
#define __INTERNAL_TRUSTED_STORAGE_SETTINGS_H_

#include <psa/storage_common.h>
#include <psa/internal_trusted_storage.h>

/* Prefix to use for the data & medatata file name */
#define ITS_STORAGE_FILENAME_PREFIX CONFIG_INTERNAL_TRUSTED_STORAGE_SETTINGS_PREFIX

/* Trust implementation entry points */

/*
 * Load & validate the data within the trust implementation
 * Object flags has already been checked by the caller.
 * Returns 0 or a negative PSA error value if an error occurs.
 */
psa_status_t psa_its_get_settings_trusted(psa_storage_uid_t uid,
				const char *prefix,
				size_t data_size, size_t data_offset,
				size_t data_length, void *p_data,
				size_t *p_data_length,
				psa_storage_create_flags_t create_flags);

/*
 * Stores & authenticates the data within the trust implementation
 * Returns 0 or a negative PSA error value if an error occurs.
 */
psa_status_t psa_its_set_settings_trusted(psa_storage_uid_t uid,
				const char *prefix, size_t data_length,
				const void *p_data,
				psa_storage_create_flags_t create_flags);

/*
 * Removes data and metadata stored by the trust implementation
 * Returns 0 or a negative PSA error value if an error occurs.
 */
psa_status_t psa_its_remove_settings_trusted(psa_storage_uid_t uid,
				const char *prefix,
				psa_storage_create_flags_t create_flags);

#endif /* __INTERNAL_TRUSTED_STORAGE_BACKEND_H_ */
