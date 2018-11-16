/*
 * Copyright (c) 2018 Nordic Semiconductor
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Test log core
 *
 */


#include <tc_util.h>
#include <stdbool.h>
#include <zephyr.h>
#include <ztest.h>
#include <logging/log_backend.h>
#include <logging/log_ctrl.h>
#include <logging/log.h>
#include "test_module.h"

#define LOG_MODULE_NAME test
LOG_MODULE_REGISTER(LOG_MODULE_NAME);

typedef void (*custom_put_callback_t)(struct log_backend const *const backend,
				      struct log_msg *msg, size_t counter);
struct backend_cb {
	size_t counter;
	bool panic;
	bool keep_msgs;
	bool check_id;
	u32_t exp_id[100];
	bool check_timestamp;
	u32_t exp_timestamps[100];
	bool check_args;
	u32_t exp_nargs[100];
	bool check_strdup;
	bool exp_strdup[100];
	custom_put_callback_t callback;
};

static void put(struct log_backend const *const backend,
		struct log_msg *msg)
{
	log_msg_get(msg);
	u32_t nargs = log_msg_nargs_get(msg);
	struct backend_cb *cb = (struct backend_cb *)backend->cb->ctx;

	if (cb->check_id) {
		u32_t exp_id = cb->exp_id[cb->counter];

		zassert_equal(log_msg_source_id_get(msg),
			      exp_id,
			      "Unexpected source_id");
	}
	if (cb->check_timestamp) {
		u32_t exp_timestamp = cb->exp_timestamps[cb->counter];

		zassert_equal(log_msg_timestamp_get(msg),
			      exp_timestamp,
			      "Unexpected message index");
	}

	/* Arguments in the test are fixed, 1,2,3,4,5,... */
	if (cb->check_args && log_msg_is_std(msg) && nargs > 0) {
		for (int i = 0; i < nargs; i++) {
			u32_t arg = log_msg_arg_get(msg, i);

			zassert_equal(i+1, arg,
				      "Unexpected argument in the message");
		}
	}

	if (cb->check_strdup) {
		zassert_false(cb->exp_strdup[cb->counter]
			      ^ log_is_strdup((void *)log_msg_arg_get(msg, 0)),
			      NULL);
	}

	if (cb->callback) {
		cb->callback(backend, msg, cb->counter);
	}

	cb->counter++;

	if (!cb->keep_msgs) {
		log_msg_put(msg);
	}
}

static void panic(struct log_backend const *const backend)
{
	struct backend_cb *cb = (struct backend_cb *)backend->cb->ctx;

	cb->panic = true;
}

const struct log_backend_api log_backend_test_api = {
	.put = put,
	.panic = panic,
};

LOG_BACKEND_DEFINE(backend1, log_backend_test_api, true);
struct backend_cb backend1_cb;

LOG_BACKEND_DEFINE(backend2, log_backend_test_api, true);
struct backend_cb backend2_cb;

static u32_t stamp;

static u32_t test_source_id;

static u32_t timestamp_get(void)
{
	return stamp++;
}

/**
 * @brief Function for finding source ID based on source name.
 *
 * @param name Source name
 *
 * @return Source ID.
 */
static int log_source_id_get(const char *name)
{

	for (int i = 0; i < log_src_cnt_get(CONFIG_LOG_DOMAIN_ID); i++) {
		if (strcmp(log_source_name_get(CONFIG_LOG_DOMAIN_ID, i), name)
		    == 0) {
			return i;
		}
	}
	return -1;
}

static void log_setup(bool backend2_enable)
{
	stamp = 0;

	log_init();

	zassert_equal(0, log_set_timestamp_func(timestamp_get, 0),
		      "Expects successful timestamp function setting.");

	memset(&backend1_cb, 0, sizeof(backend1_cb));

	log_backend_enable(&backend1, &backend1_cb, LOG_LEVEL_DBG);

	if (backend2_enable) {
		memset(&backend2_cb, 0, sizeof(backend2_cb));

		log_backend_enable(&backend2, &backend2_cb, LOG_LEVEL_DBG);
	} else {
		log_backend_disable(&backend2);
	}

	test_source_id = log_source_id_get(STRINGIFY(LOG_MODULE_NAME));
}

