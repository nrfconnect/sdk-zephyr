/* main.c - Application main entry point */

/*
 * Copyright (c) 2019 Andrei Stoica
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/types.h>
#include <stddef.h>
#include <sys/printk.h>
#include <sys/util.h>
#include <sys/byteorder.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_vs.h>

#include <bluetooth/conn.h>
#include <bluetooth/uuid.h>
#include <bluetooth/gatt.h>
#include <bluetooth/services/hrs.h>

static struct bt_conn *default_conn;
static u16_t default_conn_handle;

static const struct bt_data ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
	BT_DATA_BYTES(BT_DATA_UUID16_ALL, 0x0d, 0x18),
};

#define DEVICE_NAME CONFIG_BT_DEVICE_NAME
#define DEVICE_NAME_LEN (sizeof(DEVICE_NAME) - 1)
#define DEVICE_BEACON_TXPOWER_NUM  8

static struct k_thread pwr_thread_data;
static K_THREAD_STACK_DEFINE(pwr_thread_stack, 320);

static const s8_t txp[DEVICE_BEACON_TXPOWER_NUM] = {4, 0, -3, -8,
						    -15, -18, -23, -30};
static const struct bt_le_adv_param *param = BT_LE_ADV_PARAM
	(BT_LE_ADV_OPT_CONNECTABLE | BT_LE_ADV_OPT_USE_NAME,
	0x0020, 0x0020);

static void read_conn_rssi(u16_t handle, s8_t *rssi)
{
	struct net_buf *buf, *rsp = NULL;
	struct bt_hci_cp_read_rssi *cp;
	struct bt_hci_rp_read_rssi *rp;

	int err;

	buf = bt_hci_cmd_create(BT_HCI_OP_READ_RSSI, sizeof(*cp));
	if (!buf) {
		printk("Unable to allocate command buffer\n");
		return;
	}

	cp = net_buf_add(buf, sizeof(*cp));
	cp->handle = sys_cpu_to_le16(handle);

	err = bt_hci_cmd_send_sync(BT_HCI_OP_READ_RSSI, buf, &rsp);
	if (err) {
		u8_t reason = rsp ?
			((struct bt_hci_rp_read_rssi *)rsp->data)->status : 0;
		printk("Read RSSI err: %d reason 0x%02x\n", err, reason);
		return;
	}

	rp = (void *)rsp->data;
	*rssi = rp->rssi;

	net_buf_unref(rsp);
}


static void set_tx_power(u8_t handle_type, u16_t handle, s8_t tx_pwr_lvl)
{
	struct bt_hci_cp_vs_write_tx_power_level *cp;
	struct bt_hci_rp_vs_write_tx_power_level *rp;
	struct net_buf *buf, *rsp = NULL;
	int err;

	buf = bt_hci_cmd_create(BT_HCI_OP_VS_WRITE_TX_POWER_LEVEL,
				sizeof(*cp));
	if (!buf) {
		printk("Unable to allocate command buffer\n");
		return;
	}

	cp = net_buf_add(buf, sizeof(*cp));
	cp->handle = sys_cpu_to_le16(handle);
	cp->handle_type = handle_type;
	cp->tx_power_level = tx_pwr_lvl;

	err = bt_hci_cmd_send_sync(BT_HCI_OP_VS_WRITE_TX_POWER_LEVEL,
				   buf, &rsp);
	if (err) {
		u8_t reason = rsp ?
			((struct bt_hci_rp_vs_write_tx_power_level *)
			  rsp->data)->status : 0;
		printk("Set Tx power err: %d reason 0x%02x\n", err, reason);
		return;
	}

	rp = (void *)rsp->data;
	printk("Actual Tx Power: %d\n", rp->selected_tx_power);

	net_buf_unref(rsp);
}

static void get_tx_power(u8_t handle_type, u16_t handle, s8_t *tx_pwr_lvl)
{
	struct bt_hci_cp_vs_read_tx_power_level *cp;
	struct bt_hci_rp_vs_read_tx_power_level *rp;
	struct net_buf *buf, *rsp = NULL;
	int err;

	*tx_pwr_lvl = 0xFF;
	buf = bt_hci_cmd_create(BT_HCI_OP_VS_READ_TX_POWER_LEVEL,
				sizeof(*cp));
	if (!buf) {
		printk("Unable to allocate command buffer\n");
		return;
	}

	cp = net_buf_add(buf, sizeof(*cp));
	cp->handle = sys_cpu_to_le16(handle);
	cp->handle_type = handle_type;

	err = bt_hci_cmd_send_sync(BT_HCI_OP_VS_READ_TX_POWER_LEVEL,
				   buf, &rsp);
	if (err) {
		u8_t reason = rsp ?
			((struct bt_hci_rp_vs_read_tx_power_level *)
			  rsp->data)->status : 0;
		printk("Read Tx power err: %d reason 0x%02x\n", err, reason);
		return;
	}

	rp = (void *)rsp->data;
	*tx_pwr_lvl = rp->tx_power_level;

	net_buf_unref(rsp);
}

static void connected(struct bt_conn *conn, u8_t err)
{
	char addr[BT_ADDR_LE_STR_LEN];
	s8_t txp;
	int ret;

	if (err) {
		printk("Connection failed (err 0x%02x)\n", err);
	} else {
		default_conn = bt_conn_ref(conn);
		ret = bt_hci_get_conn_handle(default_conn,
					     &default_conn_handle);
		if (ret) {
			printk("No connection handle (err %d)\n", ret);
		} else {
			/* Send first at the default selected power */
			bt_addr_le_to_str(bt_conn_get_dst(conn),
							  addr, sizeof(addr));
			printk("Connected via connection (%d) at %s\n",
			       default_conn_handle, addr);
			get_tx_power(BT_HCI_VS_LL_HANDLE_TYPE_CONN,
				     default_conn_handle, &txp);
			printk("Connection (%d) - Initial Tx Power = %d\n",
			       default_conn_handle, txp);

			set_tx_power(BT_HCI_VS_LL_HANDLE_TYPE_CONN,
				     default_conn_handle,
				     BT_HCI_VS_LL_TX_POWER_LEVEL_NO_PREF);
			get_tx_power(BT_HCI_VS_LL_HANDLE_TYPE_CONN,
				     default_conn_handle, &txp);
			printk("Connection (%d) - Tx Power = %d\n",
			       default_conn_handle, txp);
		}
	}
}

