/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file  udc_nrf.c
 * @brief Nordic USB device controller (UDC) driver
 *
 * The driver implements the interface between the nRF USBD peripheral
 * driver from nrfx package and UDC API.
 */

#include <string.h>
#include <stdio.h>
#include <soc.h>

#include <zephyr/kernel.h>
#include <zephyr/drivers/usb/udc.h>
#include <zephyr/drivers/clock_control.h>
#include <zephyr/drivers/clock_control/nrf_clock_control.h>
#include <zephyr/dt-bindings/regulator/nrf5x.h>

#include <nrf_usbd_common.h>
#include <hal/nrf_usbd.h>
#include <nrfx_power.h>
#include "udc_common.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(udc_nrf, CONFIG_UDC_DRIVER_LOG_LEVEL);

/*
 * There is no real advantage to change control endpoint size
 * but we can use it for testing UDC driver API and higher layers.
 */
#define UDC_NRF_MPS0		UDC_MPS0_64
#define UDC_NRF_EP0_SIZE	64

enum udc_nrf_event_type {
	/* An event generated by the HAL driver */
	UDC_NRF_EVT_HAL,
	/* Shim driver event to trigger next transfer */
	UDC_NRF_EVT_XFER,
	/* Let controller perform status stage */
	UDC_NRF_EVT_STATUS_IN,
};

struct udc_nrf_evt {
	enum udc_nrf_event_type type;
	union {
		nrf_usbd_common_evt_t hal_evt;
		uint8_t ep;
	};
};

K_MSGQ_DEFINE(drv_msgq, sizeof(struct udc_nrf_evt),
	      CONFIG_UDC_NRF_MAX_QMESSAGES, sizeof(uint32_t));

static K_KERNEL_STACK_DEFINE(drv_stack, CONFIG_UDC_NRF_THREAD_STACK_SIZE);
static struct k_thread drv_stack_data;

/* USB device controller access from devicetree */
#define DT_DRV_COMPAT nordic_nrf_usbd

#define CFG_EPIN_CNT		DT_INST_PROP(0, num_in_endpoints)
#define CFG_EPOUT_CNT		DT_INST_PROP(0, num_out_endpoints)
#define CFG_EP_ISOIN_CNT	DT_INST_PROP(0, num_isoin_endpoints)
#define CFG_EP_ISOOUT_CNT	DT_INST_PROP(0, num_isoout_endpoints)

static struct udc_ep_config ep_cfg_out[CFG_EPOUT_CNT + CFG_EP_ISOOUT_CNT + 1];
static struct udc_ep_config ep_cfg_in[CFG_EPIN_CNT + CFG_EP_ISOIN_CNT + 1];
static bool udc_nrf_setup_rcvd, udc_nrf_setup_set_addr, udc_nrf_fake_setup;
static uint8_t udc_nrf_address;
const static struct device *udc_nrf_dev;

struct udc_nrf_config {
	clock_control_subsys_t clock;
	nrfx_power_config_t pwr;
	nrfx_power_usbevt_config_t evt;
};

static struct onoff_manager *hfxo_mgr;
static struct onoff_client hfxo_cli;

static void udc_nrf_clear_control_out(const struct device *dev)
{
	if (nrf_usbd_common_last_setup_dir_get() == USB_CONTROL_EP_OUT &&
	    udc_nrf_setup_rcvd) {
		/* Allow data chunk on EP0 OUT */
		nrf_usbd_common_setup_data_clear();
		udc_nrf_setup_rcvd = false;
		LOG_INF("Allow data OUT");
	}
}

static void udc_event_xfer_in_next(const struct device *dev, const uint8_t ep)
{
	struct net_buf *buf;

	if (udc_ep_is_busy(dev, ep)) {
		return;
	}

	buf = udc_buf_peek(dev, ep);
	if (buf != NULL) {
		nrf_usbd_common_transfer_t xfer = {
			.p_data = {.tx = buf->data},
			.size = buf->len,
			.flags = udc_ep_buf_has_zlp(buf) ?
				 NRF_USBD_COMMON_TRANSFER_ZLP_FLAG : 0,
		};
		nrfx_err_t err;

		err = nrf_usbd_common_ep_transfer(ep, &xfer);
		if (err != NRFX_SUCCESS) {
			LOG_ERR("ep 0x%02x nrfx error: %x", ep, err);
			/* REVISE: remove from endpoint queue? ASSERT? */
		} else {
			udc_ep_set_busy(dev, ep, true);
		}
	}
}

