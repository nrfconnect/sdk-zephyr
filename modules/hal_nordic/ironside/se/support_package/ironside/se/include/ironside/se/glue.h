/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef IRONSIDE_SE_GLUE_H_
#define IRONSIDE_SE_GLUE_H_

#include <ironside/se/call.h>

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

#ifdef __cplusplus
}
#endif
#endif /* NRF_IRONSIDE_SE_GLUE_H_ */
