/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>

#include <zephyr/device.h>
#include <zephyr/ipc/icmsg.h>
#include <zephyr/ipc/ipc_service_backend.h>

#define DT_DRV_COMPAT	zephyr_ipc_icmsg_me_initiator

#define SEND_BUF_SIZE CONFIG_IPC_SERVICE_BACKEND_ICMSG_ME_SEND_BUF_SIZE
#define NUM_EP        CONFIG_IPC_SERVICE_BACKEND_ICMSG_ME_NUM_EP
#define EP_NAME_LEN   CONFIG_IPC_SERVICE_BACKEND_ICMSG_ME_EP_NAME_LEN

#define EVENT_BOUND 0x01

/* If more bytes than 1 was used for endpoint id, endianness should be
 * considered.
 */
typedef uint8_t ept_id_t;

struct backend_data_t {
	struct icmsg_data_t icmsg_data;
	struct ipc_ept_cfg ept_cfg;

	struct k_event event;

	struct k_mutex epts_mutex;
	struct k_mutex send_mutex;
	const struct ipc_ept_cfg *epts[NUM_EP];
	ept_id_t ids[NUM_EP];

	uint8_t send_buffer[SEND_BUF_SIZE] __aligned(4);
};

static void bound(void *priv)
{
	const struct device *instance = priv;
	struct backend_data_t *dev_data = instance->data;

	k_event_post(&dev_data->event, EVENT_BOUND);
}

static void received(const void *data, size_t len, void *priv)
{
	const struct device *instance = priv;
	struct backend_data_t *dev_data = instance->data;

	const ept_id_t *id = data;

	__ASSERT_NO_MSG(len > 0);

	if (*id == 0) {
		__ASSERT_NO_MSG(len > 1);

		id++;
		ept_id_t ept_id = *id;

		int i = ept_id - 1;

		if (ept_id > NUM_EP) {
			return;
		}

		const struct ipc_ept_cfg *ept = dev_data->epts[i];

		if (ept->cb.bound) {
			ept->cb.bound(ept->priv);
		}
	} else {
		int i = *id - 1;

		if (i >= NUM_EP) {
			return;
		}
		if (dev_data->epts[i] == NULL) {
			return;
		}
		if (dev_data->epts[i]->cb.received) {
			dev_data->epts[i]->cb.received(id + 1,
					len - sizeof(ept_id_t),
					dev_data->epts[i]->priv);
		}
	}
}

static const struct ipc_service_cb cb = {
	.bound = bound,
	.received = received,
	.error = NULL,
};

static int open(const struct device *instance)
{
	const struct icmsg_config_t *conf = instance->config;
	struct backend_data_t *dev_data = instance->data;

	dev_data->ept_cfg.cb = cb;
	dev_data->ept_cfg.priv = (void *)instance;

	return icmsg_open(conf, &dev_data->icmsg_data, &dev_data->ept_cfg.cb,
			dev_data->ept_cfg.priv);
}

static int register_ept(const struct device *instance, void **token,
			const struct ipc_ept_cfg *cfg)
{
	const struct icmsg_config_t *conf = instance->config;
	struct backend_data_t *data = instance->data;
	int r = 0;
	int i;
	ept_id_t id;
	size_t name_len = strlen(cfg->name);

	if (name_len > EP_NAME_LEN) {
		return -EINVAL;
	}

	k_mutex_lock(&data->epts_mutex, K_FOREVER);

	for (i = 0; i < NUM_EP; i++) {
		if (data->epts[i] == NULL) {
			break;
		}
	}

	if (i >= NUM_EP) {
		r = -ENOMEM;
		goto exit;
	}

	id = i + 1;

	uint8_t ep_disc_req[EP_NAME_LEN + 2 * sizeof(ept_id_t)] = {
		0,  /* EP discovery endpoint id */
		id, /* Bound endpoint id */
	};
	memcpy(&ep_disc_req[2], cfg->name, name_len);

	data->epts[i] = cfg;
	data->ids[i] = id;
	*token = &data->ids[i];

	k_event_wait(&data->event, EVENT_BOUND, false, K_FOREVER);

	r = icmsg_send(conf, &data->icmsg_data, ep_disc_req,
		       2 * sizeof(ept_id_t) + name_len);
	if (r < 0) {
		data->epts[i] = NULL;
		goto exit;
	} else {
		r = 0;
	}

exit:
	k_mutex_unlock(&data->epts_mutex);
	return r;
}

