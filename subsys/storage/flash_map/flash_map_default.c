/*
 * Copyright (c) 2017 Nordic Semiconductor ASA
 * Copyright (c) 2015 Runtime Inc
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <storage/flash_map.h>

/**
 * Have flash_map_default use Partition Manager information instead of
 * DeviceTree information when we are using the Partition Manager.
 */

#if USE_PARTITION_MANAGER
#include <pm_config.h>
#include <sys/util.h>

#define FLASH_MAP_OFFSET(i) UTIL_CAT(PM_, UTIL_CAT(PM_##i##_LABEL, _ADDRESS))
#define FLASH_MAP_DEV(i)    UTIL_CAT(PM_, UTIL_CAT(PM_##i##_LABEL, _DEV_NAME))
#define FLASH_MAP_SIZE(i)   UTIL_CAT(PM_, UTIL_CAT(PM_##i##_LABEL, _SIZE))
#define FLASH_MAP_NUM       PM_NUM

#else

#define FLASH_MAP_OFFSET(i) DT_FLASH_AREA_##i##_OFFSET
#define FLASH_MAP_DEV(i)    DT_FLASH_AREA_##i##_DEV
#define FLASH_MAP_SIZE(i)   DT_FLASH_AREA_##i##_SIZE
#define FLASH_MAP_NUM       DT_FLASH_AREA_NUM

#endif /* USE_PARTITION_MANAGER */

#define FLASH_AREA_FOO(i, _)                \
	{                                       \
		.fa_id       = i,                   \
		.fa_off      = FLASH_MAP_OFFSET(i), \
		.fa_dev_name = FLASH_MAP_DEV(i),    \
		.fa_size     = FLASH_MAP_SIZE(i)    \
	},

const struct flash_area default_flash_map[] = {
	UTIL_LISTIFY(FLASH_MAP_NUM, FLASH_AREA_FOO, ~)
};

const int flash_map_entries = ARRAY_SIZE(default_flash_map);
const struct flash_area *flash_map = default_flash_map;
