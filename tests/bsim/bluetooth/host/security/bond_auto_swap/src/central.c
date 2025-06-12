/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "bs_bt_utils.h"
#include <zephyr/bluetooth/addr.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/settings/settings.h>

#include <stdint.h>

#include <zephyr/bluetooth/bluetooth.h>

#include "babblekit/testcase.h"
#include "babblekit/flags.h"

#if defined(CONFIG_BT_CENTRAL)

#define SCAN_CONNECT_TO_FIRST_RESULT(directed, fal) \
	do { \
		int err; \
		err = scan_connect_to_first_result(directed, fal); \
		TEST_ASSERT(err == 0, "Failed to start scanning (err %d)", err); \
	} while (0)

#define CONNECTION_ENCRYPT() \
	do { \
		int err; \
		err = bt_conn_set_security(g_conn, BT_SECURITY_L2); \
		TEST_ASSERT(err == 0, "Failed to set security (err %d)", err); \
	} while (0)

#define DISCONNECT() \
	do { \
		int err; \
		err = bt_conn_disconnect(g_conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN); \
		TEST_ASSERT(err == 0, "Failed to disconnect (err %d)", err); \
	} while (0)

void central_basic(void)
{
	bs_bt_utils_setup();

	printk("== Bonding id a ==\n");
	SCAN_CONNECT_TO_FIRST_RESULT(false, false);
	WAIT_FOR_CONNECTED();
	CONNECTION_ENCRYPT();
	TAKE_FLAG(flag_pairing_complete);
	DISCONNECT();
	WAIT_FOR_DISCONNECTED();
	clear_g_conn();

	printk("== Bonding id b ==\n");
	SCAN_CONNECT_TO_FIRST_RESULT(false, false);
	WAIT_FOR_CONNECTED();
	CONNECTION_ENCRYPT();
	TAKE_FLAG(flag_pairing_complete);
	DISCONNECT();
	WAIT_FOR_DISCONNECTED();
	clear_g_conn();

	printk("== Connect to id a again ==\n");
	SCAN_CONNECT_TO_FIRST_RESULT(false, false);
	WAIT_FOR_CONNECTED();
	CONNECTION_ENCRYPT();
	WAIT_FOR_ENCRYPTED_CONNECTION();
	DISCONNECT();
	WAIT_FOR_DISCONNECTED();
	clear_g_conn();

	printk("== Connect to id b again ==\n");
	SCAN_CONNECT_TO_FIRST_RESULT(false, false);
	WAIT_FOR_CONNECTED();
	CONNECTION_ENCRYPT();
	WAIT_FOR_ENCRYPTED_CONNECTION();
	DISCONNECT();
	WAIT_FOR_DISCONNECTED();
	clear_g_conn();

	printk("== Connect to id a again ==\n");
	SCAN_CONNECT_TO_FIRST_RESULT(false, false);
	WAIT_FOR_CONNECTED();
	CONNECTION_ENCRYPT();
	WAIT_FOR_ENCRYPTED_CONNECTION();
	DISCONNECT();
	WAIT_FOR_DISCONNECTED();
	clear_g_conn();

	printk("== Connect to id b again ==\n");
	SCAN_CONNECT_TO_FIRST_RESULT(false, false);
	WAIT_FOR_CONNECTED();
	CONNECTION_ENCRYPT();
	WAIT_FOR_ENCRYPTED_CONNECTION();
	DISCONNECT();
	WAIT_FOR_DISCONNECTED();
	clear_g_conn();

	TEST_PASS("PASS");
}

