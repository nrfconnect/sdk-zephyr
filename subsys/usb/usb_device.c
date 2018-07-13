/*
 *  LPCUSB, an USB device driver for LPC microcontrollers
 *  Copyright (C) 2006 Bertrik Sikken (bertrik@sikken.nl)
 *  Copyright (c) 2016 Intel Corporation
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 *  IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 *  OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 *  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 *  NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 *  THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @file
 * @brief USB device core layer
 *
 * This module handles control transfer handler, standard request handler and
 * USB Interface for customer application.
 *
 * Control transfers handler is normally installed on the
 * endpoint 0 callback.
 *
 * Control transfers can be of the following type:
 * 0 Standard;
 * 1 Class;
 * 2 Vendor;
 * 3 Reserved.
 *
 * A callback can be installed for each of these control transfers using
 * usb_register_request_handler.
 * When an OUT request arrives, data is collected in the data store provided
 * with the usb_register_request_handler call. When the transfer is done, the
 * callback is called.
 * When an IN request arrives, the callback is called immediately to either
 * put the control transfer data in the data store, or to get a pointer to
 * control transfer data. The data is then packetized and sent to the host.
 *
 * Standard request handler handles the 'chapter 9' processing, specifically
 * the standard device requests in table 9-3 from the universal serial bus
 * specification revision 2.0
 */

#include <errno.h>
#include <stddef.h>
#include <misc/util.h>
#include <misc/__assert.h>
#include <init.h>
#include <board.h>
#if defined(USB_VUSB_EN_GPIO)
#include <gpio.h>
#endif
#include <usb/usb_device.h>
#include <usb/usbstruct.h>
#include <usb/usb_common.h>
#include <usb_descriptor.h>

#define SYS_LOG_LEVEL CONFIG_SYS_LOG_USB_DEVICE_LEVEL
#define SYS_LOG_NO_NEWLINE
#include <logging/sys_log.h>

#include <usb/bos.h>
#include <os_desc.h>

#define MAX_DESC_HANDLERS           4 /** Device, interface, endpoint, other */

/* general descriptor field offsets */
#define DESC_bLength                0 /** Length offset */
#define DESC_bDescriptorType        1 /** Descriptor type offset */

/* config descriptor field offsets */
#define CONF_DESC_wTotalLength      2 /** Total length offset */
#define CONF_DESC_bConfigurationValue 5 /** Configuration value offset */
#define CONF_DESC_bmAttributes      7 /** configuration characteristics */

/* interface descriptor field offsets */
#define INTF_DESC_bInterfaceNumber  2 /** Interface number offset */
#define INTF_DESC_bAlternateSetting 3 /** Alternate setting offset */

/* endpoint descriptor field offsets */
#define ENDP_DESC_bEndpointAddress  2 /** Endpoint address offset */
#define ENDP_DESC_bmAttributes      3 /** Bulk or interrupt? */
#define ENDP_DESC_wMaxPacketSize    4 /** Maximum packet size offset */

#define MAX_NUM_REQ_HANDLERS        4
#define MAX_STD_REQ_MSG_SIZE        8

#define MAX_NUM_TRANSFERS           4 /** Max number of parallel transfers */

/* Default USB control EP, always 0 and 0x80 */
#define USB_CONTROL_OUT_EP0         0
#define USB_CONTROL_IN_EP0          0x80

/* Linker-defined symbols bound the USB descriptor structs */
extern struct usb_cfg_data __usb_data_start[];
extern struct usb_cfg_data __usb_data_end[];

struct usb_transfer_data {
	/** endpoint associated to the transfer */
	u8_t ep;
	/** Transfer status */
	int status;
	/** Transfer read/write buffer */
	u8_t *buffer;
	/** Transfer buffer size */
	size_t bsize;
	/** Transferred size */
	size_t tsize;
	/** Transfer callback */
	usb_transfer_callback cb;
	/** Transfer caller private data */
	void *priv;
	/** Transfer synchronization semaphore */
	struct k_sem sem;
	/** Transfer read/write work */
	struct k_work work;
	/** Transfer flags */
	unsigned int flags;
};

static void usb_transfer_work(struct k_work *item);

static struct usb_dev_priv {
	/** Setup packet */
	struct usb_setup_packet setup;
	/** Pointer to data buffer */
	u8_t *data_buf;
	/** Eemaining bytes in buffer */
	s32_t data_buf_residue;
	/** Total length of control transfer */
	s32_t data_buf_len;
	/** Installed custom request handler */
	usb_request_handler custom_req_handler;
	/** Installed vendor request handler */
	usb_request_handler vendor_req_handler;
	/** USB stack status clalback */
	usb_status_callback status_callback;
	/** Pointer to registered descriptors */
	const u8_t *descriptors;
	/** Array of installed request handler callbacks */
	usb_request_handler req_handlers[MAX_NUM_REQ_HANDLERS];
	/** Array of installed request data pointers */
	u8_t *data_store[MAX_NUM_REQ_HANDLERS];
	/* Buffer used for storing standard usb request data */
	u8_t std_req_data[MAX_STD_REQ_MSG_SIZE];
	/** Variable to check whether the usb has been enabled */
	bool enabled;
	/** Currently selected configuration */
	u8_t configuration;
	/** Transfer list */
	struct usb_transfer_data transfer[MAX_NUM_TRANSFERS];
} usb_dev;

/*
 * @brief print the contents of a setup packet
 *
 * @param [in] setup The setup packet
 *
 */
static void usb_print_setup(struct usb_setup_packet *setup)
{
	/* avoid compiler warning if SYS_LOG_DBG is not defined */
	ARG_UNUSED(setup);

	SYS_LOG_DBG("SETUP\n");
	SYS_LOG_DBG("%x %x %x %x %x\n",
	    setup->bmRequestType,
	    setup->bRequest,
	    setup->wValue,
	    setup->wIndex,
	    setup->wLength);
}

