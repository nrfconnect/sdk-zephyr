/*
 * Copyright (c) 2018 PHYTEC Messtechnik GmbH
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_LEVEL CONFIG_DISPLAY_LOG_LEVEL
#include <logging/log.h>
LOG_MODULE_REGISTER(ssd1306);

#include <string.h>
#include <device.h>
#include <init.h>
#include <gpio.h>
#include <i2c.h>

#include "ssd1306_regs.h"
#include <display/cfb.h>

#if DT_SSD1306_PANEL_SEGMENT_REMAP == 1
#define SSD1306_PANEL_SEGMENT_REMAP	true
#else
#define SSD1306_PANEL_SEGMENT_REMAP	false
#endif

#if DT_SSD1306_PANEL_COM_INVDIR == 1
#define SSD1306_PANEL_COM_INVDIR	true
#else
#define SSD1306_PANEL_COM_INVDIR	false
#endif

#define SSD1306_PANEL_NUMOF_PAGES	(DT_SSD1306_PANEL_HEIGHT / 8)
#define SSD1306_CLOCK_DIV_RATIO		0x0
#define SSD1306_CLOCK_FREQUENCY		0x8
#define SSD1306_PANEL_MUX_RATIO		63
#define SSD1306_PANEL_VCOM_DESEL_LEVEL	0x20
#define SSD1306_PANEL_PUMP_VOLTAGE	SSD1306_SET_PUMP_VOLTAGE_90

#if defined(CONFIG_SSD1306_SH1106_COMPATIBLE)
#define SSD1306_PANEL_NUMOF_COLUMS	132
#else
#define SSD1306_PANEL_NUMOF_COLUMS	128
#endif

#ifndef SSD1306_ADDRESSING_MODE
#define SSD1306_ADDRESSING_MODE		(SSD1306_SET_MEM_ADDRESSING_HORIZONTAL)
#endif

struct ssd1306_data {
	struct device *i2c;
	u8_t contrast;
	u8_t scan_mode;
};

static inline int ssd1306_reg_read(struct ssd1306_data *driver,
				   u8_t reg, u8_t * const val)
{
	return i2c_reg_read_byte(driver->i2c, DT_SSD1306_I2C_ADDR,
				 reg, val);
}

static inline int ssd1306_reg_write(struct ssd1306_data *driver,
				    u8_t reg, u8_t val)
{
	return i2c_reg_write_byte(driver->i2c, DT_SSD1306_I2C_ADDR,
				  reg, val);
}

static inline int ssd1306_reg_update(struct ssd1306_data *driver, u8_t reg,
				     u8_t mask, u8_t val)
{
	return i2c_reg_update_byte(driver->i2c, DT_SSD1306_I2C_ADDR,
				   reg, mask, val);
}

static inline int ssd1306_set_panel_orientation(struct device *dev)
{
	struct ssd1306_data *driver = dev->driver_data;
	u8_t cmd_buf[] = {
		SSD1306_CONTROL_BYTE_CMD,
		(SSD1306_PANEL_SEGMENT_REMAP ?
		 SSD1306_SET_SEGMENT_MAP_REMAPED :
		 SSD1306_SET_SEGMENT_MAP_NORMAL),
		SSD1306_CONTROL_LAST_BYTE_CMD,
		(SSD1306_PANEL_COM_INVDIR ?
		 SSD1306_SET_COM_OUTPUT_SCAN_FLIPPED :
		 SSD1306_SET_COM_OUTPUT_SCAN_NORMAL)
	};

	return i2c_write(driver->i2c, cmd_buf, sizeof(cmd_buf),
			 DT_SSD1306_I2C_ADDR);
}

static inline int ssd1306_set_timing_setting(struct device *dev)
{
	struct ssd1306_data *driver = dev->driver_data;
	u8_t cmd_buf[] = {
		SSD1306_CONTROL_BYTE_CMD,
		SSD1306_SET_CLOCK_DIV_RATIO,
		SSD1306_CONTROL_BYTE_CMD,
		(SSD1306_CLOCK_FREQUENCY << 4) | SSD1306_CLOCK_DIV_RATIO,
		SSD1306_CONTROL_BYTE_CMD,
		SSD1306_SET_CHARGE_PERIOD,
		SSD1306_CONTROL_BYTE_CMD,
		DT_SSD1306_PANEL_PRECHARGE_PERIOD,
		SSD1306_CONTROL_BYTE_CMD,
		SSD1306_SET_VCOM_DESELECT_LEVEL,
		SSD1306_CONTROL_LAST_BYTE_CMD,
		SSD1306_PANEL_VCOM_DESEL_LEVEL
	};

	return i2c_write(driver->i2c, cmd_buf, sizeof(cmd_buf),
			 DT_SSD1306_I2C_ADDR);
}

static inline int ssd1306_set_hardware_config(struct device *dev)
{
	struct ssd1306_data *driver = dev->driver_data;
	u8_t cmd_buf[] = {
		SSD1306_CONTROL_BYTE_CMD,
		SSD1306_SET_START_LINE,
		SSD1306_CONTROL_BYTE_CMD,
		SSD1306_SET_DISPLAY_OFFSET,
		SSD1306_CONTROL_BYTE_CMD,
		DT_SSD1306_PANEL_DISPLAY_OFFSET,
		SSD1306_CONTROL_BYTE_CMD,
		SSD1306_SET_PADS_HW_CONFIG,
		SSD1306_CONTROL_BYTE_CMD,
		SSD1306_SET_PADS_HW_ALTERNATIVE,
		SSD1306_CONTROL_BYTE_CMD,
		SSD1306_SET_MULTIPLEX_RATIO,
		SSD1306_CONTROL_LAST_BYTE_CMD,
		SSD1306_PANEL_MUX_RATIO
	};

	return i2c_write(driver->i2c, cmd_buf, sizeof(cmd_buf),
			 DT_SSD1306_I2C_ADDR);
}

static inline int ssd1306_set_charge_pump(const struct device *dev)
{
	struct ssd1306_data *driver = dev->driver_data;
	u8_t cmd_buf[] = {
#if defined(CONFIG_SSD1306_DEFAULT)
		SSD1306_CONTROL_BYTE_CMD,
		SSD1306_SET_CHARGE_PUMP_ON,
		SSD1306_CONTROL_BYTE_CMD,
		SSD1306_SET_CHARGE_PUMP_ON_ENABLED,
#endif
#if defined(CONFIG_SSD1306_SH1106_COMPATIBLE)
		SSD1306_CONTROL_BYTE_CMD,
		SH1106_SET_DCDC_MODE,
		SSD1306_CONTROL_BYTE_CMD,
		SH1106_SET_DCDC_ENABLED,
#endif
		SSD1306_CONTROL_LAST_BYTE_CMD,
		SSD1306_PANEL_PUMP_VOLTAGE,
	};

	return i2c_write(driver->i2c, cmd_buf, sizeof(cmd_buf),
			 DT_SSD1306_I2C_ADDR);
}

int ssd1306_resume(const struct device *dev)
{
	struct ssd1306_data *driver = dev->driver_data;
	/* set display on */
	return ssd1306_reg_write(driver, SSD1306_CONTROL_LAST_BYTE_CMD,
				 SSD1306_DISPLAY_ON);
}

