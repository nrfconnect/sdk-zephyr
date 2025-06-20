/*
 * Copyright (c) 2025: Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "bs_bt_utils.h"
#include <zephyr/bluetooth/addr.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/toolchain.h>
#include <zephyr/settings/settings.h>

#include "host/keys.h"

#include <stdint.h>
#include <string.h>

#include "babblekit/testcase.h"
#include "babblekit/flags.h"

#if defined(CONFIG_BT_PERIPHERAL)

BUILD_ASSERT(IS_ENABLED(CONFIG_BT_ID_AUTO_SWAP_MATCHING_BONDS), "");

#define ADVERTISE_CONNECTABLE(id, central, fal) \
	do { \
		int err; \
		err = advertise_connectable(id, central, fal); \
		TEST_ASSERT(err == 0, "Advertising failed to start (err %d)", err); \
	} while (0)

#define UNPAIR(id, addr) \
	do { \
		int err; \
		err = bt_unpair(id, addr); \
		TEST_ASSERT(err == 0, "Unpairing failed (err %d)", err); \
	} while (0)

#define ID_CONFLICT_FLAG_CHECK(id, addr, set) \
	do { \
		struct bt_keys *keys; \
		keys = bt_keys_find_addr(id, addr); \
		TEST_ASSERT(keys != NULL, "No keys found for id %d addr %s", id,\
			    addr_to_str(addr)); \
		if (set) { \
			TEST_ASSERT(keys->flags & BT_KEYS_ID_CONFLICT, \
				    "ID conflict flag not set for id %d addr %s", id, \
				    addr_to_str(addr)); \
		} else { \
			TEST_ASSERT(!(keys->flags & BT_KEYS_ID_CONFLICT), \
				    "ID conflict flag set for id %d addr %s", id, \
				    addr_to_str(addr));\
		} \
	} while (0)

void peripheral_basic(void)
{
	int id_a;
	int id_b;
	bt_addr_le_t central;

	bs_bt_utils_setup();

	id_a = bt_id_create(NULL, NULL);
	TEST_ASSERT(id_a >= 0, "bt_id_create id_a failed (err %d)", id_a);

	id_b = bt_id_create(NULL, NULL);
	TEST_ASSERT(id_b >= 0, "bt_id_create id_b failed (err %d)", id_b);

	printk("== Bonding id a ==\n");
	ADVERTISE_CONNECTABLE(id_a, NULL, false);
	WAIT_FOR_CONNECTED();
	/* Central should bond here, and trigger a disconnect. */
	TAKE_FLAG(flag_pairing_complete);
	WAIT_FOR_ENCRYPTED_CONNECTION();
	WAIT_FOR_DISCONNECTED();
	central = *bt_conn_get_dst(g_conn);
	clear_g_conn();

	printk("== Bonding id b ==\n");
	ADVERTISE_CONNECTABLE(id_b, NULL, false);
	WAIT_FOR_CONNECTED();
	/* Central should bond here, and trigger a disconnect. */
	TAKE_FLAG(flag_pairing_complete);
	WAIT_FOR_ENCRYPTED_CONNECTION();
	WAIT_FOR_DISCONNECTED();
	clear_g_conn();

	printk("== Advertising id a again ==\n");
	ADVERTISE_CONNECTABLE(id_a, NULL, false);
	WAIT_FOR_CONNECTED();
	/* Central should encrypt connection here and trigger disconnect. */
	WAIT_FOR_ENCRYPTED_CONNECTION();
	WAIT_FOR_DISCONNECTED();
	clear_g_conn();

	printk("== Advertising id b again ==\n");
	ADVERTISE_CONNECTABLE(id_b, NULL, false);
	WAIT_FOR_CONNECTED();
	/* Central should encrypt connection here and trigger disconnect. */
	WAIT_FOR_ENCRYPTED_CONNECTION();
	WAIT_FOR_DISCONNECTED();
	clear_g_conn();

	printk("== Advertising id a again ==\n");
	ADVERTISE_CONNECTABLE(id_a, NULL, false);
	WAIT_FOR_CONNECTED();
	/* Central should encrypt connection here and trigger disconnect. */
	WAIT_FOR_ENCRYPTED_CONNECTION();
	WAIT_FOR_DISCONNECTED();
	clear_g_conn();

	printk("== Advertising id b again ==\n");
	ADVERTISE_CONNECTABLE(id_b, NULL, false);
	WAIT_FOR_CONNECTED();
	/* Central should encrypt connection here and trigger disconnect. */
	WAIT_FOR_ENCRYPTED_CONNECTION();
	WAIT_FOR_DISCONNECTED();
	clear_g_conn();

	TEST_PASS("PASS");
}

