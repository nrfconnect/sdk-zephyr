/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef IRONSIDE_SE_EVENT_REPORT_H_
#define IRONSIDE_SE_EVENT_REPORT_H_

#include <ironside/se/memory_map.h>
#include <ironside/se/internal/mdk.h>

#include <nrfx.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(IRONSIDE_SE_EVENT_REPORT_ADDRESS)
/** Event report for the current processor. */
#define IRONSIDE_SE_EVENT_REPORT                                                                   \
	((struct ironside_se_event_report *)IRONSIDE_SE_EVENT_REPORT_ADDRESS)
#endif /* defined(IRONSIDE_SE_EVENT_REPORT_ADDRESS) */

/** @brief Index for given SPU within the event report.
 */
enum ironside_se_event_report_spu {
	IRONSIDE_SE_EVENT_REPORT_SPU110_IDX = 0,
	IRONSIDE_SE_EVENT_REPORT_SPU111_IDX,
	IRONSIDE_SE_EVENT_REPORT_SPU120_IDX,
	IRONSIDE_SE_EVENT_REPORT_SPU121_IDX,
	IRONSIDE_SE_EVENT_REPORT_SPU122_IDX,
	IRONSIDE_SE_EVENT_REPORT_SPU130_IDX,
	IRONSIDE_SE_EVENT_REPORT_SPU131_IDX,
	IRONSIDE_SE_EVENT_REPORT_SPU132_IDX,
	IRONSIDE_SE_EVENT_REPORT_SPU133_IDX,
	IRONSIDE_SE_EVENT_REPORT_SPU134_IDX,
	IRONSIDE_SE_EVENT_REPORT_SPU135_IDX,
	IRONSIDE_SE_EVENT_REPORT_SPU136_IDX,
	IRONSIDE_SE_EVENT_REPORT_SPU137_IDX,
};
#define IRONSIDE_SE_EVENT_REPORT_SPU_NUM (IRONSIDE_SE_EVENT_REPORT_SPU137_IDX + 1)

/** @brief Index for given MPC within the event report.
 */
enum ironside_se_event_report_mpc {
	IRONSIDE_SE_EVENT_REPORT_MPC110_IDX = 0,
	IRONSIDE_SE_EVENT_REPORT_MPC111_IDX,
	IRONSIDE_SE_EVENT_REPORT_MPC120_IDX,
	IRONSIDE_SE_EVENT_REPORT_MPC130_IDX,
};
#define IRONSIDE_SE_EVENT_REPORT_MPC_NUM (IRONSIDE_SE_EVENT_REPORT_MPC130_IDX + 1)

/** @brief Index for given MRAMC within the event report.
 */
enum ironside_se_event_report_mramc {
	IRONSIDE_SE_EVENT_REPORT_MRAMC110_IDX = 0,
	IRONSIDE_SE_EVENT_REPORT_MRAMC111_IDX,
};
#define IRONSIDE_SE_EVENT_REPORT_MRAMC_NUM (IRONSIDE_SE_EVENT_REPORT_MRAMC111_IDX + 1)

/* SPU_INFO @Bits 0..3 contains data from SPU.PERIPHACCERR.INFO register */
#define IRONSIDE_SE_SPU_PERIPHACCERR_INFO_Pos (0UL)
#define IRONSIDE_SE_SPU_PERIPHACCERR_INFO_Msk (0xfUL << IRONSIDE_SE_SPU_PERIPHACCERR_INFO_Pos)

/* SPU_ADDRESS @Bits 4..19 contains data from SPU.PERIPHACCERR.ADDRESS register */
#define IRONSIDE_SE_SPU_PERIPHACCERR_ADDRESS_Pos (4UL)
#define IRONSIDE_SE_SPU_PERIPHACCERR_ADDRESS_Msk                                                   \
	(0xffffUL << IRONSIDE_SE_SPU_PERIPHACCERR_ADDRESS_Pos)

/* SPU_EVENT @Bits 31 contains data from SPU.EVENTS_PERIPHACCERR register */
#define IRONSIDE_SE_SPU_PERIPHACCERR_EVENT_Pos (31UL)
#define IRONSIDE_SE_SPU_PERIPHACCERR_EVENT_Msk (1UL << IRONSIDE_SE_SPU_PERIPHACCERR_EVENT_Pos)

