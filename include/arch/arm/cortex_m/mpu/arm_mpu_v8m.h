/*
 * Copyright (c) 2018 Linaro Limited.
 * Copyright (c) 2018 Nordic Semiconductor ASA.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Privileged No Access, Unprivileged No Access */
/*#define NO_ACCESS       0x0 */
/*#define NO_ACCESS_Msk   ((NO_ACCESS << MPU_RASR_AP_Pos) & MPU_RASR_AP_Msk) */
/* Privileged No Access, Unprivileged No Access */
/*#define P_NA_U_NA       0x0 */
/*#define P_NA_U_NA_Msk   ((P_NA_U_NA << MPU_RASR_AP_Pos) & MPU_RASR_AP_Msk) */
/* Privileged Read Write, Unprivileged No Access */
#define P_RW_U_NA       0x0
#define P_RW_U_NA_Msk   ((P_RW_U_NA << MPU_RBAR_AP_Pos) & MPU_RBAR_AP_Msk)
/* Privileged Read Write, Unprivileged Read Only */
/*#define P_RW_U_RO       0x2 */
/*#define P_RW_U_RO_Msk   ((P_RW_U_RO << MPU_RASR_AP_Pos) & MPU_RASR_AP_Msk)*/
/* Privileged Read Write, Unprivileged Read Write */
#define P_RW_U_RW       0x1
#define P_RW_U_RW_Msk   ((P_RW_U_RW << MPU_RBAR_AP_Pos) & MPU_RBAR_AP_Msk)
/* Privileged Read Write, Unprivileged Read Write */
#define FULL_ACCESS     0x1
#define FULL_ACCESS_Msk ((FULL_ACCESS << MPU_RBAR_AP_Pos) & MPU_RBAR_AP_Msk)
/* Privileged Read Only, Unprivileged No Access */
#define P_RO_U_NA       0x2
#define P_RO_U_NA_Msk   ((P_RO_U_NA << MPU_RBAR_AP_Pos) & MPU_RBAR_AP_Msk)
/* Privileged Read Only, Unprivileged Read Only */
#define P_RO_U_RO       0x3
#define P_RO_U_RO_Msk   ((P_RO_U_RO << MPU_RBAR_AP_Pos) & MPU_RBAR_AP_Msk)
/* Privileged Read Only, Unprivileged Read Only */
#define RO              0x3
#define RO_Msk          ((RO << MPU_RBAR_AP_Pos) & MPU_RBAR_AP_Msk)

/* Attribute flag for not-allowing execution (eXecute Never) */
#define NOT_EXEC MPU_RBAR_XN_Msk

/* Attribute flags for share-ability */
#define NON_SHAREABLE       0x0
#define NON_SHAREABLE_Msk \
	((NON_SHAREABLE << MPU_RBAR_SH_Pos) & MPU_RBAR_SH_Msk)
#define OUTER_SHAREABLE 0x2
#define OUTER_SHAREABLE_Msk \
	((OUTER_SHAREABLE << MPU_RBAR_SH_Pos) & MPU_RBAR_SH_Msk)
#define INNER_SHAREABLE 0x3
#define INNER_SHAREABLE_Msk \
	((INNER_SHAREABLE << MPU_RBAR_SH_Pos) & MPU_RBAR_SH_Msk)

/* Helper define to calculate the region limit address. */
#define REGION_LIMIT_ADDR(base, size) \
	(((base & MPU_RBAR_BASE_Msk) + size - 1) & MPU_RLAR_LIMIT_Msk)


/* Attribute flags for cache-ability */

/* Read/Write Allocation Configurations for Cacheable Memory */
#define R_NON_W_NON     0x0 /* Do not allocate Read/Write */
#define R_NON_W_ALLOC   0x1 /* Do not allocate Read, Allocate Write */
#define R_ALLOC_W_NON   0x2 /* Allocate Read, Do not allocate Write */
#define R_ALLOC_W_ALLOC 0x3 /* Allocate Read/Write */

