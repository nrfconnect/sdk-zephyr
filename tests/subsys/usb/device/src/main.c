/*
 * Copyright (c) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <ztest.h>
#include <tc_util.h>

#include <sys/byteorder.h>
#include <usb/usb_device.h>
#include <usb/usb_common.h>

/* Max packet size for endpoints */
#define BULK_EP_MPS		64

#define ENDP_BULK_IN		0x81

#define VALID_EP		ENDP_BULK_IN
#define INVALID_EP		0x20

struct usb_device_desc {
	struct usb_if_descriptor if0;
	struct usb_ep_descriptor if0_in_ep;
} __packed;

#define INITIALIZER_IF							\
	{								\
		.bLength = sizeof(struct usb_if_descriptor),		\
		.bDescriptorType = USB_INTERFACE_DESC,			\
		.bInterfaceNumber = 0,					\
		.bAlternateSetting = 0,					\
		.bNumEndpoints = 1,					\
		.bInterfaceClass = CUSTOM_CLASS,			\
		.bInterfaceSubClass = 0,				\
		.bInterfaceProtocol = 0,				\
		.iInterface = 0,					\
	}

#define INITIALIZER_IF_EP(addr, attr, mps, interval)			\
	{								\
		.bLength = sizeof(struct usb_ep_descriptor),		\
		.bDescriptorType = USB_ENDPOINT_DESC,			\
		.bEndpointAddress = addr,				\
		.bmAttributes = attr,					\
		.wMaxPacketSize = sys_cpu_to_le16(mps),			\
		.bInterval = interval,					\
	}

USBD_CLASS_DESCR_DEFINE(primary, 0) struct usb_device_desc dev_desc = {
	.if0 = INITIALIZER_IF,
	.if0_in_ep = INITIALIZER_IF_EP(ENDP_BULK_IN, USB_DC_EP_BULK,
				       BULK_EP_MPS, 0),
};

static void status_cb(struct usb_cfg_data *cfg,
		      enum usb_dc_status_code status,
		      const u8_t *param)
{
	ARG_UNUSED(cfg);
	ARG_UNUSED(status);
	ARG_UNUSED(param);
}

/* EP Bulk IN handler, used to send data to the Host */
static void bulk_in(u8_t ep, enum usb_dc_ep_cb_status_code ep_status)
{
}

/* Describe EndPoints configuration */
static struct usb_ep_cfg_data device_ep[] = {
	{
		.ep_cb = bulk_in,
		.ep_addr = ENDP_BULK_IN
	},
};

USBD_CFG_DATA_DEFINE(primary, device) struct usb_cfg_data device_config = {
	.usb_device_description = NULL,
	.interface_descriptor = &dev_desc.if0,
	.cb_usb_status = status_cb,
	.interface = {
		.vendor_handler = NULL,
		.class_handler = NULL,
		.custom_handler = NULL,
	},
	.num_endpoints = ARRAY_SIZE(device_ep),
	.endpoint = device_ep,
};

static void test_usb_disable(void)
{
	zassert_equal(usb_disable(), TC_PASS, "usb_disable() failed");
}

static void test_usb_deconfig(void)
{
	zassert_equal(usb_deconfig(), TC_PASS, "usb_deconfig() failed");
}

/* Test USB Device Cotnroller API */
static void test_usb_dc_api(void)
{
	/* Control endpoins are configured */
	zassert_equal(usb_dc_ep_mps(0x0), 64,
		      "usb_dc_ep_mps(0x00) failed");
	zassert_equal(usb_dc_ep_mps(0x80), 64,
		      "usb_dc_ep_mps(0x80) failed");

	/* Bulk EP is not configured yet */
	zassert_equal(usb_dc_ep_mps(ENDP_BULK_IN), 0,
		      "usb_dc_ep_mps(ENDP_BULK_IN) not configured");
}

