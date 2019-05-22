/*
 * Copyright (c) 2018, Nordic Semiconductor ASA
 * Copyright (c) 2018 Sundar Subramaniyan <sundar.subramaniyan@gmail.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file  usb_dc_nrfx.c
 * @brief Nordic USB device controller driver
 *
 * The driver implements the interface between the USBD peripheral
 * driver from nrfx package and the operating system.
 */

#include <soc.h>
#include <string.h>
#include <stdio.h>
#include <kernel.h>
#include <usb/usb_dc.h>
#include <usb/usb_device.h>
#include <clock_control.h>
#include <hal/nrf_power.h>
#include <drivers/clock_control/nrf_clock_control.h>
#include <nrfx_usbd.h>


#define LOG_LEVEL CONFIG_USB_DRIVER_LOG_LEVEL
#include <logging/log.h>
LOG_MODULE_REGISTER(usb_nrfx);

#define USB_BREQUEST_SETADDRESS 		0x05
#define USB_BMREQUESTTYPE_DIR_POS		7uL
#define USB_BMREQUESTTYPE_DIR_MASK		(1uL << USB_BMREQUESTTYPE_DIR_POS)
#define USB_BMREQUESTTYPE_DIR_HOSTTODEVICE_MASK	0uL
#define USB_BMREQUESTTYPE_DIR_DEVICETOHOST_MASK	(1uL << USB_BMREQUESTTYPE_DIR_POS)
#define USB_BMREQUESTTYPE_TYPE_POS		5uL
#define USB_BMREQUESTTYPE_TYPE_MASK		(3uL << USB_BMREQUESTTYPE_TYPE_POS)
#define USB_BMREQUESTTYPE_TYPE_STANDARD_MASK	0uL
#define USB_BMREQUESTTYPE_TYPE_CLASS_MASK	(1uL << USB_BMREQUESTTYPE_TYPE_POS)
#define USB_BMREQUESTTYPE_TYPE_CLASS_VENDOR	(2uL << USB_BMREQUESTTYPE_TYPE_POS)

#define MAX_EP_BUF_SZ           64UL
#define MAX_ISO_EP_BUF_SZ       1024UL

#define USBD_EPSTATUS_EPIN_MASK         (0x1FF << USBD_EPSTATUS_EPIN0_Pos)
#define USBD_EPSTATUS_EPOUT_MASK        (0x1FF << USBD_EPSTATUS_EPOUT0_Pos)
#define USBD_EPDATASTATUS_EPIN_MASK     (0x7F << USBD_EPDATASTATUS_EPIN1_Pos)
#define USBD_EPDATASTATUS_EPOUT_MASK    (0x7F << USBD_EPDATASTATUS_EPOUT1_Pos)


/**
 * @brief nRF USBD peripheral states
 */
enum usbd_periph_state {
	USBD_DETACHED,
	USBD_ATTACHED,
	USBD_POWERED,
	USBD_SUSPENDED,
	USBD_RESUMED,
	USBD_DEFAULT,
	USBD_ADDRESS_SET,
	USBD_CONFIGURED,
};

/**
 * @brief Endpoint event types.
 */
enum usbd_ep_event_type {
	EP_EVT_SETUP_RECV,
	EP_EVT_RECV_REQ,
	EP_EVT_RECV_COMPLETE,
	EP_EVT_WRITE_COMPLETE,
};

/**
 * @brief USBD peripheral event types.
 */
enum usbd_event_type {
	USBD_EVT_POWER,
	USBD_EVT_EP,
	USBD_EVT_RESET,
	USBD_EVT_SOF,
	USBD_EVT_REINIT
};

/**
 * @brief Endpoint configuration.
 *
 * @param cb      Endpoint callback.
 * @param max_sz  Max packet size supported by endpoint.
 * @param en      Enable/Disable flag.
 * @param addr    Endpoint address.
 * @param type    Endpoint type.
 */
struct nrf_usbd_ep_cfg {
	usb_dc_ep_callback cb;
	u32_t max_sz;
	bool en;
	u8_t addr;
	enum usb_dc_ep_type type;

};

/**
 * @brief Endpoint buffer
 *
 * @param len    Remaining length to be read/written.
 * @param block  Mempool block, for freeing up buffer after use.
 * @param data	 Pointer to the data buffer	for the endpoint.
 * @param curr	 Pointer to the current offset in the endpoint buffer.
 */
struct nrf_usbd_ep_buf {
	u32_t len;
	struct k_mem_block block;
	u8_t *data;
	u8_t *curr;
};

/**
 * @brief Endpoint context
 *
 * @param cfg			Endpoint configuration
 * @param buf			Endpoint buffer
 * @param read_complete		A flag indicating that DMA read operation has been completed.
 * @param read_pending		A flag indicating that the Host has requested a data transfer.
 * @param write_in_progress	A flag indicating that write operation has been scheduled.
 * @param write_fragmented	A flag indicating that IN transfer has been fragmented.
 */
struct nrf_usbd_ep_ctx {
	struct nrf_usbd_ep_cfg cfg;
	struct nrf_usbd_ep_buf buf;
	volatile bool read_complete;
	volatile bool read_pending;
	volatile bool write_in_progress;
	bool write_fragmented;
};

/**
 * @brief Endpoint event structure
 *
 * @param ep		Endpoint control block pointer
 * @param evt_type	Event type
 */
struct usbd_ep_event {
	struct nrf_usbd_ep_ctx *ep;
	enum usbd_ep_event_type evt_type;
};

/**
 * @brief Power event structure
 *
 * @param state		New USBD peripheral state.
 */
struct usbd_pwr_event {
	enum usbd_periph_state state;
};

/**
 * @brief Endpoint USB event
 *	  Used by ISR to send events to work handler
 *
 * @param node		Used by the kernel for FIFO management
 * @param block		Mempool block pointer for freeing up after use
 * @param evt		Event data field
 * @param evt_type	Type of event that has occurred from the USBD peripheral
 */
struct usbd_event {
	sys_snode_t node;
	struct k_mem_block block;
	union {
		struct usbd_ep_event ep_evt;
		struct usbd_pwr_event pwr_evt;
	} evt;
	enum usbd_event_type evt_type;
};

/**
 * @brief Fifo element pool
 *	Used for allocating fifo elements to pass from ISR to work handler
 * TODO: The number of FIFO elements is an arbitrary number now but it should
 * be derived from the theoretical number of backlog events possible depending
 * on the number of endpoints configured.
 */
#define FIFO_ELEM_MIN_SZ        sizeof(struct usbd_event)
#define FIFO_ELEM_MAX_SZ        sizeof(struct usbd_event)
#define FIFO_ELEM_COUNT         CONFIG_USB_NRFX_EVT_QUEUE_SIZE
#define FIFO_ELEM_ALIGN         sizeof(unsigned int)

K_MEM_POOL_DEFINE(fifo_elem_pool, FIFO_ELEM_MIN_SZ, FIFO_ELEM_MAX_SZ,
		  FIFO_ELEM_COUNT, FIFO_ELEM_ALIGN);

#if CONFIG_USB_NRFX_EVT_QUEUE_SIZE < 4
#error Invalid USBD event queue size (CONFIG_USB_NRFX_EVT_QUEUE_SIZE). Minimum size: 4.
#endif

/**
 * @brief Endpoint buffer pool
 *	Used for allocating buffers for the endpoints' data transfer
 *	Max pool size possible: 3072 Bytes (16 EP * 64B + 2 ISO * 1024B)
 */

