/*
 * Copyright (c) 2016 Intel Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/usb/usb_device.h>
#include <zephyr/usb/usbd.h>
#include <zephyr/drivers/uart.h>

BUILD_ASSERT(DT_NODE_HAS_COMPAT(DT_CHOSEN(zephyr_console), zephyr_cdc_acm_uart),
	     "Console device is not ACM CDC UART device");

#if defined(CONFIG_USB_DEVICE_STACK_NEXT)
USBD_CONFIGURATION_DEFINE(config_1,
			  USB_SCD_SELF_POWERED,
			  200);

USBD_DESC_LANG_DEFINE(sample_lang);
USBD_DESC_MANUFACTURER_DEFINE(sample_mfr, "ZEPHYR");
USBD_DESC_PRODUCT_DEFINE(sample_product, "Zephyr USBD ACM console");
USBD_DESC_SERIAL_NUMBER_DEFINE(sample_sn, "0123456789ABCDEF");

USBD_DEVICE_DEFINE(sample_usbd,
		   DEVICE_DT_GET(DT_NODELABEL(zephyr_udc0)),
		   0x2fe3, 0x0001);

static int enable_usb_device_next(void)
{
	int err;

	err = usbd_add_descriptor(&sample_usbd, &sample_lang);
	if (err) {
		return err;
	}

	err = usbd_add_descriptor(&sample_usbd, &sample_mfr);
	if (err) {
		return err;
	}

	err = usbd_add_descriptor(&sample_usbd, &sample_product);
	if (err) {
		return err;
	}

	err = usbd_add_descriptor(&sample_usbd, &sample_sn);
	if (err) {
		return err;
	}

	err = usbd_add_configuration(&sample_usbd, &config_1);
	if (err) {
		return err;
	}

	err = usbd_register_class(&sample_usbd, "cdc_acm_0", 1);
	if (err) {
		return err;
	}

	err = usbd_init(&sample_usbd);
	if (err) {
		return err;
	}

	err = usbd_enable(&sample_usbd);
	if (err) {
		return err;
	}

	return 0;
}
#endif /* IS_ENABLED(CONFIG_USB_DEVICE_STACK_NEXT) */

int main(void)
{
	const struct device *const dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));
	uint32_t dtr = 0;

#if defined(CONFIG_USB_DEVICE_STACK_NEXT)
	if (enable_usb_device_next()) {
		return 0;
	}
#else
	if (usb_enable(NULL)) {
		return 0;
	}
#endif

	/* Poll if the DTR flag was set */
	while (!dtr) {
		uart_line_ctrl_get(dev, UART_LINE_CTRL_DTR, &dtr);
		/* Give CPU resources to low priority threads. */
		k_sleep(K_MSEC(100));
	}

	while (1) {
		printk("Hello World! %s\n", CONFIG_ARCH);
		k_sleep(K_SECONDS(1));
	}
}
