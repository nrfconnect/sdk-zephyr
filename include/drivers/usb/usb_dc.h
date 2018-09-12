/* usb_dc.h - USB device controller driver interface */

/*
 * Copyright (c) 2016 Intel Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief USB device controller APIs
 *
 * This file contains the USB device controller APIs. All device controller
 * drivers should implement the APIs described in this file.
 */

#ifndef __USB_DC_H__
#define __USB_DC_H__

#include <device.h>

/**
 * USB endpoint direction and number.
 */

#define USB_EP_DIR_MASK		0x80
#define USB_EP_DIR_IN		0x80
#define USB_EP_DIR_OUT		0x00

/**
 * @brief USB Device Controller API
 * @defgroup _usb_device_controller_api USB Device Controller API
 * @{
 */

/**
 * @brief USB Driver Status Codes
 *
 * Status codes reported by the registered device status callback.
 */
enum usb_dc_status_code {
	/** USB error reported by the controller */
	USB_DC_ERROR,
	/** USB reset */
	USB_DC_RESET,
	/** USB connection established, hardware enumeration is completed */
	USB_DC_CONNECTED,
	/** USB configuration done */
	USB_DC_CONFIGURED,
	/** USB connection lost */
	USB_DC_DISCONNECTED,
	/** USB connection suspended by the HOST */
	USB_DC_SUSPEND,
	/** USB connection resumed by the HOST */
	USB_DC_RESUME,
	/** USB interface selected */
	USB_DC_INTERFACE,
	/** Set Feature ENDPOINT_HALT received */
	USB_DC_SET_HALT,
	/** Clear Feature ENDPOINT_HALT received */
	USB_DC_CLEAR_HALT,
	/** Initial USB connection status */
	USB_DC_UNKNOWN
};

/**
 * @brief USB Endpoint Callback Status Codes
 *
 * Status Codes reported by the registered endpoint callback.
 */
enum usb_dc_ep_cb_status_code {
	/** SETUP received */
	USB_DC_EP_SETUP,
	/** Out transaction on this EP, data is available for read */
	USB_DC_EP_DATA_OUT,
	/** In transaction done on this EP */
	USB_DC_EP_DATA_IN
};

/**
 * @brief USB Endpoint type
 */
enum usb_dc_ep_type {
	/** Control type endpoint */
	USB_DC_EP_CONTROL = 0,
	/** Isochronous type endpoint */
	USB_DC_EP_ISOCHRONOUS,
	/** Bulk type endpoint */
	USB_DC_EP_BULK,
	/** Interrupt type endpoint  */
	USB_DC_EP_INTERRUPT
};

/**
 * @brief USB Endpoint Configuration.
 *
 * Structure containing the USB endpoint configuration.
 */
struct usb_dc_ep_cfg_data {
	/** The number associated with the EP in the device
	 *  configuration structure
	 *       IN  EP = 0x80 | \<endpoint number\>
	 *       OUT EP = 0x00 | \<endpoint number\>
	 */
	u8_t ep_addr;
	/** Endpoint max packet size */
	u16_t ep_mps;
	/** Endpoint type. May be Bulk, Interrupt or Control. Isochronous
	 *  endpoints are not supported for now.
	 */
	enum usb_dc_ep_type ep_type;
};

/**
 * Callback function signature for the USB Endpoint status
 */
typedef void (*usb_dc_ep_callback)(u8_t ep,
				   enum usb_dc_ep_cb_status_code cb_status);

/**
 * Callback function signature for the device
 */
typedef void (*usb_dc_status_callback)(enum usb_dc_status_code cb_status,
				       u8_t *param);

/**
 * @brief Attach USB for device connection
 *
 * Function to attach USB for device connection. Upon success, the USB PLL
 * is enabled, and the USB device is now capable of transmitting and receiving
 * on the USB bus and of generating interrupts.
 *
 * @return 0 on success, negative errno code on fail.
 */
int usb_dc_attach(void);

/**
 * @brief Detach the USB device
 *
 * Function to detach the USB device. Upon success, the USB hardware PLL
 * is powered down and USB communication is disabled.
 *
 * @return 0 on success, negative errno code on fail.
 */
