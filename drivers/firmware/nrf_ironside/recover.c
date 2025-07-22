/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/drivers/firmware/nrf_ironside/recover.h>
#include <zephyr/drivers/firmware/nrf_ironside/call.h>

int ironside_reboot_into_recovery(void)
{
	int err;
	struct ironside_call_buf *const buf = ironside_call_alloc();

	buf->id = IRONSIDE_CALL_ID_RECOVER_SERVICE_V0;
	/* No arguments needed for recovery call */

	ironside_call_dispatch(buf);

	if (buf->status == IRONSIDE_CALL_STATUS_RSP_SUCCESS) {
		err = buf->args[IRONSIDE_RECOVER_SERVICE_RETCODE_IDX];
	} else {
		err = buf->status;
	}

	ironside_call_release(buf);

	return err;
}
