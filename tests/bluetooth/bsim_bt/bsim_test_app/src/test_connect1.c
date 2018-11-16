/*
 * Copyright (c) 2015-2016 Intel Corporation
 * Copyright (c) 2017-2018 Oticon A/S
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "kernel.h"

#include "bs_types.h"
#include "bs_tracing.h"
#include "time_machine.h"
#include "bstests.h"

#include <zephyr/types.h>
#include <stddef.h>
#include <errno.h>
#include <zephyr.h>
#include <misc/printk.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/conn.h>
#include <bluetooth/uuid.h>
#include <bluetooth/gatt.h>
#include <misc/byteorder.h>

static struct bt_conn *default_conn;

static struct bt_uuid_16 uuid = BT_UUID_INIT_16(0);
static struct bt_gatt_discover_params discover_params;
static struct bt_gatt_subscribe_params subscribe_params;

static bool encrypt_link;
/*
 * Basic connection test:
 *   We expect to find a connectable peripheral (a Zephyr's
 *   samples/bluetooth/peripheral) to which we will connect.
 *
 *   After connecting we expect to receive some notification.
 *   If we do, the test case passes.
 *   If we do not in 5 seconds, the testcase is considered failed
 *
 *   The thread code is mostly a copy of the central_hr sample device
 */

#define WAIT_TIME 5 /*seconds*/
extern enum bst_result_t bst_result;

static void test_con1_init(void)
{
	bst_ticker_set_next_tick_absolute(WAIT_TIME*1e6);
	bst_result = In_progress;
}

static void test_con_encrypted_init(void)
{
	encrypt_link = true;
	test_con1_init();
}

static void test_con1_tick(bs_time_t HW_device_time)
{
	/*
	 * If in WAIT_TIME seconds the testcase did not already pass
	 * (and finish) we consider it failed
	 */
	bst_result = Failed;
	bs_trace_error_line("test: connect1 failed (no notification received "
			    "after %i seconds)\n", WAIT_TIME);
}

static u8_t notify_func(struct bt_conn *conn,
			struct bt_gatt_subscribe_params *params,
			const void *data, u16_t length)
{
	if (!data) {
		printk("[UNSUBSCRIBED]\n");
		params->value_handle = 0;
		return BT_GATT_ITER_STOP;
	}

	printk("[NOTIFICATION] data %p length %u\n", data, length);

	/* We have passed*/
	bst_result = Passed;
	bs_trace_exit_time("Testcase passed\n");
	/**/

	return BT_GATT_ITER_CONTINUE;
}

static u8_t discover_func(struct bt_conn *conn,
		const struct bt_gatt_attr *attr,
		struct bt_gatt_discover_params *params)
{
	int err;

	if (!attr) {
		printk("Discover complete\n");
		memset(params, 0, sizeof(*params));
		return BT_GATT_ITER_STOP;
	}

	printk("[ATTRIBUTE] handle %u\n", attr->handle);

	if (!bt_uuid_cmp(discover_params.uuid, BT_UUID_HRS)) {
		memcpy(&uuid, BT_UUID_HRS_MEASUREMENT, sizeof(uuid));
		discover_params.uuid = &uuid.uuid;
		discover_params.start_handle = attr->handle + 1;
		discover_params.type = BT_GATT_DISCOVER_CHARACTERISTIC;

		err = bt_gatt_discover(conn, &discover_params);
		if (err) {
			printk("Discover failed (err %d)\n", err);
		}
	} else if (!bt_uuid_cmp(discover_params.uuid,
			BT_UUID_HRS_MEASUREMENT)) {
		memcpy(&uuid, BT_UUID_GATT_CCC, sizeof(uuid));
		discover_params.uuid = &uuid.uuid;
		discover_params.start_handle = attr->handle + 2;
		discover_params.type = BT_GATT_DISCOVER_DESCRIPTOR;
		subscribe_params.value_handle = attr->handle + 1;

		err = bt_gatt_discover(conn, &discover_params);
		if (err) {
			printk("Discover failed (err %d)\n", err);
		}
	} else {
		subscribe_params.notify = notify_func;
		subscribe_params.value = BT_GATT_CCC_NOTIFY;
		subscribe_params.ccc_handle = attr->handle;

		err = bt_gatt_subscribe(conn, &subscribe_params);
		if (err && err != -EALREADY) {
			printk("Subscribe failed (err %d)\n", err);
		} else {
			printk("[SUBSCRIBED]\n");
		}

		return BT_GATT_ITER_STOP;
	}

	return BT_GATT_ITER_STOP;
}

