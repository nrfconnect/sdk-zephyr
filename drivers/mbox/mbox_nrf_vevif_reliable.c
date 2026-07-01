/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT nordic_nrf_vevif_reliable

#include <zephyr/devicetree.h>
#include <zephyr/drivers/mbox.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/atomic.h>

LOG_MODULE_REGISTER(mbox_nrf_vevif_reliable, CONFIG_MBOX_LOG_LEVEL);

#define RELIABLE_TX_CHANNEL 0U
#define RELIABLE_RX_CHANNEL 1U
#define RELIABLE_CHANNEL_COUNT 2U

#define RELIABLE_SHARED_MAGIC 0x56455253U
#define RELIABLE_SHARED_VERSION 1U

struct mbox_vevif_reliable_dir_state {
	atomic_t produced;
	atomic_t consumed;
	atomic_t send_lock;
};

struct mbox_vevif_reliable_shared_state {
	uint32_t magic;
	uint32_t version;
	struct mbox_vevif_reliable_dir_state dir[2];
};

struct mbox_vevif_reliable_config {
	struct mbox_dt_spec raw_rx;
	struct mbox_dt_spec raw_tx;
	struct mbox_vevif_reliable_shared_state *shared;
	uint8_t local_id;
};

struct mbox_vevif_reliable_data {
	mbox_callback_t cb;
	void *user_data;
	atomic_t rx_enabled;
};

static struct mbox_vevif_reliable_dir_state *
reliable_tx_dir(const struct mbox_vevif_reliable_config *config)
{
	return &config->shared->dir[config->local_id];
}

static struct mbox_vevif_reliable_dir_state *
reliable_rx_dir(const struct mbox_vevif_reliable_config *config)
{
	return &config->shared->dir[1U - config->local_id];
}

static bool reliable_wait_for_consumed(struct mbox_vevif_reliable_dir_state *state,
				       atomic_val_t ticket)
{
	if (atomic_get(&state->consumed) >= ticket) {
		return true;
	}

	k_busy_wait(CONFIG_MBOX_NRF_VEVIF_RELIABLE_RETRY_DELAY_USEC);

	return atomic_get(&state->consumed) >= ticket;
}

static int reliable_acquire_send_lock(struct mbox_vevif_reliable_dir_state *state)
{
	const int lock_retries =
		MAX(1, CONFIG_MBOX_NRF_VEVIF_RELIABLE_LOCK_WAIT_USEC /
			    CONFIG_MBOX_NRF_VEVIF_RELIABLE_RETRY_DELAY_USEC);

	for (int attempt = 0; attempt < lock_retries; attempt++) {
		if (atomic_cas(&state->send_lock, 0, 1)) {
			return 0;
		}

		k_busy_wait(CONFIG_MBOX_NRF_VEVIF_RELIABLE_RETRY_DELAY_USEC);
	}

	return -EBUSY;
}

static void reliable_release_send_lock(struct mbox_vevif_reliable_dir_state *state)
{
	atomic_set(&state->send_lock, 0);
}

static void reliable_raw_rx_callback(const struct device *raw_dev, mbox_channel_id_t channel_id,
				     void *user_data, struct mbox_msg *msg)
{
	const struct device *dev = user_data;
	const struct mbox_vevif_reliable_config *config = dev->config;
	struct mbox_vevif_reliable_data *data = dev->data;
	struct mbox_vevif_reliable_dir_state *state = reliable_rx_dir(config);

	ARG_UNUSED(raw_dev);
	ARG_UNUSED(channel_id);
	ARG_UNUSED(msg);

	if ((atomic_get(&data->rx_enabled) == 0) || (data->cb == NULL)) {
		return;
	}

	while (true) {
		atomic_val_t produced = atomic_get(&state->produced);
		atomic_val_t consumed = atomic_get(&state->consumed);

		if (consumed >= produced) {
			break;
		}

		data->cb(dev, RELIABLE_RX_CHANNEL, data->user_data, NULL);
		atomic_set(&state->consumed, consumed + 1);
	}
}

static int reliable_send(const struct device *dev, mbox_channel_id_t channel_id,
			 const struct mbox_msg *msg)
{
	const struct mbox_vevif_reliable_config *config = dev->config;
	struct mbox_vevif_reliable_dir_state *state = reliable_tx_dir(config);
	atomic_val_t ticket;
	int ret;

	if (channel_id == RELIABLE_RX_CHANNEL) {
		return -ENOSYS;
	}

	if (channel_id != RELIABLE_TX_CHANNEL) {
		return -EINVAL;
	}

	if (msg != NULL) {
		return -EMSGSIZE;
	}

	if (k_is_in_isr()) {
		return -EWOULDBLOCK;
	}

	ret = reliable_acquire_send_lock(state);
	if (ret < 0) {
		LOG_WRN("Reliable send lock timed out");
		return ret;
	}

	ticket = atomic_inc(&state->produced) + 1;

	for (int attempt = 0; attempt < CONFIG_MBOX_NRF_VEVIF_RELIABLE_MAX_RETRIES; attempt++) {
		ret = mbox_send_dt(&config->raw_tx, NULL);
		if (ret < 0) {
			reliable_release_send_lock(state);
			return ret;
		}

		if (reliable_wait_for_consumed(state, ticket)) {
			reliable_release_send_lock(state);
			return 0;
		}
	}

	LOG_WRN("Reliable send timed out ticket=%ld produced=%ld consumed=%ld",
		(long)ticket, (long)atomic_get(&state->produced),
		(long)atomic_get(&state->consumed));

	reliable_release_send_lock(state);

	return -ETIMEDOUT;
}

