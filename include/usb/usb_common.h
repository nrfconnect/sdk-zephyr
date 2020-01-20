/***************************************************************************
 *
 *
 * Copyright(c) 2015,2016 Intel Corporation.
 * Copyright(c) 2017 PHYTEC Messtechnik GmbH
 * Copyright(c) 2018 Nordic Semiconductor ASA
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in
 * the documentation and/or other materials provided with the
 * distribution.
 * * Neither the name of Intel Corporation nor the names of its
 * contributors may be used to endorse or promote products derived
 * from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 ***************************************************************************/

/**
 * @file
 * @brief useful constants and macros for the USB application
 *
 * This file contains useful constants and macros for the USB applications.
 */

#include <version.h>

#ifndef ZEPHYR_INCLUDE_USB_USB_COMMON_H_
#define ZEPHYR_INCLUDE_USB_USB_COMMON_H_

#define BCD(x) ((((x) / 10) << 4) | ((x) % 10))

/* Descriptor size in bytes */
#define USB_DEVICE_DESC_SIZE		18
#define USB_CONFIGURATION_DESC_SIZE	9
#define USB_INTERFACE_DESC_SIZE		9
#define USB_ENDPOINT_DESC_SIZE		7
#define USB_STRING_DESC_SIZE		4
#define USB_HID_DESC_SIZE		9
#define USB_DFU_DESC_SIZE		9
#define USB_DEVICE_QUAL_DESC_SIZE	10
#define USB_INTERFACE_ASSOC_DESC_SIZE	8

/* Descriptor type */
#define USB_DEVICE_DESC			0x01
#define USB_CONFIGURATION_DESC		0x02
#define USB_STRING_DESC			0x03
#define USB_INTERFACE_DESC		0x04
#define USB_ENDPOINT_DESC		0x05
#define USB_DEVICE_QUAL_DESC		0x06
#define USB_OTHER_SPEED			0x07
#define USB_INTERFACE_POWER		0x08
#define USB_INTERFACE_ASSOC_DESC	0x0B
#define USB_DEVICE_CAPABILITY_DESC	0x10
#define USB_HID_DESC			0x21
#define USB_HID_REPORT_DESC		0x22
#define USB_CS_INTERFACE_DESC		0x24
#define USB_CS_ENDPOINT_DESC		0x25
#define USB_DFU_FUNCTIONAL_DESC		0x21
#define USB_ASSOCIATION_DESC		0x0B
#define USB_BINARY_OBJECT_STORE_DESC	0x0F

/* Useful define */
#define USB_1_1				0x0110
#define USB_2_0				0x0200
/* Set USB version to 2.1 so that the host will request the BOS descriptor */
#define USB_2_1				0x0210

#define BCDDEVICE_RELNUM		(BCD(KERNEL_VERSION_MAJOR) << 8 | \
					BCD(KERNEL_VERSION_MINOR))

/* Highest value of Frame Number in SOF packets. */
#define USB_SOF_MAX			2047

/* bmAttributes:
 * D7:Reserved, always 1,
 * D6:Self-Powered -> 1,
 * D5:Remote Wakeup -> 0,
 * D4...0:Reserved -> 0
 */
#define USB_CONFIGURATION_ATTRIBUTES_REMOTE_WAKEUP	BIT(5)
#define USB_CONFIGURATION_ATTRIBUTES_SELF_POWERED	BIT(6)
#define USB_CONFIGURATION_ATTRIBUTES BIT(7) \
	| ((COND_CODE_1(CONFIG_USB_SELF_POWERED, \
	   (USB_CONFIGURATION_ATTRIBUTES_SELF_POWERED), (0)))   \
	| (COND_CODE_1(CONFIG_USB_DEVICE_REMOTE_WAKEUP,         \
	   (USB_CONFIGURATION_ATTRIBUTES_REMOTE_WAKEUP), (0))))

/* Classes */
#define COMMUNICATION_DEVICE_CLASS	0x02
#define COMMUNICATION_DEVICE_CLASS_DATA	0x0A
#define HID_CLASS			0x03
#define MASS_STORAGE_CLASS		0x08
#define WIRELESS_DEVICE_CLASS		0xE0
#define MISC_CLASS			0xEF
#define CUSTOM_CLASS			0xFF
#define DFU_DEVICE_CLASS		0xFE

/* Sub-classes */
#define CDC_NCM_SUBCLASS		0x0d
#define BOOT_INTERFACE_SUBCLASS		0x01
#define SCSI_TRANSPARENT_SUBCLASS	0x06
#define DFU_INTERFACE_SUBCLASS		0x01
#define RF_SUBCLASS			0x01
#define CUSTOM_SUBCLASS			0xFF
/* Misc subclasses */
#define MISC_RNDIS_SUBCLASS		0x04

/* Protocols */
#define V25TER_PROTOCOL			0x01
#define MOUSE_PROTOCOL			0x02
#define BULK_ONLY_PROTOCOL		0x50
#define DFU_RUNTIME_PROTOCOL		0x01
#define DFU_MODE_PROTOCOL		0x02
#define BLUETOOTH_PROTOCOL		0x01
/* CDC ACM protocols */
#define ACM_VENDOR_PROTOCOL		0xFF
/* Misc protocols */
#define MISC_ETHERNET_PROTOCOL		0x01

/** Standard Device Descriptor */
struct usb_device_descriptor {
	u8_t bLength;
	u8_t bDescriptorType;
	u16_t bcdUSB;
	u8_t bDeviceClass;
	u8_t bDeviceSubClass;
	u8_t bDeviceProtocol;
	u8_t bMaxPacketSize0;
	u16_t idVendor;
	u16_t idProduct;
	u16_t bcdDevice;
	u8_t iManufacturer;
	u8_t iProduct;
	u8_t iSerialNumber;
	u8_t bNumConfigurations;
} __packed;

/** Unicode (UTF16LE) String Descriptor */
struct usb_string_descriptor {
	u8_t bLength;
	u8_t bDescriptorType;
	u16_t bString;
} __packed;

/** Association Descriptor */
struct usb_association_descriptor {
	u8_t bLength;
	u8_t bDescriptorType;
	u8_t bFirstInterface;
	u8_t bInterfaceCount;
	u8_t bFunctionClass;
	u8_t bFunctionSubClass;
	u8_t bFunctionProtocol;
	u8_t iFunction;
} __packed;

/** Standard Configuration Descriptor */
struct usb_cfg_descriptor {
	u8_t bLength;
	u8_t bDescriptorType;
	u16_t wTotalLength;
	u8_t bNumInterfaces;
	u8_t bConfigurationValue;
	u8_t iConfiguration;
	u8_t bmAttributes;
	u8_t bMaxPower;
} __packed;

/** Standard Interface Descriptor */
struct usb_if_descriptor {
	u8_t bLength;
	u8_t bDescriptorType;
	u8_t bInterfaceNumber;
	u8_t bAlternateSetting;
	u8_t bNumEndpoints;
	u8_t bInterfaceClass;
	u8_t bInterfaceSubClass;
	u8_t bInterfaceProtocol;
	u8_t iInterface;
} __packed;

/** Standard Endpoint Descriptor */
struct usb_ep_descriptor {
	u8_t bLength;
	u8_t bDescriptorType;
	u8_t bEndpointAddress;
	u8_t bmAttributes;
	u16_t wMaxPacketSize;
	u8_t bInterval;
} __packed;

#endif /* ZEPHYR_INCLUDE_USB_USB_COMMON_H_ */
