/*
 * Copyright (c) 2019, NXP
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <display.h>
#include <fsl_elcdif.h>

#ifdef CONFIG_HAS_MCUX_CACHE
#include <fsl_cache.h>
#endif

#include <logging/log.h>

LOG_MODULE_REGISTER(display_mcux_elcdif, CONFIG_DISPLAY_LOG_LEVEL);

K_MEM_POOL_DEFINE(mcux_elcdif_pool,
		  CONFIG_MCUX_ELCDIF_POOL_BLOCK_MIN,
		  CONFIG_MCUX_ELCDIF_POOL_BLOCK_MAX,
		  CONFIG_MCUX_ELCDIF_POOL_BLOCK_NUM,
		  CONFIG_MCUX_ELCDIF_POOL_BLOCK_ALIGN);

struct mcux_elcdif_config {
	LCDIF_Type *base;
	void (*irq_config_func)(struct device *dev);
	elcdif_rgb_mode_config_t rgb_mode;
	enum display_pixel_format pixel_format;
	u8_t bits_per_pixel;
};

struct mcux_elcdif_data {
	struct k_mem_block fb[2];
	struct k_sem sem;
	size_t pixel_bytes;
	size_t fb_bytes;
	u8_t write_idx;
};

static int mcux_elcdif_write(const struct device *dev, const u16_t x,
			     const u16_t y,
			     const struct display_buffer_descriptor *desc,
			     const void *buf)
{
	const struct mcux_elcdif_config *config = dev->config->config_info;
	struct mcux_elcdif_data *data = dev->driver_data;

	u8_t write_idx = data->write_idx;
	u8_t read_idx = !write_idx;

	int h_idx;
	const u8_t *src;
	u8_t *dst;

	__ASSERT((data->pixel_bytes * desc->pitch * desc->height) <=
		 desc->buf_size, "Input buffer too small");

	LOG_DBG("W=%d, H=%d, @%d,%d", desc->width, desc->height, x, y);

	k_sem_take(&data->sem, K_FOREVER);

	memcpy(data->fb[write_idx].data, data->fb[read_idx].data,
	       data->fb_bytes);

	src = buf;
	dst = data->fb[data->write_idx].data;
	dst += data->pixel_bytes * (y * config->rgb_mode.panelWidth + x);

	for (h_idx = 0; h_idx < desc->height; h_idx++) {
		memcpy(dst, src, data->pixel_bytes * desc->width);
		src += data->pixel_bytes * desc->pitch;
		dst += data->pixel_bytes * config->rgb_mode.panelWidth;
	}

#ifdef CONFIG_HAS_MCUX_CACHE
	DCACHE_CleanByRange((uint32_t) data->fb[write_idx].data,
			    data->fb_bytes);
#endif

	ELCDIF_SetNextBufferAddr(config->base,
				 (uint32_t) data->fb[write_idx].data);

	data->write_idx = read_idx;

	return 0;
}

static int mcux_elcdif_read(const struct device *dev, const u16_t x,
			    const u16_t y,
			    const struct display_buffer_descriptor *desc,
			    void *buf)
{
	LOG_ERR("Read not implemented");
	return -ENOTSUP;
}

static void *mcux_elcdif_get_framebuffer(const struct device *dev)
{
	LOG_ERR("Direct framebuffer access not implemented");
	return NULL;
}

static int mcux_elcdif_display_blanking_off(const struct device *dev)
{
	LOG_ERR("Display blanking control not implemented");
	return -ENOTSUP;
}

static int mcux_elcdif_display_blanking_on(const struct device *dev)
{
	LOG_ERR("Display blanking control not implemented");
	return -ENOTSUP;
}

static int mcux_elcdif_set_brightness(const struct device *dev,
				      const u8_t brightness)
{
	LOG_WRN("Set brightness not implemented");
	return -ENOTSUP;
}

static int mcux_elcdif_set_contrast(const struct device *dev,
				    const u8_t contrast)
{
	LOG_ERR("Set contrast not implemented");
	return -ENOTSUP;
}

static int mcux_elcdif_set_pixel_format(const struct device *dev,
					const enum display_pixel_format
					pixel_format)
{
	const struct mcux_elcdif_config *config = dev->config->config_info;

	if (pixel_format == config->pixel_format) {
		return 0;
	}
	LOG_ERR("Pixel format change not implemented");
	return -ENOTSUP;
}

static int mcux_elcdif_set_orientation(const struct device *dev,
		const enum display_orientation orientation)
{
	if (orientation == DISPLAY_ORIENTATION_NORMAL) {
		return 0;
	}
	LOG_ERR("Changing display orientation not implemented");
	return -ENOTSUP;
}

static void mcux_elcdif_get_capabilities(const struct device *dev,
		struct display_capabilities *capabilities)
{
	const struct mcux_elcdif_config *config = dev->config->config_info;

	memset(capabilities, 0, sizeof(struct display_capabilities));
	capabilities->x_resolution = config->rgb_mode.panelWidth;
	capabilities->y_resolution = config->rgb_mode.panelHeight;
	capabilities->supported_pixel_formats = config->pixel_format;
	capabilities->current_pixel_format = config->pixel_format;
	capabilities->current_orientation = DISPLAY_ORIENTATION_NORMAL;
}

static void mcux_elcdif_isr(void *arg)
{
	struct device *dev = (struct device *)arg;
	const struct mcux_elcdif_config *config = dev->config->config_info;
	struct mcux_elcdif_data *data = dev->driver_data;
	u32_t status;

	status = ELCDIF_GetInterruptStatus(config->base);
	ELCDIF_ClearInterruptStatus(config->base, status);

	k_sem_give(&data->sem);
}

static int mcux_elcdif_init(struct device *dev)
{
	const struct mcux_elcdif_config *config = dev->config->config_info;
	struct mcux_elcdif_data *data = dev->driver_data;
	int i;

	elcdif_rgb_mode_config_t rgb_mode = config->rgb_mode;

	data->pixel_bytes = config->bits_per_pixel / 8U;
	data->fb_bytes = data->pixel_bytes *
			 rgb_mode.panelWidth * rgb_mode.panelHeight;
	data->write_idx = 1U;

	for (i = 0; i < ARRAY_SIZE(data->fb); i++) {
		if (k_mem_pool_alloc(&mcux_elcdif_pool, &data->fb[i],
				     data->fb_bytes, K_NO_WAIT) != 0) {
			LOG_ERR("Could not allocate frame buffer %d", i);
			return -ENOMEM;
		}
		memset(data->fb[i].data, 0, data->fb_bytes);
	}
	rgb_mode.bufferAddr = (uint32_t) data->fb[0].data;

	k_sem_init(&data->sem, 1, 1);

	config->irq_config_func(dev);

	ELCDIF_RgbModeInit(config->base, &rgb_mode);
	ELCDIF_EnableInterrupts(config->base,
				kELCDIF_CurFrameDoneInterruptEnable);
	ELCDIF_RgbModeStart(config->base);

	return 0;
}

static const struct display_driver_api mcux_elcdif_api = {
	.blanking_on = mcux_elcdif_display_blanking_on,
	.blanking_off = mcux_elcdif_display_blanking_off,
	.write = mcux_elcdif_write,
	.read = mcux_elcdif_read,
	.get_framebuffer = mcux_elcdif_get_framebuffer,
	.set_brightness = mcux_elcdif_set_brightness,
	.set_contrast = mcux_elcdif_set_contrast,
	.get_capabilities = mcux_elcdif_get_capabilities,
	.set_pixel_format = mcux_elcdif_set_pixel_format,
	.set_orientation = mcux_elcdif_set_orientation,
};

static void mcux_elcdif_config_func_1(struct device *dev);

static struct mcux_elcdif_config mcux_elcdif_config_1 = {
	.base = (LCDIF_Type *) DT_FSL_IMX6SX_LCDIF_0_BASE_ADDRESS,
	.irq_config_func = mcux_elcdif_config_func_1,
#ifdef CONFIG_MCUX_ELCDIF_PANEL_RK043FN02H
	.rgb_mode = {
		.panelWidth = 480,
		.panelHeight = 272,
		.hsw = 41,
		.hfp = 4,
		.hbp = 8,
		.vsw = 10,
		.vfp = 4,
		.vbp = 2,
		.polarityFlags = kELCDIF_DataEnableActiveHigh |
				 kELCDIF_VsyncActiveLow |
				 kELCDIF_HsyncActiveLow |
				 kELCDIF_DriveDataOnRisingClkEdge,
		.pixelFormat = kELCDIF_PixelFormatRGB565,
		.dataBus = kELCDIF_DataBus16Bit,
	},
	.pixel_format = PIXEL_FORMAT_RGB_565,
	.bits_per_pixel = 16,
#endif
};

static struct mcux_elcdif_data mcux_elcdif_data_1;

DEVICE_AND_API_INIT(mcux_elcdif_1, DT_FSL_IMX6SX_LCDIF_0_LABEL,
		    &mcux_elcdif_init,
		    &mcux_elcdif_data_1, &mcux_elcdif_config_1,
		    POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEVICE,
		    &mcux_elcdif_api);

static void mcux_elcdif_config_func_1(struct device *dev)
{
	IRQ_CONNECT(DT_FSL_IMX6SX_LCDIF_0_IRQ_0,
		    DT_FSL_IMX6SX_LCDIF_0_IRQ_0_PRIORITY,
		    mcux_elcdif_isr, DEVICE_GET(mcux_elcdif_1), 0);

	irq_enable(DT_FSL_IMX6SX_LCDIF_0_IRQ_0);
}
