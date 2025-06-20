/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "bs_bt_utils.h"
#include <zephyr/bluetooth/addr.h>
#include <zephyr/bluetooth/conn.h>

#include <stdint.h>

#include <zephyr/bluetooth/bluetooth.h>

#include "babblekit/testcase.h"
#include "babblekit/flags.h"

#if defined(CONFIG_BT_CENTRAL)

#define SCAN_CONNECT_TO_FIRST_RESULT(link, directed, fal) \
	do { \
		int err; \
		err = scan_connect_to_first_result(link, directed, fal); \
		TEST_ASSERT(err == 0, "Failed to start scanning (err %d)", err); \
	} while (0)

#define CONNECTION_ENCRYPT(link) \
	do { \
		int err; \
		err = bt_conn_set_security((link)->conn, BT_SECURITY_L2); \
		TEST_ASSERT(err == 0, "Failed to set security (err %d)", err); \
	} while (0)

#define DISCONNECT(link) \
	do { \
		int err; \
		err = bt_conn_disconnect((link)->conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN); \
		TEST_ASSERT(err == 0, "Failed to disconnect (err %d)", err); \
	} while (0)

void central_mult_adv_reject(void)
{
	struct link_ctx link = {};
	bt_addr_le_t peripheral_b;

	bs_bt_utils_setup();
	link_ctx_init(&link);

	printk("== Bonding id a ==\n");
	SCAN_CONNECT_TO_FIRST_RESULT(&link, false, false);
	WAIT_FOR_CONNECTED(&link);
	CONNECTION_ENCRYPT(&link);
	WAIT_FOR_PAIRING_COMPLETE(&link);
	DISCONNECT(&link);
	WAIT_FOR_DISCONNECTED(&link);
	clear_conn(&link);

	printk("== Bonding id b ==\n");
	SCAN_CONNECT_TO_FIRST_RESULT(&link, false, false);
	WAIT_FOR_CONNECTED(&link);
	CONNECTION_ENCRYPT(&link);
	WAIT_FOR_PAIRING_COMPLETE(&link);
	DISCONNECT(&link);
	WAIT_FOR_DISCONNECTED(&link);
	peripheral_b = *bt_conn_get_dst(link.conn);
	clear_conn(&link);

	printk("== Connect to id b again ==\n");
	FAL_ADD(&peripheral_b);
	SCAN_CONNECT_TO_FIRST_RESULT(&link, true, true);
	WAIT_FOR_CONNECTED(&link);
	CONNECTION_ENCRYPT(&link);
	WAIT_FOR_ENCRYPTED_CONNECTION(&link);
	DISCONNECT(&link);
	WAIT_FOR_DISCONNECTED(&link);
	clear_conn(&link);
	FAL_CLEAR();

	TEST_PASS("PASS");
}

#endif /* CONFIG_BT_CENTRAL */