static void udc_event_xfer_ctrl_in(const struct device *dev,
				   struct net_buf *const buf)
{
	if (udc_ctrl_stage_is_status_in(dev) ||
	    udc_ctrl_stage_is_no_data(dev)) {
		/* Status stage finished, notify upper layer */
		udc_ctrl_submit_status(dev, buf);
	}

	if (udc_ctrl_stage_is_data_in(dev)) {
		/*
		 * s-in-[status] finished, release buffer.
		 * Since the controller supports auto-status we cannot use
		 * if (udc_ctrl_stage_is_status_out()) after state update.
		 */
		net_buf_unref(buf);
	}

	/* Update to next stage of control transfer */
	udc_ctrl_update_stage(dev, buf);

	if (!udc_nrf_setup_set_addr) {
		nrf_usbd_common_setup_clear();
	}
}

static void udc_event_fake_status_in(const struct device *dev)
{
	struct net_buf *buf;

	buf = udc_buf_get(dev, USB_CONTROL_EP_IN);
	if (unlikely(buf == NULL)) {
		LOG_DBG("ep 0x%02x queue is empty", USB_CONTROL_EP_IN);
		return;
	}

	LOG_DBG("Fake status IN %p", buf);
	udc_event_xfer_ctrl_in(dev, buf);
}

static void udc_event_xfer_in(const struct device *dev,
			      nrf_usbd_common_evt_t const *const event)
{
	uint8_t ep = event->data.eptransfer.ep;
	struct net_buf *buf;

	switch (event->data.eptransfer.status) {
	case NRF_USBD_COMMON_EP_OK:
		buf = udc_buf_get(dev, ep);
		if (buf == NULL) {
			LOG_ERR("ep 0x%02x queue is empty", ep);
			__ASSERT_NO_MSG(false);
			return;
		}

		udc_ep_set_busy(dev, ep, false);
		if (ep == USB_CONTROL_EP_IN) {
			udc_event_xfer_ctrl_in(dev, buf);
			return;
		}

		udc_submit_ep_event(dev, buf, 0);
		break;

	case NRF_USBD_COMMON_EP_ABORTED:
		LOG_WRN("aborted IN ep 0x%02x", ep);
		buf = udc_buf_get_all(dev, ep);

		if (buf == NULL) {
			LOG_DBG("ep 0x%02x queue is empty", ep);
			return;
		}

		udc_ep_set_busy(dev, ep, false);
		udc_submit_ep_event(dev, buf, -ECONNABORTED);
		break;

	default:
		LOG_ERR("Unexpected event (nrfx_usbd): %d, ep 0x%02x",
			event->data.eptransfer.status, ep);
		udc_submit_event(dev, UDC_EVT_ERROR, -EIO);
		break;
	}
}

static void udc_event_xfer_ctrl_out(const struct device *dev,
				    struct net_buf *const buf)
{
	/*
	 * In case s-in-status, controller supports auto-status therefore we
	 * do not have to call udc_ctrl_stage_is_status_out().
	 */

	/* Update to next stage of control transfer */
	udc_ctrl_update_stage(dev, buf);

	if (udc_ctrl_stage_is_status_in(dev)) {
		udc_ctrl_submit_s_out_status(dev, buf);
	}
}

static void udc_event_xfer_out_next(const struct device *dev, const uint8_t ep)
{
	struct net_buf *buf;

	if (udc_ep_is_busy(dev, ep)) {
		return;
	}

	buf = udc_buf_peek(dev, ep);
	if (buf != NULL) {
		nrf_usbd_common_transfer_t xfer = {
			.p_data = {.rx = buf->data},
			.size = buf->size,
			.flags = 0,
		};
		nrfx_err_t err;

		err = nrf_usbd_common_ep_transfer(ep, &xfer);
		if (err != NRFX_SUCCESS) {
			LOG_ERR("ep 0x%02x nrfx error: %x", ep, err);
			/* REVISE: remove from endpoint queue? ASSERT? */
		} else {
			udc_ep_set_busy(dev, ep, true);
		}
	} else {
		LOG_DBG("ep 0x%02x waiting, queue is empty", ep);
	}
}

