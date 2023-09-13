/* hci.h - Bluetooth Host Control Interface definitions */

/*
 * Copyright (c) 2015-2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef ZEPHYR_INCLUDE_BLUETOOTH_HCI_H_
#define ZEPHYR_INCLUDE_BLUETOOTH_HCI_H_

#include <stdbool.h>
#include <stdint.h>

#include <zephyr/net/buf.h>
#include <zephyr/bluetooth/addr.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/hci_types.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Allocate a HCI command buffer.
  *
  * This function allocates a new buffer for a HCI command. It is given
  * the OpCode (encoded e.g. using the BT_OP macro) and the total length
  * of the parameters. Upon successful return the buffer is ready to have
  * the parameters encoded into it.
  *
  * @param opcode     Command OpCode.
  * @param param_len  Length of command parameters.
  *
  * @return Newly allocated buffer.
  */
struct net_buf *bt_hci_cmd_create(uint16_t opcode, uint8_t param_len);

/** Send a HCI command asynchronously.
  *
  * This function is used for sending a HCI command asynchronously. It can
  * either be called for a buffer created using bt_hci_cmd_create(), or
  * if the command has no parameters a NULL can be passed instead. The
  * sending of the command will happen asynchronously, i.e. upon successful
  * return from this function the caller only knows that it was queued
  * successfully.
  *
  * If synchronous behavior, and retrieval of the Command Complete parameters
  * is desired, the bt_hci_cmd_send_sync() API should be used instead.
  *
  * @param opcode Command OpCode.
  * @param buf    Command buffer or NULL (if no parameters).
  *
  * @return 0 on success or negative error value on failure.
  */
int bt_hci_cmd_send(uint16_t opcode, struct net_buf *buf);

/** Send a HCI command synchronously.
  *
  * This function is used for sending a HCI command synchronously. It can
  * either be called for a buffer created using bt_hci_cmd_create(), or
  * if the command has no parameters a NULL can be passed instead.
  *
  * The function will block until a Command Status or a Command Complete
  * event is returned. If either of these have a non-zero status the function
  * will return a negative error code and the response reference will not
  * be set. If the command completed successfully and a non-NULL rsp parameter
  * was given, this parameter will be set to point to a buffer containing
  * the response parameters.
  *
  * @param opcode Command OpCode.
  * @param buf    Command buffer or NULL (if no parameters).
  * @param rsp    Place to store a reference to the command response. May
  *               be NULL if the caller is not interested in the response
  *               parameters. If non-NULL is passed the caller is responsible
  *               for calling net_buf_unref() on the buffer when done parsing
  *               it.
  *
  * @return 0 on success or negative error value on failure.
  */
int bt_hci_cmd_send_sync(uint16_t opcode, struct net_buf *buf,
			 struct net_buf **rsp);

/** @brief Get connection handle for a connection.
 *
 * @param conn Connection object.
 * @param conn_handle Place to store the Connection handle.
 *
 * @return 0 on success or negative error value on failure.
 */
int bt_hci_get_conn_handle(const struct bt_conn *conn, uint16_t *conn_handle);

/** @brief Get advertising handle for an advertising set.
 *
 * @param adv Advertising set.
 * @param adv_handle Place to store the advertising handle.
 *
 * @return 0 on success or negative error value on failure.
 */
int bt_hci_get_adv_handle(const struct bt_le_ext_adv *adv, uint8_t *adv_handle);

/** @typedef bt_hci_vnd_evt_cb_t
  * @brief Callback type for vendor handling of HCI Vendor-Specific Events.
  *
  * A function of this type is registered with bt_hci_register_vnd_evt_cb()
  * and will be called for any HCI Vendor-Specific Event.
  *
  * @param buf Buffer containing event parameters.
  *
  * @return true if the function handles the event or false to defer the
  *         handling of this event back to the stack.
  */
typedef bool bt_hci_vnd_evt_cb_t(struct net_buf_simple *buf);

/** Register user callback for HCI Vendor-Specific Events
  *
  * @param cb Callback to be called when the stack receives a
  *           HCI Vendor-Specific Event.
  *
  * @return 0 on success or negative error value on failure.
  */
int bt_hci_register_vnd_evt_cb(bt_hci_vnd_evt_cb_t cb);

/** @brief Get Random bytes from the LE Controller.
 *
 * Send the HCI_LE_Rand to the LE Controller as many times as required to
 * fill the provided @p buffer.
 *
 * @note This function is provided as a helper to gather an arbitrary number of
 * random bytes from an LE Controller using the HCI_LE_Rand command.
 *
 * @param buffer Buffer to fill with random bytes.
 * @param len Length of the buffer in bytes.
 *
 * @return 0 on success or negative error value on failure.
 */
int bt_hci_le_rand(void *buffer, size_t len);


#ifdef __cplusplus
}
#endif

#endif /* ZEPHYR_INCLUDE_BLUETOOTH_HCI_H_ */