static int send(const struct device *instance, void *token,
		const void *msg, size_t len)
{
	const struct icmsg_config_t *conf = instance->config;
	struct backend_data_t *dev_data = instance->data;
	ept_id_t *id = token;
	int r;
	int sent_bytes;

	if (len >= SEND_BUF_SIZE - sizeof(ept_id_t)) {
		return -EBADMSG;
	}

	k_mutex_lock(&dev_data->send_mutex, K_FOREVER);

	/* TODO: Optimization: How to avoid this copying? */
	/* We could implement scatter list for icmsg_send, but it would require
	 * scatter list also for SPSC buffer implementation.
	 */
	dev_data->send_buffer[0] = *id;
	memcpy(dev_data->send_buffer + sizeof(ept_id_t), msg, len);

	r = icmsg_send(conf, &dev_data->icmsg_data, dev_data->send_buffer,
			len + sizeof(ept_id_t));
	if (r > 0) {
		sent_bytes = r - 1;
	}

	k_mutex_unlock(&dev_data->send_mutex);

	if (r > 0) {
		return sent_bytes;
	} else {
		return r;
	}
}

const static struct ipc_service_backend backend_ops = {
	.open_instance = open,
	.register_endpoint = register_ept,
	.send = send,
};

static int backend_init(const struct device *instance)
{
	const struct icmsg_config_t *conf = instance->config;
	struct backend_data_t *dev_data = instance->data;

	k_event_init(&dev_data->event);
	k_mutex_init(&dev_data->epts_mutex);
	k_mutex_init(&dev_data->send_mutex);

	return icmsg_init(conf, &dev_data->icmsg_data);
}

#define BACKEND_CONFIG_POPULATE(i)						\
	{									\
		.tx_shm_size = DT_REG_SIZE(DT_INST_PHANDLE(i, tx_region)),	\
		.tx_shm_addr = DT_REG_ADDR(DT_INST_PHANDLE(i, tx_region)),	\
		.rx_shm_size = DT_REG_SIZE(DT_INST_PHANDLE(i, rx_region)),	\
		.rx_shm_addr = DT_REG_ADDR(DT_INST_PHANDLE(i, rx_region)),	\
		.mbox_tx = MBOX_DT_CHANNEL_GET(DT_DRV_INST(i), tx),		\
		.mbox_rx = MBOX_DT_CHANNEL_GET(DT_DRV_INST(i), rx),		\
	}

#define DEFINE_BACKEND_DEVICE(i)						\
	static const struct icmsg_config_t backend_config_##i =			\
		BACKEND_CONFIG_POPULATE(i);					\
	static struct backend_data_t backend_data_##i;				\
										\
	DEVICE_DT_INST_DEFINE(i,						\
			 &backend_init,						\
			 NULL,							\
			 &backend_data_##i,					\
			 &backend_config_##i,					\
			 POST_KERNEL,						\
			 CONFIG_IPC_SERVICE_REG_BACKEND_PRIORITY,		\
			 &backend_ops);

DT_INST_FOREACH_STATUS_OKAY(DEFINE_BACKEND_DEVICE)

#if defined(CONFIG_IPC_SERVICE_BACKEND_ICMSG_ME_SHMEM_RESET)
#define BACKEND_CONFIG_DEFINE(i) BACKEND_CONFIG_POPULATE(i),
static int shared_memory_prepare(const struct device *arg)
{
	const struct icmsg_config_t *backend_config;
	const struct icmsg_config_t backend_configs[] = {
		DT_INST_FOREACH_STATUS_OKAY(BACKEND_CONFIG_DEFINE)
	};

	for (backend_config = backend_configs;
	     backend_config < backend_configs + ARRAY_SIZE(backend_configs);
	     backend_config++) {
		icmsg_clear_tx_memory(backend_config);
	}

	return 0;
}

SYS_INIT(shared_memory_prepare, PRE_KERNEL_1, 1);
#endif /* CONFIG_IPC_SERVICE_BACKEND_ICMSG_ME_SHMEM_RESET */
