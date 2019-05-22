/*
 * Copyright (c) 2018 Piotr Mienkowski
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/** @file
 * @brief Serial Wire Output (SWO) backend implementation.
 *
 * SWO/SWV has been developed by ARM. The following code works only on ARM
 * architecture.
 *
 * An SWO viewer program will typically set-up the SWO port including its
 * frequency when connected to the debug probe. Such configuration can persist
 * only until the MCU reset. The SWO backend initialization function will
 * re-configure the SWO port upon boot and set the frequency as specified by
 * the LOG_BACKEND_SWO_FREQ_HZ Kconfig option. To ensure flawless operation
 * this frequency should much the one set by the SWO viewer program.
 *
 * The initialization code assumes that SWO core frequency is equal to HCLK
 * as defined by SYS_CLOCK_HW_CYCLES_PER_SEC Kconfig option. This may require
 * additional, vendor specific configuration.
 */

#include <logging/log_backend.h>
#include <logging/log_core.h>
#include <logging/log_msg.h>
#include <logging/log_output.h>
#include <soc.h>

/** The stimulus port from which SWO data is received and displayed */
#define ITM_PORT_LOGGER       0

/* Set TPIU prescaler for the current debug trace clock frequency. */
#if CONFIG_LOG_BACKEND_SWO_FREQ_HZ == 0
#define SWO_FREQ_DIV  1
#else
#define SWO_FREQ (CONFIG_SYS_CLOCK_HW_CYCLES_PER_SEC \
		  + (CONFIG_LOG_BACKEND_SWO_FREQ_HZ / 2))
#define SWO_FREQ_DIV  (SWO_FREQ / CONFIG_LOG_BACKEND_SWO_FREQ_HZ)
#if SWO_FREQ_DIV > 0xFFFF
#error CONFIG_LOG_BACKEND_SWO_FREQ_HZ is too low. SWO clock divider is 16-bit. \
	Minimum supported SWO clock frequency is \
	CONFIG_SYS_CLOCK_HW_CYCLES_PER_SEC/2^16.
#endif
#endif

static u8_t buf[1];

static int char_out(u8_t *data, size_t length, void *ctx)
{
	ARG_UNUSED(ctx);

	for (size_t i = 0; i < length; i++) {
		ITM_SendChar(data[i]);
	}

	return length;
}

LOG_OUTPUT_DEFINE(log_output, char_out, buf, sizeof(buf));

static void log_backend_swo_put(const struct log_backend *const backend,
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

static void log_backend_swo_init(void)
{
	/* Enable DWT and ITM units */
	CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
	/* Enable access to ITM registers */
	ITM->LAR  = 0xC5ACCE55;
	/* Disable stimulus ports ITM_STIM0-ITM_STIM31 */
	ITM->TER  = 0x0;
	/* Disable ITM */
	ITM->TCR  = 0x0;
	/* Select NRZ (UART) encoding protocol */
	TPI->SPPR = 2;
	/* Set SWO baud rate prescaler value: SWO_clk = ref_clock/(ACPR + 1) */
	TPI->ACPR = SWO_FREQ_DIV - 1;
	/* Enable unprivileged access to ITM stimulus ports */
	ITM->TPR  = 0x0;
	/* Configure Debug Watchpoint and Trace */
	DWT->CTRL = 0x400003FE;
	/* Configure Formatter and Flush Control Register */
	TPI->FFCR = 0x00000100;
	/* Enable ITM, set TraceBusID=1, no local timestamp generation */
	ITM->TCR  = 0x0001000D;
	/* Enable stimulus port used by the logger */
	ITM->TER  = 1 << ITM_PORT_LOGGER;
}

static void log_backend_swo_panic(struct log_backend const *const backend)
{
}

static void log_backend_swo_sync_string(const struct log_backend *const backend,
		struct log_msg_ids src_level, u32_t timestamp,
		const char *fmt, va_list ap)
{
	u32_t flags = LOG_OUTPUT_FLAG_LEVEL | LOG_OUTPUT_FLAG_TIMESTAMP;
	u32_t key;

	if (IS_ENABLED(CONFIG_LOG_BACKEND_SHOW_COLOR)) {
		flags |= LOG_OUTPUT_FLAG_COLORS;
	}

	if (IS_ENABLED(CONFIG_LOG_BACKEND_FORMAT_TIMESTAMP)) {
		flags |= LOG_OUTPUT_FLAG_FORMAT_TIMESTAMP;
	}

	key = irq_lock();
	log_output_string(&log_output, src_level, timestamp, fmt, ap, flags);
	irq_unlock(key);
}

static void log_backend_swo_sync_hexdump(
		const struct log_backend *const backend,
		struct log_msg_ids src_level, u32_t timestamp,
		const char *metadata, const u8_t *data, u32_t length)
{
	u32_t flags = LOG_OUTPUT_FLAG_LEVEL | LOG_OUTPUT_FLAG_TIMESTAMP;
	u32_t key;

	if (IS_ENABLED(CONFIG_LOG_BACKEND_SHOW_COLOR)) {
		flags |= LOG_OUTPUT_FLAG_COLORS;
	}

	if (IS_ENABLED(CONFIG_LOG_BACKEND_FORMAT_TIMESTAMP)) {
		flags |= LOG_OUTPUT_FLAG_FORMAT_TIMESTAMP;
	}

	key = irq_lock();
	log_output_hexdump(&log_output, src_level, timestamp,
			metadata, data, length, flags);
	irq_unlock(key);
}

const struct log_backend_api log_backend_swo_api = {
	.put = IS_ENABLED(CONFIG_LOG_IMMEDIATE) ? NULL : log_backend_swo_put,
	.put_sync_string = IS_ENABLED(CONFIG_LOG_IMMEDIATE) ?
			log_backend_swo_sync_string : NULL,
	.put_sync_hexdump = IS_ENABLED(CONFIG_LOG_IMMEDIATE) ?
			log_backend_swo_sync_hexdump : NULL,
	.panic = log_backend_swo_panic,
	.init = log_backend_swo_init,
};

LOG_BACKEND_DEFINE(log_backend_swo, log_backend_swo_api, true);
