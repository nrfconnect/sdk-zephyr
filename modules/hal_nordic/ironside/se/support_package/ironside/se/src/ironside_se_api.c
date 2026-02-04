/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>

#include <ironside/se/api.h>
#include <ironside/se/call.h>
#include <ironside/se/glue.h>
#include <ironside/se/internal/api_serialization.h>
#include <ironside/se/internal/bounce_buffer.h>

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

int ironside_se_events_enable(uint64_t event_mask)
{
	int status;
	struct ironside_se_call_buf *buf = ironside_se_call_alloc();

	buf->id = IRONSIDE_SE_CALL_ID_EVENTS_ENABLE_V1;
	buf->args[IRONSIDE_SE_EVENTS_ENABLE_REQ_IDX_EVENT_MASK_0] =
		(uint32_t)(event_mask & 0xFFFFFFFF);
	buf->args[IRONSIDE_SE_EVENTS_ENABLE_REQ_IDX_EVENT_MASK_1] = (uint32_t)(event_mask >> 32);

	ironside_se_call_dispatch(buf);

	status = buf->status;
	if (status == IRONSIDE_SE_CALL_STATUS_RSP_SUCCESS) {
		status = buf->args[IRONSIDE_SE_EVENTS_ENABLE_RSP_IDX_RETCODE];
	}

	ironside_se_call_release(buf);

	return status;
}

int ironside_se_events_disable(uint64_t event_mask)
{
	int status;
	struct ironside_se_call_buf *buf = ironside_se_call_alloc();

	buf->id = IRONSIDE_SE_CALL_ID_EVENTS_DISABLE_V1;
	buf->args[IRONSIDE_SE_EVENTS_DISABLE_REQ_IDX_EVENT_MASK_0] =
		(uint32_t)(event_mask & 0xFFFFFFFF);
	buf->args[IRONSIDE_SE_EVENTS_DISABLE_REQ_IDX_EVENT_MASK_1] = (uint32_t)(event_mask >> 32);

	ironside_se_call_dispatch(buf);

	status = buf->status;
	if (status == IRONSIDE_SE_CALL_STATUS_RSP_SUCCESS) {
		status = buf->args[IRONSIDE_SE_EVENTS_DISABLE_RSP_IDX_RETCODE];
	}

	ironside_se_call_release(buf);

	return status;
}

int ironside_se_snapshot_capture(enum ironside_se_snapshot_capture_mode mode)
{
	int status;
	struct ironside_se_call_buf *buf = ironside_se_call_alloc();

	buf->id = IRONSIDE_SE_CALL_ID_SNAPSHOT_CAPTURE_V1;
	buf->args[IRONSIDE_SE_SNAPSHOT_CAPTURE_REQ_IDX_MODE] = mode;

	ironside_se_call_dispatch(buf);

	status = buf->status;
	if (status == IRONSIDE_SE_CALL_STATUS_RSP_SUCCESS) {
		status = buf->args[IRONSIDE_SE_SNAPSHOT_CAPTURE_RSP_IDX_RETCODE];
	}

	ironside_se_call_release(buf);

	return status;
}

struct ironside_se_periphconf_status ironside_se_periphconf_read(struct periphconf_entry *entries,
								 size_t count)
{
	struct ironside_se_periphconf_status status = { 0 };

	const bool is_inline = count <= IRONSIDE_SE_PERIPHCONF_INLINE_READ_MAX_COUNT;

	if (!is_inline &&
	    ironside_se_bounce_buffer_is_needed(entries, sizeof(struct periphconf_entry) * count)) {
		status.status = -IRONSIDE_SE_PERIPHCONF_ERROR_POINTER_UNALIGNED;
		return status;
	}

	struct ironside_se_call_buf *buf = ironside_se_call_alloc();

	if (is_inline) {
		buf->id = IRONSIDE_SE_CALL_ID_PERIPHCONF_INLINE_READ_V1;
		buf->args[IRONSIDE_SE_PERIPHCONF_INLINE_READ_REQ_IDX_COUNT] = count;

		for (size_t i = 0; i < count; i++) {
			buf->args[IRONSIDE_SE_PERIPHCONF_INLINE_READ_REQ_IDX_REGPTR_0 + i] =
				entries[i].regptr;
		}
	} else {
		buf->id = IRONSIDE_SE_CALL_ID_PERIPHCONF_BUFFER_READ_V1;
		buf->args[IRONSIDE_SE_PERIPHCONF_BUFFER_READ_REQ_IDX_ADDRESS] = (uint32_t)entries;
		buf->args[IRONSIDE_SE_PERIPHCONF_BUFFER_READ_REQ_IDX_COUNT] = count;

		ironside_se_data_cache_writeback(entries, sizeof(struct periphconf_entry) * count);
	}

	ironside_se_call_dispatch(buf);

	if (!is_inline) {
		ironside_se_data_cache_writeback_invalidate(
			entries, sizeof(struct periphconf_entry) * count);
	}

	status.status = buf->status;
	if (status.status == IRONSIDE_SE_CALL_STATUS_RSP_SUCCESS) {
		const uint32_t detail =
			buf->args[IRONSIDE_SE_PERIPHCONF_INLINE_READ_RSP_IDX_DETAIL];

		status.status =
			(detail & IRONSIDE_SE_PERIPHCONF_INLINE_READ_RSP_DETAIL_STATUS_MASK) >>
			IRONSIDE_SE_PERIPHCONF_COMMON_RSP_DETAIL_STATUS_OFFSET;
		status.index =
			(detail & IRONSIDE_SE_PERIPHCONF_INLINE_READ_RSP_DETAIL_INDEX_MASK) >>
			IRONSIDE_SE_PERIPHCONF_COMMON_RSP_DETAIL_INDEX_OFFSET;

		if (is_inline) {
			for (size_t i = 0; i < count; i++) {
				const size_t j =
					IRONSIDE_SE_PERIPHCONF_INLINE_READ_RSP_IDX_VALUE_0 + i;

				entries[i].value = buf->args[j];
			}
		}
	}

	ironside_se_call_release(buf);

	return status;
}