int ssd1306_suspend(const struct device *dev)
{
	struct ssd1306_data *driver = dev->driver_data;
	/* set display on */
	return ssd1306_reg_write(driver, SSD1306_CONTROL_LAST_BYTE_CMD,
				 SSD1306_DISPLAY_OFF);
}

int ssd1306_write_page(struct device *dev, u8_t page, void * const data,
		       size_t length)
{
	struct ssd1306_data *driver = dev->driver_data;
	u8_t cmd_buf[] = {
#ifdef OLED_PANEL_CONTROLLER_SSD1306
		SSD1306_CONTROL_BYTE_CMD,
		SSD1306_SET_MEM_ADDRESSING_MODE,
		SSD1306_CONTROL_BYTE_CMD,
		SSD1306_SET_MEM_ADDRESSING_PAGE,
#endif
		SSD1306_CONTROL_BYTE_CMD,
		SSD1306_SET_LOWER_COL_ADDRESS |
		(DT_SSD1306_PANEL_FIRST_SEG &
		 SSD1306_SET_LOWER_COL_ADDRESS_MASK),
		SSD1306_CONTROL_BYTE_CMD,
		SSD1306_SET_HIGHER_COL_ADDRESS |
		((DT_SSD1306_PANEL_FIRST_SEG  >> 4) &
		 SSD1306_SET_LOWER_COL_ADDRESS_MASK),
		SSD1306_CONTROL_LAST_BYTE_CMD,
		SSD1306_SET_PAGE_START_ADDRESS | page
	};

	if (page >= SSD1306_PANEL_NUMOF_PAGES) {
		return -1;
	}

	if (length > SSD1306_PANEL_NUMOF_COLUMS) {
		return -1;
	}

	if (i2c_write(driver->i2c, cmd_buf, sizeof(cmd_buf),
		      DT_SSD1306_I2C_ADDR)) {
		return -1;
	}

	return i2c_burst_write(driver->i2c, DT_SSD1306_I2C_ADDR,
			       SSD1306_CONTROL_LAST_BYTE_DATA,
			       data, length);
}

