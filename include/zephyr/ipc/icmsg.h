/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stddef.h>
#include <stdint.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/mbox.h>
#include <zephyr/ipc/ipc_service.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/sys/spsc_pbuf.h>

enum icmsg_state {
	ICMSG_STATE_OFF,
	ICMSG_STATE_BUSY,
	ICMSG_STATE_READY,
};

struct icmsg_config_t {
	uintptr_t tx_shm_addr;
	uintptr_t rx_shm_addr;
	size_t tx_shm_size;
	size_t rx_shm_size;
	struct mbox_channel mbox_tx;
	struct mbox_channel mbox_rx;
};

struct icmsg_data_t {
	/* Tx/Rx buffers. */
	struct spsc_pbuf *tx_ib;
	struct spsc_pbuf *rx_ib;

	/* Callbacks for an endpoint. */
	const struct ipc_service_cb *cb;
	void *ctx;

	/* General */
	const struct icmsg_config_t *cfg;
	struct k_work_delayable notify_work;
	struct k_work mbox_work;
	atomic_t state;
};

/** @brief Initialize an icmsg instance
 *
 *  This function is intended to be called during system initialization.
 *
 *  @param[in] conf Structure containing configuration parameters for the icmsg
 *                  instance being created.
 *  @param[inout] dev_data Structure containing run-time data used by the icmsg
 *                         instance. The structure shall be filled with zeros
 *                         when calling this function. The content of this
 *                         structure must be preserved while the icmsg instance
 *                         is active.
 *
 *  @retval 0 on success.
 *  @retval -EALREADY when the instance is already opened.
 *  @retval other errno codes from dependent modules.
 */
int icmsg_init(const struct icmsg_config_t *conf,
	       struct icmsg_data_t *dev_data);

/** @brief Open an icmsg instance
 *
 *  Open an icmsg instance to be able to send and receive messages to a remote
 *  instance.
 *  This function is blocking until the handshake with the remote instance is
 *  completed.
 *  This function is intended to be called late in the initialization process,
 *  possibly from a thread which can be safely blocked while handshake with the
 *  remote instance is being pefromed.
 *
 *  @param[in] conf Structure containing configuration parameters for the icmsg
 *                  instance being created.
 *  @param[inout] dev_data Structure containing run-time data used by the icmsg
 *                         instance. The structure is initialized with
 *                         @ref icmsg_init and its content must be preserved
 *                         while the icmsg instance is active.
 *  @param[in] cb Structure containing callback functions to be called on
 *                events generated by this icmsg instance. The pointed memory
 *                must be preserved while the icmsg instance is active.
 *  @param[in] ctx Pointer to context passed as an argument to callbacks.
 *
 *
 *  @retval 0 on success.
 *  @retval -EALREADY when the instance is already opened.
 *  @retval other errno codes from dependent modules.
 */
int icmsg_open(const struct icmsg_config_t *conf,
	       struct icmsg_data_t *dev_data,
	       const struct ipc_service_cb *cb, void *ctx);

/** @brief Close an icmsg instance
 *
 *  Closing an icmsg instance results in releasing all resources used by given
 *  instance including the shared memory regions and mbox devices.
 *
 *  @param[in] conf Structure containing configuration parameters for the icmsg
 *                  instance being closed. Its content must be the same as used
 *                  for creating this instance with @ref icmsg_open.
 *  @param[inout] dev_data Structure containing run-time data used by the icmsg
 *                         instance. The structure is initialized with
 *                         @ref icmsg_init and its content must be preserved
 *                         while the icmsg instance is active.
 *
 *  @retval 0 on success.
 *  @retval other errno codes from dependent modules.
 */
int icmsg_close(const struct icmsg_config_t *conf,
		struct icmsg_data_t *dev_data);

/** @brief Send a message to the remote icmsg instance.
 *
 *  @param[in] conf Structure containing configuration parameters for the icmsg
 *                  instance being created.
 *  @param[inout] dev_data Structure containing run-time data used by the icmsg
 *                         instance. The structure is initialized with
 *                         @ref icmsg_init and its content must be preserved
 *                         while the icmsg instance is active.
 *  @param[in] msg Pointer to a buffer containing data to send.
 *  @param[in] len Size of data in the @p msg buffer.
 *
 *
 *  @retval 0 on success.
 *  @retval -EBUSY when the instance has not finished handshake with the remote
 *                 instance.
 *  @retval -ENODATA when the requested data to send is empty.
 *  @retval -EBADMSG when the requested data to send is too big.
 *  @retval other errno codes from dependent modules.
 */
int icmsg_send(const struct icmsg_config_t *conf,
	       struct icmsg_data_t *dev_data,
	       const void *msg, size_t len);

/** @brief Clear memory in TX buffer.
 *
 *  This function is intended to be called at an early stage of boot process,
 *  before the instance is initialized and before the remote core has started.
 *
 *  @param[in] conf Structure containing configuration parameters for the icmsg
 *                  instance being created.
 *
 *  @retval 0 on success.
 */
int icmsg_clear_tx_memory(const struct icmsg_config_t *conf);

/** @brief Clear memory in RX buffer.
 *
 *  This function is intended to be called at an early stage of boot process,
 *  before the instance is initialized and before the remote core has started.
 *
 *  @param[in] conf Structure containing configuration parameters for the icmsg
 *                  instance being created.
 *
 *  @retval 0 on success.
 */
int icmsg_clear_rx_memory(const struct icmsg_config_t *conf);