/*
 * @brief handle a request by calling one of the installed request handlers
 *
 * Local function to handle a request by calling one of the installed request
 * handlers. In case of data going from host to device, the data is at *ppbData.
 * In case of data going from device to host, the handler can either choose to
 * write its data at *ppbData or update the data pointer.
 *
 * @param [in]     setup The setup packet
 * @param [in,out] len   Pointer to data length
 * @param [in,out] data  Data buffer
 *
 * @return true if the request was handles successfully
 */
static bool usb_handle_request(struct usb_setup_packet *setup,
			       s32_t *len, u8_t **data)
{
	u32_t type = REQTYPE_GET_TYPE(setup->bmRequestType);
	usb_request_handler handler = usb_dev.req_handlers[type];

	SYS_LOG_DBG("** %d **\n", type);

	if (type >= MAX_NUM_REQ_HANDLERS) {
		SYS_LOG_DBG("Error Incorrect iType %d\n", type);
		return false;
	}

	if (handler == NULL) {
		SYS_LOG_DBG("No handler for reqtype %d\n", type);
		return false;
	}

	if ((*handler)(setup, len, data) < 0) {
		SYS_LOG_DBG("Handler Error %d\n", type);
		usb_print_setup(setup);
		return false;
	}

	return true;
}

/*
 * @brief send next chunk of data (possibly 0 bytes) to host
 *
 * @return N/A
 */
static void usb_data_to_host(void)
{
	u32_t chunk = min(MAX_PACKET_SIZE0, usb_dev.data_buf_residue);

	/*Always EP0 for control*/
	usb_dc_ep_write(0x80, usb_dev.data_buf, chunk, &chunk);
	usb_dev.data_buf += chunk;
	usb_dev.data_buf_residue -= chunk;
}

/*
 * @brief handle IN/OUT transfers on EP0
 *
 * @param [in] ep        Endpoint address
 * @param [in] ep_status Endpoint status
 *
 * @return N/A
 */
static void usb_handle_control_transfer(u8_t ep,
					enum usb_dc_ep_cb_status_code ep_status)
{
	u32_t chunk = 0;
	u32_t type = 0;
	struct usb_setup_packet *setup = &usb_dev.setup;

	SYS_LOG_DBG("usb_handle_control_transfer ep %x, status %x\n", ep,
		    ep_status);
	if (ep == USB_CONTROL_OUT_EP0 && ep_status == USB_DC_EP_SETUP) {
		/*
		 * OUT transfer, Setup packet,
		 * reset request message state machine
		 */
		if (usb_dc_ep_read(ep,
				   (u8_t *)setup, sizeof(*setup), NULL) < 0) {
			SYS_LOG_DBG("Read Setup Packet failed\n");
			usb_dc_ep_set_stall(USB_CONTROL_IN_EP0);
			return;
		}

		/* Defaults for data pointer and residue */
		type = REQTYPE_GET_TYPE(setup->bmRequestType);
		usb_dev.data_buf = usb_dev.data_store[type];
		if (!usb_dev.data_buf) {
			SYS_LOG_DBG("buffer not available\n");
			usb_dc_ep_set_stall(USB_CONTROL_OUT_EP0);
			usb_dc_ep_set_stall(USB_CONTROL_IN_EP0);
			return;
		}

		usb_dev.data_buf_residue = setup->wLength;
		usb_dev.data_buf_len = setup->wLength;

		if (setup->wLength &&
		    REQTYPE_GET_DIR(setup->bmRequestType)
		    == REQTYPE_DIR_TO_DEVICE) {
			return;
		}

		/* Ask installed handler to process request */
		if (!usb_handle_request(setup,
					&usb_dev.data_buf_len,
					&usb_dev.data_buf)) {
			SYS_LOG_DBG("usb_handle_request failed\n");
			usb_dc_ep_set_stall(USB_CONTROL_IN_EP0);
			return;
		}

		/* Send smallest of requested and offered length */
		usb_dev.data_buf_residue = min(usb_dev.data_buf_len,
					       setup->wLength);
		/* Send first part (possibly a zero-length status message) */
		usb_data_to_host();
	} else if (ep == USB_CONTROL_OUT_EP0) {
		/* OUT transfer, data or status packets */
		if (usb_dev.data_buf_residue <= 0) {
			/* absorb zero-length status message */
			if (usb_dc_ep_read(USB_CONTROL_OUT_EP0,
					   usb_dev.data_buf, 0, &chunk) < 0) {
				SYS_LOG_DBG("Read DATA Packet failed\n");
				usb_dc_ep_set_stall(USB_CONTROL_IN_EP0);
			}
			return;
		}

		if (usb_dc_ep_read(USB_CONTROL_OUT_EP0,
				   usb_dev.data_buf,
				   usb_dev.data_buf_residue, &chunk) < 0) {
			SYS_LOG_DBG("Read DATA Packet failed\n");
			usb_dc_ep_set_stall(USB_CONTROL_IN_EP0);
			usb_dc_ep_set_stall(USB_CONTROL_OUT_EP0);
			return;
		}

		usb_dev.data_buf += chunk;
		usb_dev.data_buf_residue -= chunk;
		if (usb_dev.data_buf_residue == 0) {
			/* Received all, send data to handler */
			type = REQTYPE_GET_TYPE(setup->bmRequestType);
			usb_dev.data_buf = usb_dev.data_store[type];
			if (!usb_handle_request(setup,
						&usb_dev.data_buf_len,
						&usb_dev.data_buf)) {
				SYS_LOG_DBG("usb_handle_request1 failed\n");
				usb_dc_ep_set_stall(USB_CONTROL_IN_EP0);
				return;
			}

			/*Send status to host*/
			SYS_LOG_DBG(">> usb_data_to_host(2)\n");
			usb_data_to_host();
		}
	} else if (ep == USB_CONTROL_IN_EP0) {
		/* Send more data if available */
		if (usb_dev.data_buf_residue != 0) {
			usb_data_to_host();
		}
	} else {
		__ASSERT_NO_MSG(false);
	}
}

/*
 * @brief register a callback for handling requests
 *
 * @param [in] type       Type of request, e.g. REQTYPE_TYPE_STANDARD
 * @param [in] handler    Callback function pointer
 * @param [in] data_store Data storage area for this type of request
 *
 * @return N/A
 */
