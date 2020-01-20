/*
 * Copyright (c) 2019 Christian Taedcke <hacking@taedcke.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_LEVEL CONFIG_SPI_LOG_LEVEL
#include <logging/log.h>
LOG_MODULE_REGISTER(spi_gecko);
#include "spi_context.h"

#include <sys/sys_io.h>
#include <device.h>
#include <drivers/spi.h>
#include <soc.h>

#include "em_cmu.h"
#include "em_usart.h"

#include <stdbool.h>

#ifndef CONFIG_SOC_GECKO_HAS_INDIVIDUAL_PIN_LOCATION
#error "Individual pin location support is required"
#endif

#define CLOCK_USART(id) _CONCAT(cmuClock_USART, id)

#define SPI_WORD_SIZE 8

#define DEV_DATA(dev) ((struct spi_gecko_data *) ((dev)->driver_data))

/* Structure Declarations */

struct spi_gecko_data {
	struct spi_context ctx;
};

struct spi_gecko_config {
	USART_TypeDef *base;
	CMU_Clock_TypeDef clock;
	struct soc_gpio_pin pin_rx;
	struct soc_gpio_pin pin_tx;
	struct soc_gpio_pin pin_clk;
	u8_t loc_rx;
	u8_t loc_tx;
	u8_t loc_clk;
};


/* Helper Functions */
static int spi_config(struct device *dev, const struct spi_config *config,
		      u16_t *control)
{
	const struct spi_gecko_config *gecko_config = dev->config->config_info;
	struct spi_gecko_data *data = DEV_DATA(dev);

	if (SPI_WORD_SIZE_GET(config->operation) != SPI_WORD_SIZE) {
		LOG_ERR("Word size must be %d", SPI_WORD_SIZE);
		return -ENOTSUP;
	}

	if (config->operation & SPI_CS_ACTIVE_HIGH) {
		LOG_ERR("CS active high not supported");
		return -ENOTSUP;
	}

	if (config->operation & SPI_LOCK_ON) {
		LOG_ERR("Lock On not supported");
		return -ENOTSUP;
	}

	if ((config->operation & SPI_LINES_MASK) != SPI_LINES_SINGLE) {
		LOG_ERR("Only supports single mode");
		return -ENOTSUP;
	}

	if (config->operation & SPI_TRANSFER_LSB) {
		LOG_ERR("LSB first not supported");
		return -ENOTSUP;
	}

	if (config->operation & (SPI_MODE_CPOL | SPI_MODE_CPHA)) {
		LOG_ERR("Only supports CPOL=CPHA=0");
		return -ENOTSUP;
	}

	if (config->operation & SPI_OP_MODE_SLAVE) {
		LOG_ERR("Slave mode not supported");
		return -ENOTSUP;
	}

	/* Set Loopback */
	if (config->operation & SPI_MODE_LOOP) {
		gecko_config->base->CTRL |= USART_CTRL_LOOPBK;
	} else {
		gecko_config->base->CTRL &= ~USART_CTRL_LOOPBK;
	}

	/* Set word size */
	gecko_config->base->FRAME = usartDatabits8
	    | USART_FRAME_STOPBITS_DEFAULT
	    | USART_FRAME_PARITY_DEFAULT;

	/* At this point, it's mandatory to set this on the context! */
	data->ctx.config = config;

	spi_context_cs_configure(&data->ctx);

	return 0;
}

static void spi_gecko_send(USART_TypeDef *usart, u8_t frame)
{
	/* Write frame to register */
	USART_Tx(usart, frame);

	/* Wait until the transfer ends */
	while (!(usart->STATUS & USART_STATUS_TXC)) {
	}
}

static u8_t spi_gecko_recv(USART_TypeDef *usart)
{
	/* Return data inside rx register */
	return (u8_t)usart->RXDATA;
}

static bool spi_gecko_transfer_ongoing(struct spi_gecko_data *data)
{
	return spi_context_tx_on(&data->ctx) || spi_context_rx_on(&data->ctx);
}

