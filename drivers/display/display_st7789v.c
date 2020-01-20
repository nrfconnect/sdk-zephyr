/*
 * Copyright (c) 2017 Jan Van Winkel <jan.van_winkel@dxplore.eu>
 * Copyright (c) 2019 Nordic Semiconductor ASA
 * Copyright (c) 2019 Marc Reilly
 * Copyright (c) 2019 PHYTEC Messtechnik GmbH
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "display_st7789v.h"

#include <device.h>
#include <drivers/spi.h>
#include <drivers/gpio.h>
#include <sys/byteorder.h>
#include <drivers/display.h>

#define LOG_LEVEL CONFIG_DISPLAY_LOG_LEVEL
#include <logging/log.h>
LOG_MODULE_REGISTER(display_st7789v);

#define ST7789V_CS_PIN		DT_INST_0_SITRONIX_ST7789V_CS_GPIOS_PIN
#define ST7789V_CMD_DATA_PIN	DT_INST_0_SITRONIX_ST7789V_CMD_DATA_GPIOS_PIN
#define ST7789V_RESET_PIN	DT_INST_0_SITRONIX_ST7789V_RESET_GPIOS_PIN

static u8_t st7789v_porch_param[] = DT_INST_0_SITRONIX_ST7789V_PORCH_PARAM;
static u8_t st7789v_cmd2en_param[] = DT_INST_0_SITRONIX_ST7789V_CMD2EN_PARAM;
static u8_t st7789v_pwctrl1_param[] = DT_INST_0_SITRONIX_ST7789V_PWCTRL1_PARAM;
static u8_t st7789v_pvgam_param[] = DT_INST_0_SITRONIX_ST7789V_PVGAM_PARAM;
static u8_t st7789v_nvgam_param[] = DT_INST_0_SITRONIX_ST7789V_NVGAM_PARAM;
static u8_t st7789v_ram_param[] = DT_INST_0_SITRONIX_ST7789V_RAM_PARAM;
static u8_t st7789v_rgb_param[] = DT_INST_0_SITRONIX_ST7789V_RGB_PARAM;

struct st7789v_data {
	struct device *spi_dev;
	struct spi_config spi_config;
#ifdef DT_INST_0_SITRONIX_ST7789V_CS_GPIOS_CONTROLLER
	struct spi_cs_control cs_ctrl;
#endif

#ifdef DT_INST_0_SITRONIX_ST7789V_RESET_GPIOS_CONTROLLER
	struct device *reset_gpio;
#endif
	struct device *cmd_data_gpio;

	u16_t height;
	u16_t width;
	u16_t x_offset;
	u16_t y_offset;
};

#ifdef CONFIG_ST7789V_RGB565
#define ST7789V_PIXEL_SIZE 2u
#else
#define ST7789V_PIXEL_SIZE 3u
#endif

static void st7789v_set_lcd_margins(struct st7789v_data *data,
			     u16_t x_offset, u16_t y_offset)
{
	data->x_offset = x_offset;
	data->y_offset = y_offset;
}

static void st7789v_set_cmd(struct st7789v_data *data, int is_cmd)
{
	gpio_pin_write(data->cmd_data_gpio, ST7789V_CMD_DATA_PIN, !is_cmd);
}

static void st7789v_transmit(struct st7789v_data *data, u8_t cmd,
		u8_t *tx_data, size_t tx_count)
{
	struct spi_buf tx_buf = { .buf = &cmd, .len = 1 };
	struct spi_buf_set tx_bufs = { .buffers = &tx_buf, .count = 1 };

	st7789v_set_cmd(data, true);
	spi_write(data->spi_dev, &data->spi_config, &tx_bufs);

	if (tx_data != NULL) {
		tx_buf.buf = tx_data;
		tx_buf.len = tx_count;
		st7789v_set_cmd(data, false);
		spi_write(data->spi_dev, &data->spi_config, &tx_bufs);
	}
}

static void st7789v_exit_sleep(struct st7789v_data *data)
{
	st7789v_transmit(data, ST7789V_CMD_SLEEP_OUT, NULL, 0);
	k_sleep(K_MSEC(120));
}

static void st7789v_reset_display(struct st7789v_data *data)
{
	LOG_DBG("Resetting display");
#ifdef DT_INST_0_SITRONIX_ST7789V_RESET_GPIOS_CONTROLLER
	gpio_pin_write(data->reset_gpio, ST7789V_RESET_PIN, 1);
	k_sleep(K_MSEC(1));
	gpio_pin_write(data->reset_gpio, ST7789V_RESET_PIN, 0);
	k_sleep(K_MSEC(6));
	gpio_pin_write(data->reset_gpio, ST7789V_RESET_PIN, 1);
	k_sleep(K_MSEC(20));
#else
	st7789v_transmit(p_st7789v, ST7789V_CMD_SW_RESET, NULL, 0);
	k_sleep(K_MSEC(5));
#endif
}

static int st7789v_blanking_on(const struct device *dev)
{
	struct st7789v_data *driver = (struct st7789v_data *)dev->driver_data;

	st7789v_transmit(driver, ST7789V_CMD_DISP_OFF, NULL, 0);
	return 0;
}

static int st7789v_blanking_off(const struct device *dev)
{
	struct st7789v_data *driver = (struct st7789v_data *)dev->driver_data;

	st7789v_transmit(driver, ST7789V_CMD_DISP_ON, NULL, 0);
	return 0;
}

static int st7789v_read(const struct device *dev,
			const u16_t x,
			const u16_t y,
			const struct display_buffer_descriptor *desc,
			void *buf)
{
	return -ENOTSUP;
}

static void st7789v_set_mem_area(struct st7789v_data *data, const u16_t x,
				 const u16_t y, const u16_t w, const u16_t h)
{
	u16_t spi_data[2];

	u16_t ram_x = x + data->x_offset;
	u16_t ram_y = y + data->y_offset;

	spi_data[0] = sys_cpu_to_be16(ram_x);
	spi_data[1] = sys_cpu_to_be16(ram_x + w - 1);
	st7789v_transmit(data, ST7789V_CMD_CASET, (u8_t *)&spi_data[0], 4);

	spi_data[0] = sys_cpu_to_be16(ram_y);
	spi_data[1] = sys_cpu_to_be16(ram_y + h - 1);
	st7789v_transmit(data, ST7789V_CMD_RASET, (u8_t *)&spi_data[0], 4);
}

static int st7789v_write(const struct device *dev,
			 const u16_t x,
			 const u16_t y,
			 const struct display_buffer_descriptor *desc,
			 const void *buf)
{
	struct st7789v_data *data = (struct st7789v_data *)dev->driver_data;
	const u8_t *write_data_start = (u8_t *) buf;
	struct spi_buf tx_buf;
	struct spi_buf_set tx_bufs;
	u16_t write_cnt;
	u16_t nbr_of_writes;
	u16_t write_h;

	__ASSERT(desc->width <= desc->pitch, "Pitch is smaller then width");
	__ASSERT((desc->pitch * ST7789V_PIXEL_SIZE * desc->height) <= desc->buf_size,
			"Input buffer to small");

	LOG_DBG("Writing %dx%d (w,h) @ %dx%d (x,y)",
			desc->width, desc->height, x, y);
	st7789v_set_mem_area(data, x, y, desc->width, desc->height);

	if (desc->pitch > desc->width) {
		write_h = 1U;
		nbr_of_writes = desc->height;
	} else {
		write_h = desc->height;
		nbr_of_writes = 1U;
	}

	st7789v_transmit(data, ST7789V_CMD_RAMWR,
			 (void *) write_data_start,
			 desc->width * ST7789V_PIXEL_SIZE * write_h);

	tx_bufs.buffers = &tx_buf;
	tx_bufs.count = 1;

	write_data_start += (desc->pitch * ST7789V_PIXEL_SIZE);
	for (write_cnt = 1U; write_cnt < nbr_of_writes; ++write_cnt) {
		tx_buf.buf = (void *)write_data_start;
		tx_buf.len = desc->width * ST7789V_PIXEL_SIZE * write_h;
		spi_write(data->spi_dev, &data->spi_config, &tx_bufs);
		write_data_start += (desc->pitch * ST7789V_PIXEL_SIZE);
	}

	return 0;
}

static void *st7789v_get_framebuffer(const struct device *dev)
{
	return NULL;
}

static int st7789v_set_brightness(const struct device *dev,
			   const u8_t brightness)
{
	return -ENOTSUP;
}

static int st7789v_set_contrast(const struct device *dev,
			 const u8_t contrast)
{
	return -ENOTSUP;
}

static void st7789v_get_capabilities(const struct device *dev,
			      struct display_capabilities *capabilities)
{
	struct st7789v_data *data = (struct st7789v_data *)dev->driver_data;

	memset(capabilities, 0, sizeof(struct display_capabilities));
	capabilities->x_resolution = data->width;
	capabilities->y_resolution = data->height;

#ifdef CONFIG_ST7789V_RGB565
	capabilities->supported_pixel_formats = PIXEL_FORMAT_RGB_565;
	capabilities->current_pixel_format = PIXEL_FORMAT_RGB_565;
#else
	capabilities->supported_pixel_formats = PIXEL_FORMAT_RGB_888;
	capabilities->current_pixel_format = PIXEL_FORMAT_RGB_888;
#endif
	capabilities->current_orientation = DISPLAY_ORIENTATION_NORMAL;
}

static int st7789v_set_pixel_format(const struct device *dev,
			     const enum display_pixel_format pixel_format)
{
#ifdef CONFIG_ST7789V_RGB565
	if (pixel_format == PIXEL_FORMAT_RGB_565) {
#else
	if (pixel_format == PIXEL_FORMAT_RGB_888) {
#endif
		return 0;
	}
	LOG_ERR("Pixel format change not implemented");
	return -ENOTSUP;
}

static int st7789v_set_orientation(const struct device *dev,
			    const enum display_orientation orientation)
{
	if (orientation == DISPLAY_ORIENTATION_NORMAL) {
		return 0;
	}
	LOG_ERR("Changing display orientation not implemented");
	return -ENOTSUP;
}

static void st7789v_lcd_init(struct st7789v_data *p_st7789v)
{
	u8_t tmp;

	st7789v_set_lcd_margins(p_st7789v, 0, 0);

	st7789v_transmit(p_st7789v, ST7789V_CMD_PORCTRL, st7789v_porch_param,
			 sizeof(st7789v_porch_param));

	st7789v_transmit(p_st7789v, ST7789V_CMD_CMD2EN, st7789v_cmd2en_param,
			 sizeof(st7789v_cmd2en_param));

	/* Digital Gamma Enable, default disabled */
	tmp = 0x00;
	st7789v_transmit(p_st7789v, ST7789V_CMD_DGMEN, &tmp, 1);

	/* Frame Rate Control in Normal Mode, default value */
	tmp = 0x0f;
	st7789v_transmit(p_st7789v, ST7789V_CMD_FRCTRL2, &tmp, 1);

	tmp = DT_INST_0_SITRONIX_ST7789V_GCTRL;
	st7789v_transmit(p_st7789v, ST7789V_CMD_GCTRL, &tmp, 1);

	tmp = DT_INST_0_SITRONIX_ST7789V_VCOM;
	st7789v_transmit(p_st7789v, ST7789V_CMD_VCOMS, &tmp, 1);

