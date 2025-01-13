
/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include "ipc_if.h"
#include "bal_structs.h"
#include "qspi.h"
#include "hal_structs.h"

/* Define addresses to use for the free queues */
#define EVENT_FREEQ_ADDR        0x20008000
#define CMD_FREEQ_ADDR          0x20005000

#define NUM_INSTANCES 3
#define NUM_ENDPOINTS 1

struct device *ipc_instances[NUM_INSTANCES];
struct ipc_ept ept[NUM_ENDPOINTS];
struct ipc_ept_cfg ept_cfg[NUM_ENDPOINTS];

static WIFI_IPC_T wifi_event;
static WIFI_IPC_T wifi_cmd;
static WIFI_IPC_T wifi_tx;

static int (*callback_func)(void *data);

static void event_recv(void *data, void *priv)
{
	struct nrf_wifi_bus_qspi_dev_ctx *dev_ctx = NULL;
	struct nrf_wifi_bal_dev_ctx *bal_dev_ctx = NULL;
	struct nrf_wifi_hal_dev_ctx *hal_dev_ctx = NULL;

	dev_ctx = (struct nrf_wifi_bus_qspi_dev_ctx *) priv;
	bal_dev_ctx = (struct nrf_wifi_bal_dev_ctx *) dev_ctx->bal_dev_ctx;
	hal_dev_ctx = (struct nrf_wifi_hal_dev_ctx *) bal_dev_ctx->hal_dev_ctx;

	hal_dev_ctx->ipc_msg = data;
	callback_func(priv);
}

int ipc_init() {
	wifi_ipc_host_event_init(&wifi_event, EVENT_FREEQ_ADDR);
	wifi_ipc_host_cmd_init(&wifi_cmd, CMD_FREEQ_ADDR);
	return 0;
}

int ipc_deinit(void) {
	return 0;
}

int ipc_recv(ipc_ctx_t ctx, void *data, int len) {
	return 0;
}

int ipc_send(ipc_ctx_t ctx, const void *data, int len) {

	int ret;

	switch (ctx.inst) {
		case IPC_INSTANCE_CMD_CTRL:
			/* IPC service on RPU may not have been established. Keep trying. */
			do {
				ret = wifi_ipc_host_cmd_send_memcpy(&wifi_cmd, data, len);
			} while (ret == WIFI_IPC_STATUS_BUSYQ_NOTREADY);

			/* Critical error during IPC service transfer. Should never happen. */
			if (ret) {
				return -1;
			}
			break;
		case IPC_INSTANCE_CMD_TX:
			/* IPC service on RPU may not have been established. Keep trying. */
			do {
				ret = wifi_ipc_host_tx_send(&wifi_tx, data);
		   } while (ret == WIFI_IPC_STATUS_BUSYQ_NOTREADY);

			/* Critical error during IPC service transfer. Should never happen. */
			if (ret) {
				return -1;
			}
		case IPC_INSTANCE_RX:
			break;
		default:
			break;
	}

	return 0;
}

int ipc_register_rx_cb(int (*rx_handler)(void *priv), void *data)
{
	int ret;

	callback_func = rx_handler;

	ret = wifi_ipc_bind_ipc_service_tx_rx(&wifi_cmd, &wifi_event, DEVICE_DT_GET(DT_NODELABEL(ipc0)), event_recv, data);
	if (ret != WIFI_IPC_STATUS_OK) {
		return -1;
	}

	ret = wifi_ipc_bind_ipc_service(&wifi_tx, DEVICE_DT_GET(DT_NODELABEL(ipc1)), event_recv, data);
	if (ret != WIFI_IPC_STATUS_OK) {
		return -1;
	}

	return 0;
}
