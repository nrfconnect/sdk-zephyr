/** @file
 *  @brief GATT Device Infromation Service
 */

/*
 * Copyright (c) 2019 Demant
 * Copyright (c) 2018 Nordic Semiconductor ASA
 * Copyright (c) 2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/types.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>
#include <zephyr.h>
#include <init.h>

#include <settings/settings.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/conn.h>
#include <bluetooth/uuid.h>
#include <bluetooth/gatt.h>

#include "../host/settings.h"

#define BT_DBG_ENABLED IS_ENABLED(CONFIG_BT_DEBUG_SERVICE)
#define LOG_MODULE_NAME bt_dis
#include "common/log.h"

#if CONFIG_BT_GATT_DIS_PNP
struct dis_pnp {
	u8_t pnp_vid_src;
	u16_t pnp_vid;
	u16_t pnp_pid;
	u16_t pnp_ver;
} __packed;

static struct dis_pnp dis_pnp_id = {
	.pnp_vid_src = CONFIG_BT_GATT_DIS_PNP_VID_SRC,
	.pnp_vid = CONFIG_BT_GATT_DIS_PNP_VID,
	.pnp_pid = CONFIG_BT_GATT_DIS_PNP_PID,
	.pnp_ver = CONFIG_BT_GATT_DIS_PNP_VER,
};
#endif

#if defined(CONFIG_BT_GATT_DIS_SETTINGS)
static u8_t dis_model[CONFIG_BT_GATT_DIS_STR_MAX] = CONFIG_BT_GATT_DIS_MODEL;
static u8_t dis_manuf[CONFIG_BT_GATT_DIS_STR_MAX] = CONFIG_BT_GATT_DIS_MANUF;
#if defined(CONFIG_BT_GATT_DIS_SERIAL_NUMBER)
static u8_t dis_serial_number[CONFIG_BT_GATT_DIS_STR_MAX] =
	CONFIG_BT_GATT_DIS_SERIAL_NUMBER_STR;
#endif
#if defined(CONFIG_BT_GATT_DIS_FW_REV)
static u8_t dis_fw_rev[CONFIG_BT_GATT_DIS_STR_MAX] =
	CONFIG_BT_GATT_DIS_FW_REV_STR;
#endif
#if defined(CONFIG_BT_GATT_DIS_HW_REV)
static u8_t dis_hw_rev[CONFIG_BT_GATT_DIS_STR_MAX] =
	CONFIG_BT_GATT_DIS_HW_REV_STR;
#endif
#if defined(CONFIG_BT_GATT_DIS_SW_REV)
static u8_t dis_sw_rev[CONFIG_BT_GATT_DIS_STR_MAX] =
	CONFIG_BT_GATT_DIS_SW_REV_STR;
#endif

#define BT_GATT_DIS_MODEL_REF			dis_model
#define BT_GATT_DIS_MANUF_REF			dis_manuf
#define BT_GATT_DIS_SERIAL_NUMBER_STR_REF	dis_serial_number
#define BT_GATT_DIS_FW_REV_STR_REF		dis_fw_rev
#define BT_GATT_DIS_HW_REV_STR_REF		dis_hw_rev
#define BT_GATT_DIS_SW_REV_STR_REF		dis_sw_rev

#else /* CONFIG_BT_GATT_DIS_SETTINGS */

#define BT_GATT_DIS_MODEL_REF		    CONFIG_BT_GATT_DIS_MODEL
#define BT_GATT_DIS_MANUF_REF		    CONFIG_BT_GATT_DIS_MANUF
#define BT_GATT_DIS_SERIAL_NUMBER_STR_REF   CONFIG_BT_GATT_DIS_SERIAL_NUMBER_STR
#define BT_GATT_DIS_FW_REV_STR_REF	    CONFIG_BT_GATT_DIS_FW_REV_STR
#define BT_GATT_DIS_HW_REV_STR_REF	    CONFIG_BT_GATT_DIS_HW_REV_STR
#define BT_GATT_DIS_SW_REV_STR_REF	    CONFIG_BT_GATT_DIS_SW_REV_STR

#endif /* CONFIG_BT_GATT_DIS_SETTINGS */

static ssize_t read_str(struct bt_conn *conn,
			  const struct bt_gatt_attr *attr, void *buf,
			  u16_t len, u16_t offset)
{
	return bt_gatt_attr_read(conn, attr, buf, len, offset, attr->user_data,
				 strlen(attr->user_data));
}

#if CONFIG_BT_GATT_DIS_PNP
static ssize_t read_pnp_id(struct bt_conn *conn,
			   const struct bt_gatt_attr *attr, void *buf,
			   u16_t len, u16_t offset)
{
	return bt_gatt_attr_read(conn, attr, buf, len, offset, &dis_pnp_id,
				 sizeof(dis_pnp_id));
}
#endif