static void udc_event_xfer_out(const struct device *dev,
			       nrf_usbd_common_evt_t const *const event)
{
	uint8_t ep = event->data.eptransfer.ep;
	nrf_usbd_common_ep_status_t err_code;
	struct net_buf *buf;
	size_t len;

	switch (event->data.eptransfer.status) {
	case NRF_USBD_COMMON_EP_WAITING:
		/*
		 * There is nothing to do here, new transfer
		 * will be tried in both cases later.
		 */
		break;

	case NRF_USBD_COMMON_EP_OK:
		err_code = nrf_usbd_common_ep_status_get(ep, &len);
		if (err_code != NRF_USBD_COMMON_EP_OK) {
			LOG_ERR("OUT transfer failed %d", err_code);
		}

		buf = udc_buf_get(dev, ep);
		if (buf == NULL) {
			LOG_ERR("ep 0x%02x ok, queue is empty", ep);
			return;
		}

		net_buf_add(buf, len);
		udc_ep_set_busy(dev, ep, false);
		if (ep == USB_CONTROL_EP_OUT) {
			udc_event_xfer_ctrl_out(dev, buf);
		} else {
			udc_submit_ep_event(dev, buf, 0);
		}

		break;

	default:
		LOG_ERR("Unexpected event (nrfx_usbd): %d, ep 0x%02x",
			event->data.eptransfer.status, ep);
		udc_submit_event(dev, UDC_EVT_ERROR, -EIO);
		break;
	}
}

static int usbd_ctrl_feed_dout(const struct device *dev,
			       const size_t length)
{

	struct udc_ep_config *cfg = udc_get_ep_cfg(dev, USB_CONTROL_EP_OUT);
	struct net_buf *buf;

	buf = udc_ctrl_alloc(dev, USB_CONTROL_EP_OUT, length);
	if (buf == NULL) {
		return -ENOMEM;
	}

	k_fifo_put(&cfg->fifo, buf);
	udc_nrf_clear_control_out(dev);

	return 0;
}

static int udc_event_xfer_setup(const struct device *dev)
{
	nrf_usbd_common_setup_t *setup;
	struct net_buf *buf;
	int err;

	buf = udc_ctrl_alloc(dev, USB_CONTROL_EP_OUT,
			     sizeof(struct usb_setup_packet));
	if (buf == NULL) {
		LOG_ERR("Failed to allocate for setup");
		return -ENOMEM;
	}

	udc_ep_buf_set_setup(buf);
	setup = (nrf_usbd_common_setup_t *)buf->data;
	nrf_usbd_common_setup_get(setup);

	/* USBD peripheral automatically handles Set Address in slightly
	 * different manner than the USB stack.
	 *
	 * USBD peripheral doesn't care about wLength, but the peripheral
	 * switches to new address only after status stage. The device won't
	 * automatically accept Data Stage packets.
	 *
	 * However, in the case the host:
	 *   * sends SETUP Set Address with non-zero wLength
	 *   * does not send corresponding OUT DATA packets (to match wLength)
	 *     or sends the packets but disregards NAK
	 *     or sends the packets that device ACKs
	 *   * sends IN token (either incorrectly proceeds to status stage, or
	 *     manages to send IN before SW sets STALL)
	 * then the USBD peripheral will accept the address and USB stack won't.
	 * This will lead to state mismatch between the stack and peripheral.
	 *
	 * In cases where the USB stack would like to STALL the request there is
	 * a race condition between host issuing Set Address status stage (IN
	 * token) and SW setting STALL bit. If host wins the race, the device
	 * ACKs status stage and use new address. If device wins the race, the
	 * device STALLs status stage and address remains unchanged.
	 */
	udc_nrf_setup_set_addr =
		setup->bmRequestType == 0 &&
		setup->bRequest == USB_SREQ_SET_ADDRESS;
	if (udc_nrf_setup_set_addr) {
		if (setup->wLength) {
			/* Currently USB stack only STALLs OUT Data Stage when
			 * buffer allocation fails. To prevent the device from
			 * ACKing the Data Stage, simply ignore the request
			 * completely.
			 *
			 * If host incorrectly proceeds to status stage there
			 * will be address mismatch (unless the new address is
			 * equal to current device address). If host does not
			 * issue IN token then the mismatch will be avoided.
			 */
			net_buf_unref(buf);
			return 0;
		}

		/* nRF52/nRF53 USBD doesn't care about wValue bits 8..15 and
		 * wIndex value but USB device stack does.
		 *
		 * Just clear the bits so stack will handle the request in the
		 * same way as USBD peripheral does, avoiding the mismatch.
		 */
		setup->wValue &= 0x7F;
		setup->wIndex = 0;
	}

	if (!udc_nrf_setup_set_addr && udc_nrf_address != NRF_USBD->USBADDR) {
		/* Address mismatch detected. Fake Set Address handling to
		 * correct the situation, then repeat handling.
		 */
		udc_nrf_fake_setup = true;
		udc_nrf_setup_set_addr = true;

		setup->bmRequestType = 0;
		setup->bRequest = USB_SREQ_SET_ADDRESS;
		setup->wValue = NRF_USBD->USBADDR;
		setup->wIndex = 0;
		setup->wLength = 0;
	} else {
		udc_nrf_fake_setup = false;
	}

	net_buf_add(buf, sizeof(nrf_usbd_common_setup_t));
	udc_nrf_setup_rcvd = true;

	/* Update to next stage of control transfer */
	udc_ctrl_update_stage(dev, buf);

	if (udc_ctrl_stage_is_data_out(dev)) {
		/*  Allocate and feed buffer for data OUT stage */
		LOG_DBG("s:%p|feed for -out-", buf);
		err = usbd_ctrl_feed_dout(dev, udc_data_stage_length(buf));
		if (err == -ENOMEM) {
			err = udc_submit_ep_event(dev, buf, err);
		}
	} else if (udc_ctrl_stage_is_data_in(dev)) {
		err = udc_ctrl_submit_s_in_status(dev);
	} else {
		err = udc_ctrl_submit_s_status(dev);
	}

	return err;
}

