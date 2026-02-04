/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include <ironside/se/internal/bounce_buffer.h>
#include <ironside/se/glue.h>

#include <nrfx.h>

static size_t size_round_up(size_t value, size_t alignment)
{
	return ((value + alignment - 1) / alignment) * alignment;
}

void *ironside_se_bounce_buffer_prepare(void *original_buffer, size_t size)
{
	if (!ironside_se_bounce_buffer_is_needed(original_buffer, size)) {
		return original_buffer;
	}

	/* The allocator code is required to allocate a memory slab that is aligned with the
	 * data unit size. To make things simpler for implementers we do the alignment here.
	 */
	const size_t aligned_size = size_round_up(size, IRONSIDE_SE_CACHE_DATA_UNIT_SIZE);
	void *out_buffer = ironside_se_bounce_buffer_heap_alloc(aligned_size);

	if (out_buffer != NULL) {
		memcpy(out_buffer, original_buffer, size);
	}

	return out_buffer;
}

void ironside_se_bounce_buffer_release(void *original_buffer, void *out_buffer, size_t size)
{
	if (out_buffer == NULL || out_buffer == original_buffer) {
		return;
	}

	memcpy(original_buffer, out_buffer, size);

	const size_t aligned_size = size_round_up(size, IRONSIDE_SE_CACHE_DATA_UNIT_SIZE);

	/* Clear buffer before returning it to not leak sensitive data */
	memset(out_buffer, 0, aligned_size);

	ironside_se_data_cache_writeback(out_buffer, size);
	ironside_se_bounce_buffer_heap_free(out_buffer);
}
