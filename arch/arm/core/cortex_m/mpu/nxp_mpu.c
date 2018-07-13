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
#include <arch/arm/cortex_m/mpu/nxp_mpu.h>
#include <logging/sys_log.h>
#include <misc/__assert.h>
#include <linker/linker-defs.h>

/* NXP MPU Enabled state */
static u8_t nxp_mpu_enabled;

/**
 * This internal function is utilized by the MPU driver to parse the intent
 * type (i.e. THREAD_STACK_REGION) and return the correct parameter set.
 */
static inline u32_t _get_region_attr_by_type(u32_t type)
{
	switch (type) {
#ifdef CONFIG_USERSPACE
	case THREAD_STACK_REGION:
		return REGION_USER_MODE_ATTR;
#endif
#ifdef CONFIG_MPU_STACK_GUARD
	case THREAD_STACK_GUARD_REGION:
		/* The stack guard region has to be not accessible */
		return REGION_RO_ATTR;
#endif
#ifdef CONFIG_APPLICATION_MEMORY
	case THREAD_APP_DATA_REGION:
		return REGION_USER_MODE_ATTR;
#endif
	default:
		/* Size 0 region */
		return 0;
	}
}

static inline u8_t _get_num_regions(void)
{
	return FSL_FEATURE_SYSMPU_DESCRIPTOR_COUNT;
}

static inline u8_t _get_num_usable_regions(void)
{
	u8_t max = _get_num_regions();
#ifdef CONFIG_MPU_STACK_GUARD
	/* Last region reserved for stack guard.
	 * See comments in nxp_mpu_setup_sram_region()
	 */
	return max - 1;
#else
	return max;
#endif
}

static void _region_init(u32_t index, u32_t region_base,
			 u32_t region_end, u32_t region_attr)
{
	if (index == 0) {
		/* The MPU does not allow writes from the core to affect the
		 * RGD0 start or end addresses nor the permissions associated
		 * with the debugger; it can only write the permission fields
		 * associated with the other masters. These protections
		 * guarantee that the debugger always has access to the entire
		 * address space.
		 */
		__ASSERT(region_base == SYSMPU->WORD[index][0],
			 "Region %d base address got 0x%08x expected 0x%08x",
			 index, region_base, (u32_t)SYSMPU->WORD[index][0]);

		__ASSERT(region_end == SYSMPU->WORD[index][1],
			 "Region %d end address got 0x%08x expected 0x%08x",
			 index, region_end, (u32_t)SYSMPU->WORD[index][1]);

		/* Changes to the RGD0_WORD2 alterable fields should be done
		 * via a write to RGDAAC0.
		 */
		SYSMPU->RGDAAC[index] = region_attr;

	} else {
		SYSMPU->WORD[index][0] = region_base;
		SYSMPU->WORD[index][1] = region_end;
		SYSMPU->WORD[index][2] = region_attr;
		SYSMPU->WORD[index][3] = SYSMPU_WORD_VLD_MASK;
	}

	SYS_LOG_DBG("[%d] 0x%08x 0x%08x 0x%08x 0x%08x", index,
		    SYSMPU->WORD[index][0],
		    SYSMPU->WORD[index][1],
		    SYSMPU->WORD[index][2],
		    SYSMPU->WORD[index][3]);
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

	__ASSERT(region_index < _get_num_usable_regions(),
		 "out of MPU regions, requested %u max is %u",
		 region_index, _get_num_usable_regions() - 1);

	return region_index;
}

/**
 * This internal function checks if region is enabled or not.
 */
static inline int _is_enabled_region(u32_t r_index)
{
	return SYSMPU->WORD[r_index][3] & SYSMPU_WORD_VLD_MASK;
}

/**
 * This internal function checks if the given buffer is in the region.
 */
static inline int _is_in_region(u32_t r_index, u32_t start, u32_t size)
{
	u32_t r_addr_start;
	u32_t r_addr_end;

	r_addr_start = SYSMPU->WORD[r_index][0];
	r_addr_end = SYSMPU->WORD[r_index][1];

	if (start >= r_addr_start && (start + size - 1) <= r_addr_end) {
		return 1;
	}

	return 0;
}

