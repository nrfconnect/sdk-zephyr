/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <zephyr/logging/log.h>
#include <zephyr/logging/log_backend.h>
#include <zephyr/logging/log_ctrl.h>
#include <zephyr/logging/log_output.h>
#include <zephyr/logging/log_internal.h>
#include <zephyr/sys/mpsc_pbuf.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys_clock.h>
#include <zephyr/init.h>
#include <zephyr/sys/__assert.h>
#include <zephyr/sys/atomic.h>
#include <ctype.h>
#include <zephyr/logging/log_frontend.h>
#include <zephyr/syscall_handler.h>
#include <zephyr/logging/log_output_dict.h>
#include <zephyr/linker/utils.h>

LOG_MODULE_REGISTER(log);

#ifndef CONFIG_LOG_PROCESS_THREAD_SLEEP_MS
#define CONFIG_LOG_PROCESS_THREAD_SLEEP_MS 0
#endif

#ifndef CONFIG_LOG_PROCESS_TRIGGER_THRESHOLD
#define CONFIG_LOG_PROCESS_TRIGGER_THRESHOLD 0
#endif

#ifndef CONFIG_LOG_PROCESS_THREAD_STACK_SIZE
#define CONFIG_LOG_PROCESS_THREAD_STACK_SIZE 1
#endif

#ifndef CONFIG_LOG_BLOCK_IN_THREAD_TIMEOUT_MS
#define CONFIG_LOG_BLOCK_IN_THREAD_TIMEOUT_MS 0
#endif

#ifndef CONFIG_LOG_BUFFER_SIZE
#define CONFIG_LOG_BUFFER_SIZE 4
#endif

#ifdef CONFIG_LOG_PROCESS_THREAD_CUSTOM_PRIORITY
#define LOG_PROCESS_THREAD_PRIORITY CONFIG_LOG_PROCESS_THREAD_PRIORITY
#else
#define LOG_PROCESS_THREAD_PRIORITY K_LOWEST_APPLICATION_THREAD_PRIO
#endif

#ifndef CONFIG_LOG_TAG_MAX_LEN
#define CONFIG_LOG_TAG_MAX_LEN 0
#endif

#ifndef CONFIG_LOG_ALWAYS_RUNTIME
BUILD_ASSERT(!IS_ENABLED(CONFIG_NO_OPTIMIZATIONS),
	     "Option must be enabled when CONFIG_NO_OPTIMIZATIONS is set");
BUILD_ASSERT(!IS_ENABLED(CONFIG_LOG_MODE_IMMEDIATE),
	     "Option must be enabled when CONFIG_LOG_MODE_IMMEDIATE is set");
#endif

static const log_format_func_t format_table[] = {
	[LOG_OUTPUT_TEXT] = IS_ENABLED(CONFIG_LOG_OUTPUT) ?
						log_output_msg_process : NULL,
	[LOG_OUTPUT_SYST] = IS_ENABLED(CONFIG_LOG_MIPI_SYST_ENABLE) ?
						log_output_msg_syst_process : NULL,
	[LOG_OUTPUT_DICT] = IS_ENABLED(CONFIG_LOG_DICTIONARY_SUPPORT) ?
						log_dict_output_msg_process : NULL
};

log_format_func_t log_format_func_t_get(uint32_t log_type)
{
	return format_table[log_type];
}

size_t log_format_table_size(void)
{
	return ARRAY_SIZE(format_table);
}

K_SEM_DEFINE(log_process_thread_sem, 0, 1);

static atomic_t initialized;
static bool panic_mode;
static bool backend_attached;
static atomic_t buffered_cnt;
static atomic_t dropped_cnt;
static k_tid_t proc_tid;
static struct k_timer log_process_thread_timer;

static log_timestamp_t dummy_timestamp(void);
static log_timestamp_get_t timestamp_func = dummy_timestamp;

struct mpsc_pbuf_buffer log_buffer;
static uint32_t __aligned(Z_LOG_MSG2_ALIGNMENT)
	buf32[CONFIG_LOG_BUFFER_SIZE / sizeof(int)];

static void notify_drop(const struct mpsc_pbuf_buffer *buffer,
			const union mpsc_pbuf_generic *item);

