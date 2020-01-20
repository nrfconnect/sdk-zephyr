/*
 * Copyright (c) 2019 Thomas Schmid <tom@lfence.de>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <drivers/spi.h>
#include <sys/byteorder.h>
#include "ms5607.h"

#define LOG_LEVEL CONFIG_SENSOR_LOG_LEVEL
#include <logging/log.h>
LOG_MODULE_DECLARE(ms5607);

#ifdef DT_MEAS_MS5607_BUS_SPI

#if defined(DT_INST_0_MEAS_MS5607_CS_GPIOS_CONTROLLER)
static struct spi_cs_control ms5607_cs_ctrl;
#endif

#define SPI_CS NULL

static struct spi_config ms5607_spi_conf = {
	.frequency = DT_INST_0_MEAS_MS5607_SPI_MAX_FREQUENCY,
	.operation = (SPI_OP_MODE_MASTER | SPI_WORD_SET(8) |
		      SPI_MODE_CPOL | SPI_MODE_CPHA |
		      SPI_TRANSFER_MSB |
		      SPI_LINES_SINGLE),
	.slave = DT_INST_0_MEAS_MS5607_BASE_ADDRESS,
	.cs = SPI_CS,
};

static int ms5607_spi_raw_cmd(const struct ms5607_data *data, u8_t cmd)
{
	const struct spi_buf buf = {
		.buf = &cmd,
		.len = 1,
	};

	const struct spi_buf_set buf_set = {
		.buffers = &buf,
		.count = 1,
	};

	return spi_write(data->ms5607_device, &ms5607_spi_conf, &buf_set);
}

static int ms5607_spi_reset(const struct ms5607_data *data)
{
	int err = ms5607_spi_raw_cmd(data, MS5607_CMD_RESET);

	if (err < 0) {
		return err;
	}

	k_sleep(K_MSEC(3));
	return 0;
}

static int ms5607_spi_read_prom(const struct ms5607_data *data, u8_t cmd,
				u16_t *val)
{
	int err;

	u8_t tx[3] = { cmd, 0, 0 };
	const struct spi_buf tx_buf = {
		.buf = tx,
		.len = 3,
	};

	union {
		struct {
			u8_t pad;
			u16_t prom_value;
		} __packed;
		u8_t rx[3];
	} rx;


	const struct spi_buf rx_buf = {
		.buf = &rx,
		.len = 3,
	};

	const struct spi_buf_set rx_buf_set = {
		.buffers = &rx_buf,
		.count = 1,
	};

	const struct spi_buf_set tx_buf_set = {
		.buffers = &tx_buf,
		.count = 1,
	};

	err = spi_transceive(data->ms5607_device,
			     &ms5607_spi_conf,
			     &tx_buf_set,
			     &rx_buf_set);
	if (err < 0) {
		return err;
	}

	*val = sys_be16_to_cpu(rx.prom_value);

	return 0;
}


static int ms5607_spi_start_conversion(const struct ms5607_data *data, u8_t cmd)
{
	return ms5607_spi_raw_cmd(data, cmd);
}

static int ms5607_spi_read_adc(const struct ms5607_data *data, u32_t *val)
{
	int err;

	u8_t tx[4] = { MS5607_CMD_CONV_READ_ADC, 0, 0, 0 };
	const struct spi_buf tx_buf = {
		.buf = tx,
		.len = 4,
	};

	union {
		struct {
			u32_t adc_value;
		} __packed;
		u8_t rx[4];
	} rx;

	const struct spi_buf rx_buf = {
		.buf = &rx,
		.len = 4,
	};

	const struct spi_buf_set rx_buf_set = {
		.buffers = &rx_buf,
		.count = 1,
	};

	const struct spi_buf_set tx_buf_set = {
		.buffers = &tx_buf,
		.count = 1,
	};

	err = spi_transceive(data->ms5607_device,
			     &ms5607_spi_conf,
			     &tx_buf_set,
			     &rx_buf_set);
	if (err < 0) {
		return err;
	}

	rx.rx[0] = 0;
	*val = sys_be32_to_cpu(rx.adc_value);
	return 0;
}

static const struct ms5607_transfer_function ms5607_spi_transfer_function = {
	.reset = ms5607_spi_reset,
	.read_prom = ms5607_spi_read_prom,
	.start_conversion = ms5607_spi_start_conversion,
	.read_adc = ms5607_spi_read_adc,
};

int ms5607_spi_init(struct device *dev)
{
	struct ms5607_data *data = dev->driver_data;

	data->tf = &ms5607_spi_transfer_function;

#if defined(DT_INST_0_MEAS_MS5607_CS_GPIOS_CONTROLLER)
	ms5607_cs_ctrl.gpio_dev = device_get_binding(
		DT_INST_0_MEAS_MS5607_CS_GPIOS_CONTROLLER);
	if (!ms5607_cs_ctrl.gpio_dev) {
		LOG_ERR("Unable to get GPIO SPI CS device");
		return -ENODEV;
	}

	ms5607_cs_ctrl.gpio_pin = DT_INST_0_MEAS_MS5607_CS_GPIOS_PIN;
	ms5607_cs_ctrl.delay = 0U;

	ms5607_spi_conf.cs = &ms5607_cs_ctrl;

	LOG_DBG("SPI GPIO CS configured on %s:%u",
		DT_INST_0_MEAS_MS5607_CS_GPIOS_CONTROLLER,
		DT_INST_0_MEAS_MS5607_CS_GPIOS_PIN);
#endif
	return 0;
}

#endif