#ifdef CONFIG_MPU_STACK_GUARD
static void nxp_mpu_setup_sram_region(u32_t base, u32_t size)
{
	u32_t last_region = _get_num_regions() - 1;

	/*
	 * The NXP MPU manages the permissions of the overlapping regions
	 * doing the logical OR in between them, hence they can't be used
	 * for stack/stack guard protection. For this reason the last region of
	 * the MPU will be reserved.
	 *
	 * A consequence of this is that the SRAM is split in different
	 * regions. In example if THREAD_STACK_GUARD_REGION is selected:
	 * - SRAM before THREAD_STACK_GUARD_REGION: RW
	 * - SRAM THREAD_STACK_GUARD_REGION: RO
	 * - SRAM after THREAD_STACK_GUARD_REGION: RW
	 */

	/* Configure SRAM_0 region
	 *
	 * The mpu_config.sram_region contains the index of the region in
	 * the mpu_config.mpu_regions array but the region 0 on the NXP MPU
	 * is the background region, so on this MPU the regions are mapped
	 * starting from 1, hence the mpu_config.sram_region on the data
	 * structure is mapped on the mpu_config.sram_region + 1 region of
	 * the MPU.
	 */
	_region_init(mpu_config.sram_region,
		     mpu_config.mpu_regions[mpu_config.sram_region].base,
		     ENDADDR_ROUND(base),
		     mpu_config.mpu_regions[mpu_config.sram_region].attr);

	/* Configure SRAM_1 region */
	_region_init(last_region, base + size,
	   ENDADDR_ROUND(mpu_config.mpu_regions[mpu_config.sram_region].end),
			mpu_config.mpu_regions[mpu_config.sram_region].attr);

}
#endif /* CONFIG_MPU_STACK_GUARD */

/* ARM Core MPU Driver API Implementation for NXP MPU */

/**
 * @brief enable the MPU
 */
void arm_core_mpu_enable(void)
{
	if (nxp_mpu_enabled == 0) {
		/* Enable MPU */
		SYSMPU->CESR |= SYSMPU_CESR_VLD_MASK;

		nxp_mpu_enabled = 1;
	}
}

/**
 * @brief disable the MPU
 */
void arm_core_mpu_disable(void)
{
	if (nxp_mpu_enabled == 1) {
		/* Disable MPU */
		SYSMPU->CESR &= ~SYSMPU_CESR_VLD_MASK;
		/* Clear Interrupts */
		SYSMPU->CESR |=  SYSMPU_CESR_SPERR_MASK;

		nxp_mpu_enabled = 0;
	}
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
	SYS_LOG_DBG("Region info: 0x%x 0x%x", base, size);
	u32_t region_index = _get_region_index_by_type(type);
	u32_t region_attr = _get_region_attr_by_type(type);

	_region_init(region_index, base,
		     ENDADDR_ROUND(base + size),
		     region_attr);
#ifdef CONFIG_MPU_STACK_GUARD
	if (type == THREAD_STACK_GUARD_REGION) {
		nxp_mpu_setup_sram_region(base, size);
	}
#endif
}

