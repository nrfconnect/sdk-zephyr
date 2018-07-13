/* USB device controller driver for STM32 devices */

/*
 * Copyright (c) 2017 Christer Weinigel.
 * Copyright (c) 2017, I-SENSE group of ICCS
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief USB device controller driver for STM32 devices
 *
 * This driver uses the STM32 Cube low level drivers to talk to the USB
 * device controller on the STM32 family of devices using the
 * STM32Cube HAL layer.
 *
 * There is a bit of an impedance mismatch between the Zephyr
 * usb_device and the STM32 Cube HAL layer where higher levels make
 * assumptions about the low level drivers that don't quite match how
 * the low level drivers actually work.
 *
 * The usb_dc_ep_read function expects to get the data it wants
 * immediately while the HAL_PCD_EP_Receive function only starts a
 * read transaction and the data won't be available until call to
 * HAL_PCD_DataOutStageCallback. To work around this I've
 * had to add an extra packet buffer in the driver which wastes memory
 * and also leads to an extra copy of all received data.  It would be
 * better if higher drivers could call start_read and get_read_count
 * in this driver directly.
 *
 * To enable the driver together with the CDC_ACM high level driver,
 * add the following to your board's defconfig:
 *
 * CONFIG_USB=y
 * CONFIG_USB_DC_STM32=y
 * CONFIG_USB_CDC_ACM=y
 * CONFIG_USB_DEVICE_STACK=y
 *
 * To use the USB device as a console, also add:
 *
 * CONFIG_UART_CONSOLE_ON_DEV_NAME="CDC_ACM"
 * CONFIG_USB_UART_CONSOLE=y
 * CONFIG_UART_LINE_CTRL=y
 */

#include <soc.h>
#include <string.h>
#include <usb/usb_dc.h>
#include <usb/usb_device.h>
#include <clock_control/stm32_clock_control.h>
#include <misc/util.h>
#include <gpio.h>

#define SYS_LOG_LEVEL CONFIG_SYS_LOG_USB_DRIVER_LEVEL
#include <logging/sys_log.h>

/*
 * USB LL API provides the EP_TYPE_* defines. STM32Cube does not
 * provide USB LL API for STM32F0, STM32F3 and STM32L0 families.
 * Map EP_TYPE_* defines to PCD_EP_TYPE_* defines
 */
#if defined(CONFIG_SOC_SERIES_STM32F3X) || \
	defined(CONFIG_SOC_SERIES_STM32F0X) || \
	defined(CONFIG_SOC_SERIES_STM32L0X)
#define EP_TYPE_CTRL PCD_EP_TYPE_CTRL
#define EP_TYPE_ISOC PCD_EP_TYPE_ISOC
#define EP_TYPE_BULK PCD_EP_TYPE_BULK
#define EP_TYPE_INTR PCD_EP_TYPE_INTR
#endif

/*
 * USB and USB_OTG_FS are defined in STM32Cube HAL and allows to distinguish
 * between two kind of USB DC. STM32 F0, F3, and L0 series support USB device
 * controller. STM32 F4 and F7 series support USB_OTG_FS device controller.
 * STM32 F1 and L4 series support either USB or USB_OTG_FS device controller.
 *
 * WARNING: Don't mix USB defined in STM32Cube HAL and CONFIG_USB from Zephyr
 * Kconfig system.
 */
#ifdef USB

#define EP0_MPS 64U
#define EP_MPS 64U

/*
 * USB BTABLE is stored in the PMA. The size of BTABLE is 4 bytes
 * per endpoint.
 *
 */
#define USB_BTABLE_SIZE  (8 * CONFIG_USB_NUM_BIDIR_ENDPOINTS)

#else /* USB_OTG_FS */

#define EP0_MPS USB_OTG_MAX_EP0_SIZE
#define EP_MPS USB_OTG_FS_MAX_PACKET_SIZE

/* We need one RX FIFO and n TX-IN FIFOs */
#define FIFO_NUM (1 + CONFIG_USB_NUM_BIDIR_ENDPOINTS)

/* 4-byte words FIFO */
#define FIFO_WORDS (CONFIG_USB_RAM_SIZE / 4)

