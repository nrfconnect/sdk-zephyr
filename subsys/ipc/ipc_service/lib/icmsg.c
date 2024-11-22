/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/ipc/icmsg.h>

#include <string.h>
#include <zephyr/drivers/mbox.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/ipc/pbuf.h>
#include <zephyr/init.h>

#define LOCAL_SID_REQ_FROM_RX(rx_handshake) ((rx_handshake) & 0xFFFF)
#define REMOTE_SID_ACK_FROM_RX(rx_handshake) ((rx_handshake) >> 16)
#define REMOTE_SID_REQ_FROM_TX(tx_handshake) ((tx_handshake) & 0xFFFF)
#define LOCAL_SID_ACK_FROM_TX(tx_handshake) ((tx_handshake) >> 16)
#define MAKE_RX_HANDSHAKE(local_sid_req, remote_sid_ack) ((local_sid_req) | ((remote_sid_ack) << 16))
#define MAKE_TX_HANDSHAKE(remote_sid_req, local_sid_ack) ((remote_sid_req) | ((local_sid_ack) << 16))

#define SID_DISCONNECTED 0

#define BOND_NOTIFY_REPEAT_TO	K_MSEC(CONFIG_IPC_SERVICE_ICMSG_BOND_NOTIFY_REPEAT_TO_MS)
#define SHMEM_ACCESS_TO		K_MSEC(CONFIG_IPC_SERVICE_ICMSG_SHMEM_ACCESS_TO_MS)

static const uint8_t magic[] = {0x45, 0x6d, 0x31, 0x6c, 0x31, 0x4b,
				0x30, 0x72, 0x6e, 0x33, 0x6c, 0x69, 0x34};

#ifdef CONFIG_MULTITHREADING
#if defined(CONFIG_IPC_SERVICE_BACKEND_ICMSG_WQ_ENABLE)
static K_THREAD_STACK_DEFINE(icmsg_stack, CONFIG_IPC_SERVICE_BACKEND_ICMSG_WQ_STACK_SIZE);
static struct k_work_q icmsg_workq;
static struct k_work_q *const workq = &icmsg_workq;
#else
static struct k_work_q *const workq = &k_sys_work_q;
#endif
static void mbox_callback_process(struct k_work *item);
#else
static bool mbox_callback_process(struct icmsg_data_t *dev_data);
#endif

static int mbox_deinit(const struct icmsg_config_t *conf,
		       struct icmsg_data_t *dev_data)
{
	int err;

	err = mbox_set_enabled_dt(&conf->mbox_rx, 0);
	if (err != 0) {
		return err;
	}

	err = mbox_register_callback_dt(&conf->mbox_rx, NULL, NULL);
	if (err != 0) {
		return err;
	}

#ifdef CONFIG_MULTITHREADING
	(void)k_work_cancel(&dev_data->mbox_work);
	(void)k_work_cancel_delayable(&dev_data->notify_work);
#endif

	return 0;
}

static bool is_endpoint_ready(atomic_t state)
{
	return (state >= MIN(ICMSG_STATE_CONNECTED_SID_DISABLED, ICMSG_STATE_CONNECTED_SID_ENABLED));
}

#ifdef CONFIG_MULTITHREADING
static void notify_process(struct k_work *item)
{
	struct k_work_delayable *dwork = k_work_delayable_from_work(item);
	struct icmsg_data_t *dev_data =
		CONTAINER_OF(dwork, struct icmsg_data_t, notify_work);

	(void)mbox_send_dt(&dev_data->cfg->mbox_tx, NULL);

	atomic_t state = atomic_get(&dev_data->state);

	if (!is_endpoint_ready(state)) {
		int ret;

		ret = k_work_reschedule_for_queue(workq, dwork, BOND_NOTIFY_REPEAT_TO);
		__ASSERT_NO_MSG(ret >= 0);
		(void)ret;
	}
}
#else
static void notify_process(struct icmsg_data_t *dev_data)
{
	(void)mbox_send_dt(&dev_data->cfg->mbox_tx, NULL);
#if defined(CONFIG_SYS_CLOCK_EXISTS)
	int64_t start = k_uptime_get();
#endif

	while (!is_endpoint_ready(atomic_get(&dev_data->state))) {
		mbox_callback_process(dev_data);

#if defined(CONFIG_SYS_CLOCK_EXISTS)
		if ((k_uptime_get() - start) > CONFIG_IPC_SERVICE_ICMSG_BOND_NOTIFY_REPEAT_TO_MS) {
#endif
			(void)mbox_send_dt(&dev_data->cfg->mbox_tx, NULL);
#if defined(CONFIG_SYS_CLOCK_EXISTS)
			start = k_uptime_get();
		};
#endif
	}
}
#endif

