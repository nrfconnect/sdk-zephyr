/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "bs_bt_utils.h"

BUILD_ASSERT(CONFIG_BT_MAX_PAIRED >= 2, "CONFIG_BT_MAX_PAIRED is too small.");
BUILD_ASSERT(CONFIG_BT_ID_MAX >= 3, "CONFIG_BT_ID_MAX is too small.");

DEFINE_FLAG(flag_is_connected);

static sys_slist_t g_link_list = SYS_SLIST_STATIC_INIT(&g_link_list);

bool g_is_central;

const char *addr_to_str(const bt_addr_le_t *addr)
{
	static char str[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(addr, str, sizeof(str));

	return str;
}

void link_ctx_init(struct link_ctx *ctx)
{
	TEST_ASSERT(ctx->node.next == NULL,
		    "%s called on an already initialized context.", __func__);
	sys_slist_append(&g_link_list, &ctx->node);

	UNSET_FLAG(ctx->connected);
	UNSET_FLAG(ctx->pairing_complete);
	UNSET_FLAG(ctx->timedout);
	UNSET_FLAG(ctx->encrypted);
}

#if defined(CONFIG_BT_PERIPHERAL)
static struct link_ctx *get_link_ctx_by_adv(struct bt_le_ext_adv *adv)
{
	struct link_ctx *ctx;

	SYS_SLIST_FOR_EACH_CONTAINER(&g_link_list, ctx, node) {
		if (ctx->adv == adv) {
			return ctx;
		}
	}

	return NULL;
}
#endif /* CONFIG_BT_PERIPHERAL */

static struct link_ctx *get_link_ctx_by_conn(struct bt_conn *conn)
{
	struct link_ctx *ctx;

	SYS_SLIST_FOR_EACH_CONTAINER(&g_link_list, ctx, node) {
		if (ctx->conn == conn) {
			return ctx;
		}
	}

	return NULL;
}

void clear_conn(struct link_ctx *ctx)
{
	TEST_ASSERT(ctx->conn != NULL, "clear_g_conn called with NULL ctx->conn.");
	bt_conn_unref(ctx->conn);
	ctx->conn = NULL;
}

static void connected(struct bt_conn *conn, uint8_t err)
{
	const bt_addr_le_t *dst;
	char addr_str[BT_ADDR_LE_STR_LEN];
	struct link_ctx *ctx;

	dst = bt_conn_get_dst(conn);
	bt_addr_le_to_str(dst, addr_str, sizeof(addr_str));
	printk("Connected to %p, err %d dst %s\n", conn, err, addr_str);

	if (!g_is_central) {
		/* The connection for Peripheral will be handled in the advertiser connected
		 * callback, so we don't need to do anything here.
		 */
		TEST_ASSERT(err == 0 || err == BT_HCI_ERR_ADV_TIMEOUT,
			    "Failed to connect (err %d)", err);
		return;
	}

	TEST_ASSERT(err == 0, "Failed to connect (err %d)", err);

	ctx = get_link_ctx_by_conn(conn);
	TEST_ASSERT(ctx, "No link context for this connection.");
	TEST_ASSERT(ctx->conn == conn, "Connected to unexpected connection.");

	UNSET_FLAG(ctx->encrypted);
	SET_FLAG(ctx->connected);
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	const bt_addr_le_t *dst;
	char addr_str[BT_ADDR_LE_STR_LEN];
	struct link_ctx *ctx;

	dst = bt_conn_get_dst(conn);
	bt_addr_le_to_str(dst, addr_str, sizeof(addr_str));
	printk("Disconnected from %p (addr %s) (reason %d)\n", conn, addr_str, reason);

	ctx = get_link_ctx_by_conn(conn);
	TEST_ASSERT(ctx, "No link context for this connection.");
	TEST_ASSERT(ctx->conn == conn, "Disconnected from unexpected connection.");

	if (!g_is_central) {
		UNSET_FLAG(ctx->timedout);
	}

	UNSET_FLAG(ctx->connected);
}

BUILD_ASSERT(CONFIG_BT_MAX_CONN >= 2, "This test assumes a multi link.");

static void security_changed(struct bt_conn *conn, bt_security_t level,
			     enum bt_security_err err)
{
	const bt_addr_le_t *dst;
	char addr_str[BT_ADDR_LE_STR_LEN];
	struct link_ctx *ctx;

	dst = bt_conn_get_dst(conn);
	bt_addr_le_to_str(dst, addr_str, sizeof(addr_str));
	printk("Security changed for %p (addr %s), level %d, err %d\n", conn, addr_str, level, err);

	ctx = get_link_ctx_by_conn(conn);
	TEST_ASSERT(ctx, "No link context for this connection.");
	TEST_ASSERT(ctx->conn == conn, "Security changed for unexpected connection.");

	if (!err && level == BT_SECURITY_L2) {
		SET_FLAG(ctx->encrypted);
	}
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
	.connected = connected,
	.disconnected = disconnected,
	.security_changed = security_changed,
};

static void pairing_failed(struct bt_conn *conn, enum bt_security_err reason)
{
	printk("Pairing failed with %p (reason %d)\n", conn, reason);

	/* Pairing should not fail in this test, so we assert here. */
	TEST_ASSERT(false, "Pairing failed with %p (reason %d)", conn, reason);
}

static void pairing_complete(struct bt_conn *conn, bool bonded)
{
	const bt_addr_le_t *dst;
	char addr_str[BT_ADDR_LE_STR_LEN];
	struct link_ctx *ctx;

	dst = bt_conn_get_dst(conn);
	bt_addr_le_to_str(dst, addr_str, sizeof(addr_str));
	printk("Pairing complete with %p (bonded %d) dst %s\n", conn, bonded, addr_str);

	ctx = get_link_ctx_by_conn(conn);
	TEST_ASSERT(ctx, "No link context for this connection.");

	SET_FLAG(ctx->pairing_complete);
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
static struct link_ctx *scanning_ctx;

static void scan_connect_to_first_result__device_found(const bt_addr_le_t *addr, int8_t rssi,
						       uint8_t type, struct net_buf_simple *ad)
{
	char addr_str[BT_ADDR_LE_STR_LEN];
	int err;

	if (scanning_ctx == NULL) {
		/* Not scanning, ignore the advertisement. */
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

	err = bt_conn_le_create(addr, BT_CONN_LE_CREATE_CONN, BT_LE_CONN_PARAM_DEFAULT,
				&scanning_ctx->conn);
	TEST_ASSERT(!err, "Err bt_conn_le_create %d", err);

	scanning_ctx = NULL;
}

int scan_connect_to_first_result(struct link_ctx *ctx, bool directed, bool filter)
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

	TEST_ASSERT(scanning_ctx == NULL, "Already scanning, cannot start a new scan.");
	scanning_ctx = ctx;

	TEST_ASSERT(scanning_ctx->conn == NULL, "%s called on an already initialized context.",
		    __func__);

	return bt_le_scan_start(&param, scan_connect_to_first_result__device_found);
}
#endif /* CONFIG_BT_CENTRAL */

#if defined(CONFIG_BT_PERIPHERAL)
static void adv_sent(struct bt_le_ext_adv *adv, struct bt_le_ext_adv_sent_info *info)
{
	struct link_ctx *ctx;

	ctx = get_link_ctx_by_adv(adv);
	TEST_ASSERT(ctx, "No link context for this advertiser.");

	printk("Advertiser %p sent, num_sent %d\n", adv, info->num_sent);

	SET_FLAG(ctx->timedout);
}

static void adv_connected(struct bt_le_ext_adv *adv, struct bt_le_ext_adv_connected_info *info)
{
	struct link_ctx *ctx;
	const bt_addr_le_t *dst;
	char addr_str[BT_ADDR_LE_STR_LEN];

	dst = bt_conn_get_dst(info->conn);
	bt_addr_le_to_str(dst, addr_str, sizeof(addr_str));
	printk("Connected to %p, dst %s\n", info->conn, addr_str);

	TEST_ASSERT(!g_is_central, "%s called in central role, this should not happen.", __func__);

	ctx = get_link_ctx_by_adv(adv);
	TEST_ASSERT(ctx, "No link context for this advertiser.");

	TEST_ASSERT(!ctx->conn, "Already connected on this advertiser.");
	ctx->conn = bt_conn_ref(info->conn);

	UNSET_FLAG(ctx->encrypted);
	SET_FLAG(ctx->connected);
}

static struct bt_le_ext_adv_cb ext_adv_callbacks = {
	.sent = adv_sent,
	.connected = adv_connected,
};

int advertiser_create(struct link_ctx *ctx, int id, bt_addr_le_t *directed_dst, bool filter)
{
	struct bt_le_adv_param param = {
		.id = id,
		.interval_min = 0x0020,
		.interval_max = 0x4000,
	};

	TEST_ASSERT(ctx->adv == NULL, "%s called on an already initialized context.", __func__);

	param.options |= BT_LE_ADV_OPT_CONN;

	if (filter) {
		param.options |= BT_LE_ADV_OPT_FILTER_CONN;
	}

	if (directed_dst) {
		param.options |= BT_LE_ADV_OPT_DIR_ADDR_RPA;
		param.peer = directed_dst;
	}

	return bt_le_ext_adv_create(&param, &ext_adv_callbacks, &ctx->adv);
}

int advertise_connectable(struct link_ctx *ctx)
{
	struct bt_le_ext_adv_start_param start_param = {
		.timeout = 100,
		.num_events = 0,
	};

	TEST_ASSERT(ctx->adv != NULL,
		    "advertise_connectable_ext called on an uninitialized context.");

	return bt_le_ext_adv_start(ctx->adv, &start_param);
}

void advertiser_destroy(struct link_ctx *ctx)
{
	int err;

	TEST_ASSERT(ctx->adv != NULL, "%s called on an uninitialized context.", __func__);

	err = bt_le_ext_adv_stop(ctx->adv);
	TEST_ASSERT(err == 0, "Failed to stop advertiser (err %d)", err);

	err = bt_le_ext_adv_delete(ctx->adv);
	TEST_ASSERT(err == 0, "Failed to delete advertiser (err %d)", err);

	ctx->adv = NULL;
}
#endif /* CONFIG_BT_PERIPHERAL */