/* Allocate FIFO memory evenly between the FIFOs */
#define FIFO_EP_WORDS (FIFO_WORDS / FIFO_NUM)

#endif /* USB */

/* Size of a USB SETUP packet */
#define SETUP_SIZE 8

/* Helper macros to make it easier to work with endpoint numbers */
#define EP0_IDX 0
#define EP0_IN (EP0_IDX | USB_EP_DIR_IN)
#define EP0_OUT (EP0_IDX | USB_EP_DIR_OUT)

#define EP_IDX(ep) ((ep) & ~USB_EP_DIR_MASK)
#define EP_IS_IN(ep) (((ep) & USB_EP_DIR_MASK) == USB_EP_DIR_IN)
#define EP_IS_OUT(ep) (((ep) & USB_EP_DIR_MASK) == USB_EP_DIR_OUT)

/* Endpoint state */
struct usb_dc_stm32_ep_state {
	u16_t ep_mps;	/** Endpoint max packet size */
	u8_t ep_type;	/** Endpoint type (STM32 HAL enum) */
	usb_dc_ep_callback cb;	/** Endpoint callback function */
	u8_t ep_stalled;	/** Endpoint stall flag */
	u32_t read_count;	/** Number of bytes in read buffer  */
	u32_t read_offset;	/** Current offset in read buffer */
	struct k_sem write_sem;	/** Write boolean semaphore */
};

/* Driver state */
struct usb_dc_stm32_state {
	PCD_HandleTypeDef pcd;	/* Storage for the HAL_PCD api */
	usb_dc_status_callback status_cb; /* Status callback */
	struct usb_dc_stm32_ep_state out_ep_state[CONFIG_USB_NUM_BIDIR_ENDPOINTS];
	struct usb_dc_stm32_ep_state in_ep_state[CONFIG_USB_NUM_BIDIR_ENDPOINTS];
	u8_t ep_buf[CONFIG_USB_NUM_BIDIR_ENDPOINTS][EP_MPS];

#ifdef USB
	u32_t pma_offset;
#endif /* USB */
};

static struct usb_dc_stm32_state usb_dc_stm32_state;

/* Internal functions */

static struct usb_dc_stm32_ep_state *usb_dc_stm32_get_ep_state(u8_t ep)
{
	struct usb_dc_stm32_ep_state *ep_state_base;

	if (EP_IDX(ep) >= CONFIG_USB_NUM_BIDIR_ENDPOINTS) {
		return NULL;
	}

	if (EP_IS_OUT(ep)) {
		ep_state_base = usb_dc_stm32_state.out_ep_state;
	} else {
		ep_state_base = usb_dc_stm32_state.in_ep_state;
	}

	return ep_state_base + EP_IDX(ep);
}

static void usb_dc_stm32_isr(void *arg)
{
	HAL_PCD_IRQHandler(&usb_dc_stm32_state.pcd);
}

