/*
 * Copyright (c) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/** @file
 * @brief I2S bus (SSP) driver for Intel CAVS.
 *
 * Limitations:
 * - DMA is used in simple single block transfer mode (with linked list
 *   enabled) and "interrupt on full transfer completion" mode.
 */

#include <errno.h>
#include <string.h>
#include <misc/__assert.h>
#include <kernel.h>
#include <device.h>
#include <init.h>
#include <dma.h>
#include <i2s.h>
#include <soc.h>
#include "i2s_cavs.h"

#define LOG_DOMAIN dev_i2s_cavs
#define LOG_LEVEL CONFIG_I2S_LOG_LEVEL
#include <logging/log.h>
LOG_MODULE_REGISTER(LOG_DOMAIN);

#define I2S_IRQ_CONNECT(i2s_id)					\
	IRQ_CONNECT(I2S##i2s_id##_CAVS_IRQ,			\
			CONFIG_I2S_CAVS_IRQ_PRI,		\
			i2s_cavs_isr,				\
			DEVICE_GET(i2s##i2s_id##_cavs), 0)

#define I2S_DEVICE_NAME(i2s_id)		i2s##i2s_id##_cavs
#define I2S_DEVICE_DATA_NAME(i2s_id)	i2s##i2s_id##_cavs_data
#define I2S_DEVICE_CONFIG_NAME(i2s_id)	i2s##i2s_id##_cavs_config

#define I2S_DEVICE_CONFIG_DEFINE(i2s_id)				\
	static const struct i2s_cavs_config i2s##i2s_id##_cavs_config = {\
		.regs = (struct i2s_cavs_ssp *)SSP_BASE(i2s_id),	\
		.mn_regs = (struct i2s_cavs_mn_div *)SSP_MN_DIV_BASE(i2s_id),\
		.irq_id = I2S##i2s_id##_CAVS_IRQ,		\
		.irq_connect = i2s##i2s_id##_cavs_irq_connect,	\
	}

#define I2S_DMA_CHANNEL(i2s_id, dir)				\
		CONFIG_I2S_CAVS_##i2s_id##_DMA_##dir##_CHANNEL

#define I2S_DEVICE_OBJECT_DECLARE(i2s_id)			\
	DEVICE_DECLARE(I2S_DEVICE_NAME(i2s_id))

#define I2S_DEVICE_OBJECT(i2s_id)				\
		DEVICE_GET(I2S_DEVICE_NAME(i2s_id))

#define I2S_DEVICE_DATA_DEFINE(i2s_id)				\
	static struct i2s_cavs_dev_data i2s##i2s_id##_cavs_data = {\
		.tx = {						\
			.dma_channel = I2S_DMA_CHANNEL(i2s_id, TX),	\
			.dma_cfg = {				\
				.source_burst_length = CAVS_I2S_DMA_BURST_SIZE,\
				.dest_burst_length = CAVS_I2S_DMA_BURST_SIZE,\
				.dma_callback = i2s_dma_tx_callback,	\
				.callback_arg = I2S_DEVICE_OBJECT(i2s_id),\
				.complete_callback_en = 1,	\
				.error_callback_en = 1,		\
				.block_count = 1,		\
				.head_block =			\
				&i2s##i2s_id##_cavs_data.tx.dma_block,\
				.channel_direction = MEMORY_TO_PERIPHERAL,\
				.dma_slot = DMA_HANDSHAKE_SSP##i2s_id##_TX,\
			},					\
		},						\
		.rx = {						\
			.dma_channel = I2S_DMA_CHANNEL(i2s_id, RX),	\
			.dma_cfg = {				\
				.source_burst_length = CAVS_I2S_DMA_BURST_SIZE,\
				.dest_burst_length = CAVS_I2S_DMA_BURST_SIZE,\
				.dma_callback = i2s_dma_rx_callback,\
				.callback_arg = I2S_DEVICE_OBJECT(i2s_id),\
				.complete_callback_en = 1,	\
				.error_callback_en = 1,		\
				.block_count = 1,		\
				.head_block =			\
				&i2s##i2s_id##_cavs_data.rx.dma_block,\
				.channel_direction = PERIPHERAL_TO_MEMORY,\
				.dma_slot = DMA_HANDSHAKE_SSP##i2s_id##_RX,\
				},				\
		},						\
	}