void peripheral_fal(void)
{
	int id_a;
	int id_b;
	bt_addr_le_t central;

	bs_bt_utils_setup();

	id_a = bt_id_create(NULL, NULL);
	TEST_ASSERT(id_a >= 0, "bt_id_create id_a failed (err %d)", id_a);

	id_b = bt_id_create(NULL, NULL);
	TEST_ASSERT(id_b >= 0, "bt_id_create id_b failed (err %d)", id_b);

	printk("== Bonding id a ==\n");
	ADVERTISE_CONNECTABLE(id_a, NULL, false);
	WAIT_FOR_CONNECTED();
	/* Central should bond here, and trigger a disconnect. */
	TAKE_FLAG(flag_pairing_complete);
	WAIT_FOR_ENCRYPTED_CONNECTION();
	WAIT_FOR_DISCONNECTED();
	central = *bt_conn_get_dst(g_conn);
	clear_g_conn();

	printk("== Bonding id b ==\n");
	ADVERTISE_CONNECTABLE(id_b, NULL, false);
	WAIT_FOR_CONNECTED();
	TAKE_FLAG(flag_pairing_complete);
	/* Central should bond here, and trigger a disconnect. */
	WAIT_FOR_ENCRYPTED_CONNECTION();
	WAIT_FOR_DISCONNECTED();
	clear_g_conn();

	printk("== Advertising id a again ==\n");
	FAL_ADD(&central);
	ADVERTISE_CONNECTABLE(id_a, NULL, true);
	WAIT_FOR_CONNECTED();
	/* Central should encrypt connection here and trigger disconnect. */
	WAIT_FOR_ENCRYPTED_CONNECTION();
	WAIT_FOR_DISCONNECTED();
	clear_g_conn();
	FAL_CLEAR();

	printk("== Advertising id b again ==\n");
	FAL_ADD(&central);
	ADVERTISE_CONNECTABLE(id_b, NULL, true);
	WAIT_FOR_CONNECTED();
	/* Central should encrypt connection here and trigger disconnect. */
	WAIT_FOR_ENCRYPTED_CONNECTION();
	WAIT_FOR_DISCONNECTED();
	clear_g_conn();
	FAL_CLEAR();

	printk("== Advertising id a again ==\n");
	FAL_ADD(&central);
	ADVERTISE_CONNECTABLE(id_a, NULL, true);
	WAIT_FOR_CONNECTED();
	/* Central should encrypt connection here and trigger disconnect. */
	WAIT_FOR_ENCRYPTED_CONNECTION();
	WAIT_FOR_DISCONNECTED();
	clear_g_conn();
	FAL_CLEAR();

	printk("== Advertising id b again ==\n");
	FAL_ADD(&central);
	ADVERTISE_CONNECTABLE(id_b, NULL, true);
	WAIT_FOR_CONNECTED();
	/* Central should encrypt connection here and trigger disconnect. */
	WAIT_FOR_ENCRYPTED_CONNECTION();
	WAIT_FOR_DISCONNECTED();
	clear_g_conn();
	FAL_CLEAR();

	TEST_PASS("PASS");
}