int usb_dc_detach(void);

/**
 * @brief Reset the USB device
 *
 * This function returns the USB device and firmware back to it's initial state.
 * N.B. the USB PLL is handled by the usb_detach function
 *
 * @return 0 on success, negative errno code on fail.
 */
int usb_dc_reset(void);

/**
 * @brief Set USB device address
 *
 * @param[in] addr Device address
 *
 * @return 0 on success, negative errno code on fail.
 */
int usb_dc_set_address(const u8_t addr);

/**
 * @brief Set USB device controller status callback
 *
 * Function to set USB device controller status callback. The registered
 * callback is used to report changes in the status of the device controller.
 * The status code are described by the usb_dc_status_code enumeration.
 *
 * @param[in] cb Callback function
 *
 * @return 0 on success, negative errno code on fail.
 */
int usb_dc_set_status_callback(const usb_dc_status_callback cb);

/**
 * @brief check endpoint capabilities
 *
 * Function to check capabilities of an endpoint. usb_dc_ep_cfg_data structure
 * provides the endpoint configuration parameters: endpoint address,
 * endpoint maximum packet size and endpoint type.
 * The driver should check endpoint capabilities and return 0 if the
 * endpoint configuration is possible.
 *
 * @param[in] cfg Endpoint config
 *
 * @return 0 on success, negative errno code on fail.
 */
int usb_dc_ep_check_cap(const struct usb_dc_ep_cfg_data * const cfg);

/**
 * @brief Configure endpoint
 *
 * Function to configure an endpoint. usb_dc_ep_cfg_data structure provides
 * the endpoint configuration parameters: endpoint address, endpoint maximum
 * packet size and endpoint type.
 *
 * @param[in] cfg Endpoint config
 *
 * @return 0 on success, negative errno code on fail.
 */
int usb_dc_ep_configure(const struct usb_dc_ep_cfg_data * const cfg);

/**
 * @brief Set stall condition for the selected endpoint
 *
 * @param[in] ep Endpoint address corresponding to the one
 *               listed in the device configuration table
 *
 * @return 0 on success, negative errno code on fail.
 */
int usb_dc_ep_set_stall(const u8_t ep);

/**
 * @brief Clear stall condition for the selected endpoint
 *
 * @param[in] ep Endpoint address corresponding to the one
 *               listed in the device configuration table
 *
 * @return 0 on success, negative errno code on fail.
 */
int usb_dc_ep_clear_stall(const u8_t ep);

/**
 * @brief Check if the selected endpoint is stalled
 *
 * @param[in]  ep       Endpoint address corresponding to the one
 *                      listed in the device configuration table
 * @param[out] stalled  Endpoint stall status
 *
 * @return 0 on success, negative errno code on fail.
 */
int usb_dc_ep_is_stalled(const u8_t ep, u8_t *const stalled);

/**
 * @brief Halt the selected endpoint
 *
 * @param[in] ep Endpoint address corresponding to the one
 *               listed in the device configuration table
 *
 * @return 0 on success, negative errno code on fail.
 */
int usb_dc_ep_halt(const u8_t ep);

/**
 * @brief Enable the selected endpoint
 *
 * Function to enable the selected endpoint. Upon success interrupts are
 * enabled for the corresponding endpoint and the endpoint is ready for
 * transmitting/receiving data.
 *
 * @param[in] ep Endpoint address corresponding to the one
 *               listed in the device configuration table
 *
 * @return 0 on success, negative errno code on fail.
 */
int usb_dc_ep_enable(const u8_t ep);

/**
 * @brief Disable the selected endpoint
 *
 * Function to disable the selected endpoint. Upon success interrupts are
 * disabled for the corresponding endpoint and the endpoint is no longer able
 * for transmitting/receiving data.
 *
 * @param[in] ep Endpoint address corresponding to the one
 *               listed in the device configuration table
 *
 * @return 0 on success, negative errno code on fail.
 */
int usb_dc_ep_disable(const u8_t ep);

/**
 * @brief Flush the selected endpoint
 *
 * This function flushes the FIFOs for the selected endpoint.
 *
 * @param[in] ep Endpoint address corresponding to the one
 *               listed in the device configuration table
 *
 * @return 0 on success, negative errno code on fail.
 */
