/*
 * Copyright (c) 2018-2021 mcumgr authors
 * Copyright (c) 2021-2022 Nordic Semiconductor ASA
 * Copyright (c) 2022 Laird Connectivity
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/sys/util.h>
#include <zephyr/kernel.h>
#include <zephyr/debug/object_tracing.h>
#include <zephyr/kernel_structs.h>
#include <zephyr/mgmt/mcumgr/mgmt/mgmt.h>
#include <zephyr/mgmt/mcumgr/smp/smp.h>
#include <zephyr/mgmt/mcumgr/grp/os_mgmt/os_mgmt.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>

#include <zcbor_common.h>
#include <zcbor_encode.h>
#include <zcbor_decode.h>
#ifdef CONFIG_REBOOT
#include <zephyr/sys/reboot.h>
#endif

#ifdef CONFIG_MCUMGR_MGMT_NOTIFICATION_HOOKS
#include <zephyr/mgmt/mcumgr/mgmt/callbacks.h>
#endif

#ifdef CONFIG_REBOOT
static void os_mgmt_reset_work_handler(struct k_work *work);
static void os_mgmt_reset_cb(struct k_timer *timer);

K_WORK_DEFINE(os_mgmt_reset_work, os_mgmt_reset_work_handler);
static K_TIMER_DEFINE(os_mgmt_reset_timer, os_mgmt_reset_cb, NULL);
#endif

/* This is passed to zcbor_map_start/end_endcode as a number of
 * expected "columns" (tid, priority, and so on)
 * The value here does not affect memory allocation is is used
 * to predict how big the map may be. If you increase number
 * of "columns" the taskstat sends you may need to increase the
 * value otherwise zcbor_map_end_encode may return with error.
 */
#define TASKSTAT_COLUMNS_MAX	20

#ifdef CONFIG_OS_MGMT_TASKSTAT
/* Thread iterator information passing structure */
struct thread_iterator_info {
	zcbor_state_t *zse;
	int thread_idx;
	bool ok;
};
#endif

/**
 * Command handler: os echo
 */
#ifdef CONFIG_OS_MGMT_ECHO
static int os_mgmt_echo(struct smp_streamer *ctxt)
{
	struct zcbor_string value = { 0 };
	struct zcbor_string key;
	bool ok;
	zcbor_state_t *zsd = ctxt->reader->zs;
	zcbor_state_t *zse = ctxt->writer->zs;

	if (!zcbor_map_start_decode(zsd)) {
		return MGMT_ERR_EUNKNOWN;
	}

	do {
		ok = zcbor_tstr_decode(zsd, &key);

		if (ok) {
			if (key.len == 1 && *key.value == 'd') {
				ok = zcbor_tstr_decode(zsd, &value);
				break;
			}

			ok = zcbor_any_skip(zsd, NULL);
		}
	} while (ok);

	if (!ok || !zcbor_map_end_decode(zsd)) {
		return MGMT_ERR_EUNKNOWN;
	}

	ok = zcbor_tstr_put_lit(zse, "r")		&&
	     zcbor_tstr_encode(zse, &value);

	return ok ? MGMT_ERR_EOK : MGMT_ERR_EMSGSIZE;
}
#endif

#ifdef CONFIG_OS_MGMT_TASKSTAT

#ifdef CONFIG_OS_MGMT_TASKSTAT_USE_THREAD_NAME_FOR_NAME
static inline bool
os_mgmt_taskstat_encode_thread_name(zcbor_state_t *zse, int idx,
				    const struct k_thread *thread)
{
	size_t name_len = strlen(thread->name);

	ARG_UNUSED(idx);

	if (name_len > CONFIG_OS_MGMT_TASKSTAT_THREAD_NAME_LEN) {
		name_len = CONFIG_OS_MGMT_TASKSTAT_THREAD_NAME_LEN;
	}

	return zcbor_tstr_encode_ptr(zse, thread->name, name_len);
}

#else
static inline bool
os_mgmt_taskstat_encode_thread_name(zcbor_state_t *zse, int idx,
				    const struct k_thread *thread)
{
	char thread_name[CONFIG_OS_MGMT_TASKSTAT_THREAD_NAME_LEN + 1];

#if defined(CONFIG_OS_MGMT_TASKSTAT_USE_THREAD_PRIO_FOR_NAME)
	idx = (int)thread->base.prio;
#elif defined(CONFIG_OS_MGMT_TASKSTAT_USE_THREAD_IDX_FOR_NAME)
	ARG_UNUSED(thread);
#else
#error Unsupported option for taskstat thread name
#endif

	snprintf(thread_name, sizeof(thread_name) - 1, "%d", idx);
	thread_name[sizeof(thread_name) - 1] = 0;

	return zcbor_tstr_put_term(zse, thread_name);
}