static inline int reserve_tx_buffer_if_unused(struct icmsg_data_t *dev_data)
{
#ifdef CONFIG_IPC_SERVICE_ICMSG_SHMEM_ACCESS_SYNC
	return k_mutex_lock(&dev_data->tx_lock, SHMEM_ACCESS_TO);
#else
	return 0;
#endif
}

static inline int release_tx_buffer(struct icmsg_data_t *dev_data)
{
#ifdef CONFIG_IPC_SERVICE_ICMSG_SHMEM_ACCESS_SYNC
	return k_mutex_unlock(&dev_data->tx_lock);
#else
	return 0;
#endif
}

static uint32_t data_available(struct icmsg_data_t *dev_data)
{
	return pbuf_read(dev_data->rx_pb, NULL, 0);
}

#ifdef CONFIG_MULTITHREADING
static void submit_mbox_work(struct icmsg_data_t *dev_data)
{
	if (k_work_submit_to_queue(workq, &dev_data->mbox_work) < 0) {
		/* The mbox processing work is never canceled.
		 * The negative error code should never be seen.
		 */
		__ASSERT_NO_MSG(false);
	}
}

#endif

#ifdef CONFIG_MULTITHREADING
static void mbox_callback_process(struct k_work *item)
#else
static bool mbox_callback_process(struct icmsg_data_t *dev_data)
#endif
{
#ifdef CONFIG_MULTITHREADING
	struct icmsg_data_t *dev_data = CONTAINER_OF(item, struct icmsg_data_t, mbox_work);
#endif
	int ret;
	uint8_t rx_buffer[CONFIG_PBUF_RX_READ_BUF_SIZE] __aligned(4);
	uint32_t len = 0;
	uint32_t len_available;
	bool rerun = false;
	bool notify_remote = false;
	atomic_t state = atomic_get(&dev_data->state);

	uint32_t tx_handshake = pbuf_handshake_read(dev_data->tx_pb);
	uint32_t remote_sid_req = REMOTE_SID_REQ_FROM_TX(tx_handshake);
	uint32_t local_sid_ack = LOCAL_SID_ACK_FROM_TX(tx_handshake);

	volatile char *magic_buf;
	uint16_t magic_len;

	switch (state) {
	case ICMSG_STATE_INITIALIZING_SID_COMPAT:
		// Initialization with detection of remote session awareness

		ret = pbuf_get_initial_buf(dev_data->rx_pb, &magic_buf, &magic_len);

		if (ret == 0 && magic_len == sizeof(magic) && memcmp((void*)magic_buf, magic, sizeof(magic)) == 0) {
			// Remote initalized in session-unaware mode, so we do old way of initialization.
			ret = pbuf_tx_init(dev_data->tx_pb); // TODO: maybe extract to common function with "open".
			if (ret < 0) {
				if (dev_data->cb->error) { // TODO: goto cleanup
					dev_data->cb->error("Incorrect Tx configuration", dev_data->ctx);
				}
				__ASSERT(false, "Incorrect Tx configuration");
				atomic_set(&dev_data->state, ICMSG_STATE_DISCONNECTED);
				return;
			}
			ret = pbuf_write(dev_data->tx_pb, magic, sizeof(magic));
			if (ret < 0) {
				if (dev_data->cb->error) { // TODO: goto cleanup
					dev_data->cb->error("Incorrect Tx configuration", dev_data->ctx);
				}
				__ASSERT(false, "Incorrect Tx configuration");
				atomic_set(&dev_data->state, ICMSG_STATE_DISCONNECTED);
				return;
			}
			// We got magic data, so we can handle it later.
			notify_remote = true;
			rerun = true;
			atomic_set(&dev_data->state, ICMSG_STATE_INITIALIZING_SID_DISABLED);
			break;
		}
		// If remote did not initialize the RX in session-unaware mode, we can try
		// with session-aware initialization.
		__fallthrough;

	case ICMSG_STATE_INITIALIZING_SID_ENABLED:
		if (remote_sid_req != dev_data->remote_session && remote_sid_req != SID_DISCONNECTED) {
			// We can now initialize TX, since we know that remote, during receiving,
			// will first read FIFO indexes and later, it will check if session has changed
			// before using indexes to receive the message. Additionally, we know that
			// remote after session request change will no try to receive more data.
			ret = pbuf_tx_init(dev_data->tx_pb);
			if (ret < 0) {
				if (dev_data->cb->error) {
					dev_data->cb->error("Incorrect Tx configuration", dev_data->ctx);
				}
				__ASSERT(false, "Incorrect Tx configuration");
				atomic_set(&dev_data->state, ICMSG_STATE_DISCONNECTED);
				return;
			}
			// Acknowledge the remote session.
			dev_data->remote_session = remote_sid_req;
			pbuf_handshake_write(dev_data->rx_pb, MAKE_RX_HANDSHAKE(dev_data->local_session, dev_data->remote_session));
			notify_remote = true;
		}

		if (local_sid_ack == dev_data->local_session &&
		    dev_data->remote_session != SID_DISCONNECTED) {
			// We send acknowledge to remote, receive acknowledge from remote,
			// so we are ready.
			atomic_set(&dev_data->state, ICMSG_STATE_CONNECTED_SID_ENABLED);

			if (dev_data->cb->bound) {
				dev_data->cb->bound(dev_data->ctx);
			}
			// Re-run this handler, because remote may already send something.
			rerun = true;
			notify_remote = true;
		}

		break;

	case ICMSG_STATE_INITIALIZING_SID_DISABLED:
	case ICMSG_STATE_CONNECTED_SID_ENABLED:
	case ICMSG_STATE_CONNECTED_SID_DISABLED:

		len_available = data_available(dev_data);

		if (len_available > 0 && sizeof(rx_buffer) >= len_available) {
			len = pbuf_read(dev_data->rx_pb, rx_buffer, sizeof(rx_buffer));
		}

		if (state == ICMSG_STATE_CONNECTED_SID_ENABLED) {
			// The incoming message is valid only if remote session is as expected, so we need
			// to check remote session now.
			remote_sid_req = REMOTE_SID_REQ_FROM_TX(pbuf_handshake_read(dev_data->tx_pb));

			if (remote_sid_req != dev_data->remote_session) {
				atomic_set(&dev_data->state, ICMSG_STATE_CONNECTED_SID_ENABLED);
				if (dev_data->cb->unbound) {
					dev_data->cb->unbound(dev_data->ctx);
				}
				return;
			}
		}

		if (len_available == 0) {
			/* Unlikely, no data in buffer. */
			return;
		}

		__ASSERT_NO_MSG(len_available <= sizeof(rx_buffer));

		if (sizeof(rx_buffer) < len_available) {
			return;
		}

		if (state != ICMSG_STATE_INITIALIZING_SID_DISABLED) {
			if (dev_data->cb->received) {
				dev_data->cb->received(rx_buffer, len, dev_data->ctx);
			}
		} else {
			/* Allow magic number longer than sizeof(magic) for future protocol version. */
			bool endpoint_invalid = (len < sizeof(magic) ||
						memcmp(magic, rx_buffer, sizeof(magic)));

			if (endpoint_invalid) {
				__ASSERT_NO_MSG(false);
				return;
			}

			if (dev_data->cb->bound) {
				dev_data->cb->bound(dev_data->ctx);
			}

			atomic_set(&dev_data->state, ICMSG_STATE_CONNECTED_SID_DISABLED);
			notify_remote = true;
		}

		rerun = (data_available(dev_data) > 0);
		break;

	case ICMSG_STATE_UNINITIALIZED:
	case ICMSG_STATE_DISCONNECTED:
	default:
		// Nothing to do in this state.
		return;
	}

	if (notify_remote) {
		(void)mbox_send_dt(&dev_data->cfg->mbox_tx, NULL);
	}

#ifdef CONFIG_MULTITHREADING
	if (rerun) {
		submit_mbox_work(dev_data);
	}
#else
	return rerun;
#endif
}

