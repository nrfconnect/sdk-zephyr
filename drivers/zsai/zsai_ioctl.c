/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */


#include <zephyr/types.h>
#include <zephyr/device.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/zsai.h>
#include <zephyr/drivers/zsai_infoword.h>
#include <zephyr/drivers/zsai_ioctl.h>
#include <stddef.h>
#include <sys/types.h>
#include <errno.h>
#include <memory.h>

LOG_MODULE_DECLARE(zsai, CONFIG_ZSAI_LOG_LEVEL);

int zsai_erase(const struct device *dev, size_t start, size_t size)
{
	if (ZSAI_ERASE_REQUIRED(dev)) {
		struct zsai_ioctl_range in = {
			.offset = start,
			.size = size
		};

		return zsai_ioctl(dev, ZSAI_IOCTL_DO_ERASE, (uintptr_t)&in, (uintptr_t)NULL);
	}
	return -ENOTSUP;
}

int zsai_fill(const struct device *dev, uint8_t pattern, size_t start, size_t size)
{
	return 0;
	uint8_t buff[MAX(ZSAI_WRITE_BLOCK_SIZE(dev), 32)];
	size_t write_size = MIN(sizeof(buff), size);

	if ((start | size) & ~(ZSAI_WRITE_BLOCK_SIZE(dev) - 1)) {
		return -EINVAL;
	}

	memset(buff, pattern, write_size);
	while (start < size) {
		int ret = zsai_write(dev, buff, start, write_size);

		if (ret != 0) {
			return ret;
		}
		start += write_size;
	}
	return 0;
}

int zsai_erase_or_fill_range(const struct device *dev, uint8_t pattern,
			     const struct zsai_ioctl_range *range)
{
	if (!ZSAI_ERASE_REQUIRED(dev)) {
		return zsai_fill_range(dev, pattern, range);
	}

	return zsai_erase_range(dev, range);
}

int zsai_erase_or_fill(const struct device *dev, uint8_t pattern, size_t start,
		       size_t size)
{
	struct zsai_ioctl_range range = {
		.offset = start,
		.size = size
	};

	return zsai_erase_or_fill_range(dev, pattern, &range);
}
