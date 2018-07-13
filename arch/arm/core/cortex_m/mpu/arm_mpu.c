/*
 * Copyright (c) 2017 Linaro Limited.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <device.h>
#include <init.h>
#include <kernel.h>
#include <soc.h>
#include <arch/arm/cortex_m/cmsis.h>
#include <arch/arm/cortex_m/mpu/arm_mpu.h>
#include <arch/arm/cortex_m/mpu/arm_core_mpu.h>
#include <logging/sys_log.h>
#include <linker/linker-defs.h>

/**
 *  Get the number of supported MPU regions.
 */
static inline u8_t _get_num_regions(void)
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

/* This internal function performs MPU region initialization.
 *
 * Note:
 *   The caller must provide a valid region index.
 */
static void _region_init(u32_t index, struct arm_mpu_region *region_conf)
{
	/* Select the region you want to access */
	MPU->RNR = index;
	/* Configure the region */
	MPU->RBAR = (region_conf->base & MPU_RBAR_ADDR_Msk)
				| MPU_RBAR_VALID_Msk | index;
	MPU->RASR = region_conf->attr | MPU_RASR_ENABLE_Msk;
	SYS_LOG_DBG("[%d] 0x%08x 0x%08x",
		index, region_conf->base, region_conf->attr);
}

#if defined(CONFIG_USERSPACE) || defined(CONFIG_MPU_STACK_GUARD) || \
	defined(CONFIG_APPLICATION_MEMORY)

/**
 * Generate the value of the MPU Region Attribute and Size Register
 * (MPU_RASR) that corresponds to the supplied MPU region attributes.
 * This function is internal to the driver.
 */
static inline u32_t _get_region_attr(u32_t xn, u32_t ap, u32_t tex,
				     u32_t c, u32_t b, u32_t s,
				     u32_t srd, u32_t size)
{
	return (((xn << MPU_RASR_XN_Pos) & MPU_RASR_XN_Msk)
		| ((ap << MPU_RASR_AP_Pos) & MPU_RASR_AP_Msk)
		| ((tex << MPU_RASR_TEX_Pos) & MPU_RASR_TEX_Msk)
		| ((s << MPU_RASR_S_Pos) & MPU_RASR_S_Msk)
		| ((c << MPU_RASR_C_Pos) & MPU_RASR_C_Msk)
		| ((b << MPU_RASR_B_Pos) & MPU_RASR_B_Msk)
		| ((srd << MPU_RASR_SRD_Pos) & MPU_RASR_SRD_Msk)
		| (size));
}

/**
 * This internal function converts the region size to
 * the SIZE field value of MPU_RASR.
 */
static inline u32_t _size_to_mpu_rasr_size(u32_t size)
{
	/* The minimal supported region size is 32 bytes */
	if (size <= 32) {
		return REGION_32B;
	}

	/*
	 * A size value greater than 2^31 could not be handled by
	 * round_up_to_next_power_of_two() properly. We handle
	 * it separately here.
	 */
	if (size > (1 << 31)) {
		return REGION_4G;
	}

	size = 1 << (32 - __builtin_clz(size - 1));
	return (32 - __builtin_clz(size) - 2) << 1;
}

/**
 * This internal function is utilized by the MPU driver to parse the intent
 * type (i.e. THREAD_STACK_REGION) and return the correct parameter set.
 */
static inline u32_t _get_region_attr_by_type(u32_t type, u32_t size)
{
	int region_size = _size_to_mpu_rasr_size(size);

	switch (type) {
#ifdef CONFIG_USERSPACE
	case THREAD_STACK_REGION:
		return _get_region_attr(1, P_RW_U_RW, 1, 1, 1,
					1, 0, region_size);
#endif
#ifdef CONFIG_MPU_STACK_GUARD
	case THREAD_STACK_GUARD_REGION:
		return _get_region_attr(1, P_RO_U_NA, 1, 1, 1,
					1, 0, region_size);
#endif
#ifdef CONFIG_APPLICATION_MEMORY
	case THREAD_APP_DATA_REGION:
		return _get_region_attr(1, P_RW_U_RW, 1, 1, 1,
					1, 0, region_size);
#endif
	default:
		/* Size 0 region */
		return 0;
	}
}

/**
 * This internal function is utilized by the MPU driver to combine a given
 * MPU attribute configuration and region size and return the correct
 * parameter set.
 */
static inline u32_t _get_region_attr_by_conf(u32_t attr, u32_t size)
{
	return attr | _size_to_mpu_rasr_size(size);
}

/**
 * This internal function is utilized by the MPU driver to parse the intent
 * type (i.e. THREAD_STACK_REGION) and return the correct region index.
 */
static inline u32_t _get_region_index_by_type(u32_t type)
{
	u32_t region_index;

	__ASSERT(type < THREAD_MPU_REGION_LAST,
		 "unsupported region type");

	region_index = mpu_config.num_regions + type;

	__ASSERT(region_index < _get_num_regions(),
		 "out of MPU regions, requested %u max is %u",
		 region_index, _get_num_regions() - 1);

	return region_index;
}