/* MPC_INFO @Bits 0..16 contains data from MPC.MEMACCERR.INFO register */
#define IRONSIDE_SE_MPC_MEMACCERR_INFO_Pos (0UL)
#define IRONSIDE_SE_MPC_MEMACCERR_INFO_Msk (0x1ffffUL << IRONSIDE_SE_MPC_MEMACCERR_INFO_Pos)

/* MPC_EVENT @Bits 31 contains data from MPC.EVENTS_MEMACCERR register */
#define IRONSIDE_SE_MPC_MEMACCERR_EVENT_Pos (31UL)
#define IRONSIDE_SE_MPC_MEMACCERR_EVENT_Msk (1UL << IRONSIDE_SE_MPC_MEMACCERR_EVENT_Pos)

/** @brief MPC.MEMACCERR structure. See IRONSIDE_SE_MPC_MEMACCERR_.*
 */
struct ironside_se_mpc_memaccerr {
	uint32_t address; /* Data from MPC.MEMACCERR.ADDRESS */
	uint32_t info;    /* Data from MPC.MEMACCERR.INFO */
};

/** @brief Top level event report structure. Use 'ironside_se_.*_get' to read the data.
 */
struct ironside_se_event_report {
	/* SPU error event */
	uint32_t periphaccerr[IRONSIDE_SE_EVENT_REPORT_SPU_NUM];

	/* MPU memory access error list */
	struct ironside_se_mpc_memaccerr memaccerr[IRONSIDE_SE_EVENT_REPORT_MPC_NUM];

	/* MRAMC ECC error event address */
	uint32_t mramc_ecc_error[IRONSIDE_SE_EVENT_REPORT_MRAMC_NUM];

	/* MRAMC ECC error corrected address */
	uint32_t mramc_ecc_errorcorr[IRONSIDE_SE_EVENT_REPORT_MRAMC_NUM];

	/* MRAMC access error. Any non-0 value indicates that this event is reported */
	uint32_t mramc_accesserr[IRONSIDE_SE_EVENT_REPORT_MRAMC_NUM];
};

/**
 * @brief Function to check if SPU.PERIPHACCERR event is set
 *
 * @param report Pointer to the event report structure
 * @param spu SPU to check
 *
 * @retval true if event is set
 * @retval false if event is not set
 */
static inline bool
ironside_se_spu_periphaccerr_event_check(const struct ironside_se_event_report *report,
					 enum ironside_se_event_report_spu spu)
{
	return ((report->periphaccerr[spu] & IRONSIDE_SE_SPU_PERIPHACCERR_EVENT_Msk) >>
		IRONSIDE_SE_SPU_PERIPHACCERR_EVENT_Pos) ==
	       SPU_EVENTS_PERIPHACCERR_EVENTS_PERIPHACCERR_Generated;
}

/**
 * @brief Function to clear SPU.PERIPHACCERR event.
 *
 * Once this event has been set, this must be called for future events to be reported.
 *
 * @param report Pointer to the event report structure
 * @param spu SPU to clear event for.
 */
static inline void ironside_se_spu_periphaccerr_event_clear(struct ironside_se_event_report *report,
							    enum ironside_se_event_report_spu spu)
{
	report->periphaccerr[spu] =
		(report->periphaccerr[spu] & ~IRONSIDE_SE_SPU_PERIPHACCERR_EVENT_Msk) |
		(SPU_EVENTS_PERIPHACCERR_EVENTS_PERIPHACCERR_NotGenerated
		 << IRONSIDE_SE_SPU_PERIPHACCERR_EVENT_Pos);
}

/**
 * @brief Function to get the data from SPU.PERIPHACCERR for a given SPU.
 *
 * @param report Pointer to the event report structure
 * @param spu SPU to get data from.
 * @param[out] periphaccerr Pointer to where to store the data.
 */
