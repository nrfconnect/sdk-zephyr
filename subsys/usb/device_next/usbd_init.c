/*
 * Copyright (c) 2021-2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/sys/slist.h>
#include <zephyr/drivers/usb/udc.h>
#include <zephyr/usb/usbd.h>

#include "usbd_device.h"
#include "usbd_config.h"
#include "usbd_class.h"
#include "usbd_class_api.h"
#include "usbd_endpoint.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(usbd_init, CONFIG_USBD_LOG_LEVEL);

/* TODO: Allow to disable automatic assignment of endpoint features */

/* Assign endpoint address and update wMaxPacketSize */
static int assign_ep_addr(const struct device *dev,
			  struct usb_ep_descriptor *const ed,
			  uint32_t *const config_ep_bm,
			  uint32_t *const class_ep_bm)
{
	int ret = -ENODEV;

	for (unsigned int idx = 1; idx < 16U; idx++) {
		uint16_t mps = ed->wMaxPacketSize;
		uint8_t ep;

		if (USB_EP_DIR_IS_IN(ed->bEndpointAddress)) {
			ep = USB_EP_DIR_IN | idx;
		} else {
			ep = idx;
		}

		if (usbd_ep_bm_is_set(config_ep_bm, ep) ||
		    usbd_ep_bm_is_set(class_ep_bm, ep)) {
			continue;
		}


		ret = udc_ep_try_config(dev, ep,
					ed->bmAttributes, &mps,
					ed->bInterval);

		if (ret == 0) {
			LOG_DBG("ep 0x%02x -> 0x%02x", ed->bEndpointAddress, ep);
			ed->bEndpointAddress = ep;
			ed->wMaxPacketSize = mps;
			usbd_ep_bm_set(class_ep_bm, ed->bEndpointAddress);
			usbd_ep_bm_set(config_ep_bm, ed->bEndpointAddress);

			return 0;
		}
	}

	return ret;
}

/* Unassign all endpoint of a class instance based on class_ep_bm */
static int unassign_eps(struct usbd_contex *const uds_ctx,
			uint32_t *const config_ep_bm,
			uint32_t *const class_ep_bm)
{
	for (unsigned int idx = 1; idx < 16U && *class_ep_bm; idx++) {
		uint8_t ep_in = USB_EP_DIR_IN | idx;
		uint8_t ep_out = idx;

		if (usbd_ep_bm_is_set(class_ep_bm, ep_in)) {
			if (!usbd_ep_bm_is_set(config_ep_bm, ep_in)) {
				LOG_ERR("Endpoing 0x%02x not assigned", ep_in);
				return -EINVAL;
			}

			usbd_ep_bm_clear(config_ep_bm, ep_in);
			usbd_ep_bm_clear(class_ep_bm, ep_in);
		}

		if (usbd_ep_bm_is_set(class_ep_bm, ep_out)) {
			if (!usbd_ep_bm_is_set(config_ep_bm, ep_out)) {
				LOG_ERR("Endpoing 0x%02x not assigned", ep_out);
				return -EINVAL;
			}

			usbd_ep_bm_clear(config_ep_bm, ep_out);
			usbd_ep_bm_clear(class_ep_bm, ep_out);
		}
	}

	return 0;
}

/*
 * Configure all interfaces and endpoints of a class instance
 *
 * The total number of interfaces is stored in the configuration descriptor's
 * value bNumInterfaces. This value is reset at the beginning of configuration
 * initialization and is increased according to the number of interfaces.
 * The respective bInterfaceNumber must be assigned to all interfaces
 * of a class instance.
 *
 * Like bInterfaceNumber the endpoint addresses must be assigned
 * for all registered instances and respective endpoint descriptors.
 * We use config_ep_bm variable as map for assigned endpoint for an
 * USB device configuration.
 */