void central_fal(void)
{
	bt_addr_le_t peripheral_a;
	bt_addr_le_t peripheral_b;

	bs_bt_utils_setup();

	printk("== Bonding id a ==\n");
	SCAN_CONNECT_TO_FIRST_RESULT(false, false);
	WAIT_FOR_CONNECTED();
	CONNECTION_ENCRYPT();
	TAKE_FLAG(flag_pairing_complete);
	DISCONNECT();
	WAIT_FOR_DISCONNECTED();
	peripheral_a = *bt_conn_get_dst(g_conn);
	clear_g_conn();

	printk("== Bonding id b ==\n");
	SCAN_CONNECT_TO_FIRST_RESULT(false, false);
	WAIT_FOR_CONNECTED();
	CONNECTION_ENCRYPT();
	TAKE_FLAG(flag_pairing_complete);
	DISCONNECT();
	WAIT_FOR_DISCONNECTED();
	peripheral_b = *bt_conn_get_dst(g_conn);
	clear_g_conn();

	printk("== Connect to id a again ==\n");
	FAL_ADD(&peripheral_a);
	SCAN_CONNECT_TO_FIRST_RESULT(false, true);
	WAIT_FOR_CONNECTED();
	CONNECTION_ENCRYPT();
	WAIT_FOR_ENCRYPTED_CONNECTION();
	DISCONNECT();
	WAIT_FOR_DISCONNECTED();
	clear_g_conn();
	FAL_CLEAR();

	printk("== Connect to id b again ==\n");
	FAL_ADD(&peripheral_b);
	SCAN_CONNECT_TO_FIRST_RESULT(false, true);
	WAIT_FOR_CONNECTED();
	CONNECTION_ENCRYPT();
	WAIT_FOR_ENCRYPTED_CONNECTION();
	DISCONNECT();
	WAIT_FOR_DISCONNECTED();
	clear_g_conn();
	FAL_CLEAR();

	printk("== Connect to id a again ==\n");
	FAL_ADD(&peripheral_a);
	SCAN_CONNECT_TO_FIRST_RESULT(false, true);
	WAIT_FOR_CONNECTED();
	CONNECTION_ENCRYPT();
	WAIT_FOR_ENCRYPTED_CONNECTION();
	DISCONNECT();
	WAIT_FOR_DISCONNECTED();
	clear_g_conn();
	FAL_CLEAR();

	printk("== Connect to id b again ==\n");
	FAL_ADD(&peripheral_b);
	SCAN_CONNECT_TO_FIRST_RESULT(false, true);
	WAIT_FOR_CONNECTED();
	CONNECTION_ENCRYPT();
	WAIT_FOR_ENCRYPTED_CONNECTION();
	DISCONNECT();
	WAIT_FOR_DISCONNECTED();
	clear_g_conn();
	FAL_CLEAR();

	TEST_PASS("PASS");
}

void central_dir(void)
{
	bs_bt_utils_setup();

	printk("== Bonding id a ==\n");
	SCAN_CONNECT_TO_FIRST_RESULT(false, false);
	WAIT_FOR_CONNECTED();
	CONNECTION_ENCRYPT();
	TAKE_FLAG(flag_pairing_complete);
	DISCONNECT();
	WAIT_FOR_DISCONNECTED();
	clear_g_conn();

	printk("== Bonding id b ==\n");
	SCAN_CONNECT_TO_FIRST_RESULT(false, false);
	WAIT_FOR_CONNECTED();
	CONNECTION_ENCRYPT();
	TAKE_FLAG(flag_pairing_complete);
	DISCONNECT();
	WAIT_FOR_DISCONNECTED();
	clear_g_conn();

	printk("== Connect to id a again ==\n");
	SCAN_CONNECT_TO_FIRST_RESULT(true, false);
	WAIT_FOR_CONNECTED();
	CONNECTION_ENCRYPT();
	WAIT_FOR_ENCRYPTED_CONNECTION();
	DISCONNECT();
	WAIT_FOR_DISCONNECTED();
	clear_g_conn();

	printk("== Connect to id b again ==\n");
	SCAN_CONNECT_TO_FIRST_RESULT(true, false);
	WAIT_FOR_CONNECTED();
	CONNECTION_ENCRYPT();
	WAIT_FOR_ENCRYPTED_CONNECTION();
	DISCONNECT();
	WAIT_FOR_DISCONNECTED();
	clear_g_conn();

	printk("== Connect to id a again ==\n");
	SCAN_CONNECT_TO_FIRST_RESULT(true, false);
	WAIT_FOR_CONNECTED();
	CONNECTION_ENCRYPT();
	WAIT_FOR_ENCRYPTED_CONNECTION();
	DISCONNECT();
	WAIT_FOR_DISCONNECTED();
	clear_g_conn();

	printk("== Connect to id b again ==\n");
	SCAN_CONNECT_TO_FIRST_RESULT(true, false);
	WAIT_FOR_CONNECTED();
	CONNECTION_ENCRYPT();
	WAIT_FOR_ENCRYPTED_CONNECTION();
	DISCONNECT();
	WAIT_FOR_DISCONNECTED();
	clear_g_conn();

	TEST_PASS("PASS");
}