/** Number of IN Endpoints configured (including control) */
#define CFG_EPIN_CNT (DT_NORDIC_NRF_USBD_USBD_0_NUM_IN_ENDPOINTS + \
		      DT_NORDIC_NRF_USBD_USBD_0_NUM_BIDIR_ENDPOINTS)

/** Number of OUT Endpoints configured (including control) */
#define CFG_EPOUT_CNT (DT_NORDIC_NRF_USBD_USBD_0_NUM_OUT_ENDPOINTS + \
		       DT_NORDIC_NRF_USBD_USBD_0_NUM_BIDIR_ENDPOINTS)

/** Number of ISO IN Endpoints */
#define CFG_EP_ISOIN_CNT DT_NORDIC_NRF_USBD_USBD_0_NUM_ISOIN_ENDPOINTS

/** Number of ISO OUT Endpoints */
#define CFG_EP_ISOOUT_CNT DT_NORDIC_NRF_USBD_USBD_0_NUM_ISOOUT_ENDPOINTS

/** ISO endpoint index */
#define EP_ISOIN_INDEX CFG_EPIN_CNT
#define EP_ISOOUT_INDEX (CFG_EPIN_CNT + CFG_EP_ISOIN_CNT + CFG_EPOUT_CNT)

/** Minimum endpoint buffer size */
#define EP_BUF_MIN_SZ MAX_EP_BUF_SZ

/** Maximum endpoint buffer size */
#if (CFG_EP_ISOIN_CNT || CFG_EP_ISOOUT_CNT)
#define EP_BUF_MAX_SZ MAX_ISO_EP_BUF_SZ
#else
#define EP_BUF_MAX_SZ MAX_EP_BUF_SZ
#endif

/** Total endpoints configured */
#define CFG_EP_CNT (CFG_EPIN_CNT + CFG_EP_ISOIN_CNT + \
		    CFG_EPOUT_CNT + CFG_EP_ISOOUT_CNT)

/** Total buffer size for all endpoints */
#define EP_BUF_TOTAL ((CFG_EPIN_CNT * MAX_EP_BUF_SZ) +	       \
		      (CFG_EPOUT_CNT * MAX_EP_BUF_SZ) +	       \
		      (CFG_EP_ISOIN_CNT * MAX_ISO_EP_BUF_SZ) + \
		      (CFG_EP_ISOOUT_CNT * MAX_ISO_EP_BUF_SZ))

/** Total number of maximum sized buffers needed */
#define EP_BUF_COUNT ((EP_BUF_TOTAL / EP_BUF_MAX_SZ) + \
		      ((EP_BUF_TOTAL % EP_BUF_MAX_SZ) ? 1 : 0))

/** 4 Byte Buffer alignment required by hardware */
#define EP_BUF_ALIGN sizeof(unsigned int)

K_MEM_POOL_DEFINE(ep_buf_pool, EP_BUF_MIN_SZ, EP_BUF_MAX_SZ,
		  EP_BUF_COUNT, EP_BUF_ALIGN);

/**
 * @brief USBD control structure
 *
 * @param status_cb	Status callback for USB DC notifications
 * @param attached	USBD Attached flag
 * @param ready		USBD Ready flag set after pullup
 * @param usb_work	USBD work item
 * @param work_queue	FIFO used for queuing up events from ISR
 * @param drv_lock	Mutex for thread-safe nrfx driver use
 * @param ep_ctx	Endpoint contexts
 * @param ctrl_read_len	State of control read operation (EP0).
 */
struct nrf_usbd_ctx {
	usb_dc_status_callback status_cb;

	bool attached;
	bool ready;

	struct k_work  usb_work;
	struct k_mutex drv_lock;

	struct nrf_usbd_ep_ctx ep_ctx[CFG_EP_CNT];

	u16_t ctrl_read_len;
};


K_FIFO_DEFINE(work_queue);


static struct nrf_usbd_ctx usbd_ctx = {
	.attached = false,
	.ready = false,
};

static inline struct nrf_usbd_ctx *get_usbd_ctx(void)
{
	return &usbd_ctx;
}

static inline bool dev_attached(void)
{
	return get_usbd_ctx()->attached;
}

static inline bool dev_ready(void)
{
	return get_usbd_ctx()->ready;
}

static inline nrfx_usbd_ep_t ep_addr_to_nrfx(uint8_t ep)
{
	return (nrfx_usbd_ep_t)ep;
}

static inline uint8_t nrfx_addr_to_ep(nrfx_usbd_ep_t ep)
{
	return (uint8_t)ep;
}

static inline bool ep_is_valid(const u8_t ep)
{
	u8_t ep_num = NRF_USBD_EP_NR_GET(ep);

	if (NRF_USBD_EPIN_CHECK(ep)) {
		if (unlikely(NRF_USBD_EPISO_CHECK(ep))) {
			if (CFG_EP_ISOIN_CNT == 0) {
				return false;
			}
		} else {
			if (ep_num >= CFG_EPIN_CNT) {
				return false;
			}
		}
	} else {
		if (unlikely(NRF_USBD_EPISO_CHECK(ep))) {
			if (CFG_EP_ISOOUT_CNT == 0) {
				return false;
			}
		} else {
			if (ep_num >= CFG_EPOUT_CNT) {
				return false;
			}
		}
	}

	return true;
}

static struct nrf_usbd_ep_ctx *endpoint_ctx(const u8_t ep)
{
	struct nrf_usbd_ctx *ctx;
	u8_t ep_num;

	if (!ep_is_valid(ep)) {
		return NULL;
	}

	ctx = get_usbd_ctx();
	ep_num = NRF_USBD_EP_NR_GET(ep);

	if (NRF_USBD_EPIN_CHECK(ep)) {
		if (unlikely(NRF_USBD_EPISO_CHECK(ep))) {
			return &ctx->ep_ctx[EP_ISOIN_INDEX];
		} else {
			return &ctx->ep_ctx[ep_num];
		}
	} else {
		if (unlikely(NRF_USBD_EPISO_CHECK(ep))) {
			return &ctx->ep_ctx[EP_ISOOUT_INDEX];
		} else {
			return &ctx->ep_ctx[CFG_EPIN_CNT +
					    CFG_EP_ISOIN_CNT +
					    ep_num];
		}
	}

	return NULL;
}

static struct nrf_usbd_ep_ctx *in_endpoint_ctx(const u8_t ep)
{
	return endpoint_ctx(NRF_USBD_EPIN(ep));
}

static struct nrf_usbd_ep_ctx *out_endpoint_ctx(const u8_t ep)
{
	return endpoint_ctx(NRF_USBD_EPOUT(ep));
}

/**
 * @brief Schedule USBD event processing.
 *
 * Should be called after usbd_evt_put().
 */
static inline void usbd_work_schedule(void)
{
	k_work_submit(&get_usbd_ctx()->usb_work);
}

/**
 * @brief Free previously allocated USBD event.
 *
 * Should be called after usbd_evt_get().
 *
 * @param Pointer to the USBD event structure.
 */
static inline void usbd_evt_free(struct usbd_event *ev)
{
	k_mem_pool_free(&ev->block);
}

/**
 * @brief Enqueue USBD event.
 *
 * @param Pointer to the previously allocated and filled event structure.
 */
static inline void usbd_evt_put(struct usbd_event *ev)
{
	k_fifo_put(&work_queue, ev);
}

/**
 * @brief Get next enqueued USBD event if present.
 */
static inline struct usbd_event *usbd_evt_get(void)
{
	return k_fifo_get(&work_queue, K_NO_WAIT);
}