static const struct mpsc_pbuf_buffer_config mpsc_config = {
	.buf = (uint32_t *)buf32,
	.size = ARRAY_SIZE(buf32),
	.notify_drop = notify_drop,
	.get_wlen = log_msg_generic_get_wlen,
	.flags = (IS_ENABLED(CONFIG_LOG_MODE_OVERFLOW) ?
		  MPSC_PBUF_MODE_OVERWRITE : 0) |
		 (IS_ENABLED(CONFIG_LOG_MEM_UTILIZATION) ?
		  MPSC_PBUF_MAX_UTILIZATION : 0)
};

/* Check that default tag can fit in tag buffer. */
COND_CODE_0(CONFIG_LOG_TAG_MAX_LEN, (),
	(BUILD_ASSERT(sizeof(CONFIG_LOG_TAG_DEFAULT) <= CONFIG_LOG_TAG_MAX_LEN + 1,
		      "Default string longer than tag capacity")));

static char tag[CONFIG_LOG_TAG_MAX_LEN + 1] =
	COND_CODE_0(CONFIG_LOG_TAG_MAX_LEN, ({}), (CONFIG_LOG_TAG_DEFAULT));

static void msg_process(union log_msg_generic *msg);

static log_timestamp_t dummy_timestamp(void)
{
	return 0;
}

log_timestamp_t z_log_timestamp(void)
{
	return timestamp_func();
}

static void z_log_msg_post_finalize(void)
{
	atomic_val_t cnt = atomic_inc(&buffered_cnt);

	if (panic_mode) {
		static struct k_spinlock process_lock;
		k_spinlock_key_t key = k_spin_lock(&process_lock);
		(void)log_process();

		k_spin_unlock(&process_lock, key);
	} else if (proc_tid != NULL) {
		if (cnt == 0) {
			k_timer_start(&log_process_thread_timer,
				      K_MSEC(CONFIG_LOG_PROCESS_THREAD_SLEEP_MS),
				      K_NO_WAIT);
		} else if (CONFIG_LOG_PROCESS_TRIGGER_THRESHOLD &&
			   cnt == CONFIG_LOG_PROCESS_TRIGGER_THRESHOLD) {
			k_timer_stop(&log_process_thread_timer);
			k_sem_give(&log_process_thread_sem);
		} else {
			/* No action needed. Message processing will be triggered by the
			 * timeout or when number of upcoming messages exceeds the
			 * threshold.
			 */
		}
	}
}

const struct log_backend *log_format_set_all_active_backends(size_t log_type)
{
	const struct log_backend *backend;
	const struct log_backend *failed_backend = NULL;

	for (int i = 0; i < log_backend_count_get(); i++) {
		backend = log_backend_get(i);
		if (log_backend_is_active(backend)) {
			int retCode = log_backend_format_set(backend, log_type);

			if (retCode != 0) {
				failed_backend = backend;
			}
		}
	}
	return failed_backend;
}

void z_log_vprintk(const char *fmt, va_list ap)
{
	if (!IS_ENABLED(CONFIG_LOG_PRINTK)) {
		return;
	}

	z_log_msg_runtime_vcreate(CONFIG_LOG_DOMAIN_ID, NULL,
				   LOG_LEVEL_INTERNAL_RAW_STRING, NULL, 0, 0,
				   fmt, ap);
}

static log_timestamp_t default_get_timestamp(void)
{
	return IS_ENABLED(CONFIG_LOG_TIMESTAMP_64BIT) ?
		sys_clock_tick_get() : k_cycle_get_32();
}

static log_timestamp_t default_lf_get_timestamp(void)
{
	return IS_ENABLED(CONFIG_LOG_TIMESTAMP_64BIT) ?
		k_uptime_get() : k_uptime_get_32();
}

void log_core_init(void)
{
	uint32_t freq;
	log_timestamp_get_t _timestamp_func;

	panic_mode = false;
	dropped_cnt = 0;

	if (IS_ENABLED(CONFIG_LOG_FRONTEND)) {
		log_frontend_init();
		if (IS_ENABLED(CONFIG_LOG_FRONTEND_ONLY)) {
			return;
		}
	}

	/* Set default timestamp. */
	if (sys_clock_hw_cycles_per_sec() > 1000000) {
		_timestamp_func = default_lf_get_timestamp;
		freq = 1000U;
	} else {
		_timestamp_func = default_get_timestamp;
		freq = sys_clock_hw_cycles_per_sec();
	}

	log_set_timestamp_func(_timestamp_func, freq);

	if (IS_ENABLED(CONFIG_LOG_MODE_DEFERRED)) {
		z_log_msg_init();
	}

	if (IS_ENABLED(CONFIG_LOG_RUNTIME_FILTERING)) {
		z_log_runtime_filters_init();
	}
}

