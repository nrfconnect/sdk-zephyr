/**
 * Copyright (c) 2018 Linaro
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#define LOG_LEVEL CONFIG_WIFI_LOG_LEVEL
#include <logging/log.h>
LOG_MODULE_REGISTER(wifi_eswifi_bus_spi);

#include <zephyr.h>
#include <kernel.h>
#include <device.h>
#include <string.h>
#include <errno.h>
#include <gpio.h>
#include <spi.h>

#include "eswifi.h"

#define ESWIFI_SPI_THREAD_STACK_SIZE 1024
K_THREAD_STACK_MEMBER(eswifi_spi_poll_stack, ESWIFI_SPI_THREAD_STACK_SIZE);

struct eswifi_spi_data {
	struct device *spi_dev;
	struct eswifi_gpio csn;
	struct eswifi_gpio dr;
	struct k_thread poll_thread;
	struct spi_config spi_cfg;
	struct spi_cs_control spi_cs;
};

static struct eswifi_spi_data eswifi_spi0; /* Static instance */

static bool eswifi_spi_cmddata_ready(struct eswifi_spi_data *spi)
{
	int value;

	gpio_pin_read(spi->dr.dev, spi->dr.pin, &value);

	return value ? true : false;
}

static int eswifi_spi_wait_cmddata_ready(struct eswifi_spi_data *spi)
{
	unsigned int max_retries = 60 * 1000; /* 1 minute */

	do {
		/* allow other threads to be scheduled */
		k_sleep(1);
	} while (!eswifi_spi_cmddata_ready(spi) && --max_retries);

	return max_retries ? 0 : -ETIMEDOUT;
}

static int eswifi_spi_write(struct eswifi_dev *eswifi, char *data, size_t dlen)
{
	struct eswifi_spi_data *spi = eswifi->bus_data;
	struct spi_buf spi_tx_buf[1];
	struct spi_buf_set spi_tx;
	int status;

	spi_tx_buf[0].buf = data;
	spi_tx_buf[0].len = dlen / 2; /* 16-bit words */
	spi_tx.buffers = spi_tx_buf;
	spi_tx.count = ARRAY_SIZE(spi_tx_buf);

	status = spi_write(spi->spi_dev, &spi->spi_cfg, &spi_tx);
	if (status) {
		LOG_ERR("SPI write error %d", status);
	} else {
		status = dlen;
	}

	return status;
}

static int eswifi_spi_read(struct eswifi_dev *eswifi, char *data, size_t dlen)
{
	struct eswifi_spi_data *spi = eswifi->bus_data;
	struct spi_buf spi_rx_buf[1];
	struct spi_buf_set spi_rx;
	int status;

	spi_rx_buf[0].buf = data;
	spi_rx_buf[0].len = dlen / 2; /* 16-bit words */
	spi_rx.buffers = spi_rx_buf;
	spi_rx.count = ARRAY_SIZE(spi_rx_buf);

	status = spi_read(spi->spi_dev, &spi->spi_cfg, &spi_rx);
	if (status) {
		LOG_ERR("SPI read error %d", status);
	} else {
		status = dlen;
	}

	return status;
}

static int eswifi_spi_request(struct eswifi_dev *eswifi, char *cmd, size_t clen,
			      char *rsp, size_t rlen)
{
	struct eswifi_spi_data *spi = eswifi->bus_data;
	char tmp[2];
	int err;

	LOG_DBG("cmd=%p (%u byte), rsp=%p (%u byte)", cmd, clen, rsp, rlen);

	/*
	 * CMD/DATA protocol:
	 * 1. Module raises data-ready when ready for **command phase**
	 * 2. Host announces command start by lowering chip-select (csn)
	 * 3. Host write the command (possibly several spi transfers)
	 * 4. Host announces end of command by raising chip-select
	 * 5. Module lowers data-ready signal
	 * 6. Module raises data-ready to signal start of the **data phase**
	 * 7. Host lowers chip-select
	 * 8. Host fetch data as long as data-ready pin is up
	 * 9. Module lowers data-ready to signal the end of the data Phase
	 * 10. Host raises chip-select
	 *
	 * Note:
	 * All commands to the eS-WiFi module must be post-padded with
	 * 0x0A (Line Feed) to an even number of bytes.
	 * All data from eS-WiFi module are post-padded with 0x15(NAK) to an
	 * even number of bytes.
	 */

	if (!cmd) {
		goto data;
	}

	/* CMD/DATA READY signals the Command Phase */
	err = eswifi_spi_wait_cmddata_ready(spi);
	if (err) {
		LOG_ERR("CMD ready timeout\n");
		return err;
	}

	if (clen % 2) { /* Add post-padding if necessary */
		/* cmd is a string so cmd[clen] is 0x00 */
		cmd[clen] = 0x0a;
		clen++;
	}

	eswifi_spi_write(eswifi, cmd, clen);

	/* Our device is flagged with SPI_HOLD_ON_CS|SPI_LOCK_ON, release */
	spi_release(spi->spi_dev, &spi->spi_cfg);

data:
	/* CMD/DATA READY signals the Data Phase */
	err = eswifi_spi_wait_cmddata_ready(spi);
	if (err) {
		LOG_ERR("DATA ready timeout\n");
		return err;
	}

	eswifi_spi_read(eswifi, rsp, rlen);
	k_sleep(1);

	while (eswifi_spi_cmddata_ready(spi)) {
		eswifi_spi_read(eswifi, tmp, 2);
		k_sleep(1);
	}

	/* Our device is flagged with SPI_HOLD_ON_CS|SPI_LOCK_ON, release */
	spi_release(spi->spi_dev, &spi->spi_cfg);

	LOG_DBG("success");

	return 0;
}