static void usb_register_request_handler(s32_t type,
					 usb_request_handler handler,
					 u8_t *data_store)
{
	usb_dev.req_handlers[type] = handler;
	usb_dev.data_store[type] = data_store;
}

/*
 * @brief register a pointer to a descriptor block
 *
 * This function registers a pointer to a descriptor block containing all
 * descriptors for the device.
 *
 * @param [in] usb_descriptors The descriptor byte array
 */
static void usb_register_descriptors(const u8_t *usb_descriptors)
{
	usb_dev.descriptors = usb_descriptors;
}

/*
 * @brief get specified USB descriptor
 *
 * This function parses the list of installed USB descriptors and attempts
 * to find the specified USB descriptor.
 *
 * @param [in]  type_index Type and index of the descriptor
 * @param [in]  lang_id    Language ID of the descriptor (currently unused)
 * @param [out] len        Descriptor length
 * @param [out] data       Descriptor data
 *
 * @return true if the descriptor was found, false otherwise
 */
static bool usb_get_descriptor(u16_t type_index, u16_t lang_id,
		s32_t *len, u8_t **data)
{
	u8_t type = 0;
	u8_t index = 0;
	u8_t *p = NULL;
	s32_t cur_index = 0;
	bool found = false;

	/*Avoid compiler warning until this is used for something*/
	ARG_UNUSED(lang_id);

	type = GET_DESC_TYPE(type_index);
	index = GET_DESC_INDEX(type_index);

	/*
	 * Invalid types of descriptors,
	 * see USB Spec. Revision 2.0, 9.4.3 Get Descriptor
	 */
	if ((type == DESC_INTERFACE) || (type == DESC_ENDPOINT) ||
	    (type > DESC_OTHER_SPEED)) {
		return false;
	}

	p = (u8_t *)usb_dev.descriptors;
	cur_index = 0;

	while (p[DESC_bLength] != 0) {
		if (p[DESC_bDescriptorType] == type) {
			if (cur_index == index) {
				found = true;
				break;
			}
			cur_index++;
		}
		/* skip to next descriptor */
		p += p[DESC_bLength];
	}

	if (found) {
		/* set data pointer */
		*data = p;
		/* get length from structure */
		if (type == DESC_CONFIGURATION) {
			/* configuration descriptor is an
			 * exception, length is at offset
			 * 2 and 3
			 */
			*len = (p[CONF_DESC_wTotalLength]) |
			    (p[CONF_DESC_wTotalLength + 1] << 8);
		} else {
			/* normally length is at offset 0 */
			*len = p[DESC_bLength];
		}
	} else {
		/* nothing found */
		SYS_LOG_DBG("Desc %x not found!\n", type_index);
	}
	return found;
}

/*
 * @brief set USB configuration
 *
 * This function configures the device according to the specified configuration
 * index and alternate setting by parsing the installed USB descriptor list.
 * A configuration index of 0 unconfigures the device.
 *
 * @param [in] config_index Configuration index
 * @param [in] alt_setting  Alternate setting number
 *
 * @return true if successfully configured false if error or unconfigured
 */
static bool usb_set_configuration(u8_t config_index, u8_t alt_setting)
{
	u8_t *p = NULL;
	u8_t cur_config = 0;
	u8_t cur_alt_setting = 0;

	if (config_index == 0) {
		/* unconfigure device */
		SYS_LOG_DBG("Device not configured - invalid configuration "
			    "offset\n");
		return true;
	}

	/* configure endpoints for this configuration/altsetting */
	p = (u8_t *)usb_dev.descriptors;
	cur_config = 0xFF;
	cur_alt_setting = 0xFF;

	while (p[DESC_bLength] != 0) {
		switch (p[DESC_bDescriptorType]) {
		case DESC_CONFIGURATION:
			/* remember current configuration index */
			cur_config = p[CONF_DESC_bConfigurationValue];
			break;

		case DESC_INTERFACE:
			/* remember current alternate setting */
			cur_alt_setting =
			    p[INTF_DESC_bAlternateSetting];
			break;

		case DESC_ENDPOINT:
			if ((cur_config == config_index) &&
			    (cur_alt_setting == alt_setting)) {
				struct usb_dc_ep_cfg_data ep_cfg;
				/* endpoint found for desired config
				 * and alternate setting
				 */
				ep_cfg.ep_type =
				    p[ENDP_DESC_bmAttributes];
				ep_cfg.ep_mps =
				    (p[ENDP_DESC_wMaxPacketSize]) |
				    (p[ENDP_DESC_wMaxPacketSize + 1]
					    << 8);
				ep_cfg.ep_addr =
				    p[ENDP_DESC_bEndpointAddress];
				usb_dc_ep_configure(&ep_cfg);
				usb_dc_ep_enable(ep_cfg.ep_addr);
			}
			break;

		default:
			break;
		}
		/* skip to next descriptor */
		p += p[DESC_bLength];
	}

	if (usb_dev.status_callback) {
		usb_dev.status_callback(USB_DC_CONFIGURED, &config_index);
	}

	return true;
}

/*
 * @brief set USB interface
 *
 * @param [in] iface Interface index
 * @param [in] alt_setting  Alternate setting number
 *
 * @return true if successfully configured false if error or unconfigured
 */