#endif

static inline bool
os_mgmt_taskstat_encode_stack_info(zcbor_state_t *zse,
				   const struct k_thread *thread)
{
#ifdef CONFIG_OS_MGMT_TASKSTAT_STACK_INFO
	size_t stack_size = 0;
	size_t stack_used = 0;
	bool ok = true;

#ifdef CONFIG_THREAD_STACK_INFO
	stack_size = thread->stack_info.size / 4;

#ifdef CONFIG_INIT_STACKS
	unsigned int stack_unused;

	if (k_thread_stack_space_get(thread, &stack_unused) == 0) {
		stack_used = (thread->stack_info.size - stack_unused) / 4;
	}
#endif /* CONFIG_INIT_STACKS */
#endif /* CONFIG_THREAD_STACK_INFO */
	ok = zcbor_tstr_put_lit(zse, "stksiz")		&&
	     zcbor_uint64_put(zse, stack_size)		&&
	     zcbor_tstr_put_lit(zse, "stkuse")		&&
	     zcbor_uint64_put(zse, stack_used);

	return ok;
#else
	return true;
#endif /* CONFIG_OS_MGMT_TASKSTAT_STACK_INFO */
}

static inline bool
os_mgmt_taskstat_encode_runtime_info(zcbor_state_t *zse,
				     const struct k_thread *thread)
{
	bool ok = true;

#if defined(CONFIG_SCHED_THREAD_USAGE)
	k_thread_runtime_stats_t thread_stats;

	k_thread_runtime_stats_get((struct k_thread *)thread, &thread_stats);

	ok = zcbor_tstr_put_lit(zse, "runtime") &&
	zcbor_uint64_put(zse, thread_stats.execution_cycles);
#elif !defined(CONFIG_OS_MGMT_TASKSTAT_ONLY_SUPPORTED_STATS)
	ok = zcbor_tstr_put_lit(zse, "runtime") &&
	zcbor_uint32_put(zse, 0);
#endif

	return ok;
}

static inline bool os_mgmt_taskstat_encode_unsupported(zcbor_state_t *zse)
{
	bool ok = true;

	if (!IS_ENABLED(CONFIG_OS_MGMT_TASKSTAT_ONLY_SUPPORTED_STATS)) {
		ok = zcbor_tstr_put_lit(zse, "cswcnt")		&&
		     zcbor_uint32_put(zse, 0)			&&
		     zcbor_tstr_put_lit(zse, "last_checkin")	&&
		     zcbor_uint32_put(zse, 0)			&&
		     zcbor_tstr_put_lit(zse, "next_checkin")	&&
		     zcbor_uint32_put(zse, 0);
	} else {
		ARG_UNUSED(zse);
	}

	return ok;
}

static inline bool
os_mgmt_taskstat_encode_priority(zcbor_state_t *zse, const struct k_thread *thread)
{
	return (zcbor_tstr_put_lit(zse, "prio")					&&
		IS_ENABLED(CONFIG_OS_MGMT_TASKSTAT_SIGNED_PRIORITY) ?
		zcbor_int32_put(zse, (int)thread->base.prio) :
		zcbor_uint32_put(zse, (unsigned int)thread->base.prio) & 0xff);
}

/**
 * Encodes a single taskstat entry.
 */
static void os_mgmt_taskstat_encode_one(const struct k_thread *thread, void *user_data)
{
	/*
	 * Threads are sent as map where thread name is key and value is map
	 * of thread parameters
	 */
	struct thread_iterator_info *iterator_ctx = (struct thread_iterator_info *)user_data;

	if (iterator_ctx->ok == true) {
		iterator_ctx->ok =
			os_mgmt_taskstat_encode_thread_name(iterator_ctx->zse,
							    iterator_ctx->thread_idx, thread)	&&
			zcbor_map_start_encode(iterator_ctx->zse, TASKSTAT_COLUMNS_MAX)		&&
			os_mgmt_taskstat_encode_priority(iterator_ctx->zse, thread)		&&
			zcbor_tstr_put_lit(iterator_ctx->zse, "tid")				&&
			zcbor_uint32_put(iterator_ctx->zse, iterator_ctx->thread_idx)		&&
			zcbor_tstr_put_lit(iterator_ctx->zse, "state")				&&
			zcbor_uint32_put(iterator_ctx->zse, thread->base.thread_state)		&&
			os_mgmt_taskstat_encode_stack_info(iterator_ctx->zse, thread)		&&
			os_mgmt_taskstat_encode_runtime_info(iterator_ctx->zse, thread)		&&
			os_mgmt_taskstat_encode_unsupported(iterator_ctx->zse)			&&
			zcbor_map_end_encode(iterator_ctx->zse, TASKSTAT_COLUMNS_MAX);

		++iterator_ctx->thread_idx;
	}
}