#define I2S_DEVICE_AND_API_INIT(i2s_id)				\
	DEVICE_AND_API_INIT(I2S_DEVICE_NAME(i2s_id),		\
			CONFIG_I2S_CAVS_##i2s_id##_NAME,	\
			i2s_cavs_initialize,			\
			&I2S_DEVICE_DATA_NAME(i2s_id),		\
			&I2S_DEVICE_CONFIG_NAME(i2s_id),	\
			POST_KERNEL,				\
			CONFIG_I2S_INIT_PRIORITY,		\
			&i2s_cavs_driver_api)

/* length of the buffer queue */
#define I2S_CAVS_BUF_Q_LEN			2

#define CAVS_SSP_WORD_SIZE_BITS_MIN     4
#define CAVS_SSP_WORD_SIZE_BITS_MAX     32
#define CAVS_SSP_WORD_PER_FRAME_MIN     1
#define CAVS_SSP_WORD_PER_FRAME_MAX     8

#define CAVS_I2S_DMA_BURST_SIZE		8

/*
 * This indicates the Tx/Rx stream. Most members of the stream are
 * self-explanatory
 *
 * in_queue and out_queue are used as follows
 *   transmit stream:
 *      application provided buffer is queued to in_queue until loaded to DMA.
 *      when DMA channel is idle, buffer is retrieved from in_queue and loaded
 *      to DMA and queued to out_queue.
 *      when DMA completes, buffer is retrieved from out_queue and freed.
 *
 *   receive stream:
 *      driver allocates buffer from slab and loads DMA
 *      buffer is queued to in_queue
 *      when DMA completes, buffer is retrieved from in_queue and queued to
 *      out_queue
 *	when application reads, buffer is read (may optionally block) from
 *      out_queue and presented to application.
 */
struct stream {
	s32_t state;
	u32_t dma_channel;
	struct dma_config dma_cfg;
	struct dma_block_config dma_block;
	struct k_msgq in_queue;
	void *in_msgs[I2S_CAVS_BUF_Q_LEN];
	struct k_msgq out_queue;
	void *out_msgs[I2S_CAVS_BUF_Q_LEN];
};

struct i2s_cavs_config {
	struct i2s_cavs_ssp *regs;
	struct i2s_cavs_mn_div *mn_regs;
	u32_t irq_id;
	void (*irq_connect)(void);
};

/* Device run time data */
struct i2s_cavs_dev_data {
	struct i2s_config cfg;
	struct device *dev_dma;
	struct stream tx;
	struct stream rx;
};

#define DEV_NAME(dev) ((dev)->config->name)
#define DEV_CFG(dev) \
	((const struct i2s_cavs_config *const)(dev)->config->config_info)
#define DEV_DATA(dev) \
	((struct i2s_cavs_dev_data *const)(dev)->driver_data)

I2S_DEVICE_OBJECT_DECLARE(1);
I2S_DEVICE_OBJECT_DECLARE(2);
I2S_DEVICE_OBJECT_DECLARE(3);

static void i2s_dma_tx_callback(void *, u32_t, int);
static void i2s_tx_stream_disable(struct i2s_cavs_dev_data *,
		volatile struct i2s_cavs_ssp *const, struct device *);
static void i2s_rx_stream_disable(struct i2s_cavs_dev_data *,
		volatile struct i2s_cavs_ssp *const, struct device *);

static inline void i2s_purge_stream_buffers(struct stream *strm,
	struct k_mem_slab *mem_slab)
{
	void *buffer;

	while (k_msgq_get(&strm->in_queue, &buffer, K_NO_WAIT) == 0) {
		k_mem_slab_free(mem_slab, &buffer);
	}
	while (k_msgq_get(&strm->out_queue, &buffer, K_NO_WAIT) == 0) {
		k_mem_slab_free(mem_slab, &buffer);
	}
}

