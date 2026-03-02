/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef IRONSIDE_SE_MPCCONF_H_
#define IRONSIDE_SE_MPCCONF_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Entry in the MPCCONF table.
 *  @note Bitfields are described by the MPCCONF_ENTRY_CONFIG* macros below.
 */
struct mpcconf_entry {
	/** Packed value containing: LOCK, ENABLE, REGPTR */
	uint32_t config0;
	/** Packed value containing: READ, WRITE, EXECUTE, SECATTR, STARTADDR */
	uint32_t config1;
	/** Packed value containing: ENDADDR_OR_MASK, OWNERID */
	uint32_t config2;
	/** Packed value containing: MASTERPORT */
	uint32_t config3;
};

/** @brief Encode @ref mpcconf_entry config0 (LOCK, ENABLE, REGPTR).
 *
 * @param _lock If non-zero, set LOCK.
 * @param _enable If non-zero, set ENABLE.
 * @param _regptr Register pointer (upper bits 4..31).
 */
#define MPCCONF_ENTRY_CONFIG0_VALUE(_lock, _enable, _regptr)                                       \
	(uint32_t)((((_lock) ? MPCCONF_ENTRY_CONFIG0_LOCK_Msk : 0UL)) |                            \
		   (((_enable) ? MPCCONF_ENTRY_CONFIG0_ENABLE_Msk : 0UL)) |                        \
		   ((_regptr) & MPCCONF_ENTRY_CONFIG0_REGPTR_Msk))

/** @brief Encode @ref mpcconf_entry config1 (READ, WRITE, EXECUTE, SECATTR, STARTADDR).
 *
 * @param _read If non-zero, set READ.
 * @param _write If non-zero, set WRITE.
 * @param _execute If non-zero, set EXECUTE.
 * @param _secattr If non-zero, set SECATTR.
 * @param _startaddr STARTADDR field (upper bits 5..31).
 */
#define MPCCONF_ENTRY_CONFIG1_VALUE(_read, _write, _execute, _secattr, _startaddr)                 \
	(uint32_t)(((_read) ? MPCCONF_ENTRY_CONFIG1_READ_Msk : 0UL) |                              \
		   ((_write) ? MPCCONF_ENTRY_CONFIG1_WRITE_Msk : 0UL) |                            \
		   ((_execute) ? MPCCONF_ENTRY_CONFIG1_EXECUTE_Msk : 0UL) |                        \
		   ((_secattr) ? MPCCONF_ENTRY_CONFIG1_SECATTR_Msk : 0UL) |                        \
		   ((_startaddr) & MPCCONF_ENTRY_CONFIG1_STARTADDR_Msk))

/** @brief Encode @ref mpcconf_entry config2 (OWNERID, ENDADDR_OR_MASK).
 *
 * @param _ownerid OWNERID field (lower bits 0..3).
 * @param _endaddr_or_mask ENDADDR_OR_MASK field (upper bits 5..31).
 */
#define MPCCONF_ENTRY_CONFIG2_VALUE(_ownerid, _endaddr_or_mask)                                    \
	(uint32_t)((((_ownerid) << MPCCONF_ENTRY_CONFIG2_OWNERID_Pos) &                            \
		    MPCCONF_ENTRY_CONFIG2_OWNERID_Msk) |                                           \
		   ((_endaddr_or_mask) & MPCCONF_ENTRY_CONFIG2_ENDADDR_OR_MASK_Msk))

/** @brief Encode @ref mpcconf_entry config3 (MASTERPORT).
 *
 * @param _masterport MASTERPORT field value.
 */
#define MPCCONF_ENTRY_CONFIG3_VALUE(_masterport)                                                   \
	(uint32_t)(((_masterport) << MPCCONF_ENTRY_CONFIG3_MASTERPORT_Pos) &                       \
		   MPCCONF_ENTRY_CONFIG3_MASTERPORT_Msk)