static void mbox_callback(const struct device *instance, uint32_t channel,
			  void *user_data, struct mbox_msg *msg_data)
{
	bool rerun;
	struct icmsg_data_t *dev_data = user_data;

#ifdef CONFIG_MULTITHREADING
	ARG_UNUSED(rerun);
	submit_mbox_work(dev_data);
#else
	do {
		rerun = mbox_callback_process(dev_data);
	} while (rerun);
#endif
}

static int mbox_init(const struct icmsg_config_t *conf,
		     struct icmsg_data_t *dev_data)
{
	int err;

#ifdef CONFIG_MULTITHREADING
	k_work_init(&dev_data->mbox_work, mbox_callback_process);
	k_work_init_delayable(&dev_data->notify_work, notify_process);
#endif

	err = mbox_register_callback_dt(&conf->mbox_rx, mbox_callback, dev_data);
	if (err != 0) {
		return err;
	}

	return mbox_set_enabled_dt(&conf->mbox_rx, 1);
}

int icmsg_open(const struct icmsg_config_t *conf,
	       struct icmsg_data_t *dev_data,
	       const struct ipc_service_cb *cb, void *ctx)
{
	int ret;
	enum icmsg_state old_state;

	// Unbound mode has the same values as ICMSG_STATE_INITIALIZING_*
	old_state = atomic_set(&dev_data->state, conf->unbound_mode);

	dev_data->cb = cb;
	dev_data->ctx = ctx;
	dev_data->cfg = conf;

#ifdef CONFIG_IPC_SERVICE_ICMSG_SHMEM_ACCESS_SYNC
	k_mutex_init(&dev_data->tx_lock);
#endif

	ret = pbuf_rx_init(dev_data->rx_pb);

	if (ret < 0) {
		__ASSERT(false, "Incorrect Rx configuration");
		return ret;
	}

	if (conf->unbound_mode != ICMSG_UNBOUND_MODE_DISABLE) {
		// Increment local session id without conflicts with forbidden values.
		uint32_t local_session_ack =
			LOCAL_SID_ACK_FROM_TX(pbuf_handshake_read(dev_data->tx_pb));
		dev_data->local_session =
			LOCAL_SID_REQ_FROM_RX(pbuf_handshake_read(dev_data->tx_pb));
		dev_data->remote_session = SID_DISCONNECTED;
		do {
			dev_data->local_session = (dev_data->local_session + 1) & 0xFFFF;
		} while (dev_data->local_session == local_session_ack || dev_data->local_session == SID_DISCONNECTED);
		// Write local session id request without remote acknowledge
		pbuf_handshake_write(dev_data->rx_pb, MAKE_RX_HANDSHAKE(dev_data->local_session, SID_DISCONNECTED));
	} else {
		// Initialization of RX and sending magic bytes must be postponed
		// if unbound mode enabled. We need to wait until remote is ready.

		ret = pbuf_tx_init(dev_data->tx_pb);

		if (ret < 0) {
			__ASSERT(false, "Incorrect Tx configuration");
			return ret;
		}

		ret = pbuf_write(dev_data->tx_pb, magic, sizeof(magic));

		if (ret < 0) {
			__ASSERT_NO_MSG(false);
			return ret;
		}

		if (ret < (int)sizeof(magic)) {
			__ASSERT_NO_MSG(ret == sizeof(magic));
			return ret;
		}
	}

	if (old_state == ICMSG_STATE_UNINITIALIZED) {
		// Initialize mbox only if we are doing first-time open (not re-open after unbound)
		ret = mbox_init(conf, dev_data);
		if (ret) {
			return ret;
		}
	}

	if (conf->unbound_mode == ICMSG_UNBOUND_MODE_ENABLE) {
		// We need to send a notification to remote, it may not be delivered
		// since it may be uninitialized yet, but when it finishes the initialization
		// we get a notification from it. We need to send this notification in callback
		// again to make sure that it arrived.
		ret = mbox_send_dt(&conf->mbox_tx, NULL);
	} else {
		// Polling is only when unbound mode is disabled.
#ifdef CONFIG_MULTITHREADING
		ret = k_work_schedule_for_queue(workq, &dev_data->notify_work, K_NO_WAIT);
#else
		notify_process(dev_data);
		ret = 0;
#endif
	}
	return ret;
}

