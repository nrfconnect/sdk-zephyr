/*
 * Copyright (c) 2018 PHYTEC Messtechnik GmbH
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_LEVEL CONFIG_DISPLAY_LOG_LEVEL
#include <logging/log.h>
LOG_MODULE_REGISTER(ssd1673);

#include <string.h>
#include <device.h>
#include <display.h>
#include <init.h>
#include <gpio.h>
#include <spi.h>

#include "ssd1673_regs.h"
#include <display/cfb.h>

#define EPD_PANEL_WIDTH			250
#define EPD_PANEL_HEIGHT		120
#define EPD_PANEL_NUMOF_COLUMS		250
#define EPD_PANEL_NUMOF_ROWS_PER_PAGE	8
#define EPD_PANEL_NUMOF_PAGES		(EPD_PANEL_HEIGHT / \
					 EPD_PANEL_NUMOF_ROWS_PER_PAGE)

#define SSD1673_PANEL_FIRST_PAGE	0
#define SSD1673_PANEL_LAST_PAGE		(EPD_PANEL_NUMOF_PAGES - 1)
#define SSD1673_PANEL_FIRST_GATE	0
#define SSD1673_PANEL_LAST_GATE		249

#define SSD1673_PIXELS_PER_BYTE		8

struct ssd1673_data {
	struct device *reset;
	struct device *dc;
	struct device *busy;
	struct device *spi_dev;
	struct spi_config spi_config;
#if defined(DT_SSD1673_SPI_GPIO_CS_DRV_NAME)
	struct spi_cs_control cs_ctrl;
#endif
	u8_t scan_mode;
};

#define SSD1673_LAST_LUT_INITIAL		0
#define SSD1673_LAST_LUT_DEFAULT		255
#define SSD1673_LUT_SIZE			29

static u8_t ssd1673_lut_initial[SSD1673_LUT_SIZE] = {
	0x22, 0x55, 0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x11,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x1E, 0x1E, 0x1E, 0x1E, 0x1E, 0x1E, 0x1E, 0x1E,
	0x01, 0x00, 0x00, 0x00, 0x00
};

static u8_t ssd1673_lut_default[SSD1673_LUT_SIZE] = {
	0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x0F, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00
};

static inline int ssd1673_write_cmd(struct ssd1673_data *driver,
				    u8_t cmd, u8_t *data, size_t len)
{
	int err;
	struct spi_buf buf = {.buf = &cmd, .len = sizeof(cmd)};
	struct spi_buf_set buf_set = {.buffers = &buf, .count = 1};

	gpio_pin_write(driver->dc, DT_SSD1673_DC_PIN, 0);
	err = spi_write(driver->spi_dev, &driver->spi_config, &buf_set);
	if (err < 0) {
		return err;
	}

	if (data != NULL) {
		buf.buf = data;
		buf.len = len;
		gpio_pin_write(driver->dc, DT_SSD1673_DC_PIN, 1);
		err = spi_write(driver->spi_dev, &driver->spi_config, &buf_set);
		if (err < 0) {
			return err;
		}
	}

	return 0;
}

static inline void ssd1673_busy_wait(struct ssd1673_data *driver)
{
	u32_t val = 0U;

	gpio_pin_read(driver->busy, DT_SSD1673_BUSY_PIN, &val);
	while (val) {
		k_busy_wait(SSD1673_BUSY_DELAY);
		gpio_pin_read(driver->busy, DT_SSD1673_BUSY_PIN, &val);
	}
}

static inline int ssd1673_set_ram_param(struct ssd1673_data *driver,
					u8_t sx, u8_t ex, u8_t sy, u8_t ey)
{
	int err;
	u8_t tmp[2];

	tmp[0] = sx; tmp[1] = ex;
	err = ssd1673_write_cmd(driver, SSD1673_CMD_RAM_XPOS_CTRL, tmp,
				sizeof(tmp));
	if (err < 0) {
		return err;
	}

	tmp[0] = sy; tmp[1] = ey;
	err = ssd1673_write_cmd(driver, SSD1673_CMD_RAM_YPOS_CTRL, tmp,
				sizeof(tmp));
	if (err < 0) {
		return err;
	}

	return 0;
}

static inline int ssd1673_set_ram_ptr(struct ssd1673_data *driver,
				       u8_t x, u8_t y)
{
	int err;

	err = ssd1673_write_cmd(driver, SSD1673_CMD_RAM_XPOS_CNTR, &x,
				sizeof(x));
	if (err < 0) {
		return err;
	}

	return ssd1673_write_cmd(driver, SSD1673_CMD_RAM_YPOS_CNTR, &y,
				 sizeof(y));
}

static void ssd1673_set_orientation_internall(struct ssd1673_data *driver)

{
#if DT_SSD1673_ORIENTATION_FLIPPED == 1
	driver->scan_mode = SSD1673_DATA_ENTRY_XIYDY;
#else
	driver->scan_mode = SSD1673_DATA_ENTRY_XDYIY;
#endif
}

int ssd1673_resume(const struct device *dev)
{
	struct ssd1673_data *driver = dev->driver_data;
	u8_t tmp;

	/*
	 * Uncomment for voltage measurement
	 * tmp = SSD1673_CTRL2_ENABLE_CLK;
	 * ssd1673_write_cmd(SSD1673_CMD_UPDATE_CTRL2, &tmp, sizeof(tmp));
	 * ssd1673_write_cmd(SSD1673_CMD_MASTER_ACTIVATION, NULL, 0);
	 */

	tmp = SSD1673_SLEEP_MODE_PON;
	return ssd1673_write_cmd(driver, SSD1673_CMD_SLEEP_MODE,
				 &tmp, sizeof(tmp));
}