static bool usb_set_interface(u8_t iface, u8_t alt_setting)
{
	const u8_t *p = usb_dev.descriptors;
	u8_t cur_iface = 0xFF;
	u8_t cur_alt_setting = 0xFF;
	struct usb_dc_ep_cfg_data ep_cfg;

	SYS_LOG_DBG("iface %u alt_setting %u\n", iface, alt_setting);

	while (p[DESC_bLength] != 0) {
		switch (p[DESC_bDescriptorType]) {
		case DESC_INTERFACE:
			/* remember current alternate setting */
			cur_alt_setting = p[INTF_DESC_bAlternateSetting];
			cur_iface = p[INTF_DESC_bInterfaceNumber];
			break;
		case DESC_ENDPOINT:
			if ((cur_iface != iface) ||
			    (cur_alt_setting != alt_setting)) {
				break;
			}

			/* Endpoint is found for desired interface and
			 * alternate setting
			 */
			ep_cfg.ep_type = p[ENDP_DESC_bmAttributes];
			ep_cfg.ep_mps = (p[ENDP_DESC_wMaxPacketSize]) |
				(p[ENDP_DESC_wMaxPacketSize + 1] << 8);
			ep_cfg.ep_addr = p[ENDP_DESC_bEndpointAddress];
			usb_dc_ep_configure(&ep_cfg);
			usb_dc_ep_enable(ep_cfg.ep_addr);

			SYS_LOG_DBG("Found: ep_addr 0x%x\n", ep_cfg.ep_addr);
			break;
		default:
			break;
		}

		/* skip to next descriptor */
		p += p[DESC_bLength];
		SYS_LOG_DBG("p %p\n", p);
	}

	if (usb_dev.status_callback) {
		usb_dev.status_callback(USB_DC_INTERFACE, &iface);
	}

	return true;
}

/*
 * @brief handle a standard device request
 *
 * @param [in]     setup    The setup packet
 * @param [in,out] len      Pointer to data length
 * @param [in,out] data_buf Data buffer
 *
 * @return true if the request was handled successfully
 */
static bool usb_handle_std_device_req(struct usb_setup_packet *setup,
		s32_t *len, u8_t **data_buf)
{
	bool ret = true;
	u8_t *data = *data_buf;

	switch (setup->bRequest) {
	case REQ_GET_STATUS:
		SYS_LOG_DBG("REQ_GET_STATUS\n");
		/* bit 0: self-powered */
		/* bit 1: remote wakeup = not supported */
		data[0] = 0;
		data[1] = 0;
		*len = 2;
		break;

	case REQ_SET_ADDRESS:
		SYS_LOG_DBG("REQ_SET_ADDRESS, addr 0x%x\n", setup->wValue);
		usb_dc_set_address(setup->wValue);
		break;

	case REQ_GET_DESCRIPTOR:
		SYS_LOG_DBG("REQ_GET_DESCRIPTOR\n");
		ret = usb_get_descriptor(setup->wValue,
		    setup->wIndex, len, data_buf);
		break;

	case REQ_GET_CONFIGURATION:
		SYS_LOG_DBG("REQ_GET_CONFIGURATION\n");
		/* indicate if we are configured */
		data[0] = usb_dev.configuration;
		*len = 1;
		break;

	case REQ_SET_CONFIGURATION:
		SYS_LOG_DBG("REQ_SET_CONFIGURATION, conf 0x%x\n",
			    setup->wValue & 0xFF);
		if (!usb_set_configuration(setup->wValue & 0xFF, 0)) {
			SYS_LOG_DBG("USBSetConfiguration failed!\n");
			ret = false;
		} else {
			/* configuration successful,
			 * update current configuration
			 */
			usb_dev.configuration = setup->wValue & 0xFF;
		}
		break;

	case REQ_CLEAR_FEATURE:
		SYS_LOG_DBG("REQ_CLEAR_FEATURE\n");
		break;
	case REQ_SET_FEATURE:
		SYS_LOG_DBG("REQ_SET_FEATURE\n");

		if (setup->wValue == FEA_REMOTE_WAKEUP) {
			/* put DEVICE_REMOTE_WAKEUP code here */
		}

		if (setup->wValue == FEA_TEST_MODE) {
			/* put TEST_MODE code here */
		}
		ret = false;
		break;

	case REQ_SET_DESCRIPTOR:
		SYS_LOG_DBG("Device req %x not implemented\n", setup->bRequest);
		ret = false;
		break;

	default:
		SYS_LOG_DBG("Illegal device req %x\n", setup->bRequest);
		ret = false;
		break;
	}

	return ret;
}

/*
 * @brief handle a standard interface request
 *
 * @param [in]     setup    The setup packet
 * @param [in,out] len      Pointer to data length
 * @param [in]     data_buf Data buffer
 *
 * @return true if the request was handled successfully
 */
static bool usb_handle_std_interface_req(struct usb_setup_packet *setup,
		s32_t *len, u8_t **data_buf)
{
	u8_t *data = *data_buf;

	switch (setup->bRequest) {
	case REQ_GET_STATUS:
		/* no bits specified */
		data[0] = 0;
		data[1] = 0;
		*len = 2;
		break;

	case REQ_CLEAR_FEATURE:
	case REQ_SET_FEATURE:
		/* not defined for interface */
		return false;

	case REQ_GET_INTERFACE:
		/* there is only one interface, return n-1 (= 0) */
		data[0] = 0;
		*len = 1;
		break;

	case REQ_SET_INTERFACE:
		SYS_LOG_DBG("REQ_SET_INTERFACE\n");
		usb_set_interface(setup->wIndex, setup->wValue);
		*len = 0;
		break;

	default:
		SYS_LOG_DBG("Illegal interface req %d\n", setup->bRequest);
		return false;
	}

	return true;
}

/*
 * @brief handle a standard endpoint request
 *
 * @param [in]     setup    The setup packet
 * @param [in,out] len      Pointer to data length
 * @param [in]     data_buf Data buffer
 *
 * @return true if the request was handled successfully
 */
