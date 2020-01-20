/*
 * Copyright (c) 2016 Linaro Limited.
 * Copyright (c) 2019 Song Qiang <songqiang1304521@gmail.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @brief Common part of DMA drivers for stm32.
 * @note  Functions named with stm32_dma_* are SoCs related functions
 *        implemented in dma_stm32_v*.c
 */

#include "dma_stm32.h"

#define LOG_LEVEL CONFIG_DMA_LOG_LEVEL
#include <logging/log.h>
LOG_MODULE_REGISTER(dma_stm32);

#include <clock_control/stm32_clock_control.h>

static u32_t table_m_size[] = {
	LL_DMA_MDATAALIGN_BYTE,
	LL_DMA_MDATAALIGN_HALFWORD,
	LL_DMA_MDATAALIGN_WORD,
};

static u32_t table_p_size[] = {
	LL_DMA_PDATAALIGN_BYTE,
	LL_DMA_PDATAALIGN_HALFWORD,
	LL_DMA_PDATAALIGN_WORD,
};

struct dma_stm32_stream {
	u32_t direction;
	bool source_periph;
	bool busy;
	u32_t src_size;
	u32_t dst_size;
	void *callback_arg;
	void (*dma_callback)(void *arg, u32_t id,
			     int error_code);
};

struct dma_stm32_data {
	int max_streams;
	struct dma_stm32_stream *streams;
};

struct dma_stm32_config {
	struct stm32_pclken pclken;
	void (*config_irq)(struct device *dev);
	bool support_m2m;
	u32_t base;
};

/* Maximum data sent in single transfer (Bytes) */
#define DMA_STM32_MAX_DATA_ITEMS		0xffff

static void dma_stm32_dump_stream_irq(struct device *dev, u32_t id)
{
	const struct dma_stm32_config *config = dev->config->config_info;
	DMA_TypeDef *dma = (DMA_TypeDef *)(config->base);

	stm32_dma_dump_stream_irq(dma, id);
}

static void dma_stm32_clear_stream_irq(struct device *dev, u32_t id)
{
	const struct dma_stm32_config *config = dev->config->config_info;
	DMA_TypeDef *dma = (DMA_TypeDef *)(config->base);

	func_ll_clear_tc[id](dma);
	func_ll_clear_ht[id](dma);
	stm32_dma_clear_stream_irq(dma, id);
}

static void dma_stm32_irq_handler(void *arg)
{
	struct device *dev = arg;
	struct dma_stm32_data *data = dev->driver_data;
	const struct dma_stm32_config *config = dev->config->config_info;
	DMA_TypeDef *dma = (DMA_TypeDef *)(config->base);
	struct dma_stm32_stream *stream;
	int id;

	for (id = 0; id < data->max_streams; id++) {
		if (func_ll_is_active_tc[id](dma)) {
			break;
		}
		if (stm32_dma_is_irq_happened(dma, id)) {
			break;
		}
	}

	if (id == data->max_streams) {
		LOG_ERR("Unknown interrupt happened.");
		return;
	}

	stream = &data->streams[id];
	stream->busy = false;

	if (func_ll_is_active_tc[id](dma)) {
		func_ll_clear_tc[id](dma);

		stream->dma_callback(stream->callback_arg, id, 0);
	} else if (stm32_dma_is_unexpected_irq_happened(dma, id)) {
		LOG_ERR("Unexpected irq happened.");
		stream->dma_callback(stream->callback_arg, id, -EIO);
	} else {
		LOG_ERR("Transfer Error.");
		dma_stm32_dump_stream_irq(dev, id);
		dma_stm32_clear_stream_irq(dev, id);

		stream->dma_callback(stream->callback_arg, id, -EIO);
	}
}

static u32_t dma_stm32_width_config(struct dma_config *config,
				    bool source_periph,
				    DMA_TypeDef *dma,
				    LL_DMA_InitTypeDef *DMA_InitStruct,
				    u32_t id)
{
	u32_t periph, memory;
	u32_t m_size = 0, p_size = 0;

	if (source_periph) {
		periph = config->source_data_size;
		memory = config->dest_data_size;
	} else {
		periph = config->dest_data_size;
		memory = config->source_data_size;
	}
	int index = find_lsb_set(config->source_data_size) - 1;

	m_size = table_m_size[index];
	index = find_lsb_set(config->dest_data_size) - 1;
	p_size = table_p_size[index];

	DMA_InitStruct->PeriphOrM2MSrcDataSize = p_size;
	DMA_InitStruct->MemoryOrM2MDstDataSize = m_size;
	return 0;
}

