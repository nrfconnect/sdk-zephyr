/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>

#include <ironside/se/api.h>
#include <ironside/se/call.h>
#include <ironside/se/glue.h>
#include <ironside/se/internal/api_serialization.h>

int ironside_se_update(const struct ironside_se_update_blob *update)
{
	int status;
	struct ironside_se_call_buf *const buf = ironside_se_call_alloc();

	buf->id = IRONSIDE_SE_CALL_ID_UPDATE_V1;
	buf->args[IRONSIDE_SE_UPDATE_REQ_IDX_UPDATE_PTR] = (uintptr_t)update;

	ironside_se_call_dispatch(buf);

	status = buf->status;
	if (status == IRONSIDE_SE_CALL_STATUS_RSP_SUCCESS) {
		status = buf->args[IRONSIDE_SE_UPDATE_RSP_IDX_RETCODE];
	}

	ironside_se_call_release(buf);

	return status;
}

int ironside_se_cpuconf(NRF_PROCESSORID_Type cpu, const void *vector_table, bool cpu_wait,
			const uint8_t *msg, size_t msg_size)
{
	int status;
	struct ironside_se_call_buf *buf;
	uint8_t *buf_msg;

	if (msg_size > IRONSIDE_SE_CPUCONF_REQ_MSG_MAX_SIZE) {
		return -IRONSIDE_SE_CPUCONF_ERROR_MESSAGE_TOO_LARGE;
	}

	buf = ironside_se_call_alloc();

	buf->id = IRONSIDE_SE_CALL_ID_CPUCONF_V1;
	buf->args[IRONSIDE_SE_CPUCONF_REQ_IDX_CPU_PARAMS] =
		(((uint32_t)cpu << IRONSIDE_SE_CPUCONF_REQ_CPU_PARAMS_CPU_OFFSET) &
		 IRONSIDE_SE_CPUCONF_REQ_CPU_PARAMS_CPU_MASK) |
		(cpu_wait ? IRONSIDE_SE_CPUCONF_REQ_CPU_PARAMS_WAIT_BIT : 0);
	buf->args[IRONSIDE_SE_CPUCONF_REQ_IDX_VECTOR_TABLE] = (uint32_t)vector_table;

	buf_msg = (uint8_t *)&buf->args[IRONSIDE_SE_CPUCONF_REQ_IDX_MSG_0];
	if (msg_size > 0) {
		memcpy(buf_msg, msg, msg_size);
	}
	if (msg_size < IRONSIDE_SE_CPUCONF_REQ_MSG_MAX_SIZE) {
		memset(&buf_msg[msg_size], 0, IRONSIDE_SE_CPUCONF_REQ_MSG_MAX_SIZE - msg_size);
	}

	ironside_se_call_dispatch(buf);

	status = buf->status;
	if (status == IRONSIDE_SE_CALL_STATUS_RSP_SUCCESS) {
		status = buf->args[IRONSIDE_SE_CPUCONF_RSP_IDX_RETCODE];
	}

	ironside_se_call_release(buf);

	return status;
}

int ironside_se_dvfs_req_oppoint(enum ironside_se_dvfs_oppoint oppoint)
{
	int status;

	struct ironside_se_call_buf *const buf = ironside_se_call_alloc();

	buf->id = IRONSIDE_SE_CALL_ID_DVFS_REQ_OPPOINT_V1;
	buf->args[IRONSIDE_SE_DVFS_REQ_IDX_OPPOINT] = oppoint;

	ironside_se_call_dispatch(buf);

	status = buf->status;
	if (status == IRONSIDE_SE_CALL_STATUS_RSP_SUCCESS) {
		status = buf->args[IRONSIDE_SE_DVFS_RSP_IDX_RETCODE];
	}

	ironside_se_call_release(buf);

	return status;
}

int ironside_se_tdd_configure(const enum ironside_se_tdd_config config)
{
	int status;
	struct ironside_se_call_buf *const buf = ironside_se_call_alloc();

	buf->id = IRONSIDE_SE_CALL_ID_TDD_CONFIGURE_V1;
	buf->args[IRONSIDE_SE_TDD_REQ_IDX_CONFIG] = (uint32_t)config;

	ironside_se_call_dispatch(buf);

	status = buf->status;
	if (status == IRONSIDE_SE_CALL_STATUS_RSP_SUCCESS) {
		status = buf->args[IRONSIDE_SE_TDD_RSP_IDX_RETCODE];
	}

	ironside_se_call_release(buf);

	return status;
}