/*
 * Test is using 2 backends and runtime filtering is enabled. After first call
 * filtering for backend2 is reduced to warning. It is expected that next INFO
 * level log message will be passed only to backend1.
 */
static void test_log_backend_runtime_filtering(void)
{
	log_setup(true);
	backend1_cb.check_timestamp = true;
	backend2_cb.check_timestamp = true;

	backend1_cb.exp_timestamps[0] = 0;
	backend1_cb.exp_timestamps[1] = 1;
	backend1_cb.exp_timestamps[2] = 2;

	/* Expect one less log message */
	backend2_cb.exp_timestamps[0] = 0;
	backend2_cb.exp_timestamps[1] = 2;

	LOG_INF("test");
	while (log_process(false)) {
	}

	log_filter_set(&backend2,
			CONFIG_LOG_DOMAIN_ID,
			test_source_id,
			LOG_LEVEL_WRN);

	LOG_INF("test");
	LOG_WRN("test");

	while (log_process(false)) {
	}

	zassert_equal(3,
		      backend1_cb.counter,
		      "Unexpected amount of messages received by the backend.");

	zassert_equal(2,
		      backend2_cb.counter,
		      "Unexpected amount of messages received by the backend.");
}

/*
 * When LOG_MOVE_OVERFLOW is enabled, logger should discard oldest messages when
 * there is no room. However, if after discarding all messages there is still no
 * room then current log is discarded.
 */
u8_t data[CONFIG_LOG_BUFFER_SIZE];
static void test_log_overflow(void)
{
	u32_t msgs_in_buf = CONFIG_LOG_BUFFER_SIZE/sizeof(union log_msg_chunk);
	u32_t max_hexdump_len = LOG_MSG_HEXDUMP_BYTES_HEAD_CHUNK +
			    HEXDUMP_BYTES_CONT_MSG * (msgs_in_buf - 1);
	u32_t hexdump_len = max_hexdump_len - HEXDUMP_BYTES_CONT_MSG;


	zassert_true(IS_ENABLED(CONFIG_LOG_MODE_OVERFLOW),
		     "Test requires that overflow mode is enabled");

	log_setup(false);
	backend1_cb.check_timestamp = true;
	backend2_cb.check_timestamp = true;

	/* expect first message to be dropped */
	backend1_cb.exp_timestamps[0] = 1;
	backend1_cb.exp_timestamps[1] = 2;

	LOG_INF("test");
	LOG_INF("test");
	LOG_HEXDUMP_INF(data, hexdump_len, "test");

	while (log_process(false)) {
	}

	/* Expect big message to be dropped because it does not fit in.
	 * First message is also dropped in the process of finding free space.
	 */
	backend1_cb.exp_timestamps[2] = 3;

	LOG_INF("test");
	LOG_HEXDUMP_INF(data, max_hexdump_len+1, "test");

	while (log_process(false)) {
	}

	zassert_equal(2,
		      backend1_cb.counter,
		      "Unexpected amount of messages received by the backend.");
}

/*
 * Test checks if arguments are correctly processed by the logger.
 *
 * Log messages with supported number of messages are called. Test backend
 * validates number of arguments and values.
 */
static void test_log_arguments(void)
{
	log_setup(false);
	backend1_cb.check_args = true;

	backend1_cb.exp_nargs[0] = 10;
	backend1_cb.exp_nargs[1] = 1;
	backend1_cb.exp_nargs[2] = 2;
	backend1_cb.exp_nargs[3] = 3;
	backend1_cb.exp_nargs[4] = 4;
	backend1_cb.exp_nargs[5] = 5;
	backend1_cb.exp_nargs[6] = 6;
	backend1_cb.exp_nargs[7] = 10;

	LOG_INF("test");
	LOG_INF("test %d", 1);
	LOG_INF("test %d %d", 1, 2);
	LOG_INF("test %d %d %d", 1, 2, 3);
	LOG_INF("test %d %d %d %d", 1, 2, 3, 4);
	LOG_INF("test %d %d %d %d %d", 1, 2, 3, 4, 5);
	LOG_INF("test %d %d %d %d %d %d", 1, 2, 3, 4, 5, 6);
	LOG_INF("test %d %d %d %d %d %d %d %d %d %d",
			1, 2, 3, 4, 5, 6, 7, 8, 9, 10);

	while (log_process(false)) {
	}

	zassert_equal(8,
		      backend1_cb.counter,
		      "Unexpected amount of messages received by the backend.");
}