static bool usb_handle_std_endpoint_req(struct usb_setup_packet *setup,
		s32_t *len, u8_t **data_buf)
{
	u8_t *data = *data_buf;
	u8_t ep = setup->wIndex;

	switch (setup->bRequest) {
	case REQ_GET_STATUS:
		/* bit 0 = endpointed halted or not */
		usb_dc_ep_is_stalled(ep, &data[0]);
		data[1] = 0;
		*len = 2;
		break;

	case REQ_CLEAR_FEATURE:
		if (setup->wValue == FEA_ENDPOINT_HALT) {
			/* clear HALT by unstalling */
			SYS_LOG_INF("... EP clear halt %x\n", ep);
			usb_dc_ep_clear_stall(ep);
			if (usb_dev.status_callback) {
				usb_dev.status_callback(USB_DC_CLEAR_HALT, &ep);
			}
			break;
		}
		/* only ENDPOINT_HALT defined for endpoints */
		return false;

	case REQ_SET_FEATURE:
		if (setup->wValue == FEA_ENDPOINT_HALT) {
			/* set HALT by stalling */
			SYS_LOG_INF("--- EP SET halt %x\n", ep);
			usb_dc_ep_set_stall(ep);
			if (usb_dev.status_callback) {
				usb_dev.status_callback(USB_DC_SET_HALT, &ep);
			}
			break;
		}
		/* only ENDPOINT_HALT defined for endpoints */
		return false;

	case REQ_SYNCH_FRAME:
		SYS_LOG_DBG("EP req %d not implemented\n", setup->bRequest);
		return false;

	default:
		SYS_LOG_DBG("Illegal EP req %d\n", setup->bRequest);
		return false;
	}

	return true;
}


/*
 * @brief default handler for standard ('chapter 9') requests
 *
 * If a custom request handler was installed, this handler is called first.
 *
 * @param [in]     setup    The setup packet
 * @param [in,out] len      Pointer to data length
 * @param [in]     data_buf Data buffer
 *
 * @return true if the request was handled successfully
 */
static int usb_handle_standard_request(struct usb_setup_packet *setup,
				       s32_t *len, u8_t **data_buf)
{
	int rc = 0;

	if (!usb_handle_bos(setup, len, data_buf)) {
		return 0;
	}

	if (!usb_handle_os_desc(setup, len, data_buf)) {
		return 0;
	}

	/* try the custom request handler first */
	if (usb_dev.custom_req_handler &&
	    !usb_dev.custom_req_handler(setup, len, data_buf)) {
		return 0;
	}

	switch (REQTYPE_GET_RECIP(setup->bmRequestType)) {
	case REQTYPE_RECIP_DEVICE:
		if (usb_handle_std_device_req(setup, len, data_buf) == false)
			rc = -EINVAL;
		break;
	case REQTYPE_RECIP_INTERFACE:
		if (usb_handle_std_interface_req(setup, len, data_buf) == false)
			rc = -EINVAL;
		break;
	case REQTYPE_RECIP_ENDPOINT:
		if (usb_handle_std_endpoint_req(setup, len, data_buf) == false)
			rc = -EINVAL;
		break;
	default:
		rc = -EINVAL;
	}
	return rc;
}

static int usb_handle_vendor_request(struct usb_setup_packet *setup,
				     s32_t *len, u8_t **data_buf)
{
	SYS_LOG_DBG("\n");

	if (usb_os_desc_enabled()) {
		if (!usb_handle_os_desc_feature(setup, len, data_buf)) {
			return 0;
		}
	}

	if (usb_dev.vendor_req_handler) {
		return usb_dev.vendor_req_handler(setup, len, data_buf);
	}

	return -ENOTSUP;
}

/*
 * @brief Registers a callback for custom device requests
 *
 * In usb_register_custom_req_handler, the custom request handler gets a first
 * chance at handling the request before it is handed over to the 'chapter 9'
 * request handler.
 *
 * This can be used for example in HID devices, where a REQ_GET_DESCRIPTOR
 * request is sent to an interface, which is not covered by the 'chapter 9'
 * specification.
 *
 * @param [in] handler Callback function pointer
 */
static void usb_register_custom_req_handler(usb_request_handler handler)
{
	usb_dev.custom_req_handler = handler;
}

/*
 * @brief register a callback for device status
 *
 * This function registers a callback for device status. The registered callback
 * is used to report changes in the status of the device controller.
 *
 * @param [in] cb Callback function pointer
 */
static void usb_register_status_callback(usb_status_callback cb)
{
	usb_dev.status_callback = cb;
}

/**
 * @brief turn on/off USB VBUS voltage
 *
 * @param on Set to false to turn off and to true to turn on VBUS
 *
 * @return 0 on success, negative errno code on fail
 */
static int usb_vbus_set(bool on)
{
#if defined(USB_VUSB_EN_GPIO)
	int ret = 0;
	struct device *gpio_dev = device_get_binding(USB_GPIO_DRV_NAME);

	if (!gpio_dev) {
		SYS_LOG_DBG("USB requires GPIO. Cannot find %s!\n",
			    USB_GPIO_DRV_NAME);
		return -ENODEV;
	}

	/* Enable USB IO */
	ret = gpio_pin_configure(gpio_dev, USB_VUSB_EN_GPIO, GPIO_DIR_OUT);
	if (ret)
		return ret;

	ret = gpio_pin_write(gpio_dev, USB_VUSB_EN_GPIO, on == true ? 1 : 0);
	if (ret)
		return ret;
#endif

	return 0;
}

int usb_set_config(struct usb_cfg_data *config)
{
	if (!config)
		return -EINVAL;

	/* register descriptors */
	usb_register_descriptors(config->usb_device_description);

	/* register standard request handler */
	usb_register_request_handler(REQTYPE_TYPE_STANDARD,
				     usb_handle_standard_request,
				     usb_dev.std_req_data);

	/* register class request handlers for each interface*/
	if (config->interface.class_handler != NULL) {
		usb_register_request_handler(REQTYPE_TYPE_CLASS,
					     config->interface.class_handler,
					     config->interface.payload_data);
	}

	/* register vendor request handler */
	if (config->interface.vendor_handler || usb_os_desc_enabled()) {
		usb_register_request_handler(REQTYPE_TYPE_VENDOR,
					     usb_handle_vendor_request,
					     config->interface.vendor_data);

		if (config->interface.vendor_handler) {
			usb_dev.vendor_req_handler =
				config->interface.vendor_handler;
		}
	}

	/* register class request handlers for each interface*/
	if (config->interface.custom_handler != NULL) {
		usb_register_custom_req_handler(
		    config->interface.custom_handler);
	}

	/* register status callback */
	if (config->cb_usb_status != NULL) {
		usb_register_status_callback(config->cb_usb_status);
	}

	return 0;
}

