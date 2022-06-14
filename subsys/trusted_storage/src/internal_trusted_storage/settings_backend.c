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

LOG_MODULE_REGISTER(internal_trusted_storage_settings,
		    CONFIG_TRUSTED_STORAGE_LOG_LEVEL);

/* PSA Internal Trusted Storage with Zephyr Settings Backend */

psa_status_t psa_its_get_info_backend(psa_storage_uid_t uid,
				      struct psa_storage_info_t *p_info)
{
	return trusted_storage_get_info(uid, ITS_STORAGE_FILENAME_PREFIX,
					p_info);
}

psa_status_t psa_its_get_backend(psa_storage_uid_t uid, size_t data_offset,
				 size_t data_length, void *p_data,
				 size_t *p_data_length)
{
	return trusted_storage_get(uid, ITS_STORAGE_FILENAME_PREFIX, data_offset,
				   data_length, p_data, p_data_length,
				   psa_its_get_settings_trusted);
}

psa_status_t psa_its_set_backend(psa_storage_uid_t uid,
				 size_t data_length,
				 const void *p_data,
				 psa_storage_create_flags_t create_flags)
{
	return trusted_storage_set(uid, ITS_STORAGE_FILENAME_PREFIX,
				   data_length, p_data, create_flags,
				   psa_its_set_settings_trusted);
}

psa_status_t psa_its_remove_backend(psa_storage_uid_t uid)
{
	return trusted_storage_remove(uid, ITS_STORAGE_FILENAME_PREFIX,
				      psa_its_remove_settings_trusted);
}

static int psa_its_settings_init(const struct device *dev)
{
	int ret;

	ARG_UNUSED(dev);

	ret = settings_subsys_init();
	if (ret != 0) {
		LOG_ERR("%s failed (ret %d)", __func__, ret);
	}

	return ret;
}

SYS_INIT(psa_its_settings_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