int ssd1306_write(const struct device *dev, const u16_t x, const u16_t y,
		  const struct display_buffer_descriptor *desc,
		  const void *buf)
{
	if (desc->pitch < desc->width) {
		LOG_ERR("Pitch is smaller then width");
		return -1;
	}

	if (buf == NULL || desc->buf_size == 0) {
		LOG_ERR("Display buffer is not available");
		return -1;
	}

	if (desc->pitch > desc->width) {
		LOG_ERR("Unsupported mode");
		return -1;
	}

	if (x != 0 && y != 0) {
		LOG_ERR("Unsupported origin");
		return -1;
	}

#if defined(CONFIG_SSD1306_DEFAULT)
	struct ssd1306_data *driver = dev->driver_data;

	u8_t cmd_buf[] = {
		SSD1306_CONTROL_BYTE_CMD,
		SSD1306_SET_MEM_ADDRESSING_MODE,
		SSD1306_CONTROL_BYTE_CMD,
		SSD1306_ADDRESSING_MODE,
		SSD1306_CONTROL_BYTE_CMD,
		SSD1306_SET_COLUMN_ADDRESS,
		SSD1306_CONTROL_BYTE_CMD,
		0,
		SSD1306_CONTROL_BYTE_CMD,
		(SSD1306_PANEL_NUMOF_COLUMS - 1),
		SSD1306_CONTROL_BYTE_CMD,
		SSD1306_SET_PAGE_ADDRESS,
		SSD1306_CONTROL_BYTE_CMD,
		0,
		SSD1306_CONTROL_LAST_BYTE_CMD,
		(SSD1306_PANEL_NUMOF_PAGES - 1)
	};

	if (i2c_write(driver->i2c, cmd_buf, sizeof(cmd_buf),
		      DT_SSD1306_I2C_ADDR)) {
		LOG_ERR("Failed to write command");
		return -1;
	}

	return i2c_burst_write(driver->i2c, DT_SSD1306_I2C_ADDR,
			       SSD1306_CONTROL_LAST_BYTE_DATA,
			       (u8_t *)buf, desc->buf_size);

#elif defined(CONFIG_SSD1306_SH1106_COMPATIBLE)
	if (len != SSD1306_PANEL_NUMOF_PAGES * DT_SSD1306_PANEL_WIDTH) {
		return -1;
	}

	for (size_t pidx = 0; pidx < SSD1306_PANEL_NUMOF_PAGES; pidx++) {
		if (ssd1306_write_page(dev, pidx, buf, DT_SSD1306_PANEL_WIDTH)) {
			return -1;
		}
		buf = (u8_t *)buf + DT_SSD1306_PANEL_WIDTH;
	}
#endif

	return 0;
}

static int ssd1306_read(const struct device *dev, const u16_t x,
			const u16_t y,
			const struct display_buffer_descriptor *desc,
			void *buf)
{
	LOG_ERR("Unsupported");
	return -ENOTSUP;
}

static void *ssd1306_get_framebuffer(const struct device *dev)
{
	LOG_ERR("Unsupported");
	return NULL;
}