static int ssd1673_suspend(const struct device *dev)
{
	struct ssd1673_data *driver = dev->driver_data;
	u8_t tmp = SSD1673_SLEEP_MODE_DSM;

	return ssd1673_write_cmd(driver, SSD1673_CMD_SLEEP_MODE,
				 &tmp, sizeof(tmp));
}

static int ssd1673_update_display(const struct device *dev)
{
	struct ssd1673_data *driver = dev->driver_data;
	u8_t tmp;
	int err;

	tmp = SSD1673_CTRL1_INITIAL_UPDATE_LH;
	err = ssd1673_write_cmd(driver, SSD1673_CMD_UPDATE_CTRL1,
				&tmp, sizeof(tmp));
	if (err < 0) {
		return err;
	}

	tmp = (SSD1673_CTRL2_ENABLE_CLK |
	       SSD1673_CTRL2_ENABLE_ANALOG |
	       SSD1673_CTRL2_TO_PATTERN |
	       SSD1673_CTRL2_DISABLE_ANALOG |
	       SSD1673_CTRL2_DISABLE_CLK);
	err = ssd1673_write_cmd(driver, SSD1673_CMD_UPDATE_CTRL2, &tmp,
				sizeof(tmp));
	if (err < 0) {
		return err;
	}

	return ssd1673_write_cmd(driver, SSD1673_CMD_MASTER_ACTIVATION,
				 NULL, 0);
}

