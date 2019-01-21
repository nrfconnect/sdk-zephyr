/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <logging/log_backend.h>
#include <logging/log_core.h>
#include <logging/log_msg.h>
#include <logging/log_output.h>
#include <device.h>
#include <uart.h>
#include <assert.h>

static int char_out(u8_t *data, size_t length, void *ctx)
{
	struct device *dev = (struct device *)ctx;

	for (size_t i = 0; i < length; i++) {
		uart_poll_out(dev, data[i]);
	}

	return length;
}

static u8_t buf;

LOG_OUTPUT_DEFINE(log_output, char_out, &buf, 1);

static void put(const struct log_backend *const backend,
		struct log_msg *msg)
{
	log_msg_get(msg);

	u32_t flags = LOG_OUTPUT_FLAG_LEVEL | LOG_OUTPUT_FLAG_TIMESTAMP;

	if (IS_ENABLED(CONFIG_LOG_BACKEND_SHOW_COLOR)) {
		flags |= LOG_OUTPUT_FLAG_COLORS;
	}

	if (IS_ENABLED(CONFIG_LOG_BACKEND_FORMAT_TIMESTAMP)) {
		flags |= LOG_OUTPUT_FLAG_FORMAT_TIMESTAMP;
	}

	log_output_msg_process(&log_output, msg, flags);

	log_msg_put(msg);

}

static void log_backend_uart_init(void)
{
	struct device *dev;

	dev = device_get_binding(CONFIG_UART_CONSOLE_ON_DEV_NAME);
	assert(dev);

	log_output_ctx_set(&log_output, dev);
}

static void panic(struct log_backend const *const backend)
{
	log_output_flush(&log_output);
}

static void dropped(const struct log_backend *const backend, u32_t cnt)
{
	ARG_UNUSED(backend);

	log_output_dropped_process(&log_output, cnt);
}

const struct log_backend_api log_backend_uart_api = {
	.put = put,
	.panic = panic,
	.init = log_backend_uart_init,
	.dropped = dropped,
};

LOG_BACKEND_DEFINE(log_backend_uart, log_backend_uart_api, true);