/**
 * @brief Drop all enqueued events.
 */
static inline void usbd_evt_flush(void)
{
	struct usbd_event *ev;

	do {
		ev = usbd_evt_get();
		if (ev) {
			usbd_evt_free(ev);
		}
	} while (ev != NULL);
}

/**
 * @brief Allocate USBD event.
 *
 * This function should be called prior to usbd_evt_put().
 *
 * @returns Pointer to the allocated event or NULL if there was no space left.
 */
static inline struct usbd_event *usbd_evt_alloc(void)
{
	int ret;
	struct usbd_event *ev;
	struct k_mem_block block;

	ret = k_mem_pool_alloc(&fifo_elem_pool, &block,
			       sizeof(struct usbd_event),
			       K_NO_WAIT);

	if (ret < 0) {
		LOG_ERR("USBD event allocation failed!");

		/* This should NOT happen in a properly designed system.
		 * Allocation may fail if workqueue thread is starved
		 * or event queue size is too small (CONFIG_USB_NRFX_EVT_QUEUE_SIZE).
		 * Wipe all events, free the space and schedule reinitialization.
		 */
		usbd_evt_flush();

		ret = k_mem_pool_alloc(&fifo_elem_pool, &block,
					       sizeof(struct usbd_event),
					       K_NO_WAIT);
		if (ret < 0) {
			/* This should never fail in a properly operating system. */
			LOG_ERR("USBD event memory corrupted.");
			__ASSERT_NO_MSG(0);
			return NULL;
		}

		ev = (struct usbd_event *)block.data;
		ev->block = block;
		ev->evt_type = USBD_EVT_REINIT;
		usbd_evt_put(ev);
		usbd_work_schedule();

		return NULL;
	}

	ev = (struct usbd_event *)block.data;
	ev->block = block;

	return ev;
}

void usb_dc_nrfx_power_event_callback(nrf_power_event_t event)
{
	enum usbd_periph_state new_state;

	switch (event) {
	case NRF_POWER_EVENT_USBDETECTED:
		new_state = USBD_ATTACHED;
		break;
	case NRF_POWER_EVENT_USBPWRRDY:
		new_state = USBD_POWERED;
		break;
	case NRF_POWER_EVENT_USBREMOVED:
		new_state = USBD_DETACHED;
		break;
	default:
		LOG_ERR("Unknown USB power event");
		return;
	}

	struct usbd_event *ev = usbd_evt_alloc();

	if (!ev) {
		return;
	}

	ev->evt_type = USBD_EVT_POWER;
	ev->evt.pwr_evt.state = new_state;


	usbd_evt_put(ev);

	if (usbd_ctx.attached) {
		usbd_work_schedule();
	}
}

/**
 * @brief Enable/Disable the HF clock
 *
 * Toggle the HF clock. It needs to be enabled for USBD data exchange
 *
 * @param on		Set true to enable the HF clock, false to disable.
 * @param blocking	Set true to block wait till HF clock stabilizes.
 *
 * @return 0 on success, error number otherwise
 */
static int hf_clock_enable(bool on, bool blocking)
{
	int ret = -ENODEV;
	struct device *clock;
	static bool clock_requested;

	clock = device_get_binding(DT_NORDIC_NRF_CLOCK_0_LABEL "_16M");
	if (!clock) {
		LOG_ERR("NRF HF Clock device not found!");
		return ret;
	}

	if (on) {
		if (clock_requested) {
			/* Do not request HFCLK multiple times. */
			return 0;
		}
		ret = clock_control_on(clock, (void *)blocking);
	} else {
		if (!clock_requested) {
			/* Cancel the operation if clock has not
			 * been requested by this driver before.
			 */
			return 0;
		}
		ret = clock_control_off(clock, (void *)blocking);
		if (ret == -EBUSY) {
			/* This is an expected behaviour.
			 * -EBUSY means that some other module has also
			 * requested the clock to run.
			 */
			ret = 0;
		}
	}

	if (ret && (blocking || (ret != -EINPROGRESS))) {
		LOG_ERR("HF clock %s fail: %d",
			on ? "start" : "stop", ret);
		return ret;
	}

	clock_requested = on;
	LOG_DBG("HF clock %s success (%d)", on ? "start" : "stop", ret);

	/* NOTE: Non-blocking HF clock enable can return -EINPROGRESS
	 * if HF clock start was already requested. Such error code
	 * does not need to be propagated, hence returned value is 0.
	 */
	return 0;
}

static void usbd_enable_endpoints(struct nrf_usbd_ctx *ctx)
{
	struct nrf_usbd_ep_ctx *ep_ctx;
	int i;

	for (i = 0; i < CFG_EPIN_CNT; i++) {
		ep_ctx = in_endpoint_ctx(i);
		__ASSERT_NO_MSG(ep_ctx);

		if (ep_ctx->cfg.en) {
			nrfx_usbd_ep_enable(ep_addr_to_nrfx(ep_ctx->cfg.addr));
		}
	}

	if (CFG_EP_ISOIN_CNT) {
		ep_ctx = in_endpoint_ctx(NRF_USBD_EPIN(8));
		__ASSERT_NO_MSG(ep_ctx);

		if (ep_ctx->cfg.en) {
			nrfx_usbd_ep_enable(ep_addr_to_nrfx(ep_ctx->cfg.addr));
		}
	}

	for (i = 0; i < CFG_EPOUT_CNT; i++) {
		ep_ctx = out_endpoint_ctx(i);
		__ASSERT_NO_MSG(ep_ctx);

		if (ep_ctx->cfg.en) {
			nrfx_usbd_ep_enable(ep_addr_to_nrfx(ep_ctx->cfg.addr));
		}
	}

	if (CFG_EP_ISOOUT_CNT) {
		ep_ctx = out_endpoint_ctx(NRF_USBD_EPOUT(8));
		__ASSERT_NO_MSG(ep_ctx);

		if (ep_ctx->cfg.en) {
			nrfx_usbd_ep_enable(ep_addr_to_nrfx(ep_ctx->cfg.addr));
		}
	}
}

/**
 * @brief Reset endpoint state.
 *
 * Resets the internal logic state for a given endpoint.
 *
 * @param[in]  ep_cts   Endpoint structure control block
 */
static void ep_ctx_reset(struct nrf_usbd_ep_ctx *ep_ctx)
{
	ep_ctx->buf.data = ep_ctx->buf.block.data;
	ep_ctx->buf.curr = ep_ctx->buf.data;
	ep_ctx->buf.len  = 0U;

	ep_ctx->read_complete = true;
	ep_ctx->read_pending = false;
	ep_ctx->write_in_progress = false;
}

/**
 * @brief Initialize all endpoint structures.
 *
 * Endpoint buffers are allocated during the first call of this function.
 * This function may also be called again on every USB reset event
 * to reinitialize the state of all endpoints.
 */
