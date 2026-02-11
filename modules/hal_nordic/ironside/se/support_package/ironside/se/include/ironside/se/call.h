/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef IRONSIDE_SE_CALL_H_
#define IRONSIDE_SE_CALL_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Maximum number of arguments to an IronSide SE call.
 *
 * This is chosen so that the containing message buffer size is minimal but
 * cache line aligned.
 */
#define IRONSIDE_SE_CALL_NUM_ARGS (7)

/** @brief Message buffer. */
struct ironside_se_call_buf {
	/** Status code. This is set by the API. */
	uint16_t status;
	/** Operation identifier. This is set by the user. */
	uint16_t id;
	/** Operation arguments. These are set by the user. */
	uint32_t args[IRONSIDE_SE_CALL_NUM_ARGS];
};

/**
 * @name Message buffer status codes.
 * @{
 */

/** Buffer is idle and available for allocation. */
#define IRONSIDE_SE_CALL_STATUS_IDLE                   (0)
/** Request was processed successfully by the server. */
#define IRONSIDE_SE_CALL_STATUS_RSP_SUCCESS            (1)
/** Request status code is unknown. */
#define IRONSIDE_SE_CALL_STATUS_RSP_ERR_UNKNOWN_STATUS (2)
/** Request status code is no longer supported. */
#define IRONSIDE_SE_CALL_STATUS_RSP_ERR_EXPIRED_STATUS (3)
/** Operation identifier is unknown. */
#define IRONSIDE_SE_CALL_STATUS_RSP_ERR_UNKNOWN_ID     (4)
/** Operation identifier is no longer supported. */
#define IRONSIDE_SE_CALL_STATUS_RSP_ERR_EXPIRED_ID     (5)
/** Buffer contains a request from the client. */
#define IRONSIDE_SE_CALL_STATUS_REQ                    (6)

/**
 * @}
 */

#ifdef __cplusplus
}
#endif
#endif /* NRF_IRONSIDE_SE_CALL_H_ */