/* Test USB Device Cotnroller API for invalid parameters */
static void test_usb_dc_api_invalid(void)
{
	u32_t size;
	u8_t byte;

	/* Set stall to invalid EP */
	zassert_not_equal(usb_dc_ep_set_stall(INVALID_EP), TC_PASS,
			  "usb_dc_ep_set_stall(INVALID_EP)");

	/* Clear stall to invalid EP */
	zassert_not_equal(usb_dc_ep_clear_stall(INVALID_EP), TC_PASS,
			  "usb_dc_ep_clear_stall(INVALID_EP)");

	/* Check if the selected endpoint is stalled */
	zassert_not_equal(usb_dc_ep_is_stalled(INVALID_EP, &byte), TC_PASS,
			  "usb_dc_ep_is_stalled(INVALID_EP, stalled)");
	zassert_not_equal(usb_dc_ep_is_stalled(VALID_EP, NULL), TC_PASS,
			  "usb_dc_ep_is_stalled(VALID_EP, NULL)");

	/* Halt invalid EP */
	zassert_not_equal(usb_dc_ep_halt(INVALID_EP), TC_PASS,
			  "usb_dc_ep_halt(INVALID_EP)");

	/* Enable invalid EP */
	zassert_not_equal(usb_dc_ep_enable(INVALID_EP), TC_PASS,
			  "usb_dc_ep_enable(INVALID_EP)");

	/* Disable invalid EP */
	zassert_not_equal(usb_dc_ep_disable(INVALID_EP), TC_PASS,
			  "usb_dc_ep_disable(INVALID_EP)");

	/* Flush invalid EP */
	zassert_not_equal(usb_dc_ep_flush(INVALID_EP), TC_PASS,
			  "usb_dc_ep_flush(INVALID_EP)");

	/* Set callback to invalid EP */
	zassert_not_equal(usb_dc_ep_set_callback(INVALID_EP, NULL), TC_PASS,
			  "usb_dc_ep_set_callback(INVALID_EP, NULL)");

	/* Write to invalid EP */
	zassert_not_equal(usb_dc_ep_write(INVALID_EP, &byte, sizeof(byte),
					  &size),
			  TC_PASS, "usb_dc_ep_write(INVALID_EP)");

	/* Read invalid EP */
	zassert_not_equal(usb_dc_ep_read(INVALID_EP, &byte, sizeof(byte),
					 &size),
			  TC_PASS, "usb_dc_ep_read(INVALID_EP)");
	zassert_not_equal(usb_dc_ep_read_wait(INVALID_EP, &byte, sizeof(byte),
					      &size),
			  TC_PASS, "usb_dc_ep_read_wait(INVALID_EP)");
	zassert_not_equal(usb_dc_ep_read_continue(INVALID_EP), TC_PASS,
			  "usb_dc_ep_read_continue(INVALID_EP)");

	/* Get endpoint max packet size for invalid EP */
	zassert_not_equal(usb_dc_ep_mps(INVALID_EP), TC_PASS,
			  "usb_dc_ep_mps(INVALID_EP)");
}

static void test_usb_dc_api_read_write(void)
{
	u32_t size;
	u8_t byte;

	/* Read invalid EP */
	zassert_not_equal(usb_read(INVALID_EP, &byte, sizeof(byte), &size),
			  TC_PASS, "usb_read(INVALID_EP)");

	/* Write to invalid EP */
	zassert_not_equal(usb_write(INVALID_EP, &byte, sizeof(byte), &size),
			  TC_PASS, "usb_write(INVALID_EP)");
}

/* test case main entry */
void test_main(void)
{
	int ret;

	ret = usb_enable(NULL);
	if (ret != 0) {
		printk("Failed to enable USB\n");
		return;
	}

	ztest_test_suite(test_device,
			 /* Test API for not USB attached state */
			 ztest_unit_test(test_usb_dc_api_invalid),
			 ztest_unit_test(test_usb_dc_api),
			 ztest_unit_test(test_usb_dc_api_read_write),
			 ztest_unit_test(test_usb_dc_api_invalid),
			 ztest_unit_test(test_usb_deconfig),
			 ztest_unit_test(test_usb_disable));

	ztest_run_test_suite(test_device);
}