static int usb_dc_stm32_clock_enable(void)
{
	struct device *clk = device_get_binding(STM32_CLOCK_CONTROL_NAME);
	struct stm32_pclken pclken = {

#ifdef USB
		.bus = STM32_CLOCK_BUS_APB1,
		.enr = LL_APB1_GRP1_PERIPH_USB,
#else /* USB_OTG_FS */

#ifdef CONFIG_SOC_SERIES_STM32F1X
		.bus = STM32_CLOCK_BUS_AHB1,
		.enr = LL_AHB1_GRP1_PERIPH_OTGFS,
#else
		.bus = STM32_CLOCK_BUS_AHB2,
		.enr = LL_AHB2_GRP1_PERIPH_OTGFS,
#endif /* CONFIG_SOC_SERIES_STM32F1X */

#endif /* USB */
	};

	/*
	 * Some SoCs in STM32F0/L0/L4 series disable USB clock by
	 * default.  We force USB clock source to PLL clock for this
	 * SoCs.  However, if these parts have an HSI48 clock, use
	 * that instead.  Example reference manual RM0360 for
	 * STM32F030x4/x6/x8/xC and STM32F070x6/xB.
	 */
#if defined(RCC_HSI48_SUPPORT)

	/*
	 * In STM32L0 series, HSI48 requires VREFINT and its buffer
	 * with 48 MHz RC to be enabled.
	 * See ENREF_HSI48 in referenc maual RM0367 section10.2.3:
	 * "Reference control and status register (SYSCFG_CFGR3)"
	 */
#ifdef CONFIG_SOC_SERIES_STM32L0X
	if (LL_APB2_GRP1_IsEnabledClock(LL_APB2_GRP1_PERIPH_SYSCFG)) {
		LL_SYSCFG_VREFINT_EnableHSI48();
	} else {
		SYS_LOG_ERR("System Configuration Controller clock is "
			    "disabled. Unable to enable VREFINT which "
			    "is required by HSI48.");
	}
#endif /* CONFIG_SOC_SERIES_STM32L0X */

	LL_RCC_HSI48_Enable();
	while (!LL_RCC_HSI48_IsReady()) {
		/* Wait for HSI48 to become ready */
	}

	LL_RCC_SetUSBClockSource(LL_RCC_USB_CLKSOURCE_HSI48);
#elif defined(LL_RCC_USB_CLKSOURCE_NONE)
	if (LL_RCC_PLL_IsReady()) {
		LL_RCC_SetUSBClockSource(LL_RCC_USB_CLKSOURCE_PLL);
	} else {
		SYS_LOG_ERR("Unable to set USB clock source to PLL.");
	}
#endif /* RCC_HSI48_SUPPORT / LL_RCC_USB_CLKSOURCE_NONE */

	clock_control_on(clk, (clock_control_subsys_t *)&pclken);

	return 0;
}

static int usb_dc_stm32_init(void)
{
	HAL_StatusTypeDef status;
	unsigned int i;

#ifdef USB
	usb_dc_stm32_state.pcd.Instance = USB;
	usb_dc_stm32_state.pcd.Init.speed = PCD_SPEED_FULL;
	usb_dc_stm32_state.pcd.Init.dev_endpoints = CONFIG_USB_NUM_BIDIR_ENDPOINTS;
	usb_dc_stm32_state.pcd.Init.phy_itface = PCD_PHY_EMBEDDED;
	usb_dc_stm32_state.pcd.Init.ep0_mps = PCD_EP0MPS_64;
	usb_dc_stm32_state.pcd.Init.low_power_enable = 0;
#else /* USB_OTG_FS */
	usb_dc_stm32_state.pcd.Instance = USB_OTG_FS;
	usb_dc_stm32_state.pcd.Init.dev_endpoints = CONFIG_USB_NUM_BIDIR_ENDPOINTS;
	usb_dc_stm32_state.pcd.Init.speed = USB_OTG_SPEED_FULL;
	usb_dc_stm32_state.pcd.Init.phy_itface = PCD_PHY_EMBEDDED;
	usb_dc_stm32_state.pcd.Init.ep0_mps = USB_OTG_MAX_EP0_SIZE;
	usb_dc_stm32_state.pcd.Init.vbus_sensing_enable = DISABLE;

#ifndef CONFIG_SOC_SERIES_STM32F1X
	usb_dc_stm32_state.pcd.Init.dma_enable = DISABLE;
#endif

#endif /* USB */

	SYS_LOG_DBG("HAL_PCD_Init");
	status = HAL_PCD_Init(&usb_dc_stm32_state.pcd);
	if (status != HAL_OK) {
		SYS_LOG_ERR("PCD_Init failed, %d", (int)status);
		return -EIO;
	}

	SYS_LOG_DBG("HAL_PCD_Start");
	status = HAL_PCD_Start(&usb_dc_stm32_state.pcd);
	if (status != HAL_OK) {
		SYS_LOG_ERR("PCD_Start failed, %d", (int)status);
		return -EIO;
	}

	usb_dc_stm32_state.out_ep_state[EP0_IDX].ep_mps = EP0_MPS;
	usb_dc_stm32_state.out_ep_state[EP0_IDX].ep_type = EP_TYPE_CTRL;
	usb_dc_stm32_state.in_ep_state[EP0_IDX].ep_mps = EP0_MPS;
	usb_dc_stm32_state.in_ep_state[EP0_IDX].ep_type = EP_TYPE_CTRL;

#ifdef USB
	/* Start PMA configuration for the endpoints after the BTABLE. */
	usb_dc_stm32_state.pma_offset = USB_BTABLE_SIZE;

	for (i = 0; i < CONFIG_USB_NUM_BIDIR_ENDPOINTS; i++) {
		k_sem_init(&usb_dc_stm32_state.in_ep_state[i].write_sem, 1, 1);
	}
#else /* USB_OTG_FS */
	/* TODO: make this dynamic (depending usage) */
	HAL_PCDEx_SetRxFiFo(&usb_dc_stm32_state.pcd, FIFO_EP_WORDS);
	for (i = 0; i < CONFIG_USB_NUM_BIDIR_ENDPOINTS; i++) {
		HAL_PCDEx_SetTxFiFo(&usb_dc_stm32_state.pcd, i,
				    FIFO_EP_WORDS);
		k_sem_init(&usb_dc_stm32_state.in_ep_state[i].write_sem, 1, 1);
	}
#endif /* USB */

	IRQ_CONNECT(CONFIG_USB_IRQ, CONFIG_USB_IRQ_PRI,
		    usb_dc_stm32_isr, 0, 0);
	irq_enable(CONFIG_USB_IRQ);
	return 0;
}