static void udc_nrf_thread(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	const struct device *dev = p1;

	while (true) {
		bool start_xfer = false;
		struct udc_nrf_evt evt;
		uint8_t ep;

		k_msgq_get(&drv_msgq, &evt, K_FOREVER);

		switch (evt.type) {
		case UDC_NRF_EVT_HAL:
			ep = evt.hal_evt.data.eptransfer.ep;
			switch (evt.hal_evt.type) {
			case NRF_USBD_COMMON_EVT_SUSPEND:
				LOG_INF("SUSPEND state detected");
				nrf_usbd_common_suspend();
				udc_set_suspended(udc_nrf_dev, true);
				udc_submit_event(udc_nrf_dev, UDC_EVT_SUSPEND, 0);
				break;
			case NRF_USBD_COMMON_EVT_RESUME:
				LOG_INF("RESUMING from suspend");
				udc_set_suspended(udc_nrf_dev, false);
				udc_submit_event(udc_nrf_dev, UDC_EVT_RESUME, 0);
				break;
			case NRF_USBD_COMMON_EVT_WUREQ:
				LOG_INF("Remote wakeup initiated");
				udc_set_suspended(udc_nrf_dev, false);
				udc_submit_event(udc_nrf_dev, UDC_EVT_RESUME, 0);
				break;
			case NRF_USBD_COMMON_EVT_EPTRANSFER:
				start_xfer = true;
				if (USB_EP_DIR_IS_IN(ep)) {
					udc_event_xfer_in(dev, &evt.hal_evt);
				} else {
					udc_event_xfer_out(dev, &evt.hal_evt);
				}
				break;
			case NRF_USBD_COMMON_EVT_SETUP:
				udc_event_xfer_setup(dev);
				break;
			default:
				break;
			}
			break;
		case UDC_NRF_EVT_XFER:
			start_xfer = true;
			ep = evt.ep;
			break;
		case UDC_NRF_EVT_STATUS_IN:
			udc_event_fake_status_in(dev);
			break;
		}

		if (start_xfer) {
			if (USB_EP_DIR_IS_IN(ep)) {
				udc_event_xfer_in_next(dev, ep);
			} else {
				udc_event_xfer_out_next(dev, ep);
			}
		}
	}
}

