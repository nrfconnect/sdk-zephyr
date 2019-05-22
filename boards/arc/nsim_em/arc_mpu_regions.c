/*
 * Copyright (c) 2017 Synopsys
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <generated_dts_board.h>
#include <soc.h>
#include <arch/arc/v2/mpu/arc_mpu.h>
#include <linker/linker-defs.h>

static struct arc_mpu_region mpu_regions[] = {
#if DT_ICCM_SIZE > 0
	/* Region ICCM */
	MPU_REGION_ENTRY("ICCM",
			 DT_ICCM_BASE_ADDRESS,
			 DT_ICCM_SIZE * 1024,
			 REGION_ROM_ATTR),
#endif
#if DT_DCCM_SIZE > 0
	/* Region DCCM */
	MPU_REGION_ENTRY("DCCM",
			 DT_DCCM_BASE_ADDRESS,
			 DT_DCCM_SIZE * 1024,
			 REGION_KERNEL_RAM_ATTR | REGION_DYNAMIC),
#endif
	/* Region Peripheral */
	MPU_REGION_ENTRY("PERIPHERAL",
			 0xF0000000,
			 64 * 1024,
			 REGION_KERNEL_RAM_ATTR),
};

struct arc_mpu_config mpu_config = {
	.num_regions = ARRAY_SIZE(mpu_regions),
	.mpu_regions = mpu_regions,
};
