/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <init.h>
#include "settings/settings_file.h"
#include <zephyr/device.h>
#include <zephyr/fs/fs.h>
#include <zephyr/fs/littlefs.h>
#include <psa/crypto.h>

/* NFFS work area strcut */
FS_LITTLEFS_DECLARE_DEFAULT_CONFIG(cstorage);
static struct fs_mount_t littlefs_mnt = {
	.type = FS_LITTLEFS,
	.fs_data = &cstorage,
	.storage_dev = (void *)FLASH_AREA_ID(littlefs_dev),
	.mnt_point = "/ff",
};

static int setup_settings_backend(const struct device *dev)
{
	int rc;
	const struct flash_area *fap;

	ARG_UNUSED(dev);

	rc = flash_area_open(FLASH_AREA_ID(littlefs_dev), &fap);
	if (rc != 0)
		return rc;

	rc = flash_area_erase(fap, fap->fa_off, fap->fa_size);
	if (rc != 0)
		return rc;

	rc = fs_mount(&littlefs_mnt);
	if (rc != 0)
		return rc;

	return 0;
}

SYS_INIT(setup_settings_backend, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
