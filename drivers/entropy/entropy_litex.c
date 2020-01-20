/*
 * Copyright (c) 2019 Antmicro <www.antmicro.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <device.h>
#include <drivers/entropy.h>
#include <errno.h>
#include <init.h>
#include <soc.h>
#include <string.h>
#include <zephyr.h>

#define PRBS_STATUS     ((volatile uint32_t *)DT_INST_0_LITEX_PRBS_BASE_ADDRESS)
#define PRBS_WIDTH      DT_INST_0_LITEX_PRBS_SIZE
#define SUBREG_SIZE_BIT 8

static inline unsigned int prbs_read(volatile u32_t *reg_status,
					 volatile u32_t reg_width)
{
	u32_t shifted_data, shift, i;
	u32_t result = 0;

	for (i = 0; i < reg_width; ++i) {
		shifted_data = *(reg_status + i);
		shift = (reg_width - i - 1) * SUBREG_SIZE_BIT;
		result |= (shifted_data << shift);
	}

	return result;
}

static int entropy_prbs_get_entropy(struct device *dev, u8_t *buffer,
					 u16_t length)
{
	while (length > 0) {
		size_t to_copy;
		u32_t value;

		value = prbs_read(PRBS_STATUS, PRBS_WIDTH);
		to_copy = MIN(length, sizeof(value));

		memcpy(buffer, &value, to_copy);
		buffer += to_copy;
		length -= to_copy;
	}
	return 0;
}

static int entropy_prbs_init(struct device *dev)
{
	return 0;
}

static const struct entropy_driver_api entropy_prbs_api = {
	.get_entropy = entropy_prbs_get_entropy
};

DEVICE_AND_API_INIT(entropy_prbs, CONFIG_ENTROPY_NAME,
		    entropy_prbs_init, NULL, NULL,
		    PRE_KERNEL_1, CONFIG_KERNEL_INIT_PRIORITY_DEVICE,
		    &entropy_prbs_api);