int usb_deconfig(void)
{
	/* unregister descriptors */
	usb_register_descriptors(NULL);

	/* unegister standard request handler */
	usb_register_request_handler(REQTYPE_TYPE_STANDARD, NULL, NULL);

	/* unregister class request handlers for each interface*/
	usb_register_request_handler(REQTYPE_TYPE_CLASS, NULL, NULL);

	/* unregister class request handlers for each interface*/
	usb_register_custom_req_handler(NULL);

	/* unregister status callback */
	usb_register_status_callback(NULL);

	/* Reset USB controller */
	usb_dc_reset();

	return 0;
}

int usb_enable(struct usb_cfg_data *config)
{
	int ret;
	u32_t i;
	struct usb_dc_ep_cfg_data ep0_cfg;

	if (true == usb_dev.enabled) {
		return 0;
	}

	/* Enable VBUS if needed */
	ret = usb_vbus_set(true);
	if (ret < 0)
		return ret;

	ret = usb_dc_set_status_callback(config->cb_usb_status);
	if (ret < 0)
		return ret;

	ret = usb_dc_attach();
	if (ret < 0)
		return ret;

	/* Configure control EP */
	ep0_cfg.ep_mps = MAX_PACKET_SIZE0;
	ep0_cfg.ep_type = USB_DC_EP_CONTROL;

	ep0_cfg.ep_addr = USB_CONTROL_OUT_EP0;
	ret = usb_dc_ep_configure(&ep0_cfg);
	if (ret < 0)
		return ret;

	ep0_cfg.ep_addr = USB_CONTROL_IN_EP0;
	ret = usb_dc_ep_configure(&ep0_cfg);
	if (ret < 0)
		return ret;

	/*register endpoint 0 handlers*/
	ret = usb_dc_ep_set_callback(USB_CONTROL_OUT_EP0,
	    usb_handle_control_transfer);
	if (ret < 0)
		return ret;
	ret = usb_dc_ep_set_callback(USB_CONTROL_IN_EP0,
	    usb_handle_control_transfer);
	if (ret < 0)
		return ret;

	/*register endpoint handlers*/
	for (i = 0; i < config->num_endpoints; i++) {
		ret = usb_dc_ep_set_callback(config->endpoint[i].ep_addr,
		    config->endpoint[i].ep_cb);
		if (ret < 0)
			return ret;
	}

	/* init transfer slots */
	for (i = 0; i < MAX_NUM_TRANSFERS; i++) {
		k_work_init(&usb_dev.transfer[i].work, usb_transfer_work);
		k_sem_init(&usb_dev.transfer[i].sem, 1, 1);
	}

	/* enable control EP */
	ret = usb_dc_ep_enable(USB_CONTROL_OUT_EP0);
	if (ret < 0)
		return ret;

	ret = usb_dc_ep_enable(USB_CONTROL_IN_EP0);
	if (ret < 0)
		return ret;

	usb_dev.enabled = true;

	return 0;
}

int usb_disable(void)
{
	int ret;

	if (true != usb_dev.enabled) {
		/*Already disabled*/
		return 0;
	}

	ret = usb_dc_detach();
	if (ret < 0)
		return ret;

	/* Disable VBUS if needed */
	usb_vbus_set(false);

	usb_dev.enabled = false;

	return 0;
}

int usb_write(u8_t ep, const u8_t *data, u32_t data_len,
		u32_t *bytes_ret)
{
	return usb_dc_ep_write(ep, data, data_len, bytes_ret);
}

int usb_read(u8_t ep, u8_t *data, u32_t max_data_len,
		u32_t *ret_bytes)
{
	return usb_dc_ep_read(ep, data, max_data_len, ret_bytes);
}

int usb_ep_set_stall(u8_t ep)
{
	return usb_dc_ep_set_stall(ep);
}

int usb_ep_clear_stall(u8_t ep)
{
	return usb_dc_ep_clear_stall(ep);
}

int usb_ep_read_wait(u8_t ep, u8_t *data, u32_t max_data_len,
			u32_t *ret_bytes)
{
	return usb_dc_ep_read_wait(ep, data, max_data_len, ret_bytes);
}

int usb_ep_read_continue(u8_t ep)
{
	return usb_dc_ep_read_continue(ep);
}

/* Transfer management */
static struct usb_transfer_data *usb_ep_get_transfer(u8_t ep)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(usb_dev.transfer); i++) {
		if (usb_dev.transfer[i].ep == ep) {
			return &usb_dev.transfer[i];
		}
	}

	return NULL;
}

static void usb_transfer_work(struct k_work *item)
{
	struct usb_transfer_data *trans;
	int ret = 0, bytes;
	u8_t ep;

	trans = CONTAINER_OF(item, struct usb_transfer_data, work);
	ep = trans->ep;

	if (trans->status != -EBUSY) {
		/* transfer cancelled or already completed */
		goto done;
	}

	if (trans->flags & USB_TRANS_WRITE) {
		if (!trans->bsize) {
			if (!(trans->flags & USB_TRANS_NO_ZLP)) {
				usb_dc_ep_write(ep, NULL, 0, NULL);
			}
			trans->status = 0;
			goto done;
		}

		ret = usb_dc_ep_write(ep, trans->buffer, trans->bsize, &bytes);
		if (ret) {
			/* transfer error */
			trans->status = -EINVAL;
			goto done;
		}

		trans->buffer += bytes;
		trans->bsize -= bytes;
		trans->tsize += bytes;
	} else {
		ret = usb_dc_ep_read_wait(ep, trans->buffer, trans->bsize,
					  &bytes);
		if (ret) {
			/* transfer error */
			trans->status = -EINVAL;
			goto done;
		}

		trans->buffer += bytes;
		trans->bsize -= bytes;
		trans->tsize += bytes;

		/* ZLP, short-pkt or buffer full */
		if (!bytes || (bytes % usb_dc_ep_mps(ep)) || !trans->bsize) {
			/* transfer complete */
			trans->status = 0;
			goto done;
		}

		/* we expect mote data, clear NAK */
		usb_dc_ep_read_continue(ep);
	}

done:
	if (trans->status != -EBUSY && trans->cb) { /* Transfer complete */
		usb_transfer_callback cb = trans->cb;
		int tsize = trans->tsize;
		void *priv = trans->priv;

		if (k_is_in_isr()) {
			/* reschedule completion in thread context */
			k_work_submit(&trans->work);
			return;
		}

		SYS_LOG_DBG("transfer done, ep=%02x, status=%d, size=%u\n",
			    trans->ep, trans->status, trans->tsize);

		trans->cb = NULL;
		k_sem_give(&trans->sem);

		/* Transfer completion callback */
		cb(ep, tsize, priv);
	}
}