static void udc_sof_check_iso_out(const struct device *dev)
{
	const uint8_t iso_out_addr = 0x08;
	struct udc_nrf_evt evt = {
		.type = UDC_NRF_EVT_XFER,
		.ep = iso_out_addr,
	};
	struct udc_ep_config *ep_cfg;

	ep_cfg = udc_get_ep_cfg(dev, iso_out_addr);
	if (ep_cfg == NULL) {
		return;
	}

	if (ep_cfg->stat.enabled && !k_fifo_is_empty(&ep_cfg->fifo)) {
		k_msgq_put(&drv_msgq, &evt, K_NO_WAIT);
	}
}

static void usbd_event_handler(nrf_usbd_common_evt_t const *const hal_evt)
{
	switch (hal_evt->type) {
	case NRF_USBD_COMMON_EVT_RESET:
		LOG_INF("Reset");
		udc_submit_event(udc_nrf_dev, UDC_EVT_RESET, 0);
		break;
	case NRF_USBD_COMMON_EVT_SOF:
		udc_submit_event(udc_nrf_dev, UDC_EVT_SOF, 0);
		udc_sof_check_iso_out(udc_nrf_dev);
		break;
	case NRF_USBD_COMMON_EVT_SUSPEND:
	case NRF_USBD_COMMON_EVT_RESUME:
	case NRF_USBD_COMMON_EVT_WUREQ:
	case NRF_USBD_COMMON_EVT_EPTRANSFER:
	case NRF_USBD_COMMON_EVT_SETUP: {
		struct udc_nrf_evt evt = {
			.type = UDC_NRF_EVT_HAL,
			.hal_evt = *hal_evt,
		};

		/* Forward these two to the thread since mutually exclusive
		 * access to the controller is necessary.
		 */
		k_msgq_put(&drv_msgq, &evt, K_NO_WAIT);
		break;
	}
	default:
		break;
	}
}

static void udc_nrf_power_handler(nrfx_power_usb_evt_t pwr_evt)
{
	switch (pwr_evt) {
	case NRFX_POWER_USB_EVT_DETECTED:
		LOG_DBG("POWER event detected");
		udc_submit_event(udc_nrf_dev, UDC_EVT_VBUS_READY, 0);
		break;
	case NRFX_POWER_USB_EVT_READY:
		LOG_DBG("POWER event ready");
		nrf_usbd_common_start(true);
		break;
	case NRFX_POWER_USB_EVT_REMOVED:
		LOG_DBG("POWER event removed");
		udc_submit_event(udc_nrf_dev, UDC_EVT_VBUS_REMOVED, 0);
		break;
	default:
		LOG_ERR("Unknown power event %d", pwr_evt);
	}
}

static bool udc_nrf_fake_status_in(const struct device *dev)
{
	struct udc_nrf_evt evt = {
		.type = UDC_NRF_EVT_STATUS_IN,
		.ep = USB_CONTROL_EP_IN,
	};

	if (nrf_usbd_common_last_setup_dir_get() == USB_CONTROL_EP_OUT ||
	    udc_nrf_fake_setup) {
		/* Let controller perform status IN stage */
		k_msgq_put(&drv_msgq, &evt, K_NO_WAIT);
		return true;
	}

	return false;
}

static int udc_nrf_ep_enqueue(const struct device *dev,
			      struct udc_ep_config *cfg,
			      struct net_buf *buf)
{
	struct udc_nrf_evt evt = {
		.type = UDC_NRF_EVT_XFER,
		.ep = cfg->addr,
	};

	udc_buf_put(cfg, buf);

	if (cfg->addr == USB_CONTROL_EP_IN && buf->len == 0) {
		if (udc_nrf_fake_status_in(dev)) {
			return 0;
		}
	}

	k_msgq_put(&drv_msgq, &evt, K_NO_WAIT);

	return 0;
}

static int udc_nrf_ep_dequeue(const struct device *dev,
			      struct udc_ep_config *cfg)
{
	bool busy = nrf_usbd_common_ep_is_busy(cfg->addr);