/* This function is executed in the interrupt context */
static void i2s_dma_tx_callback(void *arg, u32_t channel,
		int status)
{
	struct device *dev = (struct device *)arg;
	const struct i2s_cavs_config *const dev_cfg = DEV_CFG(dev);
	struct i2s_cavs_dev_data *const dev_data = DEV_DATA(dev);

	volatile struct i2s_cavs_ssp *const ssp = dev_cfg->regs;
	struct stream *strm = &dev_data->tx;
	void *buffer;
	int ret;

	ret = k_msgq_get(&strm->out_queue, &buffer, K_NO_WAIT);
	if (ret == 0) {
		/* transmission complete. free the buffer */
		k_mem_slab_free(dev_data->cfg.mem_slab, &buffer);
	} else {
		LOG_ERR("no buffer in output queue for channel %u",
				channel);
	}

	switch (strm->state) {

	case I2S_STATE_RUNNING:
		/* get the next buffer from queue */
		ret = k_msgq_get(&strm->in_queue, &buffer, K_NO_WAIT);
		if (ret == 0) {
			/* reload the DMA */
			dma_reload(dev_data->dev_dma, strm->dma_channel,
					(u32_t)buffer, (u32_t)&ssp->ssd,
					dev_data->cfg.block_size);
			dma_start(dev_data->dev_dma, strm->dma_channel);
			ssp->ssc1 |= SSCR1_TSRE;
			k_msgq_put(&strm->out_queue, &buffer, K_NO_WAIT);
		}

		if (ret || status) {
			/*
			 * DMA encountered an error (status != 0)
			 * or
			 * No bufers in input queue
			 */
			LOG_ERR("DMA status %08x channel %u k_msgq_get ret %d",
					status, channel, ret);
			strm->state = I2S_STATE_STOPPING;
			i2s_tx_stream_disable(dev_data, ssp, dev_data->dev_dma);
		}

		break;

	case I2S_STATE_STOPPING:
		i2s_tx_stream_disable(dev_data, ssp, dev_data->dev_dma);
		break;
	}
}

static void i2s_dma_rx_callback(void *arg, u32_t channel, int status)
{
	struct device *dev = (struct device *)arg;
	const struct i2s_cavs_config *const dev_cfg = DEV_CFG(dev);
	struct i2s_cavs_dev_data *const dev_data = DEV_DATA(dev);
	volatile struct i2s_cavs_ssp *const ssp = dev_cfg->regs;
	struct stream *strm = &dev_data->rx;
	void *buffer;
	int ret;

	switch (strm->state) {

	case I2S_STATE_RUNNING:
		/* retrieve buffer from input queue */
		ret = k_msgq_get(&strm->in_queue, &buffer, K_NO_WAIT);
		if (ret != 0) {
			LOG_ERR("get buffer from in_queue %p failed (%d)",
					&strm->in_queue, ret);
		}
		/* put buffer to output queue */
		ret = k_msgq_put(&strm->out_queue, &buffer, K_NO_WAIT);
		if (ret != 0) {
			LOG_ERR("buffer %p -> out_queue %p err %d",
					buffer, &strm->out_queue, ret);
		}
		/* allocate new buffer for next audio frame */
		ret = k_mem_slab_alloc(dev_data->cfg.mem_slab, &buffer,
				K_NO_WAIT);
		if (ret != 0) {
			LOG_ERR("buffer alloc from slab %p err %d",
					dev_data->cfg.mem_slab, ret);
			i2s_rx_stream_disable(dev_data, ssp, dev_data->dev_dma);
			strm->state = I2S_STATE_READY;
		} else {
			/* put buffer in input queue */
			ret = k_msgq_put(&strm->in_queue, &buffer, K_NO_WAIT);
			if (ret != 0) {
				LOG_ERR("buffer %p -> in_queue %p err %d",
						buffer, &strm->in_queue, ret);
			}

			SOC_DCACHE_INVALIDATE(buffer, dev_data->cfg.block_size);

			/* reload the DMA */
			dma_reload(dev_data->dev_dma, strm->dma_channel,
					(u32_t)&ssp->ssd, (u32_t)buffer,
					dev_data->cfg.block_size);
			dma_start(dev_data->dev_dma, strm->dma_channel);
			ssp->ssc1 |= SSCR1_RSRE;
		}
		break;
	case I2S_STATE_STOPPING:
		i2s_rx_stream_disable(dev_data, ssp, dev_data->dev_dma);
		strm->state = I2S_STATE_READY;
		break;
	}
}

