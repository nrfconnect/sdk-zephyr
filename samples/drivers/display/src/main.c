/*
 * Copyright (c) 2019 Jan Van Winkel <jan.van_winkel@dxplore.eu>
 *
 * Based on ST7789V sample:
 * Copyright (c) 2019 Marc Reilly
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "display.h"
#include <logging/log.h>
LOG_MODULE_REGISTER(sample, LOG_LEVEL_INF);

#include <zephyr.h>
#include <device.h>
#include <drivers/display.h>

#ifdef CONFIG_ARCH_POSIX
#include "posix_board_if.h"
#endif

#ifdef CONFIG_ARCH_POSIX
#define RETURN_FROM_MAIN(exit_code) posix_exit_main(exit_code)
#else
#define RETURN_FROM_MAIN(exit_code) return
#endif

enum corner {
	TOP_LEFT,
	TOP_RIGHT,
	BOTTOM_RIGHT,
	BOTTOM_LEFT
};

typedef void (*fill_buffer)(enum corner corner, u8_t grey, u8_t *buf,
			    size_t buf_size);


#ifdef CONFIG_ARCH_POSIX
static void posix_exit_main(int exit_code)
{
#if CONFIG_TEST
	if (exit_code == 0) {
		LOG_INF("PROJECT EXECUTION SUCCESSFUL");
	} else {
		LOG_INF("PROJECT EXECUTION FAILED");
	}
#endif
	posix_exit(exit_code);
}
#endif

static void fill_buffer_argb8888(enum corner corner, u8_t grey, u8_t *buf,
			       size_t buf_size)
{
	u32_t color = 0;

	switch (corner) {
	case TOP_LEFT:
		color = 0x00FF0000u;
		break;
	case TOP_RIGHT:
		color = 0x0000FF00u;
		break;
	case BOTTOM_RIGHT:
		color = 0x000000FFu;
		break;
	case BOTTOM_LEFT:
		color = grey << 16 | grey << 8 | grey;
		break;
	}

	for (size_t idx = 0; idx < buf_size; idx += 4) {
		*((u32_t *)(buf + idx)) = color;
	}
}

static void fill_buffer_rgb888(enum corner corner, u8_t grey, u8_t *buf,
			       size_t buf_size)
{
	u32_t color = 0;

	switch (corner) {
	case TOP_LEFT:
		color = 0x00FF0000u;
		break;
	case TOP_RIGHT:
		color = 0x0000FF00u;
		break;
	case BOTTOM_RIGHT:
		color = 0x000000FFu;
		break;
	case BOTTOM_LEFT:
		color = grey << 16 | grey << 8 | grey;
		break;
	}

	for (size_t idx = 0; idx < buf_size; idx += 3) {
		*(buf + idx + 0) = color >> 16;
		*(buf + idx + 1) = color >> 8;
		*(buf + idx + 2) = color >> 0;
	}
}

static void fill_buffer_rgb565(enum corner corner, u8_t grey, u8_t *buf,
			       size_t buf_size)
{
	u16_t color = 0;
	u16_t grey_5bit;

	switch (corner) {
	case TOP_LEFT:
		color = 0xF800u;
		break;
	case TOP_RIGHT:
		color = 0x07E0u;
		break;
	case BOTTOM_RIGHT:
		color = 0x001Fu;
		break;
	case BOTTOM_LEFT:
		grey_5bit = grey & 0x1Fu;
		/* shift the green an extra bit, it has 6 bits */
		color = grey_5bit << 11 | grey_5bit << (5 + 1) | grey_5bit;
		break;
	}

	for (size_t idx = 0; idx < buf_size; idx += 2) {
		*(buf + idx + 0) = (color >> 8) & 0xFFu;
		*(buf + idx + 1) = (color >> 0) & 0xFFu;
	}
}

static void fill_buffer_mono(enum corner corner, u8_t grey, u8_t *buf,
			     size_t buf_size)
{
	u16_t color;

	switch (corner) {
	case BOTTOM_LEFT:
		color = (grey & 0x01u) ? 0xFFu : 0x00u;
		break;
	default:
		color = 0;
		break;
	}

	memset(buf, color, buf_size);
}