static inline void ironside_se_spu_periphaccerr_get(const struct ironside_se_event_report *report,
						    enum ironside_se_event_report_spu spu,
						    NRF_SPU_PERIPHACCERR_Type *periphaccerr)
{
	uint32_t report_periphaccerr = report->periphaccerr[spu];

	*(uint32_t *)&periphaccerr->ADDRESS =
		(report_periphaccerr & IRONSIDE_SE_SPU_PERIPHACCERR_ADDRESS_Msk) >>
		IRONSIDE_SE_SPU_PERIPHACCERR_ADDRESS_Pos;
	*(uint32_t *)&periphaccerr->INFO =
		(report_periphaccerr & IRONSIDE_SE_SPU_PERIPHACCERR_INFO_Msk) >>
		IRONSIDE_SE_SPU_PERIPHACCERR_INFO_Pos;
}

/**
 * @brief Function to check if MPC.MEMACCERR event is set
 *
 * @param report Pointer to the event report structure
 * @param mpc MPC to check
 *
 * @retval true if event is set
 * @retval false if event is not set
 */
static inline bool
ironside_se_mpc_memaccerr_event_check(const struct ironside_se_event_report *report,
				      enum ironside_se_event_report_mpc mpc)
{
	const struct ironside_se_mpc_memaccerr *mpc_err = &report->memaccerr[mpc];

	return ((mpc_err->info & IRONSIDE_SE_MPC_MEMACCERR_EVENT_Msk) >>
		IRONSIDE_SE_MPC_MEMACCERR_EVENT_Pos) ==
	       MPC_EVENTS_MEMACCERR_EVENTS_MEMACCERR_Generated;
}

/**
 * @brief Function to clear MPC.MEMACCERR event
 *
 * Once this event has been set, this must be called for future events to be reported.
 *
 * @param report Pointer to the event report structure
 * @param mpc MPC to clear event for.
 */
static inline void ironside_se_mpc_memaccerr_event_clear(struct ironside_se_event_report *report,
							 enum ironside_se_event_report_mpc mpc)
{
	struct ironside_se_mpc_memaccerr *mpc_err = &report->memaccerr[mpc];

	mpc_err->info = (mpc_err->info & ~IRONSIDE_SE_MPC_MEMACCERR_EVENT_Msk) |
			(MPC_EVENTS_MEMACCERR_EVENTS_MEMACCERR_NotGenerated
			 << IRONSIDE_SE_MPC_MEMACCERR_EVENT_Pos);
}

/**
 * @brief Function to get the data from MPC.MEMACCERR for a given MPC.
 *
 * @param report Pointer to the event report structure
 * @param mpc MPC to get data from.
 * @param[out] memaccerr Pointer to where to store the data.
 */
static inline void ironside_se_mpc_memaccerr_get(const struct ironside_se_event_report *report,
						 enum ironside_se_event_report_mpc mpc,
						 NRF_MPC_MEMACCERR_Type *memaccerr)
{
	const struct ironside_se_mpc_memaccerr *mpc_err = &report->memaccerr[mpc];

	*(uint32_t *)&memaccerr->ADDRESS = mpc_err->address;
	*(uint32_t *)&memaccerr->INFO = (mpc_err->info & IRONSIDE_SE_MPC_MEMACCERR_INFO_Msk) >>
					IRONSIDE_SE_MPC_MEMACCERR_INFO_Pos;
}

/**
 * @brief Function to check if MRAMC.ECC.ERROR event is set
 *
 * @param report Pointer to the event report structure
 * @param mramc MRAMC to check
 *
 * @retval true if event is set
 * @retval false if event is not set
 */
static inline bool
ironside_se_mramc_ecc_error_event_check(const struct ironside_se_event_report *report,
					enum ironside_se_event_report_mramc mramc)
{
	return report->mramc_ecc_error[mramc] != 0;
}

/**
 * @brief Function to clear MRAMC.ECC.ERROR event.
 *
 * Once this event has been set, this must be called for future events to be reported.
 *
 * @param report Pointer to the event report structure
 * @param mramc MRAMC to clear event for.
 */
static inline void
ironside_se_mramc_ecc_error_event_clear(struct ironside_se_event_report *report,
					enum ironside_se_event_report_mramc mramc)
{
	report->mramc_ecc_error[mramc] = 0;
}

/**
 * @brief Function to get the data from MRAMC.ECC.ERROR for a given MRAMC.
 *
 * @param report Pointer to the event report structure
 * @param mramc MRAMC to get data from.
 * @param[out] ecc_erroraddr Pointer to where to store the data.
 */