struct ironside_se_periphconf_status
ironside_se_periphconf_write(const struct periphconf_entry *entries, size_t count)
{
	struct ironside_se_periphconf_status status = { 0 };

	const bool is_inline = count <= IRONSIDE_SE_PERIPHCONF_INLINE_WRITE_MAX_COUNT;

	struct ironside_se_call_buf *buf = ironside_se_call_alloc();

	if (is_inline) {
		buf->id = IRONSIDE_SE_CALL_ID_PERIPHCONF_INLINE_WRITE_V1;
		buf->args[IRONSIDE_SE_PERIPHCONF_INLINE_WRITE_REQ_IDX_COUNT] = count;
		for (size_t i = 0; i < count; i++) {
			buf->args[IRONSIDE_SE_PERIPHCONF_INLINE_WRITE_REQ_IDX_REGPTR_0 + 2 * i] =
				entries[i].regptr;
			buf->args[IRONSIDE_SE_PERIPHCONF_INLINE_WRITE_REQ_IDX_VALUE_0 + 2 * i] =
				entries[i].value;
		}
	} else {
		buf->id = IRONSIDE_SE_CALL_ID_PERIPHCONF_BUFFER_WRITE_V1;
		buf->args[IRONSIDE_SE_PERIPHCONF_BUFFER_WRITE_REQ_IDX_ADDRESS] = (uint32_t)entries;
		buf->args[IRONSIDE_SE_PERIPHCONF_BUFFER_WRITE_REQ_IDX_COUNT] = count;

		ironside_se_data_cache_writeback((void *)entries,
						 sizeof(struct periphconf_entry) * count);
	}

	ironside_se_call_dispatch(buf);

	status.status = buf->status;
	if (status.status == IRONSIDE_SE_CALL_STATUS_RSP_SUCCESS) {
		const uint32_t detail =
			buf->args[IRONSIDE_SE_PERIPHCONF_INLINE_WRITE_RSP_IDX_DETAIL];

		status.status =
			(detail & IRONSIDE_SE_PERIPHCONF_INLINE_WRITE_RSP_DETAIL_STATUS_MASK) >>
			IRONSIDE_SE_PERIPHCONF_INLINE_WRITE_RSP_DETAIL_STATUS_OFFSET;
		status.index =
			(detail & IRONSIDE_SE_PERIPHCONF_INLINE_WRITE_RSP_DETAIL_INDEX_MASK) >>
			IRONSIDE_SE_PERIPHCONF_INLINE_WRITE_RSP_DETAIL_INDEX_OFFSET;
	}

	ironside_se_call_release(buf);

	return status;
}

int ironside_se_periphconf_finish_init(void)
{
	int status;

	struct ironside_se_call_buf *const buf = ironside_se_call_alloc();

	buf->id = IRONSIDE_SE_CALL_ID_PERIPHCONF_FINISH_INIT_V1;

	ironside_se_call_dispatch(buf);

	status = buf->status;
	if (status == IRONSIDE_SE_CALL_STATUS_RSP_SUCCESS) {
		status = buf->args[IRONSIDE_SE_PERIPHCONF_FINISH_INIT_RSP_IDX_RETCODE];
	}

	ironside_se_call_release(buf);

	return status;
}