static void connected(struct bt_conn *conn, u8_t conn_err)
{
	char addr[BT_ADDR_LE_STR_LEN];
	int err;

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	if (conn_err) {
		printk("Failed to connect to %s (%u)\n", addr, conn_err);
		return;
	}

	printk("Connected: %s\n", addr);

	if (conn != default_conn) {
		return;
	}

	if (encrypt_link) {
		k_sleep(500);
		bt_conn_auth_cb_register(NULL);
		err = bt_conn_security(conn, BT_SECURITY_MEDIUM);
		if (err) {
			printk("bt_conn_security failed (err %d)\n", err);
			return;
		}
	}

	memcpy(&uuid, BT_UUID_HRS, sizeof(uuid));
	discover_params.uuid = &uuid.uuid;
	discover_params.func = discover_func;
	discover_params.start_handle = 0x0001;
	discover_params.end_handle = 0xffff;
	discover_params.type = BT_GATT_DISCOVER_PRIMARY;

	err = bt_gatt_discover(default_conn, &discover_params);
	if (err) {
		printk("Discover failed(err %d)\n", err);
		return;
	}
}

static bool eir_found(u8_t type, const u8_t *data, u8_t data_len,
		void *user_data)
{
	bt_addr_le_t *addr = user_data;
	int i;

	printk("[AD]: %u data_len %u\n", type, data_len);

	switch (type) {
	case BT_DATA_UUID16_SOME:
	case BT_DATA_UUID16_ALL:
		if (data_len % sizeof(u16_t) != 0) {
			printk("AD malformed\n");
			return true;
		}

		for (i = 0; i < data_len; i += sizeof(u16_t)) {
			struct bt_uuid *uuid;
			u16_t u16;
			int err;

			memcpy(&u16, &data[i], sizeof(u16));
			uuid = BT_UUID_DECLARE_16(sys_le16_to_cpu(u16));
			if (bt_uuid_cmp(uuid, BT_UUID_HRS)) {
				continue;
			}

			err = bt_le_scan_stop();
			if (err) {
				printk("Stop LE scan failed (err %d)\n", err);
				continue;
			}

			default_conn = bt_conn_create_le(addr,
					BT_LE_CONN_PARAM_DEFAULT);
			return false;
		}
	}

	return true;
}

static void ad_parse(struct net_buf_simple *ad,
		bool (*func)(u8_t type, const u8_t *data,
				u8_t data_len, void *user_data),
				void *user_data)
{
	while (ad->len > 1) {
		u8_t len = net_buf_simple_pull_u8(ad);
		u8_t type;

		/* Check for early termination */
		if (len == 0) {
			return;
		}

		if (len > ad->len) {
			printk("AD malformed\n");
			return;
		}

		type = net_buf_simple_pull_u8(ad);

		if (!func(type, ad->data, len - 1, user_data)) {
			return;
		}

		net_buf_simple_pull(ad, len - 1);
	}
}

static void device_found(const bt_addr_le_t *addr, s8_t rssi, u8_t type,
		struct net_buf_simple *ad)
{
	char dev[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(addr, dev, sizeof(dev));
	printk("[DEVICE]: %s, AD evt type %u, AD data len %u, RSSI %i\n",
			dev, type, ad->len, rssi);

	/* We're only interested in connectable events */
	if (type == BT_LE_ADV_IND || type == BT_LE_ADV_DIRECT_IND) {
		/* TODO: Move this to a place it can be shared */
		ad_parse(ad, eir_found, (void *)addr);
	}
}

static void disconnected(struct bt_conn *conn, u8_t reason)
{
	char addr[BT_ADDR_LE_STR_LEN];
	int err;

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	printk("Disconnected: %s (reason %u)\n", addr, reason);

	if (default_conn != conn) {
		return;
	}

	bt_conn_unref(default_conn);
	default_conn = NULL;

	/* This demo doesn't require active scan */
	err = bt_le_scan_start(BT_LE_SCAN_PASSIVE, device_found);
	if (err) {
		printk("Scanning failed to start (err %d)\n", err);
	}
}

static struct bt_conn_cb conn_callbacks = {
	.connected = connected,
	.disconnected = disconnected,
};

static void test_con1_main(void)
{
	int err;

	err = bt_enable(NULL);

	if (err) {
		printk("Bluetooth init failed (err %d)\n", err);
		return;
	}

	printk("Bluetooth initialized\n");

	bt_conn_cb_register(&conn_callbacks);

	err = bt_le_scan_start(BT_LE_SCAN_ACTIVE, device_found);

	if (err) {
		printk("Scanning failed to start (err %d)\n", err);
		return;
	}

	printk("Scanning successfully started\n");
}

static const struct bst_test_instance test_connect[] = {
	{
		.test_id = "connect",
		.test_descr = "Basic connection test. It expects that another "
			      "device running a sample/bluetooth/peripheral_hr "
			      "can be found. The test will pass if it can "
			      "connect to it, and receive a notification in "
			      "less than 5 seconds",
		.test_post_init_f = test_con1_init,
		.test_tick_f = test_con1_tick,
		.test_main_f = test_con1_main
	},
	{
		.test_id = "connect_encrypted",
		.test_descr = "Same as connect but with an encrypted link",
		.test_post_init_f = test_con_encrypted_init,
		.test_tick_f = test_con1_tick,
		.test_main_f = test_con1_main
	},
	BSTEST_END_MARKER
};

struct bst_test_list *test_connect1_install(struct bst_test_list *tests)
{
	tests = bst_add_tests(tests, test_connect);
	return tests;
}
