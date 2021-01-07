/*
 * Copyright (c) 2020-2021, Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <ipc_service/rpmsg_service.h>

#include "rpmsg_backend.h"

#include <zephyr.h>
#include <device.h>
#include <logging/log.h>

#include <openamp/open_amp.h>

#define LOG_LEVEL LOG_LEVEL_INFO
#define LOG_MODULE_NAME rpmsg_service
LOG_MODULE_REGISTER(LOG_MODULE_NAME);

/* Configuration defines */
#if !DT_HAS_CHOSEN(zephyr_ipc_shm)
#error "Module requires definition of shared memory for rpmsg"
#endif

#define IPC_MASTER IS_ENABLED(CONFIG_IPC_SERVICE_MODE_MASTER)

static struct virtio_device vdev;
static struct rpmsg_virtio_device rvdev;
static struct metal_io_region *io;

#if IPC_MASTER
static struct rpmsg_virtio_shm_pool shpool;
#endif

static struct {
	const char *name;
	rpmsg_ept_cb cb;
	struct rpmsg_endpoint ep;
	volatile bool bound;
} endpoints[CONFIG_IPC_SERVICE_NUM_ENDPOINTS];

static void rpmsg_service_unbind(struct rpmsg_endpoint *ep)
{
	rpmsg_destroy_ept(ep);
}

#if IPC_MASTER

static void ns_bind_cb(struct rpmsg_device *rdev,
					const char *name,
					uint32_t dest)
{
	int err;

	for (int i = 0; i < CONFIG_IPC_SERVICE_NUM_ENDPOINTS; ++i) {
		if (strcmp(name, endpoints[i].name) == 0) {
			err = rpmsg_create_ept(&endpoints[i].ep,
						   rdev,
						   name,
						   RPMSG_ADDR_ANY,
						   dest,
						   endpoints[i].cb,
						   rpmsg_service_unbind);

			if (err != 0) {
				LOG_ERR("Creating remote endpoint %s"
					" failed wirh error %d", name, err);
			} else {
				endpoints[i].bound = true;
			}

			return;
		}
	}

	LOG_ERR("Remote endpoint %s not registered locally", name);
}

#endif

static int rpmsg_service_init(const struct device *dev)
{
	int32_t err;

	(void)dev;

	LOG_DBG("RPMsg service initialization start");

	err = rpmsg_backend_init(&io, &vdev);
	if (err) {
		LOG_ERR("RPMsg backend init failed with error %d", err);
		return err;
	}

#if IPC_MASTER
	rpmsg_virtio_init_shm_pool(&shpool, (void *)SHM_START_ADDR, SHM_SIZE);
	err = rpmsg_init_vdev(&rvdev, &vdev, ns_bind_cb, io, &shpool);
#else
	err = rpmsg_init_vdev(&rvdev, &vdev, NULL, io, NULL);
#endif

	if (err) {
		LOG_ERR("rpmsg_init_vdev failed %d", err);
		return err;
	}

	LOG_DBG("RPMsg service initialized");

	return 0;
}

int rpmsg_service_register_endpoint(const char *name, rpmsg_ept_cb cb)
{
	for (int i = 0; i < CONFIG_IPC_SERVICE_NUM_ENDPOINTS; ++i) {
		if (!endpoints[i].name) {
			endpoints[i].name = name;
			endpoints[i].cb = cb;

#if !IPC_MASTER
			int err;
			struct rpmsg_device *rdev;

			rdev = rpmsg_virtio_get_rpmsg_device(&rvdev);
			err = rpmsg_create_ept(&endpoints[i].ep, rdev, name,
						RPMSG_ADDR_ANY, RPMSG_ADDR_ANY,
						cb, rpmsg_service_unbind);

			return err;
#endif /* !IPC_MASTER */

			return i;
		}
	}

	LOG_ERR("No free slots to register endpoint %s", name);

	return -ENOMEM;
}

bool rpmsg_service_endpoint_is_bound(int endpoint_id)
{
	return endpoints[endpoint_id].bound;
}

int rpmsg_service_send(int endpoint_id, const void *data, size_t len)
{
	return rpmsg_send(&endpoints[endpoint_id].ep, data, len);
}

SYS_INIT(rpmsg_service_init, POST_KERNEL, CONFIG_IPC_SERVICE_INIT_PRIORITY);