void peripheral_dir(void)
{
	int id_a;
	int id_b;
	bt_addr_le_t central;

	bs_bt_utils_setup();

	id_a = bt_id_create(NULL, NULL);
	TEST_ASSERT(id_a >= 0, "bt_id_create id_a failed (err %d)", id_a);

	id_b = bt_id_create(NULL, NULL);
	TEST_ASSERT(id_b >= 0, "bt_id_create id_b failed (err %d)", id_b);

	printk("== Bonding id a ==\n");
	ADVERTISE_CONNECTABLE(id_a, NULL, false);
	WAIT_FOR_CONNECTED();
	/* Central should bond here, and trigger a disconnect. */
	TAKE_FLAG(flag_pairing_complete);
	WAIT_FOR_ENCRYPTED_CONNECTION();
	WAIT_FOR_DISCONNECTED();
	central = *bt_conn_get_dst(g_conn);
	clear_g_conn();

	printk("== Bonding id b ==\n");
	ADVERTISE_CONNECTABLE(id_b, NULL, false);
	WAIT_FOR_CONNECTED();
	/* Central should bond here, and trigger a disconnect. */
	TAKE_FLAG(flag_pairing_complete);
	WAIT_FOR_ENCRYPTED_CONNECTION();
	WAIT_FOR_DISCONNECTED();
	clear_g_conn();

	printk("== Advertising id a again ==\n");
	ADVERTISE_CONNECTABLE(id_a, &central, false);
	WAIT_FOR_CONNECTED();
	/* Central should encrypt connection here and trigger disconnect. */
	WAIT_FOR_ENCRYPTED_CONNECTION();
	WAIT_FOR_DISCONNECTED();
	clear_g_conn();
	FAL_CLEAR();

	printk("== Advertising id b again ==\n");
	FAL_ADD(&central);
	ADVERTISE_CONNECTABLE(id_b, &central, false);
	WAIT_FOR_CONNECTED();
	/* Central should encrypt connection here and trigger disconnect. */
	WAIT_FOR_ENCRYPTED_CONNECTION();
	WAIT_FOR_DISCONNECTED();
	clear_g_conn();
	FAL_CLEAR();

	printk("== Advertising id a again ==\n");
	ADVERTISE_CONNECTABLE(id_a, &central, false);
	WAIT_FOR_CONNECTED();
	/* Central should encrypt connection here and trigger disconnect. */
	WAIT_FOR_ENCRYPTED_CONNECTION();
	WAIT_FOR_DISCONNECTED();
	clear_g_conn();
	FAL_CLEAR();

	printk("== Advertising id b again ==\n");
	FAL_ADD(&central);
	ADVERTISE_CONNECTABLE(id_b, &central, false);
	WAIT_FOR_CONNECTED();
	/* Central should encrypt connection here and trigger disconnect. */
	WAIT_FOR_ENCRYPTED_CONNECTION();
	WAIT_FOR_DISCONNECTED();
	clear_g_conn();
	FAL_CLEAR();

	TEST_PASS("PASS");
}

