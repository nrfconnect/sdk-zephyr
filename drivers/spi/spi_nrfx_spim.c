/*
 * Copyright (c) 2017 - 2018, Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <drivers/spi.h>
#include <pm/device.h>
#ifdef CONFIG_SOC_NRF52832_ALLOW_SPIM_DESPITE_PAN_58
#include <nrfx_gpiote.h>
#include <nrfx_ppi.h>
#endif
#include <nrfx_spim.h>
#include <hal/nrf_clock.h>
#include <string.h>

#define LOG_DOMAIN "spi_nrfx_spim"
#define LOG_LEVEL CONFIG_SPI_LOG_LEVEL
#include <logging/log.h>
LOG_MODULE_REGISTER(spi_nrfx_spim);

#include "spi_context.h"

struct spi_nrfx_data {
	struct spi_context ctx;
	const struct device *dev;
	size_t  chunk_len;
	bool    busy;
	bool    initialized;
#if (CONFIG_SPI_NRFX_RAM_BUFFER_SIZE > 0)
	uint8_t buffer[CONFIG_SPI_NRFX_RAM_BUFFER_SIZE];
#endif
#ifdef CONFIG_SOC_NRF52832_ALLOW_SPIM_DESPITE_PAN_58
	bool    anomaly_58_workaround_active;
	uint8_t ppi_ch;
	uint8_t gpiote_ch;
#endif
};

struct spi_nrfx_config {
	nrfx_spim_t	   spim;
	size_t		   max_chunk_len;
	uint32_t	   max_freq;
	nrfx_spim_config_t def_config;
#ifdef CONFIG_SOC_NRF52832_ALLOW_SPIM_DESPITE_PAN_58
	bool		   anomaly_58_workaround;
#endif
};

static void event_handler(const nrfx_spim_evt_t *p_event, void *p_context);

static inline struct spi_nrfx_data *get_dev_data(const struct device *dev)
{
	return dev->data;
}

static inline const struct spi_nrfx_config *get_dev_config(const struct device *dev)
{
	return dev->config;
}

static inline nrf_spim_frequency_t get_nrf_spim_frequency(uint32_t frequency)
{
	/* Get the highest supported frequency not exceeding the requested one.
	 */
	if (frequency < 250000) {
		return NRF_SPIM_FREQ_125K;
	} else if (frequency < 500000) {
		return NRF_SPIM_FREQ_250K;
	} else if (frequency < 1000000) {
		return NRF_SPIM_FREQ_500K;
	} else if (frequency < 2000000) {
		return NRF_SPIM_FREQ_1M;
	} else if (frequency < 4000000) {
		return NRF_SPIM_FREQ_2M;
	} else if (frequency < 8000000) {
		return NRF_SPIM_FREQ_4M;
/* Only the devices with HS-SPI can use SPI clock higher than 8 MHz and
 * have SPIM_FREQUENCY_FREQUENCY_M32 defined in their own bitfields.h
 */
#if defined(SPIM_FREQUENCY_FREQUENCY_M32)
	} else if (frequency < 16000000) {
		return NRF_SPIM_FREQ_8M;
	} else if (frequency < 32000000) {
		return NRF_SPIM_FREQ_16M;
	} else {
		return NRF_SPIM_FREQ_32M;
#else
	} else {
		return NRF_SPIM_FREQ_8M;
#endif
	}
}

static inline nrf_spim_mode_t get_nrf_spim_mode(uint16_t operation)
{
	if (SPI_MODE_GET(operation) & SPI_MODE_CPOL) {
		if (SPI_MODE_GET(operation) & SPI_MODE_CPHA) {
			return NRF_SPIM_MODE_3;
		} else {
			return NRF_SPIM_MODE_2;
		}
	} else {
		if (SPI_MODE_GET(operation) & SPI_MODE_CPHA) {
			return NRF_SPIM_MODE_1;
		} else {
			return NRF_SPIM_MODE_0;
		}
	}
}

static inline nrf_spim_bit_order_t get_nrf_spim_bit_order(uint16_t operation)
{
	if (operation & SPI_TRANSFER_LSB) {
		return NRF_SPIM_BIT_ORDER_LSB_FIRST;
	} else {
		return NRF_SPIM_BIT_ORDER_MSB_FIRST;
	}
}