static int ssd1306_set_brightness(const struct device *dev,
				  const u8_t brightness)
{
	LOG_WRN("Unsupported");
	return -ENOTSUP;
}

int ssd1306_set_contrast(const struct device *dev, const u8_t contrast)
{
	struct ssd1306_data *driver = dev->driver_data;
	u8_t cmd_buf[] = {
		SSD1306_CONTROL_BYTE_CMD,
		SSD1306_SET_CONTRAST_CTRL,
		SSD1306_CONTROL_LAST_BYTE_CMD,
		contrast,
	};

	return i2c_write(driver->i2c, cmd_buf, sizeof(cmd_buf),
			 DT_SSD1306_I2C_ADDR);
}

static void ssd1306_get_capabilities(const struct device *dev,
				     struct display_capabilities *caps)
{
	memset(caps, 0, sizeof(struct display_capabilities));
	caps->x_resolution = DT_SSD1306_PANEL_WIDTH;
	caps->y_resolution = DT_SSD1306_PANEL_HEIGHT;
	caps->supported_pixel_formats = PIXEL_FORMAT_MONO10;
	caps->current_pixel_format = PIXEL_FORMAT_MONO10;
	caps->screen_info = SCREEN_INFO_MONO_VTILED;
}

static int ssd1306_set_pixel_format(const struct device *dev,
				    const enum display_pixel_format pf)
{
	LOG_ERR("Unsupported");
	return -ENOTSUP;
}

static int ssd1306_init_device(struct device *dev)
{
	struct ssd1306_data *driver = dev->driver_data;

	u8_t cmd_buf[] = {
		SSD1306_CONTROL_BYTE_CMD,
		SSD1306_SET_ENTIRE_DISPLAY_OFF,
		SSD1306_CONTROL_LAST_BYTE_CMD,
		SSD1306_SET_NORMAL_DISPLAY,
	};

	/* Turn display off */
	if (ssd1306_reg_write(driver, SSD1306_CONTROL_LAST_BYTE_CMD,
			      SSD1306_DISPLAY_OFF)) {
		return -EIO;
	}

	if (ssd1306_set_timing_setting(dev)) {
		return -EIO;
	}

	if (ssd1306_set_hardware_config(dev)) {
		return -EIO;
	}

	if (ssd1306_set_panel_orientation(dev)) {
		return -EIO;
	}

	if (ssd1306_set_charge_pump(dev)) {
		return -EIO;
	}

	if (i2c_write(driver->i2c, cmd_buf, sizeof(cmd_buf),
		      DT_SSD1306_I2C_ADDR)) {
		return -EIO;
	}

	if (ssd1306_set_contrast(dev, CONFIG_SSD1306_DEFAULT_CONTRAST)) {
		return -EIO;
	}

	ssd1306_resume(dev);

	return 0;
}

static int ssd1306_init(struct device *dev)
{
	struct ssd1306_data *driver = dev->driver_data;

	LOG_DBG("");

	driver->i2c = device_get_binding(DT_SSD1306_I2C_MASTER_DEV_NAME);
	if (driver->i2c == NULL) {
		LOG_ERR("Failed to get pointer to %s device!",
			    DT_SSD1306_I2C_MASTER_DEV_NAME);
		return -EINVAL;
	}

	if (ssd1306_init_device(dev)) {
		LOG_ERR("Failed to initialize device!");
		return -EIO;
	}

	return 0;
}

static struct ssd1306_data ssd1306_driver;

static struct display_driver_api ssd1306_driver_api = {
	.blanking_on = ssd1306_resume,
	.blanking_off = ssd1306_suspend,
	.write = ssd1306_write,
	.read = ssd1306_read,
	.get_framebuffer = ssd1306_get_framebuffer,
	.set_brightness = ssd1306_set_brightness,
	.set_contrast = ssd1306_set_contrast,
	.get_capabilities = ssd1306_get_capabilities,
	.set_pixel_format = ssd1306_set_pixel_format,
};

DEVICE_AND_API_INIT(ssd1306, DT_SSD1306_DEV_NAME, ssd1306_init,
		    &ssd1306_driver, NULL,
		    POST_KERNEL, CONFIG_APPLICATION_INIT_PRIORITY,
		    &ssd1306_driver_api);
