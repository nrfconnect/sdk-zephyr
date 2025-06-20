/**
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "bs_tracing.h"
#include "bs_types.h"
#include "bstests.h"
#include "time_machine.h"
#include <zephyr/sys/__assert.h>

#include <errno.h>
#include <stddef.h>
#include <stdint.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/kernel.h>
#include <zephyr/types.h>

#include "babblekit/testcase.h"
#include "babblekit/flags.h"

DECLARE_FLAG(flag_pairing_complete);
DECLARE_FLAG(flag_is_connected);
DECLARE_FLAG(security_changed_flag);

#define WAIT_FOR_CONNECTED() WAIT_FOR_FLAG(flag_is_connected)
#define WAIT_FOR_DISCONNECTED() WAIT_FOR_FLAG_UNSET(flag_is_connected)
#define WAIT_FOR_ENCRYPTED_CONNECTION() \
	do { \
		WAIT_FOR_FLAG(security_changed_flag); \
		bool pairing_not_triggered; \
		pairing_not_triggered = !IS_FLAG_SET(flag_pairing_complete); \
		TEST_ASSERT(pairing_not_triggered, "Pairing triggered during encryption"); \
	} while (0)

#define FAL_ADD(addr) \
	do { \
		int err; \
		err = bt_le_filter_accept_list_add(addr); \
		TEST_ASSERT(!err, "Err bt_le_filter_accept_list_add %d", err); \
	} while (0)

#define FAL_CLEAR() \
	do { \
		int err; \
		err = bt_le_filter_accept_list_clear(); \
		TEST_ASSERT(!err, "Err bt_le_filter_accept_list_clear %d", err); \
	} while (0)

extern struct bt_conn *g_conn;

const char *addr_to_str(const bt_addr_le_t *addr);
void clear_g_conn(void);
void bs_bt_utils_setup(void);
int scan_connect_to_first_result(bool directed, bool filter);
int advertise_connectable(int id, bt_addr_le_t *directed_dst, bool filter);