static int eps_ctx_init(void)
{
	struct nrf_usbd_ep_ctx *ep_ctx;
	int err;
	u32_t i;

	for (i = 0U; i < CFG_EPIN_CNT; i++) {
		ep_ctx = in_endpoint_ctx(i);
		__ASSERT_NO_MSG(ep_ctx);

		if (!ep_ctx->buf.block.data) {
			err = k_mem_pool_alloc(&ep_buf_pool, &ep_ctx->buf.block,
					       MAX_EP_BUF_SZ, K_NO_WAIT);
			if (err < 0) {
				LOG_ERR("EP buffer alloc failed for EPIN%d", i);
				return -ENOMEM;
			}
		}

		ep_ctx_reset(ep_ctx);
	}

	for (i = 0U; i < CFG_EPOUT_CNT; i++) {
		ep_ctx = out_endpoint_ctx(i);
		__ASSERT_NO_MSG(ep_ctx);

		if (!ep_ctx->buf.block.data) {
			err = k_mem_pool_alloc(&ep_buf_pool, &ep_ctx->buf.block,
					       MAX_EP_BUF_SZ, K_NO_WAIT);
			if (err < 0) {
				LOG_ERR("EP buffer alloc failed for EPOUT%d", i);
				return -ENOMEM;
			}
		}

		ep_ctx_reset(ep_ctx);
	}

	if (CFG_EP_ISOIN_CNT) {
		ep_ctx = in_endpoint_ctx(NRF_USBD_EPIN(8));
		__ASSERT_NO_MSG(ep_ctx);

		if (!ep_ctx->buf.block.data) {
			err = k_mem_pool_alloc(&ep_buf_pool, &ep_ctx->buf.block,
					       MAX_ISO_EP_BUF_SZ, K_NO_WAIT);
			if (err < 0) {
				LOG_ERR("EP buffer alloc failed for ISOIN");
				return -ENOMEM;
			}
		}

		ep_ctx_reset(ep_ctx);
	}

	if (CFG_EP_ISOOUT_CNT) {
		ep_ctx = out_endpoint_ctx(NRF_USBD_EPOUT(8));
		__ASSERT_NO_MSG(ep_ctx);

		if (!ep_ctx->buf.block.data) {
			err = k_mem_pool_alloc(&ep_buf_pool, &ep_ctx->buf.block,
					       MAX_ISO_EP_BUF_SZ, K_NO_WAIT);
			if (err < 0) {
				LOG_ERR("EP buffer alloc failed for ISOOUT");
				return -ENOMEM;
			}
		}

		ep_ctx_reset(ep_ctx);
	}

	return 0;
}

static void eps_ctx_uninit(void)
{
	struct nrf_usbd_ep_ctx *ep_ctx;
	u32_t i;

	for (i = 0U; i < CFG_EPIN_CNT; i++) {
		ep_ctx = in_endpoint_ctx(i);
		__ASSERT_NO_MSG(ep_ctx);
		k_mem_pool_free(&ep_ctx->buf.block);
		memset(ep_ctx, 0, sizeof(*ep_ctx));
	}

	for (i = 0U; i < CFG_EPOUT_CNT; i++) {
		ep_ctx = out_endpoint_ctx(i);
		__ASSERT_NO_MSG(ep_ctx);
		k_mem_pool_free(&ep_ctx->buf.block);
		memset(ep_ctx, 0, sizeof(*ep_ctx));
	}

	if (CFG_EP_ISOIN_CNT) {
		ep_ctx = in_endpoint_ctx(NRF_USBD_EPIN(8));
		__ASSERT_NO_MSG(ep_ctx);
		k_mem_pool_free(&ep_ctx->buf.block);
		memset(ep_ctx, 0, sizeof(*ep_ctx));
	}

	if (CFG_EP_ISOOUT_CNT) {
		ep_ctx = out_endpoint_ctx(NRF_USBD_EPOUT(8));
		__ASSERT_NO_MSG(ep_ctx);
		k_mem_pool_free(&ep_ctx->buf.block);
		memset(ep_ctx, 0, sizeof(*ep_ctx));
	}
}

static inline void usbd_work_process_pwr_events(struct usbd_pwr_event *pwr_evt)
{
	struct nrf_usbd_ctx *ctx = get_usbd_ctx();

	switch (pwr_evt->state) {
	case USBD_ATTACHED:
		if (!nrfx_usbd_is_enabled()) {
			LOG_DBG("USB detected");
			nrfx_usbd_enable();
			(void) hf_clock_enable(true, false);
		}

		/* No callback here.
		 * Stack will be notified when the peripheral is ready.
		 */
		break;

	case USBD_POWERED:
		usbd_enable_endpoints(ctx);
		nrfx_usbd_start(true);
		ctx->ready = true;

		LOG_DBG("USB Powered");

		if (ctx->status_cb) {
			ctx->status_cb(USB_DC_CONNECTED, NULL);
		}
		break;

	case USBD_DETACHED:
		ctx->ready = false;
		nrfx_usbd_disable();
		(void) hf_clock_enable(false, false);

		LOG_DBG("USB Removed");

		if (ctx->status_cb) {
			ctx->status_cb(USB_DC_DISCONNECTED, NULL);
		}
		break;

	case USBD_SUSPENDED:
		if (dev_ready()) {
			nrfx_usbd_suspend();
			LOG_DBG("USB Suspend state");

			if (ctx->status_cb) {
				ctx->status_cb(USB_DC_SUSPEND, NULL);
			}
		}
		break;
	case USBD_RESUMED:
		if (ctx->status_cb && dev_ready()) {
			LOG_DBG("USB resume");
			ctx->status_cb(USB_DC_RESUME, NULL);
		}
		break;

	default:
		break;
	}
}

static inline void usbd_work_process_setup(struct nrf_usbd_ep_ctx *ep_ctx)
{
	__ASSERT_NO_MSG(ep_ctx);
	__ASSERT(ep_ctx->cfg.type == USB_DC_EP_CONTROL,
		 "Invalid event on CTRL EP.");

	struct usb_setup_packet *usbd_setup;

	/* SETUP packets are handled by USBD hardware.
	 * For compatibility with the USB stack,
	 * SETUP packet must be reassembled.
	 */
	usbd_setup = (struct usb_setup_packet *)ep_ctx->buf.data;
	memset(usbd_setup, 0, sizeof(struct usb_setup_packet));
	usbd_setup->bmRequestType = nrf_usbd_setup_bmrequesttype_get();
	usbd_setup->bRequest = nrf_usbd_setup_brequest_get();
	usbd_setup->wValue = nrf_usbd_setup_wvalue_get();
	usbd_setup->wIndex = nrf_usbd_setup_windex_get();
	usbd_setup->wLength = nrf_usbd_setup_wlength_get();
	ep_ctx->buf.len = sizeof(struct usb_setup_packet);

	LOG_DBG("SETUP: r:%d rt:%d v:%d i:%d l:%d",
		(u32_t)usbd_setup->bRequest,
		(u32_t)usbd_setup->bmRequestType,
		(u32_t)usbd_setup->wValue,
		(u32_t)usbd_setup->wIndex,
		(u32_t)usbd_setup->wLength);

	/* Inform the stack. */
	ep_ctx->cfg.cb(ep_ctx->cfg.addr, USB_DC_EP_SETUP);

	struct nrf_usbd_ctx *ctx = get_usbd_ctx();

	if (((usbd_setup->bmRequestType & USB_BMREQUESTTYPE_DIR_MASK)
	     == USB_BMREQUESTTYPE_DIR_HOSTTODEVICE_MASK)
	    && (usbd_setup->wLength)) {
		struct nrf_usbd_ctx *ctx = get_usbd_ctx();

		ctx->ctrl_read_len -= usbd_setup->wLength;
		nrfx_usbd_setup_data_clear();
	} else {
		ctx->ctrl_read_len = 0U;
	}
}

static inline void usbd_work_process_recvreq(struct nrf_usbd_ctx *ctx,
					     struct nrf_usbd_ep_ctx *ep_ctx)
{
	if (!ep_ctx->read_pending) {
		return;
	}
	if (!ep_ctx->read_complete) {
		return;
	}

