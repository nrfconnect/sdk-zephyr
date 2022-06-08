/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <psa/storage_common.h>
#include <psa/protected_storage.h>

#include "backend.h"

psa_status_t psa_ps_get_info(psa_storage_uid_t uid,
			     struct psa_storage_info_t *p_info)
{
	if (p_info == NULL) {
		return PSA_ERROR_INVALID_ARGUMENT;
	}

	return psa_ps_get_info_backend(uid, p_info);
}

psa_status_t psa_ps_get(psa_storage_uid_t uid, size_t data_offset,
			size_t data_length, void *p_data,
			size_t *p_data_length)
{
	if (data_length == 0 || p_data == NULL || p_data_length == NULL) {
		return PSA_ERROR_INVALID_ARGUMENT;
	}

	return psa_ps_get_backend(uid, data_offset, data_length,
				  p_data, p_data_length);
}

psa_status_t psa_ps_set(psa_storage_uid_t uid,
			size_t data_length,
			const void *p_data,
			psa_storage_create_flags_t create_flags)
{
	if (data_length == 0 || p_data == NULL) {
		return PSA_ERROR_INVALID_ARGUMENT;
	}

	return psa_ps_set_backend(uid, data_length, p_data, create_flags);
}

psa_status_t psa_ps_remove(psa_storage_uid_t uid)
{
	return psa_ps_remove_backend(uid);
}

uint32_t psa_ps_get_support(void)
{
	return psa_ps_get_support_backend();
}

psa_status_t psa_ps_create(psa_storage_uid_t uid,
			   size_t capacity,
			   psa_storage_create_flags_t create_flags)
{
	return psa_ps_create_backend(uid, capacity, create_flags);
}

psa_status_t psa_ps_set_extended(psa_storage_uid_t uid,
				 size_t data_offset,
				 size_t data_length,
				 const void *p_data)
{
	return psa_ps_set_extended_backend(uid, data_offset,
					   data_length, p_data);
}
