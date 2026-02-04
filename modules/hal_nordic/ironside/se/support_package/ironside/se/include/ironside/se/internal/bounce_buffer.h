/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef IRONSIDE_SE_INTERNAL_BOUNCE_BUFFERS_H_
#define IRONSIDE_SE_INTERNAL_BOUNCE_BUFFERS_H_

#include <stddef.h>

#include <nrfx.h>

#include <ironside/se/glue.h>

/**
 * @brief Check if a bounce buffer would need to be allocated for this out buffer.
 *
 * A buffer needs to be allocated if the data is not aligned to the cache data unit size.
 *
 * @retval true If a bounce buffer needs to be allocated.
 * @retval false If a bounce buffer does not need to be allocated.
 */
static inline bool ironside_se_bounce_buffer_is_needed(void *original_buffer, size_t size)
{
	const bool addr_is_unaligned =
		(uintptr_t)original_buffer % IRONSIDE_SE_CACHE_DATA_UNIT_SIZE != 0;
	const bool size_is_unaligned = size % IRONSIDE_SE_CACHE_DATA_UNIT_SIZE != 0;

	return size != 0 && (addr_is_unaligned || size_is_unaligned);
}

/**
 * @brief Prepare an out buffer in case the original buffer is not aligned.
 *
 * If the original buffer is not aligned, a new buffer is allocated and the data is copied to it.
 * This is needed to achieve DCache DataUnit alignment.
 *
 * @param original_buffer Original buffer
 * @param size Size of the buffer
 * @return void* NULL if the buffer could not be allocated, original_buffer if it was aligned, else
 * a new buffer from the heap
 */
void *ironside_se_bounce_buffer_prepare(void *original_buffer, size_t size);

/**
 * @brief Release an out buffer if it was allocated.
 *
 * If the out buffer was allocated, the data is copied back to the original buffer and the out
 * buffer is first zeroed and then freed.
 *
 * @param original_buffer The original buffer
 * @param out_buffer The buffer to release
 * @param size Size of the buffer
 */
void ironside_se_bounce_buffer_release(void *original_buffer, void *out_buffer, size_t size);

#endif /* IRONSIDE_SE_INTERNAL_BOUNCE_BUFFERS_H_ */