void central_unpair(void)
{
	bs_bt_utils_setup();

	printk("== Bonding id a ==\n");
	SCAN_CONNECT_TO_FIRST_RESULT(false, false);
	WAIT_FOR_CONNECTED();
	CONNECTION_ENCRYPT();
	TAKE_FLAG(flag_pairing_complete);
	DISCONNECT();
	WAIT_FOR_DISCONNECTED();
	clear_g_conn();

	printk("== Bonding id b ==\n");
	SCAN_CONNECT_TO_FIRST_RESULT(false, false);
	WAIT_FOR_CONNECTED();
	CONNECTION_ENCRYPT();
	TAKE_FLAG(flag_pairing_complete);
	DISCONNECT();
	WAIT_FOR_DISCONNECTED();
	clear_g_conn();

	printk("== Bonding id c ==\n");
	SCAN_CONNECT_TO_FIRST_RESULT(false, false);
	WAIT_FOR_CONNECTED();
	CONNECTION_ENCRYPT();
	TAKE_FLAG(flag_pairing_complete);
	DISCONNECT();
	WAIT_FOR_DISCONNECTED();
	clear_g_conn();

	printk("== Connect to id a again ==\n");
	SCAN_CONNECT_TO_FIRST_RESULT(false, false);
	WAIT_FOR_CONNECTED();
	CONNECTION_ENCRYPT();
	WAIT_FOR_ENCRYPTED_CONNECTION();
	/* Peripheral should unpair and thus disconnect. */
	WAIT_FOR_DISCONNECTED();
	clear_g_conn();

	printk("== Connect to id b again ==\n");
	SCAN_CONNECT_TO_FIRST_RESULT(false, false);
	WAIT_FOR_CONNECTED();
	CONNECTION_ENCRYPT();
	WAIT_FOR_ENCRYPTED_CONNECTION();
	/* Peripheral should unpair and thus disconnect. */
	WAIT_FOR_DISCONNECTED();
	clear_g_conn();

	printk("== Connect to id c again ==\n");
	SCAN_CONNECT_TO_FIRST_RESULT(false, false);
	WAIT_FOR_CONNECTED();
	CONNECTION_ENCRYPT();
	WAIT_FOR_ENCRYPTED_CONNECTION();
	/* Peripheral should unpair and thus disconnect. */
	WAIT_FOR_DISCONNECTED();
	clear_g_conn();

	TEST_PASS("PASS");
}

#if defined(CONFIG_BT_SETTINGS)
void central_persist_bond(void)
{
	int err;

	bs_bt_utils_setup();

	err = settings_load();
	TEST_ASSERT(!err, "Failed to load settings (err %d)", err);

	printk("== Bonding id a ==\n");
	SCAN_CONNECT_TO_FIRST_RESULT(false, false);
	WAIT_FOR_CONNECTED();
	CONNECTION_ENCRYPT();
	TAKE_FLAG(flag_pairing_complete);
	DISCONNECT();
	WAIT_FOR_DISCONNECTED();
	clear_g_conn();

	printk("== Bonding id b ==\n");
	SCAN_CONNECT_TO_FIRST_RESULT(false, false);
	WAIT_FOR_CONNECTED();
	CONNECTION_ENCRYPT();
	TAKE_FLAG(flag_pairing_complete);
	DISCONNECT();
	WAIT_FOR_DISCONNECTED();
	clear_g_conn();

	TEST_PASS("PASS");
}

void central_persist_reconnect(void)
{
	int err;

	bs_bt_utils_setup();

	err = settings_load();
	TEST_ASSERT(!err, "Failed to load settings (err %d)", err);

	printk("== Connect to id a again ==\n");
	SCAN_CONNECT_TO_FIRST_RESULT(true, false);
	WAIT_FOR_CONNECTED();
	CONNECTION_ENCRYPT();
	WAIT_FOR_ENCRYPTED_CONNECTION();
	DISCONNECT();
	WAIT_FOR_DISCONNECTED();
	clear_g_conn();

	printk("== Connect to id b again ==\n");
	SCAN_CONNECT_TO_FIRST_RESULT(true, false);
	WAIT_FOR_CONNECTED();
	CONNECTION_ENCRYPT();
	WAIT_FOR_ENCRYPTED_CONNECTION();
	DISCONNECT();
	WAIT_FOR_DISCONNECTED();
	clear_g_conn();

	printk("== Connect to id a again ==\n");
	SCAN_CONNECT_TO_FIRST_RESULT(true, false);
	WAIT_FOR_CONNECTED();
	CONNECTION_ENCRYPT();
	WAIT_FOR_ENCRYPTED_CONNECTION();
	DISCONNECT();
	WAIT_FOR_DISCONNECTED();
	clear_g_conn();

	printk("== Connect to id b again ==\n");
	SCAN_CONNECT_TO_FIRST_RESULT(true, false);
	WAIT_FOR_CONNECTED();
	CONNECTION_ENCRYPT();
	WAIT_FOR_ENCRYPTED_CONNECTION();
	DISCONNECT();
	WAIT_FOR_DISCONNECTED();
	clear_g_conn();

	TEST_PASS("PASS");
}
#endif /* CONFIG_BT_SETTINGS */

#endif /* CONFIG_BT_CENTRAL */