	ep_ctx->read_pending = false;
	ep_ctx->read_complete = false;

	k_mutex_lock(&ctx->drv_lock, K_FOREVER);
	NRFX_USBD_TRANSFER_OUT(transfer, ep_ctx->buf.data,
			       ep_ctx->cfg.max_sz);
	nrfx_err_t err = nrfx_usbd_ep_transfer(
		ep_addr_to_nrfx(ep_ctx->cfg.addr), &transfer);
	if (err != NRFX_SUCCESS) {
		LOG_ERR("nRF USBD transfer error (OUT): %d.", err);
	}
	k_mutex_unlock(&ctx->drv_lock);
}


static inline void usbd_work_process_ep_events(struct usbd_ep_event *ep_evt)
{
	struct nrf_usbd_ctx *ctx = get_usbd_ctx();
	struct nrf_usbd_ep_ctx *ep_ctx = ep_evt->ep;

	__ASSERT_NO_MSG(ep_ctx);

	switch (ep_evt->evt_type) {
	case EP_EVT_SETUP_RECV:
		usbd_work_process_setup(ep_ctx);
		break;

	case EP_EVT_RECV_REQ:
		usbd_work_process_recvreq(ctx, ep_ctx);
		break;

	case EP_EVT_RECV_COMPLETE:
		ep_ctx->cfg.cb(ep_ctx->cfg.addr,
			       USB_DC_EP_DATA_OUT);
		break;

	case EP_EVT_WRITE_COMPLETE:
		if ((ep_ctx->cfg.type == USB_DC_EP_CONTROL)
		    && (!ep_ctx->write_fragmented)) {
			/* Trigger the hardware to perform
			 * status stage, but only if there is
			 * no more data to send (IN transfer
			 * has not beed fragmented).
			 */
			k_mutex_lock(&ctx->drv_lock, K_FOREVER);
			nrfx_usbd_setup_clear();
			k_mutex_unlock(&ctx->drv_lock);
		}
		ep_ctx->cfg.cb(ep_ctx->cfg.addr,
			       USB_DC_EP_DATA_IN);
		break;
	default:
		break;
	}
}

static void usbd_event_transfer_ctrl(nrfx_usbd_evt_t const *const p_event)
{
	struct nrf_usbd_ep_ctx *ep_ctx =
		endpoint_ctx(p_event->data.eptransfer.ep);

	if (NRF_USBD_EPIN_CHECK(p_event->data.eptransfer.ep)) {
		switch (p_event->data.eptransfer.status) {
		case NRFX_USBD_EP_OK: {
			struct usbd_event *ev = usbd_evt_alloc();

			if (!ev) {
				return;
			}

			ep_ctx->write_in_progress = false;
			ev->evt_type = USBD_EVT_EP;
			ev->evt.ep_evt.evt_type = EP_EVT_WRITE_COMPLETE;
			ev->evt.ep_evt.ep = ep_ctx;

			LOG_DBG("ctrl write complete");
			usbd_evt_put(ev);
			usbd_work_schedule();
		}
		break;

		default: {
			LOG_ERR(
				"Unexpected event (nrfx_usbd): %d, ep %d",
				p_event->data.eptransfer.status,
				p_event->data.eptransfer.ep);
		}
		break;
		}
	} else {
		switch (p_event->data.eptransfer.status) {
		case NRFX_USBD_EP_WAITING: {
			struct usbd_event *ev = usbd_evt_alloc();

			if (!ev) {
				return;
			}

			LOG_DBG("ctrl read request");

			ep_ctx->read_pending = true;
			ev->evt_type = USBD_EVT_EP;
			ev->evt.ep_evt.evt_type = EP_EVT_RECV_REQ;
			ev->evt.ep_evt.ep = ep_ctx;

			usbd_evt_put(ev);
			usbd_work_schedule();
		}
		break;

		case NRFX_USBD_EP_OK: {
			struct nrf_usbd_ctx *ctx = get_usbd_ctx();
			struct usbd_event *ev = usbd_evt_alloc();

			if (!ev) {
				return;
			}
			nrfx_usbd_ep_status_t err_code;

			ev->evt_type = USBD_EVT_EP;
			ev->evt.ep_evt.evt_type = EP_EVT_RECV_COMPLETE;
			ev->evt.ep_evt.ep = ep_ctx;

			err_code = nrfx_usbd_ep_status_get(
				p_event->data.eptransfer.ep, &ep_ctx->buf.len);

			if (err_code != NRFX_USBD_EP_OK) {
				LOG_ERR("_ep_status_get failed! Code: %d.",
					err_code);
				__ASSERT_NO_MSG(0);
			}
			LOG_DBG("ctrl read done: %d", ep_ctx->buf.len);

			if (ctx->ctrl_read_len > ep_ctx->buf.len) {
				ctx->ctrl_read_len -= ep_ctx->buf.len;
				nrfx_usbd_setup_data_clear();
			} else {
				ctx->ctrl_read_len = 0U;
			}

			usbd_evt_put(ev);
			usbd_work_schedule();
		}
		break;

		default: {
			LOG_ERR("Unexpected event from nrfx_usbd: %d, ep %d",
				p_event->data.eptransfer.status,
				p_event->data.eptransfer.ep);
		}
		break;
		}
	}
}

static void usbd_event_transfer_data(nrfx_usbd_evt_t const *const p_event)
{
	struct nrf_usbd_ep_ctx *ep_ctx =
		endpoint_ctx(p_event->data.eptransfer.ep);

	if (NRF_USBD_EPIN_CHECK(p_event->data.eptransfer.ep)) {
		switch (p_event->data.eptransfer.status) {
		case NRFX_USBD_EP_OK: {
			struct usbd_event *ev = usbd_evt_alloc();

			if (!ev) {
				return;
			}

			LOG_DBG("write complete, ep %d",
				(u32_t)p_event->data.eptransfer.ep);

			ep_ctx->write_in_progress = false;
			ev->evt_type = USBD_EVT_EP;
			ev->evt.ep_evt.evt_type = EP_EVT_WRITE_COMPLETE;
			ev->evt.ep_evt.ep = ep_ctx;
			usbd_evt_put(ev);
			usbd_work_schedule();
		}
		break;

		default: {
			LOG_ERR("Unexpected event from nrfx_usbd: %d, ep %d",
				p_event->data.eptransfer.status,
				p_event->data.eptransfer.ep);
		}
		break;
		}

	} else {
		switch (p_event->data.eptransfer.status) {
		case NRFX_USBD_EP_WAITING: {
			struct usbd_event *ev = usbd_evt_alloc();

			if (!ev) {
				return;
			}

			LOG_DBG("read request, ep %d",
				(u32_t)p_event->data.eptransfer.ep);

			ep_ctx->read_pending = true;
			ev->evt_type = USBD_EVT_EP;
			ev->evt.ep_evt.evt_type = EP_EVT_RECV_REQ;
			ev->evt.ep_evt.ep = ep_ctx;

			usbd_evt_put(ev);
			usbd_work_schedule();
		}
		break;

		case NRFX_USBD_EP_OK: {
			struct usbd_event *ev = usbd_evt_alloc();

			if (!ev) {
				return;
			}

			ep_ctx->buf.len = nrf_usbd_ep_amount_get(
				p_event->data.eptransfer.ep);

			LOG_DBG("read complete, ep %d, len %d",
				(u32_t)p_event->data.eptransfer.ep,
				ep_ctx->buf.len);

			ev->evt_type = USBD_EVT_EP;
			ev->evt.ep_evt.evt_type = EP_EVT_RECV_COMPLETE;
			ev->evt.ep_evt.ep = ep_ctx;

			usbd_evt_put(ev);
			usbd_work_schedule();
		}
		break;

		default: {
			LOG_ERR("Unexpected event from nrfx_usbd: %d, ep %d",
				p_event->data.eptransfer.status,
				p_event->data.eptransfer.ep);
		}
		break;
		}
	}
}