	nrf_usbd_common_ep_abort(cfg->addr);
	if (USB_EP_DIR_IS_OUT(cfg->addr) || !busy) {
		struct net_buf *buf;

		/*
		 * HAL driver does not generate event for an OUT endpoint
		 * or when IN endpoint is not busy.
		 */
		buf = udc_buf_get_all(dev, cfg->addr);
		if (buf) {
			udc_submit_ep_event(dev, buf, -ECONNABORTED);
		} else {
			LOG_INF("ep 0x%02x queue is empty", cfg->addr);
		}

	}

	udc_ep_set_busy(dev, cfg->addr, false);

	return 0;
}

static int udc_nrf_ep_enable(const struct device *dev,
			     struct udc_ep_config *cfg)
{
	uint16_t mps;

	__ASSERT_NO_MSG(cfg);
	mps = (udc_mps_ep_size(cfg) == 0) ? cfg->caps.mps : udc_mps_ep_size(cfg);
	nrf_usbd_common_ep_max_packet_size_set(cfg->addr, mps);
	nrf_usbd_common_ep_enable(cfg->addr);
	if (!NRF_USBD_EPISO_CHECK(cfg->addr)) {
		/* ISO transactions for full-speed device do not support
		 * toggle sequencing and should only send DATA0 PID.
		 */
		nrf_usbd_common_ep_dtoggle_clear(cfg->addr);
		nrf_usbd_common_ep_stall_clear(cfg->addr);
	}

	LOG_DBG("Enable ep 0x%02x", cfg->addr);

	return 0;
}

static int udc_nrf_ep_disable(const struct device *dev,
			      struct udc_ep_config *cfg)
{
	__ASSERT_NO_MSG(cfg);
	nrf_usbd_common_ep_disable(cfg->addr);
	LOG_DBG("Disable ep 0x%02x", cfg->addr);

	return 0;
}

static int udc_nrf_ep_set_halt(const struct device *dev,
				struct udc_ep_config *cfg)
{
	LOG_DBG("Halt ep 0x%02x", cfg->addr);

	if (cfg->addr == USB_CONTROL_EP_OUT ||
	    cfg->addr == USB_CONTROL_EP_IN) {
		nrf_usbd_common_setup_stall();
	} else {
		nrf_usbd_common_ep_stall(cfg->addr);
	}

	return 0;
}

static int udc_nrf_ep_clear_halt(const struct device *dev,
				struct udc_ep_config *cfg)
{
	LOG_DBG("Clear halt ep 0x%02x", cfg->addr);

	nrf_usbd_common_ep_dtoggle_clear(cfg->addr);
	nrf_usbd_common_ep_stall_clear(cfg->addr);

	return 0;
}

static int udc_nrf_set_address(const struct device *dev, const uint8_t addr)
{
	/*
	 * If the status stage already finished (which depends entirely on when
	 * the host sends IN token) then NRF_USBD->USBADDR will have the same
	 * address, otherwise it won't (unless new address is unchanged).
	 *
	 * Store the address so the driver can detect address mismatches
	 * between USB stack and USBD peripheral. The mismatches can occur if:
	 *   * SW has high enough latency in SETUP handling, or
	 *   * Host did not issue Status Stage after Set Address request
	 *
	 * The SETUP handling latency is a problem because the Set Address is
	 * automatically handled by device. Because whole Set Address handling
	 * can finish in less than 21 us, the latency required (with perfect
	 * timing) to hit the issue is relatively short (2 ms Set Address
	 * recovery interval + negligible Set Address handling time). If host
	 * sends new SETUP before SW had a chance to read the Set Address one,
	 * the Set Address one will be overwritten without a trace.
	 */
	udc_nrf_address = addr;

	if (udc_nrf_fake_setup) {
		struct udc_nrf_evt evt = {
			.type = UDC_NRF_EVT_HAL,
			.hal_evt = {
				.type = NRF_USBD_COMMON_EVT_SETUP,
			},
		};

		/* Finished handling lost Set Address, now handle the pending
		 * SETUP transfer.
		 */
		k_msgq_put(&drv_msgq, &evt, K_NO_WAIT);
	}

	return 0;
}

static int udc_nrf_host_wakeup(const struct device *dev)
{
	bool res = nrf_usbd_common_wakeup_req();

	LOG_DBG("Host wakeup request");
	if (!res) {
		return -EAGAIN;
	}

	return 0;
}