static int reliable_register_callback(const struct device *dev, mbox_channel_id_t channel_id,
				      mbox_callback_t cb, void *user_data)
{
	struct mbox_vevif_reliable_data *data = dev->data;

	if (channel_id == RELIABLE_TX_CHANNEL) {
		return -ENOSYS;
	}

	if (channel_id != RELIABLE_RX_CHANNEL) {
		return -EINVAL;
	}

	data->cb = cb;
	data->user_data = user_data;

	return 0;
}

static int reliable_mtu_get(const struct device *dev)
{
	ARG_UNUSED(dev);

	return 0;
}

static uint32_t reliable_max_channels_get(const struct device *dev)
{
	ARG_UNUSED(dev);

	return RELIABLE_CHANNEL_COUNT;
}

static int reliable_set_enabled(const struct device *dev, mbox_channel_id_t channel_id, bool enable)
{
	const struct mbox_vevif_reliable_config *config = dev->config;
	struct mbox_vevif_reliable_data *data = dev->data;
	int ret;

	if (channel_id == RELIABLE_TX_CHANNEL) {
		return -ENOSYS;
	}

	if (channel_id != RELIABLE_RX_CHANNEL) {
		return -EINVAL;
	}

	if (enable) {
		if (atomic_get(&data->rx_enabled) != 0) {
			return -EALREADY;
		}

		if (data->cb == NULL) {
			return -EINVAL;
		}

		ret = mbox_set_enabled_dt(&config->raw_rx, true);
		if (ret < 0) {
			return ret;
		}

		atomic_set(&data->rx_enabled, 1);
	} else {
		if (atomic_get(&data->rx_enabled) == 0) {
			return -EALREADY;
		}

		ret = mbox_set_enabled_dt(&config->raw_rx, false);
		if (ret < 0) {
			return ret;
		}

		atomic_set(&data->rx_enabled, 0);
	}

	return 0;
}

static DEVICE_API(mbox, mbox_vevif_reliable_api) = {
	.send = reliable_send,
	.register_callback = reliable_register_callback,
	.mtu_get = reliable_mtu_get,
	.max_channels_get = reliable_max_channels_get,
	.set_enabled = reliable_set_enabled,
};

static int mbox_vevif_reliable_init(const struct device *dev)
{
	const struct mbox_vevif_reliable_config *config = dev->config;
	struct mbox_vevif_reliable_shared_state *shared = config->shared;
	int ret;

	if (!mbox_is_ready_dt(&config->raw_rx) || !mbox_is_ready_dt(&config->raw_tx)) {
		return -ENODEV;
	}

	if ((shared->magic != RELIABLE_SHARED_MAGIC) || (shared->version != RELIABLE_SHARED_VERSION)) {
		for (size_t i = 0; i < ARRAY_SIZE(shared->dir); i++) {
			atomic_set(&shared->dir[i].produced, 0);
			atomic_set(&shared->dir[i].consumed, 0);
			atomic_set(&shared->dir[i].send_lock, 0);
		}

		shared->version = RELIABLE_SHARED_VERSION;
		shared->magic = RELIABLE_SHARED_MAGIC;
	}

	ret = mbox_register_callback_dt(&config->raw_rx, reliable_raw_rx_callback, (void *)dev);
	if (ret < 0) {
		return ret;
	}

	return 0;
}

#define RELIABLE_RAW_RX(inst) MBOX_DT_SPEC_GET(DT_DRV_INST(inst), rx)
#define RELIABLE_RAW_TX(inst) MBOX_DT_SPEC_GET(DT_DRV_INST(inst), tx)
#define RELIABLE_MEMORY_NODE(inst) DT_INST_PHANDLE(inst, memory_region)

#define MBOX_VEVIF_RELIABLE_DEFINE(inst)                                                           \
	BUILD_ASSERT(DT_REG_SIZE(RELIABLE_MEMORY_NODE(inst)) >=                                    \
			     sizeof(struct mbox_vevif_reliable_shared_state),                      \
		     "Reliable VEVIF shared memory region is too small");                          \
	static struct mbox_vevif_reliable_data mbox_vevif_reliable_data_##inst;                    \
	static const struct mbox_vevif_reliable_config mbox_vevif_reliable_config_##inst = {       \
		.raw_rx = RELIABLE_RAW_RX(inst),                                                   \
		.raw_tx = RELIABLE_RAW_TX(inst),                                                   \
		.shared = (struct mbox_vevif_reliable_shared_state *)                              \
			  DT_REG_ADDR(RELIABLE_MEMORY_NODE(inst)),                                 \
		.local_id = DT_INST_PROP(inst, nordic_local_id),                                   \
	};                                                                                         \
	DEVICE_DT_INST_DEFINE(inst, mbox_vevif_reliable_init, NULL,                                \
			      &mbox_vevif_reliable_data_##inst,                                    \
			      &mbox_vevif_reliable_config_##inst, POST_KERNEL,                     \
			      CONFIG_MBOX_NRF_VEVIF_RELIABLE_INIT_PRIORITY,                        \
			      &mbox_vevif_reliable_api);

DT_INST_FOREACH_STATUS_OKAY(MBOX_VEVIF_RELIABLE_DEFINE)
