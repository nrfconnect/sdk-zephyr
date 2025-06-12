/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "bs_bt_utils.h"
#include <zephyr/bluetooth/addr.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/toolchain.h>

#include <stdint.h>
#include <string.h>

#include "babblekit/testcase.h"
#include "babblekit/flags.h"

#if defined(CONFIG_BT_PERIPHERAL)

#define ADVERTISER_CREATE(link, id, dir_addr, filter) \
	do { \
		int err; \
		err = advertiser_create(link, id, dir_addr, filter); \
		TEST_ASSERT(err == 0, "advertiser_create failed (err %d)", err); \
	} while (0)

#define ADVERTISE_CONNECTABLE(link) \
	do { \
		int err; \
		err = advertise_connectable(link); \
		TEST_ASSERT(err == 0, "advertise_connectable failed (err %d)", err); \
	} while (0)

void peripheral_mult_adv_reject(void)
{
	int id_a;
	int id_b;
	struct link_ctx link_a = {};
	struct link_ctx link_b = {};
	bt_addr_le_t central;
	int err;

	bs_bt_utils_setup();
	link_ctx_init(&link_a);
	link_ctx_init(&link_b);

	id_a = bt_id_create(NULL, NULL);
	TEST_ASSERT(id_a >= 0, "bt_id_create id_a failed (err %d)", id_a);

	id_b = bt_id_create(NULL, NULL);
	TEST_ASSERT(id_b >= 0, "bt_id_create id_b failed (err %d)", id_b);

	ADVERTISER_CREATE(&link_a, id_a, NULL, false);
	ADVERTISER_CREATE(&link_b, id_b, NULL, false);

	printk("== Bonding id a ==\n");
	ADVERTISE_CONNECTABLE(&link_a);
	WAIT_FOR_CONNECTED(&link_a);
	/* Central should bond here, and trigger a disconnect. */
	WAIT_FOR_PAIRING_COMPLETE(&link_a);
	WAIT_FOR_ENCRYPTED_CONNECTION(&link_a);
	WAIT_FOR_DISCONNECTED(&link_a);
	central = *bt_conn_get_dst(link_a.conn);
	clear_conn(&link_a);

	printk("== Bonding id b ==\n");
	ADVERTISE_CONNECTABLE(&link_b);
	WAIT_FOR_CONNECTED(&link_b);
	/* Central should bond here, and trigger a disconnect. */
	WAIT_FOR_PAIRING_COMPLETE(&link_b);
	WAIT_FOR_ENCRYPTED_CONNECTION(&link_b);
	WAIT_FOR_DISCONNECTED(&link_b);
	clear_conn(&link_b);

	/* Re-create advertisers to use directed advertisements. */
	advertiser_destroy(&link_a);
	advertiser_destroy(&link_b);
	ADVERTISER_CREATE(&link_a, id_a, &central, false);
	ADVERTISER_CREATE(&link_b, id_b, &central, false);

	printk("== Advertising id a again ==\n");
	ADVERTISE_CONNECTABLE(&link_a);
	printk("== Advertising id b at the same time ==\n");
	err = advertise_connectable(&link_b);
	TEST_ASSERT(err == -EPERM, "advertise_connectable incorrectly failed (err %d)", err);
	WAIT_FOR_ADV_TIMEOUT(&link_a);
	printk("== Advertising id b again ==\n");
	ADVERTISE_CONNECTABLE(&link_b);
	WAIT_FOR_CONNECTED(&link_b);
	WAIT_FOR_ENCRYPTED_CONNECTION(&link_b);
	WAIT_FOR_DISCONNECTED(&link_b);
	clear_conn(&link_b);

	TEST_PASS("PASS");
}

#endif /* CONFIG_BT_PERIPHERAL */