/**
 * @brief nRFx USBD driver event handler function.
 */
static void usbd_event_handler(nrfx_usbd_evt_t const *const p_event)
{
	struct nrf_usbd_ep_ctx *ep_ctx;
	struct usbd_event evt = {0};
	bool put_evt = false;

	switch (p_event->type) {
	case NRFX_USBD_EVT_SUSPEND:
		LOG_DBG("SUSPEND state detected.");
		evt.evt_type = USBD_EVT_POWER;
		evt.evt.pwr_evt.state = USBD_SUSPENDED;
		put_evt = true;
		break;
	case NRFX_USBD_EVT_RESUME:
		LOG_DBG("RESUMING from suspend.");
		evt.evt_type = USBD_EVT_POWER;
		evt.evt.pwr_evt.state = USBD_RESUMED;
		put_evt = true;
		break;
	case NRFX_USBD_EVT_WUREQ:
		LOG_DBG("RemoteWU initiated.");
		break;
	case NRFX_USBD_EVT_RESET:
		evt.evt_type = USBD_EVT_RESET;
		put_evt = true;
		break;
	case NRFX_USBD_EVT_SOF:
		if (IS_ENABLED(CONFIG_USB_DEVICE_SOF)) {
			evt.evt_type = USBD_EVT_SOF;
			put_evt = true;
		}
		break;

	case NRFX_USBD_EVT_EPTRANSFER:
		ep_ctx = endpoint_ctx(p_event->data.eptransfer.ep);
		switch (ep_ctx->cfg.type) {
		case USB_DC_EP_CONTROL:
			usbd_event_transfer_ctrl(p_event);
			break;
		case USB_DC_EP_BULK:
		case USB_DC_EP_INTERRUPT:
			usbd_event_transfer_data(p_event);
			break;
		case USB_DC_EP_ISOCHRONOUS:
			usbd_event_transfer_data(p_event);
			break;
		default:
			break;
		}
		break;

	case NRFX_USBD_EVT_SETUP: {
		nrfx_usbd_setup_t drv_setup;

		nrfx_usbd_setup_get(&drv_setup);
		if ((drv_setup.bRequest != USB_BREQUEST_SETADDRESS)
		    || ((drv_setup.bmRequestType & USB_BMREQUESTTYPE_TYPE_MASK)
			!= USB_BMREQUESTTYPE_TYPE_STANDARD_MASK)) {
			/* SetAddress is habdled by USBD hardware.
			 * No software action required.
			 */

			struct nrf_usbd_ep_ctx *ep_ctx =
				endpoint_ctx(NRF_USBD_EPOUT(0));

			evt.evt_type = USBD_EVT_EP;
			evt.evt.ep_evt.ep = ep_ctx;
			evt.evt.ep_evt.evt_type = EP_EVT_SETUP_RECV;
			put_evt = true;
		}
		break;
	}

	default:
		break;
	}

	if (put_evt) {
		struct usbd_event *ev;

		ev = usbd_evt_alloc();
		if (!ev) {
			return;
		}
		ev->evt_type = evt.evt_type;
		ev->evt = evt.evt;
		usbd_evt_put(ev);
		usbd_work_schedule();
	}
}

static inline void usbd_reinit(void)
{
	int ret;
	nrfx_err_t err;

	nrf5_power_usb_power_int_enable(false);
	nrfx_usbd_disable();
	nrfx_usbd_uninit();

	usbd_evt_flush();
	ret = eps_ctx_init();
	__ASSERT_NO_MSG(ret == 0);

	nrf5_power_usb_power_int_enable(true);
	err = nrfx_usbd_init(usbd_event_handler);

	if (err != NRFX_SUCCESS) {
		LOG_DBG("nRF USBD driver reinit failed. Code: %d.",
			(u32_t)err);
		__ASSERT_NO_MSG(0);
	}
}


/* Work handler */
static void usbd_work_handler(struct k_work *item)
{
	struct nrf_usbd_ctx *ctx;
	struct usbd_event *ev;

	ctx = CONTAINER_OF(item, struct nrf_usbd_ctx, usb_work);

	while ((ev = usbd_evt_get()) != NULL) {
		if (!dev_ready() && ev->evt_type != USBD_EVT_POWER) {
			/* Drop non-power events when cable is detached. */
			continue;
		}

		switch (ev->evt_type) {
		case USBD_EVT_EP:
			if (!ctx->attached) {
				LOG_ERR("EP %d event dropped (not attached).",
					(u32_t)ev->evt.ep_evt.ep->cfg.addr);
			}
			usbd_work_process_ep_events(&ev->evt.ep_evt);
			break;
		case USBD_EVT_POWER:
			usbd_work_process_pwr_events(&ev->evt.pwr_evt);
			break;
		case USBD_EVT_RESET:
			LOG_DBG("USBD reset event.");
			k_mutex_lock(&ctx->drv_lock, K_FOREVER);
			eps_ctx_init();
			k_mutex_unlock(&ctx->drv_lock);

			if (ctx->status_cb) {
				ctx->status_cb(USB_DC_RESET, NULL);
			}
			break;
		case USBD_EVT_SOF:
			if (ctx->status_cb) {
				ctx->status_cb(USB_DC_SOF, NULL);
			}
			break;
		case USBD_EVT_REINIT: {
				/* Reinitialize the peripheral after queue overflow. */
				LOG_ERR("USBD event queue full!");
				usbd_reinit();
				break;
			}
		default:
			LOG_ERR("Unknown USBD event: %"PRId16".", ev->evt_type);
			break;
		}
		usbd_evt_free(ev);
	}
}

int usb_dc_attach(void)
{
	struct nrf_usbd_ctx *ctx = get_usbd_ctx();
	nrfx_err_t err;
	int ret;

	if (ctx->attached) {
		return 0;
	}

	k_work_init(&ctx->usb_work, usbd_work_handler);
	k_mutex_init(&ctx->drv_lock);

	IRQ_CONNECT(DT_NORDIC_NRF_USBD_USBD_0_IRQ,
		    DT_NORDIC_NRF_USBD_USBD_0_IRQ_PRIORITY,
		    nrfx_isr, nrfx_usbd_irq_handler, 0);

	err = nrfx_usbd_init(usbd_event_handler);

	if (err != NRFX_SUCCESS) {
		LOG_DBG("nRF USBD driver init failed. Code: %d.",
			(u32_t)err);
		return -EIO;
	}
	nrf5_power_usb_power_int_enable(true);

	ret = eps_ctx_init();
	if (ret == 0) {
		ctx->attached = true;
	}

	if (!k_fifo_is_empty(&work_queue)) {
		usbd_work_schedule();
	}

	if (nrf_power_usbregstatus_vbusdet_get()) {
		/* USBDETECTED event is be generated on cable attachment and
		 * when cable is already attached during reset, but not when
		 * the peripheral is re-enabled.
		 * When USB-enabled bootloader is used, target application
		 * will not receive this event and it needs to be generated
		 * again here.
		 */
		usb_dc_nrfx_power_event_callback(NRF_POWER_EVENT_USBDETECTED);
	}

	return ret;
}

