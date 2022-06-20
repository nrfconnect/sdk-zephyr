/*
 * Copyright (c) 2017-2021 Nordic Semiconductor ASA
 * Copyright (c) 2015 Runtime Inc
 * Copyright (c) 2017 Linaro Ltd
 * Copyright (c) 2020 Gerson Fernando Budke <nandojve@gmail.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/types.h>
#include <stddef.h>
#include <sys/types.h>
#include <device.h>
#include <storage/flash_map.h>
#include "flash_map_priv.h"
#include <drivers/flash.h>
#include <soc.h>
#include <init.h>

void flash_area_foreach(flash_area_cb_t user_cb, void *user_data)
{
	for (int i = 0; i < flash_map_entries; i++) {
		user_cb(&flash_map[i], user_data);
	}
}

int flash_area_open(uint8_t id, const struct flash_area **fap)
{
	const struct flash_area *area;

	if (flash_map == NULL) {
		return -EACCES;
	}

	area = get_flash_area_from_id(id);
	if (area == NULL) {
		return -ENOENT;
	}

	if (device_get_binding(area->fa_dev_name) == NULL) {
		return -ENODEV;
	}

	*fap = area;

	return 0;
}

void flash_area_close(const struct flash_area *fa)
{
	/* nothing to do for now */
}

int flash_area_read(const struct flash_area *fa, off_t off, void *dst,
		    size_t len)
{
	const struct device *dev;

	if (!is_in_flash_area_bounds(fa, off, len)) {
		return -EINVAL;
	}

	dev = device_get_binding(fa->fa_dev_name);

	return flash_read(dev, fa->fa_off + off, dst, len);
}

int flash_area_write(const struct flash_area *fa, off_t off, const void *src,
		     size_t len)
{
	const struct device *flash_dev;
	int rc;

	if (!is_in_flash_area_bounds(fa, off, len)) {
		return -EINVAL;
	}

	flash_dev = device_get_binding(fa->fa_dev_name);

	rc = flash_write(flash_dev, fa->fa_off + off, (void *)src, len);

	return rc;
}

int flash_area_erase(const struct flash_area *fa, off_t off, size_t len)
{
	const struct device *flash_dev;
	int rc;

	if (!is_in_flash_area_bounds(fa, off, len)) {
		return -EINVAL;
	}

	flash_dev = device_get_binding(fa->fa_dev_name);

	rc = flash_erase(flash_dev, fa->fa_off + off, len);

	return rc;
}

uint32_t flash_area_align(const struct flash_area *fa)
{
	const struct device *dev;

	dev = device_get_binding(fa->fa_dev_name);

	return flash_get_write_block_size(dev);
}

int flash_area_has_driver(const struct flash_area *fa)
{
	if (device_get_binding(fa->fa_dev_name) == NULL) {
		return -ENODEV;
	}

	return 1;
}

const struct device *flash_area_get_device(const struct flash_area *fa)
{
	return device_get_binding(fa->fa_dev_name);
}

uint8_t flash_area_erased_val(const struct flash_area *fa)
{
	const struct flash_parameters *param;

	param = flash_get_parameters(device_get_binding(fa->fa_dev_name));

	return param->erase_value;
}
