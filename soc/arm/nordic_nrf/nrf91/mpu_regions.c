/*
 * Copyright (c) 2017 Linaro Limited.
 * Copyright (c) 2018 Nordic Semiconductor ASA.
 *
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <misc/slist.h>
#include <arch/arm/cortex_m/mpu/arm_mpu.h>


#define XICR_BASE	0x10000000
#define PERIPH_BASE	0x40000000
#define M33_PPB_BASE	0xE0000000

static struct arm_mpu_region mpu_regions[] = {
	/* Region 0 */
	MPU_REGION_ENTRY("FLASH_0",
		CONFIG_FLASH_BASE_ADDRESS,
		REGION_FLASH_ATTR(CONFIG_FLASH_BASE_ADDRESS, \
			CONFIG_FLASH_SIZE * 1024)),
	/* Region 1 */
	MPU_REGION_ENTRY("SRAM_0",
		CONFIG_SRAM_BASE_ADDRESS,
		REGION_RAM_ATTR(CONFIG_SRAM_BASE_ADDRESS, \
			CONFIG_SRAM_SIZE * 1024)),
};

struct arm_mpu_config mpu_config = {
	.num_regions = ARRAY_SIZE(mpu_regions),
	.mpu_regions = mpu_regions,
};
