/*
 * Copyright (c) 2017 Linaro Limited.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <device.h>
#include <init.h>
#include <kernel.h>
#include <soc.h>
#include <arch/arm/cortex_m/mpu/arm_core_mpu_dev.h>
#include <arch/arm/cortex_m/mpu/arm_core_mpu.h>
#include <linker/linker-defs.h>

#define LOG_LEVEL CONFIG_MPU_LOG_LEVEL
#include <logging/log.h>
LOG_MODULE_DECLARE(mpu);

/*
 * Global status variable holding the number of HW MPU region indices, which
 * have been reserved by the MPU driver to program the static (fixed) memory
 * regions.
 */
static u8_t static_regions_num;

/**
 *  Get the number of supported MPU regions.
 */
static inline u8_t get_num_regions(void)
{
#if defined(CONFIG_CPU_CORTEX_M0PLUS) || \
	defined(CONFIG_CPU_CORTEX_M3) || \
	defined(CONFIG_CPU_CORTEX_M4)
	/* Cortex-M0+, Cortex-M3, and Cortex-M4 MCUs may
	 * have a fixed number of 8 MPU regions.
	 */
	return 8;
#else
	u32_t type = MPU->TYPE;

	type = (type & MPU_TYPE_DREGION_Msk) >> MPU_TYPE_DREGION_Pos;

	return (u8_t)type;
#endif
}

/* Include architecture-specific internal headers. */
#if defined(CONFIG_CPU_CORTEX_M0PLUS) || \
	defined(CONFIG_CPU_CORTEX_M3) || \
	defined(CONFIG_CPU_CORTEX_M4) || \
	defined(CONFIG_CPU_CORTEX_M7)
#include <arm_mpu_v7_internal.h>
#elif defined(CONFIG_CPU_CORTEX_M23) || \
	defined(CONFIG_CPU_CORTEX_M33)
#include <arm_mpu_v8_internal.h>
#else
#error "Unsupported ARM CPU"
#endif

static int region_allocate_and_init(const u8_t index,
	const struct arm_mpu_region *region_conf)
{
	/* Attempt to allocate new region index. */
	if (index > (get_num_regions() - 1)) {

		/* No available MPU region index. */
		LOG_ERR("Failed to allocate new MPU region %u\n", index);
		return -EINVAL;
	}

	LOG_DBG("Program MPU region at index 0x%x", index);

	/* Program region */
	region_init(index, region_conf);

	return index;
}

/* This internal function programs an MPU region
 * of a given configuration at a given MPU index.
 */
static int mpu_configure_region(const u8_t index,
	const struct k_mem_partition *new_region)
{
	struct arm_mpu_region region_conf;

	LOG_DBG("Configure MPU region at index 0x%x", index);

	/* Populate internal ARM MPU region configuration structure. */
	region_conf.base = new_region->start;
	get_region_attr_from_k_mem_partition_info(&region_conf.attr,
		&new_region->attr, new_region->start, new_region->size);

	/* Allocate and program region */
	return region_allocate_and_init(index,
		(const struct arm_mpu_region *)&region_conf);
}

/* ARM Core MPU Driver API Implementation for ARM MPU */

/**
 * @brief enable the MPU
 */
void arm_core_mpu_enable(void)
{
	/* Enable MPU and use the default memory map as a
	 * background region for privileged software access.
	 */
	MPU->CTRL = MPU_CTRL_ENABLE_Msk | MPU_CTRL_PRIVDEFENA_Msk;

	/* Make sure that all the registers are set before proceeding */
	__DSB();
	__ISB();
}

/**
 * @brief disable the MPU
 */
void arm_core_mpu_disable(void)
{
	/* Force any outstanding transfers to complete before disabling MPU */
	__DMB();

	/* Disable MPU */
	MPU->CTRL = 0;
}

#if defined(CONFIG_USERSPACE)
/**
 * @brief update configuration of an active memory partition
 */
void arm_core_mpu_mem_partition_config_update(
	struct k_mem_partition *partition,
	k_mem_partition_attr_t *new_attr)
{
	/* Find the partition. ASSERT if not found. */
	u8_t i;
	u8_t reg_index = get_num_regions();

	for (i = get_dyn_region_min_index(); i < get_num_regions(); i++) {
		if (!is_enabled_region(i)) {
			continue;
		}

		u32_t base = mpu_region_get_base(i);

		if (base != partition->start) {
			continue;
		}

		u32_t size = mpu_region_get_size(i);

		if (size != partition->size) {
			continue;
		}

		/* Region found */
		reg_index = i;
		break;
	}
	__ASSERT(reg_index != get_num_regions(),
		 "Memory domain partition not found\n");

	/* Modify the permissions */
	partition->attr = *new_attr;
	mpu_configure_region(reg_index, partition);
}