static int udc_nrf_enable(const struct device *dev)
{
	unsigned int key;
	int ret;

	ret = nrf_usbd_common_init(usbd_event_handler);
	if (ret != NRFX_SUCCESS) {
		LOG_ERR("nRF USBD driver initialization failed");
		return -EIO;
	}

	if (udc_ep_enable_internal(dev, USB_CONTROL_EP_OUT,
				   USB_EP_TYPE_CONTROL, UDC_NRF_EP0_SIZE, 0)) {
		LOG_ERR("Failed to enable control endpoint");
		return -EIO;
	}

	if (udc_ep_enable_internal(dev, USB_CONTROL_EP_IN,
				   USB_EP_TYPE_CONTROL, UDC_NRF_EP0_SIZE, 0)) {
		LOG_ERR("Failed to enable control endpoint");
		return -EIO;
	}

	sys_notify_init_spinwait(&hfxo_cli.notify);
	ret = onoff_request(hfxo_mgr, &hfxo_cli);
	if (ret < 0) {
		LOG_ERR("Failed to start HFXO %d", ret);
		return ret;
	}

	/* Disable interrupts until USBD is enabled */
	key = irq_lock();
	nrf_usbd_common_enable();
	irq_unlock(key);

	return 0;
}

static int udc_nrf_disable(const struct device *dev)
{
	int ret;

	nrf_usbd_common_disable();

	if (udc_ep_disable_internal(dev, USB_CONTROL_EP_OUT)) {
		LOG_ERR("Failed to disable control endpoint");
		return -EIO;
	}

	if (udc_ep_disable_internal(dev, USB_CONTROL_EP_IN)) {
		LOG_ERR("Failed to disable control endpoint");
		return -EIO;
	}

	nrf_usbd_common_uninit();

	ret = onoff_cancel_or_release(hfxo_mgr, &hfxo_cli);
	if (ret < 0) {
		LOG_ERR("Failed to stop HFXO %d", ret);
		return ret;
	}

	return 0;
}

static int udc_nrf_init(const struct device *dev)
{
	const struct udc_nrf_config *cfg = dev->config;

	hfxo_mgr = z_nrf_clock_control_get_onoff(cfg->clock);

#ifdef CONFIG_HAS_HW_NRF_USBREG
	/* Use CLOCK/POWER priority for compatibility with other series where
	 * USB events are handled by CLOCK interrupt handler.
	 */
	IRQ_CONNECT(USBREGULATOR_IRQn,
		    DT_IRQ(DT_INST(0, nordic_nrf_clock), priority),
		    nrfx_isr, nrfx_usbreg_irq_handler, 0);
#endif

	IRQ_CONNECT(DT_INST_IRQN(0), DT_INST_IRQ(0, priority),
		    nrfx_isr, nrf_usbd_common_irq_handler, 0);

	(void)nrfx_power_init(&cfg->pwr);
	nrfx_power_usbevt_init(&cfg->evt);

	nrfx_power_usbevt_enable();
	LOG_INF("Initialized");

	return 0;
}

static int udc_nrf_shutdown(const struct device *dev)
{
	LOG_INF("shutdown");

	nrfx_power_usbevt_disable();
	nrfx_power_usbevt_uninit();

	return 0;
}