void usb_transfer_ep_callback(u8_t ep, enum usb_dc_ep_cb_status_code status)
{
	struct usb_transfer_data *trans = usb_ep_get_transfer(ep);

	if (status != USB_DC_EP_DATA_IN && status != USB_DC_EP_DATA_OUT) {
		return;
	}

	if (!trans) {
		if (status == USB_DC_EP_DATA_IN) {
			u32_t bytes;
			/* In the unlikely case we receive data while no
			 * transfer is ongoing, we have to consume the data
			 * anyway. This is to prevent stucking reception on
			 * other endpoints (e.g dw driver has only one rx-fifo,
			 * so drain it).
			 */
			do {
				u8_t data;

				usb_dc_ep_read_wait(ep, &data, 1, &bytes);
			} while (bytes);

			SYS_LOG_ERR("RX data lost, no transfer");
		}
		return;
	}

	if (!k_is_in_isr() || (status == USB_DC_EP_DATA_OUT)) {
		/* If we are not in IRQ context, no need to defer work */
		/* Read (out) needs to be done from ep_callback */
		usb_transfer_work(&trans->work);
	} else {
		k_work_submit(&trans->work);
	}
}

int usb_transfer(u8_t ep, u8_t *data, size_t dlen, unsigned int flags,
		 usb_transfer_callback cb, void *cb_data)
{
	struct usb_transfer_data *trans = NULL;
	int i, key, ret = 0;

	SYS_LOG_DBG("transfer start, ep=%02x, data=%p, dlen=%d\n",
		    ep, data, dlen);

	key = irq_lock();

	for (i = 0; i < MAX_NUM_TRANSFERS; i++) {
		if (!k_sem_take(&usb_dev.transfer[i].sem, K_NO_WAIT)) {
			trans = &usb_dev.transfer[i];
			break;
		}
	}

	if (!trans) {
		SYS_LOG_ERR("no transfer slot available\n");
		ret = -ENOMEM;
		goto done;
	}

	if (trans->status == -EBUSY) {
		/* A transfer is already ongoing and not completed */
		k_sem_give(&trans->sem);
		ret = -EBUSY;
		goto done;
	}

	/* Configure new transfer */
	trans->ep = ep;
	trans->buffer = data;
	trans->bsize = dlen;
	trans->tsize = 0;
	trans->cb = cb;
	trans->flags = flags;
	trans->priv = cb_data;
	trans->status = -EBUSY;

	if (usb_dc_ep_mps(ep) && (dlen % usb_dc_ep_mps(ep))) {
		/* no need to send ZLP since last packet will be a short one */
		trans->flags |= USB_TRANS_NO_ZLP;
	}

	if (flags & USB_TRANS_WRITE) {
		/* start writing first chunk */
		k_work_submit(&trans->work);
	} else {
		/* ready to read, clear NAK */
		ret = usb_dc_ep_read_continue(ep);
	}

done:
	irq_unlock(key);
	return ret;
}

void usb_cancel_transfer(u8_t ep)
{
	struct usb_transfer_data *trans;
	int key;

	key = irq_lock();

	trans = usb_ep_get_transfer(ep);
	if (!trans) {
		goto done;
	}

	if (trans->status != -EBUSY) {
		goto done;
	}

	trans->status = -ECANCELED;
	k_work_submit(&trans->work);

done:
	irq_unlock(key);
}

struct usb_transfer_sync_priv {
	int tsize;
	struct k_sem sem;
};

static void usb_transfer_sync_cb(u8_t ep, int size, void *priv)
{
	struct usb_transfer_sync_priv *pdata = priv;

	pdata->tsize = size;
	k_sem_give(&pdata->sem);
}

int usb_transfer_sync(u8_t ep, u8_t *data, size_t dlen, unsigned int flags)
{
	struct usb_transfer_sync_priv pdata;
	int ret;

	k_sem_init(&pdata.sem, 0, 1);

	ret = usb_transfer(ep, data, dlen, flags, usb_transfer_sync_cb, &pdata);
	if (ret) {
		return ret;
	}

	/* Semaphore will be released by the transfer completion callback */
	k_sem_take(&pdata.sem, K_FOREVER);

	return pdata.tsize;
}

#ifdef CONFIG_USB_COMPOSITE_DEVICE

static u8_t iface_data_buf[CONFIG_USB_COMPOSITE_BUFFER_SIZE];

static void forward_status_cb(enum usb_dc_status_code status, u8_t *param)
{
	size_t size = (__usb_data_end - __usb_data_start);

	for (size_t i = 0; i < size; i++) {
		if (__usb_data_start[i].cb_usb_status) {
			__usb_data_start[i].cb_usb_status(status, param);
		}
	}
}

/*
 * The functions class_handler(), custom_handler() and vendor_handler()
 * go through the interfaces one after the other and compare the
 * bInterfaceNumber with the wIndex and and then call the appropriate
 * callback of the USB function.
 * Note, a USB function can have more than one interface and the
 * request does not have to be directed to the first interface (unlikely).
 * These functions can be simplified and moved to usb_handle_request()
 * when legacy initialization throgh the usb_set_config() and
 * usb_enable() is no longer needed.
 */

