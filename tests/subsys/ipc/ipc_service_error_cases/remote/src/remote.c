/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/ipc/ipc_service.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(remote, LOG_LEVEL_INF);

static const struct device *ipc_dut = DEVICE_DT_GET(DT_ALIAS(dut_ipc));

static void dut_bound_cb(void *priv)
{
	bool *bound = priv;
	*bound = true;
	// ARG_UNUSED(priv);
	LOG_INF("DUT endpoint bound");
}

static void dut_unbound_cb(void *priv)
{
	ARG_UNUSED(priv);
	LOG_INF("DUT endpoint unbound");
}

static void dut_error_cb(const char *message, void *priv)
{
	ARG_UNUSED(priv);
	LOG_ERR("Endpoint error: %s", message);
}

static void dut_received_cb(const void *data, size_t len, void *priv)
{
	ARG_UNUSED(data);
	ARG_UNUSED(priv);
	LOG_INF("DUT received %u", len);
}

static bool dut_bound_flag = false;

static struct ipc_ept_cfg ept_dut_cfg = {
	.name = "ept0",
	.cb = {
		.bound    = dut_bound_cb,
		.unbound  = dut_unbound_cb,
		.error    = dut_error_cb,
		.received = dut_received_cb,
	},
	.priv     = &dut_bound_flag,
};

int main(void)
{
	int ret;
	struct ipc_ept ept_dut;

	ret = ipc_service_open_instance(ipc_dut);
	if (ret < 0 && ret != -EALREADY) {
		LOG_ERR("ipc_service_open_instance() failed: %d", ret);
	}

	ret = ipc_service_register_endpoint(ipc_dut, &ept_dut, &ept_dut_cfg);
	// if (ret != 0) {
	// 	LOG_ERR("ipc_service_register_endpoint(dut) returned %d while expected was 0",
	// 		ret);
	// }

	ret = ipc_service_deregister_endpoint(&ept_dut);
	// if (ret != 0) {
	// 	LOG_ERR("ipc_service_deregister_endpoint(dut) returned %d while expected was 0",
	// 		ret);
	// }

	ret = ipc_service_close_instance(ipc_dut);
	// if (ret < 0) {
	// 	LOG_ERR("ipc_service_close_instance() failed: %d", ret);
	// }

	return 0;
}