static void eswifi_spi_read_msg(struct eswifi_dev *eswifi)
{
	char cmd[] = "MR\r";
	int err;

	eswifi_lock(eswifi);

	err = eswifi_request(eswifi, cmd, strlen(cmd),
			     eswifi->buf, sizeof(eswifi->buf));
	if (err || !eswifi_is_buf_at_ok(eswifi->buf)) {
		LOG_ERR("Unable to read msg %d", err);
	}

	eswifi_unlock(eswifi);
}

static void eswifi_spi_poll_thread(void *p1)
{
	struct eswifi_dev *eswifi = p1;

	while (1) {
		k_sleep(K_MSEC(1000));
		eswifi_spi_read_msg(eswifi);
	}
}

int eswifi_spi_init(struct eswifi_dev *eswifi)
{
	struct eswifi_spi_data *spi = &eswifi_spi0; /* Static instance */

	/* SPI DEV */
	spi->spi_dev = device_get_binding("SPI_3");
	if (!spi->spi_dev) {
		LOG_ERR("Failed to initialize SPI driver");
		return -ENODEV;
	}

	/* SPI DATA READY PIN */
	spi->dr.dev = device_get_binding(ESWIFI0_DATA_GPIOS_CONTROLLER);
	if (!spi->dr.dev) {
		LOG_ERR("Failed to initialize GPIO driver: %s",
			    ESWIFI0_DATA_GPIOS_CONTROLLER);
		return -ENODEV;
	}
	spi->dr.pin = ESWIFI0_DATA_GPIOS_PIN;
	gpio_pin_configure(spi->dr.dev, spi->dr.pin, GPIO_DIR_IN);


	/* SPI CONFIG/CS */
	spi->spi_cfg.frequency = ESWIFI0_SPI_MAX_FREQUENCY;
	spi->spi_cfg.operation = (SPI_OP_MODE_MASTER | SPI_TRANSFER_MSB |
				  SPI_WORD_SET(16) | SPI_LINES_SINGLE |
				  SPI_HOLD_ON_CS | SPI_LOCK_ON);
	spi->spi_cfg.slave = ESWIFI0_BASE_ADDRESS;
	spi->spi_cs.gpio_dev = device_get_binding(DT_ESWIFI0_CS_GPIOS_CONTROLLER);
	spi->spi_cs.gpio_pin = DT_ESWIFI0_CS_GPIOS_PIN;
	spi->spi_cs.delay = 1000;
	spi->spi_cfg.cs = &spi->spi_cs;

	eswifi->bus_data = spi;

	LOG_DBG("success");

	k_thread_create(&spi->poll_thread, eswifi_spi_poll_stack,
			ESWIFI_SPI_THREAD_STACK_SIZE,
			(k_thread_entry_t)eswifi_spi_poll_thread, eswifi, NULL,
			NULL, K_PRIO_COOP(CONFIG_WIFI_ESWIFI_THREAD_PRIO), 0,
			K_NO_WAIT);

	return 0;
}

struct eswifi_bus_ops eswifi_bus_ops_spi = {
	.init = eswifi_spi_init,
	.request = eswifi_spi_request,
};
