/*
 * Copyright (c) 2017 Linaro Limited.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef _ARM_CORTEX_M_MPU_MEM_CFG_H_
#define _ARM_CORTEX_M_MPU_MEM_CFG_H_

#include <arch/arm/aarch32/mpu/arm_mpu.h>

#if !defined(CONFIG_ARMV8_M_BASELINE) && !defined(CONFIG_ARMV8_M_MAINLINE)

/* Flash Region Definitions */
#if CONFIG_FLASH_SIZE <= 64
#define REGION_FLASH_SIZE REGION_64K
#elif CONFIG_FLASH_SIZE <= 128
#define REGION_FLASH_SIZE REGION_128K
#elif CONFIG_FLASH_SIZE <= 256
#define REGION_FLASH_SIZE REGION_256K
#elif CONFIG_FLASH_SIZE <= 512
#define REGION_FLASH_SIZE REGION_512K
#elif CONFIG_FLASH_SIZE <= 1024
#define REGION_FLASH_SIZE REGION_1M
#elif CONFIG_FLASH_SIZE <= 2048
#define REGION_FLASH_SIZE REGION_2M
#elif CONFIG_FLASH_SIZE <= 4096
#define REGION_FLASH_SIZE REGION_4M
#elif CONFIG_FLASH_SIZE <= 8192
#define REGION_FLASH_SIZE REGION_8M
#elif CONFIG_FLASH_SIZE <= 16384
#define REGION_FLASH_SIZE REGION_16M
#elif CONFIG_FLASH_SIZE <= 65536
#define REGION_FLASH_SIZE REGION_64M
#elif CONFIG_FLASH_SIZE <= 131072
#define REGION_FLASH_SIZE REGION_128M
#elif CONFIG_FLASH_SIZE <= 262144
#define REGION_FLASH_SIZE REGION_256M
#elif CONFIG_FLASH_SIZE <= 524288
#define REGION_FLASH_SIZE REGION_512M
#else
#error "Unsupported flash size configuration"
#endif

/* SRAM Region Definitions */
#if CONFIG_SRAM_SIZE <= 16
#define REGION_SRAM_SIZE REGION_16K
#elif CONFIG_SRAM_SIZE <= 32
#define REGION_SRAM_SIZE REGION_32K
#elif CONFIG_SRAM_SIZE <= 64
#define REGION_SRAM_SIZE REGION_64K
#elif CONFIG_SRAM_SIZE <= 128
#define REGION_SRAM_SIZE REGION_128K
#elif CONFIG_SRAM_SIZE <= 256
#define REGION_SRAM_SIZE REGION_256K
#elif CONFIG_SRAM_SIZE <= 512
#define REGION_SRAM_SIZE REGION_512K
#elif CONFIG_SRAM_SIZE <= 1024
#define REGION_SRAM_SIZE REGION_1M
#elif CONFIG_SRAM_SIZE <= 2048
#define REGION_SRAM_SIZE REGION_2M
#elif CONFIG_SRAM_SIZE <= 4096
#define REGION_SRAM_SIZE REGION_4M
#elif CONFIG_SRAM_SIZE <= 8192
#define REGION_SRAM_SIZE REGION_8M
#elif CONFIG_SRAM_SIZE <= 16384
#define REGION_SRAM_SIZE REGION_16M
#elif CONFIG_SRAM_SIZE == 32768
#define REGION_SRAM_SIZE REGION_32M
#elif CONFIG_SRAM_SIZE == 65536
#define REGION_SRAM_SIZE REGION_64M
#else
#error "Unsupported sram size configuration"
#endif

#define MPU_REGION_SIZE_32		REGION_32B
#define MPU_REGION_SIZE_64		REGION_64B
#define MPU_REGION_SIZE_128		REGION_128B
#define MPU_REGION_SIZE_256		REGION_256B
#define MPU_REGION_SIZE_512		REGION_512B
#define MPU_REGION_SIZE_1024		REGION_1K
#define MPU_REGION_SIZE_2048		REGION_2K
#define MPU_REGION_SIZE_4096		REGION_4K
#define MPU_REGION_SIZE_8192		REGION_8K
#define MPU_REGION_SIZE_16384		REGION_16K
#define MPU_REGION_SIZE_32768		REGION_32K
#define MPU_REGION_SIZE_65536		REGION_64K
#define MPU_REGION_SIZE_131072		REGION_128K
#define MPU_REGION_SIZE_262144		REGION_256K
#define MPU_REGION_SIZE_524288		REGION_512K
#define MPU_REGION_SIZE_1048576		REGION_1M
#define MPU_REGION_SIZE_2097152		REGION_2M
#define MPU_REGION_SIZE_4194304		REGION_4M
#define MPU_REGION_SIZE_8388608		REGION_8M
#define MPU_REGION_SIZE_16777216	REGION_16M
#define MPU_REGION_SIZE_33554432	REGION_32M
#define MPU_REGION_SIZE_67108864	REGION_64M
#define MPU_REGION_SIZE_134217728	REGION_128M
#define MPU_REGION_SIZE_268435456	REGION_256M
#define MPU_REGION_SIZE_536870912	REGION_512M

#define MPU_REGION_SIZE(x)		MPU_REGION_SIZE_ ## x

#define ARM_MPU_REGION_INIT(p_name, p_base, p_size, p_attr)	\
	{ .name = p_name,					\
	  .base = p_base,					\
	  .attr = p_attr(MPU_REGION_SIZE(p_size)),		\
	}

#else

#define ARM_MPU_REGION_INIT(p_name, p_base, p_size, p_attr)	\
	{ .name = p_name,					\
	  .base = p_base,					\
	  .attr = p_attr(p_base, p_size),			\
	}

#endif /* !ARMV8_M_BASELINE && !ARMV8_M_MAINLINE */

#endif /* _ARM_CORTEX_M_MPU_MEM_CFG_H_ */
