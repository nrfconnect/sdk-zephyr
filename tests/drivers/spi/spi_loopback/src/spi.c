/*
 * Copyright (c) 2017 Intel Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_LEVEL CONFIG_LOG_DEFAULT_LEVEL
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(main);

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <string.h>
#include <stdio.h>
#include <zephyr/ztest.h>

#include <zephyr/drivers/spi.h>

#define SPI_FAST_DEV	DT_COMPAT_GET_ANY_STATUS_OKAY(test_spi_loopback_fast)
#define SPI_SLOW_DEV	DT_COMPAT_GET_ANY_STATUS_OKAY(test_spi_loopback_slow)

#if CONFIG_SPI_LOOPBACK_MODE_LOOP
#define MODE_LOOP SPI_MODE_LOOP
#else
#define MODE_LOOP 0
#endif

#define SPI_OP SPI_OP_MODE_MASTER | SPI_MODE_CPOL | MODE_LOOP | \
	       SPI_MODE_CPHA | SPI_WORD_SET(8) | SPI_LINES_SINGLE


struct spi_dt_spec spi_fast = SPI_DT_SPEC_GET(SPI_FAST_DEV, SPI_OP, 0);
struct spi_dt_spec spi_slow = SPI_DT_SPEC_GET(SPI_SLOW_DEV, SPI_OP, 0);

/* to run this test, connect MOSI pin to the MISO of the SPI */

#define STACK_SIZE 512
#define BUF_SIZE 17
#define BUF2_SIZE 36

#if CONFIG_NOCACHE_MEMORY
static const char tx_data[BUF_SIZE] = "0123456789abcdef\0";
static __aligned(32) char buffer_tx[BUF_SIZE] __used __attribute__((__section__(".nocache")));
static __aligned(32) char buffer_rx[BUF_SIZE] __used __attribute__((__section__(".nocache")));
static const char tx2_data[BUF2_SIZE] = "Thequickbrownfoxjumpsoverthelazydog\0";
static __aligned(32) char buffer2_tx[BUF2_SIZE] __used __attribute__((__section__(".nocache")));
static __aligned(32) char buffer2_rx[BUF2_SIZE] __used __attribute__((__section__(".nocache")));
#else
/* this src memory shall be in RAM to support using as a DMA source pointer.*/
uint8_t buffer_tx[] = "0123456789abcdef\0";
uint8_t buffer_rx[BUF_SIZE] = {};

uint8_t buffer2_tx[] = "Thequickbrownfoxjumpsoverthelazydog\0";
uint8_t buffer2_rx[BUF2_SIZE] = {};
#endif

/*
 * We need 5x(buffer size) + 1 to print a comma-separated list of each
 * byte in hex, plus a null.
 */
uint8_t buffer_print_tx[BUF_SIZE * 5 + 1];
uint8_t buffer_print_rx[BUF_SIZE * 5 + 1];

uint8_t buffer_print_tx2[BUF2_SIZE * 5 + 1];
uint8_t buffer_print_rx2[BUF2_SIZE * 5 + 1];

static void to_display_format(const uint8_t *src, size_t size, char *dst)
{
	size_t i;

	for (i = 0; i < size; i++) {
		sprintf(dst + 5 * i, "0x%02x,", src[i]);
	}
}