int ironside_se_bootmode_secondary_reboot(const uint8_t *msg, size_t msg_size)
{
	int status;
	struct ironside_se_call_buf *buf;
	uint8_t *buf_msg;

	if (msg_size > IRONSIDE_SE_BOOTMODE_REQ_MSG_MAX_SIZE) {
		return -IRONSIDE_SE_BOOTMODE_ERROR_MESSAGE_TOO_LARGE;
	}

	buf = ironside_se_call_alloc();

	buf->id = IRONSIDE_SE_CALL_ID_BOOTMODE_V1;
	buf->args[IRONSIDE_SE_BOOTMODE_REQ_IDX_MODE] = IRONSIDE_SE_BOOTMODE_REQ_MODE_SECONDARY;

	buf_msg = (uint8_t *)&buf->args[IRONSIDE_SE_BOOTMODE_REQ_IDX_MSG_0];
	memset(buf_msg, 0, IRONSIDE_SE_BOOTMODE_REQ_MSG_MAX_SIZE);
	if (msg_size > 0) {
		memcpy(buf_msg, msg, msg_size);
	}

	ironside_se_call_dispatch(buf);

	status = buf->status;
	if (status == IRONSIDE_SE_CALL_STATUS_RSP_SUCCESS) {
		status = buf->args[IRONSIDE_SE_BOOTMODE_RSP_IDX_RETCODE];
	}

	ironside_se_call_release(buf);

	return status;
}

int ironside_se_counter_set(enum ironside_se_counter counter_id, uint32_t value)
{
	int status;
	struct ironside_se_call_buf *buf = ironside_se_call_alloc();

	buf->id = IRONSIDE_SE_CALL_ID_COUNTER_SET_V1;
	buf->args[IRONSIDE_SE_COUNTER_SET_REQ_IDX_COUNTER_ID] = (uint32_t)counter_id;
	buf->args[IRONSIDE_SE_COUNTER_SET_REQ_IDX_VALUE] = value;

	ironside_se_call_dispatch(buf);

	status = buf->status;
	if (status == IRONSIDE_SE_CALL_STATUS_RSP_SUCCESS) {
		status = buf->args[IRONSIDE_SE_COUNTER_SET_RSP_IDX_RETCODE];
	}

	ironside_se_call_release(buf);

	return status;
}

int ironside_se_counter_get(enum ironside_se_counter counter_id, uint32_t *value)
{
	int status;
	struct ironside_se_call_buf *buf;

	if (value == NULL) {
		return -IRONSIDE_SE_COUNTER_ERROR_INVALID_PARAM;
	}

	buf = ironside_se_call_alloc();

	buf->id = IRONSIDE_SE_CALL_ID_COUNTER_GET_V1;
	buf->args[IRONSIDE_SE_COUNTER_GET_REQ_IDX_COUNTER_ID] = (uint32_t)counter_id;

	ironside_se_call_dispatch(buf);

	status = buf->status;
	if (status == IRONSIDE_SE_CALL_STATUS_RSP_SUCCESS) {
		status = buf->args[IRONSIDE_SE_COUNTER_GET_RSP_IDX_RETCODE];
		if (status == 0) {
			*value = buf->args[IRONSIDE_SE_COUNTER_GET_RSP_IDX_VALUE];
		}
	}

	ironside_se_call_release(buf);

	return status;
}

int ironside_se_counter_lock(enum ironside_se_counter counter_id)
{
	int status;
	struct ironside_se_call_buf *buf = ironside_se_call_alloc();

	buf->id = IRONSIDE_SE_CALL_ID_COUNTER_LOCK_V1;
	buf->args[IRONSIDE_SE_COUNTER_LOCK_REQ_IDX_COUNTER_ID] = (uint32_t)counter_id;

	ironside_se_call_dispatch(buf);

	status = buf->status;
	if (status == IRONSIDE_SE_CALL_STATUS_RSP_SUCCESS) {
		status = buf->args[IRONSIDE_SE_COUNTER_LOCK_RSP_IDX_RETCODE];
	}

	ironside_se_call_release(buf);

	return status;
}
