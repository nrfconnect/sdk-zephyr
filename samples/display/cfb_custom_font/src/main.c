/*
 * Copyright (c) 2018 Henrik Brix Andersen <henrik@brixandersen.dk>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <device.h>
#include <display/cfb.h>
#include <misc/printk.h>

#include "cfb_font_dice.h"

#if defined(CONFIG_SSD1673)
#define DISPLAY_DRIVER "SSD1673"
#else
#error Unsupported board
#endif

struct device *display;

void main(void)
{
	int err;

	display = device_get_binding(DISPLAY_DRIVER);
	if (!display) {
		printk("Could not get device binding for display device\n");
	}

	err = cfb_framebuffer_init(display);
	if (err) {
		printk("Could not initialize framebuffer (err %d)\n", err);
	}

	err = cfb_framebuffer_clear(display, true);
	if (err) {
		printk("Could not clear framebuffer (err %d)\n", err);
	}

	err = cfb_print(display, "123456", 0, 0);
	if (err) {
		printk("Could not display custom font (err %d)\n", err);
	}

	err = cfb_framebuffer_finalize(display);
	if (err) {
		printk("Could not finalize framebuffer (err %d)\n", err);
	}
}