static inline void
ironside_se_mramc_ecc_erroraddr_get(const struct ironside_se_event_report *report,
				    enum ironside_se_event_report_mramc mramc,
				    uint32_t *ecc_erroraddr)
{
	*ecc_erroraddr = report->mramc_ecc_error[mramc];
}

/**
 * @brief Function to check if MRAMC.ECC.ERRORCORR event is set
 *
 * @param report Pointer to the event report structure
 * @param mramc MRAMC to check
 *
 * @retval true if event is set
 * @retval false if event is not set
 */
static inline bool
ironside_se_mramc_ecc_errorcorr_event_check(const struct ironside_se_event_report *report,
					    enum ironside_se_event_report_mramc mramc)
{
	return report->mramc_ecc_errorcorr[mramc] != 0;
}

/**
 * @brief Function to clear MRAMC.ECC.ERRORCORR event.
 *
 * Once this event has been set, this must be called for future events to be reported.
 *
 * @param report Pointer to the event report structure
 * @param mramc MRAMC to clear event for.
 */
static inline void
ironside_se_mramc_ecc_errorcorr_event_clear(struct ironside_se_event_report *report,
					    enum ironside_se_event_report_mramc mramc)
{
	report->mramc_ecc_errorcorr[mramc] = 0;
}

/**
 * @brief Function to get the data from MRAMC.ECC.ERRORCORR for a given MRAMC.
 *
 * @param report Pointer to the event report structure
 * @param mramc MRAMC to get data from.
 * @param[out] ecc_corraddr Pointer to where to store the data.
 */
static inline void ironside_se_mramc_ecc_corraddr_get(const struct ironside_se_event_report *report,
						      enum ironside_se_event_report_mramc mramc,
						      uint32_t *ecc_corraddr)
{
	*ecc_corraddr = report->mramc_ecc_errorcorr[mramc];
}

/**
 * @brief Function to check if MRAMC.ACCESSERR event is set
 *
 * @param report Pointer to the event report structure
 * @param mramc MRAMC to check
 *
 * @retval true if event is set
 * @retval false if event is not set
 */
static inline bool
ironside_se_mramc_accesserr_event_check(const struct ironside_se_event_report *report,
					enum ironside_se_event_report_mramc mramc)
{
	return report->mramc_accesserr[mramc] != 0;
}

/**
 * @brief Function to clear MRAMC.ACCESSERR event.
 *
 * Once this event has been set, this must be called for future events to be reported.
 *
 * @param report Pointer to the event report structure
 * @param mramc MRAMC to clear event for.
 */
static inline void
ironside_se_mramc_accesserr_event_clear(struct ironside_se_event_report *report,
					enum ironside_se_event_report_mramc mramc)
{
	report->mramc_accesserr[mramc] = 0;
}

/** @brief Mapping from ironside_se_event_report_spu to instance address to assist with logging.
 */
#define IRONSIDE_SE_EVENT_REPORT_SPU_ADDRESS_ARRAY                                                 \
	{                                                                                          \
		[IRONSIDE_SE_EVENT_REPORT_SPU110_IDX] = 0x5F080000UL,                              \
		[IRONSIDE_SE_EVENT_REPORT_SPU111_IDX] = 0x5F090000UL,                              \
		[IRONSIDE_SE_EVENT_REPORT_SPU120_IDX] = 0x5F8C0000UL,                              \
		[IRONSIDE_SE_EVENT_REPORT_SPU121_IDX] = 0x5F8D0000UL,                              \
		[IRONSIDE_SE_EVENT_REPORT_SPU122_IDX] = 0x5F8E0000UL,                              \
		[IRONSIDE_SE_EVENT_REPORT_SPU130_IDX] = 0x5F900000UL,                              \
		[IRONSIDE_SE_EVENT_REPORT_SPU131_IDX] = 0x5F920000UL,                              \
		[IRONSIDE_SE_EVENT_REPORT_SPU132_IDX] = 0x5F980000UL,                              \
		[IRONSIDE_SE_EVENT_REPORT_SPU133_IDX] = 0x5F990000UL,                              \
		[IRONSIDE_SE_EVENT_REPORT_SPU134_IDX] = 0x5F9A0000UL,                              \
		[IRONSIDE_SE_EVENT_REPORT_SPU135_IDX] = 0x5F9B0000UL,                              \
		[IRONSIDE_SE_EVENT_REPORT_SPU136_IDX] = 0x5F9C0000UL,                              \
		[IRONSIDE_SE_EVENT_REPORT_SPU137_IDX] = 0x5F9D0000UL,                              \
	}