/* Device Information Service Declaration */
BT_GATT_SERVICE_DEFINE(dis_svc,
	BT_GATT_PRIMARY_SERVICE(BT_UUID_DIS),

	BT_GATT_CHARACTERISTIC(BT_UUID_DIS_MODEL_NUMBER,
			       BT_GATT_CHRC_READ, BT_GATT_PERM_READ,
			       read_str, NULL, BT_GATT_DIS_MODEL_REF),
	BT_GATT_CHARACTERISTIC(BT_UUID_DIS_MANUFACTURER_NAME,
			       BT_GATT_CHRC_READ, BT_GATT_PERM_READ,
			       read_str, NULL, BT_GATT_DIS_MANUF_REF),
#if CONFIG_BT_GATT_DIS_PNP
	BT_GATT_CHARACTERISTIC(BT_UUID_DIS_PNP_ID,
			       BT_GATT_CHRC_READ, BT_GATT_PERM_READ,
			       read_pnp_id, NULL, &dis_pnp_id),
#endif

#if defined(CONFIG_BT_GATT_DIS_SERIAL_NUMBER)
	BT_GATT_CHARACTERISTIC(BT_UUID_DIS_SERIAL_NUMBER,
			       BT_GATT_CHRC_READ, BT_GATT_PERM_READ,
			       read_str, NULL,
			       BT_GATT_DIS_SERIAL_NUMBER_STR_REF),
#endif
#if defined(CONFIG_BT_GATT_DIS_FW_REV)
	BT_GATT_CHARACTERISTIC(BT_UUID_DIS_FIRMWARE_REVISION,
			       BT_GATT_CHRC_READ, BT_GATT_PERM_READ,
			       read_str, NULL, BT_GATT_DIS_FW_REV_STR_REF),
#endif
#if defined(CONFIG_BT_GATT_DIS_HW_REV)
	BT_GATT_CHARACTERISTIC(BT_UUID_DIS_HARDWARE_REVISION,
			       BT_GATT_CHRC_READ, BT_GATT_PERM_READ,
			       read_str, NULL, BT_GATT_DIS_HW_REV_STR_REF),
#endif
#if defined(CONFIG_BT_GATT_DIS_SW_REV)
	BT_GATT_CHARACTERISTIC(BT_UUID_DIS_SOFTWARE_REVISION,
			       BT_GATT_CHRC_READ, BT_GATT_PERM_READ,
			       read_str, NULL, BT_GATT_DIS_SW_REV_STR_REF),
#endif

);

#if defined(CONFIG_BT_SETTINGS) && defined(CONFIG_BT_GATT_DIS_SETTINGS)
static int dis_set(int argc, char **argv, size_t len_rd,
		   settings_read_cb read_cb, void *store)
{
	int len;

	if (!strcmp(argv[0], "manuf")) {
		len = read_cb(store, &dis_manuf, sizeof(dis_manuf) - 1);
		if (len < 0) {
			BT_ERR("Failed to read manufacturer from storage"
				       " (err %d)", len);
		} else {
			dis_manuf[len] = '\0';

			BT_DBG("Manufacturer set to %s", dis_manuf);
		}
		return 0;
	}
	if (!strcmp(argv[0], "model")) {
		len = read_cb(store, &dis_model, sizeof(dis_model) - 1);
		if (len < 0) {
			BT_ERR("Failed to read model from storage"
				       " (err %d)", len);
		} else {
			dis_model[len] = '\0';

			BT_DBG("Model set to %s", dis_model);
		}
		return 0;
	}
#if defined(CONFIG_BT_GATT_DIS_SERIAL_NUMBER)
	if (!strcmp(argv[0], "serial")) {
		len = read_cb(store, &dis_serial_number,
			   sizeof(dis_serial_number) - 1);
		if (len < 0) {
			BT_ERR("Failed to read serial number from storage"
				       " (err %d)", len);
		} else {
			dis_serial_number[len] = '\0';

			BT_DBG("Serial number set to %s", dis_serial_number);
		}
		return 0;
	}
#endif
#if defined(CONFIG_BT_GATT_DIS_FW_REV)
	if (!strcmp(argv[0], "fw")) {
		len = read_cb(store, &dis_fw_rev, sizeof(dis_fw_rev) - 1);
		if (len < 0) {
			BT_ERR("Failed to read firmware revision from storage"
				       " (err %d)", len);
		} else {
			dis_fw_rev[len] = '\0';

			BT_DBG("Firmware revision set to %s", dis_fw_rev);
		}
		return 0;
	}
#endif
#if defined(CONFIG_BT_GATT_DIS_HW_REV)
	if (!strcmp(argv[0], "hw")) {
		len = read_cb(store, &dis_hw_rev, sizeof(dis_hw_rev) - 1);
		if (len < 0) {
			BT_ERR("Failed to read hardware revision from storage"
				       " (err %d)", len);
		} else {
			dis_hw_rev[len] = '\0';

			BT_DBG("Hardware revision set to %s", dis_hw_rev);
		}
		return 0;
	}
#endif
#if defined(CONFIG_BT_GATT_DIS_SW_REV)
	if (!strcmp(argv[0], "sw")) {
		len = read_cb(store, &dis_sw_rev, sizeof(dis_sw_rev) - 1);
		if (len < 0) {
			BT_ERR("Failed to read software revision from storage"
				       " (err %d)", len);
		} else {
			dis_sw_rev[len] = '\0';

			BT_DBG("Software revision set to %s", dis_sw_rev);
		}
		return 0;
	}
#endif
	return 0;
}

BT_SETTINGS_DEFINE(dis, dis_set, NULL, NULL);
#endif /* CONFIG_BT_GATT_DIS_SETTINGS && CONFIG_BT_SETTINGS*/