#if defined(CONFIG_USERSPACE)
void arm_core_mpu_configure_user_context(struct k_thread *thread)
{
	u32_t base = (u32_t)thread->stack_info.start;
	u32_t size = thread->stack_info.size;
	u32_t index = _get_region_index_by_type(THREAD_STACK_REGION);
	u32_t region_attr = _get_region_attr_by_type(THREAD_STACK_REGION);

	/* configure stack */
	_region_init(index, base, ENDADDR_ROUND(base + size), region_attr);
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
	u32_t region_attr;
	u32_t num_partitions;
	struct k_mem_partition *pparts;

	if (mem_domain) {
		SYS_LOG_DBG("configure domain: %p", mem_domain);
		num_partitions = mem_domain->num_partitions;
		pparts = mem_domain->partitions;
	} else {
		SYS_LOG_DBG("disable domain partition regions");
		num_partitions = 0;
		pparts = NULL;
	}

	/*
	 * Don't touch the last region, it is reserved for SRAM_1 region.
	 * See comments in arm_core_mpu_configure().
	 */
	for (; region_index < _get_num_usable_regions(); region_index++) {
		if (num_partitions && pparts->size) {
			SYS_LOG_DBG("set region 0x%x 0x%x 0x%x",
				    region_index, pparts->start, pparts->size);
			region_attr = pparts->attr;
			_region_init(region_index, pparts->start,
				     ENDADDR_ROUND(pparts->start+pparts->size),
				     region_attr);
			num_partitions--;
		} else {
			SYS_LOG_DBG("disable region 0x%x", region_index);
			/* Disable region */
			SYSMPU->WORD[region_index][0] = 0;
			SYSMPU->WORD[region_index][1] = 0;
			SYSMPU->WORD[region_index][2] = 0;
			SYSMPU->WORD[region_index][3] = 0;
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
	u32_t region_attr;

	SYS_LOG_DBG("configure partition index: %u", part_index);

	if (part) {
		SYS_LOG_DBG("set region 0x%x 0x%x 0x%x",
			    region_index + part_index, part->start, part->size);
		region_attr = part->attr;
		_region_init(region_index + part_index, part->start,
			     ENDADDR_ROUND(part->start + part->size),
			     region_attr);
	} else {
		SYS_LOG_DBG("disable region 0x%x", region_index);
		/* Disable region */
		SYSMPU->WORD[region_index + part_index][0] = 0;
		SYSMPU->WORD[region_index + part_index][1] = 0;
		SYSMPU->WORD[region_index + part_index][2] = 0;
		SYSMPU->WORD[region_index + part_index][3] = 0;
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

	SYS_LOG_DBG("disable region 0x%x", region_index);
	/* Disable region */
	SYSMPU->WORD[region_index + part_index][0] = 0;
	SYSMPU->WORD[region_index + part_index][1] = 0;
	SYSMPU->WORD[region_index + part_index][2] = 0;
	SYSMPU->WORD[region_index + part_index][3] = 0;
}

/**
 * @brief get the maximum number of free regions for memory domain partitions
 */
int arm_core_mpu_get_max_domain_partition_regions(void)
{
	/*
	 * Subtract the start of domain partition regions from total regions
	 * should get the maximum number of free regions for memory domain
	 * partitions
	 */
	return _get_num_usable_regions() -
		_get_region_index_by_type(THREAD_DOMAIN_PARTITION_REGION);
}

/**
 * This internal function checks if the region is user accessible or not
 */
static inline int _is_user_accessible_region(u32_t r_index, int write)
{
	u32_t r_ap = SYSMPU->WORD[r_index][2];

	/* always return true if this is the thread stack region */
	if (_get_region_index_by_type(THREAD_STACK_REGION) == r_index) {
		return 1;
	}

	if (write) {
		return (r_ap & MPU_REGION_WRITE) == MPU_REGION_WRITE;
	}

	return (r_ap & MPU_REGION_READ) == MPU_REGION_READ;
}

/**
 * @brief validate the given buffer is user accessible or not
 */
int arm_core_mpu_buffer_validate(void *addr, size_t size, int write)
{
	u8_t r_index;

	/* Iterate all MPU regions */
	for (r_index = 0; r_index < _get_num_usable_regions(); r_index++) {
		if (!_is_enabled_region(r_index) ||
		    !_is_in_region(r_index, (u32_t)addr, size)) {
			continue;
		}

		/* For NXP MPU, priority given to granting permission over
		 * denying access for overlapping region.
		 * So we can stop the iteration immediately once we find the
		 * matched region that grants permission.
		 */
		if (_is_user_accessible_region(r_index, write)) {
			return 0;
		}
	}

	return -EPERM;
}
#endif /* CONFIG_USERSPACE */

/* NXP MPU Driver Initial Setup */

/*
 * @brief MPU default configuration
 *
 * This function provides the default configuration mechanism for the Memory
 * Protection Unit (MPU).
 */
static void _nxp_mpu_config(void)
{
	u32_t r_index;

	__ASSERT(mpu_config.num_regions <= _get_num_regions(),
		 "too many static MPU regions defined");
	SYS_LOG_DBG("total region count: %d", _get_num_regions());

	/* Disable MPU */
	SYSMPU->CESR &= ~SYSMPU_CESR_VLD_MASK;
	/* Clear Interrupts */
	SYSMPU->CESR |=  SYSMPU_CESR_SPERR_MASK;

	/* MPU Configuration */

	/* Configure regions */
	for (r_index = 0; r_index < mpu_config.num_regions; r_index++) {
		_region_init(r_index,
			     mpu_config.mpu_regions[r_index].base,
			     mpu_config.mpu_regions[r_index].end,
			     mpu_config.mpu_regions[r_index].attr);
	}

	/* Enable MPU */
	SYSMPU->CESR |= SYSMPU_CESR_VLD_MASK;

	nxp_mpu_enabled = 1;

#if defined(CONFIG_APPLICATION_MEMORY)
	u32_t index, region_attr, base, size;

	/* configure app data portion */
	index = _get_region_index_by_type(THREAD_APP_DATA_REGION);
	region_attr = _get_region_attr_by_type(THREAD_APP_DATA_REGION);
	base = (u32_t)&__app_ram_start;
	size = (u32_t)&__app_ram_end - (u32_t)&__app_ram_start;

	/* set up app data region if exists, otherwise disable */
	if (size > 0) {
		_region_init(index, base, ENDADDR_ROUND(base + size),
			     region_attr);
	} else {
		SYSMPU->WORD[index][3] = 0;
	}
#endif
	/* Make sure that all the registers are set before proceeding */
	__DSB();
	__ISB();
}

/*
 * @brief MPU clock configuration
 *
 * This function provides the clock configuration for the Memory Protection
 * Unit (MPU).
 */
static void _nxp_mpu_clock_cfg(void)
{
	/* Enable Clock */
	CLOCK_EnableClock(kCLOCK_Sysmpu0);
}

static int nxp_mpu_init(struct device *arg)
{
	ARG_UNUSED(arg);

	_nxp_mpu_clock_cfg();

	_nxp_mpu_config();

	return 0;
}

#if defined(CONFIG_SYS_LOG)
/* To have logging the driver needs to be initialized later */
SYS_INIT(nxp_mpu_init, POST_KERNEL,
	 CONFIG_KERNEL_INIT_PRIORITY_DEFAULT);
#else
SYS_INIT(nxp_mpu_init, PRE_KERNEL_1,
	 CONFIG_KERNEL_INIT_PRIORITY_DEFAULT);
#endif
