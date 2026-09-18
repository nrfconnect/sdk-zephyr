/** @file
 *  @brief Channel carrying a fatal error report out of a Bluetooth controller.
 */

/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_INCLUDE_BLUETOOTH_HCI_FATAL_REPORT_H_
#define ZEPHYR_INCLUDE_BLUETOOTH_HCI_FATAL_REPORT_H_

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Fatal error report channel
 * @defgroup bt_hci_fatal_report Fatal error report channel
 * @ingroup bluetooth
 * @{
 *
 * A controller that runs on its own core reports a fatal error from a fault handler, with the
 * kernel in an unknown state and often from an interrupt that cannot block. The IPC instance
 * carrying HCI is unusable there, because a general purpose IPC backend may take a lock or hand
 * the message to a thread, and neither of those survives a fault.
 *
 * This channel exists to get that one report across anyway. Sending is nothing but a write into
 * shared memory followed by a mailbox signal, in the same spirit as reaching for
 * @c uart_poll_out() instead of the interrupt driven UART API. There is no session handshake and
 * no acknowledgement, so it works even when the two cores never agreed on anything beforehand.
 *
 * The channel is described in the devicetree with the @c zephyr,bt-hci-fatal-report binding, and
 * is only compiled in when such a node is present.
 */

/** @brief Called on the host when the controller reports a fatal error.
 *
 *  Invoked from a workqueue, so it may block.
 *
 *  @param data      The report, in the same format it would have had over HCI.
 *  @param len       Length of @p data in bytes.
 *  @param user_data Value passed to @ref bt_hci_fatal_report_rx_enable().
 */
typedef void (*bt_hci_fatal_report_cb_t)(const void *data, size_t len, void *user_data);

/** @brief Start taking fatal error reports from the controller.
 *
 *  Call this from the host. It is safe to call again to replace the callback.
 *
 *  @param cb        Callback invoked for each report.
 *  @param user_data Passed back to @p cb.
 *
 *  @retval 0 on success.
 *  @retval -EINVAL if @p cb is NULL.
 *  @retval -errno from the mailbox driver otherwise.
 */
int bt_hci_fatal_report_rx_enable(bt_hci_fatal_report_cb_t cb, void *user_data);

/** @brief Stop taking fatal error reports.
 *
 *  @retval 0 on success.
 *  @retval -errno from the mailbox driver otherwise.
 */
int bt_hci_fatal_report_rx_disable(void);

/** @brief Prepare the channel for sending.
 *
 *  Call this from the controller once it is running, and before any report can be raised. It
 *  resets the shared memory, which is why it cannot be left until a fault has happened.
 *
 *  @retval 0 on success.
 *  @retval -EINVAL if the shared memory region is not usable.
 */
int bt_hci_fatal_report_tx_enable(void);

/** @brief Send a fatal error report to the host.
 *
 *  Callable from any context, including an interrupt that cannot block and a fault handler with
 *  interrupts locked. Only one report fits in the channel at a time, on the assumption that the
 *  controller is reset afterwards.
 *
 *  @param data Report to send, in the format the host expects over HCI.
 *  @param len  Length of @p data in bytes.
 *
 *  @retval 0 on success.
 *  @retval -EINVAL if @p data is NULL or @p len is zero.
 *  @retval -ENOMEM if the report does not fit in the shared memory region.
 *  @retval -EIO if @ref bt_hci_fatal_report_tx_enable() has not been called.
 *  @retval -errno from the mailbox driver otherwise.
 */
int bt_hci_fatal_report_send(const void *data, size_t len);

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* ZEPHYR_INCLUDE_BLUETOOTH_HCI_FATAL_REPORT_H_ */