/* Zephyr USB device controller API implementation */

int usb_dc_attach(void)
{
	int ret;

	SYS_LOG_DBG("");

	/*
	 * For STM32F0 series SoCs on QFN28 and TSSOP20 packages enable PIN
	 * pair PA11/12 mapped instead of PA9/10 (e.g. stm32f070x6)
	 */
#if defined(CONFIG_SOC_SERIES_STM32F0X) && defined(SYSCFG_CFGR1_PA11_PA12_RMP)
	if (LL_APB1_GRP2_IsEnabledClock(LL_APB1_GRP2_PERIPH_SYSCFG)) {
		LL_SYSCFG_EnablePinRemap();
	} else {
		SYS_LOG_ERR("System Configuration Controller clock is "
			    "disable. Unable to enable pin remapping."
	}
#endif

	ret = usb_dc_stm32_clock_enable();
	if (ret) {
		return ret;
	}

	ret = usb_dc_stm32_init();
	if (ret) {
		return ret;
	}

	/*
	 * Required for at least STM32L4 devices as they electrically
	 * isolate USB features from VDDUSB. It must be enabled before
	 * USB can function. Refer to section 5.1.3 in DM00083560 or
	 * DM00310109.
	 */
#ifdef PWR_CR2_USV
	if (LL_APB1_GRP1_IsEnabledClock(LL_APB1_GRP1_PERIPH_PWR)) {
		LL_PWR_EnableVddUSB();
	} else {
		LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_PWR);
		LL_PWR_EnableVddUSB();
		LL_APB1_GRP1_DisableClock(LL_APB1_GRP1_PERIPH_PWR);
	}
#endif /* PWR_CR2_USV */

	return 0;
}

int usb_dc_ep_set_callback(const u8_t ep, const usb_dc_ep_callback cb)
{
	struct usb_dc_stm32_ep_state *ep_state = usb_dc_stm32_get_ep_state(ep);

	SYS_LOG_DBG("ep 0x%02x", ep);

	if (!ep_state) {
		return -EINVAL;
	}

	ep_state->cb = cb;

	return 0;
}

int usb_dc_set_status_callback(const usb_dc_status_callback cb)
{
	SYS_LOG_DBG("");

	usb_dc_stm32_state.status_cb = cb;

	return 0;
}

int usb_dc_set_address(const u8_t addr)
{
	HAL_StatusTypeDef status;

	SYS_LOG_DBG("addr %u (0x%02x)", addr, addr);

	status = HAL_PCD_SetAddress(&usb_dc_stm32_state.pcd, addr);
	if (status != HAL_OK) {
		SYS_LOG_ERR("HAL_PCD_SetAddress failed(0x%02x), %d",
			    addr, (int)status);
		return -EIO;
	}

	return 0;
}

