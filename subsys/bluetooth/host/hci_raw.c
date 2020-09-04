/* hci_userchan.c - HCI user channel Bluetooth handling */

/*
 * Copyright (c) 2015-2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <sys/atomic.h>
#include <sys/byteorder.h>

#include <drivers/bluetooth/hci_driver.h>
#include <bluetooth/hci_raw.h>
#include <bluetooth/l2cap.h>

#define BT_DBG_ENABLED IS_ENABLED(CONFIG_BT_DEBUG_HCI_CORE)
#define LOG_MODULE_NAME bt_hci_raw
#include "common/log.h"

#include "hci_ecc.h"
#include "monitor.h"
#include "hci_raw_internal.h"

#define H4_CMD 0x01
#define H4_ACL 0x02
#define H4_SCO 0x03
#define H4_EVT 0x04

static struct k_fifo *raw_rx;

#if defined(CONFIG_BT_HCI_RAW_H4_ENABLE)
static u8_t raw_mode = BT_HCI_RAW_MODE_H4;
#else
static u8_t raw_mode;
#endif

NET_BUF_POOL_FIXED_DEFINE(hci_rx_pool, CONFIG_BT_RX_BUF_COUNT,
			  BT_BUF_RX_SIZE, NULL);
NET_BUF_POOL_FIXED_DEFINE(hci_cmd_pool, CONFIG_BT_HCI_CMD_COUNT,
			  BT_BUF_RX_SIZE, NULL);
NET_BUF_POOL_FIXED_DEFINE(hci_acl_pool, BT_HCI_ACL_COUNT,
			  BT_BUF_ACL_SIZE, NULL);

struct bt_dev_raw bt_dev;
struct bt_hci_raw_cmd_ext *cmd_ext;
static size_t cmd_ext_size;

int bt_hci_driver_register(const struct bt_hci_driver *drv)
{
	if (bt_dev.drv) {
		return -EALREADY;
	}

	if (!drv->open || !drv->send) {
		return -EINVAL;
	}

	bt_dev.drv = drv;

	BT_DBG("Registered %s", drv->name ? drv->name : "");

	bt_monitor_new_index(BT_MONITOR_TYPE_PRIMARY, drv->bus,
			     BT_ADDR_ANY, drv->name ? drv->name : "bt0");

	return 0;
}

struct net_buf *bt_buf_get_rx(enum bt_buf_type type, k_timeout_t timeout)
{
	struct net_buf *buf;

	switch (type) {
	case BT_BUF_EVT:
	case BT_BUF_ACL_IN:
		break;
	default:
		BT_ERR("Invalid type: %u", type);
		return NULL;
	}

	buf = net_buf_alloc(&hci_rx_pool, timeout);
	if (!buf) {
		return buf;
	}

	net_buf_reserve(buf, BT_BUF_RESERVE);
	bt_buf_set_type(buf, type);

	return buf;
}

struct net_buf *bt_buf_get_tx(enum bt_buf_type type, k_timeout_t timeout,
			      const void *data, size_t size)
{
	struct net_buf_pool *pool;
	struct net_buf *buf;

	switch (type) {
	case BT_BUF_CMD:
		pool = &hci_cmd_pool;
		break;
	case BT_BUF_ACL_OUT:
		pool = &hci_acl_pool;
		break;
	case BT_BUF_H4:
		if (IS_ENABLED(CONFIG_BT_HCI_RAW_H4) &&
		    raw_mode == BT_HCI_RAW_MODE_H4) {
			switch (((u8_t *)data)[0]) {
			case H4_CMD:
				type = BT_BUF_CMD;
				pool = &hci_cmd_pool;
				break;
			case H4_ACL:
				type = BT_BUF_ACL_OUT;
				pool = &hci_acl_pool;
				break;
			default:
				LOG_ERR("Unknown H4 type %u", type);
				return NULL;
			}

			/* Adjust data pointer to discard the header */
			data = (u8_t *)data + 1;
			size--;
			break;
		}
	/* Fallthrough */
	default:
		BT_ERR("Invalid type: %u", type);
		return NULL;
	}

	buf = net_buf_alloc(pool, timeout);
	if (!buf) {
		return buf;
	}

	net_buf_reserve(buf, BT_BUF_RESERVE);
	bt_buf_set_type(buf, type);

	if (data && size) {
		net_buf_add_mem(buf, data, size);
	}

	return buf;
}

struct net_buf *bt_buf_get_cmd_complete(k_timeout_t timeout)
{
	return bt_buf_get_rx(BT_BUF_EVT, timeout);
}

struct net_buf *bt_buf_get_evt(u8_t evt, bool discardable, k_timeout_t timeout)
{
	return bt_buf_get_rx(BT_BUF_EVT, timeout);
}