static int i2s_cavs_configure(struct device *dev, enum i2s_dir dir,
			      struct i2s_config *i2s_cfg)
{
	const struct i2s_cavs_config *const dev_cfg = DEV_CFG(dev);
	struct i2s_cavs_dev_data *const dev_data = DEV_DATA(dev);
	volatile struct i2s_cavs_ssp *const ssp = dev_cfg->regs;
	volatile struct i2s_cavs_mn_div *const mn_div = dev_cfg->mn_regs;
	struct dma_block_config *dma_block;
	u8_t num_words = i2s_cfg->channels;
	u8_t word_size_bits = i2s_cfg->word_size;
	u8_t word_size_bytes;
	u32_t bit_clk_freq, mclk;
	int ret;

	u32_t ssc0;
	u32_t ssc1;
	u32_t ssc2;
	u32_t ssc3;
	u32_t sspsp;
	u32_t sspsp2;
	u32_t sstsa;
	u32_t ssrsa;
	u32_t ssto;
	u32_t ssioc = 0U;
	u32_t mdiv;
	u32_t i2s_m = 0U;
	u32_t i2s_n = 0U;
	u32_t frame_len = 0U;
	bool inverted_frame = false;

	if ((dev_data->tx.state != I2S_STATE_NOT_READY) &&
			(dev_data->tx.state != I2S_STATE_READY) &&
			(dev_data->rx.state != I2S_STATE_NOT_READY) &&
			(dev_data->rx.state != I2S_STATE_READY)) {
		LOG_ERR("invalid state tx(%u) rx(%u)", dev_data->tx.state,
				dev_data->rx.state);
		return -EINVAL;
	}

	if (i2s_cfg->frame_clk_freq == 0U) {
		LOG_ERR("Invalid frame_clk_freq %u",
				i2s_cfg->frame_clk_freq);
		return -EINVAL;
	}

	if (word_size_bits < CAVS_SSP_WORD_SIZE_BITS_MIN ||
	    word_size_bits > CAVS_SSP_WORD_SIZE_BITS_MAX) {
		LOG_ERR("Unsupported I2S word size %u", word_size_bits);
		return -EINVAL;
	}

	if (num_words < CAVS_SSP_WORD_PER_FRAME_MIN ||
	    num_words > CAVS_SSP_WORD_PER_FRAME_MAX) {
		LOG_ERR("Unsupported words per frame number %u", num_words);
		return -EINVAL;
	}

	if ((i2s_cfg->options & I2S_OPT_PINGPONG) == I2S_OPT_PINGPONG) {
		LOG_ERR("Ping-pong mode not supported");
		return -ENOTSUP;
	}

	memcpy(&dev_data->cfg, i2s_cfg, sizeof(struct i2s_config));

	/* reset SSP settings */
	/* sscr0 dynamic settings are DSS, EDSS, SCR, FRDC, ECS */
	ssc0 = SSCR0_MOD | SSCR0_PSP | SSCR0_RIM;

	/* sscr1 dynamic settings are SFRMDIR, SCLKDIR, SCFR */
	ssc1 = SSCR1_TTE | SSCR1_TTELP | SSCR1_TRAIL;

	/* sscr2 dynamic setting is LJDFD */
	ssc2 = 0U;

	/* sscr3 dynamic settings are TFT, RFT */
	ssc3 = SSCR3_TX(CAVS_I2S_DMA_BURST_SIZE) |
		SSCR3_RX(CAVS_I2S_DMA_BURST_SIZE);

	/* sspsp dynamic settings are SCMODE, SFRMP, DMYSTRT, SFRMWDTH */
	sspsp = 0U;

	/* sspsp2 no dynamic setting */
	sspsp2 = 0x0;

	/* ssto no dynamic setting */
	ssto = 0x0;

	/* sstsa dynamic setting is TTSA, set according to num_words */
	sstsa = BIT_MASK(num_words);
	/* ssrsa dynamic setting is RTSA, set according to num_words */
	ssrsa = BIT_MASK(num_words);

	if (i2s_cfg->options & I2S_OPT_BIT_CLK_SLAVE) {
		/* set BCLK mode as slave */
		ssc1 |= SSCR1_SCLKDIR;
	} else {
		/* enable BCLK output */
		ssioc = SSIOC_SCOE;
	}

	if (i2s_cfg->options & I2S_OPT_FRAME_CLK_SLAVE) {
		/* set WCLK mode as slave */
		ssc1 |= SSCR1_SFRMDIR;
	}

	ssioc |= SSIOC_SFCR;