/*
 * Test checks if panic is correctly executed. On panic logger should flush all
 * messages and process logs in place (not in deferred way).
 */
static void test_log_panic(void)
{
	log_setup(false);

	LOG_INF("test");
	LOG_INF("test");

	/* logs should be flushed in panic */
	log_panic();
	zassert_true(backend1_cb.panic,
		     "Expecting backend to receive panic notification.");

	zassert_equal(2,
		      backend1_cb.counter,
		      "Unexpected amount of messages received by the backend.");

	/* messages processed where called */
	LOG_INF("test");

	zassert_equal(3,
		      backend1_cb.counter,
		      "Unexpected amount of messages received by the backend.");
}

/* Function comes from the file which is part of test module. It is
 * expected that logs coming from it will have same source_id as current
 * module (this file).
 */
static void test_log_from_declared_module(void)
{
	log_setup(false);

	/* Setup log backend to validate source_id of the message. */
	backend1_cb.check_id = true;
	backend1_cb.exp_id[0] = LOG_CURRENT_MODULE_ID();
	backend1_cb.exp_id[1] = LOG_CURRENT_MODULE_ID();

	test_func();
	test_inline_func();

	while (log_process(false)) {
	}

	zassert_equal(2, backend1_cb.counter,
		      "Unexpected amount of messages received by the backend.");
}

static void test_log_strdup_gc(void)
{
	char test_str[] = "test string";

	log_setup(false);

	BUILD_ASSERT_MSG(CONFIG_LOG_STRDUP_BUF_COUNT == 1,
			"Test assumes certain configuration");

	backend1_cb.exp_strdup[0] = true;
	backend1_cb.exp_strdup[1] = false;

	LOG_INF("%s", log_strdup(test_str));
	LOG_INF("%s", log_strdup(test_str));

	while (log_process(false)) {
	}

	zassert_equal(2, backend1_cb.counter,
		      "Unexpected amount of messages received by the backend.");

	/* Processing should free strdup buffer. */
	backend1_cb.exp_strdup[2] = true;
	LOG_INF("%s", log_strdup(test_str));

	while (log_process(false)) {
	}

	zassert_equal(3, backend1_cb.counter,
		      "Unexpected amount of messages received by the backend.");

}

static void strdup_trim_callback(struct log_backend const *const backend,
			  struct log_msg *msg, size_t counter)
{
	char *str = (char *)log_msg_arg_get(msg, 0);
	size_t len = strlen(str);

	zassert_equal(len, CONFIG_LOG_STRDUP_MAX_STRING,
			"Expected trimmed string");
}

static void test_strdup_trimming(void)
{
	char test_str[] = "123456789";

	BUILD_ASSERT_MSG(CONFIG_LOG_STRDUP_MAX_STRING == 8,
			"Test assumes certain configuration");

	log_setup(false);

	backend1_cb.callback = strdup_trim_callback;

	LOG_INF("%s", log_strdup(test_str));

	while (log_process(false)) {
	}

	zassert_equal(1, backend1_cb.counter,
		      "Unexpected amount of messages received by the backend.");
}

/*test case main entry*/
void test_main(void)
{
	ztest_test_suite(test_log_list,
			 ztest_unit_test(test_log_backend_runtime_filtering),
			 ztest_unit_test(test_log_overflow),
			 ztest_unit_test(test_log_arguments),
			 ztest_unit_test(test_log_panic),
			 ztest_unit_test(test_log_from_declared_module),
			 ztest_unit_test(test_log_strdup_gc),
			 ztest_unit_test(test_strdup_trimming));
	ztest_run_test_suite(test_log_list);
}