static inline u8_t spi_gecko_next_tx(struct spi_gecko_data *data)
{
	u8_t tx_frame = 0;

	if (spi_context_tx_buf_on(&data->ctx)) {
		tx_frame = UNALIGNED_GET((u8_t *)(data->ctx.tx_buf));
	}

	return tx_frame;
}

static int spi_gecko_shift_frames(USART_TypeDef *usart,
				  struct spi_gecko_data *data)
{
	u8_t tx_frame;
	u8_t rx_frame;

	tx_frame = spi_gecko_next_tx(data);
	spi_gecko_send(usart, tx_frame);
	spi_context_update_tx(&data->ctx, 1, 1);

	rx_frame = spi_gecko_recv(usart);

	if (spi_context_rx_buf_on(&data->ctx)) {
		UNALIGNED_PUT(rx_frame, (u8_t *)data->ctx.rx_buf);
	}
	spi_context_update_rx(&data->ctx, 1, 1);
	return 0;
}


static void spi_gecko_xfer(struct device *dev,
		const struct spi_config *config)
{
	int ret;
	struct spi_context *ctx = &DEV_DATA(dev)->ctx;
	const struct spi_gecko_config *gecko_config = dev->config->config_info;
	struct spi_gecko_data *data = DEV_DATA(dev);

	spi_context_cs_control(ctx, true);

	do {
		ret = spi_gecko_shift_frames(gecko_config->base, data);
	} while (!ret && spi_gecko_transfer_ongoing(data));

	spi_context_cs_control(ctx, false);
	spi_context_complete(ctx, 0);
}

static void spi_gecko_init_pins(struct device *dev)
{
	const struct spi_gecko_config *config = dev->config->config_info;

	soc_gpio_configure(&config->pin_rx);
	soc_gpio_configure(&config->pin_tx);
	soc_gpio_configure(&config->pin_clk);

	/* disable all pins while configuring */
	config->base->ROUTEPEN = 0;

	config->base->ROUTELOC0 =
		(config->loc_tx << _USART_ROUTELOC0_TXLOC_SHIFT) |
		(config->loc_rx << _USART_ROUTELOC0_RXLOC_SHIFT) |
		(config->loc_clk << _USART_ROUTELOC0_CLKLOC_SHIFT);

	config->base->ROUTELOC1 = _USART_ROUTELOC1_RESETVALUE;

	config->base->ROUTEPEN = USART_ROUTEPEN_RXPEN | USART_ROUTEPEN_TXPEN |
		USART_ROUTEPEN_CLKPEN;
}


/* API Functions */

static int spi_gecko_init(struct device *dev)
{
	const struct spi_gecko_config *config = dev->config->config_info;
	USART_InitSync_TypeDef usartInit = USART_INITSYNC_DEFAULT;

	/* The peripheral and gpio clock are already enabled from soc and gpio
	 * driver
	 */

	usartInit.enable = usartDisable;
	usartInit.baudrate = 1000000;
	usartInit.databits = usartDatabits8;
	usartInit.master = 1;
	usartInit.msbf = 1;
	usartInit.clockMode = usartClockMode0;
#if defined(USART_INPUT_RXPRS) && defined(USART_TRIGCTRL_AUTOTXTEN)
	usartInit.prsRxEnable = 0;
	usartInit.prsRxCh = 0;
	usartInit.autoTx = 0;
#endif

	/* Enable USART clock */
	CMU_ClockEnable(config->clock, true);

	/* Init USART */
	USART_InitSync(config->base, &usartInit);

	/* Initialize USART pins */
	spi_gecko_init_pins(dev);

	/* Enable the peripheral */
	config->base->CMD = (uint32_t) usartEnable;

	return 0;
}

static int spi_gecko_transceive(struct device *dev,
			  const struct spi_config *config,
			  const struct spi_buf_set *tx_bufs,
			  const struct spi_buf_set *rx_bufs)
{
	u16_t control = 0;

	spi_config(dev, config, &control);
	spi_context_buffers_setup(&DEV_DATA(dev)->ctx, tx_bufs, rx_bufs, 1);
	spi_gecko_xfer(dev, config);
	return 0;
}

