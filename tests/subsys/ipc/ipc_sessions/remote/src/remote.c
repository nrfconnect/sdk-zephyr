/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/ipc/ipc_service.h>
#include <zephyr/drivers/watchdog.h>
#include <zephyr/sys/reboot.h>

#include <test_commands.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(remote, LOG_LEVEL_INF);

#define IPC_TEST_EV_REBOND 0x01
#define IPC_TEST_EV_BOND   0x02

static const struct device *ipc0_instance = DEVICE_DT_GET(DT_NODELABEL(ipc0));
static volatile bool ipc0_bounded;
K_SEM_DEFINE(bound_sem, 0, 1);
K_EVENT_DEFINE(ipc_ev_req);

static struct k_timer timer_reboot;
static struct k_timer timer_rebond;

static void ep_bound(void *priv);
static void ep_unbound(void *priv);
static void ep_recv(const void *data, size_t len, void *priv);
static void ep_error(const char *message, void *priv);

static struct ipc_ept_cfg ep_cfg = {
	.cb = {
		.bound = ep_bound,
		.unbound = ep_unbound,
		.received = ep_recv,
		.error = ep_error
	},
};

/**
 * @brief Trying to reset by WDT
 *
 * @note If this function return, it means it fails
 */
static int reboot_by_wdt(void)
{
	int err;
	static const struct device *const wdt =
		COND_CODE_1(DT_NODE_HAS_STATUS_OKAY(DT_ALIAS(watchdog0)),
			    (DEVICE_DT_GET(DT_ALIAS(watchdog0))), (NULL));
	static const struct wdt_timeout_cfg m_cfg_wdt = {
		.callback = NULL,
		.flags = WDT_FLAG_RESET_SOC,
		.window.max = 10,
	};
	static const uint8_t wdt_options[] = {
		WDT_OPT_PAUSE_HALTED_BY_DBG | WDT_OPT_PAUSE_IN_SLEEP,
		WDT_OPT_PAUSE_IN_SLEEP,
		0
	};

	if (!wdt) {
		return -ENOTSUP;
	}

	if (!device_is_ready(wdt)) {
		LOG_ERR("WDT device is not ready");
		return -EIO;
	}

	err = wdt_install_timeout(wdt, &m_cfg_wdt);
	if (err < 0) {
		LOG_ERR("WDT install error");
		return -EIO;
	}

	for (size_t i = 0; i < ARRAY_SIZE(wdt_options); ++i) {
		err = wdt_setup(wdt, wdt_options[i]);
		if (err < 0) {
			LOG_ERR("Failed WDT setup with options = %u", wdt_options[i]);
		} else {
			/* We are ok with the configuration:
			 * just wait for the WDT to trigger
			 */
			for (;;) {
				k_cpu_idle();
			}
		}
	}

	return -EIO;
}

/**
 * @brief Just force to reboot, anyway you find possible
 */
FUNC_NORETURN static void reboot_anyway(void)
{
	reboot_by_wdt();
	/* If WDT restart fails - try another way */
	sys_reboot(SYS_REBOOT_COLD);
}

static void ep_bound(void *priv)
{
	ipc0_bounded = true;
	k_sem_give(&bound_sem);

	LOG_INF("Endpoint bounded");
}

static void ep_unbound(void *priv)
{
	ipc0_bounded = false;
	k_sem_give(&bound_sem);

	LOG_INF("Endpoint unbounded");

	/* Try to restore the connection */
	k_event_set(&ipc_ev_req, IPC_TEST_EV_BOND);
}

