/*
 * Copyright (c) 2018 Jan Van Winkel <jan.van_winkel@dxplore.eu>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <display.h>

#include <SDL.h>
#include <string.h>
#include <soc.h>

#define LOG_LEVEL CONFIG_DISPLAY_LOG_LEVEL
#include <logging/log.h>
LOG_MODULE_REGISTER(display_sdl);

struct sdl_display_data {
	SDL_Window *window;
	SDL_Renderer *renderer;
	SDL_Texture *texture;
	bool display_on;
	enum display_pixel_format current_pixel_format;
	u8_t buf[4 * CONFIG_SDL_DISPLAY_X_RES * CONFIG_SDL_DISPLAY_Y_RES];
};

static struct sdl_display_data sdl_display_data;

static int sdl_display_init(struct device *dev)
{
	struct sdl_display_data *disp_data =
	    (struct sdl_display_data *)dev->driver_data;
	LOG_DBG("Initializing display driver");

	memset(disp_data, 0, sizeof(struct sdl_display_data));

	disp_data->current_pixel_format = PIXEL_FORMAT_ARGB_8888;

	disp_data->window =
	    SDL_CreateWindow("Zephyr Display", SDL_WINDOWPOS_UNDEFINED,
			     SDL_WINDOWPOS_UNDEFINED, CONFIG_SDL_DISPLAY_X_RES,
			     CONFIG_SDL_DISPLAY_Y_RES, SDL_WINDOW_SHOWN);
	if (disp_data->window == NULL) {
		LOG_ERR("Failed to create SDL window: %s", SDL_GetError());
		return -EIO;
	}

	disp_data->renderer =
	    SDL_CreateRenderer(disp_data->window, -1, SDL_RENDERER_ACCELERATED);
	if (disp_data->renderer == NULL) {
		LOG_ERR("Failed to create SDL renderer: %s",
			    SDL_GetError());
		return -EIO;
	}

	disp_data->texture = SDL_CreateTexture(
	    disp_data->renderer, SDL_PIXELFORMAT_ARGB8888,
	    SDL_TEXTUREACCESS_STATIC, CONFIG_SDL_DISPLAY_X_RES,
	    CONFIG_SDL_DISPLAY_Y_RES);
	if (disp_data->texture == NULL) {
		LOG_ERR("Failed to create SDL texture: %s", SDL_GetError());
		return -EIO;
	}

	disp_data->display_on = false;
	SDL_SetRenderDrawColor(disp_data->renderer, 0, 0, 0, 0xFF);
	SDL_RenderClear(disp_data->renderer);
	SDL_RenderPresent(disp_data->renderer);

	return 0;
}

static void sdl_display_write_argb8888(void *disp_buf,
		const struct display_buffer_descriptor *desc, const void *buf)
{
	__ASSERT((4 * desc->pitch * desc->height) <= desc->buf_size,
			"Input buffer to small");

	memcpy(disp_buf, buf, 4 * desc->pitch * desc->height);
}

static void sdl_display_write_rgb888(u8_t *disp_buf,
		const struct display_buffer_descriptor *desc, const void *buf)
{
	u32_t w_idx;
	u32_t h_idx;
	u32_t pixel;
	const u8_t *byte_ptr;

	__ASSERT((3 * desc->pitch * desc->height) <= desc->buf_size,
			"Input buffer to small");

	for (h_idx = 0; h_idx < desc->height; ++h_idx) {
		for (w_idx = 0; w_idx < desc->width; ++w_idx) {
			byte_ptr = (const u8_t *)buf +
				3 * ((h_idx * desc->pitch) + w_idx);
			pixel = *byte_ptr << 16;
			pixel |= *(byte_ptr + 1) << 8;
			pixel |= *(byte_ptr + 2);
			*((u32_t *)disp_buf) = pixel;
			disp_buf += 4;
		}
	}
}

static void sdl_display_write_mono(u8_t *disp_buf,
		const struct display_buffer_descriptor *desc, const void *buf,
		const bool one_is_black)
{
	u32_t w_idx;
	u32_t h_idx;
	u32_t tile_idx;
	u32_t pixel;
	const u8_t *byte_ptr;
	u32_t one_color;
	u8_t *disp_buf_start;

	__ASSERT((desc->pitch * desc->height) <= (8 * desc->buf_size),
			"Input buffer to small");
	__ASSERT((desc->height % 8) == 0,
			"Input buffer height not aligned per 8 pixels");

	if (one_is_black) {
		one_color = 0;
	} else {
		one_color = 0x00FFFFFF;
	}

	for (tile_idx = 0; tile_idx < desc->height/8; ++tile_idx) {
		for (w_idx = 0; w_idx < desc->width; ++w_idx) {
			byte_ptr = (const u8_t *)buf +
				((tile_idx * desc->pitch) + w_idx);
			disp_buf_start = disp_buf;
			for (h_idx = 0; h_idx < 8; ++h_idx) {
				if ((*byte_ptr & BIT(7-h_idx)) != 0)  {
					pixel = one_color;
				} else {
					pixel = (~one_color) & 0x00FFFFFF;
				}
				*((u32_t *)disp_buf) = pixel;
				disp_buf += (4 * desc->width);
			}
			disp_buf = disp_buf_start;
			disp_buf += 4;
		}
		disp_buf += 7 * (4 * desc->width);
	}
}

static int sdl_display_write(const struct device *dev, const u16_t x,
			     const u16_t y,
			     const struct display_buffer_descriptor *desc,
			     const void *buf)
{
	SDL_Rect rect;

	struct sdl_display_data *disp_data =
		(struct sdl_display_data *)dev->driver_data;

	LOG_DBG("Writing %dx%d (w,h) bitmap @ %dx%d (x,y)", desc->width,
			desc->height, x, y);

	__ASSERT(desc->width <= desc->pitch, "Pitch is smaller then width");

	if (disp_data->current_pixel_format == PIXEL_FORMAT_ARGB_8888) {
		sdl_display_write_argb8888(disp_data->buf, desc, buf);
	} else if (disp_data->current_pixel_format == PIXEL_FORMAT_RGB_888) {
		sdl_display_write_rgb888(disp_data->buf, desc, buf);
	} else if (disp_data->current_pixel_format == PIXEL_FORMAT_MONO10) {
		sdl_display_write_mono(disp_data->buf, desc, buf, true);
	} else if (disp_data->current_pixel_format == PIXEL_FORMAT_MONO01) {
		sdl_display_write_mono(disp_data->buf, desc, buf, false);
	}

	rect.x = x;
	rect.y = y;
	rect.w = desc->width;
	rect.h = desc->height;

	SDL_UpdateTexture(disp_data->texture, &rect, disp_data->buf,
			4 * rect.w);

	if (disp_data->display_on) {
		SDL_RenderClear(disp_data->renderer);
		SDL_RenderCopy(disp_data->renderer, disp_data->texture, NULL,
				NULL);
		SDL_RenderPresent(disp_data->renderer);
	}

	return 0;
}

static int sdl_display_read(const struct device *dev, const u16_t x,
			    const u16_t y,
			    const struct display_buffer_descriptor *desc,
			    void *buf)
{
	struct sdl_display_data *disp_data =
		(struct sdl_display_data *)dev->driver_data;
	SDL_Rect rect;

	rect.x = x;
	rect.y = y;
	rect.w = desc->width;
	rect.h = desc->height;

	LOG_DBG("Reading %dx%d (w,h) bitmap @ %dx%d (x,y)", desc->width,
			desc->height, x, y);

	__ASSERT(desc->width <= desc->pitch, "Pitch is smaller then width");
	__ASSERT((3 * desc->pitch * desc->height) <= desc->buf_size,
			"Input buffer to small");

	return SDL_RenderReadPixels(disp_data->renderer, &rect, 0, buf,
			4 * desc->pitch);
}

static void *sdl_display_get_framebuffer(const struct device *dev)
{
	return NULL;
}

static int sdl_display_blanking_off(const struct device *dev)
{
	struct sdl_display_data *disp_data =
		(struct sdl_display_data *)dev->driver_data;

	LOG_DBG("Turning display blacking off");

	disp_data->display_on = true;

	SDL_RenderClear(disp_data->renderer);
	SDL_RenderCopy(disp_data->renderer, disp_data->texture, NULL, NULL);
	SDL_RenderPresent(disp_data->renderer);

	return 0;
}

static int sdl_display_blanking_on(const struct device *dev)
{
	struct sdl_display_data *disp_data =
		(struct sdl_display_data *)dev->driver_data;

	LOG_DBG("Turning display blanking on");

	disp_data->display_on = false;

	SDL_RenderClear(disp_data->renderer);
	SDL_RenderPresent(disp_data->renderer);
	return 0;
}

static int sdl_display_set_brightness(const struct device *dev,
		const u8_t brightness)
{
	return -ENOTSUP;
}

static int sdl_display_set_contrast(const struct device *dev,
		const u8_t contrast)
{
	return -ENOTSUP;
}

static void sdl_display_get_capabilities(
	const struct device *dev, struct display_capabilities *capabilities)
{
	struct sdl_display_data *disp_data =
	    (struct sdl_display_data *)dev->driver_data;

	memset(capabilities, 0, sizeof(struct display_capabilities));
	capabilities->x_resolution = CONFIG_SDL_DISPLAY_X_RES;
	capabilities->y_resolution = CONFIG_SDL_DISPLAY_Y_RES;
	capabilities->supported_pixel_formats = PIXEL_FORMAT_ARGB_8888 |
		PIXEL_FORMAT_RGB_888 |
		PIXEL_FORMAT_MONO01 |
		PIXEL_FORMAT_MONO10;
	capabilities->current_pixel_format = disp_data->current_pixel_format;
	capabilities->screen_info = SCREEN_INFO_MONO_VTILED |
		SCREEN_INFO_MONO_MSB_FIRST;
}

static int sdl_display_set_pixel_format(const struct device *dev,
		const enum display_pixel_format pixel_format)
{
	struct sdl_display_data *disp_data =
		(struct sdl_display_data *)dev->driver_data;

	switch (pixel_format) {
	case PIXEL_FORMAT_ARGB_8888:
	case PIXEL_FORMAT_RGB_888:
	case PIXEL_FORMAT_MONO01:
	case PIXEL_FORMAT_MONO10:
		disp_data->current_pixel_format = pixel_format;
		return 0;
	default:
		LOG_ERR("Pixel format not supported");
		return -ENOTSUP;
	}
}

static void sdl_display_cleanup(void)
{
	if (sdl_display_data.texture != NULL) {
		SDL_DestroyTexture(sdl_display_data.texture);
		sdl_display_data.texture = NULL;
	}

	if (sdl_display_data.renderer != NULL) {
		SDL_DestroyRenderer(sdl_display_data.renderer);
		sdl_display_data.renderer = NULL;
	}

	if (sdl_display_data.window != NULL) {
		SDL_DestroyWindow(sdl_display_data.window);
		sdl_display_data.window = NULL;
	}
}

static const struct display_driver_api sdl_display_api = {
	.blanking_on = sdl_display_blanking_on,
	.blanking_off = sdl_display_blanking_off,
	.write = sdl_display_write,
	.read = sdl_display_read,
	.get_framebuffer = sdl_display_get_framebuffer,
	.set_brightness = sdl_display_set_brightness,
	.set_contrast = sdl_display_set_contrast,
	.get_capabilities = sdl_display_get_capabilities,
	.set_pixel_format = sdl_display_set_pixel_format,
};

DEVICE_AND_API_INIT(sdl_display, CONFIG_SDL_DISPLAY_DEV_NAME, &sdl_display_init,
		    &sdl_display_data, NULL, APPLICATION,
		    CONFIG_APPLICATION_INIT_PRIORITY, &sdl_display_api);


NATIVE_TASK(sdl_display_cleanup, ON_EXIT, 1);