#ifdef CONFIG_SPI_ASYNC
static int spi_gecko_transceive_async(struct device *dev,
			  const struct spi_config *config,
			  const struct spi_buf_set *tx_bufs,
			  const struct spi_buf_set *rx_bufs,
			  struct k_poll_signal *async)
{
	return -ENOTSUP;
}
#endif /* CONFIG_SPI_ASYNC */

static int spi_gecko_release(struct device *dev,
			     const struct spi_config *config)
{
	const struct spi_gecko_config *gecko_config = dev->config->config_info;

	if (!(gecko_config->base->STATUS & USART_STATUS_TXIDLE)) {
		return -EBUSY;
	}
	return 0;
}

/* Device Instantiation */
static struct spi_driver_api spi_gecko_api = {
	.transceive = spi_gecko_transceive,
#ifdef CONFIG_SPI_ASYNC
	.transceive_async = spi_gecko_transceive_async,
#endif /* CONFIG_SPI_ASYNC */
	.release = spi_gecko_release,
};

#define SPI_INIT2(n, usart)				    \
	static struct spi_gecko_data spi_gecko_data_##n = { \
		SPI_CONTEXT_INIT_LOCK(spi_gecko_data_##n, ctx), \
		SPI_CONTEXT_INIT_SYNC(spi_gecko_data_##n, ctx), \
	}; \
	static struct spi_gecko_config spi_gecko_cfg_##n = { \
	    .base = (USART_TypeDef *) \
		 DT_INST_##n##_SILABS_GECKO_SPI_USART_BASE_ADDRESS, \
	    .clock = CLOCK_USART(usart), \
	    .pin_rx = { DT_INST_##n##_SILABS_GECKO_SPI_USART_LOCATION_RX_1, \
			DT_INST_##n##_SILABS_GECKO_SPI_USART_LOCATION_RX_2, \
			gpioModeInput, 1},				\
	    .pin_tx = { DT_INST_##n##_SILABS_GECKO_SPI_USART_LOCATION_TX_1, \
			DT_INST_##n##_SILABS_GECKO_SPI_USART_LOCATION_TX_2, \
			gpioModePushPull, 1},				\
	    .pin_clk = { DT_INST_##n##_SILABS_GECKO_SPI_USART_LOCATION_CLK_1, \
			DT_INST_##n##_SILABS_GECKO_SPI_USART_LOCATION_CLK_2, \
			gpioModePushPull, 1},				\
	    .loc_rx = DT_INST_##n##_SILABS_GECKO_SPI_USART_LOCATION_RX_0, \
	    .loc_tx = DT_INST_##n##_SILABS_GECKO_SPI_USART_LOCATION_TX_0, \
	    .loc_clk = DT_INST_##n##_SILABS_GECKO_SPI_USART_LOCATION_CLK_0, \
	}; \
	DEVICE_AND_API_INIT(spi_##n, \
			DT_INST_##n##_SILABS_GECKO_SPI_USART_LABEL, \
			spi_gecko_init, \
			&spi_gecko_data_##n, \
			&spi_gecko_cfg_##n, \
			POST_KERNEL, \
			CONFIG_SPI_INIT_PRIORITY, \
			&spi_gecko_api)

#define SPI_ID(n) DT_INST_##n##_SILABS_GECKO_SPI_USART_PERIPHERAL_ID

#define SPI_INIT(n) SPI_INIT2(n, SPI_ID(n))

#ifdef DT_INST_0_SILABS_GECKO_SPI_USART_LABEL

SPI_INIT(0);

#endif /* DT_INST_0_SILABS_GECKO_SPI_USART_LABEL */

#ifdef DT_INST_1_SILABS_GECKO_SPI_USART_LABEL

SPI_INIT(1);

#endif /* DT_INST_1_SILABS_GECKO_SPI_USART_LABEL */

#ifdef DT_INST_2_SILABS_GECKO_SPI_USART_LABEL

SPI_INIT(2);

#endif /* DT_INST_2_SILABS_GECKO_SPI_USART_LABEL */

#ifdef DT_INST_3_SILABS_GECKO_SPI_USART_LABEL

SPI_INIT(3);

#endif /* DT_INST_3_SILABS_GECKO_SPI_USART_LABEL */