void peripheral_unpair(void)
{
	int id_a;
	int id_b;
	int id_c;
	bt_addr_le_t central;

	bs_bt_utils_setup();

	id_a = bt_id_create(NULL, NULL);
	TEST_ASSERT(id_a >= 0, "bt_id_create id_a failed (err %d)", id_a);

	id_b = bt_id_create(NULL, NULL);
	TEST_ASSERT(id_b >= 0, "bt_id_create id_b failed (err %d)", id_b);

	id_c = bt_id_create(NULL, NULL);
	TEST_ASSERT(id_c >= 0, "bt_id_create id_c failed (err %d)", id_c);

	printk("== Bonding id a ==\n");
	ADVERTISE_CONNECTABLE(id_a, NULL, false);
	WAIT_FOR_CONNECTED();
	/* Central should bond here, and trigger a disconnect. */
	TAKE_FLAG(flag_pairing_complete);
	WAIT_FOR_ENCRYPTED_CONNECTION();
	WAIT_FOR_DISCONNECTED();
	central = *bt_conn_get_dst(g_conn);
	clear_g_conn();

	printk("== Bonding id b ==\n");
	ADVERTISE_CONNECTABLE(id_b, NULL, false);
	WAIT_FOR_CONNECTED();
	/* Central should bond here, and trigger a disconnect. */
	TAKE_FLAG(flag_pairing_complete);
	WAIT_FOR_ENCRYPTED_CONNECTION();
	WAIT_FOR_DISCONNECTED();
	ID_CONFLICT_FLAG_CHECK(id_a, &central, true);
	ID_CONFLICT_FLAG_CHECK(id_b, &central, true);
	clear_g_conn();

	printk("== Bonding id c ==\n");
	ADVERTISE_CONNECTABLE(id_c, NULL, false);
	WAIT_FOR_CONNECTED();
	/* Central should bond here, and trigger a disconnect. */
	TAKE_FLAG(flag_pairing_complete);
	WAIT_FOR_ENCRYPTED_CONNECTION();
	WAIT_FOR_DISCONNECTED();
	ID_CONFLICT_FLAG_CHECK(id_b, &central, true);
	ID_CONFLICT_FLAG_CHECK(id_c, &central, true);
	clear_g_conn();

	printk("== Advertising id a again ==\n");
	ADVERTISE_CONNECTABLE(id_a, NULL, false);
	WAIT_FOR_CONNECTED();
	/* Central should encrypt connection here. */
	WAIT_FOR_ENCRYPTED_CONNECTION();
	printk("== Unpairing id a ==\n");
	UNPAIR(id_a, &central);
	WAIT_FOR_DISCONNECTED();
	clear_g_conn();

	ID_CONFLICT_FLAG_CHECK(id_b, &central, true);
	ID_CONFLICT_FLAG_CHECK(id_c, &central, true);

	printk("== Advertising id b again ==\n");
	ADVERTISE_CONNECTABLE(id_b, NULL, false);
	WAIT_FOR_CONNECTED();
	/* Central should encrypt connection here. */
	WAIT_FOR_ENCRYPTED_CONNECTION();
	printk("== Unpairing id b ==\n");
	UNPAIR(id_b, &central);
	WAIT_FOR_DISCONNECTED();
	clear_g_conn();

	ID_CONFLICT_FLAG_CHECK(id_c, &central, false);

	printk("== Advertising id c again ==\n");
	ADVERTISE_CONNECTABLE(id_c, NULL, false);
	WAIT_FOR_CONNECTED();
	/* Central should encrypt connection here. */
	WAIT_FOR_ENCRYPTED_CONNECTION();
	printk("== Unpairing id c ==\n");
	UNPAIR(id_c, &central);
	WAIT_FOR_DISCONNECTED();
	clear_g_conn();

	TEST_PASS("PASS");
}

#if defined(CONFIG_BT_SETTINGS)
void peripheral_persist_bond(void)
{
	int id_a;
	int id_b;
	bt_addr_le_t central;
	int err;

	bs_bt_utils_setup();

	err = settings_load();
	TEST_ASSERT(!err, "settings_load failed (err %d)", err);

	id_a = bt_id_create(NULL, NULL);
	TEST_ASSERT(id_a == 1, "bt_id_create id_a failed (err %d)", id_a);

	id_b = bt_id_create(NULL, NULL);
	TEST_ASSERT(id_b == 2, "bt_id_create id_b failed (err %d)", id_b);

	printk("== Bonding id a ==\n");
	ADVERTISE_CONNECTABLE(id_a, NULL, false);
	WAIT_FOR_CONNECTED();
	/* Central should bond here, and trigger a disconnect. */
	TAKE_FLAG(flag_pairing_complete);
	WAIT_FOR_ENCRYPTED_CONNECTION();
	WAIT_FOR_DISCONNECTED();
	central = *bt_conn_get_dst(g_conn);
	clear_g_conn();

	printk("== Bonding id b ==\n");
	ADVERTISE_CONNECTABLE(id_b, NULL, false);
	WAIT_FOR_CONNECTED();
	/* Central should bond here, and trigger a disconnect. */
	TAKE_FLAG(flag_pairing_complete);
	WAIT_FOR_ENCRYPTED_CONNECTION();
	WAIT_FOR_DISCONNECTED();
	clear_g_conn();

	TEST_PASS("PASS");
}