static int ssd1673_write(const struct device *dev, const u16_t x,
			 const u16_t y,
			 const struct display_buffer_descriptor *desc,
			 const void *buf)
{
	struct ssd1673_data *driver = dev->driver_data;
	int err;
	u8_t x_start;
	u8_t x_end;
	u8_t y_start;
	u8_t y_end;

	if (desc->pitch < desc->width) {
		LOG_ERR("Pitch is smaller then width");
		return -EINVAL;
	}

	if (buf == NULL || desc->buf_size == 0) {
		LOG_ERR("Display buffer is not available");
		return -EINVAL;
	}

	if (desc->pitch > desc->width) {
		LOG_ERR("Unsupported mode");
		return -ENOTSUP;
	}

	if ((y + desc->height) > EPD_PANEL_HEIGHT) {
		LOG_ERR("Buffer out of bounds (height)");
		return -EINVAL;
	}

	if ((x + desc->width) > EPD_PANEL_WIDTH) {
		LOG_ERR("Buffer out of bounds (width)");
		return -EINVAL;
	}

	if ((desc->height % EPD_PANEL_NUMOF_ROWS_PER_PAGE) != 0) {
		LOG_ERR("Buffer height not multiple of %d",
				EPD_PANEL_NUMOF_ROWS_PER_PAGE);
		return -EINVAL;
	}

	if ((y % EPD_PANEL_NUMOF_ROWS_PER_PAGE) != 0) {
		LOG_ERR("Y coordinate not multiple of %d",
				EPD_PANEL_NUMOF_ROWS_PER_PAGE);
		return -EINVAL;
	}

	switch (driver->scan_mode) {
	case SSD1673_DATA_ENTRY_XIYDY:
		x_start = y / SSD1673_PIXELS_PER_BYTE;
		x_end = (y + desc->height - 1) / SSD1673_PIXELS_PER_BYTE;
		y_start = (x + desc->width - 1);
		y_end = x;
		break;

	case SSD1673_DATA_ENTRY_XDYIY:
		x_start = (EPD_PANEL_HEIGHT - 1 - y) / SSD1673_PIXELS_PER_BYTE;
		x_end = (EPD_PANEL_HEIGHT - 1 - (y + desc->height - 1)) /
			SSD1673_PIXELS_PER_BYTE;
		y_start = x;
		y_end = (x + desc->width - 1);
		break;
	default:
		return -EINVAL;
	}

	ssd1673_busy_wait(driver);

	err = ssd1673_write_cmd(driver, SSD1673_CMD_ENTRY_MODE,
				&driver->scan_mode, sizeof(driver->scan_mode));
	if (err < 0) {
		return err;
	}

	err = ssd1673_set_ram_param(driver, x_start, x_end, y_start, y_end);
	if (err < 0) {
		return err;
	}

	err = ssd1673_set_ram_ptr(driver, x_start, y_start);
	if (err < 0) {
		return err;
	}

	err = ssd1673_write_cmd(driver, SSD1673_CMD_WRITE_RAM, (u8_t *)buf,
				desc->buf_size);
	if (err < 0) {
		return err;
	}

	return ssd1673_update_display(dev);
}

static int ssd1673_read(const struct device *dev, const u16_t x,
			const u16_t y,
			const struct display_buffer_descriptor *desc,
			void *buf)
{
	LOG_ERR("not supported");
	return -ENOTSUP;
}

static void *ssd1673_get_framebuffer(const struct device *dev)
{
	LOG_ERR("not supported");
	return NULL;
}

static int ssd1673_set_brightness(const struct device *dev,
				  const u8_t brightness)
{
	LOG_WRN("not supported");
	return -ENOTSUP;
}

static int ssd1673_set_contrast(const struct device *dev, u8_t contrast)
{
	LOG_WRN("not supported");
	return -ENOTSUP;
}

static void ssd1673_get_capabilities(const struct device *dev,
				     struct display_capabilities *caps)
{
	memset(caps, 0, sizeof(struct display_capabilities));
	caps->x_resolution = EPD_PANEL_WIDTH;
	caps->y_resolution = EPD_PANEL_HEIGHT;
	caps->supported_pixel_formats = PIXEL_FORMAT_MONO10;
	caps->current_pixel_format = PIXEL_FORMAT_MONO10;
	caps->screen_info = SCREEN_INFO_MONO_VTILED |
			    SCREEN_INFO_MONO_MSB_FIRST |
			    SCREEN_INFO_EPD |
			    SCREEN_INFO_DOUBLE_BUFFER;
}

static int ssd1673_set_orientation(const struct device *dev,
				   const enum display_orientation
				   orientation)
{
	LOG_ERR("Unsupported");
	return -ENOTSUP;
}

static int ssd1673_set_pixel_format(const struct device *dev,
				    const enum display_pixel_format pf)
{
	if (pf == PIXEL_FORMAT_MONO10) {
		return 0;
	}

	LOG_ERR("not supported");
	return -ENOTSUP;
}

