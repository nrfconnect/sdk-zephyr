/*
 * Copyright (c) 2018 Makaio GmbH
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/shell/shell_rtt.h>
#include <zephyr/init.h>
#include <SEGGER_RTT.h>
#include <zephyr/logging/log.h>

#if IS_ENABLED(CONFIG_LOG_BACKEND_RTT)
BUILD_ASSERT(!(CONFIG_SHELL_BACKEND_RTT_BUFFER == CONFIG_LOG_BACKEND_RTT_BUFFER),
	     "Conflicting log RTT backend enabled on the same channel");
#endif

static uint8_t shell_rtt_up_buf[CONFIG_SEGGER_RTT_BUFFER_SIZE_UP];
static uint8_t shell_rtt_down_buf[CONFIG_SEGGER_RTT_BUFFER_SIZE_DOWN];

SHELL_RTT_DEFINE(shell_transport_rtt);
SHELL_DEFINE(shell_rtt, CONFIG_SHELL_PROMPT_RTT, &shell_transport_rtt,
	     CONFIG_SHELL_BACKEND_RTT_LOG_MESSAGE_QUEUE_SIZE,
	     CONFIG_SHELL_BACKEND_RTT_LOG_MESSAGE_QUEUE_TIMEOUT,
	     SHELL_FLAG_OLF_CRLF);

LOG_MODULE_REGISTER(shell_rtt, CONFIG_SHELL_RTT_LOG_LEVEL);

static bool rtt_blocking;

static void timer_handler(struct k_timer *timer)
{
	const struct shell_rtt *sh_rtt = k_timer_user_data_get(timer);

	if (SEGGER_RTT_HasData(CONFIG_SHELL_BACKEND_RTT_BUFFER)) {
		sh_rtt->handler(SHELL_TRANSPORT_EVT_RX_RDY, sh_rtt->context);
	}
}

static int init(const struct shell_transport *transport,
		const void *config,
		shell_transport_handler_t evt_handler,
		void *context)
{
	struct shell_rtt *sh_rtt = (struct shell_rtt *)transport->ctx;

	sh_rtt->handler = evt_handler;
	sh_rtt->context = context;

	k_timer_init(&sh_rtt->timer, timer_handler, NULL);
	k_timer_user_data_set(&sh_rtt->timer, (void *)sh_rtt);
	k_timer_start(&sh_rtt->timer, K_MSEC(CONFIG_SHELL_RTT_RX_POLL_PERIOD),
			K_MSEC(CONFIG_SHELL_RTT_RX_POLL_PERIOD));

	if (CONFIG_SHELL_BACKEND_RTT_BUFFER > 0) {
		SEGGER_RTT_ConfigUpBuffer(CONFIG_SHELL_BACKEND_RTT_BUFFER, "Shell",
					  shell_rtt_up_buf, sizeof(shell_rtt_up_buf),
					  SEGGER_RTT_MODE_NO_BLOCK_SKIP);

		SEGGER_RTT_ConfigDownBuffer(CONFIG_SHELL_BACKEND_RTT_BUFFER, "Shell",
					  shell_rtt_down_buf, sizeof(shell_rtt_down_buf),
					  SEGGER_RTT_MODE_NO_BLOCK_SKIP);
	}

	return 0;
}

static int uninit(const struct shell_transport *transport)
{
	struct shell_rtt *sh_rtt = (struct shell_rtt *)transport->ctx;

	k_timer_stop(&sh_rtt->timer);

	return 0;
}

static int enable(const struct shell_transport *transport, bool blocking)
{
	struct shell_rtt *sh_rtt = (struct shell_rtt *)transport->ctx;

	if (blocking) {
		rtt_blocking = true;
		k_timer_stop(&sh_rtt->timer);
	}

	return 0;
}

static int write(const struct shell_transport *transport,
		 const void *data, size_t length, size_t *cnt)
{
	struct shell_rtt *sh_rtt = (struct shell_rtt *)transport->ctx;
	const uint8_t *data8 = (const uint8_t *)data;

	if (rtt_blocking) {
		*cnt = SEGGER_RTT_WriteNoLock(CONFIG_SHELL_BACKEND_RTT_BUFFER, data8, length);
		while (SEGGER_RTT_HasDataUp(CONFIG_SHELL_BACKEND_RTT_BUFFER)) {
			/* empty */
		}
	} else {
		*cnt = SEGGER_RTT_Write(CONFIG_SHELL_BACKEND_RTT_BUFFER, data8, length);
	}

	sh_rtt->handler(SHELL_TRANSPORT_EVT_TX_RDY, sh_rtt->context);

	return 0;
}

static int read(const struct shell_transport *transport,
		void *data, size_t length, size_t *cnt)
{
	*cnt = SEGGER_RTT_Read(CONFIG_SHELL_BACKEND_RTT_BUFFER, data, length);

	return 0;
}

const struct shell_transport_api shell_rtt_transport_api = {
	.init = init,
	.uninit = uninit,
	.enable = enable,
	.write = write,
	.read = read
};

static int enable_shell_rtt(void)
{
	bool log_backend = CONFIG_SHELL_RTT_INIT_LOG_LEVEL > 0;
	uint32_t level = (CONFIG_SHELL_RTT_INIT_LOG_LEVEL > LOG_LEVEL_DBG) ?
		      CONFIG_LOG_MAX_LEVEL : CONFIG_SHELL_RTT_INIT_LOG_LEVEL;
	static const struct shell_backend_config_flags cfg_flags =
					SHELL_DEFAULT_BACKEND_CONFIG_FLAGS;

	shell_init(&shell_rtt, NULL, cfg_flags, log_backend, level);

	return 0;
}

/* Function is used for testing purposes */
const struct shell *shell_backend_rtt_get_ptr(void)
{
	return &shell_rtt;
}
SYS_INIT(enable_shell_rtt, POST_KERNEL, 0);