/** @ref mpcconf_entry config0 @Bit 0 contains LOCK */
#define MPCCONF_ENTRY_CONFIG0_LOCK_Pos (0)
#define MPCCONF_ENTRY_CONFIG0_LOCK_Msk (0x1UL << MPCCONF_ENTRY_CONFIG0_LOCK_Pos)

/** @ref mpcconf_entry config0 @Bit 1 contains ENABLE */
#define MPCCONF_ENTRY_CONFIG0_ENABLE_Pos (1)
#define MPCCONF_ENTRY_CONFIG0_ENABLE_Msk (0x1UL << MPCCONF_ENTRY_CONFIG0_ENABLE_Pos)

/** @ref mpcconf_entry config0 @Bits 4..31 contain REGPTR */
#define MPCCONF_ENTRY_CONFIG0_REGPTR_Pos (4)
#define MPCCONF_ENTRY_CONFIG0_REGPTR_Msk (0xFFFFFFFUL << MPCCONF_ENTRY_CONFIG0_REGPTR_Pos)

/** @ref mpcconf_entry config1 @Bit 0 contains READ */
#define MPCCONF_ENTRY_CONFIG1_READ_Pos (0)
#define MPCCONF_ENTRY_CONFIG1_READ_Msk (0x1UL << MPCCONF_ENTRY_CONFIG1_READ_Pos)

/** @ref mpcconf_entry config1 @Bit 1 contains WRITE */
#define MPCCONF_ENTRY_CONFIG1_WRITE_Pos (1)
#define MPCCONF_ENTRY_CONFIG1_WRITE_Msk (0x1UL << MPCCONF_ENTRY_CONFIG1_WRITE_Pos)

/** @ref mpcconf_entry config1 @Bit 2 contains EXECUTE */
#define MPCCONF_ENTRY_CONFIG1_EXECUTE_Pos (2)
#define MPCCONF_ENTRY_CONFIG1_EXECUTE_Msk (0x1UL << MPCCONF_ENTRY_CONFIG1_EXECUTE_Pos)

/** @ref mpcconf_entry config1 @Bit 3 contains SECATTR */
#define MPCCONF_ENTRY_CONFIG1_SECATTR_Pos (3)
#define MPCCONF_ENTRY_CONFIG1_SECATTR_Msk (0x1UL << MPCCONF_ENTRY_CONFIG1_SECATTR_Pos)

/** @ref mpcconf_entry config1 @Bits 5..31 contain STARTADDR */
#define MPCCONF_ENTRY_CONFIG1_STARTADDR_Pos (5)
#define MPCCONF_ENTRY_CONFIG1_STARTADDR_Msk (0x7FFFFFFUL << MPCCONF_ENTRY_CONFIG1_STARTADDR_Pos)

/** @ref mpcconf_entry config2 @Bits 0..3 contain OWNERID */
#define MPCCONF_ENTRY_CONFIG2_OWNERID_Pos (0)
#define MPCCONF_ENTRY_CONFIG2_OWNERID_Msk (0xFUL << MPCCONF_ENTRY_CONFIG2_OWNERID_Pos)

/** @ref mpcconf_entry config2 @Bits 5..31 contain ENDADDR_OR_MASK */
#define MPCCONF_ENTRY_CONFIG2_ENDADDR_OR_MASK_Pos (5)
#define MPCCONF_ENTRY_CONFIG2_ENDADDR_OR_MASK_Msk                                                  \
	(0x7FFFFFFUL << MPCCONF_ENTRY_CONFIG2_ENDADDR_OR_MASK_Pos)

/** @ref mpcconf_entry config3 @Bits 0..31 contain MASTERPORT */
#define MPCCONF_ENTRY_CONFIG3_MASTERPORT_Pos (0)
#define MPCCONF_ENTRY_CONFIG3_MASTERPORT_Msk (0xFFFFFFFFUL << MPCCONF_ENTRY_CONFIG3_MASTERPORT_Pos)

#ifdef __cplusplus
}
#endif
#endif /* IRONSIDE_SE_MPCCONF_H_ */