static int dma_stm32_get_priority(u8_t priority, u32_t *ll_priority)
{
	switch (priority) {
	case 0x0:
		*ll_priority = LL_DMA_PRIORITY_LOW;
		break;
	case 0x1:
		*ll_priority = LL_DMA_PRIORITY_MEDIUM;
		break;
	case 0x2:
		*ll_priority = LL_DMA_PRIORITY_HIGH;
		break;
	case 0x3:
		*ll_priority = LL_DMA_PRIORITY_VERYHIGH;
		break;
	default:
		LOG_ERR("Priority error. %d", priority);
		return -EINVAL;
	}

	return 0;
}

static int dma_stm32_get_direction(enum dma_channel_direction direction,
				   u32_t *ll_direction)
{
	switch (direction) {
	case MEMORY_TO_MEMORY:
		*ll_direction = LL_DMA_DIRECTION_MEMORY_TO_MEMORY;
		break;
	case MEMORY_TO_PERIPHERAL:
		*ll_direction = LL_DMA_DIRECTION_MEMORY_TO_PERIPH;
		break;
	case PERIPHERAL_TO_MEMORY:
		*ll_direction = LL_DMA_DIRECTION_PERIPH_TO_MEMORY;
		break;
	default:
		LOG_ERR("Direction error. %d", direction);
		return -EINVAL;
	}

	return 0;
}

static int dma_stm32_get_memory_increment(enum dma_addr_adj increment,
					  u32_t *ll_increment)
{
	switch (increment) {
	case DMA_ADDR_ADJ_INCREMENT:
		*ll_increment = LL_DMA_MEMORY_INCREMENT;
		break;
	case DMA_ADDR_ADJ_NO_CHANGE:
		*ll_increment = LL_DMA_MEMORY_NOINCREMENT;
		break;
	case DMA_ADDR_ADJ_DECREMENT:
		return -ENOTSUP;
	default:
		LOG_ERR("Memory increment error. %d", increment);
		return -EINVAL;
	}

	return 0;
}

static int dma_stm32_get_periph_increment(enum dma_addr_adj increment,
					  u32_t *ll_increment)
{
	switch (increment) {
	case DMA_ADDR_ADJ_INCREMENT:
		*ll_increment = LL_DMA_PERIPH_INCREMENT;
		break;
	case DMA_ADDR_ADJ_NO_CHANGE:
		*ll_increment = LL_DMA_PERIPH_NOINCREMENT;
		break;
	case DMA_ADDR_ADJ_DECREMENT:
		return -ENOTSUP;
	default:
		LOG_ERR("Periph increment error. %d", increment);
		return -EINVAL;
	}

	return 0;
}

static int dma_stm32_configure(struct device *dev, u32_t id,
			       struct dma_config *config)
{
	struct dma_stm32_data *data = dev->driver_data;
	struct dma_stm32_stream *stream = &data->streams[id];
	const struct dma_stm32_config *dev_config =
					dev->config->config_info;
	DMA_TypeDef *dma = (DMA_TypeDef *)dev_config->base;
	LL_DMA_InitTypeDef DMA_InitStruct;
	u32_t msize;
	int ret;

	if (id >= data->max_streams) {
		return -EINVAL;
	}

	if (stream->busy) {
		return -EBUSY;
	}

	stm32_dma_disable_stream(dma, id);
	dma_stm32_clear_stream_irq(dev, id);

	if (config->head_block->block_size > DMA_STM32_MAX_DATA_ITEMS) {
		LOG_ERR("Data size too big: %d\n",
		       config->head_block->block_size);
		return -EINVAL;
	}

	if ((stream->direction == MEMORY_TO_MEMORY) &&
		(!dev_config->support_m2m)) {
		LOG_ERR("Memcopy not supported for device %s",
			dev->config->name);
		return -ENOTSUP;
	}

	if (config->source_data_size != 4U &&
	    config->source_data_size != 2U &&
	    config->source_data_size != 1U) {
		LOG_ERR("Source unit size error, %d",
			config->source_data_size);
		return -EINVAL;
	}

