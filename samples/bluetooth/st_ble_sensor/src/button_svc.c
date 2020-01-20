/** @file
 *  @brief Button Service sample
 */

/*
 * Copyright (c) 2019 Marcio Montenegro <mtuxpe@gmail.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/types.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>
#include <sys/printk.h>
#include <sys/byteorder.h>
#include <zephyr.h>
#include <drivers/gpio.h>
#include <logging/log.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/conn.h>
#include <bluetooth/uuid.h>
#include <bluetooth/gatt.h>
#include "button_svc.h"

LOG_MODULE_REGISTER(button_svc);

#define BUT_PORT    DT_ALIAS_SW0_GPIOS_CONTROLLER
#define BUT_PIN     DT_ALIAS_SW0_GPIOS_PIN

extern struct bt_conn *conn;
extern struct bt_gatt_service_static stsensor_svc[];
extern volatile bool notify_enable;

static struct device *button_dev;
static struct gpio_callback gpio_cb;
u16_t but_val;

void button_pressed(struct device *gpiob, struct gpio_callback *cb,
		    u32_t pins)
{
	int err;

	LOG_INF("Button SW1 pressed");
	if (conn) {
		if (notify_enable) {
			err = bt_gatt_notify(NULL, &stsensor_svc->attrs[2],
					     &but_val, sizeof(but_val));
			if (err) {
				LOG_ERR("Notify error: %d", err);
			} else {
				LOG_INF("Send notify ok");
				but_val = (but_val == 0) ? 0x100 : 0;
			}
		} else {
			LOG_INF("Notify not enabled");
		}
	} else {
		LOG_INF("BLE not connected");
	}
}

int button_init(void)
{
	button_dev = device_get_binding(BUT_PORT);
	if (!button_dev) {
		return (-EOPNOTSUPP);
	}

	gpio_pin_configure(button_dev, BUT_PIN,
			   GPIO_DIR_IN | GPIO_INT | GPIO_INT_EDGE |
			   GPIO_PUD_PULL_UP | GPIO_INT_DEBOUNCE |
			   GPIO_INT_ACTIVE_LOW);
	gpio_init_callback(&gpio_cb, button_pressed, BIT(BUT_PIN));
	gpio_add_callback(button_dev, &gpio_cb);
	gpio_pin_enable_callback(button_dev, BUT_PIN);
	but_val = 0;
	return 0;
}