void main(void)
{
	size_t x;
	size_t y;
	size_t rect_w;
	size_t rect_h;
	size_t h_step;
	size_t scale;
	size_t grey_count;
	u8_t *buf;
	s32_t grey_scale_sleep;
	struct device *display_dev;
	struct display_capabilities capabilities;
	struct display_buffer_descriptor buf_desc;
	size_t buf_size = 0;
	fill_buffer fill_buffer_fnc = NULL;

	LOG_INF("Display sample for %s", DISPLAY_DEV_NAME);

	display_dev = device_get_binding(DISPLAY_DEV_NAME);

	if (display_dev == NULL) {
		LOG_ERR("Device %s not found. Aborting sample.",
			DISPLAY_DEV_NAME);
		RETURN_FROM_MAIN(1);
	}

	display_get_capabilities(display_dev, &capabilities);

	if (capabilities.screen_info & SCREEN_INFO_MONO_VTILED)  {
		rect_w = 16;
		rect_h = 8;
	} else {
		rect_w = 2;
		rect_h = 1;
	}

	h_step = rect_h;
	scale = (capabilities.x_resolution / 8) / rect_h;

	rect_w *= scale;
	rect_h *= scale;

	if (capabilities.screen_info & SCREEN_INFO_EPD) {
		grey_scale_sleep = K_MSEC(10000);
	} else {
		grey_scale_sleep = K_MSEC(100);
	}

	buf_size = rect_w * rect_h;

	if (buf_size < (capabilities.x_resolution * h_step)) {
		buf_size = capabilities.x_resolution * h_step;
	}

	switch (capabilities.current_pixel_format) {
	case PIXEL_FORMAT_ARGB_8888:
		fill_buffer_fnc = fill_buffer_argb8888;
		buf_size *= 4;
		break;
	case PIXEL_FORMAT_RGB_888:
		fill_buffer_fnc = fill_buffer_rgb888;
		buf_size *= 3;
		break;
	case PIXEL_FORMAT_RGB_565:
		fill_buffer_fnc = fill_buffer_rgb565;
		buf_size *= 2;
		break;
	case PIXEL_FORMAT_MONO01:
	case PIXEL_FORMAT_MONO10:
		fill_buffer_fnc = fill_buffer_mono;
		buf_size /= 8;
		break;
	default:
		LOG_ERR("Unsupported pixel format. Aborting sample.");
		RETURN_FROM_MAIN(1);
	}

	buf = k_malloc(buf_size);

	if (buf == NULL) {
		LOG_ERR("Could not allocate memory. Aborting sample.");
		RETURN_FROM_MAIN(1);
	}

	(void)memset(buf, 0xFFu, buf_size);

	buf_desc.buf_size = buf_size;
	buf_desc.pitch = capabilities.x_resolution;
	buf_desc.width = capabilities.x_resolution;
	buf_desc.height = h_step;

	for (int idx = 0; idx < capabilities.y_resolution; idx += h_step) {
		display_write(display_dev, 0, idx, &buf_desc, buf);
	}

	buf_desc.pitch = rect_w;
	buf_desc.width = rect_w;
	buf_desc.height = rect_h;

	fill_buffer_fnc(TOP_LEFT, 0, buf, buf_size);
	x = 0;
	y = 0;
	display_write(display_dev, x, y, &buf_desc, buf);

	fill_buffer_fnc(TOP_RIGHT, 0, buf, buf_size);
	x = capabilities.x_resolution - rect_w;
	y = 0;
	display_write(display_dev, x, y, &buf_desc, buf);

	fill_buffer_fnc(BOTTOM_RIGHT, 0, buf, buf_size);
	x = capabilities.x_resolution - rect_w;
	y = capabilities.y_resolution - rect_h;
	display_write(display_dev, x, y, &buf_desc, buf);

	display_blanking_off(display_dev);

	grey_count = 0;
	x = 0;
	y = capabilities.y_resolution - rect_h;

	while (1) {
		fill_buffer_fnc(BOTTOM_LEFT, grey_count, buf, buf_size);
		display_write(display_dev, x, y, &buf_desc, buf);
		++grey_count;
		k_sleep(grey_scale_sleep);
#if CONFIG_TEST
		if (grey_count >= 1024) {
			break;
		}
#endif
	}

	RETURN_FROM_MAIN(0);
}
