/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <logging/log.h>
#define LOG_LEVEL CONFIG_BOARD_LOG_LEVEL
LOG_MODULE_REGISTER(board_nonsecure);

#include <init.h>
#include <hal/nrf_clock.h>
#include <nrfx.h>
#include <gpio.h>
#if defined(CONFIG_BSD_LIBRARY) && \
defined(CONFIG_NET_SOCKETS_OFFLOAD)
#include <net/socket.h>
#endif

#include "adp536x.h"

#define POWER_CTRL_1V8_PIN		3
#define POWER_CTRL_3V3_PIN		28
#define ADP536x_I2C_DEV_NAME		I2C_2_LABEL
#define LC_MAX_READ_LENGTH		128

static struct device *gpio_dev;

static int pca20035_magpio_configure(void)
{
#if defined(CONFIG_BSD_LIBRARY) && \
defined(CONFIG_NET_SOCKETS_OFFLOAD)
	static const char magpio_at_cmd[] =
		"AT%XMAGPIO=1,1,1,450,451,746,803,698,748,824,894,"
		"880,960,1710,2200,791,849,1574,1577";
	int at_socket_fd;
	int buffer;
	u8_t read_buffer[LC_MAX_READ_LENGTH];

	at_socket_fd = socket(AF_LTE, 0, NPROTO_AT);
	if (at_socket_fd == -1) {
		return -EFAULT;
	}

	LOG_DBG("AT CMD: %s", magpio_at_cmd);
	buffer = send(at_socket_fd, magpio_at_cmd, strlen(magpio_at_cmd), 0);
	if (buffer != strlen(magpio_at_cmd)) {
		close(at_socket_fd);
		return -EIO;
	}

	buffer = recv(at_socket_fd, read_buffer, LC_MAX_READ_LENGTH, 0);
	LOG_DBG("AT RESP: %s", read_buffer);
	if ((buffer < 2) ||
	    (memcmp("OK", read_buffer, 2 != 0))) {
		close(at_socket_fd);
		return -EIO;
	}

	close(at_socket_fd);
#endif
	return 0;
}

int pca20035_power_1v8_set(bool enable)
{
	if (!gpio_dev) {
		return -ENODEV;
	}

	return gpio_pin_write(gpio_dev, POWER_CTRL_1V8_PIN, enable);
}

int pca20035_power_3v3_set(bool enable)
{
	if (!gpio_dev) {
		return -ENODEV;
	}

	return gpio_pin_write(gpio_dev, POWER_CTRL_3V3_PIN, enable);
}

static int pca20035_power_ctrl_pins_init(void)
{
	int err;

	gpio_dev = device_get_binding(DT_GPIO_P0_DEV_NAME);
	if (!gpio_dev) {
		return -ENODEV;
	}

	err = gpio_pin_configure(gpio_dev, POWER_CTRL_1V8_PIN, GPIO_DIR_OUT);
	if (err) {
		return err;
	}

	err = gpio_pin_configure(gpio_dev, POWER_CTRL_3V3_PIN, GPIO_DIR_OUT);
	if (err) {
		return err;
	}

	return 0;
}

static int power_mgmt_init(void)
{
	int err;

	err = adp536x_init(ADP536x_I2C_DEV_NAME);
	if (err) {
		return err;
	}

	err = adp536x_buck_1v8_set();
	if (err) {
		return err;
	}

	err = adp536x_buckbst_3v3_set();
	if (err) {
		return err;
	}

	err = adp536x_buckbst_enable(true);
	if (err) {
		return err;
	}

	/* The value 0x07 sets VBUS current limit to 500 mA. */
	err = adp536x_vbus_current_set(0x07);
	if (err) {
		return err;
	}

	/* The value 0x09 corresponds to 100 mA charging current. */
	err = adp536x_charger_current_set(0x09);
	if (err) {
		return err;
	}

	err = adp536x_charging_enable(true);
	if (err) {
		return err;
	}

	return 0;
}

static int pca20035_board_init(struct device *dev)
{
	int err;

	err = pca20035_power_ctrl_pins_init();
	if (err) {
		LOG_ERR("pca20035_power_ctrl_pins_init: failed! %d", err);
		return err;
	}

	err = power_mgmt_init();
	if (err) {
		LOG_ERR("power_mgmt_init: failed! %d", err);
		return err;
	}

	err = pca20035_power_1v8_set(true);
	if (err) {
		LOG_ERR("pca20035_power_1v8_set: failed! %d", err);
		return err;
	}

	err = pca20035_power_3v3_set(true);
	if (err) {
		LOG_ERR("pca20035_power_3v3_set: failed! %d", err);
		return err;
	}

	err = pca20035_magpio_configure();
	if (err) {
		LOG_ERR("pca20035_magpio_configure: failed! %d", err);
		return err;
	}

	err = adp536x_oc_chg_hiccup_set(true);
	if (err) {
		return err;
	}


	return 0;
}

SYS_INIT(pca20035_board_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