int usb_dc_detach(void)
{
	struct nrf_usbd_ctx *ctx = get_usbd_ctx();

	k_mutex_lock(&ctx->drv_lock, K_FOREVER);

	usbd_evt_flush();
	eps_ctx_uninit();

	nrfx_usbd_disable();
	nrfx_usbd_uninit();
	(void) hf_clock_enable(false, false);
	nrf5_power_usb_power_int_enable(false);

	ctx->attached = false;
	k_mutex_unlock(&ctx->drv_lock);

	return 0;
}

int usb_dc_reset(void)
{
	int ret;

	if (!dev_attached() || !dev_ready()) {
		return -ENODEV;
	}

	LOG_DBG("USBD Reset.");

	ret = usb_dc_detach();
	if (ret) {
		return ret;
	}

	ret = usb_dc_attach();
	if (ret) {
		return ret;
	}

	return 0;
}

int usb_dc_set_address(const u8_t addr)
{
	struct nrf_usbd_ctx *ctx;

	if (!dev_attached() || !dev_ready()) {
		return -ENODEV;
	}

	/**
	 * Nothing to do here. The USBD HW already takes care of initiating
	 * STATUS stage. Just double check the address for sanity.
	 */
	__ASSERT(addr == (u8_t)NRF_USBD->USBADDR, "USB Address incorrect!");

	ctx = get_usbd_ctx();

	LOG_DBG("Address set to: %d.", addr);

	return 0;
}


int usb_dc_ep_check_cap(const struct usb_dc_ep_cfg_data *const ep_cfg)
{
	u8_t ep_idx = NRF_USBD_EP_NR_GET(ep_cfg->ep_addr);

	LOG_DBG("ep %x, mps %d, type %d", ep_cfg->ep_addr, ep_cfg->ep_mps,
		ep_cfg->ep_type);

	if ((ep_cfg->ep_type == USB_DC_EP_CONTROL) && ep_idx) {
		LOG_ERR("invalid endpoint configuration");
		return -1;
	}

	if (!NRF_USBD_EP_VALIDATE(ep_cfg->ep_addr)) {
		LOG_ERR("invalid endpoint index/address");
		return -1;
	}

	if ((ep_cfg->ep_type == USB_DC_EP_ISOCHRONOUS) &&
	    (!NRF_USBD_EPISO_CHECK(ep_cfg->ep_addr))) {
		LOG_WRN("invalid endpoint type");
		return -1;
	}

	return 0;
}

int usb_dc_ep_configure(const struct usb_dc_ep_cfg_data *const ep_cfg)
{
	struct nrf_usbd_ep_ctx *ep_ctx;

	if (!dev_attached()) {
		return -ENODEV;
	}

	/**
	 * TODO:
	 * For ISO endpoints, application has to use EPIN/OUT 8
	 * but right now there's no standard way of knowing the
	 * ISOIN/ISOOUT endpoint number in advance to configure
	 * accordingly. So either this needs to be chosen in the
	 * menuconfig in application area or perhaps in device tree
	 * at compile time or introduce a new API to read the endpoint
	 * configuration at runtime before configuring them.
	 */
	ep_ctx = endpoint_ctx(ep_cfg->ep_addr);
	if (!ep_ctx) {
		return -EINVAL;
	}

	ep_ctx->cfg.addr = ep_cfg->ep_addr;
	ep_ctx->cfg.type = ep_cfg->ep_type;
	ep_ctx->cfg.max_sz = ep_cfg->ep_mps;

	if ((ep_cfg->ep_mps & (ep_cfg->ep_mps - 1)) != 0U) {
		LOG_ERR("EP max packet size must be a power of 2.");
		return -EINVAL;
	}
	nrfx_usbd_ep_max_packet_size_set(ep_addr_to_nrfx(ep_cfg->ep_addr),
					 ep_cfg->ep_mps);

	return 0;
}

int usb_dc_ep_set_stall(const u8_t ep)
{
	struct nrf_usbd_ep_ctx *ep_ctx;

	if (!dev_attached() || !dev_ready()) {
		return -ENODEV;
	}

	ep_ctx = endpoint_ctx(ep);
	if (!ep_ctx) {
		return -EINVAL;
	}

	switch (ep_ctx->cfg.type) {
	case USB_DC_EP_CONTROL:
		nrfx_usbd_setup_stall();
		break;
	case USB_DC_EP_BULK:
	case USB_DC_EP_INTERRUPT:
		nrfx_usbd_ep_stall(ep_addr_to_nrfx(ep));
		break;
	case USB_DC_EP_ISOCHRONOUS:
		LOG_ERR("STALL unsupported on ISO endpoint.s");
		return -EINVAL;
	}

	ep_ctx->buf.len = 0U;
	ep_ctx->buf.curr = ep_ctx->buf.data;

	LOG_DBG("STALL on EP %d.", ep);

	return 0;
}

int usb_dc_ep_clear_stall(const u8_t ep)
{

	struct nrf_usbd_ep_ctx *ep_ctx;

	if (!dev_attached() || !dev_ready()) {
		return -ENODEV;
	}

	ep_ctx = endpoint_ctx(ep);
	if (!ep_ctx) {
		return -EINVAL;
	}

	nrfx_usbd_ep_stall_clear(ep_addr_to_nrfx(ep));
	LOG_DBG("Unstall on EP %d", ep);

	return 0;
}

int usb_dc_ep_halt(const u8_t ep)
{
	return usb_dc_ep_set_stall(ep);
}

int usb_dc_ep_is_stalled(const u8_t ep, u8_t *const stalled)
{
	struct nrf_usbd_ep_ctx *ep_ctx;

	if (!dev_attached() || !dev_ready()) {
		return -ENODEV;
	}

	ep_ctx = endpoint_ctx(ep);
	if (!ep_ctx) {
		return -EINVAL;
	}

	*stalled = (u8_t) nrfx_usbd_ep_stall_check(ep_addr_to_nrfx(ep));

	return 0;
}

int usb_dc_ep_enable(const u8_t ep)
{
	struct nrf_usbd_ep_ctx *ep_ctx;

	if (!dev_attached()) {
		return -ENODEV;
	}

	ep_ctx = endpoint_ctx(ep);
	if (!ep_ctx) {
		return -EINVAL;
	}

	if (ep_ctx->cfg.en) {
		return -EALREADY;
	}

	LOG_DBG("EP enable: %d.", ep);

	ep_ctx->cfg.en = true;

	/* Defer the endpoint enable if USBD is not ready yet. */
	if (dev_ready()) {
		nrfx_usbd_ep_enable(ep_addr_to_nrfx(ep));
	}

	return 0;
}

int usb_dc_ep_disable(const u8_t ep)
{
	struct nrf_usbd_ep_ctx *ep_ctx;

	if (!dev_attached() || !dev_ready()) {
		return -ENODEV;
	}

	ep_ctx = endpoint_ctx(ep);
	if (!ep_ctx) {
		return -EINVAL;
	}

	if (!ep_ctx->cfg.en) {
		return -EALREADY;
	}

	LOG_DBG("EP disable: %d.", ep);

	nrfx_usbd_ep_disable(ep_addr_to_nrfx(ep));
	ep_ctx->cfg.en = false;

	return 0;
}