static uint32_t activate_foreach_backend(uint32_t mask)
{
	uint32_t mask_cpy = mask;

	while (mask_cpy) {
		uint32_t i = __builtin_ctz(mask_cpy);
		const struct log_backend *backend = log_backend_get(i);

		mask_cpy &= ~BIT(i);
		if (backend->autostart && (log_backend_is_ready(backend) == 0)) {
			mask &= ~BIT(i);
			log_backend_enable(backend,
					   backend->cb->ctx,
					   CONFIG_LOG_MAX_LEVEL);
		}
	}

	return mask;
}

static uint32_t z_log_init(bool blocking, bool can_sleep)
{
	uint32_t mask = 0;

	if (IS_ENABLED(CONFIG_LOG_FRONTEND_ONLY)) {
		return 0;
	}

	__ASSERT_NO_MSG(log_backend_count_get() < LOG_FILTERS_NUM_OF_SLOTS);
	int i;

	if (atomic_inc(&initialized) != 0) {
		return 0;
	}

	/* Assign ids to backends. */
	for (i = 0; i < log_backend_count_get(); i++) {
		const struct log_backend *backend = log_backend_get(i);

		if (backend->autostart) {
			log_backend_init(backend);

			/* If backend has activation function then backend is
			 * not ready until activated.
			 */
			if (log_backend_is_ready(backend) == 0) {
				log_backend_enable(backend,
						   backend->cb->ctx,
						   CONFIG_LOG_MAX_LEVEL);
			} else {
				mask |= BIT(i);
			}
		}
	}

	/* If blocking init, wait until all backends are activated. */
	if (blocking) {
		while (mask) {
			mask = activate_foreach_backend(mask);
			if (IS_ENABLED(CONFIG_MULTITHREADING) && can_sleep) {
				k_msleep(10);
			}
		}
	}

	return mask;
}

void log_init(void)
{
	(void)z_log_init(true, true);
}

static void thread_set(k_tid_t process_tid)
{
	proc_tid = process_tid;

	if (IS_ENABLED(CONFIG_LOG_MODE_IMMEDIATE)) {
		return;
	}

	if (CONFIG_LOG_PROCESS_TRIGGER_THRESHOLD &&
	    process_tid &&
	    buffered_cnt >= CONFIG_LOG_PROCESS_TRIGGER_THRESHOLD) {
		k_sem_give(&log_process_thread_sem);
	}
}

void log_thread_set(k_tid_t process_tid)
{
	if (IS_ENABLED(CONFIG_LOG_PROCESS_THREAD)) {
		__ASSERT_NO_MSG(0);
	} else {
		thread_set(process_tid);
	}
}

int log_set_timestamp_func(log_timestamp_get_t timestamp_getter, uint32_t freq)
{
	if (timestamp_getter == NULL) {
		return -EINVAL;
	}

	timestamp_func = timestamp_getter;
	if (IS_ENABLED(CONFIG_LOG_OUTPUT)) {
		log_output_timestamp_freq_set(freq);
	}

	return 0;
}

void z_impl_log_panic(void)
{
	struct log_backend const *backend;

	if (panic_mode) {
		return;
	}

	/* If panic happened early logger might not be initialized.
	 * Forcing initialization of the logger and auto-starting backends.
	 */
	(void)z_log_init(true, false);

	if (IS_ENABLED(CONFIG_LOG_FRONTEND)) {
		log_frontend_panic();
		if (IS_ENABLED(CONFIG_LOG_FRONTEND_ONLY)) {
			goto out;
		}
	}

	for (int i = 0; i < log_backend_count_get(); i++) {
		backend = log_backend_get(i);

		if (log_backend_is_active(backend)) {
			log_backend_panic(backend);
		}
	}

	if (!IS_ENABLED(CONFIG_LOG_MODE_IMMEDIATE)) {
		/* Flush */
		while (log_process() == true) {
		}
	}

out:
	panic_mode = true;
}

#ifdef CONFIG_USERSPACE
void z_vrfy_log_panic(void)
{
	z_impl_log_panic();
}
#include <syscalls/log_panic_mrsh.c>
#endif

static bool msg_filter_check(struct log_backend const *backend,
			     union log_msg_generic *msg)
{
	if (!z_log_item_is_msg(msg)) {
		return true;
	}

	if (!IS_ENABLED(CONFIG_LOG_RUNTIME_FILTERING)) {
		return true;
	}