int usb_dc_ep_start_read(u8_t ep, u8_t *data, u32_t max_data_len)
{
	HAL_StatusTypeDef status;

	SYS_LOG_DBG("ep 0x%02x, len %u", ep, max_data_len);

	/* we flush EP0_IN by doing a 0 length receive on it */
	if (!EP_IS_OUT(ep) && (ep != EP0_IN || max_data_len)) {
		SYS_LOG_ERR("invalid ep 0x%02x", ep);
		return -EINVAL;
	}

	if (max_data_len > EP_MPS) {
		max_data_len = EP_MPS;
	}

	status = HAL_PCD_EP_Receive(&usb_dc_stm32_state.pcd, ep,
				    usb_dc_stm32_state.ep_buf[EP_IDX(ep)],
				    max_data_len);
	if (status != HAL_OK) {
		SYS_LOG_ERR("HAL_PCD_EP_Receive failed(0x%02x), %d",
			    ep, (int)status);
		return -EIO;
	}

	return 0;
}

int usb_dc_ep_get_read_count(u8_t ep, u32_t *read_bytes)
{
	if (!EP_IS_OUT(ep)) {
		SYS_LOG_ERR("invalid ep 0x%02x", ep);
		return -EINVAL;
	}

	*read_bytes = HAL_PCD_EP_GetRxCount(&usb_dc_stm32_state.pcd, ep);

	return 0;
}

int usb_dc_ep_check_cap(const struct usb_dc_ep_cfg_data * const cfg)
{
	u8_t ep_idx = EP_IDX(cfg->ep_addr);

	SYS_LOG_DBG("ep %x, mps %d, type %d", cfg->ep_addr, cfg->ep_mps,
		    cfg->ep_type);

	if ((cfg->ep_type == USB_DC_EP_CONTROL) && ep_idx) {
		SYS_LOG_ERR("invalid endpoint configuration");
		return -1;
	}

	if (ep_idx > CONFIG_USB_NUM_BIDIR_ENDPOINTS) {
		SYS_LOG_ERR("endpoint index/address out of range");
		return -1;
	}

	return 0;
}

