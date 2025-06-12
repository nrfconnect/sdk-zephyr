/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "host/hci_core.h"

#include "bs_bt_utils.h"

BUILD_ASSERT(CONFIG_BT_MAX_PAIRED >= 3, "Test needs to pair with 3 devices.");
BUILD_ASSERT(CONFIG_BT_ID_MAX >= 4, "Need 4 IDs: 1 default + 3 for the test.");

DEFINE_FLAG(flag_is_connected);

struct bt_conn *g_conn;

const char *addr_to_str(const bt_addr_le_t *addr)
{
	static char str[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(addr, str, sizeof(str));

	return str;
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	const bt_addr_le_t *dst;
	char addr_str[BT_ADDR_LE_STR_LEN];

	dst = bt_conn_get_dst(conn);
	bt_addr_le_to_str(dst, addr_str, sizeof(addr_str));
	printk("Disconnected from %p (addr %s) (reason %d)\n", conn, addr_str, reason);

	UNSET_FLAG(flag_is_connected);
}

BUILD_ASSERT(CONFIG_BT_MAX_CONN == 1, "This test assumes a single link.");
static void connected(struct bt_conn *conn, uint8_t err)
{
	const bt_addr_le_t *dst;
	char addr_str[BT_ADDR_LE_STR_LEN];

	TEST_ASSERT((!g_conn || (conn == g_conn)), "Unexpected new connection.");

	UNSET_FLAG(security_changed_flag);

	dst = bt_conn_get_dst(conn);
	bt_addr_le_to_str(dst, addr_str, sizeof(addr_str));
	printk("Connected to %p, err %d dst %s\n", conn, err, addr_str);

	if (!g_conn) {
		g_conn = bt_conn_ref(conn);
	}

	if (err != 0) {
		clear_g_conn();
		return;
	}

	printk("Set flag_is_connected\n");
	SET_FLAG(flag_is_connected);
}

DEFINE_FLAG(security_changed_flag);

static void security_changed(struct bt_conn *conn, bt_security_t level,
			     enum bt_security_err err)
{
	const bt_addr_le_t *dst;
	char addr_str[BT_ADDR_LE_STR_LEN];

	dst = bt_conn_get_dst(conn);
	bt_addr_le_to_str(dst, addr_str, sizeof(addr_str));
	printk("Security changed for %p (addr %s), level %d, err %d\n", conn, addr_str, level, err);

	if (!err && level == BT_SECURITY_L2) {
		SET_FLAG(security_changed_flag);
	}
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
	.connected = connected,
	.disconnected = disconnected,
	.security_changed = security_changed,
};

void clear_g_conn(void)
{
	struct bt_conn *conn;

	conn = g_conn;
	g_conn = NULL;
	TEST_ASSERT(conn, "Test error: No g_conn!");
	bt_conn_unref(conn);
}

/* The pairing complete flags is raised by events and lowered by test code. */
DEFINE_FLAG(flag_pairing_complete);

static void pairing_failed(struct bt_conn *conn, enum bt_security_err reason)
{
	printk("Pairing failed with %p (reason %d)\n", conn, reason);

	/* Pairing should never fail in this test. */
	TEST_ASSERT(false, "Pairing failed with %p (reason %d)", conn, reason);
}

static void pairing_complete(struct bt_conn *conn, bool bonded)
{
	const bt_addr_le_t *dst;
	char addr_str[BT_ADDR_LE_STR_LEN];

	dst = bt_conn_get_dst(conn);
	bt_addr_le_to_str(dst, addr_str, sizeof(addr_str));
	printk("Pairing complete with %p (bonded %d) dst %s\n", conn, bonded, addr_str);

	SET_FLAG(flag_pairing_complete);
}

static struct bt_conn_auth_info_cb bt_conn_auth_info_cb = {
	.pairing_failed = pairing_failed,
	.pairing_complete = pairing_complete,
};

void bs_bt_utils_setup(void)
{
	int err;

	err = bt_enable(NULL);
	TEST_ASSERT(!err, "bt_enable failed.");
	err = bt_conn_auth_info_cb_register(&bt_conn_auth_info_cb);
	TEST_ASSERT(!err, "bt_conn_auth_info_cb_register failed.");
}

#if defined(CONFIG_BT_CENTRAL)
static enum bt_gap_adv_type scan_adv_type;

static void scan_connect_to_first_result__device_found(const bt_addr_le_t *addr, int8_t rssi,
						       uint8_t type, struct net_buf_simple *ad)
{
	char addr_str[BT_ADDR_LE_STR_LEN];
	int err;

	if (g_conn != NULL) {
		printk("Already connected, ignoring advertisement.");
		return;
	}

	/* We're only interested in connectable events */
	if (type != scan_adv_type) {
		TEST_FAIL("Unexpected advertisement type %d, expected %d", type, scan_adv_type);
	}

	bt_addr_le_to_str(addr, addr_str, sizeof(addr_str));
	printk("Got scan result, connecting.. dst %s, RSSI %d\n", addr_str, rssi);

	err = bt_le_scan_stop();
	TEST_ASSERT(!err, "Err bt_le_scan_stop %d", err);

	err = bt_conn_le_create(addr, BT_CONN_LE_CREATE_CONN, BT_LE_CONN_PARAM_DEFAULT, &g_conn);
	TEST_ASSERT(!err, "Err bt_conn_le_create %d", err);
}

int scan_connect_to_first_result(bool directed, bool filter)
{
	struct bt_le_scan_param param = BT_LE_SCAN_PARAM_INIT(BT_LE_SCAN_TYPE_PASSIVE,
							      BT_LE_SCAN_OPT_FILTER_DUPLICATE,
							      BT_GAP_SCAN_FAST_INTERVAL,
							      BT_GAP_SCAN_FAST_WINDOW);

	if (filter) {
		param.options = BT_LE_SCAN_OPT_FILTER_ACCEPT_LIST;
	}

	if (directed) {
		scan_adv_type = BT_GAP_ADV_TYPE_ADV_DIRECT_IND;
	} else {
		scan_adv_type = BT_GAP_ADV_TYPE_ADV_IND;
	}

	return bt_le_scan_start(&param, scan_connect_to_first_result__device_found);
}
#endif /* CONFIG_BT_CENTRAL */

#if defined(CONFIG_BT_PERIPHERAL)
int advertise_connectable(int id, bt_addr_le_t *directed_dst, bool filter)
{
	struct bt_le_adv_param param = {};

	param.id = id;
	param.interval_min = 0x0020;
	param.interval_max = 0x4000;
	param.options |= BT_LE_ADV_OPT_CONN;

	if (filter) {
		param.options |= BT_LE_ADV_OPT_FILTER_CONN;
	}

	if (directed_dst) {
		param.options |= BT_LE_ADV_OPT_DIR_ADDR_RPA;
		param.peer = directed_dst;
	}

	return bt_le_adv_start(&param, NULL, 0, NULL, 0);
}
#endif /* CONFIG_BT_PERIPHERAL */