	uint32_t backend_level;
	uint8_t level;
	uint8_t domain_id;
	int16_t source_id;
	struct log_source_dynamic_data *source;

	source = (struct log_source_dynamic_data *)log_msg_get_source(&msg->log);
	level = log_msg_get_level(&msg->log);
	domain_id = log_msg_get_domain(&msg->log);
	source_id = source ? log_dynamic_source_id(source) : -1;

	backend_level = log_filter_get(backend, domain_id,
				       source_id, true);

	return (level <= backend_level);
}

static void msg_process(union log_msg_generic *msg)
{
	struct log_backend const *backend;

	for (int i = 0; i < log_backend_count_get(); i++) {
		backend = log_backend_get(i);
		if (log_backend_is_active(backend) &&
		    msg_filter_check(backend, msg)) {
			log_backend_msg_process(backend, msg);
		}
	}
}

void dropped_notify(void)
{
	uint32_t dropped = z_log_dropped_read_and_clear();

	for (int i = 0; i < log_backend_count_get(); i++) {
		struct log_backend const *backend = log_backend_get(i);

		if (log_backend_is_active(backend)) {
			log_backend_dropped(backend, dropped);
		}
	}
}

void z_log_notify_backend_enabled(void)
{
	/* Wakeup logger thread after attaching first backend. It might be
	 * blocked with log messages pending.
	 */
	if (IS_ENABLED(CONFIG_LOG_PROCESS_THREAD) && !backend_attached) {
		k_sem_give(&log_process_thread_sem);
	}

	backend_attached = true;
}

bool z_impl_log_process(void)
{
	if (!IS_ENABLED(CONFIG_LOG_MODE_DEFERRED)) {
		return false;
	}

	union log_msg_generic *msg;

	if (!backend_attached) {
		return false;
	}

	msg = z_log_msg_claim();
	if (msg) {
		atomic_dec(&buffered_cnt);
		msg_process(msg);
		z_log_msg_free(msg);
	}

	if (z_log_dropped_pending()) {
		dropped_notify();
	}

	return z_log_msg_pending();
}

#ifdef CONFIG_USERSPACE
bool z_vrfy_log_process(void)
{
	return z_impl_log_process();
}
#include <syscalls/log_process_mrsh.c>
#endif

uint32_t z_impl_log_buffered_cnt(void)
{
	return buffered_cnt;
}

#ifdef CONFIG_USERSPACE
uint32_t z_vrfy_log_buffered_cnt(void)
{
	return z_impl_log_buffered_cnt();
}
#include <syscalls/log_buffered_cnt_mrsh.c>
#endif

void z_log_dropped(bool buffered)
{
	atomic_inc(&dropped_cnt);
	if (buffered) {
		atomic_dec(&buffered_cnt);
	}
}

uint32_t z_log_dropped_read_and_clear(void)
{
	return atomic_set(&dropped_cnt, 0);
}

bool z_log_dropped_pending(void)
{
	return dropped_cnt > 0;
}

static void notify_drop(const struct mpsc_pbuf_buffer *buffer,
			const union mpsc_pbuf_generic *item)
{
	ARG_UNUSED(buffer);
	ARG_UNUSED(item);

	z_log_dropped(true);
}

void z_log_msg_init(void)
{
	mpsc_pbuf_init(&log_buffer, &mpsc_config);
}

struct log_msg *z_log_msg_alloc(uint32_t wlen)
{
	if (!IS_ENABLED(CONFIG_LOG_MODE_DEFERRED)) {
		return NULL;
	}

	return (struct log_msg *)mpsc_pbuf_alloc(&log_buffer, wlen,
				K_MSEC(CONFIG_LOG_BLOCK_IN_THREAD_TIMEOUT_MS));
}

void z_log_msg_commit(struct log_msg *msg)
{
	union log_msg_generic *m = (union log_msg_generic *)msg;

	msg->hdr.timestamp = timestamp_func();

	if (IS_ENABLED(CONFIG_LOG_MODE_IMMEDIATE)) {
		msg_process(m);

		return;
	}

	mpsc_pbuf_commit(&log_buffer, &m->buf);
	z_log_msg_post_finalize();
}

union log_msg_generic *z_log_msg_claim(void)
{
	return (union log_msg_generic *)mpsc_pbuf_claim(&log_buffer);
}

void z_log_msg_free(union log_msg_generic *msg)
{
	mpsc_pbuf_free(&log_buffer, (union mpsc_pbuf_generic *)msg);
}