static int configure(const struct device *dev,
		     const struct spi_config *spi_cfg)
{
	struct spi_nrfx_data *dev_data = get_dev_data(dev);
	const struct spi_nrfx_config *dev_config = get_dev_config(dev);
	struct spi_context *ctx = &dev_data->ctx;
	uint32_t max_freq = dev_config->max_freq;
	nrfx_spim_config_t config;
	nrfx_err_t result;

	if (dev_data->initialized && spi_context_configured(ctx, spi_cfg)) {
		/* Already configured. No need to do it again. */
		return 0;
	}

	if (spi_cfg->operation & SPI_HALF_DUPLEX) {
		LOG_ERR("Half-duplex not supported");
		return -ENOTSUP;
	}

	if (SPI_OP_MODE_GET(spi_cfg->operation) != SPI_OP_MODE_MASTER) {
		LOG_ERR("Slave mode is not supported on %s", dev->name);
		return -EINVAL;
	}

	if (spi_cfg->operation & SPI_MODE_LOOP) {
		LOG_ERR("Loopback mode is not supported");
		return -EINVAL;
	}

	if (IS_ENABLED(CONFIG_SPI_EXTENDED_MODES) &&
	    (spi_cfg->operation & SPI_LINES_MASK) != SPI_LINES_SINGLE) {
		LOG_ERR("Only single line mode is supported");
		return -EINVAL;
	}

	if (SPI_WORD_SIZE_GET(spi_cfg->operation) != 8) {
		LOG_ERR("Word sizes other than 8 bits are not supported");
		return -EINVAL;
	}

	if (spi_cfg->frequency < 125000) {
		LOG_ERR("Frequencies lower than 125 kHz are not supported");
		return -EINVAL;
	}

#if defined(CONFIG_SOC_NRF5340_CPUAPP)
	/* On nRF5340, the 32 Mbps speed is supported by the application core
	 * when it is running at 128 MHz (see the Timing specifications section
	 * in the nRF5340 PS).
	 */
	if (max_freq > 16000000 &&
	    nrf_clock_hfclk_div_get(NRF_CLOCK) != NRF_CLOCK_HFCLK_DIV_1) {
		max_freq = 16000000;
	}
#endif

	config = dev_config->def_config;

	/* Limit the frequency to that supported by the SPIM instance. */
	config.frequency = get_nrf_spim_frequency(MIN(spi_cfg->frequency,
						      max_freq));
	config.mode      = get_nrf_spim_mode(spi_cfg->operation);
	config.bit_order = get_nrf_spim_bit_order(spi_cfg->operation);

	if (dev_data->initialized) {
		nrfx_spim_uninit(&dev_config->spim);
		dev_data->initialized = false;
	}

	result = nrfx_spim_init(&dev_config->spim, &config,
				event_handler, dev_data);
	if (result != NRFX_SUCCESS) {
		LOG_ERR("Failed to initialize nrfx driver: %08x", result);
		return -EIO;
	}

	dev_data->initialized = true;

	ctx->config = spi_cfg;

	return 0;
}

#ifdef CONFIG_SOC_NRF52832_ALLOW_SPIM_DESPITE_PAN_58
/*
 * Brief Workaround for transmitting 1 byte with SPIM.
 *
 * Derived from the setup_workaround_for_ftpan_58() function from
 * the nRF52832 Rev 1 Errata v1.6 document anomaly 58 workaround.
 *
 * Warning Must not be used when transmitting multiple bytes.
 *
 * Warning After this workaround is used, the user must reset the PPI
 * channel and the GPIOTE channel before attempting to transmit multiple
 * bytes.
 */
static void anomaly_58_workaround_setup(const struct device *dev)
{
	struct spi_nrfx_data *dev_data = get_dev_data(dev);
	const struct spi_nrfx_config *dev_config = get_dev_config(dev);
	NRF_SPIM_Type *spim = dev_config->spim.p_reg;
	uint32_t ppi_ch = dev_data->ppi_ch;
	uint32_t gpiote_ch = dev_data->gpiote_ch;
	uint32_t eep = (uint32_t)&NRF_GPIOTE->EVENTS_IN[gpiote_ch];
	uint32_t tep = (uint32_t)&spim->TASKS_STOP;

	dev_data->anomaly_58_workaround_active = true;

	/* Create an event when SCK toggles */
	nrf_gpiote_event_configure(NRF_GPIOTE, gpiote_ch, spim->PSEL.SCK,
				   GPIOTE_CONFIG_POLARITY_Toggle);
	nrf_gpiote_event_enable(NRF_GPIOTE, gpiote_ch);

	/* Stop the spim instance when SCK toggles */
	nrf_ppi_channel_endpoint_setup(NRF_PPI, ppi_ch, eep, tep);
	nrf_ppi_channel_enable(NRF_PPI, ppi_ch);

	/* The spim instance cannot be stopped mid-byte, so it will finish
	 * transmitting the first byte and then stop. Effectively ensuring
	 * that only 1 byte is transmitted.
	 */
}