/** @brief Mapping from ironside_se_event_report_spu to instance name to assist with logging.
 */
#define IRONSIDE_SE_EVENT_REPORT_SPU_NAME_ARRAY                                                    \
	{                                                                                          \
		[IRONSIDE_SE_EVENT_REPORT_SPU110_IDX] = "110",                                     \
		[IRONSIDE_SE_EVENT_REPORT_SPU111_IDX] = "111",                                     \
		[IRONSIDE_SE_EVENT_REPORT_SPU120_IDX] = "120",                                     \
		[IRONSIDE_SE_EVENT_REPORT_SPU121_IDX] = "121",                                     \
		[IRONSIDE_SE_EVENT_REPORT_SPU122_IDX] = "122",                                     \
		[IRONSIDE_SE_EVENT_REPORT_SPU130_IDX] = "130",                                     \
		[IRONSIDE_SE_EVENT_REPORT_SPU131_IDX] = "131",                                     \
		[IRONSIDE_SE_EVENT_REPORT_SPU132_IDX] = "132",                                     \
		[IRONSIDE_SE_EVENT_REPORT_SPU133_IDX] = "133",                                     \
		[IRONSIDE_SE_EVENT_REPORT_SPU134_IDX] = "134",                                     \
		[IRONSIDE_SE_EVENT_REPORT_SPU135_IDX] = "135",                                     \
		[IRONSIDE_SE_EVENT_REPORT_SPU136_IDX] = "136",                                     \
		[IRONSIDE_SE_EVENT_REPORT_SPU137_IDX] = "137",                                     \
	}

/** @brief Mapping from ironside_se_event_report_mpc to instance address to assist with logging.
 */
#define IRONSIDE_SE_EVENT_REPORT_MPC_ADDRESS_ARRAY                                                 \
	{                                                                                          \
		[IRONSIDE_SE_EVENT_REPORT_MPC110_IDX] = 0x5F081000UL,                              \
		[IRONSIDE_SE_EVENT_REPORT_MPC111_IDX] = 0x5F091000UL,                              \
		[IRONSIDE_SE_EVENT_REPORT_MPC120_IDX] = 0x5F8C1000UL,                              \
		[IRONSIDE_SE_EVENT_REPORT_MPC130_IDX] = 0x5F901000UL,                              \
	}

/** @brief Mapping from ironside_se_event_report_mpc to instance name to assist with logging.
 */
#define IRONSIDE_SE_EVENT_REPORT_MPC_NAME_ARRAY                                                    \
	{                                                                                          \
		[IRONSIDE_SE_EVENT_REPORT_MPC110_IDX] = "110",                                     \
		[IRONSIDE_SE_EVENT_REPORT_MPC111_IDX] = "111",                                     \
		[IRONSIDE_SE_EVENT_REPORT_MPC120_IDX] = "120",                                     \
		[IRONSIDE_SE_EVENT_REPORT_MPC130_IDX] = "130",                                     \
	}

/** @brief Mapping from ironside_se_event_report_mramc to instance address to assist with logging.
 */
#define IRONSIDE_SE_EVENT_REPORT_MRAMC_ADDRESS_ARRAY                                               \
	{                                                                                          \
		[IRONSIDE_SE_EVENT_REPORT_MRAMC110_IDX] = 0x5F092000UL,                            \
		[IRONSIDE_SE_EVENT_REPORT_MRAMC111_IDX] = 0x5F093000UL,                            \
	}

/** @brief Mapping from ironside_se_event_report_mramc to instance name to assist with logging.
 */
#define IRONSIDE_SE_EVENT_REPORT_MRAMC_NAME_ARRAY                                                  \
	{                                                                                          \
		[IRONSIDE_SE_EVENT_REPORT_MRAMC110_IDX] = "110",                                   \
		[IRONSIDE_SE_EVENT_REPORT_MRAMC111_IDX] = "111",                                   \
	}

#ifdef __cplusplus
}
#endif
#endif /* IRONSIDE_SE_EVENT_REPORT_H_ */
