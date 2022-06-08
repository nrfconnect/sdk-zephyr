/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __PROTECTED_STORAGE_BACKEND_H_
#define __PROTECTED_STORAGE_BACKEND_H_

psa_status_t psa_ps_get_info_backend(psa_storage_uid_t uid,
				     struct psa_storage_info_t *p_info);
psa_status_t psa_ps_get_backend(psa_storage_uid_t uid, size_t data_offset,
				size_t data_length, void *p_data,
				size_t *p_data_length);
psa_status_t psa_ps_set_backend(psa_storage_uid_t uid,
				size_t data_length,
				const void *p_data,
				psa_storage_create_flags_t create_flags);
psa_status_t psa_ps_remove_backend(psa_storage_uid_t uid);
uint32_t psa_ps_get_support_backend(void);
psa_status_t psa_ps_create_backend(psa_storage_uid_t uid, size_t capacity,
				   psa_storage_create_flags_t create_flags);
psa_status_t psa_ps_set_extended_backend(psa_storage_uid_t uid,
					 size_t data_offset,
					 size_t data_length,
					 const void *p_data);

#endif /* __PROTECTED_STORAGE_BACKEND_H_ */
