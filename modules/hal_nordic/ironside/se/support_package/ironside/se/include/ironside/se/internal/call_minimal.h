/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef IRONSIDE_SE_INTERNAL_CALL_MINIMAL_H_
#define IRONSIDE_SE_INTERNAL_CALL_MINIMAL_H_

#include <cmsis_compiler.h>

#include <ironside/se/call.h>
#include <ironside/se/glue.h>
#include <ironside/se/memory_map.h>
#include <ironside/se/peripheral_interface.h>

#ifdef __cplusplus
extern "C" {
#endif

static IRONSIDE_SE_ALWAYS_INLINE struct ironside_se_call_buf *ironside_se_call_alloc(void)
{
	/* In this minimal implementation we only use the first buffer.
	 * We assume that there is no concurrent usage of the API,
	 * so the buffer is always available when this is called.
	 */
	return &IRONSIDE_SE_IPC_BUFFER[0];
}

static IRONSIDE_SE_ALWAYS_INLINE void ironside_se_call_dispatch(struct ironside_se_call_buf *buf)
{
	buf->status = IRONSIDE_SE_CALL_STATUS_REQ;
	__DMB();

	/* Cache handling omitted here as we assume that the IPC buffer is not cached. */

	NRF_SECDOMBELLBOARD->TASKS_TRIGGER[IRONSIDE_SE_BELLBOARD_IPC_TX_BELL_IDX] = 1;

	/* Busy wait for response. */
	while (!NRF_BELLBOARD->EVENTS_TRIGGERED[IRONSIDE_SE_BELLBOARD_IPC_RX_BELL_IDX]) {
	}

	NRF_BELLBOARD->EVENTS_TRIGGERED[IRONSIDE_SE_BELLBOARD_IPC_RX_BELL_IDX] = 0;
}

static IRONSIDE_SE_ALWAYS_INLINE void ironside_se_call_release(struct ironside_se_call_buf *buf)
{
	buf->status = IRONSIDE_SE_CALL_STATUS_IDLE;
	__DMB();

	/* Cache handling omitted here as we assume that the IPC buffer is not cached. */
}

#ifdef __cplusplus
}
#endif
#endif /* IRONSIDE_SE_INTERNAL_CALL_MINIMAL_H_ */