static void anomaly_58_workaround_clear(struct spi_nrfx_data *dev_data)
{
	uint32_t ppi_ch = dev_data->ppi_ch;
	uint32_t gpiote_ch = dev_data->gpiote_ch;

	if (dev_data->anomaly_58_workaround_active) {
		nrf_ppi_channel_disable(NRF_PPI, ppi_ch);
		nrf_gpiote_task_disable(NRF_GPIOTE, gpiote_ch);

		dev_data->anomaly_58_workaround_active = false;
	}
}

static int anomaly_58_workaround_init(const struct device *dev)
{
	struct spi_nrfx_data *data = get_dev_data(dev);
	const struct spi_nrfx_config *config = get_dev_config(dev);
	nrfx_err_t err_code;

	data->anomaly_58_workaround_active = false;

	if (config->anomaly_58_workaround) {
		err_code = nrfx_ppi_channel_alloc(&data->ppi_ch);
		if (err_code != NRFX_SUCCESS) {
			LOG_ERR("Failed to allocate PPI channel");
			return -ENODEV;
		}

		err_code = nrfx_gpiote_channel_alloc(&data->gpiote_ch);
		if (err_code != NRFX_SUCCESS) {
			LOG_ERR("Failed to allocate GPIOTE channel");
			return -ENODEV;
		}
		LOG_DBG("PAN 58 workaround enabled for %s: ppi %u, gpiote %u",
			dev->name, data->ppi_ch, data->gpiote_ch);
	}

	return 0;
}
#endif

static void transfer_next_chunk(const struct device *dev)
{
	struct spi_nrfx_data *dev_data = get_dev_data(dev);
	const struct spi_nrfx_config *dev_config = get_dev_config(dev);
	struct spi_context *ctx = &dev_data->ctx;
	int error = 0;

	size_t chunk_len = spi_context_max_continuous_chunk(ctx);

	if (chunk_len > 0) {
		nrfx_spim_xfer_desc_t xfer;
		nrfx_err_t result;
		const uint8_t *tx_buf = ctx->tx_buf;
#if (CONFIG_SPI_NRFX_RAM_BUFFER_SIZE > 0)
		if (spi_context_tx_buf_on(ctx) && !nrfx_is_in_ram(tx_buf)) {
			if (chunk_len > sizeof(dev_data->buffer)) {
				chunk_len = sizeof(dev_data->buffer);
			}

			memcpy(dev_data->buffer, tx_buf, chunk_len);
			tx_buf = dev_data->buffer;
		}
#endif
		if (chunk_len > dev_config->max_chunk_len) {
			chunk_len = dev_config->max_chunk_len;
		}

		dev_data->chunk_len = chunk_len;

		xfer.p_tx_buffer = tx_buf;
		xfer.tx_length   = spi_context_tx_buf_on(ctx) ? chunk_len : 0;
		xfer.p_rx_buffer = ctx->rx_buf;
		xfer.rx_length   = spi_context_rx_buf_on(ctx) ? chunk_len : 0;

#ifdef CONFIG_SOC_NRF52832_ALLOW_SPIM_DESPITE_PAN_58
		if (xfer.rx_length == 1 && xfer.tx_length <= 1) {
			if (dev_config->anomaly_58_workaround) {
				anomaly_58_workaround_setup(dev);
			} else {
				LOG_WRN("Transaction aborted since it would trigger "
					"nRF52832 PAN 58");
				error = -EIO;
			}
		}
#endif
		if (error == 0) {
			result = nrfx_spim_xfer(&dev_config->spim, &xfer, 0);
			if (result == NRFX_SUCCESS) {
				return;
			}
			error = -EIO;
#ifdef CONFIG_SOC_NRF52832_ALLOW_SPIM_DESPITE_PAN_58
			anomaly_58_workaround_clear(dev_data);
#endif
		}
	}

	spi_context_cs_control(ctx, false);

	LOG_DBG("Transaction finished with status %d", error);

	spi_context_complete(ctx, error);
	dev_data->busy = false;
}

static void event_handler(const nrfx_spim_evt_t *p_event, void *p_context)
{
	struct spi_nrfx_data *dev_data = p_context;

	if (p_event->type == NRFX_SPIM_EVENT_DONE) {
#ifdef CONFIG_SOC_NRF52832_ALLOW_SPIM_DESPITE_PAN_58
		anomaly_58_workaround_clear(dev_data);
#endif
		spi_context_update_tx(&dev_data->ctx, 1, dev_data->chunk_len);
		spi_context_update_rx(&dev_data->ctx, 1, dev_data->chunk_len);

		transfer_next_chunk(dev_data->dev);
	}
}