int icmsg_close(const struct icmsg_config_t *conf,
		struct icmsg_data_t *dev_data)
{
	int ret = 0;

	enum icmsg_state old_state;

	pbuf_handshake_write(dev_data->rx_pb, MAKE_RX_HANDSHAKE(SID_DISCONNECTED, SID_DISCONNECTED));

	(void)mbox_send_dt(&conf->mbox_tx, NULL);

	old_state = atomic_set(&dev_data->state, ICMSG_STATE_UNINITIALIZED);

	if (old_state != ICMSG_STATE_UNINITIALIZED) {
		ret = mbox_deinit(conf, dev_data);
	}

	return ret;
}

int icmsg_send(const struct icmsg_config_t *conf,
	       struct icmsg_data_t *dev_data,
	       const void *msg, size_t len)
{
	int ret;
	int write_ret;
#ifdef CONFIG_IPC_SERVICE_ICMSG_SHMEM_ACCESS_SYNC
	int release_ret;
#endif
	int sent_bytes;
	uint32_t state = atomic_get(&dev_data->state);

	if (!is_endpoint_ready(state)) {
		// If instance was disconnected on the remote side, some threads may still
		// don't know it yet and still may try to send messages.
		return (state == ICMSG_STATE_DISCONNECTED) ? len : -EBUSY;
	}

	/* Empty message is not allowed */
	if (len == 0) {
		return -ENODATA;
	}

	ret = reserve_tx_buffer_if_unused(dev_data);
	if (ret < 0) {
		return -ENOBUFS;
	}

	write_ret = pbuf_write(dev_data->tx_pb, msg, len);

	release_ret = release_tx_buffer(dev_data);
	__ASSERT_NO_MSG(!release_ret);

	if (write_ret < 0) {
		return write_ret;
	} else if (write_ret < len) {
		return -EBADMSG;
	}
	sent_bytes = write_ret;

	__ASSERT_NO_MSG(conf->mbox_tx.dev != NULL);

	ret = mbox_send_dt(&conf->mbox_tx, NULL);
	if (ret) {
		return ret;
	}

	return sent_bytes;
}

#if defined(CONFIG_IPC_SERVICE_BACKEND_ICMSG_WQ_ENABLE)

static int work_q_init(void)
{
	struct k_work_queue_config cfg = {
		.name = "icmsg_workq",
	};

	k_work_queue_start(&icmsg_workq,
			    icmsg_stack,
			    K_KERNEL_STACK_SIZEOF(icmsg_stack),
			    CONFIG_IPC_SERVICE_BACKEND_ICMSG_WQ_PRIORITY, &cfg);
	return 0;
}

SYS_INIT(work_q_init, POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT);

#endif
