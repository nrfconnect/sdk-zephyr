/**
 * Common functions and helpers for BSIM GATT tests
 *
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
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/kernel.h>
#include <zephyr/types.h>
#include <zephyr/sys/slist.h>

#include "babblekit/testcase.h"
#include "babblekit/flags.h"

#define WAIT_FOR_CONNECTED(link) WAIT_FOR_FLAG((link)->connected)
#define WAIT_FOR_DISCONNECTED(link) WAIT_FOR_FLAG_UNSET((link)->connected)
#define WAIT_FOR_ENCRYPTED_CONNECTION(link) \
	do { \
		WAIT_FOR_FLAG((link)->encrypted); \
		bool pairing_not_triggered; \
		pairing_not_triggered = !IS_FLAG_SET((link)->pairing_complete); \
		TEST_ASSERT(pairing_not_triggered, "Pairing triggered during encryption"); \
	} while (0)

#define WAIT_FOR_PAIRING_COMPLETE(link) \
	TAKE_FLAG((link)->pairing_complete);

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

#define WAIT_FOR_ADV_TIMEOUT(link) \
	TAKE_FLAG((link)->timedout)

struct link_ctx {
	/* Common fields for both Central and Peripheral. */
	sys_snode_t node;

	struct bt_conn *conn;
	atomic_t connected;
	atomic_t encrypted;
	atomic_t pairing_complete;

	/* Peripheral specific fields. */
	atomic_t timedout;
	struct bt_le_ext_adv *adv;
};

const char *addr_to_str(const bt_addr_le_t *addr);

extern bool g_is_central;

void bs_bt_utils_setup(void);

void link_ctx_init(struct link_ctx *ctx);
void clear_conn(struct link_ctx *ctx);

int scan_connect_to_first_result(struct link_ctx *ctx, bool directed, bool filter);

int advertiser_create(struct link_ctx *ctx, int id, bt_addr_le_t *directed_dst, bool filter);
int advertise_connectable(struct link_ctx *ctx);
void advertiser_destroy(struct link_ctx *ctx);