int usb_dc_ep_flush(const u8_t ep);

/**
 * @brief Write data to the specified endpoint
 *
 * This function is called to write data to the specified endpoint. The
 * supplied usb_ep_callback function will be called when data is transmitted
 * out.
 *
 * @param[in]  ep        Endpoint address corresponding to the one
 *                       listed in the device configuration table
 * @param[in]  data      Pointer to data to write
 * @param[in]  data_len  Length of the data requested to write. This may
 *                       be zero for a zero length status packet.
 * @param[out] ret_bytes Bytes scheduled for transmission. This value
 *                       may be NULL if the application expects all
 *                       bytes to be written
 *
 * @return 0 on success, negative errno code on fail.
 */
int usb_dc_ep_write(const u8_t ep, const u8_t *const data,
		    const u32_t data_len, u32_t * const ret_bytes);

/**
 * @brief Read data from the specified endpoint
 *
 * This function is called by the endpoint handler function, after an OUT
 * interrupt has been received for that EP. The application must only call this
 * function through the supplied usb_ep_callback function. This function clears
 * the ENDPOINT NAK, if all data in the endpoint FIFO has been read,
 * so as to accept more data from host.
 *
 * @param[in]  ep           Endpoint address corresponding to the one
 *                          listed in the device configuration table
 * @param[in]  data         Pointer to data buffer to write to
 * @param[in]  max_data_len Max length of data to read
 * @param[out] read_bytes   Number of bytes read. If data is NULL and
 *                          max_data_len is 0 the number of bytes
 *                          available for read should be returned.
 *
 * @return 0 on success, negative errno code on fail.
 */
int usb_dc_ep_read(const u8_t ep, u8_t *const data,
		   const u32_t max_data_len, u32_t *const read_bytes);

/**
 * @brief Set callback function for the specified endpoint
 *
 * Function to set callback function for notification of data received and
 * available to application or transmit done on the selected endpoint,
 * NULL if callback not required by application code. The callback status
 * code is described by usb_dc_ep_cb_status_code.
 *
 * @param[in] ep Endpoint address corresponding to the one
 *               listed in the device configuration table
 * @param[in] cb Callback function
 *
 * @return 0 on success, negative errno code on fail.
 */
int usb_dc_ep_set_callback(const u8_t ep, const usb_dc_ep_callback cb);

/**
 * @brief Read data from the specified endpoint
 *
 * This is similar to usb_dc_ep_read, the difference being that, it doesn't
 * clear the endpoint NAKs so that the consumer is not bogged down by further
 * upcalls till he is done with the processing of the data. The caller should
 * reactivate ep by invoking usb_dc_ep_read_continue() do so.
 *
 * @param[in]  ep           Endpoint address corresponding to the one
 *                          listed in the device configuration table
 * @param[in]  data         Pointer to data buffer to write to
 * @param[in]  max_data_len Max length of data to read
 * @param[out] read_bytes   Number of bytes read. If data is NULL and
 *                          max_data_len is 0 the number of bytes
 *                          available for read should be returned.
 *
 * @return 0 on success, negative errno code on fail.
 */
int usb_dc_ep_read_wait(u8_t ep, u8_t *data, u32_t max_data_len,
			u32_t *read_bytes);

/**
 * @brief Continue reading data from the endpoint
 *
 * Clear the endpoint NAK and enable the endpoint to accept more data
 * from the host. Usually called after usb_dc_ep_read_wait() when the consumer
 * is fine to accept more data. Thus these calls together act as a flow control
 * mechanism.
 *
 * @param[in]  ep           Endpoint address corresponding to the one
 *                          listed in the device configuration table
 *
 * @return 0 on success, negative errno code on fail.
 */
int usb_dc_ep_read_continue(u8_t ep);

/**
 * @brief Get endpoint max packet size
 *
 * @param[in]  ep           Endpoint address corresponding to the one
 *                          listed in the device configuration table
 *
 * @return Enpoint max packet size (mps)
 */
int usb_dc_ep_mps(u8_t ep);

/**
 * @}
 */

#endif /* __USB_DC_H__ */