int usb_dc_ep_configure(const struct usb_dc_ep_cfg_data * const ep_cfg)
{
	u8_t ep = ep_cfg->ep_addr;
	struct usb_dc_stm32_ep_state *ep_state = usb_dc_stm32_get_ep_state(ep);

	SYS_LOG_DBG("ep 0x%02x, ep_mps %u, ep_type %u",
		    ep_cfg->ep_addr, ep_cfg->ep_mps, ep_cfg->ep_type);

	if (!ep_state) {
		return -EINVAL;
	}

#ifdef USB
	if (CONFIG_USB_RAM_SIZE <=
	    (usb_dc_stm32_state.pma_offset + ep_cfg->ep_mps)) {
		return -EINVAL;
	}
	HAL_PCDEx_PMAConfig(&usb_dc_stm32_state.pcd, ep, PCD_SNG_BUF,
			    usb_dc_stm32_state.pma_offset);
	usb_dc_stm32_state.pma_offset += ep_cfg->ep_mps;
#endif
	ep_state->ep_mps = ep_cfg->ep_mps;

	switch (ep_cfg->ep_type) {
	case USB_DC_EP_CONTROL:
		ep_state->ep_type = EP_TYPE_CTRL;
		break;
	case USB_DC_EP_ISOCHRONOUS:
		ep_state->ep_type = EP_TYPE_ISOC;
		break;
	case USB_DC_EP_BULK:
		ep_state->ep_type = EP_TYPE_BULK;
		break;
	case USB_DC_EP_INTERRUPT:
		ep_state->ep_type = EP_TYPE_INTR;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

int usb_dc_ep_set_stall(const u8_t ep)
{
	struct usb_dc_stm32_ep_state *ep_state = usb_dc_stm32_get_ep_state(ep);
	HAL_StatusTypeDef status;

	SYS_LOG_DBG("ep 0x%02x", ep);

	if (!ep_state) {
		return -EINVAL;
	}

	status = HAL_PCD_EP_SetStall(&usb_dc_stm32_state.pcd, ep);
	if (status != HAL_OK) {
		SYS_LOG_ERR("HAL_PCD_EP_SetStall failed(0x%02x), %d",
			    ep, (int)status);
		return -EIO;
	}

	ep_state->ep_stalled = 1;

	return 0;
}

int usb_dc_ep_clear_stall(const u8_t ep)
{
	struct usb_dc_stm32_ep_state *ep_state = usb_dc_stm32_get_ep_state(ep);
	HAL_StatusTypeDef status;

	SYS_LOG_DBG("ep 0x%02x", ep);

	if (!ep_state) {
		return -EINVAL;
	}

	status = HAL_PCD_EP_ClrStall(&usb_dc_stm32_state.pcd, ep);
	if (status != HAL_OK) {
		SYS_LOG_ERR("HAL_PCD_EP_ClrStall failed(0x%02x), %d",
			    ep, (int)status);
		return -EIO;
	}

	ep_state->ep_stalled = 0;
	ep_state->read_count = 0;

	return 0;
}

int usb_dc_ep_is_stalled(const u8_t ep, u8_t *const stalled)
{
	struct usb_dc_stm32_ep_state *ep_state = usb_dc_stm32_get_ep_state(ep);

	SYS_LOG_DBG("ep 0x%02x", ep);

	if (!ep_state) {
		return -EINVAL;
	}

	*stalled = ep_state->ep_stalled;

	return 0;
}

int usb_dc_ep_enable(const u8_t ep)
{
	struct usb_dc_stm32_ep_state *ep_state = usb_dc_stm32_get_ep_state(ep);
	HAL_StatusTypeDef status;

	SYS_LOG_DBG("ep 0x%02x", ep);

	if (!ep_state) {
		return -EINVAL;
	}

	SYS_LOG_DBG("HAL_PCD_EP_Open(0x%02x, %u, %u)", ep,
		    ep_state->ep_mps, ep_state->ep_type);

	status = HAL_PCD_EP_Open(&usb_dc_stm32_state.pcd, ep,
				 ep_state->ep_mps, ep_state->ep_type);
	if (status != HAL_OK) {
		SYS_LOG_ERR("HAL_PCD_EP_Open failed(0x%02x), %d",
			    ep, (int)status);
		return -EIO;
	}

	if (EP_IS_OUT(ep) && ep != EP0_OUT) {
		return usb_dc_ep_start_read(ep,
					  usb_dc_stm32_state.ep_buf[EP_IDX(ep)],
					  EP_MPS);
	}

	return 0;
}

int usb_dc_ep_disable(const u8_t ep)
{
	struct usb_dc_stm32_ep_state *ep_state = usb_dc_stm32_get_ep_state(ep);
	HAL_StatusTypeDef status;

	SYS_LOG_DBG("ep 0x%02x", ep);

	if (!ep_state) {
		return -EINVAL;
	}

	status = HAL_PCD_EP_Close(&usb_dc_stm32_state.pcd, ep);
	if (status != HAL_OK) {
		SYS_LOG_ERR("HAL_PCD_EP_Close failed(0x%02x), %d",
			    ep, (int)status);
		return -EIO;
	}

	return 0;
}

int usb_dc_ep_write(const u8_t ep, const u8_t *const data,
		    const u32_t data_len, u32_t * const ret_bytes)
{
	struct usb_dc_stm32_ep_state *ep_state = usb_dc_stm32_get_ep_state(ep);
	HAL_StatusTypeDef status;
	int ret = 0;

	SYS_LOG_DBG("ep 0x%02x, len %u", ep, data_len);

	if (!EP_IS_IN(ep)) {
		SYS_LOG_ERR("invalid ep 0x%02x", ep);
		return -EINVAL;
	}

	ret = k_sem_take(&ep_state->write_sem, K_NO_WAIT);
	if (ret) {
		SYS_LOG_ERR("Unable to write ep 0x%02x (%d)", ep, ret);
		return ret;
	}

	if (!k_is_in_isr()) {
		irq_disable(CONFIG_USB_IRQ);
	}

	status = HAL_PCD_EP_Transmit(&usb_dc_stm32_state.pcd, ep,
				     (void *)data, data_len);
	if (status != HAL_OK) {
		SYS_LOG_ERR("HAL_PCD_EP_Transmit failed(0x%02x), %d",
			    ep, (int)status);
		k_sem_give(&ep_state->write_sem);
		ret = -EIO;
	}

	if (!ret && ep == EP0_IN && data_len > 0) {
		/* Wait for an empty package as from the host.
		 * This also flushes the TX FIFO to the host.
		 */
		usb_dc_ep_start_read(ep, NULL, 0);
	}

	if (!k_is_in_isr()) {
		irq_enable(CONFIG_USB_IRQ);
	}

	if (ret_bytes) {
		*ret_bytes = data_len;
	}

	return ret;
}

int usb_dc_ep_read_wait(u8_t ep, u8_t *data, u32_t max_data_len,
			u32_t *read_bytes)
{
	struct usb_dc_stm32_ep_state *ep_state = usb_dc_stm32_get_ep_state(ep);
	u32_t read_count = ep_state->read_count;

	SYS_LOG_DBG("ep 0x%02x, %u bytes, %u+%u, %p", ep,
		    max_data_len, ep_state->read_offset, read_count, data);

	if (!EP_IS_OUT(ep)) { /* check if OUT ep */
		SYS_LOG_ERR("Wrong endpoint direction: 0x%02x", ep);
		return -EINVAL;
	}

	/* When both buffer and max data to read are zero, just ingore reading
	 * and return available data in buffer. Otherwise, return data
	 * previously stored in the buffer.
	 */
	if (data) {
		read_count = min(read_count, max_data_len);
		memcpy(data, usb_dc_stm32_state.ep_buf[EP_IDX(ep)] +
		       ep_state->read_offset, read_count);
		ep_state->read_count -= read_count;
		ep_state->read_offset += read_count;
	} else if (max_data_len) {
		SYS_LOG_ERR("Wrong arguments");
	}

	if (read_bytes) {
		*read_bytes = read_count;
	}

	return 0;
}

int usb_dc_ep_read_continue(u8_t ep)
{
	struct usb_dc_stm32_ep_state *ep_state = usb_dc_stm32_get_ep_state(ep);

	if (!EP_IS_OUT(ep)) { /* Check if OUT ep */
		SYS_LOG_ERR("Not valid endpoint: %02x", ep);
		return -EINVAL;
	}

	/* If no more data in the buffer, start a new read transaction.
	 * DataOutStageCallback will called on transaction complete.
	 */
	if (ep != EP0_OUT && !ep_state->read_count) {
		usb_dc_ep_start_read(ep, usb_dc_stm32_state.ep_buf[EP_IDX(ep)],
				     EP_MPS);
	}

	return 0;
}

int usb_dc_ep_read(const u8_t ep, u8_t *const data, const u32_t max_data_len,
		   u32_t * const read_bytes)
{
	if (usb_dc_ep_read_wait(ep, data, max_data_len, read_bytes) != 0) {
		return -EINVAL;
	}

	if (usb_dc_ep_read_continue(ep) != 0) {
		return -EINVAL;
	}

	return 0;
}


int usb_dc_ep_mps(const u8_t ep)
{
	struct usb_dc_stm32_ep_state *ep_state = usb_dc_stm32_get_ep_state(ep);

	return ep_state->ep_mps;
}

/* Callbacks from the STM32 Cube HAL code */

void HAL_PCD_ResetCallback(PCD_HandleTypeDef *hpcd)
{
	SYS_LOG_DBG("");

	HAL_PCD_EP_Open(&usb_dc_stm32_state.pcd, EP0_IN, EP0_MPS, EP_TYPE_CTRL);
	HAL_PCD_EP_Open(&usb_dc_stm32_state.pcd, EP0_OUT, EP0_MPS,
			EP_TYPE_CTRL);

	if (usb_dc_stm32_state.status_cb) {
		usb_dc_stm32_state.status_cb(USB_DC_RESET, NULL);
	}
}

void HAL_PCD_ConnectCallback(PCD_HandleTypeDef *hpcd)
{
	SYS_LOG_DBG("");

	if (usb_dc_stm32_state.status_cb) {
		usb_dc_stm32_state.status_cb(USB_DC_CONNECTED, NULL);
	}
}

void HAL_PCD_DisconnectCallback(PCD_HandleTypeDef *hpcd)
{
	SYS_LOG_DBG("");

	if (usb_dc_stm32_state.status_cb) {
		usb_dc_stm32_state.status_cb(USB_DC_DISCONNECTED, NULL);
	}
}

void HAL_PCD_SuspendCallback(PCD_HandleTypeDef *hpcd)
{
	SYS_LOG_DBG("");

	if (usb_dc_stm32_state.status_cb) {
		usb_dc_stm32_state.status_cb(USB_DC_SUSPEND, NULL);
	}
}

void HAL_PCD_ResumeCallback(PCD_HandleTypeDef *hpcd)
{
	SYS_LOG_DBG("");

	if (usb_dc_stm32_state.status_cb) {
		usb_dc_stm32_state.status_cb(USB_DC_RESUME, NULL);
	}
}

void HAL_PCD_SetupStageCallback(PCD_HandleTypeDef *hpcd)
{
	struct usb_setup_packet *setup = (void *)usb_dc_stm32_state.pcd.Setup;
	struct usb_dc_stm32_ep_state *ep_state;

	SYS_LOG_DBG("");

	ep_state = usb_dc_stm32_get_ep_state(EP0_OUT); /* can't fail for ep0 */
	ep_state->read_count = SETUP_SIZE;
	ep_state->read_offset = 0;
	memcpy(&usb_dc_stm32_state.ep_buf[EP0_IDX],
	       usb_dc_stm32_state.pcd.Setup, ep_state->read_count);

	if (ep_state->cb) {
		ep_state->cb(EP0_OUT, USB_DC_EP_SETUP);

		if (!(setup->wLength == 0) &&
		    !(REQTYPE_GET_DIR(setup->bmRequestType) ==
		    REQTYPE_DIR_TO_HOST)) {
			usb_dc_ep_start_read(EP0_OUT,
					     usb_dc_stm32_state.ep_buf[EP0_IDX],
					     setup->wLength);
		}
	}
}

void HAL_PCD_DataOutStageCallback(PCD_HandleTypeDef *hpcd, u8_t epnum)
{
	u8_t ep_idx = EP_IDX(epnum);
	u8_t ep = ep_idx | USB_EP_DIR_OUT;
	struct usb_dc_stm32_ep_state *ep_state = usb_dc_stm32_get_ep_state(ep);

	SYS_LOG_DBG("epnum 0x%02x, rx_count %u", epnum,
		    HAL_PCD_EP_GetRxCount(&usb_dc_stm32_state.pcd, epnum));

	/* Transaction complete, data is now stored in the buffer and ready
	 * for the upper stack (usb_dc_ep_read to retrieve).
	 */
	usb_dc_ep_get_read_count(ep, &ep_state->read_count);
	ep_state->read_offset = 0;

	if (ep_state->cb) {
		ep_state->cb(ep, USB_DC_EP_DATA_OUT);
	}
}

void HAL_PCD_DataInStageCallback(PCD_HandleTypeDef *hpcd, u8_t epnum)
{
	u8_t ep_idx = EP_IDX(epnum);
	u8_t ep = ep_idx | USB_EP_DIR_IN;
	struct usb_dc_stm32_ep_state *ep_state = usb_dc_stm32_get_ep_state(ep);

	SYS_LOG_DBG("epnum 0x%02x", epnum);

	k_sem_give(&ep_state->write_sem);

	if (ep_state->cb) {
		ep_state->cb(ep, USB_DC_EP_DATA_IN);
	}
}

#if defined(USB) && defined(CONFIG_USB_DC_STM32_DISCONN_ENABLE)
void HAL_PCDEx_SetConnectionState(PCD_HandleTypeDef *hpcd, uint8_t state)
{
	struct device *usb_disconnect;

	usb_disconnect = device_get_binding(
				CONFIG_USB_DC_STM32_DISCONN_GPIO_PORT_NAME);
	gpio_pin_configure(usb_disconnect,
			   CONFIG_USB_DC_STM32_DISCONN_PIN, GPIO_DIR_OUT);

	if (state) {
		gpio_pin_write(usb_disconnect,
			       CONFIG_USB_DC_STM32_DISCONN_PIN,
			       CONFIG_USB_DC_STM32_DISCONN_PIN_LEVEL);
	} else {
		gpio_pin_write(usb_disconnect,
			       CONFIG_USB_DC_STM32_DISCONN_PIN,
			       !CONFIG_USB_DC_STM32_DISCONN_PIN_LEVEL);
	}
}
#endif /* USB && CONFIG_USB_DC_STM32_DISCONN_ENABLE */
