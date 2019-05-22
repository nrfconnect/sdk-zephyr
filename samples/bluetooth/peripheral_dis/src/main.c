/* main.c - Application main entry point */

/*
 * Copyright (c) 2015-2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/types.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>
#include <misc/printk.h>
#include <misc/byteorder.h>
#include <zephyr.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/conn.h>
#include <bluetooth/uuid.h>
#include <bluetooth/gatt.h>
#include <settings/settings.h>

static const struct bt_data ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
	BT_DATA_BYTES(BT_DATA_UUID16_ALL, 0x0a, 0x18),
};

static void connected(struct bt_conn *conn, u8_t err)
{
	if (err) {
		printk("Connection failed (err %u)\n", err);
	} else {
		printk("Connected\n");
	}
}

static void disconnected(struct bt_conn *conn, u8_t reason)
{
	printk("Disconnected (reason %u)\n", reason);
}

static struct bt_conn_cb conn_callbacks = {
	.connected = connected,
	.disconnected = disconnected,
};

static int zephyr_settings_fw_load(struct settings_store *cs);

static const struct settings_store_itf zephyr_settings_fw_itf = {
	.csi_load = zephyr_settings_fw_load,
};

static struct settings_store zephyr_settings_fw_store = {
	.cs_itf = &zephyr_settings_fw_itf
};

static int zephyr_settings_fw_load(struct settings_store *cs)
{

#if defined(CONFIG_BT_GATT_DIS_SETTINGS)
	settings_runtime_set("bt/dis/model",
			     "Zephyr Model",
			     sizeof("Zephyr Model"));
	settings_runtime_set("bt/dis/manuf",
			     "Zephyr Manufacturer",
			     sizeof("Zephyr Manufacturer"));
#if defined(CONFIG_BT_GATT_DIS_SERIAL_NUMBER)
	settings_runtime_set("bt/dis/serial",
			     CONFIG_BT_GATT_DIS_SERIAL_NUMBER_STR,
			     sizeof(CONFIG_BT_GATT_DIS_SERIAL_NUMBER_STR));
#endif
#if defined(CONFIG_BT_GATT_DIS_SW_REV)
	settings_runtime_set("bt/dis/sw",
			     CONFIG_BT_GATT_DIS_SW_REV_STR,
			     sizeof(CONFIG_BT_GATT_DIS_SW_REV_STR));
#endif
#if defined(CONFIG_BT_GATT_DIS_FW_REV)
	settings_runtime_set("bt/dis/fw",
			     CONFIG_BT_GATT_DIS_FW_REV_STR,
			     sizeof(CONFIG_BT_GATT_DIS_FW_REV_STR));
#endif
#if defined(CONFIG_BT_GATT_DIS_HW_REV)
	settings_runtime_set("bt/dis/hw",
			     CONFIG_BT_GATT_DIS_HW_REV_STR,
			     sizeof(CONFIG_BT_GATT_DIS_HW_REV_STR));
#endif
#endif
	return 0;
}

int settings_backend_init(void)
{
	settings_src_register(&zephyr_settings_fw_store);
	return 0;
}

void main(void)
{
	int err;

	err = bt_enable(NULL);
	if (err) {
		printk("Bluetooth init failed (err %d)\n", err);
		return;
	}
	settings_load();

	printk("Bluetooth initialized\n");

	bt_conn_cb_register(&conn_callbacks);

	err = bt_le_adv_start(BT_LE_ADV_CONN_NAME, ad, ARRAY_SIZE(ad), NULL, 0);
	if (err) {
		printk("Advertising failed to start (err %d)\n", err);
		return;
	}

	printk("Advertising successfully started\n");
}