/* test transferring different buffers on the same dma channels */
static int spi_complete_multiple(struct spi_dt_spec *spec)
{
	struct spi_buf tx_bufs[2];
	const struct spi_buf_set tx = {
		.buffers = tx_bufs,
		.count = ARRAY_SIZE(tx_bufs)
	};

	tx_bufs[0].buf = buffer_tx;
	tx_bufs[0].len = BUF_SIZE;

	tx_bufs[1].buf = buffer2_tx;
	tx_bufs[1].len = BUF2_SIZE;


	struct spi_buf rx_bufs[2];
	const struct spi_buf_set rx = {
		.buffers = rx_bufs,
		.count = ARRAY_SIZE(rx_bufs)
	};

	rx_bufs[0].buf = buffer_rx;
	rx_bufs[0].len = BUF_SIZE;

	rx_bufs[1].buf = buffer2_rx;
	rx_bufs[1].len = BUF2_SIZE;

	int ret;

	LOG_INF("Start complete multiple");

	ret = spi_transceive_dt(spec, &tx, &rx);
	if (ret) {
		LOG_ERR("Code %d", ret);
		zassert_false(ret, "SPI transceive failed");
		return ret;
	}

	if (memcmp(buffer_tx, buffer_rx, BUF_SIZE)) {
		to_display_format(buffer_tx, BUF_SIZE, buffer_print_tx);
		to_display_format(buffer_rx, BUF_SIZE, buffer_print_rx);
		LOG_ERR("Buffer contents are different: %s", buffer_print_tx);
		LOG_ERR("                           vs: %s", buffer_print_rx);
		zassert_false(1, "Buffer contents are different");
		return -1;
	}

	if (memcmp(buffer2_tx, buffer2_rx, BUF2_SIZE)) {
		to_display_format(buffer2_tx, BUF2_SIZE, buffer_print_tx2);
		to_display_format(buffer2_rx, BUF2_SIZE, buffer_print_rx2);
		LOG_ERR("Buffer 2 contents are different: %s", buffer_print_tx2);
		LOG_ERR("                             vs: %s", buffer_print_rx2);
		zassert_false(1, "Buffer 2 contents are different");
		return -1;
	}

	LOG_INF("Passed");

	return 0;
}

static int spi_complete_loop(struct spi_dt_spec *spec)
{
	const struct spi_buf tx_bufs[] = {
		{
			.buf = buffer_tx,
			.len = BUF_SIZE,
		},
	};
	const struct spi_buf rx_bufs[] = {
		{
			.buf = buffer_rx,
			.len = BUF_SIZE,
		},
	};
	const struct spi_buf_set tx = {
		.buffers = tx_bufs,
		.count = ARRAY_SIZE(tx_bufs)
	};
	const struct spi_buf_set rx = {
		.buffers = rx_bufs,
		.count = ARRAY_SIZE(rx_bufs)
	};

	int ret;

	LOG_INF("Start complete loop");

	ret = spi_transceive_dt(spec, &tx, &rx);
	if (ret) {
		LOG_ERR("Code %d", ret);
		zassert_false(ret, "SPI transceive failed");
		return ret;
	}

	if (memcmp(buffer_tx, buffer_rx, BUF_SIZE)) {
		to_display_format(buffer_tx, BUF_SIZE, buffer_print_tx);
		to_display_format(buffer_rx, BUF_SIZE, buffer_print_rx);
		LOG_ERR("Buffer contents are different: %s", buffer_print_tx);
		LOG_ERR("                           vs: %s", buffer_print_rx);
		zassert_false(1, "Buffer contents are different");
		return -1;
	}

	LOG_INF("Passed");

	return 0;
}

static int spi_null_tx_buf(struct spi_dt_spec *spec)
{
	static const uint8_t EXPECTED_NOP_RETURN_BUF[BUF_SIZE] = { 0 };

	(void)memset(buffer_rx, 0x77, BUF_SIZE);

	const struct spi_buf tx_bufs[] = {
		/* According to documentation, when sending NULL tx buf -
		 *  NOP frames should be sent on MOSI line
		 */
		{
			.buf = NULL,
			.len = BUF_SIZE,
		},
	};
	const struct spi_buf rx_bufs[] = {
		{
			.buf = buffer_rx,
			.len = BUF_SIZE,
		},
	};
	const struct spi_buf_set tx = {
		.buffers = tx_bufs,
		.count = ARRAY_SIZE(tx_bufs)
	};
	const struct spi_buf_set rx = {
		.buffers = rx_bufs,
		.count = ARRAY_SIZE(rx_bufs)
	};

	int ret;

	LOG_INF("Start null tx");

	ret = spi_transceive_dt(spec, &tx, &rx);
	if (ret) {
		LOG_ERR("Code %d", ret);
		zassert_false(ret, "SPI transceive failed");
		return ret;
	}


	if (memcmp(buffer_rx, EXPECTED_NOP_RETURN_BUF, BUF_SIZE)) {
		to_display_format(buffer_rx, BUF_SIZE, buffer_print_rx);
		LOG_ERR("Rx Buffer should contain NOP frames but got: %s",
			buffer_print_rx);
		zassert_false(1, "Buffer not as expected");
		return -1;
	}

	LOG_INF("Passed");

	return 0;
}