static int ssd1673_clear_and_write_buffer(struct device *dev)
{
	int err;
	u8_t clear_page[SSD1673_RAM_YRES];
	u8_t page;
	struct spi_buf sbuf;
	struct spi_buf_set buf_set = {.buffers = &sbuf, .count = 1};
	struct ssd1673_data *driver = dev->driver_data;
	u8_t tmp;

	tmp = SSD1673_DATA_ENTRY_XIYDY;
	err = ssd1673_write_cmd(driver, SSD1673_CMD_ENTRY_MODE, &tmp, 1);
	if (err < 0) {
		return err;
	}

	err = ssd1673_set_ram_param(driver, SSD1673_PANEL_FIRST_PAGE,
				SSD1673_PANEL_LAST_PAGE + 1,
				SSD1673_PANEL_LAST_GATE,
				SSD1673_PANEL_FIRST_GATE);
	if (err < 0) {
		return err;
	}

	err = ssd1673_set_ram_ptr(driver, SSD1673_PANEL_FIRST_PAGE,
				SSD1673_PANEL_LAST_GATE);
	if (err < 0) {
		return err;
	}

	gpio_pin_write(driver->dc, DT_SSD1673_DC_PIN, 0);

	tmp = SSD1673_CMD_WRITE_RAM;
	sbuf.buf = &tmp;
	sbuf.len = 1;
	err = spi_write(driver->spi_dev, &driver->spi_config, &buf_set);
	if (err < 0) {
		return err;
	}

	gpio_pin_write(driver->dc, DT_SSD1673_DC_PIN, 1);

	memset(clear_page, 0xff, sizeof(clear_page));
	sbuf.buf = clear_page;
	sbuf.len = sizeof(clear_page);
	for (page = 0; page <= (SSD1673_PANEL_LAST_PAGE + 1); ++page) {
		err = spi_write(driver->spi_dev, &driver->spi_config, &buf_set);
		if (err < 0) {
			return err;
		}
	}

	return ssd1673_update_display(dev);
}

static int ssd1673_controller_init(struct device *dev)
{
	int err;
	u8_t tmp[3];
	struct ssd1673_data *driver = dev->driver_data;

	LOG_DBG("");

	gpio_pin_write(driver->reset, DT_SSD1673_RESET_PIN, 0);
	k_sleep(SSD1673_RESET_DELAY);
	gpio_pin_write(driver->reset, DT_SSD1673_RESET_PIN, 1);
	k_sleep(SSD1673_RESET_DELAY);
	ssd1673_busy_wait(driver);

	err = ssd1673_write_cmd(driver, SSD1673_CMD_SW_RESET, NULL, 0);
	if (err < 0) {
		return err;
	}
	ssd1673_busy_wait(driver);

	tmp[0] = (SSD1673_RAM_YRES - 1);
	tmp[1] = 0U;
	err = ssd1673_write_cmd(driver, SSD1673_CMD_GDO_CTRL, tmp, 2);
	if (err < 0) {
		return err;
	}

	tmp[0] = SSD1673_VAL_GDV_CTRL_A;
	tmp[1] = SSD1673_VAL_GDV_CTRL_B;
	err = ssd1673_write_cmd(driver, SSD1673_CMD_GDV_CTRL, tmp, 2);
	if (err < 0) {
		return err;
	}

	tmp[0] = SSD1673_VAL_SDV_CTRL;
	err = ssd1673_write_cmd(driver, SSD1673_CMD_SDV_CTRL, tmp, 1);
	if (err < 0) {
		return err;
	}

	tmp[0] = SSD1673_VAL_VCOM_VOLTAGE;
	err = ssd1673_write_cmd(driver, SSD1673_CMD_VCOM_VOLTAGE, tmp, 1);
	if (err < 0) {
		return err;
	}

	tmp[0] = SSD1673_VAL_DUMMY_LINE;
	err = ssd1673_write_cmd(driver, SSD1673_CMD_DUMMY_LINE, tmp, 1);
	if (err < 0) {
		return err;
	}

	tmp[0] = SSD1673_VAL_GATE_LWIDTH;
	err = ssd1673_write_cmd(driver, SSD1673_CMD_GATE_LINE_WIDTH, tmp, 1);
	if (err < 0) {
		return err;
	}

	ssd1673_set_orientation_internall(driver);

	err = ssd1673_write_cmd(driver, SSD1673_CMD_UPDATE_LUT,
				ssd1673_lut_initial,
				sizeof(ssd1673_lut_initial));
	if (err < 0) {
		return err;
	}

	err = ssd1673_clear_and_write_buffer(dev);
	if (err < 0) {
		return err;
	}

	ssd1673_busy_wait(driver);

	err = ssd1673_write_cmd(driver, SSD1673_CMD_UPDATE_LUT,
				ssd1673_lut_default,
				sizeof(ssd1673_lut_default));
	if (err < 0) {
		return err;
	}

	return ssd1673_clear_and_write_buffer(dev);
}