static int udc_nrf_driver_init(const struct device *dev)
{
	struct udc_data *data = dev->data;
	int err;

	LOG_INF("Preinit");
	udc_nrf_dev = dev;
	k_mutex_init(&data->mutex);
	k_thread_create(&drv_stack_data, drv_stack,
			K_KERNEL_STACK_SIZEOF(drv_stack),
			udc_nrf_thread,
			(void *)dev, NULL, NULL,
			K_PRIO_COOP(8), 0, K_NO_WAIT);

	k_thread_name_set(&drv_stack_data, "udc_nrfx");

	for (int i = 0; i < ARRAY_SIZE(ep_cfg_out); i++) {
		ep_cfg_out[i].caps.out = 1;
		if (i == 0) {
			ep_cfg_out[i].caps.control = 1;
			ep_cfg_out[i].caps.mps = NRF_USBD_COMMON_EPSIZE;
		} else if (i < (CFG_EPOUT_CNT + 1)) {
			ep_cfg_out[i].caps.bulk = 1;
			ep_cfg_out[i].caps.interrupt = 1;
			ep_cfg_out[i].caps.mps = NRF_USBD_COMMON_EPSIZE;
		} else {
			ep_cfg_out[i].caps.iso = 1;
			ep_cfg_out[i].caps.mps = NRF_USBD_COMMON_ISOSIZE / 2;
		}

		ep_cfg_out[i].addr = USB_EP_DIR_OUT | i;
		err = udc_register_ep(dev, &ep_cfg_out[i]);
		if (err != 0) {
			LOG_ERR("Failed to register endpoint");
			return err;
		}
	}

	for (int i = 0; i < ARRAY_SIZE(ep_cfg_in); i++) {
		ep_cfg_in[i].caps.in = 1;
		if (i == 0) {
			ep_cfg_in[i].caps.control = 1;
			ep_cfg_in[i].caps.mps = NRF_USBD_COMMON_EPSIZE;
		} else if (i < (CFG_EPIN_CNT + 1)) {
			ep_cfg_in[i].caps.bulk = 1;
			ep_cfg_in[i].caps.interrupt = 1;
			ep_cfg_in[i].caps.mps = NRF_USBD_COMMON_EPSIZE;
		} else {
			ep_cfg_in[i].caps.iso = 1;
			ep_cfg_in[i].caps.mps = NRF_USBD_COMMON_ISOSIZE / 2;
		}

		ep_cfg_in[i].addr = USB_EP_DIR_IN | i;
		err = udc_register_ep(dev, &ep_cfg_in[i]);
		if (err != 0) {
			LOG_ERR("Failed to register endpoint");
			return err;
		}
	}

	data->caps.rwup = true;
	data->caps.out_ack = true;
	data->caps.mps0 = UDC_NRF_MPS0;
	data->caps.can_detect_vbus = true;

	return 0;
}

static void udc_nrf_lock(const struct device *dev)
{
	udc_lock_internal(dev, K_FOREVER);
}

static void udc_nrf_unlock(const struct device *dev)
{
	udc_unlock_internal(dev);
}

static const struct udc_nrf_config udc_nrf_cfg = {
	.clock = COND_CODE_1(NRF_CLOCK_HAS_HFCLK192M,
			     (CLOCK_CONTROL_NRF_SUBSYS_HF192M),
			     (CLOCK_CONTROL_NRF_SUBSYS_HF)),
	.pwr = {
		.dcdcen = (DT_PROP(DT_INST(0, nordic_nrf5x_regulator), regulator_initial_mode)
			   == NRF5X_REG_MODE_DCDC),
#if NRFX_POWER_SUPPORTS_DCDCEN_VDDH
		.dcdcenhv = COND_CODE_1(CONFIG_SOC_SERIES_NRF52X,
			(DT_NODE_HAS_STATUS_OKAY(DT_INST(0, nordic_nrf52x_regulator_hv))),
			(DT_NODE_HAS_STATUS_OKAY(DT_INST(0, nordic_nrf53x_regulator_hv)))),
#endif
	},

	.evt = {
		.handler = udc_nrf_power_handler
	},
};

static struct udc_data udc_nrf_data = {
	.mutex = Z_MUTEX_INITIALIZER(udc_nrf_data.mutex),
	.priv = NULL,
};

static const struct udc_api udc_nrf_api = {
	.lock = udc_nrf_lock,
	.unlock = udc_nrf_unlock,
	.init = udc_nrf_init,
	.enable = udc_nrf_enable,
	.disable = udc_nrf_disable,
	.shutdown = udc_nrf_shutdown,
	.set_address = udc_nrf_set_address,
	.host_wakeup = udc_nrf_host_wakeup,
	.ep_try_config = NULL,
	.ep_enable = udc_nrf_ep_enable,
	.ep_disable = udc_nrf_ep_disable,
	.ep_set_halt = udc_nrf_ep_set_halt,
	.ep_clear_halt = udc_nrf_ep_clear_halt,
	.ep_enqueue = udc_nrf_ep_enqueue,
	.ep_dequeue = udc_nrf_ep_dequeue,
};

DEVICE_DT_INST_DEFINE(0, udc_nrf_driver_init, NULL,
		      &udc_nrf_data, &udc_nrf_cfg,
		      POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEVICE,
		      &udc_nrf_api);