	/* clock signal polarity */
	switch (i2s_cfg->format & I2S_FMT_CLK_FORMAT_MASK) {
	case I2S_FMT_CLK_NF_NB:
		break;

	case I2S_FMT_CLK_NF_IB:
		sspsp |= SSPSP_SCMODE(2);
		break;

	case I2S_FMT_CLK_IF_NB:
		inverted_frame = true; /* handled later with format */
		break;

	case I2S_FMT_CLK_IF_IB:
		sspsp |= SSPSP_SCMODE(2);
		inverted_frame = true; /* handled later with format */
		break;

	default:
		LOG_ERR("Unsupported Clock format");
		return -EINVAL;
	}

	mclk = soc_get_ref_clk_freq();
	bit_clk_freq = i2s_cfg->frame_clk_freq * word_size_bits * num_words;

	/* BCLK is generated from MCLK - must be divisible */
	if (mclk % bit_clk_freq) {
		LOG_INF("MCLK/BCLK is not an integer, using M/N divider");

		/*
		 * Simplification: Instead of calculating lowest values of
		 * M and N, just set M and N as BCLK and MCLK respectively
		 * in 0.1KHz units
		 * In addition, double M so that it can be later divided by 2
		 * to get an approximately 50% duty cycle clock
		 */
		i2s_m = (bit_clk_freq << 1) / 100U;
		i2s_n = mclk / 100U;

		/* set divider value of 1 which divides the clock by 2 */
		mdiv = 1U;

		/* Select M/N divider as the clock source */
		ssc0 |= SSCR0_ECS;
	} else {
		mdiv = (mclk / bit_clk_freq) - 1;
	}

	/* divisor must be within SCR range */
	if (mdiv > (SSCR0_SCR_MASK >> 8)) {
		LOG_ERR("Divisor is not within SCR range");
		return -EINVAL;
	}

	/* set the SCR divisor */
	ssc0 |= SSCR0_SCR(mdiv);

	/* format */
	switch (i2s_cfg->format & I2S_FMT_DATA_FORMAT_MASK) {

	case I2S_FMT_DATA_FORMAT_I2S:
		ssc0 |= SSCR0_FRDC(i2s_cfg->channels);

		/* set asserted frame length */
		frame_len = word_size_bits;

		/* handle frame polarity, I2S default is falling/active low */
		sspsp |= SSPSP_SFRMP(!inverted_frame) | SSPSP_FSRT;
		break;

	case I2S_FMT_DATA_FORMAT_LEFT_JUSTIFIED:
		ssc0 |= SSCR0_FRDC(i2s_cfg->channels);

		/* LJDFD enable */
		ssc2 &= ~SSCR2_LJDFD;

		/* set asserted frame length */
		frame_len = word_size_bits;

		/* LEFT_J default is rising/active high, opposite of I2S */
		sspsp |= SSPSP_SFRMP(inverted_frame);
		break;

	case I2S_FMT_DATA_FORMAT_PCM_SHORT:
	case I2S_FMT_DATA_FORMAT_PCM_LONG:
	default:
		LOG_ERR("Unsupported I2S data format");
		return -EINVAL;
	}

	sspsp |= SSPSP_SFRMWDTH(frame_len);

	if (word_size_bits > 16) {
		ssc0 |= (SSCR0_EDSS | SSCR0_DSIZE(word_size_bits - 16));
	} else {
		ssc0 |= SSCR0_DSIZE(word_size_bits);
	}

	ssp->ssc0 = ssc0;
	ssp->ssc1 = ssc1;
	ssp->ssc2 = ssc2;
	ssp->ssc3 = ssc3;
	ssp->sspsp2 = sspsp2;
	ssp->sspsp = sspsp;
	ssp->ssioc = ssioc;
	ssp->ssto = ssto;
	ssp->sstsa = sstsa;
	ssp->ssrsa = ssrsa;

	mn_div->mval = I2S_MNVAL(i2s_m);
	mn_div->nval = I2S_MNVAL(i2s_n);

	/* Set up DMA channel parameters */
	word_size_bytes = (word_size_bits + 7) / 8U;
	dev_data->tx.dma_cfg.source_data_size = word_size_bytes;
	dev_data->tx.dma_cfg.dest_data_size = word_size_bytes;
	dev_data->rx.dma_cfg.source_data_size = word_size_bytes;
	dev_data->rx.dma_cfg.dest_data_size = word_size_bytes;