static int class_handler(struct usb_setup_packet *pSetup,
			 s32_t *len, u8_t **data)
{
	size_t size = (__usb_data_end - __usb_data_start);
	const struct usb_if_descriptor *if_descr;
	struct usb_interface_cfg_data *iface;

	SYS_LOG_DBG("bRequest 0x%x, wIndex 0x%x",
		    pSetup->bRequest, pSetup->wIndex);

	for (size_t i = 0; i < size; i++) {
		iface = &(__usb_data_start[i].interface);
		if_descr = __usb_data_start[i].interface_descriptor;
		if ((iface->class_handler) &&
		    (if_descr->bInterfaceNumber == pSetup->wIndex)) {
			return iface->class_handler(pSetup, len, data);
		}
	}

	return -ENOTSUP;
}

static int custom_handler(struct usb_setup_packet *pSetup,
			  s32_t *len, u8_t **data)
{
	size_t size = (__usb_data_end - __usb_data_start);
	const struct usb_if_descriptor *if_descr;
	struct usb_interface_cfg_data *iface;

	SYS_LOG_DBG("bRequest 0x%x, wIndex 0x%x",
		    pSetup->bRequest, pSetup->wIndex);

	for (size_t i = 0; i < size; i++) {
		iface = &(__usb_data_start[i].interface);
		if_descr = __usb_data_start[i].interface_descriptor;
		if ((iface->custom_handler) &&
		    (if_descr->bInterfaceNumber == pSetup->wIndex)) {
			return iface->custom_handler(pSetup, len, data);
		}
	}

	return -ENOTSUP;
}

static int vendor_handler(struct usb_setup_packet *pSetup,
			  s32_t *len, u8_t **data)
{
	size_t size = (__usb_data_end - __usb_data_start);
	const struct usb_if_descriptor *if_descr;
	struct usb_interface_cfg_data *iface;

	SYS_LOG_DBG("bRequest 0x%x, wIndex 0x%x",
		    pSetup->bRequest, pSetup->wIndex);

	if (usb_os_desc_enabled()) {
		if (!usb_handle_os_desc_feature(pSetup, len, data)) {
			return 0;
		}
	}

	for (size_t i = 0; i < size; i++) {
		iface = &(__usb_data_start[i].interface);
		if_descr = __usb_data_start[i].interface_descriptor;
		if (iface->vendor_handler) {
			if (!iface->vendor_handler(pSetup, len, data)) {
				return 0;
			}
		}
	}

	return -ENOTSUP;
}

static int composite_setup_ep_cb(void)
{
	size_t size = (__usb_data_end - __usb_data_start);
	struct usb_ep_cfg_data *ep_data;

	for (size_t i = 0; i < size; i++) {
		ep_data = __usb_data_start[i].endpoint;
		for (u8_t n = 0; n < __usb_data_start[i].num_endpoints; n++) {
			SYS_LOG_ERR("set cb, ep: 0x%x\n", ep_data[n].ep_addr);
			if (usb_dc_ep_set_callback(ep_data[n].ep_addr,
						   ep_data[n].ep_cb)) {
				return -1;
			}
		}
	}

	return 0;
}

/*
 * This function configures the USB device stack based on USB descriptor and
 * usb_cfg_data.
 */
static int usb_composite_init(struct device *dev)
{
	int ret;
	struct usb_dc_ep_cfg_data ep0_cfg;
	u8_t *device_descriptor;

	if (true == usb_dev.enabled) {
		return 0;
	}

	/* register device descriptor */
	device_descriptor = usb_get_device_descriptor();
	if (!device_descriptor) {
		SYS_LOG_ERR("Failed to configure USB device stack");
		return -1;
	}

	usb_register_descriptors(device_descriptor);

	/* register standard request handler */
	usb_register_request_handler(REQTYPE_TYPE_STANDARD,
				     &(usb_handle_standard_request),
				     usb_dev.std_req_data);

	/* register class request handlers for each interface*/
	usb_register_request_handler(REQTYPE_TYPE_CLASS,
				     class_handler,
				     iface_data_buf);

	/* register vendor request handlers */
	usb_register_request_handler(REQTYPE_TYPE_VENDOR,
				     vendor_handler,
				     iface_data_buf);

	/* register class request handlers for each interface*/
	usb_register_custom_req_handler(custom_handler);

	usb_register_status_callback(forward_status_cb);
	ret = usb_dc_set_status_callback(forward_status_cb);
	if (ret < 0) {
		return ret;
	}

	/* Enable VBUS if needed */
	ret = usb_vbus_set(true);
	if (ret < 0) {
		return ret;
	}

	ret = usb_dc_attach();
	if (ret < 0) {
		return ret;
	}

	/* Configure control EP */
	ep0_cfg.ep_mps = MAX_PACKET_SIZE0;
	ep0_cfg.ep_type = USB_DC_EP_CONTROL;

	ep0_cfg.ep_addr = USB_CONTROL_OUT_EP0;
	ret = usb_dc_ep_configure(&ep0_cfg);
	if (ret < 0) {
		return ret;
	}

	ep0_cfg.ep_addr = USB_CONTROL_IN_EP0;
	ret = usb_dc_ep_configure(&ep0_cfg);
	if (ret < 0) {
		return ret;
	}

	/*register endpoint 0 handlers*/
	ret = usb_dc_ep_set_callback(USB_CONTROL_OUT_EP0,
				     usb_handle_control_transfer);
	if (ret < 0)
		return ret;

	ret = usb_dc_ep_set_callback(USB_CONTROL_IN_EP0,
				     usb_handle_control_transfer);
	if (ret < 0)
		return ret;

	if (composite_setup_ep_cb()) {
		return -1;
	}

	/* init transfer slots */
	for (int i = 0; i < MAX_NUM_TRANSFERS; i++) {
		k_work_init(&usb_dev.transfer[i].work, usb_transfer_work);
		k_sem_init(&usb_dev.transfer[i].sem, 1, 1);
	}

	/* enable control EP */
	ret = usb_dc_ep_enable(USB_CONTROL_OUT_EP0);
	if (ret < 0) {
		return ret;
	}

	ret = usb_dc_ep_enable(USB_CONTROL_IN_EP0);
	if (ret < 0) {
		return ret;
	}

	usb_dev.enabled = true;

	return 0;
}

SYS_INIT(usb_composite_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
#endif /* CONFIG_USB_COMPOSITE_DEVICE */