static int ssd1673_init(struct device *dev)
{
	struct ssd1673_data *driver = dev->driver_data;

	LOG_DBG("");

	driver->spi_dev = device_get_binding(DT_SSD1673_SPI_DEV_NAME);
	if (driver->spi_dev == NULL) {
		LOG_ERR("Could not get SPI device for SSD1673");
		return -EIO;
	}

	driver->spi_config.frequency = DT_SSD1673_SPI_FREQ;
	driver->spi_config.operation = SPI_OP_MODE_MASTER | SPI_WORD_SET(8);
	driver->spi_config.slave = DT_SSD1673_SPI_SLAVE_NUMBER;
	driver->spi_config.cs = NULL;

	driver->reset = device_get_binding(DT_SSD1673_RESET_GPIO_PORT_NAME);
	if (driver->reset == NULL) {
		LOG_ERR("Could not get GPIO port for SSD1673 reset");
		return -EIO;
	}

	gpio_pin_configure(driver->reset, DT_SSD1673_RESET_PIN,
			   GPIO_DIR_OUT);

	driver->dc = device_get_binding(DT_SSD1673_DC_GPIO_PORT_NAME);
	if (driver->dc == NULL) {
		LOG_ERR("Could not get GPIO port for SSD1673 DC signal");
		return -EIO;
	}

	gpio_pin_configure(driver->dc, DT_SSD1673_DC_PIN,
			   GPIO_DIR_OUT);

	driver->busy = device_get_binding(DT_SSD1673_BUSY_GPIO_PORT_NAME);
	if (driver->busy == NULL) {
		LOG_ERR("Could not get GPIO port for SSD1673 busy signal");
		return -EIO;
	}

	gpio_pin_configure(driver->busy, DT_SSD1673_BUSY_PIN,
			   GPIO_DIR_IN);

#if defined(DT_SSD1673_SPI_GPIO_CS_DRV_NAME)
	driver->cs_ctrl.gpio_dev = device_get_binding(
		DT_SSD1673_SPI_GPIO_CS_DRV_NAME);
	if (!driver->cs_ctrl.gpio_dev) {
		LOG_ERR("Unable to get SPI GPIO CS device");
		return -EIO;
	}

	driver->cs_ctrl.gpio_pin = DT_SSD1673_SPI_GPIO_CS_PIN;
	driver->cs_ctrl.delay = 0;
	driver->spi_config.cs = &driver->cs_ctrl;
#endif

	return ssd1673_controller_init(dev);
}

static struct ssd1673_data ssd1673_driver;

static struct display_driver_api ssd1673_driver_api = {
	.blanking_on = ssd1673_resume,
	.blanking_off = ssd1673_suspend,
	.write = ssd1673_write,
	.read = ssd1673_read,
	.get_framebuffer = ssd1673_get_framebuffer,
	.set_brightness = ssd1673_set_brightness,
	.set_contrast = ssd1673_set_contrast,
	.get_capabilities = ssd1673_get_capabilities,
	.set_pixel_format = ssd1673_set_pixel_format,
	.set_orientation = ssd1673_set_orientation,
};


DEVICE_AND_API_INIT(ssd1673, DT_SSD1673_DEV_NAME, ssd1673_init,
		    &ssd1673_driver, NULL,
		    POST_KERNEL, CONFIG_APPLICATION_INIT_PRIORITY,
		    &ssd1673_driver_api);