static int transceive(const struct device *dev,
		      const struct spi_config *spi_cfg,
		      const struct spi_buf_set *tx_bufs,
		      const struct spi_buf_set *rx_bufs,
		      bool asynchronous,
		      struct k_poll_signal *signal)
{
	struct spi_nrfx_data *dev_data = get_dev_data(dev);
	int error;

	spi_context_lock(&dev_data->ctx, asynchronous, signal, spi_cfg);

	error = configure(dev, spi_cfg);
	if (error == 0) {
		dev_data->busy = true;

		spi_context_buffers_setup(&dev_data->ctx, tx_bufs, rx_bufs, 1);
		spi_context_cs_control(&dev_data->ctx, true);

		transfer_next_chunk(dev);

		error = spi_context_wait_for_completion(&dev_data->ctx);
	}

	spi_context_release(&dev_data->ctx, error);

	return error;
}

static int spi_nrfx_transceive(const struct device *dev,
			       const struct spi_config *spi_cfg,
			       const struct spi_buf_set *tx_bufs,
			       const struct spi_buf_set *rx_bufs)
{
	return transceive(dev, spi_cfg, tx_bufs, rx_bufs, false, NULL);
}

#ifdef CONFIG_SPI_ASYNC
static int spi_nrfx_transceive_async(const struct device *dev,
				     const struct spi_config *spi_cfg,
				     const struct spi_buf_set *tx_bufs,
				     const struct spi_buf_set *rx_bufs,
				     struct k_poll_signal *async)
{
	return transceive(dev, spi_cfg, tx_bufs, rx_bufs, true, async);
}
#endif /* CONFIG_SPI_ASYNC */

static int spi_nrfx_release(const struct device *dev,
			    const struct spi_config *spi_cfg)
{
	struct spi_nrfx_data *dev_data = get_dev_data(dev);

	if (!spi_context_configured(&dev_data->ctx, spi_cfg)) {
		return -EINVAL;
	}

	if (dev_data->busy) {
		return -EBUSY;
	}

	spi_context_unlock_unconditionally(&dev_data->ctx);

	return 0;
}

static const struct spi_driver_api spi_nrfx_driver_api = {
	.transceive = spi_nrfx_transceive,
#ifdef CONFIG_SPI_ASYNC
	.transceive_async = spi_nrfx_transceive_async,
#endif
	.release = spi_nrfx_release,
};

#ifdef CONFIG_PM_DEVICE
static int spim_nrfx_pm_action(const struct device *dev,
			       enum pm_device_action action)
{
	int ret = 0;
	struct spi_nrfx_data *data = get_dev_data(dev);
	const struct spi_nrfx_config *config = get_dev_config(dev);

	switch (action) {
	case PM_DEVICE_ACTION_RESUME:
		/* No action needed at this point, nrfx_spim_init() will be
		 * called at configuration before the next transfer.
		 */
		break;

	case PM_DEVICE_ACTION_SUSPEND:
		if (data->initialized) {
			nrfx_spim_uninit(&config->spim);
			data->initialized = false;
		}
		break;

	default:
		ret = -ENOTSUP;
	}

	return ret;
}
#endif /* CONFIG_PM_DEVICE */

/*
 * We use NODELABEL here because the nrfx API requires us to call
 * functions which are named according to SoC peripheral instance
 * being operated on. Since DT_INST() makes no guarantees about that,
 * it won't work.
 */
#define SPIM(idx) DT_NODELABEL(spi##idx)
#define SPIM_PROP(idx, prop) DT_PROP(SPIM(idx), prop)

#define SPIM_NRFX_MISO_PULL_DOWN(idx) DT_PROP(SPIM(idx), miso_pull_down)
#define SPIM_NRFX_MISO_PULL_UP(idx) DT_PROP(SPIM(idx), miso_pull_up)

#define SPIM_NRFX_MISO_PULL(idx)			\
	(SPIM_NRFX_MISO_PULL_UP(idx)			\
		? SPIM_NRFX_MISO_PULL_DOWN(idx)		\
			? -1 /* invalid configuration */\
			: NRF_GPIO_PIN_PULLUP		\
		: SPIM_NRFX_MISO_PULL_DOWN(idx)		\
			? NRF_GPIO_PIN_PULLDOWN		\
			: NRF_GPIO_PIN_NOPULL)

