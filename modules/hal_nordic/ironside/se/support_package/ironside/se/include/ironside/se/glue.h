/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef IRONSIDE_SE_GLUE_H_
#define IRONSIDE_SE_GLUE_H_

#include <stddef.h>

#include <ironside/se/call.h>

#include <nrfx.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Allocate memory for an IronSide SE call.
 *
 * This function must block when no buffers are available, until one is
 * released by another thread on the client side.
 *
 * @return Pointer to the allocated buffer.
 */
struct ironside_se_call_buf *ironside_se_call_alloc(void);

/**
 * @brief Dispatch an IronSide SE call.
 *
 * This function must block until a response is received from the server.
 *
 * @param buf Buffer returned by ironside_se_call_alloc(). It should be populated
 *            with request data before calling this function. Upon returning,
 *            this data must be replaced by response data.
 */
void ironside_se_call_dispatch(struct ironside_se_call_buf *buf);

/**
 * @brief Release an IronSide SE call buffer.
 *
 * This function must be called after processing the response.
 *
 * @param buf Buffer used to perform the call.
 */
void ironside_se_call_release(struct ironside_se_call_buf *buf);

/** Data cache data unit size used for alignment requirements. */
#define IRONSIDE_SE_CACHE_DATA_UNIT_SIZE (DCACHEDATA_DATAWIDTH * 4)

/**
 * @brief Allocate memory area for bounce buffer.
 *
 * Bounce buffers are used to ensure that memory shared with IronSide SE behaves
 * correctly with respect to data caching.
 *
 * A bounce buffer is allocated whenever an API that transfers data between
 * the caller and IronSide SE is called with a pointer/buffer that isn't aligned
 * to the cache data unit size. It is therefore often possible to avoid allocation
 * and copy operations by ensuring that buffer parameters are aligned.
 *
 * @note The returned pointer must be aligned with the dcache data unit size.
 * @note This API is always called with a size aligned with the dcache data unit size.
 *
 * @param size Number of bytes to allocate.
 * @return Pointer to allocated memory area, or NULL if unable to allocate memory.
 */
void *ironside_se_bounce_buffer_heap_alloc(size_t size);

/**
 * @brief Free memory area previously allocated for bounce buffer.
 *
 * Bounce buffers are used to ensure that memory shared with IronSide SE behaves
 * correctly with respect to data caching.
 *
 * @param ptr Pointer to previously allocated memory area, or NULL.
 */
void ironside_se_bounce_buffer_heap_free(void *ptr);

/**
 * @brief Write back data cache lines for memory range [addr, addr + size).
 *
 * @param[in] addr Start address.
 * @param[in] size Number of bytes.
 */
void ironside_se_data_cache_writeback(void *addr, size_t size);

/**
 * @brief Invalidate data cache lines for memory range [addr, addr + size).
 *
 * @param[in] addr Start address.
 * @param[in] size Number of bytes.
 */
void ironside_se_data_cache_invalidate(void *addr, size_t size);

/**
 * @brief Write back and invalidate data cache lines for memory range [addr, addr + size).
 *
 * @param[in] addr Start address.
 * @param[in] size Number of bytes.
 */
void ironside_se_data_cache_writeback_invalidate(void *addr, size_t size);

#ifdef __cplusplus
}
#endif
#endif /* NRF_IRONSIDE_SE_GLUE_H_ */