	if (config->dest_data_size != 4U &&
	    config->dest_data_size != 2U &&
	    config->dest_data_size != 1U) {
		LOG_ERR("Dest unit size error, %d",
			config->dest_data_size);
		return -EINVAL;
	}

	/*
	 * STM32's circular mode will auto reset both source address
	 * counter and destination address counter.
	 */
	if (config->head_block->source_reload_en !=
		config->head_block->dest_reload_en) {
		LOG_ERR("source_reload_en and dest_reload_en must "
			"be the same.");
		return -EINVAL;
	}

	stream->busy		= true;
	stream->dma_callback	= config->dma_callback;
	stream->direction	= config->channel_direction;
	stream->callback_arg    = config->callback_arg;
	stream->src_size	= config->source_data_size;
	stream->dst_size	= config->dest_data_size;

	if (stream->direction == MEMORY_TO_PERIPHERAL) {
		DMA_InitStruct.MemoryOrM2MDstAddress =
					config->head_block->source_address;
		DMA_InitStruct.PeriphOrM2MSrcAddress =
					config->head_block->dest_address;
	} else {
		DMA_InitStruct.PeriphOrM2MSrcAddress =
					config->head_block->source_address;
		DMA_InitStruct.MemoryOrM2MDstAddress =
					config->head_block->dest_address;
	}

	u16_t memory_addr_adj, periph_addr_adj;

	ret = dma_stm32_get_priority(config->channel_priority,
				     &DMA_InitStruct.Priority);
	if (ret < 0) {
		return ret;
	}

	ret = dma_stm32_get_direction(config->channel_direction,
				      &DMA_InitStruct.Direction);
	if (ret < 0) {
		return ret;
	}

	switch (config->channel_direction) {
	case MEMORY_TO_MEMORY:
	case PERIPHERAL_TO_MEMORY:
		memory_addr_adj = config->head_block->dest_addr_adj;
		periph_addr_adj = config->head_block->source_addr_adj;
		break;
	case MEMORY_TO_PERIPHERAL:
		memory_addr_adj = config->head_block->source_addr_adj;
		periph_addr_adj = config->head_block->dest_addr_adj;
		break;
	/* Direction has been asserted in dma_stm32_get_direction. */
	}

	ret = dma_stm32_get_memory_increment(memory_addr_adj,
					&DMA_InitStruct.MemoryOrM2MDstIncMode);
	if (ret < 0) {
		return ret;
	}
	ret = dma_stm32_get_periph_increment(periph_addr_adj,
					&DMA_InitStruct.PeriphOrM2MSrcIncMode);
	if (ret < 0) {
		return ret;
	}

	if (config->head_block->source_reload_en) {
		DMA_InitStruct.Mode = LL_DMA_MODE_CIRCULAR;
	} else {
		DMA_InitStruct.Mode = LL_DMA_MODE_NORMAL;
	}

	stream->source_periph = stream->direction == MEMORY_TO_PERIPHERAL;
	ret = dma_stm32_width_config(config, stream->source_periph, dma,
				     &DMA_InitStruct, id);
	if (ret < 0) {
		return ret;
	}
	msize = DMA_InitStruct.MemoryOrM2MDstDataSize;

#if defined(CONFIG_DMA_STM32_V1)
	DMA_InitStruct.MemBurst = stm32_dma_get_mburst(config,
						       stream->source_periph);
	DMA_InitStruct.PeriphBurst = stm32_dma_get_pburst(config,
							stream->source_periph);

	if (config->channel_direction != MEMORY_TO_MEMORY) {
		if (config->dma_slot >= 8) {
			LOG_ERR("dma slot error.");
			return -EINVAL;
		}
	} else {
		if (config->dma_slot >= 8) {
			LOG_ERR("dma slot is too big, using 0 as default.");
			config->dma_slot = 0;
		}
	}
	DMA_InitStruct.Channel = table_ll_channel[config->dma_slot];

	DMA_InitStruct.FIFOThreshold = stm32_dma_get_fifo_threshold(
					config->head_block->fifo_mode_control);

	if (stm32_dma_check_fifo_mburst(&DMA_InitStruct)) {
		DMA_InitStruct.FIFOMode = LL_DMA_FIFOMODE_ENABLE;
	} else {
		DMA_InitStruct.FIFOMode = LL_DMA_FIFOMODE_DISABLE;
	}
#endif
	if (stream->source_periph) {
		DMA_InitStruct.NbData = config->head_block->block_size /
					config->source_data_size;
	} else {
		DMA_InitStruct.NbData = config->head_block->block_size /
					config->dest_data_size;
	}