/**
 * This internal function disables a given MPU region.
 */
static inline void _disable_region(u32_t r_index)
{
	/* Attempting to configure MPU_RNR with an invalid
	 * region number has unpredictable behavior. Therefore
	 * we add a check before disabling the requested MPU
	 * region.
	 */
	__ASSERT(r_index < _get_num_regions(),
		"Index 0x%x out-of-bound (supported regions: 0x%x)\n",
		r_index,
		_get_num_regions());
	SYS_LOG_DBG("disable region 0x%x", r_index);
	/* Disable region */
	ARM_MPU_ClrRegion(r_index);
}

/**
 * This internal function checks if region is enabled or not.
 *
 * Note:
 *   The caller must provide a valid region number.
 */
static inline int _is_enabled_region(u32_t r_index)
{
	MPU->RNR = r_index;

	return MPU->RASR & MPU_RASR_ENABLE_Msk;
}

/**
 * This internal function checks if the given buffer is in the region.
 *
 * Note:
 *   The caller must provide a valid region number.
 */
static inline int _is_in_region(u32_t r_index, u32_t start, u32_t size)
{
	u32_t r_addr_start;
	u32_t r_size_lshift;
	u32_t r_addr_end;

	MPU->RNR = r_index;
	r_addr_start = MPU->RBAR & MPU_RBAR_ADDR_Msk;
	r_size_lshift = ((MPU->RASR & MPU_RASR_SIZE_Msk) >>
			MPU_RASR_SIZE_Pos) + 1;
	r_addr_end = r_addr_start + (1 << r_size_lshift) - 1;

	if (start >= r_addr_start && (start + size - 1) <= r_addr_end) {
		return 1;
	}

	return 0;
}

/**
 * This internal function returns the access permissions of an MPU region
 * specified by its region index.
 *
 * Note:
 *   The caller must provide a valid region number.
 */
static inline u32_t _get_region_ap(u32_t r_index)
{
	MPU->RNR = r_index;
	return (MPU->RASR & MPU_RASR_AP_Msk) >> MPU_RASR_AP_Pos;
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
}

/**
 * @brief disable the MPU
 */
void arm_core_mpu_disable(void)
{
	/* Disable MPU */
	MPU->CTRL = 0;
}

/**
 * @brief configure the base address and size for an MPU region
 *
 * @param   type    MPU region type
 * @param   base    base address in RAM
 * @param   size    size of the region
 */
void arm_core_mpu_configure(u8_t type, u32_t base, u32_t size)
{
	struct arm_mpu_region region_conf;

	SYS_LOG_DBG("Region info: 0x%x 0x%x", base, size);
	u32_t region_index = _get_region_index_by_type(type);
	region_conf.attr = _get_region_attr_by_type(type, size);
	region_conf.base = base;

	if (region_index >= _get_num_regions()) {
		return;
	}

	_region_init(region_index, &region_conf);
}

#if defined(CONFIG_USERSPACE)
void arm_core_mpu_configure_user_context(struct k_thread *thread)
{
	u32_t base = (u32_t)thread->stack_obj;
	u32_t size = thread->stack_info.size;

	if (!thread->arch.priv_stack_start) {
		_disable_region(_get_region_index_by_type(
			THREAD_STACK_REGION));
		return;
	}
	arm_core_mpu_configure(THREAD_STACK_REGION, base, size);
}

/**
 * @brief configure MPU regions for the memory partitions of the memory domain
 *
 * @param   mem_domain    memory domain that thread belongs to
 */
void arm_core_mpu_configure_mem_domain(struct k_mem_domain *mem_domain)
{
	u32_t region_index =
		_get_region_index_by_type(THREAD_DOMAIN_PARTITION_REGION);
	u32_t num_partitions;
	struct k_mem_partition *pparts;
	struct arm_mpu_region region_conf;

	if (mem_domain) {
		SYS_LOG_DBG("configure domain: %p", mem_domain);
		num_partitions = mem_domain->num_partitions;
		pparts = mem_domain->partitions;
	} else {
		SYS_LOG_DBG("disable domain partition regions");
		num_partitions = 0;
		pparts = NULL;
	}

	for (; region_index < _get_num_regions(); region_index++) {
		if (num_partitions && pparts->size) {
			SYS_LOG_DBG("set region 0x%x 0x%x 0x%x",
				    region_index, pparts->start, pparts->size);
			region_conf.base = pparts->start;
			region_conf.attr =
				_get_region_attr_by_conf(pparts->attr,
					pparts->size);
			_region_init(region_index, &region_conf);
			num_partitions--;
		} else {
			_disable_region(region_index);
		}
		pparts++;
	}
}

/**
 * @brief configure MPU region for a single memory partition
 *
 * @param   part_index  memory partition index
 * @param   part        memory partition info
 */