	dma_block = dev_data->tx.dma_cfg.head_block;
	dma_block->block_size = i2s_cfg->block_size;
	dma_block->source_address = (u32_t)NULL;
	dma_block->dest_address = (u32_t)&ssp->ssd;

	ret = dma_config(dev_data->dev_dma, dev_data->tx.dma_channel,
			&dev_data->tx.dma_cfg);
	if (ret < 0) {
		LOG_ERR("dma_config failed: %d", ret);
		return ret;
	}

	dma_block = dev_data->rx.dma_cfg.head_block;
	dma_block->block_size = i2s_cfg->block_size;
	dma_block->source_address = (u32_t)&ssp->ssd;
	dma_block->dest_address = (u32_t)NULL;

	ret = dma_config(dev_data->dev_dma, dev_data->rx.dma_channel,
			&dev_data->rx.dma_cfg);
	if (ret < 0) {
		LOG_ERR("dma_config failed: %d", ret);
		return ret;
	}

	/* enable port */
	ssp->ssc0 |= SSCR0_SSE;

	/* enable interrupt */
	irq_enable(dev_cfg->irq_id);

	dev_data->tx.state = I2S_STATE_READY;
	dev_data->rx.state = I2S_STATE_READY;

	return 0;
}

static int i2s_tx_stream_start(struct i2s_cavs_dev_data *dev_data,
			   volatile struct i2s_cavs_ssp *const ssp,
			   struct device *dev_dma)
{
	int ret = 0;
	void *buffer;
	unsigned int key;
	struct stream *strm = &dev_data->tx;

	/* retrieve buffer from input queue */
	ret = k_msgq_get(&strm->in_queue, &buffer, K_NO_WAIT);
	if (ret != 0) {
		LOG_ERR("No buffer in input queue to start transmission");
		return ret;
	}

	ret = dma_reload(dev_dma, strm->dma_channel, (u32_t)buffer,
			(u32_t)&ssp->ssd, dev_data->cfg.block_size);
	if (ret != 0) {
		LOG_ERR("dma_reload failed (%d)", ret);
		return ret;
	}

	/* put buffer in output queue */
	ret = k_msgq_put(&strm->out_queue, &buffer, K_NO_WAIT);
	if (ret != 0) {
		LOG_ERR("failed to put buffer in output queue");
		return ret;
	}

	ret = dma_start(dev_dma, strm->dma_channel);

	if (ret < 0) {
		LOG_ERR("dma_start failed (%d)", ret);
		return ret;
	}

	/* Enable transmit operation */
	key = irq_lock();
	ssp->ssc1 |= SSCR1_TSRE;
	ssp->sstsa |= SSTSA_TXEN;
	irq_unlock(key);

	return 0;
}

static int i2s_rx_stream_start(struct i2s_cavs_dev_data *dev_data,
		volatile struct i2s_cavs_ssp *const ssp, struct device *dev_dma)
{
	int ret = 0;
	void *buffer;
	unsigned int key;
	struct stream *strm = &dev_data->rx;

	/* allocate receive buffer from SLAB */
	ret = k_mem_slab_alloc(dev_data->cfg.mem_slab, &buffer, K_NO_WAIT);
	if (ret != 0) {
		LOG_ERR("buffer alloc from mem_slab failed (%d)", ret);
		return ret;
	}

	SOC_DCACHE_INVALIDATE(buffer, dev_data->cfg.block_size);

	ret = dma_reload(dev_dma, strm->dma_channel, (u32_t)&ssp->ssd,
			(u32_t)buffer, dev_data->cfg.block_size);
	if (ret != 0) {
		LOG_ERR("dma_reload failed (%d)", ret);
		return ret;
	}

	/* put buffer in input queue */
	ret = k_msgq_put(&strm->in_queue, &buffer, K_NO_WAIT);
	if (ret != 0) {
		LOG_ERR("failed to put buffer in output queue");
		return ret;
	}

	LOG_INF("Starting DMA Ch%u", strm->dma_channel);
	ret = dma_start(dev_dma, strm->dma_channel);
	if (ret < 0) {
		LOG_ERR("Failed to start DMA Ch%d (%d)", strm->dma_channel,
				ret);
		return ret;
	}

	/* Enable Receive operation */
	key = irq_lock();
	ssp->ssc1 |= SSCR1_RSRE;
	ssp->ssrsa |= SSRSA_RXEN;
	irq_unlock(key);

	return 0;
}