#if (defined(DT_INST_0_SITRONIX_ST7789V_VRHS) && \
	defined(DT_INST_0_SITRONIX_ST7789V_VDVS))
	tmp = 0x01;
	st7789v_transmit(p_st7789v, ST7789V_CMD_VDVVRHEN, &tmp, 1);

	tmp = DT_INST_0_SITRONIX_ST7789V_VRHS;
	st7789v_transmit(p_st7789v, ST7789V_CMD_VRH, &tmp, 1);

	tmp = DT_INST_0_SITRONIX_ST7789V_VDVS;
	st7789v_transmit(p_st7789v, ST7789V_CMD_VDS, &tmp, 1);
#endif

	st7789v_transmit(p_st7789v, ST7789V_CMD_PWCTRL1, st7789v_pwctrl1_param,
			 sizeof(st7789v_pwctrl1_param));

	/* Memory Data Access Control */
	tmp = DT_INST_0_SITRONIX_ST7789V_MDAC;
	st7789v_transmit(p_st7789v, ST7789V_CMD_MADCTL, &tmp, 1);

	/* Interface Pixel Format */
	tmp = DT_INST_0_SITRONIX_ST7789V_COLMOD;
	st7789v_transmit(p_st7789v, ST7789V_CMD_COLMOD, &tmp, 1);

	tmp = DT_INST_0_SITRONIX_ST7789V_LCM;
	st7789v_transmit(p_st7789v, ST7789V_CMD_LCMCTRL, &tmp, 1);

	tmp = DT_INST_0_SITRONIX_ST7789V_GAMMA;
	st7789v_transmit(p_st7789v, ST7789V_CMD_GAMSET, &tmp, 1);

	st7789v_transmit(p_st7789v, ST7789V_CMD_INV_ON, NULL, 0);

	st7789v_transmit(p_st7789v, ST7789V_CMD_PVGAMCTRL, st7789v_pvgam_param,
			 sizeof(st7789v_pvgam_param));

	st7789v_transmit(p_st7789v, ST7789V_CMD_NVGAMCTRL, st7789v_nvgam_param,
			 sizeof(st7789v_nvgam_param));

	st7789v_transmit(p_st7789v, ST7789V_CMD_RAMCTRL, st7789v_ram_param,
			 sizeof(st7789v_ram_param));

	st7789v_transmit(p_st7789v, ST7789V_CMD_RGBCTRL, st7789v_rgb_param,
			 sizeof(st7789v_rgb_param));
}