	LL_DMA_Init(dma, table_ll_stream[id], &DMA_InitStruct);

	LL_DMA_EnableIT_TC(dma, table_ll_stream[id]);

#if defined(CONFIG_DMA_STM32_V1)
	if (DMA_InitStruct.FIFOMode == LL_DMA_FIFOMODE_ENABLE) {
		LL_DMA_EnableFifoMode(dma, table_ll_stream[id]);
		LL_DMA_EnableIT_FE(dma, table_ll_stream[id]);
	} else {
		LL_DMA_DisableFifoMode(dma, table_ll_stream[id]);
		LL_DMA_DisableIT_FE(dma, table_ll_stream[id]);
	}
#endif
	return ret;
}

int dma_stm32_disable_stream(DMA_TypeDef *dma, u32_t id)
{
	int count = 0;

	for (;;) {
		if (!stm32_dma_disable_stream(dma, id)) {
		}
		/* After trying for 5 seconds, give up */
		if (count++ > (5 * 1000)) {
			return -EBUSY;
		}
		k_sleep(K_MSEC(1));
	}

	return 0;
}

static int dma_stm32_reload(struct device *dev, u32_t id,
			    u32_t src, u32_t dst, size_t size)
{
	const struct dma_stm32_config *config = dev->config->config_info;
	DMA_TypeDef *dma = (DMA_TypeDef *)(config->base);
	struct dma_stm32_data *data = dev->driver_data;
	struct dma_stm32_stream *stream = &data->streams[id];

	if (id >= data->max_streams) {
		return -EINVAL;
	}

	switch (stream->direction) {
	case MEMORY_TO_PERIPHERAL:
		LL_DMA_SetMemoryAddress(dma, table_ll_stream[id], src);
		LL_DMA_SetPeriphAddress(dma, table_ll_stream[id], dst);
		break;
	case MEMORY_TO_MEMORY:
	case PERIPHERAL_TO_MEMORY:
		LL_DMA_SetPeriphAddress(dma, table_ll_stream[id], src);
		LL_DMA_SetMemoryAddress(dma, table_ll_stream[id], dst);
		break;
	default:
		return -EINVAL;
	}

	if (stream->source_periph) {
		LL_DMA_SetDataLength(dma, table_ll_stream[id],
				     size / stream->src_size);
	} else {
		LL_DMA_SetDataLength(dma, table_ll_stream[id],
				     size / stream->dst_size);
	}
	return 0;
}

static int dma_stm32_start(struct device *dev, u32_t id)
{
	const struct dma_stm32_config *config = dev->config->config_info;
	DMA_TypeDef *dma = (DMA_TypeDef *)(config->base);
	struct dma_stm32_data *data = dev->driver_data;

	/* Only M2P or M2M mode can be started manually. */
	if (id >= data->max_streams) {
		return -EINVAL;
	}

	dma_stm32_clear_stream_irq(dev, id);

	stm32_dma_enable_stream(dma, id);

	return 0;
}

static int dma_stm32_stop(struct device *dev, u32_t id)
{
	struct dma_stm32_data *data = dev->driver_data;
	struct dma_stm32_stream *stream = &data->streams[id];
	const struct dma_stm32_config *config =
				dev->config->config_info;
	DMA_TypeDef *dma = (DMA_TypeDef *)(config->base);

	if (id >= data->max_streams) {
		return -EINVAL;
	}

	LL_DMA_DisableIT_TC(dma, table_ll_stream[id]);
#if defined(CONFIG_DMA_STM32_V1)
	stm32_dma_disable_fifo_irq(dma, id);
#endif
	dma_stm32_disable_stream(dma, id);
	dma_stm32_clear_stream_irq(dev, id);

	/* Finally, flag stream as free */
	stream->busy = false;

	return 0;
}

struct k_mem_block block;