static int spi_rx_half_start(struct spi_dt_spec *spec)
{
	const struct spi_buf tx_bufs[] = {
		{
			.buf = buffer_tx,
			.len = BUF_SIZE,
		},
	};
	const struct spi_buf rx_bufs[] = {
		{
			.buf = buffer_rx,
			.len = 8,
		},
	};
	const struct spi_buf_set tx = {
		.buffers = tx_bufs,
		.count = ARRAY_SIZE(tx_bufs)
	};
	const struct spi_buf_set rx = {
		.buffers = rx_bufs,
		.count = ARRAY_SIZE(rx_bufs)
	};
	int ret;

	LOG_INF("Start half start");

	(void)memset(buffer_rx, 0, BUF_SIZE);

	ret = spi_transceive_dt(spec, &tx, &rx);
	if (ret) {
		LOG_ERR("Code %d", ret);
		zassert_false(ret, "SPI transceive failed");
		return -1;
	}

	if (memcmp(buffer_tx, buffer_rx, 8)) {
		to_display_format(buffer_tx, 8, buffer_print_tx);
		to_display_format(buffer_rx, 8, buffer_print_rx);
		LOG_ERR("Buffer contents are different: %s", buffer_print_tx);
		LOG_ERR("                           vs: %s", buffer_print_rx);
		zassert_false(1, "Buffer contents are different");
		return -1;
	}

	LOG_INF("Passed");

	return 0;
}

static int spi_rx_half_end(struct spi_dt_spec *spec)
{
	const struct spi_buf tx_bufs[] = {
		{
			.buf = buffer_tx,
			.len = BUF_SIZE,
		},
	};
	const struct spi_buf rx_bufs[] = {
		{
			.buf = NULL,
			.len = 8,
		},
		{
			.buf = buffer_rx,
			.len = 8,
		},
	};
	const struct spi_buf_set tx = {
		.buffers = tx_bufs,
		.count = ARRAY_SIZE(tx_bufs)
	};
	const struct spi_buf_set rx = {
		.buffers = rx_bufs,
		.count = ARRAY_SIZE(rx_bufs)
	};
	int ret;

	if (IS_ENABLED(CONFIG_SPI_STM32_DMA)) {
		LOG_INF("Skip half end");
		return 0;
	}

	LOG_INF("Start half end");

	(void)memset(buffer_rx, 0, BUF_SIZE);

	ret = spi_transceive_dt(spec, &tx, &rx);
	if (ret) {
		LOG_ERR("Code %d", ret);
		zassert_false(ret, "SPI transceive failed");
		return -1;
	}

	if (memcmp(buffer_tx + 8, buffer_rx, 8)) {
		to_display_format(buffer_tx + 8, 8, buffer_print_tx);
		to_display_format(buffer_rx, 8, buffer_print_rx);
		LOG_ERR("Buffer contents are different: %s", buffer_print_tx);
		LOG_ERR("                           vs: %s", buffer_print_rx);
		zassert_false(1, "Buffer contents are different");
		return -1;
	}

	LOG_INF("Passed");

	return 0;
}