static int st7789v_init(struct device *dev)
{
	struct st7789v_data *data = (struct st7789v_data *)dev->driver_data;

	data->spi_dev = device_get_binding(DT_INST_0_SITRONIX_ST7789V_BUS_NAME);
	if (data->spi_dev == NULL) {
		LOG_ERR("Could not get SPI device for LCD");
		return -EPERM;
	}

	data->spi_config.frequency =
		DT_INST_0_SITRONIX_ST7789V_SPI_MAX_FREQUENCY;
	data->spi_config.operation = SPI_OP_MODE_MASTER | SPI_WORD_SET(8);
	data->spi_config.slave = DT_INST_0_SITRONIX_ST7789V_BASE_ADDRESS;

#ifdef DT_INST_0_SITRONIX_ST7789V_CS_GPIOS_CONTROLLER
	data->cs_ctrl.gpio_dev = device_get_binding(
			DT_INST_0_SITRONIX_ST7789V_CS_GPIOS_CONTROLLER);
	data->cs_ctrl.gpio_pin = DT_INST_0_SITRONIX_ST7789V_CS_GPIOS_PIN;
	data->cs_ctrl.delay = 0U;
	data->spi_config.cs = &(data->cs_ctrl);
#else
	data->spi_config.cs = NULL;
#endif

#ifdef DT_INST_0_SITRONIX_ST7789V_RESET_GPIOS_CONTROLLER
	data->reset_gpio = device_get_binding(
			DT_INST_0_SITRONIX_ST7789V_RESET_GPIOS_CONTROLLER);
	if (data->reset_gpio == NULL) {
		LOG_ERR("Could not get GPIO port for display reset");
		return -EPERM;
	}

	if (gpio_pin_configure(data->reset_gpio, ST7789V_RESET_PIN,
			       GPIO_DIR_OUT)) {
		LOG_ERR("Couldn't configure reset pin");
		return -EIO;
	}
#endif

	data->cmd_data_gpio = device_get_binding(
			DT_INST_0_SITRONIX_ST7789V_CMD_DATA_GPIOS_CONTROLLER);
	if (data->cmd_data_gpio == NULL) {
		LOG_ERR("Could not get GPIO port for cmd/DATA port");
		return -EPERM;
	}
	if (gpio_pin_configure(data->cmd_data_gpio, ST7789V_CMD_DATA_PIN,
			       GPIO_DIR_OUT)) {
		LOG_ERR("Couldn't configure cmd/DATA pin");
		return -EIO;
	}

	st7789v_reset_display(data);

	st7789v_blanking_on(dev);

	st7789v_lcd_init(data);

	st7789v_exit_sleep(data);

	return 0;
}

static const struct display_driver_api st7789v_api = {
	.blanking_on = st7789v_blanking_on,
	.blanking_off = st7789v_blanking_off,
	.write = st7789v_write,
	.read = st7789v_read,
	.get_framebuffer = st7789v_get_framebuffer,
	.set_brightness = st7789v_set_brightness,
	.set_contrast = st7789v_set_contrast,
	.get_capabilities = st7789v_get_capabilities,
	.set_pixel_format = st7789v_set_pixel_format,
	.set_orientation = st7789v_set_orientation,
};

static struct st7789v_data st7789v_data = {
	.width = DT_INST_0_SITRONIX_ST7789V_WIDTH,
	.height = DT_INST_0_SITRONIX_ST7789V_HEIGHT,
	.x_offset = DT_INST_0_SITRONIX_ST7789V_X_OFFSET,
	.y_offset = DT_INST_0_SITRONIX_ST7789V_Y_OFFSET,
};

DEVICE_AND_API_INIT(st7789v, DT_INST_0_SITRONIX_ST7789V_LABEL, &st7789v_init,
		    &st7789v_data, NULL, APPLICATION,
		    CONFIG_APPLICATION_INIT_PRIORITY, &st7789v_api);