static void ep_recv(const void *data, size_t len, void *priv)
{
	int ret;
	const struct ipc_test_cmd *cmd = data;
	struct ipc_ept *ep = priv;

	if (len < sizeof(struct ipc_test_cmd)) {
		LOG_ERR("The unexpected size of received data: %u < %u", len, sizeof(struct ipc_test_cmd));
		/* Dropping further processing */
		return;
	}

	LOG_INF("Command received: %u", cmd->cmd);

	switch (cmd->cmd) {
	case IPC_TEST_CMD_NONE:
		LOG_INF("Command processing: NONE");
		/* Ignore */
		break;
	case IPC_TEST_CMD_PING: {
		LOG_INF("Command processing: PING");

		static const struct ipc_test_cmd cmd_pong = {IPC_TEST_CMD_PONG};

		ret = ipc_service_send(ep, &cmd_pong, sizeof(cmd_pong));
		if (ret < 0) {
			LOG_ERR("PONG response failed: %d", ret);
		}
		break;
	}
	case IPC_TEST_CMD_ECHO: {
		LOG_INF("Command processing: ECHO");

		struct ipc_test_cmd *cmd_rsp = k_malloc(len);

		if (!cmd_rsp) {
			LOG_ERR("ECHO response failed: memory allocation");
			break;
		}

		cmd_rsp->cmd = IPC_TEST_CMD_ECHO_RSP;
		memcpy(cmd_rsp->data, cmd->data, len - sizeof(struct ipc_test_cmd));
		ret = ipc_service_send(ep, cmd_rsp, len);
		k_free(cmd_rsp);
		if (ret < 0) {
			LOG_ERR("ECHO response failed: %d", ret);
		}
		break;
	}
	case IPC_TEST_CMD_REBOND: {
		LOG_INF("Command processing: REBOOT");

		struct ipc_test_cmd_rebond *cmd_rebond = (struct ipc_test_cmd_rebond *)cmd;

		k_timer_start(&timer_rebond, K_MSEC(cmd_rebond->timeout_ms), K_FOREVER);
		break;
	}
	case IPC_TEST_CMD_REBOOT: {
		LOG_INF("Command processing: REBOOT");

		struct ipc_test_cmd_reboot *cmd_reboot = (struct ipc_test_cmd_reboot *)cmd;

		k_timer_start(&timer_reboot, K_MSEC(cmd_reboot->timeout_ms), K_FOREVER);
		break;
	}
	default:
		LOG_ERR("Unhandled command: %u", cmd->cmd);
		break;
	}
}

static void ep_error(const char *message, void *priv)
{
	LOG_ERR("EP error: \"%s\"", message);
}

static int init_ipc(void)
{
	int ret;
	static struct ipc_ept ep;

	/* Store the pointer to the endpoint */
	ep_cfg.priv = &ep;

	LOG_INF("IPC-sessions test remote started");

	ret = ipc_service_open_instance(ipc0_instance);
	if ((ret < 0) && (ret != -EALREADY)) {
		LOG_ERR("ipc_service_open_instance() failure: %d", ret);
		return ret;
	}

	ret = ipc_service_register_endpoint(ipc0_instance, &ep, &ep_cfg);
	if (ret < 0) {
		LOG_ERR("ipc_service_register_endpoint() failure: %d", ret);
		return ret;
	}

	do {
		k_sem_take(&bound_sem, K_FOREVER);
	} while (!ipc0_bounded);

	LOG_INF("IPC connection estabilished");

	return 0;
}

static void timer_rebond_cb(struct k_timer *timer)
{
	(void)timer;
	LOG_INF("Setting rebond request");
	k_event_set(&ipc_ev_req, IPC_TEST_EV_REBOND);
}

static void timer_reboot_cb(struct k_timer *timer)
{
	(void)timer;
	LOG_INF("Resetting CPU");
	reboot_anyway();
	__ASSERT(0, "Still working after reboot request");
}


int main(void)
{
	int ret;

	k_timer_init(&timer_rebond, timer_rebond_cb, NULL);
	k_timer_init(&timer_reboot, timer_reboot_cb, NULL);
	ret = init_ipc();
	if (ret) {
		return ret;
	}

	while (1) {
		uint32_t ev;

		ev = k_event_wait(&ipc_ev_req, ~0U, false, K_FOREVER);
		k_event_clear(&ipc_ev_req, ev);

		if (ev & IPC_TEST_EV_REBOND) {
			/* Rebond now */
			ret = ipc_service_deregister_endpoint(ep_cfg.priv);
			if (ret) {
				LOG_ERR("ipc_service_deregister_endpoint() failure: %d", ret);
				continue;
			}
			ipc0_bounded = false;

			ret = ipc_service_register_endpoint(ipc0_instance, ep_cfg.priv, &ep_cfg);
			if (ret < 0) {
				LOG_ERR("ipc_service_register_endpoint() failure: %d", ret);
				return ret;
			}

			do {
				k_sem_take(&bound_sem, K_FOREVER);
			} while (!ipc0_bounded);
		}
		if (ev & IPC_TEST_EV_BOND) {
			LOG_INF("Bonding endpoint");
			/* Bond missing endpoint */
			if (!ipc0_bounded) {
				ret = ipc_service_register_endpoint(ipc0_instance, ep_cfg.priv, &ep_cfg);
				if (ret < 0) {
					LOG_ERR("ipc_service_register_endpoint() failure: %d", ret);
					return ret;
				}

				do {
					k_sem_take(&bound_sem, K_FOREVER);
				} while (!ipc0_bounded);
			}
			LOG_INF("Bonding done");
		}


	}

	return 0;
}