/* Memory Attributes for Normal Memory */
#define NORMAL_O_WT_NT  0x80 /* Normal, Outer Write-through non-transient */
#define NORMAL_O_WB_NT  0xC0 /* Normal, Outer Write-back non-transient */

#define NORMAL_I_WT_NT  0x08 /* Normal, Inner Write-through non-transient */
#define NORMAL_I_WB_NT  0x0C /* Normal, Inner Write-back non-transient */

#define NORMAL_OUTER_INNER_WRITE_THROUGH_READ_ALLOCATE_NON_TRANS \
	((NORMAL_O_WT_NT | (R_ALLOC_W_NON << 4)) \
	 | \
	 (NORMAL_I_WT_NT | R_ALLOC_W_NON)) \

#define NORMAL_OUTER_INNER_WRITE_BACK_WRITE_READ_ALLOCATE_NON_TRANS \
	((NORMAL_O_WB_NT | (R_ALLOC_W_ALLOC << 4)) \
	 | \
	 (NORMAL_I_WB_NT | R_ALLOC_W_ALLOC))

/* Common cache-ability configuration for Flash, SRAM regions */
#define MPU_CACHE_ATTRIBUTES_FLASH \
	NORMAL_OUTER_INNER_WRITE_THROUGH_READ_ALLOCATE_NON_TRANS
#define MPU_CACHE_ATTRIBUTES_SRAM \
	NORMAL_OUTER_INNER_WRITE_BACK_WRITE_READ_ALLOCATE_NON_TRANS

/* Global MAIR configurations */
#define MPU_MAIR_ATTR_FLASH  MPU_CACHE_ATTRIBUTES_FLASH
#define MPU_MAIR_INDEX_FLASH 0
#define MPU_MAIR_ATTR_SRAM   MPU_CACHE_ATTRIBUTES_SRAM
#define MPU_MAIR_INDEX_SRAM  1

/* Some helper defines for common regions */
#define REGION_RAM_ATTR(base, size) \
	{\
		.rbar = NOT_EXEC | \
			P_RW_U_NA_Msk | NON_SHAREABLE_Msk, /* AP, XN, SH */ \
		/* Cache-ability */ \
		.mair_idx = MPU_MAIR_INDEX_SRAM, \
		.r_limit = REGION_LIMIT_ADDR(base, size),  /* Region Limit */ \
	}

#if defined(CONFIG_MPU_ALLOW_FLASH_WRITE)
/* Note that the access permissions allow for un-privileged writes, contrary
 * to ARMv7-M where un-privileged code has Read-Only permissions.
 */
#define REGION_FLASH_ATTR(base, size) \
	{\
		.rbar = P_RW_U_RW_Msk | NON_SHAREABLE_Msk, /* AP, XN, SH */ \
		/* Cache-ability */ \
		.mair_idx = MPU_MAIR_INDEX_FLASH, \
		.r_limit = REGION_LIMIT_ADDR(base, size),  /* Region Limit */ \
	}
#else /* CONFIG_MPU_ALLOW_FLASH_WRITE */
#define REGION_FLASH_ATTR(base, size) \
	{\
		.rbar = RO_Msk | NON_SHAREABLE_Msk, /* AP, XN, SH */ \
		/* Cache-ability */ \
		.mair_idx = MPU_MAIR_INDEX_FLASH, \
		.r_limit = REGION_LIMIT_ADDR(base, size),  /* Region Limit */ \
	}
#endif /* CONFIG_MPU_ALLOW_FLASH_WRITE */

struct arm_mpu_region_attr {
	/* Attributes belonging to RBAR */
	u8_t rbar: 5;
	 /* MAIR index for attribute indirection */
	u8_t mair_idx: 3;
	 /* Region Limit Address value to be written to the RLAR register. */
	u32_t r_limit;
};

 typedef struct arm_mpu_region_attr arm_mpu_region_attr_t;