static void i2s_tx_stream_disable(struct i2s_cavs_dev_data *dev_data,
			      volatile struct i2s_cavs_ssp *const ssp,
			      struct device *dev_dma)
{
	struct stream *strm = &dev_data->tx;
	unsigned int key;

	/*
	 * Enable transmit undderrun interrupt to allow notification
	 * upon transmit FIFO being emptied.
	 * Defer disabling of TX to the underrun processing in ISR
	 */
	key = irq_lock();
	ssp->ssc0 &= ~SSCR0_TIM;
	irq_unlock(key);

	LOG_INF("Stopping DMA channel %u for TX stream", strm->dma_channel);
	dma_stop(dev_dma, strm->dma_channel);

	/* purge buffers queued in the stream */
	i2s_purge_stream_buffers(strm, dev_data->cfg.mem_slab);
}

static void i2s_rx_stream_disable(struct i2s_cavs_dev_data *dev_data,
			      volatile struct i2s_cavs_ssp *const ssp,
			      struct device *dev_dma)
{
	struct stream *strm = &dev_data->rx;
	u32_t data;

	/* Disable DMA service request handshake logic. Handshake is
	 * not required now since DMA is not in operation.
	 */
	ssp->ssrsa &= ~SSRSA_RXEN;

	LOG_INF("Stopping RX stream & DMA channel %u", strm->dma_channel);
	dma_stop(dev_dma, strm->dma_channel);

	/* Empty the FIFO */
	while (ssp->sss & SSSR_RNE) {
		/* read the RX FIFO */
		data = ssp->ssd;
	}

	/* purge buffers queued in the stream */
	i2s_purge_stream_buffers(strm, dev_data->cfg.mem_slab);
}

static int i2s_cavs_trigger(struct device *dev, enum i2s_dir dir,
			    enum i2s_trigger_cmd cmd)
{
	const struct i2s_cavs_config *const dev_cfg = DEV_CFG(dev);
	struct i2s_cavs_dev_data *const dev_data = DEV_DATA(dev);
	volatile struct i2s_cavs_ssp *const ssp = dev_cfg->regs;
	struct stream *strm;
	unsigned int key;
	int ret = 0;

	strm = (dir == I2S_DIR_TX) ? &dev_data->tx : &dev_data->rx;

	key = irq_lock();
	switch (cmd) {
	case I2S_TRIGGER_START:
		if (strm->state != I2S_STATE_READY) {
			LOG_ERR("START trigger: invalid state %u", strm->state);
			ret = -EIO;
			break;
		}

		__ASSERT_NO_MSG(strm->mem_block == NULL);

		if (dir == I2S_DIR_TX) {
			ret = i2s_tx_stream_start(dev_data, ssp,
					dev_data->dev_dma);
		} else {
			ret = i2s_rx_stream_start(dev_data, ssp,
					dev_data->dev_dma);
		}

		if (ret < 0) {
			LOG_DBG("START trigger failed %d", ret);
			break;
		}

		strm->state = I2S_STATE_RUNNING;
		break;

	case I2S_TRIGGER_STOP:
	case I2S_TRIGGER_DRAIN:
	case I2S_TRIGGER_DROP:
		if (strm->state != I2S_STATE_RUNNING) {
			LOG_DBG("STOP/DRAIN/DROP trigger: invalid state");
			ret = -EIO;
			break;
		}
		strm->state = I2S_STATE_STOPPING;
		break;

	case I2S_TRIGGER_PREPARE:
		break;

	default:
		LOG_ERR("Unsupported trigger command");
		ret = -EINVAL;
	}

	irq_unlock(key);
	return ret;
}

static int i2s_cavs_read(struct device *dev, void **mem_block, size_t *size)
{
	struct i2s_cavs_dev_data *const dev_data = DEV_DATA(dev);
	struct stream *strm = &dev_data->rx;
	void *buffer;
	int ret = 0;

	if (strm->state == I2S_STATE_NOT_READY) {
		LOG_ERR("invalid state %d", strm->state);
		return -EIO;
	}

	ret = k_msgq_get(&strm->out_queue, &buffer, dev_data->cfg.timeout);
	if (ret != 0) {
		return -EAGAIN;
	}

	*mem_block = buffer;
	*size = dev_data->cfg.block_size;
	return 0;
}