static void bond_get(const struct bt_bond_info *info, void *user_data)
{
	bt_addr_le_t *central = (bt_addr_le_t *)user_data;

	if (bt_addr_le_cmp(BT_ADDR_LE_NONE, central) == 0) {
		bt_addr_le_copy(central, &info->addr);
	} else {
		char bond_addr[BT_ADDR_LE_STR_LEN];
		char central_addr[BT_ADDR_LE_STR_LEN];

		bt_addr_le_to_str(&info->addr, bond_addr, sizeof(bond_addr));
		bt_addr_le_to_str(central, central_addr, sizeof(central_addr));

		TEST_ASSERT(false, "Unexpected bond found: %s", bond_addr);
		TEST_ASSERT(bt_addr_le_cmp(&info->addr, central) == 0,
			    "Bonded address %s does not match expected %s", bond_addr,
			    central_addr);
	}
}

static void bond_check(const struct bt_bond_info *info, void *user_data)
{
	bt_addr_le_t *central = (bt_addr_le_t *)user_data;

	char bond_addr[BT_ADDR_LE_STR_LEN];
	char central_addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(&info->addr, bond_addr, sizeof(bond_addr));
	bt_addr_le_to_str(central, central_addr, sizeof(central_addr));

	TEST_ASSERT(bt_addr_le_cmp(&info->addr, central) == 0,
		    "Bonded address %s does not match expected %s", bond_addr, central_addr);
}

void peripheral_persist_reconnect(void)
{
	int id_a;
	int id_b;
	bt_addr_le_t central;
	int err;
	size_t id_count;

	bs_bt_utils_setup();

	err = settings_load();
	TEST_ASSERT(!err, "settings_load failed (err %d)", err);

	bt_id_get(NULL, &id_count);
	TEST_ASSERT(id_count == 3, "Expected 3 IDs, found %zu", id_count);

	/* Values must be identical to generated in peripheral_persist_bond() test. */
	id_a = 1;
	id_b = 2;

	/* Get the bonded Central address from the settings. */
	bt_addr_le_copy(&central, BT_ADDR_LE_NONE);
	bt_foreach_bond(id_a, bond_get, &central);

	/* Check that id_b is bonded to the same Central as id_a. */
	bt_foreach_bond(id_b, bond_check, &central);

	printk("== Advertising id a again ==\n");
	ADVERTISE_CONNECTABLE(id_a, &central, false);
	WAIT_FOR_CONNECTED();
	/* Central should encrypt connection here and trigger disconnect. */
	WAIT_FOR_ENCRYPTED_CONNECTION();
	WAIT_FOR_DISCONNECTED();
	clear_g_conn();

	printk("== Advertising id b again ==\n");
	ADVERTISE_CONNECTABLE(id_b, &central, false);
	WAIT_FOR_CONNECTED();
	/* Central should encrypt connection here and trigger disconnect. */
	WAIT_FOR_ENCRYPTED_CONNECTION();
	WAIT_FOR_DISCONNECTED();
	clear_g_conn();

	printk("== Advertising id a again ==\n");
	ADVERTISE_CONNECTABLE(id_a, &central, false);
	WAIT_FOR_CONNECTED();
	/* Central should encrypt connection here and trigger disconnect. */
	WAIT_FOR_ENCRYPTED_CONNECTION();
	WAIT_FOR_DISCONNECTED();
	clear_g_conn();

	printk("== Advertising id b again ==\n");
	ADVERTISE_CONNECTABLE(id_b, &central, false);
	WAIT_FOR_CONNECTED();
	/* Central should encrypt connection here and trigger disconnect. */
	WAIT_FOR_ENCRYPTED_CONNECTION();
	WAIT_FOR_DISCONNECTED();
	clear_g_conn();

	TEST_PASS("PASS");
}
#endif /* CONFIG_BT_SETTINGS */

#endif /* CONFIG_BT_PERIPHERAL */
