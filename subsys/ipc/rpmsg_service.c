/*
 * Copyright (c) 2020-2021, Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <ipc_service/rpmsg_service.h>

#include <zephyr.h>
#include <drivers/ipm.h>
#include <device.h>
#include <logging/log.h>

#include <openamp/open_amp.h>
#include <metal/device.h>

#define LOG_LEVEL LOG_LEVEL_INFO
#define LOG_MODULE_NAME rpmsg_service
LOG_MODULE_REGISTER(LOG_MODULE_NAME);

/* Configuration defines */
#if !DT_HAS_CHOSEN(zephyr_ipc_shm)
#error "Sample requires definition of shared memory for rpmsg"
#endif

#define IPC_MASTER IS_ENABLED(CONFIG_IPC_SERVICE_MODE_MASTER)

#if IPC_MASTER
#define VIRTQUEUE_ID 0
#define RPMSG_ROLE RPMSG_MASTER
#else
#define VIRTQUEUE_ID 1
#define RPMSG_ROLE RPMSG_REMOTE
#endif

/* Configuration defines */

#define SHM_NODE            DT_CHOSEN(zephyr_ipc_shm)

#define VDEV_START_ADDR		DT_REG_ADDR(SHM_NODE)
#define VDEV_SIZE		    DT_REG_SIZE(SHM_NODE)

#define VDEV_STATUS_ADDR	VDEV_START_ADDR
#define VDEV_STATUS_SIZE	0x400

#define SHM_START_ADDR		(VDEV_START_ADDR + VDEV_STATUS_SIZE)
#define SHM_SIZE		    (VDEV_SIZE - VDEV_STATUS_SIZE)
#define SHM_DEVICE_NAME		"sramx.shm"

#define VRING_COUNT		    2
#define VRING_RX_ADDRESS	(VDEV_START_ADDR + SHM_SIZE - VDEV_STATUS_SIZE)
#define VRING_TX_ADDRESS	(VDEV_START_ADDR + SHM_SIZE)
#define VRING_ALIGNMENT		4
#define VRING_SIZE		    16

#define IPM_WORK_QUEUE_STACK_SIZE 2048

#if IS_ENABLED(CONFIG_COOP_ENABLED)
#define IPM_WORK_QUEUE_PRIORITY -1
#else
#define IPM_WORK_QUEUE_PRIORITY 0
#endif

K_THREAD_STACK_DEFINE(ipm_stack_area, IPM_WORK_QUEUE_STACK_SIZE);

struct k_work_q ipm_work_q;

/* End of configuration defines */

static const struct device *ipm_tx_handle;
static const struct device *ipm_rx_handle;

static metal_phys_addr_t shm_physmap[] = { SHM_START_ADDR };
static struct metal_device shm_device = {
	.name = SHM_DEVICE_NAME,
	.bus = NULL,
	.num_regions = 1,
	{
		{
			.virt       = (void *) SHM_START_ADDR,
			.physmap    = shm_physmap,
			.size       = SHM_SIZE,
			.page_shift = 0xffffffff,
			.page_mask  = 0xffffffff,
			.mem_flags  = 0,
			.ops        = { NULL },
		},
	},
	.node = { NULL },
	.irq_num = 0,
	.irq_info = NULL
};

static struct virtio_vring_info rvrings[2] = {
	[0] = {
		.info.align = VRING_ALIGNMENT,
	},
	[1] = {
		.info.align = VRING_ALIGNMENT,
	},
};
static struct virtio_device vdev;
static struct rpmsg_virtio_device rvdev;
static struct metal_io_region *io;
static struct virtqueue *vq[2];

#if IPC_MASTER
static struct rpmsg_virtio_shm_pool shpool;
#endif

static struct {
	const char *name;
	rpmsg_ept_cb cb;
	struct rpmsg_endpoint ep;
	volatile bool bound;
} endpoints[CONFIG_IPC_SERVICE_NUM_ENDPOINTS];

static struct k_work ipm_work;

static unsigned char virtio_get_status(struct virtio_device *vdev)
{
#if IPC_MASTER
	return VIRTIO_CONFIG_STATUS_DRIVER_OK;
#else
	return sys_read8(VDEV_STATUS_ADDR);
#endif
}

static void virtio_set_status(struct virtio_device *vdev, unsigned char status)
{
	sys_write8(status, VDEV_STATUS_ADDR);
}

static uint32_t virtio_get_features(struct virtio_device *vdev)
{
	return BIT(VIRTIO_RPMSG_F_NS);
}

