/**
 * @file
 * @brief Internal APIs for Bluetooth CSIP
 *
 * Copyright (c) 2021-2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/bluetooth/audio/csip.h>


#define BT_CSIP_SIRK_TYPE_ENCRYPTED             0x00
#define BT_CSIP_SIRK_TYPE_PLAIN                 0x01

#define BT_CSIP_RELEASE_VALUE                   0x01
#define BT_CSIP_LOCK_VALUE                      0x02

struct csip_pending_notifications {
	bt_addr_le_t addr;
	bool pending;
	bool active;

/* Since there's a 1-to-1 connection between bonded devices, and devices in
 * the array containing this struct, if the security manager overwrites
 * the oldest keys, we also overwrite the oldest entry
 */
#if defined(CONFIG_BT_KEYS_OVERWRITE_OLDEST)
	uint32_t age;
#endif /* CONFIG_BT_KEYS_OVERWRITE_OLDEST */
};

struct bt_csip_set_sirk {
	uint8_t type;
	uint8_t value[BT_CSIP_SET_SIRK_SIZE];
} __packed;

struct bt_csip_set_coordinator_csis_inst *bt_csip_set_coordinator_csis_inst_by_handle(
	struct bt_conn *conn, uint16_t start_handle);