/**
 * @brief get the maximum number of available (free) MPU region indices
 *        for configuring dynamic MPU partitions
 */
int arm_core_mpu_get_max_available_dyn_regions(void)
{
	return get_num_regions() - static_regions_num;
}

/**
 * @brief validate the given buffer is user accessible or not
 *
 * Presumes the background mapping is NOT user accessible.
 */
int arm_core_mpu_buffer_validate(void *addr, size_t size, int write)
{
	return mpu_buffer_validate(addr, size, write);
}

#endif /* CONFIG_USERSPACE */

/**
 * @brief configure fixed (static) MPU regions.
 */
void arm_core_mpu_configure_static_mpu_regions(const struct k_mem_partition
	*static_regions[], const u8_t regions_num,
	const u32_t background_area_start, const u32_t background_area_end)
{
	if (mpu_configure_static_mpu_regions(static_regions, regions_num,
					       background_area_start, background_area_end) == -EINVAL) {

		__ASSERT(0, "Configuring %u static MPU regions failed\n",
			regions_num);
	}
}

#if defined(CONFIG_MPU_REQUIRES_NON_OVERLAPPING_REGIONS)
/**
 * @brief mark memory areas for dynamic region configuration
 */
void arm_core_mpu_mark_areas_for_dynamic_regions(
	const struct k_mem_partition dyn_region_areas[],
	const u8_t dyn_region_areas_num)
{
	if (mpu_mark_areas_for_dynamic_regions(dyn_region_areas,
						 dyn_region_areas_num) == -EINVAL) {

		__ASSERT(0, "Marking %u areas for dynamic regions failed\n",
			dyn_region_areas_num);
	}
}
#endif /* CONFIG_MPU_REQUIRES_NON_OVERLAPPING_REGIONS */

/**
 * @brief configure dynamic MPU regions.
 */
void arm_core_mpu_configure_dynamic_mpu_regions(const struct k_mem_partition
	*dynamic_regions[], u8_t regions_num)
{
	if (mpu_configure_dynamic_mpu_regions(dynamic_regions, regions_num)
		== -EINVAL) {

		__ASSERT(0, "Configuring %u dynamic MPU regions failed\n",
			regions_num);
	}
}

/* ARM MPU Driver Initial Setup */

/*
 * @brief MPU default configuration
 *
 * This function provides the default configuration mechanism for the Memory
 * Protection Unit (MPU).
 */
static int arm_mpu_init(struct device *arg)
{
	u32_t r_index;

	if (mpu_config.num_regions > get_num_regions()) {
		/* Attempt to configure more MPU regions than
		 * what is supported by hardware. As this operation
		 * is executed during system (pre-kernel) initialization,
		 * we want to ensure we can detect an attempt to
		 * perform invalid configuration.
		 */
		__ASSERT(0,
			"Request to configure: %u regions (supported: %u)\n",
			mpu_config.num_regions,
			get_num_regions()
		);
		return -1;
	}

	LOG_DBG("total region count: %d", get_num_regions());

	arm_core_mpu_disable();

	/* Architecture-specific configuration */
	mpu_init();

	/* Program fixed regions configured at SOC definition. */
	for (r_index = 0U; r_index < mpu_config.num_regions; r_index++) {
		region_init(r_index, &mpu_config.mpu_regions[r_index]);
	}

	/* Update the number of programmed MPU regions. */
	static_regions_num = mpu_config.num_regions;


	arm_core_mpu_enable();

	/* Sanity check for number of regions in Cortex-M0+, M3, and M4. */
#if defined(CONFIG_CPU_CORTEX_M0PLUS) || \
	defined(CONFIG_CPU_CORTEX_M3) || \
	defined(CONFIG_CPU_CORTEX_M4)
	__ASSERT(
		(MPU->TYPE & MPU_TYPE_DREGION_Msk) >> MPU_TYPE_DREGION_Pos == 8,
		"Invalid number of MPU regions\n");
#endif
	return 0;
}

SYS_INIT(arm_mpu_init, PRE_KERNEL_1,
	 CONFIG_KERNEL_INIT_PRIORITY_DEFAULT);