static void disconnected(struct bt_conn *conn, u8_t reason)
{
	printk("Disconnected (reason 0x%02x)\n", reason);

	if (default_conn) {
		bt_conn_unref(default_conn);
		default_conn = NULL;
	}
}

static struct bt_conn_cb conn_callbacks = {
	.connected = connected,
	.disconnected = disconnected,
};

static void bt_ready(int err)
{
	if (err) {
		printk("Bluetooth init failed (err %d)\n", err);
		return;
	}

	printk("Bluetooth initialized\n");

	/* Start advertising */
	err = bt_le_adv_start(param, ad, ARRAY_SIZE(ad),
			      NULL, 0);
	if (err) {
		printk("Advertising failed to start (err %d)\n", err);
		return;
	}

	printk("Dynamic Tx power Beacon started\n");
}

static void hrs_notify(void)
{
	static u8_t heartrate = 90U;

	/* Heartrate measurements simulation */
	heartrate++;
	if (heartrate == 160U) {
		heartrate = 90U;
	}

	bt_gatt_hrs_notify(heartrate);
}

void modulate_tx_power(void *p1, void *p2, void *p3)
{
	s8_t txp_get = 0;
	u8_t idx = 0;

	while (1) {
		if (!default_conn) {
			printk("Set Tx power level to %d\n", txp[idx]);
			set_tx_power(BT_HCI_VS_LL_HANDLE_TYPE_ADV,
				     0, txp[idx]);

			k_sleep(K_SECONDS(5));

			printk("Get Tx power level -> ");
			get_tx_power(BT_HCI_VS_LL_HANDLE_TYPE_ADV,
				     0, &txp_get);
			printk("TXP = %d\n", txp_get);

			idx = (idx+1) % DEVICE_BEACON_TXPOWER_NUM;
		} else {
			s8_t rssi = 0xFF;
			s8_t txp_adaptive;

			idx = 0;

			read_conn_rssi(default_conn_handle, &rssi);
			printk("Connected (%d) - RSSI = %d\n",
			       default_conn_handle, rssi);
			if (rssi > -70) {
				txp_adaptive = -20;
			} else if (rssi > -90) {
				txp_adaptive = -12;
			} else {
				txp_adaptive = -4;
			}
			printk("Adaptive Tx power selected = %d\n",
			       txp_adaptive);
			set_tx_power(BT_HCI_VS_LL_HANDLE_TYPE_CONN,
				     default_conn_handle, txp_adaptive);
			get_tx_power(BT_HCI_VS_LL_HANDLE_TYPE_CONN,
				     default_conn_handle, &txp_get);
			printk("Connection (%d) TXP = %d\n",
			       default_conn_handle, txp_get);

			k_sleep(K_SECONDS(1));
		}
	}
}

void main(void)
{
	s8_t txp_get = 0xFF;
	int err;

	default_conn = NULL;
	printk("Starting Dynamic Tx Power Beacon Demo\n");

	/* Initialize the Bluetooth Subsystem */
	err = bt_enable(bt_ready);
	if (err) {
		printk("Bluetooth init failed (err %d)\n", err);
	}

	printk("Get Tx power level ->");
	get_tx_power(BT_HCI_VS_LL_HANDLE_TYPE_ADV, 0, &txp_get);
	printk("-> default TXP = %d\n", txp_get);

	bt_conn_cb_register(&conn_callbacks);

	/* Wait for 5 seconds to give a chance users/testers
	 * to check that default Tx power is indeed the one
	 * selected in Kconfig.
	 */
	k_sleep(K_SECONDS(5));

	k_thread_create(&pwr_thread_data, pwr_thread_stack,
			K_THREAD_STACK_SIZEOF(pwr_thread_stack),
			modulate_tx_power, NULL, NULL, NULL,
			K_PRIO_COOP(10),
			0, K_NO_WAIT);
	k_thread_name_set(&pwr_thread_data, "DYN TX");

	while (1) {
		hrs_notify();
		k_sleep(K_SECONDS(2));
	}
}