bool z_log_msg_pending(void)
{
	return mpsc_pbuf_is_pending(&log_buffer);
}

const char *z_log_get_tag(void)
{
	return CONFIG_LOG_TAG_MAX_LEN > 0 ? tag : NULL;
}

int log_set_tag(const char *str)
{
#if CONFIG_LOG_TAG_MAX_LEN > 0
	if (str == NULL) {
		return -EINVAL;
	}

	size_t len = strlen(str);
	size_t cpy_len = MIN(len, CONFIG_LOG_TAG_MAX_LEN);

	memcpy(tag, str, cpy_len);
	tag[cpy_len] = '\0';

	if (cpy_len < len) {
		tag[cpy_len - 1] = '~';
		return -ENOMEM;
	}

	return 0;
#else
	return -ENOTSUP;
#endif
}

int log_mem_get_usage(uint32_t *buf_size, uint32_t *usage)
{
	__ASSERT_NO_MSG(buf_size != NULL);
	__ASSERT_NO_MSG(usage != NULL);

	if (!IS_ENABLED(CONFIG_LOG_MODE_DEFERRED)) {
		return -EINVAL;
	}

	mpsc_pbuf_get_utilization(&log_buffer, buf_size, usage);

	return 0;
}

int log_mem_get_max_usage(uint32_t *max)
{
	__ASSERT_NO_MSG(max != NULL);

	if (!IS_ENABLED(CONFIG_LOG_MODE_DEFERRED)) {
		return -EINVAL;
	}

	return mpsc_pbuf_get_max_utilization(&log_buffer, max);
}

static void log_backend_notify_all(enum log_backend_evt event,
				   union log_backend_evt_arg *arg)
{
	for (int i = 0; i < log_backend_count_get(); i++) {
		const struct log_backend *backend = log_backend_get(i);

		log_backend_notify(backend, event, arg);
	}
}

static void log_process_thread_timer_expiry_fn(struct k_timer *timer)
{
	k_sem_give(&log_process_thread_sem);
}

static void log_process_thread_func(void *dummy1, void *dummy2, void *dummy3)
{
	__ASSERT_NO_MSG(log_backend_count_get() > 0);

	uint32_t activate_mask = z_log_init(false, false);
	/* If some backends are not activated yet set periodical thread wake up
	 * to poll backends for readiness. Period is set arbitrary.
	 * If all backends are ready periodic wake up is not needed.
	 */
	k_timeout_t timeout = (activate_mask != 0) ? K_MSEC(50) : K_FOREVER;
	bool processed_any = false;

	thread_set(k_current_get());

	/* Logging thread is periodically waken up until all backends that
	 * should be autostarted are ready.
	 */
	while (true) {
		if (activate_mask) {
			activate_mask = activate_foreach_backend(activate_mask);
			if (!activate_mask) {
				/* Periodic wake up no longer needed since all
				 * backends are ready.
				 */
				timeout = K_FOREVER;
			}
		}

		if (log_process() == false) {
			if (processed_any) {
				processed_any = false;
				log_backend_notify_all(LOG_BACKEND_EVT_PROCESS_THREAD_DONE, NULL);
			}
			(void)k_sem_take(&log_process_thread_sem, timeout);
		} else {
			processed_any = true;
		}
	}
}

K_KERNEL_STACK_DEFINE(logging_stack, CONFIG_LOG_PROCESS_THREAD_STACK_SIZE);
struct k_thread logging_thread;

static int enable_logger(const struct device *arg)
{
	ARG_UNUSED(arg);

	if (IS_ENABLED(CONFIG_LOG_PROCESS_THREAD)) {
		k_timer_init(&log_process_thread_timer,
				log_process_thread_timer_expiry_fn, NULL);
		/* start logging thread */
		k_thread_create(&logging_thread, logging_stack,
				K_KERNEL_STACK_SIZEOF(logging_stack),
				log_process_thread_func, NULL, NULL, NULL,
				LOG_PROCESS_THREAD_PRIORITY, 0,
				COND_CODE_1(CONFIG_LOG_PROCESS_THREAD,
					K_MSEC(CONFIG_LOG_PROCESS_THREAD_STARTUP_DELAY_MS),
					K_NO_WAIT));
		k_thread_name_set(&logging_thread, "logging");
	} else {
		(void)z_log_init(false, false);
	}

	return 0;
}

SYS_INIT(enable_logger, POST_KERNEL, 0);