int bt_recv(struct net_buf *buf)
{
	BT_DBG("buf %p len %u", buf, buf->len);

	bt_monitor_send(bt_monitor_opcode(buf), buf->data, buf->len);

	if (IS_ENABLED(CONFIG_BT_HCI_RAW_H4) &&
	    raw_mode == BT_HCI_RAW_MODE_H4) {
		switch (bt_buf_get_type(buf)) {
		case BT_BUF_EVT:
			net_buf_push_u8(buf, H4_EVT);
			break;
		case BT_BUF_ACL_IN:
			net_buf_push_u8(buf, H4_ACL);
			break;
		default:
			BT_ERR("Unknown type %u", bt_buf_get_type(buf));
			return -EINVAL;
		}
	}

	/* Queue to RAW rx queue */
	net_buf_put(raw_rx, buf);

	return 0;
}

int bt_recv_prio(struct net_buf *buf)
{
	if (bt_buf_get_type(buf) == BT_BUF_EVT) {
		struct bt_hci_evt_hdr *hdr = (void *)buf->data;
		u8_t evt_flags = bt_hci_evt_get_flags(hdr->evt);

		if ((evt_flags & BT_HCI_EVT_FLAG_RECV_PRIO) &&
		    (evt_flags & BT_HCI_EVT_FLAG_RECV)) {
			/* Avoid queuing the event twice */
			return 0;
		}
	}

	return bt_recv(buf);
}

static void bt_cmd_complete_ext(u16_t op, u8_t status)
{
	struct net_buf *buf;
	struct bt_hci_evt_cc_status *cc;

	if (status == BT_HCI_ERR_EXT_HANDLED) {
		return;
	}

	buf = bt_hci_cmd_complete_create(op, sizeof(*cc));
	cc = net_buf_add(buf, sizeof(*cc));
	cc->status = status;

	bt_recv(buf);
}

static u8_t bt_send_ext(struct net_buf *buf)
{
	struct bt_hci_cmd_hdr *hdr;
	struct net_buf_simple_state state;
	int i;
	u16_t op;
	u8_t status;

	status = BT_HCI_ERR_SUCCESS;

	if (!cmd_ext) {
		return status;
	}

	net_buf_simple_save(&buf->b, &state);

	if (buf->len < sizeof(*hdr)) {
		BT_ERR("No HCI Command header");
		return BT_HCI_ERR_INVALID_PARAM;
	}

	hdr = net_buf_pull_mem(buf, sizeof(*hdr));
	if (buf->len < hdr->param_len) {
		BT_ERR("Invalid HCI CMD packet length");
		return BT_HCI_ERR_INVALID_PARAM;
	}

	op = sys_le16_to_cpu(hdr->opcode);

	for (i = 0; i < cmd_ext_size; i++) {
		struct bt_hci_raw_cmd_ext *cmd = &cmd_ext[i];

		if (cmd->op == op) {
			if (buf->len < cmd->min_len) {
				status = BT_HCI_ERR_INVALID_PARAM;
			} else {
				status = cmd->func(buf);
			}

			break;
		}
	}

	if (status) {
		bt_cmd_complete_ext(op, status);
		return status;
	}

	net_buf_simple_restore(&buf->b, &state);

	return status;
}

int bt_send(struct net_buf *buf)
{
	BT_DBG("buf %p len %u", buf, buf->len);

	bt_monitor_send(bt_monitor_opcode(buf), buf->data, buf->len);

	if (IS_ENABLED(CONFIG_BT_HCI_RAW_CMD_EXT) &&
	    bt_buf_get_type(buf) == BT_BUF_CMD) {
		u8_t status;

		status = bt_send_ext(buf);
		if (status) {
			return status;
		}
	}

	if (IS_ENABLED(CONFIG_BT_TINYCRYPT_ECC)) {
		return bt_hci_ecc_send(buf);
	}

	return bt_dev.drv->send(buf);
}

int bt_hci_raw_set_mode(u8_t mode)
{
	BT_DBG("mode %u", mode);

	if (IS_ENABLED(CONFIG_BT_HCI_RAW_H4)) {
		switch (mode) {
		case BT_HCI_RAW_MODE_PASSTHROUGH:
		case BT_HCI_RAW_MODE_H4:
			raw_mode = mode;
			return 0;
		}
	}

	return -EINVAL;
}

u8_t bt_hci_raw_get_mode(void)
{
	if (IS_ENABLED(CONFIG_BT_HCI_RAW_H4)) {
		return raw_mode;
	}

	return BT_HCI_RAW_MODE_PASSTHROUGH;
}

void bt_hci_raw_cmd_ext_register(struct bt_hci_raw_cmd_ext *cmds, size_t size)
{
	if (IS_ENABLED(CONFIG_BT_HCI_RAW_CMD_EXT)) {
		cmd_ext = cmds;
		cmd_ext_size = size;
	}
}

int bt_enable_raw(struct k_fifo *rx_queue)
{
	const struct bt_hci_driver *drv = bt_dev.drv;
	int err;

	BT_DBG("");

	raw_rx = rx_queue;

	if (!bt_dev.drv) {
		BT_ERR("No HCI driver registered");
		return -ENODEV;
	}

	if (IS_ENABLED(CONFIG_BT_TINYCRYPT_ECC)) {
		bt_hci_ecc_init();
	}

	err = drv->open();
	if (err) {
		BT_ERR("HCI driver open failed (%d)", err);
		return err;
	}

	BT_INFO("Bluetooth enabled in RAW mode");

	return 0;
}