static int init_configuration_inst(struct usbd_contex *const uds_ctx,
				   struct usbd_class_data *const data,
				   uint32_t *const config_ep_bm,
				   uint8_t *const nif)
{
	struct usb_desc_header *dh = data->desc;
	uint8_t *ptr = (uint8_t *)dh;
	struct usb_if_descriptor *ifd = NULL;
	struct usb_ep_descriptor *ed;
	uint32_t class_ep_bm = 0;
	uint8_t tmp_nif;
	int ret;

	tmp_nif = *nif;
	data->iface_bm = 0U;
	data->ep_active = 0U;

	while (dh->bLength != 0) {

		if (dh->bDescriptorType == USB_DESC_INTERFACE) {
			ifd = (struct usb_if_descriptor *)ptr;

			data->ep_active |= class_ep_bm;

			if (ifd->bAlternateSetting == 0) {
				ifd->bInterfaceNumber = tmp_nif;
				data->iface_bm |= BIT(tmp_nif);
				tmp_nif++;
			} else {
				ifd->bInterfaceNumber = tmp_nif - 1;
				/*
				 * Unassign endpoints from last alternate,
				 * to work properly it requires that the
				 * characteristics of endpoints in alternate
				 * interfaces are ascending.
				 */
				unassign_eps(uds_ctx, config_ep_bm, &class_ep_bm);
			}

			class_ep_bm = 0;
			LOG_INF("interface %u alternate %u",
				ifd->bInterfaceNumber, ifd->bAlternateSetting);
		}

		if (dh->bDescriptorType == USB_DESC_ENDPOINT) {
			ed = (struct usb_ep_descriptor *)ptr;
			ret = assign_ep_addr(uds_ctx->dev, ed,
					     config_ep_bm, &class_ep_bm);
			if (ret) {
				return ret;
			}

			LOG_INF("\tep 0x%02x interface ep-bm 0x%08x",
				ed->bEndpointAddress, class_ep_bm);
		}

		ptr += dh->bLength;
		dh = (struct usb_desc_header *)ptr;
	}

	if (tmp_nif <= *nif) {
		return -EINVAL;
	}

	*nif = tmp_nif;
	data->ep_active |= class_ep_bm;

	LOG_INF("Instance iface-bm 0x%08x ep-bm 0x%08x",
		data->iface_bm, data->ep_active);

	return 0;
}

/*
 * Initialize a device configuration
 *
 * Iterate on a list of all classes in a configuration
 */
static int init_configuration(struct usbd_contex *const uds_ctx,
			      struct usbd_config_node *const cfg_nd)
{
	struct usb_cfg_descriptor *cfg_desc = cfg_nd->desc;
	struct usbd_class_node *c_nd;
	uint32_t config_ep_bm = 0;
	size_t cfg_len = 0;
	uint8_t nif = 0;
	int ret;

	SYS_SLIST_FOR_EACH_CONTAINER(&cfg_nd->class_list, c_nd, node) {

		ret = init_configuration_inst(uds_ctx, c_nd->data,
					      &config_ep_bm, &nif);
		if (ret != 0) {
			LOG_ERR("Failed to assign endpoint addresses");
			return ret;
		}

		ret = usbd_class_init(c_nd);
		if (ret != 0) {
			LOG_ERR("Failed to initialize class instance");
			return ret;
		}

		LOG_INF("Init class node %p, descriptor length %u",
			c_nd, usbd_class_desc_len(c_nd));
		cfg_len += usbd_class_desc_len(c_nd);
	}

	/* Update wTotalLength and bNumInterfaces of configuration descriptor */
	sys_put_le16(sizeof(struct usb_cfg_descriptor) + cfg_len,
		     (uint8_t *)&cfg_desc->wTotalLength);
	cfg_desc->bNumInterfaces = nif;

	LOG_INF("bNumInterfaces %u wTotalLength %u",
		cfg_desc->bNumInterfaces,
		cfg_desc->wTotalLength);

	/* Finally reset configuration's endpoint assignment */
	SYS_SLIST_FOR_EACH_CONTAINER(&cfg_nd->class_list, c_nd, node) {
		c_nd->data->ep_assigned = c_nd->data->ep_active;
		ret = unassign_eps(uds_ctx, &config_ep_bm, &c_nd->data->ep_active);
		if (ret != 0) {
			return ret;
		}
	}

	return 0;
}

int usbd_init_configurations(struct usbd_contex *const uds_ctx)
{
	struct usbd_config_node *cfg_nd;

	SYS_SLIST_FOR_EACH_CONTAINER(&uds_ctx->configs, cfg_nd, node) {
		int ret;

		ret = init_configuration(uds_ctx, cfg_nd);
		if (ret) {
			LOG_ERR("Failed to init configuration %u",
				usbd_config_get_value(cfg_nd));
			return ret;
		}

		LOG_INF("bNumConfigurations %u",
			usbd_get_num_configs(uds_ctx));
	}

	return 0;
}