#define SPI_NRFX_SPIM_EXTENDED_CONFIG(idx)				\
	IF_ENABLED(NRFX_SPIM_EXTENDED_ENABLED,				\
		(.dcx_pin = NRFX_SPIM_PIN_NOT_USED,			\
		 IF_ENABLED(SPIM##idx##_FEATURE_RXDELAY_PRESENT,	\
			(.rx_delay = CONFIG_SPI_##idx##_NRF_RX_DELAY,))	\
		))

#define SPI_NRFX_SPIM_DEVICE(idx)					       \
	BUILD_ASSERT(							       \
		!SPIM_NRFX_MISO_PULL_UP(idx) || !SPIM_NRFX_MISO_PULL_DOWN(idx),\
		"SPIM"#idx						       \
		": cannot enable both pull-up and pull-down on MISO line");    \
	static int spi_##idx##_init(const struct device *dev)		       \
	{								       \
		int err;                                                       \
		IRQ_CONNECT(NRFX_IRQ_NUMBER_GET(NRF_SPIM##idx),		       \
			    DT_IRQ(SPIM(idx), priority),		       \
			    nrfx_isr, nrfx_spim_##idx##_irq_handler, 0);       \
		err = spi_context_cs_configure_all(&get_dev_data(dev)->ctx);   \
		if (err < 0) {                                                 \
			return err;                                            \
		}                                                              \
		spi_context_unlock_unconditionally(&get_dev_data(dev)->ctx);   \
		COND_CODE_1(CONFIG_SOC_NRF52832_ALLOW_SPIM_DESPITE_PAN_58,     \
		(return anomaly_58_workaround_init(dev);),		       \
		(return 0;))						       \
	}								       \
	static struct spi_nrfx_data spi_##idx##_data = {		       \
		SPI_CONTEXT_INIT_LOCK(spi_##idx##_data, ctx),		       \
		SPI_CONTEXT_INIT_SYNC(spi_##idx##_data, ctx),		       \
		SPI_CONTEXT_CS_GPIOS_INITIALIZE(SPIM(idx), ctx)		       \
		.dev  = DEVICE_DT_GET(SPIM(idx)),			       \
		.busy = false,						       \
	};								       \
	static const struct spi_nrfx_config spi_##idx##z_config = {	       \
		.spim = NRFX_SPIM_INSTANCE(idx),			       \
		.max_chunk_len = (1 << SPIM##idx##_EASYDMA_MAXCNT_SIZE) - 1,   \
		.max_freq = SPIM##idx##_MAX_DATARATE * 1000000,		       \
		.def_config = {						       \
			.sck_pin   = SPIM_PROP(idx, sck_pin),		       \
			.mosi_pin  = SPIM_PROP(idx, mosi_pin),		       \
			.miso_pin  = SPIM_PROP(idx, miso_pin),		       \
			.ss_pin    = NRFX_SPIM_PIN_NOT_USED,		       \
			.orc       = CONFIG_SPI_##idx##_NRF_ORC,	       \
			.miso_pull = SPIM_NRFX_MISO_PULL(idx),		       \
			SPI_NRFX_SPIM_EXTENDED_CONFIG(idx)		       \
		},							       \
		COND_CODE_1(CONFIG_SOC_NRF52832_ALLOW_SPIM_DESPITE_PAN_58,     \
			(.anomaly_58_workaround =			       \
				SPIM_PROP(idx, anomaly_58_workaround),),       \
			())						       \
	};								       \
	PM_DEVICE_DT_DEFINE(SPIM(idx), spim_nrfx_pm_action);		       \
	DEVICE_DT_DEFINE(SPIM(idx),					       \
		      spi_##idx##_init,					       \
		      PM_DEVICE_DT_REF(SPIM(idx)),			       \
		      &spi_##idx##_data,				       \
		      &spi_##idx##z_config,				       \
		      POST_KERNEL, CONFIG_SPI_INIT_PRIORITY,		       \
		      &spi_nrfx_driver_api)

#ifdef CONFIG_SPI_0_NRF_SPIM
SPI_NRFX_SPIM_DEVICE(0);
#endif

#ifdef CONFIG_SPI_1_NRF_SPIM
SPI_NRFX_SPIM_DEVICE(1);
#endif

#ifdef CONFIG_SPI_2_NRF_SPIM
SPI_NRFX_SPIM_DEVICE(2);
#endif

#ifdef CONFIG_SPI_3_NRF_SPIM
SPI_NRFX_SPIM_DEVICE(3);
#endif

#ifdef CONFIG_SPI_4_NRF_SPIM
SPI_NRFX_SPIM_DEVICE(4);
#endif