static int dma_stm32_init(struct device *dev)
{
	struct dma_stm32_data *data = dev->driver_data;
	const struct dma_stm32_config *config = dev->config->config_info;
	struct device *clk =
		device_get_binding(STM32_CLOCK_CONTROL_NAME);

	if (clock_control_on(clk,
		(clock_control_subsys_t *) &config->pclken) != 0) {
		LOG_ERR("clock op failed\n");
		return -EIO;
	}

	config->config_irq(dev);

	int size_stream =
		sizeof(struct dma_stm32_stream) * data->max_streams;
	data->streams = k_malloc(size_stream);
	if (!data->streams) {
		LOG_ERR("HEAP_MEM_POOL_SIZE is too small");
		return -ENOMEM;
	}
	memset(data->streams, 0, size_stream);

	for (int i = 0; i < data->max_streams; i++) {
		data->streams[i].busy = false;
	}

	return 0;
}

static const struct dma_driver_api dma_funcs = {
	.reload		 = dma_stm32_reload,
	.config		 = dma_stm32_configure,
	.start		 = dma_stm32_start,
	.stop		 = dma_stm32_stop,
};

#define DMA_INIT(index)							\
static void dma_stm32_config_irq_##index(struct device *dev);		\
									\
const struct dma_stm32_config dma_stm32_config_##index = {		\
	.pclken = { .bus = DT_INST_##index##_ST_STM32_DMA_CLOCK_BUS,	\
		    .enr = DT_INST_##index##_ST_STM32_DMA_CLOCK_BITS },	\
	.config_irq = dma_stm32_config_irq_##index,			\
	.base = DT_INST_##index##_ST_STM32_DMA_BASE_ADDRESS,		\
	.support_m2m = DT_INST_##index##_ST_STM32_DMA_ST_MEM2MEM,	\
};									\
									\
static struct dma_stm32_data dma_stm32_data_##index = {			\
	.max_streams = 0,						\
	.streams = NULL,						\
};									\
									\
DEVICE_AND_API_INIT(dma_##index, DT_INST_##index##_ST_STM32_DMA_LABEL,	\
		    &dma_stm32_init,					\
		    &dma_stm32_data_##index, &dma_stm32_config_##index,	\
		    POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,	\
		    &dma_funcs)

#define irq_func(chan)                                                  \
static void dma_stm32_irq_##chan(void *arg)				\
{									\
	dma_stm32_irq_handler(arg, chan);				\
}

#define IRQ_INIT(dma, chan)                                             \
do {									\
	if (!irq_is_enabled(DT_INST_##dma##_ST_STM32_DMA_IRQ_##chan)) {	\
		irq_connect_dynamic(DT_INST_##dma##_ST_STM32_DMA_IRQ_##chan,\
			DT_INST_##dma##_ST_STM32_DMA_IRQ_##chan##_PRIORITY,\
			dma_stm32_irq_handler, dev, 0);			\
		irq_enable(DT_INST_##dma##_ST_STM32_DMA_IRQ_##chan);	\
	}								\
	data->max_streams++;						\
} while (0)

#ifdef DT_INST_0_ST_STM32_DMA
DMA_INIT(0);

static void dma_stm32_config_irq_0(struct device *dev)
{
	struct dma_stm32_data *data = dev->driver_data;

	IRQ_INIT(0, 0);
	IRQ_INIT(0, 1);
	IRQ_INIT(0, 2);
	IRQ_INIT(0, 3);
	IRQ_INIT(0, 4);
#ifdef DT_INST_0_ST_STM32_DMA_IRQ_5
	IRQ_INIT(0, 5);
	IRQ_INIT(0, 6);
#ifdef DT_INST_0_ST_STM32_DMA_IRQ_7
	IRQ_INIT(0, 7);
#endif
#endif
/* Either 5 or 7 or 8 channels for DMA1 across all stm32 series. */
}
#endif

#ifdef DT_INST_1_ST_STM32_DMA
DMA_INIT(1);

static void dma_stm32_config_irq_1(struct device *dev)
{
	struct dma_stm32_data *data = dev->driver_data;

#ifdef DT_INST_1_ST_STM32_DMA_IRQ_0
	IRQ_INIT(1, 0);
	IRQ_INIT(1, 1);
	IRQ_INIT(1, 2);
	IRQ_INIT(1, 3);
	IRQ_INIT(1, 4);
#ifdef DT_INST_1_ST_STM32_DMA_IRQ_5
	IRQ_INIT(1, 5);
	IRQ_INIT(1, 6);
#ifdef DT_INST_1_ST_STM32_DMA_IRQ_7
	IRQ_INIT(1, 7);
#endif
#endif
#endif
/* Either 0 or 5 or 7 or 8 channels for DMA1 across all stm32 series. */
}
#endif
