/*
 * Copyright (c) 2025 Nordic Semiconductor ASA.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <ironside/se/uicr_deploy.h>

#include <stdbool.h>
#include <string.h>
#include <stdint.h>
#include <nrfx.h>
#include <ironside/se/uicr.h>
#include <ironside/se/internal/mdk.h>

#define MRAM_WORD_SIZE_IN_BYTES     (MRAMC110_NMRAMWORDSIZE / 8)
#define WORDS_IN_MRAM_WORD          (MRAM_WORD_SIZE_IN_BYTES / sizeof(uint32_t))

/* Lock state change is pending reset, allowing further changes now. */
static bool lock_pending;

/**
 * @brief Check if the UICR is locked from further changes.
 *
 * @retval false if unlocked, or the lock was enabled during this runtime.
 * @retval true if lock was enabled on boot.
 */
static inline bool is_locked(void)
{
	return (IRONSIDE_SE_UICR->LOCK != UICR_LOCK_PALL_UNLOCKED) && !lock_pending;
}

/**
 * @brief Commit changes to UICR (NVR0) using full MRAM word writes.
 *
 * @note Direct-write changes to MRAM words are not committed until certain
 *       circumstances are met, such as writing the last byte of the MRAM word.
 *       Therefore, the entire MRAM word is ascertained based on the target
 *       address and operated on as a whole unit.
 *
 * @param[in] address    Target NVR address.
 * @param[in] value      Value to write to NVR.
 */
static void read_modify_write(uintptr_t address, uint32_t value)
{
	uint32_t mram_word[WORDS_IN_MRAM_WORD];
	uintptr_t boundary_offset = address % MRAM_WORD_SIZE_IN_BYTES;
	uintptr_t *start_addr = (uintptr_t *)(address - boundary_offset);

	/* Read full MRAM word */
	memcpy(mram_word, start_addr, MRAM_WORD_SIZE_IN_BYTES);

	/* Update value of target word within MRAM word */
	mram_word[boundary_offset / WORDS_IN_MRAM_WORD] = value;

	/* Commit the changes by writing back the full word */
	memcpy(start_addr, mram_word, MRAM_WORD_SIZE_IN_BYTES);
}

int uicr_deploy_lock_contents(void)
{
	bool locked = is_locked();

	if (locked) {
		return -1;
	}

	lock_pending = true;
	read_modify_write((uintptr_t)&IRONSIDE_SE_UICR->LOCK, UICR_LOCK_PALL_LOCKED);

	return 0;
}

int uicr_deploy_block_eraseall(void)
{
	bool locked = is_locked();

	if (locked) {
		return -1;
	}

	read_modify_write((uintptr_t)&IRONSIDE_SE_UICR->ERASEPROTECT,
			  UICR_ERASEPROTECT_PALL_PROTECTED);

	return 0;
}