int usb_dc_ep_flush(const u8_t ep)
{
	struct nrf_usbd_ep_ctx *ep_ctx;

	if (!dev_attached() || !dev_ready()) {
		return -ENODEV;
	}

	ep_ctx = endpoint_ctx(ep);
	if (!ep_ctx) {
		return -EINVAL;
	}

	ep_ctx->buf.len = 0U;
	ep_ctx->buf.curr = ep_ctx->buf.data;

	nrfx_usbd_transfer_out_drop(ep_addr_to_nrfx(ep));

	return 0;
}

int usb_dc_ep_write(const u8_t ep, const u8_t *const data,
		    const u32_t data_len, u32_t *const ret_bytes)
{
	LOG_DBG("ep_write: ep %d, len %d", ep, data_len);
	struct nrf_usbd_ctx *ctx = get_usbd_ctx();
	struct nrf_usbd_ep_ctx *ep_ctx;
	u32_t bytes_to_copy;

	if (!dev_attached() || !dev_ready()) {
		return -ENODEV;
	}

	if (NRF_USBD_EPOUT_CHECK(ep)) {
		return -EINVAL;
	}

	ep_ctx = endpoint_ctx(ep);
	if (!ep_ctx) {
		return -EINVAL;
	}


	k_mutex_lock(&ctx->drv_lock, K_FOREVER);

	/* USBD driver does not allow scheduling multiple DMA transfers
	 * for one EP at a time. Next USB transfer on this endpoint can be
	 * triggered after the completion of previous one.
	 */
	if (ep_ctx->write_in_progress) {
		k_mutex_unlock(&ctx->drv_lock);
		return -EAGAIN;
	}

	/* NRFX driver performs the fragmentation if buffer length exceeds
	 * maximum packet size, however in current implementation, data is
	 * copied to the internal buffer and must me fragmented here.
	 * In case of fragmentation, a flag is set to prevent triggering
	 * status stage which is handled by hardware, because there will be
	 * another write coming.
	 */
	if (data_len > ep_ctx->cfg.max_sz) {
		bytes_to_copy = ep_ctx->cfg.max_sz;
		ep_ctx->write_fragmented = true;
	} else {
		bytes_to_copy = data_len;
		ep_ctx->write_fragmented = false;
	}
	memcpy(ep_ctx->buf.data, data, bytes_to_copy);
	ep_ctx->buf.len = bytes_to_copy;

	if (ret_bytes) {
		*ret_bytes = bytes_to_copy;
	}

	/* Setup stage is handled by hardware.
	 * Detect the setup stage initiated by the stack
	 * and perform appropriate action.
	 */
	if ((ep_ctx->cfg.type == USB_DC_EP_CONTROL)
	    && (nrfx_usbd_last_setup_dir_get() != ep)) {
		nrfx_usbd_setup_clear();
		k_mutex_unlock(&ctx->drv_lock);
		return 0;
	}

	int result = 0;
	ep_ctx->write_in_progress = true;
	NRFX_USBD_TRANSFER_IN(transfer, ep_ctx->buf.data, ep_ctx->buf.len, 0);
	nrfx_err_t err = nrfx_usbd_ep_transfer(ep_addr_to_nrfx(ep), &transfer);

	if (err != NRFX_SUCCESS) {
		ep_ctx->write_in_progress = false;
		result = -EIO;
		LOG_ERR("nRF USBD write error: %d.", (u32_t)err);
	}

	k_mutex_unlock(&ctx->drv_lock);
	return result;
}

int usb_dc_ep_read_wait(u8_t ep, u8_t *data, u32_t max_data_len,
			u32_t *read_bytes)
{
	struct nrf_usbd_ep_ctx *ep_ctx;
	struct nrf_usbd_ctx *ctx = get_usbd_ctx();
	u32_t bytes_to_copy;

	if (!dev_attached() || !dev_ready()) {
		return -ENODEV;
	}

	if (NRF_USBD_EPIN_CHECK(ep)) {
		return -EINVAL;
	}

	if (!data && max_data_len) {
		return -EINVAL;
	}

	ep_ctx = endpoint_ctx(ep);
	if (!ep_ctx) {
		return -EINVAL;
	}

	k_mutex_lock(&ctx->drv_lock, K_FOREVER);

	bytes_to_copy = MIN(max_data_len, ep_ctx->buf.len);

	if (!data && !max_data_len) {
		if (read_bytes) {
			*read_bytes = ep_ctx->buf.len;
		}
		k_mutex_unlock(&ctx->drv_lock);
		return 0;
	}

	memcpy(data, ep_ctx->buf.curr, bytes_to_copy);

	ep_ctx->buf.curr += bytes_to_copy;
	ep_ctx->buf.len -= bytes_to_copy;
	if (read_bytes) {
		*read_bytes = bytes_to_copy;
	}

	k_mutex_unlock(&ctx->drv_lock);
	return 0;
}

int usb_dc_ep_read_continue(u8_t ep)
{
	struct nrf_usbd_ep_ctx *ep_ctx;
	struct nrf_usbd_ctx *ctx = get_usbd_ctx();

	if (!dev_attached() || !dev_ready()) {
		return -ENODEV;
	}

	if (NRF_USBD_EPIN_CHECK(ep)) {
		return -EINVAL;
	}

	ep_ctx = endpoint_ctx(ep);
	if (!ep_ctx) {
		return -EINVAL;
	}

	k_mutex_lock(&ctx->drv_lock, K_FOREVER);
	if (!ep_ctx->buf.len) {
		ep_ctx->buf.curr = ep_ctx->buf.data;
		ep_ctx->read_complete = true;

		if (ep_ctx->read_pending) {
			struct usbd_event *ev = usbd_evt_alloc();

			if (!ev) {
				return -ENOMEM;
			}

			ev->evt_type = USBD_EVT_EP;
			ev->evt.ep_evt.ep = ep_ctx;
			ev->evt.ep_evt.evt_type = EP_EVT_RECV_REQ;
			usbd_evt_put(ev);
			usbd_work_schedule();
		}
	}
	k_mutex_unlock(&ctx->drv_lock);

	return 0;
}

int usb_dc_ep_read(const u8_t ep, u8_t *const data,
		   const u32_t max_data_len, u32_t *const read_bytes)
{
	LOG_DBG("ep_read: ep %d, maxlen %d", ep, max_data_len);
	int ret;

	ret = usb_dc_ep_read_wait(ep, data, max_data_len, read_bytes);
	if (ret) {
		return ret;
	}

	if (!data && !max_data_len) {
		return ret;
	}

	ret = usb_dc_ep_read_continue(ep);
	return ret;
}

int usb_dc_ep_set_callback(const u8_t ep, const usb_dc_ep_callback cb)
{
	struct nrf_usbd_ep_ctx *ep_ctx;

	if (!dev_attached()) {
		return -ENODEV;
	}

	ep_ctx = endpoint_ctx(ep);
	if (!ep_ctx) {
		return -EINVAL;
	}

	ep_ctx->cfg.cb = cb;

	return 0;
}

void usb_dc_set_status_callback(const usb_dc_status_callback cb)
{
	get_usbd_ctx()->status_cb = cb;
}

int usb_dc_ep_mps(const u8_t ep)
{
	struct nrf_usbd_ep_ctx *ep_ctx;

	if (!dev_attached()) {
		return -ENODEV;
	}

	ep_ctx = endpoint_ctx(ep);
	if (!ep_ctx) {
		return -EINVAL;
	}

	return ep_ctx->cfg.max_sz;
}

int usb_dc_wakeup_request(void)
{
	bool res = nrfx_usbd_wakeup_req();

	if (!res) {
		return -EAGAIN;
	}
	return 0;
}
