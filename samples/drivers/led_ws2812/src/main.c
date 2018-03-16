/*
 * Copyright (c) 2017 Linaro Limited
 * Copyright (c) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <string.h>

#define SYS_LOG_LEVEL SYS_LOG_LEVEL_DEBUG
#include <logging/sys_log.h>

#include <zephyr.h>
#include <led_strip.h>
#include <device.h>
#include <spi.h>
#include <misc/util.h>

/*
 * Number of RGB LEDs in the LED strip, adjust as needed.
 */
#if defined(CONFIG_WS2812_STRIP)
#define STRIP_NUM_LEDS 12
#define STRIP_DEV_NAME CONFIG_WS2812_STRIP_NAME
#else
#define STRIP_NUM_LEDS 24
#define STRIP_DEV_NAME CONFIG_WS2812B_SW_NAME
#endif

#define SPI_DEV_NAME "ws2812_spi"
#define DELAY_TIME K_MSEC(40)

static const struct led_rgb colors[] = {
	{ .r = 0xff, .g = 0x00, .b = 0x00, }, /* red */
	{ .r = 0x00, .g = 0xff, .b = 0x00, }, /* green */
	{ .r = 0x00, .g = 0x00, .b = 0xff, }, /* blue */
};

static const struct led_rgb black = {
	.r = 0x00,
	.g = 0x00,
	.b = 0x00,
};

struct led_rgb strip_colors[STRIP_NUM_LEDS];

const struct led_rgb *color_at(size_t time, size_t i)
{
	size_t rgb_start = time % STRIP_NUM_LEDS;

	if (rgb_start <= i && i < rgb_start + ARRAY_SIZE(colors)) {
		return &colors[i - rgb_start];
	} else {
		return &black;
	}
}

void main(void)
{
	struct device *strip;
	size_t i, time;
#if defined(CONFIG_SPI)
	struct device *spi;

	/* Double-check the configuration. */
	spi = device_get_binding(SPI_DEV_NAME);
	if (spi) {
		SYS_LOG_INF("Found SPI device %s", SPI_DEV_NAME);
	} else {
		SYS_LOG_ERR("SPI device not found; you must choose a SPI "
			    "device and configure its name to %s",
			    SPI_DEV_NAME);
		return;
	}
#endif

	strip = device_get_binding(STRIP_DEV_NAME);
	if (strip) {
		SYS_LOG_INF("Found LED strip device %s", STRIP_DEV_NAME);
	} else {
		SYS_LOG_ERR("LED strip device %s not found", STRIP_DEV_NAME);
		return;
	}

	/*
	 * Display a pattern that "walks" the three primary colors
	 * down the strip until it reaches the end, then starts at the
	 * beginning. This has the effect of moving it around in a
	 * circle in the case of rings of pixels.
	 */
	SYS_LOG_INF("Displaying pattern on strip");
	time = 0;
	while (1) {
		for (i = 0; i < STRIP_NUM_LEDS; i++) {
			memcpy(&strip_colors[i], color_at(time, i),
			       sizeof(strip_colors[i]));
		}
		led_strip_update_rgb(strip, strip_colors, STRIP_NUM_LEDS);
		k_sleep(DELAY_TIME);
		time++;
	}
}