static int spi_rx_every_4(struct spi_dt_spec *spec)
{
	const struct spi_buf tx_bufs[] = {
		{
			.buf = buffer_tx,
			.len = BUF_SIZE,
		},
	};
	const struct spi_buf rx_bufs[] = {
		{
			.buf = NULL,
			.len = 4,
		},
		{
			.buf = buffer_rx,
			.len = 4,
		},
		{
			.buf = NULL,
			.len = 4,
		},
		{
			.buf = buffer_rx + 4,
			.len = 4,
		},
	};
	const struct spi_buf_set tx = {
		.buffers = tx_bufs,
		.count = ARRAY_SIZE(tx_bufs)
	};
	const struct spi_buf_set rx = {
		.buffers = rx_bufs,
		.count = ARRAY_SIZE(rx_bufs)
	};
	int ret;

	if (IS_ENABLED(CONFIG_SPI_STM32_DMA)) {
		LOG_INF("Skip every 4");
		return 0;
	}

	if (IS_ENABLED(CONFIG_DSPI_MCUX_EDMA)) {
		LOG_INF("Skip every 4");
		return 0;
	}

	LOG_INF("Start every 4");

	(void)memset(buffer_rx, 0, BUF_SIZE);

	ret = spi_transceive_dt(spec, &tx, &rx);
	if (ret) {
		LOG_ERR("Code %d", ret);
		zassert_false(ret, "SPI transceive failed");
		return -1;
	}

	if (memcmp(buffer_tx + 4, buffer_rx, 4)) {
		to_display_format(buffer_tx + 4, 4, buffer_print_tx);
		to_display_format(buffer_rx, 4, buffer_print_rx);
		LOG_ERR("Buffer contents are different: %s", buffer_print_tx);
		LOG_ERR("                           vs: %s", buffer_print_rx);
		zassert_false(1, "Buffer contents are different");
		return -1;
	} else if (memcmp(buffer_tx + 12, buffer_rx + 4, 4)) {
		to_display_format(buffer_tx + 12, 4, buffer_print_tx);
		to_display_format(buffer_rx + 4, 4, buffer_print_rx);
		LOG_ERR("Buffer contents are different: %s", buffer_print_tx);
		LOG_ERR("                           vs: %s", buffer_print_rx);
		zassert_false(1, "Buffer contents are different");
		return -1;
	}

	LOG_INF("Passed");

	return 0;
}

#if (CONFIG_SPI_ASYNC)
static struct k_poll_signal async_sig = K_POLL_SIGNAL_INITIALIZER(async_sig);
static struct k_poll_event async_evt =
	K_POLL_EVENT_INITIALIZER(K_POLL_TYPE_SIGNAL,
				 K_POLL_MODE_NOTIFY_ONLY,
				 &async_sig);
static K_SEM_DEFINE(caller, 0, 1);
K_THREAD_STACK_DEFINE(spi_async_stack, STACK_SIZE);
static int result = 1;

static void spi_async_call_cb(struct k_poll_event *async_evt,
			      struct k_sem *caller_sem,
			      void *unused)
{
	int ret;

	LOG_DBG("Polling...");

	while (1) {
		ret = k_poll(async_evt, 1, K_MSEC(200));
		zassert_false(ret, "one or more events are not ready");

		result = async_evt->signal->result;
		k_sem_give(caller_sem);

		/* Reinitializing for next call */
		async_evt->signal->signaled = 0U;
		async_evt->state = K_POLL_STATE_NOT_READY;
	}
}