void arm_core_mpu_configure_mem_partition(u32_t part_index,
					  struct k_mem_partition *part)
{
	u32_t region_index =
		_get_region_index_by_type(THREAD_DOMAIN_PARTITION_REGION);
	struct arm_mpu_region region_conf;

	SYS_LOG_DBG("configure partition index: %u", part_index);

	if (part &&
		(region_index + part_index < _get_num_regions())) {
		SYS_LOG_DBG("set region 0x%x 0x%x 0x%x",
			    region_index + part_index, part->start, part->size);
		region_conf.attr =
			_get_region_attr_by_conf(part->attr, part->size);
		region_conf.base = part->start;
		_region_init(region_index + part_index, &region_conf);
	} else {
		_disable_region(region_index + part_index);
	}
}

/**
 * @brief Reset MPU region for a single memory partition
 *
 * @param   part_index  memory partition index
 */
void arm_core_mpu_mem_partition_remove(u32_t part_index)
{
	u32_t region_index =
		_get_region_index_by_type(THREAD_DOMAIN_PARTITION_REGION);

	_disable_region(region_index + part_index);
}

/**
 * @brief get the maximum number of free regions for memory domain partitions
 */
int arm_core_mpu_get_max_domain_partition_regions(void)
{
	/*
	 * Subtract the start of domain partition regions from total regions
	 * will get the maximum number of free regions for memory domain
	 * partitions.
	 */
	return _get_num_regions() -
		_get_region_index_by_type(THREAD_DOMAIN_PARTITION_REGION);
}

/* Only a single bit is set for all user accessible permissions.
 * In ARMv7-M MPU this is bit AP[1].
 */
#define MPU_USER_READ_ACCESSIBLE_Msk (P_RW_U_RO & P_RW_U_RW & P_RO_U_RO & RO)

/**
 * This internal function checks if the region is user accessible or not.
 *
 * Note:
 *   The caller must provide a valid region number.
 */
static inline int _is_user_accessible_region(u32_t r_index, int write)
{
	u32_t r_ap = _get_region_ap(r_index);

	/* always return true if this is the thread stack region */
	if (_get_region_index_by_type(THREAD_STACK_REGION) == r_index) {
		return 1;
	}

	if (write) {
		return r_ap == P_RW_U_RW;
	}

	return r_ap & MPU_USER_READ_ACCESSIBLE_Msk;
}

/**
 * @brief validate the given buffer is user accessible or not
 *
 * Presumes the background mapping is NOT user accessible.
 */
int arm_core_mpu_buffer_validate(void *addr, size_t size, int write)
{
	s32_t r_index;

	/* Iterate all mpu regions in reversed order */
	for (r_index = _get_num_regions() - 1; r_index >= 0;  r_index--) {
		if (!_is_enabled_region(r_index) ||
		    !_is_in_region(r_index, (u32_t)addr, size)) {
			continue;
		}

		/* For ARM MPU, higher region number takes priority.
		 * Since we iterate all mpu regions in reversed order, so
		 * we can stop the iteration immediately once we find the
		 * matched region that grants permission or denies access.
		 */
		if (_is_user_accessible_region(r_index, write)) {
			return 0;
		} else {
			return -EPERM;
		}
	}

	return -EPERM;
}
#endif /* CONFIG_USERSPACE */
#endif /* USERSPACE || MPU_STACK_GUARD || APPLICATION_MEMORY */

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

	if (mpu_config.num_regions > _get_num_regions()) {
		/* Attempt to configure more MPU regions than
		 * what is supported by hardware. As this operation
		 * is executed during system (pre-kernel) initialization,
		 * we want to ensure we can detect an attempt to
		 * perform invalid configuration.
		 */
		__ASSERT(0,
			"Request to configure: %u regions (supported: %u)\n",
			mpu_config.num_regions,
			_get_num_regions()
		);
		return -1;
	}

	SYS_LOG_DBG("total region count: %d", _get_num_regions());

	/* Disable MPU */
	MPU->CTRL = 0;

	/* Configure regions */
	for (r_index = 0; r_index < mpu_config.num_regions; r_index++) {
		_region_init(r_index, &mpu_config.mpu_regions[r_index]);
	}

	/* Enable MPU and use the default memory map as a
	 * background region for privileged software access.
	 */
	MPU->CTRL = MPU_CTRL_ENABLE_Msk | MPU_CTRL_PRIVDEFENA_Msk;

#if defined(CONFIG_APPLICATION_MEMORY)
	u32_t index, size;
	struct arm_mpu_region region_conf;

	/* configure app data portion */
	index = _get_region_index_by_type(THREAD_APP_DATA_REGION);
	size = (u32_t)&__app_ram_end - (u32_t)&__app_ram_start;
	region_conf.attr =
		_get_region_attr_by_type(THREAD_APP_DATA_REGION, size);
	region_conf.base = (u32_t)&__app_ram_start;
	if (size > 0) {
		_region_init(index, &region_conf);
	}
#endif
	/* Make sure that all the registers are set before proceeding */
	__DSB();
	__ISB();

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