/**
 * Command handler: os taskstat
 */
static int os_mgmt_taskstat_read(struct smp_streamer *ctxt)
{
	zcbor_state_t *zse = ctxt->writer->zs;
	struct thread_iterator_info iterator_ctx = {
		.zse = zse,
		.thread_idx = 0,
		.ok = true,
	};

	zcbor_tstr_put_lit(zse, "tasks");
	zcbor_map_start_encode(zse, CONFIG_OS_MGMT_TASKSTAT_MAX_NUM_THREADS);

	/* Iterate the list of tasks, encoding each. */
	k_thread_foreach(os_mgmt_taskstat_encode_one, (void *)&iterator_ctx);

	if (!iterator_ctx.ok ||
	    !zcbor_map_end_encode(zse, CONFIG_OS_MGMT_TASKSTAT_MAX_NUM_THREADS)) {
		return MGMT_ERR_EMSGSIZE;
	}

	return 0;
}
#endif /* CONFIG_OS_MGMT_TASKSTAT */

#ifdef CONFIG_REBOOT
/**
 * Command handler: os reset
 */
static void os_mgmt_reset_work_handler(struct k_work *work)
{
	sys_reboot(SYS_REBOOT_WARM);
}

static void os_mgmt_reset_cb(struct k_timer *timer)
{
	/* Reboot the system from the system workqueue thread. */
	k_work_submit(&os_mgmt_reset_work);
}

static int os_mgmt_reset(struct smp_streamer *ctxt)
{
#if defined(CONFIG_MCUMGR_GRP_OS_OS_RESET_HOOK)
	int rc = mgmt_callback_notify(MGMT_EVT_OP_OS_MGMT_RESET, NULL, 0);

	if (rc != MGMT_ERR_EOK) {
		return rc;
	}
#endif

	k_timer_start(&os_mgmt_reset_timer, K_MSEC(CONFIG_OS_MGMT_RESET_MS),
		      K_NO_WAIT);
	return 0;
}
#endif

#ifdef CONFIG_OS_MGMT_MCUMGR_PARAMS
static int
os_mgmt_mcumgr_params(struct smp_streamer *ctxt)
{
	zcbor_state_t *zse = ctxt->writer->zs;
	bool ok;

	ok = zcbor_tstr_put_lit(zse, "buf_size")		&&
	     zcbor_uint32_put(zse, CONFIG_MCUMGR_BUF_SIZE)	&&
	     zcbor_tstr_put_lit(zse, "buf_count")		&&
	     zcbor_uint32_put(zse, CONFIG_MCUMGR_BUF_COUNT);

	return ok ? MGMT_ERR_EOK : MGMT_ERR_EMSGSIZE;
}
#endif

static const struct mgmt_handler os_mgmt_group_handlers[] = {
#ifdef CONFIG_OS_MGMT_ECHO
	[OS_MGMT_ID_ECHO] = {
		os_mgmt_echo, os_mgmt_echo
	},
#endif
#ifdef CONFIG_OS_MGMT_TASKSTAT
	[OS_MGMT_ID_TASKSTAT] = {
		os_mgmt_taskstat_read, NULL
	},
#endif
#ifdef CONFIG_REBOOT
	[OS_MGMT_ID_RESET] = {
		NULL, os_mgmt_reset
	},
#endif
#ifdef CONFIG_OS_MGMT_MCUMGR_PARAMS
	[OS_MGMT_ID_MCUMGR_PARAMS] = {
		os_mgmt_mcumgr_params, NULL
	},
#endif
};

#define OS_MGMT_GROUP_SZ ARRAY_SIZE(os_mgmt_group_handlers)

static struct mgmt_group os_mgmt_group = {
	.mg_handlers = os_mgmt_group_handlers,
	.mg_handlers_count = OS_MGMT_GROUP_SZ,
	.mg_group_id = MGMT_GROUP_ID_OS,
};

void os_mgmt_register_group(void)
{
	mgmt_register_group(&os_mgmt_group);
}

void os_mgmt_module_init(void)
{
	os_mgmt_register_group();
}