static int spi_async_call(struct spi_dt_spec *spec)
{
	const struct spi_buf tx_bufs[] = {
		{
			.buf = buffer_tx,
			.len = BUF_SIZE,
		},
	};
	const struct spi_buf rx_bufs[] = {
		{
			.buf = buffer_rx,
			.len = BUF_SIZE,
		},
	};
	const struct spi_buf_set tx = {
		.buffers = tx_bufs,
		.count = ARRAY_SIZE(tx_bufs)
	};
	const struct spi_buf_set rx = {
		.buffers = rx_bufs,
		.count = ARRAY_SIZE(rx_bufs)
	};
	int ret;

	LOG_INF("Start async call");

	ret = spi_transceive_signal(spec->bus, &spec->config, &tx, &rx, &async_sig);
	if (ret == -ENOTSUP) {
		LOG_DBG("Not supported");
		return 0;
	}

	if (ret) {
		LOG_ERR("Code %d", ret);
		zassert_false(ret, "SPI transceive failed");
		return -1;
	}

	k_sem_take(&caller, K_FOREVER);

	if (result) {
		LOG_ERR("Call code %d", ret);
		zassert_false(result, "SPI transceive failed");
		return -1;
	}

	LOG_INF("Passed");

	return 0;
}
#endif

static int spi_resource_lock_test(struct spi_dt_spec *lock_spec,
				  struct spi_dt_spec *try_spec)
{
	lock_spec->config.operation |= SPI_LOCK_ON;

	if (spi_complete_loop(lock_spec)) {
		return -1;
	}

	if (spi_release_dt(lock_spec)) {
		LOG_ERR("Deadlock now?");
		zassert_false(1, "SPI release failed");
		return -1;
	}

	if (spi_complete_loop(try_spec)) {
		return -1;
	}

	return 0;
}

ZTEST(spi_loopback, test_spi_loopback)
{
#if (CONFIG_SPI_ASYNC)
	struct k_thread async_thread;
	k_tid_t async_thread_id;
#endif
	LOG_INF("SPI test on buffers TX/RX %p/%p", buffer_tx, buffer_rx);

#if (CONFIG_SPI_ASYNC)
	async_thread_id = k_thread_create(&async_thread,
					  spi_async_stack, STACK_SIZE,
					  (k_thread_entry_t)spi_async_call_cb,
					  &async_evt, &caller, NULL,
					  K_PRIO_COOP(7), 0, K_NO_WAIT);
#endif
	zassert_true(spi_is_ready_dt(&spi_slow), "Slow spi lookback device is not ready");

	LOG_INF("SPI test slow config");

	if (spi_complete_multiple(&spi_slow) ||
	    spi_complete_loop(&spi_slow) ||
	    spi_null_tx_buf(&spi_slow) ||
	    spi_rx_half_start(&spi_slow) ||
	    spi_rx_half_end(&spi_slow) ||
	    spi_rx_every_4(&spi_slow)
#if (CONFIG_SPI_ASYNC)
	    || spi_async_call(&spi_slow)
#endif
	    ) {
		goto end;
	}

	zassert_true(spi_is_ready_dt(&spi_fast), "Fast spi lookback device is not ready");

	LOG_INF("SPI test fast config");

	if (spi_complete_multiple(&spi_fast) ||
	    spi_complete_loop(&spi_fast) ||
	    spi_null_tx_buf(&spi_fast) ||
	    spi_rx_half_start(&spi_fast) ||
	    spi_rx_half_end(&spi_fast) ||
	    spi_rx_every_4(&spi_fast)
#if (CONFIG_SPI_ASYNC)
	    || spi_async_call(&spi_fast)
#endif
	    ) {
		goto end;
	}

	if (spi_resource_lock_test(&spi_slow, &spi_fast)) {
		goto end;
	}

	LOG_INF("All tx/rx passed");
end:
#if (CONFIG_SPI_ASYNC)
	k_thread_abort(async_thread_id);
#else
	;
#endif
}

static void *spi_loopback_setup(void)
{
#if CONFIG_NOCACHE_MEMORY
	memset(buffer_tx, 0, sizeof(buffer_tx));
	memcpy(buffer_tx, tx_data, sizeof(tx_data));
	memset(buffer2_tx, 0, sizeof(buffer2_tx));
	memcpy(buffer2_tx, tx2_data, sizeof(tx2_data));
#endif
	return NULL;
}

ZTEST_SUITE(spi_loopback, NULL, spi_loopback_setup, NULL, NULL, NULL);