static void virtio_set_features(struct virtio_device *vdev,
				uint32_t features)
{
}

/* TODO: virtio_set_features is needed? */

static void virtio_notify(struct virtqueue *vq)
{
	int status;

	status = ipm_send(ipm_tx_handle, 0, 0, NULL, 0);
	if (status != 0) {
		LOG_ERR("ipm_send failed to notify: %d", status);
	}
}

const struct virtio_dispatch dispatch = {
	.get_status = virtio_get_status,
	.set_status = virtio_set_status,
	.get_features = virtio_get_features,
	.set_features = virtio_set_features,
	.notify = virtio_notify,
};

static void ipm_callback_process(struct k_work *work)
{
	virtqueue_notification(vq[VIRTQUEUE_ID]);
}

static void ipm_callback(const struct device *dev,
						void *context, uint32_t id,
						volatile void *data)
{
	(void)dev;

	LOG_DBG("Got callback of id %u", id);
	/* TODO: Separate workqueue is needed only
	 * for serialization master (app core)
	 *
	 * Use sysworkq to optimize memory footprint
	 * for serialization slave (net core)
	 */
	k_work_submit_to_queue(&ipm_work_q, &ipm_work);
}

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
	int32_t                  err;
	struct metal_init_params metal_params = METAL_INIT_DEFAULTS;
	struct metal_device     *device;

	(void)dev;

	LOG_DBG("RPMsg service initialization start");

	/* Start IPM workqueue */
	k_work_q_start(&ipm_work_q, ipm_stack_area,
			   K_THREAD_STACK_SIZEOF(ipm_stack_area),
			   IPM_WORK_QUEUE_PRIORITY);
	k_thread_name_set(&ipm_work_q.thread, "ipm_work_q");

	/* Setup IPM workqueue item */
	k_work_init(&ipm_work, ipm_callback_process);

	/* Libmetal setup */
	err = metal_init(&metal_params);
	if (err) {
		LOG_ERR("metal_init: failed - error code %d", err);
		return err;
	}

	err = metal_register_generic_device(&shm_device);
	if (err) {
		LOG_ERR("Couldn't register shared memory device: %d", err);
		return err;
	}

	err = metal_device_open("generic", SHM_DEVICE_NAME, &device);
	if (err) {
		LOG_ERR("metal_device_open failed: %d", err);
		return err;
	}

	io = metal_device_io_region(device, 0);
	if (!io) {
		LOG_ERR("metal_device_io_region failed to get region");
		return err;
	}

	/* IPM setup */
#if IPC_MASTER
	ipm_tx_handle = device_get_binding("IPM_0");
	ipm_rx_handle = device_get_binding("IPM_1");
#else
	ipm_rx_handle = device_get_binding("IPM_0");
	ipm_tx_handle = device_get_binding("IPM_1");
#endif

	if (!ipm_tx_handle) {
		LOG_ERR("Could not get TX IPM device handle");
		return -ENODEV;
	}

	if (!ipm_rx_handle) {
		LOG_ERR("Could not get RX IPM device handle");
		return -ENODEV;
	}

	ipm_register_callback(ipm_rx_handle, ipm_callback, NULL);

	/* Virtqueue setup */
	vq[0] = virtqueue_allocate(VRING_SIZE);
	if (!vq[0]) {
		LOG_ERR("virtqueue_allocate failed to alloc vq[0]");
		return -ENOMEM;
	}

	vq[1] = virtqueue_allocate(VRING_SIZE);
	if (!vq[1]) {
		LOG_ERR("virtqueue_allocate failed to alloc vq[1]");
		return -ENOMEM;
	}

	rvrings[0].io = io;
	rvrings[0].info.vaddr = (void *)VRING_TX_ADDRESS;
	rvrings[0].info.num_descs = VRING_SIZE;
	rvrings[0].info.align = VRING_ALIGNMENT;
	rvrings[0].vq = vq[0];

	rvrings[1].io = io;
	rvrings[1].info.vaddr = (void *)VRING_RX_ADDRESS;
	rvrings[1].info.num_descs = VRING_SIZE;
	rvrings[1].info.align = VRING_ALIGNMENT;
	rvrings[1].vq = vq[1];

	vdev.role = RPMSG_ROLE;
	vdev.vrings_num = VRING_COUNT;
	vdev.func = &dispatch;
	vdev.vrings_info = &rvrings[0];

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