static int i2s_cavs_write(struct device *dev, void *mem_block, size_t size)
{
	struct i2s_cavs_dev_data *const dev_data = DEV_DATA(dev);
	struct stream *strm = &dev_data->tx;
	int ret;

	if (strm->state != I2S_STATE_RUNNING &&
	    strm->state != I2S_STATE_READY) {
		LOG_ERR("invalid state (%d)", strm->state);
		return -EIO;
	}

	SOC_DCACHE_FLUSH(mem_block, size);

	ret = k_msgq_put(&strm->in_queue, &mem_block, dev_data->cfg.timeout);
	if (ret) {
		LOG_ERR("k_msgq_put failed %d", ret);
		return ret;
	}

	return ret;
}

/* clear IRQ sources atm */
static void i2s_cavs_isr(void *arg)
{
	struct device *dev = (struct device *)arg;
	const struct i2s_cavs_config *const dev_cfg = DEV_CFG(dev);
	volatile struct i2s_cavs_ssp *const ssp = dev_cfg->regs;
	struct i2s_cavs_dev_data *const dev_data = DEV_DATA(dev);
	u32_t status;

	/* clear interrupts */
	status = ssp->sss;
	ssp->sss = status;

	if (status & SSSR_TUR) {
		/*
		 * transmit underrun occurred.
		 * 1. disable transmission
		 * 2. disable underrun interrupt
		 */
		ssp->sstsa &= ~SSTSA_TXEN;
		ssp->ssc0 |= SSCR0_TIM;
		dev_data->tx.state = I2S_STATE_READY;
	}
}

static int i2s_cavs_initialize(struct device *dev)
{
	const struct i2s_cavs_config *const dev_cfg = DEV_CFG(dev);
	struct i2s_cavs_dev_data *const dev_data = DEV_DATA(dev);

	dev_data->dev_dma = device_get_binding(CONFIG_I2S_CAVS_DMA_NAME);
	if (!dev_data->dev_dma) {
		LOG_ERR("%s device not found", CONFIG_I2S_CAVS_DMA_NAME);
		return -ENODEV;
	}

	/* Initialize the buffer queues */
	k_msgq_init(&dev_data->tx.in_queue, (char *)dev_data->tx.in_msgs,
			sizeof(void *), I2S_CAVS_BUF_Q_LEN);
	k_msgq_init(&dev_data->rx.in_queue, (char *)dev_data->rx.in_msgs,
			sizeof(void *), I2S_CAVS_BUF_Q_LEN);
	k_msgq_init(&dev_data->tx.out_queue, (char *)dev_data->tx.out_msgs,
			sizeof(void *), I2S_CAVS_BUF_Q_LEN);
	k_msgq_init(&dev_data->rx.out_queue, (char *)dev_data->rx.out_msgs,
			sizeof(void *), I2S_CAVS_BUF_Q_LEN);

	/* register ISR */
	dev_cfg->irq_connect();

	dev_data->tx.state = I2S_STATE_NOT_READY;
	dev_data->rx.state = I2S_STATE_NOT_READY;

	LOG_INF("Device %s initialized", DEV_NAME(dev));

	return 0;
}

static const struct i2s_driver_api i2s_cavs_driver_api = {
	.configure = i2s_cavs_configure,
	.read = i2s_cavs_read,
	.write = i2s_cavs_write,
	.trigger = i2s_cavs_trigger,
};

static void i2s1_cavs_irq_connect(void)
{
	I2S_IRQ_CONNECT(1);
}

I2S_DEVICE_CONFIG_DEFINE(1);
I2S_DEVICE_DATA_DEFINE(1);
I2S_DEVICE_AND_API_INIT(1);

static void i2s2_cavs_irq_connect(void)
{
	I2S_IRQ_CONNECT(2);
}

I2S_DEVICE_CONFIG_DEFINE(2);
I2S_DEVICE_DATA_DEFINE(2);
I2S_DEVICE_AND_API_INIT(2);

static void i2s3_cavs_irq_connect(void)
{
	I2S_IRQ_CONNECT(3);
}

I2S_DEVICE_CONFIG_DEFINE(3);
I2S_DEVICE_DATA_DEFINE(3);
I2S_DEVICE_AND_API_INIT(3);
