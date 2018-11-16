/* gatt.c - Generic Attribute Profile handling */

/*
 * Copyright (c) 2015-2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <atomic.h>
#include <misc/byteorder.h>
#include <misc/util.h>

#include <settings/settings.h>

#include <bluetooth/hci.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/uuid.h>
#include <bluetooth/gatt.h>
#include <bluetooth/hci_driver.h>

#define BT_DBG_ENABLED IS_ENABLED(CONFIG_BT_DEBUG_GATT)
#define LOG_MODULE_NAME bt_gatt
#include "common/log.h"

#include "hci_core.h"
#include "conn_internal.h"
#include "keys.h"
#include "l2cap_internal.h"
#include "att_internal.h"
#include "smp.h"
#include "settings.h"
#include "gatt_internal.h"

#define SC_TIMEOUT	K_MSEC(10)
#define CCC_STORE_DELAY	K_SECONDS(1)

/* Persistent storage format for GATT CCC */
struct ccc_store {
	u16_t handle;
	u16_t value;
};

#if defined(CONFIG_BT_GATT_CLIENT)
static sys_slist_t subscriptions;
#endif /* CONFIG_BT_GATT_CLIENT */

static const u16_t gap_appearance = CONFIG_BT_DEVICE_APPEARANCE;

static sys_slist_t db;
static atomic_t init;

static ssize_t read_name(struct bt_conn *conn, const struct bt_gatt_attr *attr,
			 void *buf, u16_t len, u16_t offset)
{
	const char *name = bt_get_name();

	return bt_gatt_attr_read(conn, attr, buf, len, offset, name,
				 strlen(name));
}

#if defined(CONFIG_BT_DEVICE_NAME_GATT_WRITABLE)

static ssize_t write_name(struct bt_conn *conn, const struct bt_gatt_attr *attr,
			 const void *buf, u16_t len, u16_t offset,
			 u8_t flags)
{
	char value[CONFIG_BT_DEVICE_NAME_MAX] = {};

	if (offset) {
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
	}

	if (len >= sizeof(value)) {
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
	}

	memcpy(value, buf, len);

	bt_set_name(value);

	return len;
}

#endif /* CONFIG_BT_DEVICE_NAME_GATT_WRITABLE */

static ssize_t read_appearance(struct bt_conn *conn,
			       const struct bt_gatt_attr *attr, void *buf,
			       u16_t len, u16_t offset)
{
	u16_t appearance = sys_cpu_to_le16(gap_appearance);

	return bt_gatt_attr_read(conn, attr, buf, len, offset, &appearance,
				 sizeof(appearance));
}

#if defined (CONFIG_BT_GAP_PERIPHERAL_PREF_PARAMS)
static ssize_t read_ppcp(struct bt_conn *conn, const struct bt_gatt_attr *attr,
			 void *buf, u16_t len, u16_t offset)
{
	struct __packed {
		uint16_t min_int;
		uint16_t max_int;
		uint16_t latency;
		uint16_t timeout;
	} ppcp;

	ppcp.min_int = sys_cpu_to_le16(CONFIG_BT_PERIPHERAL_PREF_MIN_INT);
	ppcp.max_int = sys_cpu_to_le16(CONFIG_BT_PERIPHERAL_PREF_MAX_INT);
	ppcp.latency = sys_cpu_to_le16(CONFIG_BT_PERIPHERAL_PREF_SLAVE_LATENCY);
	ppcp.timeout = sys_cpu_to_le16(CONFIG_BT_PERIPHERAL_PREF_TIMEOUT);

	return bt_gatt_attr_read(conn, attr, buf, len, offset, &ppcp,
				 sizeof(ppcp));
}
#endif

#if defined(CONFIG_BT_CENTRAL) && defined(CONFIG_BT_PRIVACY)
static ssize_t read_central_addr_res(struct bt_conn *conn,
				     const struct bt_gatt_attr *attr, void *buf,
				     u16_t len, u16_t offset)
{
	u8_t central_addr_res = BT_GATT_CENTRAL_ADDR_RES_SUPP;

	return bt_gatt_attr_read(conn, attr, buf, len, offset,
				 &central_addr_res, sizeof(central_addr_res));
}
#endif /* CONFIG_BT_CENTRAL && CONFIG_BT_PRIVACY */

static struct bt_gatt_attr gap_attrs[] = {
	BT_GATT_PRIMARY_SERVICE(BT_UUID_GAP),
#if defined(CONFIG_BT_DEVICE_NAME_GATT_WRITABLE)
	/* Require pairing for writes to device name */
	BT_GATT_CHARACTERISTIC(BT_UUID_GAP_DEVICE_NAME,
			       BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE,
			       BT_GATT_PERM_READ | BT_GATT_PERM_WRITE_ENCRYPT,
			       read_name, write_name, bt_dev.name),
#else
	BT_GATT_CHARACTERISTIC(BT_UUID_GAP_DEVICE_NAME, BT_GATT_CHRC_READ,
			       BT_GATT_PERM_READ, read_name, NULL, NULL),
#endif /* CONFIG_BT_DEVICE_NAME_GATT_WRITABLE */
	BT_GATT_CHARACTERISTIC(BT_UUID_GAP_APPEARANCE, BT_GATT_CHRC_READ,
			       BT_GATT_PERM_READ, read_appearance, NULL, NULL),
#if defined(CONFIG_BT_CENTRAL) && defined(CONFIG_BT_PRIVACY)
	BT_GATT_CHARACTERISTIC(BT_UUID_CENTRAL_ADDR_RES,
			       BT_GATT_CHRC_READ, BT_GATT_PERM_READ,
			       read_central_addr_res, NULL, NULL),
#endif /* CONFIG_BT_CENTRAL && CONFIG_BT_PRIVACY */
#if defined(CONFIG_BT_GAP_PERIPHERAL_PREF_PARAMS)
	BT_GATT_CHARACTERISTIC(BT_UUID_GAP_PPCP, BT_GATT_CHRC_READ,
			       BT_GATT_PERM_READ, read_ppcp, NULL, NULL),
#endif
};

static struct bt_gatt_service gap_svc = BT_GATT_SERVICE(gap_attrs);

static struct bt_gatt_ccc_cfg sc_ccc_cfg[BT_GATT_CCC_MAX] = {};

static void sc_ccc_cfg_changed(const struct bt_gatt_attr *attr,
			       u16_t value)
{
	BT_DBG("value 0x%04x", value);
}

static struct bt_gatt_attr gatt_attrs[] = {
	BT_GATT_PRIMARY_SERVICE(BT_UUID_GATT),
	BT_GATT_CHARACTERISTIC(BT_UUID_GATT_SC, BT_GATT_CHRC_INDICATE,
			       BT_GATT_PERM_NONE, NULL, NULL, NULL),
	BT_GATT_CCC(sc_ccc_cfg, sc_ccc_cfg_changed),
};

static struct bt_gatt_service gatt_svc = BT_GATT_SERVICE(gatt_attrs);

static int gatt_register(struct bt_gatt_service *svc)
{
	struct bt_gatt_service *last;
	u16_t handle;
	struct bt_gatt_attr *attrs = svc->attrs;
	u16_t count = svc->attr_count;

	if (sys_slist_is_empty(&db)) {
		handle = 0;
		goto populate;
	}

	last = SYS_SLIST_PEEK_TAIL_CONTAINER(&db, last, node);
	handle = last->attrs[last->attr_count - 1].handle;

populate:
	/* Populate the handles and append them to the list */
	for (; attrs && count; attrs++, count--) {
		if (!attrs->handle) {
			/* Allocate handle if not set already */
			attrs->handle = ++handle;
		} else if (attrs->handle > handle) {
			/* Use existing handle if valid */
			handle = attrs->handle;
		} else {
			/* Service has conflicting handles */
			BT_ERR("Unable to register handle 0x%04x",
			       attrs->handle);
			return -EINVAL;
		}

		BT_DBG("attr %p handle 0x%04x uuid %s perm 0x%02x",
		       attrs, attrs->handle, bt_uuid_str(attrs->uuid),
		       attrs->perm);
	}

	sys_slist_append(&db, &svc->node);

	return 0;
}

enum {
	SC_RANGE_CHANGED,    /* SC range changed */
	SC_INDICATE_PENDING, /* SC indicate pending */

	/* Total number of flags - must be at the end of the enum */
	SC_NUM_FLAGS,
};

static struct gatt_sc {
	struct bt_gatt_indicate_params params;
	u16_t start;
	u16_t end;
	struct k_delayed_work work;
	ATOMIC_DEFINE(flags, SC_NUM_FLAGS);
} gatt_sc;

static void sc_indicate_rsp(struct bt_conn *conn,
			    const struct bt_gatt_attr *attr, u8_t err)
{
	BT_DBG("err 0x%02x", err);

	atomic_clear_bit(gatt_sc.flags, SC_INDICATE_PENDING);

	/* Check if there is new change in the meantime */
	if (atomic_test_bit(gatt_sc.flags, SC_RANGE_CHANGED)) {
		/* Reschedule without any delay since it is waiting already */
		k_delayed_work_submit(&gatt_sc.work, K_NO_WAIT);
	}
}

static void sc_process(struct k_work *work)
{
	struct gatt_sc *sc = CONTAINER_OF(work, struct gatt_sc, work);
	u16_t sc_range[2];

	__ASSERT(!atomic_test_bit(sc->flags, SC_INDICATE_PENDING),
		 "Indicate already pending");

	BT_DBG("start 0x%04x end 0x%04x", sc->start, sc->end);

	sc_range[0] = sys_cpu_to_le16(sc->start);
	sc_range[1] = sys_cpu_to_le16(sc->end);

	atomic_clear_bit(sc->flags, SC_RANGE_CHANGED);
	sc->start = 0;
	sc->end = 0;

	sc->params.attr = &gatt_attrs[2];
	sc->params.func = sc_indicate_rsp;
	sc->params.data = &sc_range[0];
	sc->params.len = sizeof(sc_range);

	if (bt_gatt_indicate(NULL, &sc->params)) {
		/* No connections to indicate */
		return;
	}

	atomic_set_bit(sc->flags, SC_INDICATE_PENDING);
}

#if defined(CONFIG_BT_SETTINGS_CCC_STORE_ON_WRITE)
static struct gatt_ccc_store {
	struct bt_conn *conn_list[CONFIG_BT_MAX_CONN];
	struct k_delayed_work work;
} gatt_ccc_store;

static bool gatt_ccc_conn_is_queued(struct bt_conn *conn)
{
	return (conn == gatt_ccc_store.conn_list[bt_conn_get_id(conn)]);
}

static void gatt_ccc_conn_unqueue(struct bt_conn *conn)
{
	u8_t index = bt_conn_get_id(conn);

	if (gatt_ccc_store.conn_list[index] != NULL) {
		bt_conn_unref(gatt_ccc_store.conn_list[index]);
		gatt_ccc_store.conn_list[index] = NULL;
	}
}

static bool gatt_ccc_conn_queue_is_empty(void)
{
	for (size_t i = 0; i < CONFIG_BT_MAX_CONN; i++) {
		if (gatt_ccc_store.conn_list[i]) {
			return false;
		}
	}

	return true;
}

static void ccc_delayed_store(struct k_work *work)
{
	struct gatt_ccc_store *ccc_store =
		CONTAINER_OF(work, struct gatt_ccc_store, work);

	for (size_t i = 0; i < CONFIG_BT_MAX_CONN; i++) {
		struct bt_conn *conn = ccc_store->conn_list[i];

		if (!conn) {
			continue;
		}

		if (bt_addr_le_is_bonded(conn->id, &conn->le.dst)) {
			bt_gatt_store_ccc(conn->id, &conn->le.dst);
			bt_conn_unref(conn);
			ccc_store->conn_list[i] = NULL;
		}
	}
}
#endif

void bt_gatt_init(void)
{
	if (!atomic_cas(&init, 0, 1)) {
		return;
	}

	/* Register mandatory services */
	gatt_register(&gap_svc);
	gatt_register(&gatt_svc);

	k_delayed_work_init(&gatt_sc.work, sc_process);
#if defined(CONFIG_BT_SETTINGS_CCC_STORE_ON_WRITE)
	k_delayed_work_init(&gatt_ccc_store.work, ccc_delayed_store);
#endif
}

static bool update_range(u16_t *start, u16_t *end, u16_t new_start,
			 u16_t new_end)
{
	BT_DBG("start 0x%04x end 0x%04x new_start 0x%04x new_end 0x%04x",
	       *start, *end, new_start, new_end);

	/* Check if inside existing range */
	if (new_start >= *start && new_end <= *end) {
		return false;
	}

	/* Update range */
	if (*start > new_start) {
		*start = new_start;
	}

	if (*end < new_end) {
		*end = new_end;
	}

	return true;
}

static void sc_indicate(struct gatt_sc *sc, uint16_t start, uint16_t end)
{
	if (!atomic_test_and_set_bit(sc->flags, SC_RANGE_CHANGED)) {
		sc->start = start;
		sc->end = end;
		goto submit;
	}

	if (!update_range(&sc->start, &sc->end, start, end)) {
		return;
	}

submit:
	if (atomic_test_bit(sc->flags, SC_INDICATE_PENDING)) {
		BT_DBG("indicate pending, waiting until complete...");
		return;
	}

	/* Reschedule since the range has changed */
	k_delayed_work_submit(&sc->work, SC_TIMEOUT);
}

int bt_gatt_service_register(struct bt_gatt_service *svc)
{
	int err;

	__ASSERT(svc, "invalid parameters\n");
	__ASSERT(svc->attrs, "invalid parameters\n");
	__ASSERT(svc->attr_count, "invalid parameters\n");

	/* Init GATT core services */
	bt_gatt_init();

	/* Do no allow to register mandatory services twice */
	if (!bt_uuid_cmp(svc->attrs[0].uuid, BT_UUID_GAP) ||
	    !bt_uuid_cmp(svc->attrs[0].uuid, BT_UUID_GATT)) {
		return -EALREADY;
	}

	err = gatt_register(svc);
	if (err < 0) {
		return err;
	}

	sc_indicate(&gatt_sc, svc->attrs[0].handle,
		    svc->attrs[svc->attr_count - 1].handle);

	return 0;
}

int bt_gatt_service_unregister(struct bt_gatt_service *svc)
{
	__ASSERT(svc, "invalid parameters\n");

	if (!sys_slist_find_and_remove(&db, &svc->node)) {
		return -ENOENT;
	}

	sc_indicate(&gatt_sc, svc->attrs[0].handle,
		    svc->attrs[svc->attr_count - 1].handle);

	return 0;
}

ssize_t bt_gatt_attr_read(struct bt_conn *conn, const struct bt_gatt_attr *attr,
			  void *buf, u16_t buf_len, u16_t offset,
			  const void *value, u16_t value_len)
{
	u16_t len;

	if (offset > value_len) {
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
	}

	len = min(buf_len, value_len - offset);

	BT_DBG("handle 0x%04x offset %u length %u", attr->handle, offset,
	       len);

	memcpy(buf, (u8_t *)value + offset, len);

	return len;
}

ssize_t bt_gatt_attr_read_service(struct bt_conn *conn,
				  const struct bt_gatt_attr *attr,
				  void *buf, u16_t len, u16_t offset)
{
	struct bt_uuid *uuid = attr->user_data;

	if (uuid->type == BT_UUID_TYPE_16) {
		u16_t uuid16 = sys_cpu_to_le16(BT_UUID_16(uuid)->val);

		return bt_gatt_attr_read(conn, attr, buf, len, offset,
					 &uuid16, 2);
	}

	return bt_gatt_attr_read(conn, attr, buf, len, offset,
				 BT_UUID_128(uuid)->val, 16);
}

struct gatt_incl {
	u16_t start_handle;
	u16_t end_handle;
	u16_t uuid16;
} __packed;

static u8_t get_service_handles(const struct bt_gatt_attr *attr,
				   void *user_data)
{
	struct gatt_incl *include = user_data;

	/* Stop if attribute is a service */
	if (!bt_uuid_cmp(attr->uuid, BT_UUID_GATT_PRIMARY) ||
	    !bt_uuid_cmp(attr->uuid, BT_UUID_GATT_SECONDARY)) {
		return BT_GATT_ITER_STOP;
	}

	include->end_handle = attr->handle;

	return BT_GATT_ITER_CONTINUE;
}

ssize_t bt_gatt_attr_read_included(struct bt_conn *conn,
				   const struct bt_gatt_attr *attr,
				   void *buf, u16_t len, u16_t offset)
{
	struct bt_gatt_attr *incl = attr->user_data;
	struct bt_uuid *uuid = incl->user_data;
	struct gatt_incl pdu;
	u8_t value_len;

	/* first attr points to the start handle */
	pdu.start_handle = sys_cpu_to_le16(incl->handle);
	value_len = sizeof(pdu.start_handle) + sizeof(pdu.end_handle);

	/*
	 * Core 4.2, Vol 3, Part G, 3.2,
	 * The Service UUID shall only be present when the UUID is a
	 * 16-bit Bluetooth UUID.
	 */
	if (uuid->type == BT_UUID_TYPE_16) {
		pdu.uuid16 = sys_cpu_to_le16(BT_UUID_16(uuid)->val);
		value_len += sizeof(pdu.uuid16);
	}

	/* Lookup for service end handle */
	bt_gatt_foreach_attr(incl->handle + 1, 0xffff, get_service_handles,
			     &pdu);

	return bt_gatt_attr_read(conn, attr, buf, len, offset, &pdu, value_len);
}

struct gatt_chrc {
	u8_t properties;
	u16_t value_handle;
	union {
		u16_t uuid16;
		u8_t  uuid[16];
	};
} __packed;

ssize_t bt_gatt_attr_read_chrc(struct bt_conn *conn,
			       const struct bt_gatt_attr *attr, void *buf,
			       u16_t len, u16_t offset)
{
	struct bt_gatt_chrc *chrc = attr->user_data;
	struct gatt_chrc pdu;
	const struct bt_gatt_attr *next;
	u8_t value_len;

	pdu.properties = chrc->properties;
	/* BLUETOOTH SPECIFICATION Version 4.2 [Vol 3, Part G] page 534:
	 * 3.3.2 Characteristic Value Declaration
	 * The Characteristic Value declaration contains the value of the
	 * characteristic. It is the first Attribute after the characteristic
	 * declaration. All characteristic definitions shall have a
	 * Characteristic Value declaration.
	 */
	next = bt_gatt_attr_next(attr);
	if (!next) {
		BT_WARN("No value for characteristic at 0x%04x", attr->handle);
		pdu.value_handle = 0x0000;
	} else {
		pdu.value_handle = sys_cpu_to_le16(next->handle);
	}
	value_len = sizeof(pdu.properties) + sizeof(pdu.value_handle);

	if (chrc->uuid->type == BT_UUID_TYPE_16) {
		pdu.uuid16 = sys_cpu_to_le16(BT_UUID_16(chrc->uuid)->val);
		value_len += 2;
	} else {
		memcpy(pdu.uuid, BT_UUID_128(chrc->uuid)->val, 16);
		value_len += 16;
	}

	return bt_gatt_attr_read(conn, attr, buf, len, offset, &pdu, value_len);
}

void bt_gatt_foreach_attr(u16_t start_handle, u16_t end_handle,
			  bt_gatt_attr_func_t func, void *user_data)
{
	struct bt_gatt_service *svc;

	SYS_SLIST_FOR_EACH_CONTAINER(&db, svc, node) {
		int i;

		for (i = 0; i < svc->attr_count; i++) {
			struct bt_gatt_attr *attr = &svc->attrs[i];

			/* Check if attribute handle is within range */
			if (attr->handle < start_handle ||
			    attr->handle > end_handle) {
				continue;
			}

			if (func(attr, user_data) == BT_GATT_ITER_STOP) {
				return;
			}
		}
	}
}

static u8_t find_next(const struct bt_gatt_attr *attr, void *user_data)
{
	struct bt_gatt_attr **next = user_data;

	*next = (struct bt_gatt_attr *)attr;

	return BT_GATT_ITER_STOP;
}

struct bt_gatt_attr *bt_gatt_attr_next(const struct bt_gatt_attr *attr)
{
	struct bt_gatt_attr *next = NULL;

	bt_gatt_foreach_attr(attr->handle + 1, attr->handle + 1, find_next,
			     &next);

	return next;
}

ssize_t bt_gatt_attr_read_ccc(struct bt_conn *conn,
			      const struct bt_gatt_attr *attr, void *buf,
			      u16_t len, u16_t offset)
{
	struct _bt_gatt_ccc *ccc = attr->user_data;
	u16_t value;
	size_t i;

	for (i = 0; i < ccc->cfg_len; i++) {
		if (bt_conn_addr_le_cmp(conn, &ccc->cfg[i].peer)) {
			continue;
		}

		value = sys_cpu_to_le16(ccc->cfg[i].value);
		break;
	}

	/* Default to disable if there is no cfg for the peer */
	if (i == ccc->cfg_len) {
		value = 0x0000;
	}

	return bt_gatt_attr_read(conn, attr, buf, len, offset, &value,
				 sizeof(value));
}

static void gatt_ccc_changed(const struct bt_gatt_attr *attr,
			     struct _bt_gatt_ccc *ccc)
{
	int i;
	u16_t value = 0x0000;

	for (i = 0; i < ccc->cfg_len; i++) {
		if (ccc->cfg[i].value > value) {
			value = ccc->cfg[i].value;
		}
	}

	BT_DBG("ccc %p value 0x%04x", ccc, value);

	if (value != ccc->value) {
		ccc->value = value;
		if (ccc->cfg_changed) {
			ccc->cfg_changed(attr, value);
		}
	}
}

ssize_t bt_gatt_attr_write_ccc(struct bt_conn *conn,
			       const struct bt_gatt_attr *attr, const void *buf,
			       u16_t len, u16_t offset, u8_t flags)
{
	struct _bt_gatt_ccc *ccc = attr->user_data;
	u16_t value;
	size_t i;

	if (offset > sizeof(u16_t)) {
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
	}

	if (offset + len > sizeof(u16_t)) {
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
	}

	value = sys_get_le16(buf);

	for (i = 0; i < ccc->cfg_len; i++) {
		/* Check for existing configuration */
		if (!bt_conn_addr_le_cmp(conn, &ccc->cfg[i].peer)) {
			break;
		}
	}

	if (i == ccc->cfg_len) {
		/* If there's no existing entry, but the new value is zero,
		 * we don't need to do anything, since a disabled CCC is
		 * behavioraly the same as no written CCC.
		 */
		if (!value) {
			return len;
		}

		for (i = 0; i < ccc->cfg_len; i++) {
			/* Check for unused configuration */
			if (bt_addr_le_cmp(&ccc->cfg[i].peer, BT_ADDR_LE_ANY)) {
				continue;
			}

			bt_addr_le_copy(&ccc->cfg[i].peer, &conn->le.dst);
			break;
		}

		if (i == ccc->cfg_len) {
			BT_WARN("No space to store CCC cfg");
			return BT_GATT_ERR(BT_ATT_ERR_INSUFFICIENT_RESOURCES);
		}
	}

	ccc->cfg[i].value = value;

	BT_DBG("handle 0x%04x value %u", attr->handle, ccc->cfg[i].value);

	/* Update cfg if don't match */
	if (ccc->cfg[i].value != ccc->value) {
		gatt_ccc_changed(attr, ccc);

#if defined(CONFIG_BT_SETTINGS_CCC_STORE_ON_WRITE)
		if ((!gatt_ccc_conn_is_queued(conn)) &&
		    bt_addr_le_is_bonded(conn->id, &conn->le.dst)) {
			/* Store the connection with the same index it has in
			 * the conns array
			 */
			gatt_ccc_store.conn_list[bt_conn_get_id(conn)] =
				bt_conn_ref(conn);
			k_delayed_work_submit(&gatt_ccc_store.work,
					      CCC_STORE_DELAY);
		}
#endif
	}

	/* Disabled CCC is the same as no configured CCC, so clear the entry */
	if (!value) {
		bt_addr_le_copy(&ccc->cfg[i].peer, BT_ADDR_LE_ANY);
		ccc->cfg[i].value = 0;
	}

	return len;
}

ssize_t bt_gatt_attr_read_cep(struct bt_conn *conn,
			      const struct bt_gatt_attr *attr, void *buf,
			      u16_t len, u16_t offset)
{
	const struct bt_gatt_cep *value = attr->user_data;
	u16_t props = sys_cpu_to_le16(value->properties);

	return bt_gatt_attr_read(conn, attr, buf, len, offset, &props,
				 sizeof(props));
}

ssize_t bt_gatt_attr_read_cud(struct bt_conn *conn,
			      const struct bt_gatt_attr *attr, void *buf,
			      u16_t len, u16_t offset)
{
	const char *value = attr->user_data;

	return bt_gatt_attr_read(conn, attr, buf, len, offset, value,
				 strlen(value));
}

ssize_t bt_gatt_attr_read_cpf(struct bt_conn *conn,
			      const struct bt_gatt_attr *attr, void *buf,
			      u16_t len, u16_t offset)
{
	const struct bt_gatt_cpf *value = attr->user_data;

	return bt_gatt_attr_read(conn, attr, buf, len, offset, value,
				 sizeof(*value));
}

struct notify_data {
	int err;
	u16_t type;
	const struct bt_gatt_attr *attr;
	bt_gatt_notify_complete_func_t func;
	const void *data;
	u16_t len;
	struct bt_gatt_indicate_params *params;
};

static int gatt_notify(struct bt_conn *conn, u16_t handle, const void *data,
		       size_t len, bt_gatt_notify_complete_func_t cb)
{
	struct net_buf *buf;
	struct bt_att_notify *nfy;

	buf = bt_att_create_pdu(conn, BT_ATT_OP_NOTIFY, sizeof(*nfy) + len);
	if (!buf) {
		BT_WARN("No buffer available to send notification");
		return -ENOMEM;
	}

	BT_DBG("conn %p handle 0x%04x", conn, handle);

	nfy = net_buf_add(buf, sizeof(*nfy));
	nfy->handle = sys_cpu_to_le16(handle);

	net_buf_add(buf, len);
	memcpy(nfy->value, data, len);

	bt_l2cap_send_cb(conn, BT_L2CAP_CID_ATT, buf, cb);

	return 0;
}

static void gatt_indicate_rsp(struct bt_conn *conn, u8_t err,
			      const void *pdu, u16_t length, void *user_data)
{
	struct bt_gatt_indicate_params *params = user_data;

	params->func(conn, params->attr, err);
}

static int gatt_send(struct bt_conn *conn, struct net_buf *buf,
		     bt_att_func_t func, void *params,
		     bt_att_destroy_t destroy)
{
	int err;

	if (params) {
		struct bt_att_req *req = params;

		req->buf = buf;
		req->func = func;
		req->destroy = destroy;

		err = bt_att_req_send(conn, req);
	} else {
		err = bt_att_send(conn, buf);
	}

	if (err) {
		BT_ERR("Error sending ATT PDU: %d", err);
		net_buf_unref(buf);
	}

	return err;
}

static int gatt_indicate(struct bt_conn *conn,
			 struct bt_gatt_indicate_params *params)
{
	struct net_buf *buf;
	struct bt_att_indicate *ind;
	u16_t value_handle = params->attr->handle;

	/* Check if attribute is a characteristic then adjust the handle */
	if (!bt_uuid_cmp(params->attr->uuid, BT_UUID_GATT_CHRC)) {
		struct bt_gatt_chrc *chrc = params->attr->user_data;

		if (!(chrc->properties & BT_GATT_CHRC_INDICATE)) {
			return -EINVAL;
		}

		value_handle += 1;
	}

	buf = bt_att_create_pdu(conn, BT_ATT_OP_INDICATE,
				sizeof(*ind) + params->len);
	if (!buf) {
		BT_WARN("No buffer available to send indication");
		return -ENOMEM;
	}

	BT_DBG("conn %p handle 0x%04x", conn, value_handle);

	ind = net_buf_add(buf, sizeof(*ind));
	ind->handle = sys_cpu_to_le16(value_handle);

	net_buf_add(buf, params->len);
	memcpy(ind->value, params->data, params->len);

	return gatt_send(conn, buf, gatt_indicate_rsp, params, NULL);
}

struct sc_data {
	u16_t start;
	u16_t end;
};

static void sc_save(struct bt_gatt_ccc_cfg *cfg,
		    struct bt_gatt_indicate_params *params)
{
	struct sc_data data;
	struct sc_data *stored;

	memcpy(&data, params->data, params->len);

	data.start = sys_le16_to_cpu(data.start);
	data.end = sys_le16_to_cpu(data.end);

	/* Load data stored */
	stored = (struct sc_data *)cfg->data;

	/* Check if there is any change stored */
	if (!stored->start && !stored->end) {
		*stored = data;
		goto done;
	}

	update_range(&stored->start, &stored->end,
		     data.start, data.end);

done:
	BT_DBG("peer %s start 0x%04x end 0x%04x", bt_addr_le_str(&cfg->peer),
	       stored->start, stored->end);
}

static u8_t notify_cb(const struct bt_gatt_attr *attr, void *user_data)
{
	struct notify_data *data = user_data;
	struct _bt_gatt_ccc *ccc;
	size_t i;

	if (bt_uuid_cmp(attr->uuid, BT_UUID_GATT_CCC)) {
		/* Stop if we reach the next characteristic */
		if (!bt_uuid_cmp(attr->uuid, BT_UUID_GATT_CHRC)) {
			return BT_GATT_ITER_STOP;
		}
		return BT_GATT_ITER_CONTINUE;
	}

	/* Check attribute user_data must be of type struct _bt_gatt_ccc */
	if (attr->write != bt_gatt_attr_write_ccc) {
		return BT_GATT_ITER_CONTINUE;
	}

	ccc = attr->user_data;

	/* Notify all peers configured */
	for (i = 0; i < ccc->cfg_len; i++) {
		struct bt_gatt_ccc_cfg *cfg = &ccc->cfg[i];
		struct bt_conn *conn;
		int err;

		/* Check if config value matches data type since consolidated
		 * value may be for a different peer.
		 */
		if (cfg->value != data->type) {
			continue;
		}

		conn = bt_conn_lookup_addr_le(cfg->id, &cfg->peer);
		if (!conn) {
			if (ccc->cfg == sc_ccc_cfg) {
				sc_save(cfg, data->params);
			}
			continue;
		}

		if (conn->state != BT_CONN_CONNECTED) {
			bt_conn_unref(conn);
			continue;
		}

		if (data->type == BT_GATT_CCC_INDICATE) {
			err = gatt_indicate(conn, data->params);
		} else {
			err = gatt_notify(conn, data->attr->handle,
					  data->data, data->len,
					  data->func);
		}

		bt_conn_unref(conn);

		if (err < 0) {
			return BT_GATT_ITER_STOP;
		}

		data->err = 0;
	}

	return BT_GATT_ITER_CONTINUE;
}

int bt_gatt_notify_cb(struct bt_conn *conn, const struct bt_gatt_attr *attr,
		      const void *data, u16_t len,
		      bt_gatt_notify_complete_func_t func)
{
	struct notify_data nfy;

	__ASSERT(attr && attr->handle,
		 "invalid parameters\n");

	/* Check if attribute is a characteristic then adjust the handle */
	if (!bt_uuid_cmp(attr->uuid, BT_UUID_GATT_CHRC)) {
		struct bt_gatt_chrc *chrc = attr->user_data;

		if (!(chrc->properties & BT_GATT_CHRC_NOTIFY)) {
			return -EINVAL;
		}

		attr++;
	}

	if (conn) {
		return gatt_notify(conn, attr->handle, data,
				   len, func);
	}

	nfy.err = -ENOTCONN;
	nfy.attr = attr;
	nfy.func = func;
	nfy.type = BT_GATT_CCC_NOTIFY;
	nfy.data = data;
	nfy.len = len;

	bt_gatt_foreach_attr(attr->handle, 0xffff, notify_cb, &nfy);

	return nfy.err;
}

int bt_gatt_indicate(struct bt_conn *conn,
		     struct bt_gatt_indicate_params *params)
{
	struct notify_data nfy;

	__ASSERT(params, "invalid parameters\n");
	__ASSERT(params->attr && params->attr->handle, "invalid parameters\n");

	if (conn) {
		return gatt_indicate(conn, params);
	}

	nfy.err = -ENOTCONN;
	nfy.type = BT_GATT_CCC_INDICATE;
	nfy.params = params;

	bt_gatt_foreach_attr(params->attr->handle, 0xffff, notify_cb, &nfy);

	return nfy.err;
}

u16_t bt_gatt_get_mtu(struct bt_conn *conn)
{
	return bt_att_get_mtu(conn);
}

static void sc_restore(struct bt_gatt_ccc_cfg *cfg)
{
	struct sc_data *data = (struct sc_data *)cfg->data;

	if (!data->start && !data->end) {
		return;
	}

	BT_DBG("peer %s start 0x%04x end 0x%04x", bt_addr_le_str(&cfg->peer),
	       data->start, data->end);

	sc_indicate(&gatt_sc, data->start, data->end);

	/* Reset config data */
	(void)memset(cfg->data, 0, sizeof(cfg->data));
}

static u8_t connected_cb(const struct bt_gatt_attr *attr, void *user_data)
{
	struct bt_conn *conn = user_data;
	struct _bt_gatt_ccc *ccc;
	size_t i;

	/* Check attribute user_data must be of type struct _bt_gatt_ccc */
	if (attr->write != bt_gatt_attr_write_ccc) {
		return BT_GATT_ITER_CONTINUE;
	}

	ccc = attr->user_data;

	for (i = 0; i < ccc->cfg_len; i++) {
		/* Ignore configuration for different peer */
		if (bt_conn_addr_le_cmp(conn, &ccc->cfg[i].peer)) {
			continue;
		}

		if (ccc->cfg[i].value) {
			gatt_ccc_changed(attr, ccc);
			if (ccc->cfg == sc_ccc_cfg) {
				sc_restore(&ccc->cfg[i]);
			}
			return BT_GATT_ITER_CONTINUE;
		}
	}

	return BT_GATT_ITER_CONTINUE;
}

static u8_t disconnected_cb(const struct bt_gatt_attr *attr, void *user_data)
{
	struct bt_conn *conn = user_data;
	struct _bt_gatt_ccc *ccc;
	size_t i;

	/* Check attribute user_data must be of type struct _bt_gatt_ccc */
	if (attr->write != bt_gatt_attr_write_ccc) {
		return BT_GATT_ITER_CONTINUE;
	}

	ccc = attr->user_data;

	/* If already disabled skip */
	if (!ccc->value) {
		return BT_GATT_ITER_CONTINUE;
	}

	for (i = 0; i < ccc->cfg_len; i++) {
		struct bt_gatt_ccc_cfg *cfg = &ccc->cfg[i];

		/* Ignore configurations with disabled value */
		if (!cfg->value) {
			continue;
		}

		if (conn->id != cfg->id ||
		    bt_conn_addr_le_cmp(conn, &cfg->peer)) {
			struct bt_conn *tmp;

			/* Skip if there is another peer connected */
			tmp = bt_conn_lookup_addr_le(cfg->id, &cfg->peer);
			if (tmp) {
				if (tmp->state == BT_CONN_CONNECTED) {
					bt_conn_unref(tmp);
					return BT_GATT_ITER_CONTINUE;
				}

				bt_conn_unref(tmp);
			}
		} else {
			/* Clear value if not paired */
			if (!bt_addr_le_is_bonded(conn->id, &conn->le.dst)) {
				bt_addr_le_copy(&cfg->peer, BT_ADDR_LE_ANY);
				cfg->value = 0;
			} else {
				/* Update address in case it has changed */
				bt_addr_le_copy(&cfg->peer, &conn->le.dst);
			}
		}
	}

	/* Reset value while disconnected */
	(void)memset(&ccc->value, 0, sizeof(ccc->value));
	if (ccc->cfg_changed) {
		ccc->cfg_changed(attr, ccc->value);
	}

	BT_DBG("ccc %p reseted", ccc);

	return BT_GATT_ITER_CONTINUE;
}

#if defined(CONFIG_BT_GATT_CLIENT)
void bt_gatt_notification(struct bt_conn *conn, u16_t handle,
			  const void *data, u16_t length)
{
	struct bt_gatt_subscribe_params *params, *tmp;

	BT_DBG("handle 0x%04x length %u", handle, length);

	SYS_SLIST_FOR_EACH_CONTAINER_SAFE(&subscriptions, params, tmp, node) {
		if (bt_conn_addr_le_cmp(conn, &params->_peer) ||
		    handle != params->value_handle) {
			continue;
		}

		if (params->notify(conn, params, data, length) ==
		    BT_GATT_ITER_STOP) {
			bt_gatt_unsubscribe(conn, params);
		}
	}
}

static void update_subscription(struct bt_conn *conn,
				 struct bt_gatt_subscribe_params *params)
{
	if (params->_peer.type == BT_ADDR_LE_PUBLIC) {
		return;
	}

	/* Update address */
	bt_addr_le_copy(&params->_peer, &conn->le.dst);
}

static void gatt_subscription_remove(struct bt_conn *conn, sys_snode_t *prev,
				     struct bt_gatt_subscribe_params *params)
{
	/* Remove subscription from the list*/
	sys_slist_remove(&subscriptions, prev, &params->node);

	params->notify(conn, params, NULL, 0);
}

static void remove_subscriptions(struct bt_conn *conn)
{
	struct bt_gatt_subscribe_params *params, *tmp;
	sys_snode_t *prev = NULL;

	/* Lookup existing subscriptions */
	SYS_SLIST_FOR_EACH_CONTAINER_SAFE(&subscriptions, params, tmp, node) {
		if (bt_conn_addr_le_cmp(conn, &params->_peer)) {
			prev = &params->node;
			continue;
		}

		if (!bt_addr_le_is_bonded(conn->id, &conn->le.dst) ||
		    (params->flags & BT_GATT_SUBSCRIBE_FLAG_VOLATILE)) {
			/* Remove subscription */
			params->value = 0;
			gatt_subscription_remove(conn, prev, params);
		} else {
			update_subscription(conn, params);
			prev = &params->node;
		}
	}
}

static void gatt_mtu_rsp(struct bt_conn *conn, u8_t err, const void *pdu,
			 u16_t length, void *user_data)
{
	struct bt_gatt_exchange_params *params = user_data;

	params->func(conn, err, params);
}

int bt_gatt_exchange_mtu(struct bt_conn *conn,
			 struct bt_gatt_exchange_params *params)
{
	struct bt_att_exchange_mtu_req *req;
	struct net_buf *buf;
	u16_t mtu;

	__ASSERT(conn, "invalid parameter\n");
	__ASSERT(params && params->func, "invalid parameters\n");

	if (conn->state != BT_CONN_CONNECTED) {
		return -ENOTCONN;
	}

	buf = bt_att_create_pdu(conn, BT_ATT_OP_MTU_REQ, sizeof(*req));
	if (!buf) {
		return -ENOMEM;
	}

	mtu = BT_ATT_MTU;

	BT_DBG("Client MTU %u", mtu);

	req = net_buf_add(buf, sizeof(*req));
	req->mtu = sys_cpu_to_le16(mtu);

	return gatt_send(conn, buf, gatt_mtu_rsp, params, NULL);
}

static void gatt_discover_next(struct bt_conn *conn, u16_t last_handle,
			       struct bt_gatt_discover_params *params)
{
	/* Skip if last_handle is not set */
	if (!last_handle)
		goto discover;

	/* Continue from the last found handle */
	params->start_handle = last_handle;
	if (params->start_handle < UINT16_MAX) {
		params->start_handle++;
	} else {
		goto done;
	}

	/* Stop if over the range or the requests */
	if (params->start_handle > params->end_handle) {
		goto done;
	}

discover:
	/* Discover next range */
	if (!bt_gatt_discover(conn, params)) {
		return;
	}

done:
	params->func(conn, NULL, params);
}

static void gatt_find_type_rsp(struct bt_conn *conn, u8_t err,
			       const void *pdu, u16_t length,
			       void *user_data)
{
	const struct bt_att_find_type_rsp *rsp = pdu;
	struct bt_gatt_discover_params *params = user_data;
	u8_t i;
	u16_t end_handle = 0, start_handle;

	BT_DBG("err 0x%02x", err);

	if (err) {
		goto done;
	}

	/* Parse attributes found */
	for (i = 0; length >= sizeof(rsp->list[i]);
	     i++, length -=  sizeof(rsp->list[i])) {
		struct bt_gatt_attr attr = {};
		struct bt_gatt_service_val value;

		start_handle = sys_le16_to_cpu(rsp->list[i].start_handle);
		end_handle = sys_le16_to_cpu(rsp->list[i].end_handle);

		BT_DBG("start_handle 0x%04x end_handle 0x%04x", start_handle,
		       end_handle);

		if (params->type == BT_GATT_DISCOVER_PRIMARY) {
			attr.uuid = BT_UUID_GATT_PRIMARY;
		} else {
			attr.uuid = BT_UUID_GATT_SECONDARY;
		}

		value.end_handle = end_handle;
		value.uuid = params->uuid;

		attr.handle = start_handle;
		attr.user_data = &value;

		if (params->func(conn, &attr, params) == BT_GATT_ITER_STOP) {
			return;
		}
	}

	/* Stop if could not parse the whole PDU */
	if (length > 0) {
		goto done;
	}

	gatt_discover_next(conn, end_handle, params);

	return;
done:
	params->func(conn, NULL, params);
}

static int gatt_find_type(struct bt_conn *conn,
			 struct bt_gatt_discover_params *params)
{
	struct net_buf *buf;
	struct bt_att_find_type_req *req;
	struct bt_uuid *uuid;

	buf = bt_att_create_pdu(conn, BT_ATT_OP_FIND_TYPE_REQ, sizeof(*req));
	if (!buf) {
		return -ENOMEM;
	}

	req = net_buf_add(buf, sizeof(*req));
	req->start_handle = sys_cpu_to_le16(params->start_handle);
	req->end_handle = sys_cpu_to_le16(params->end_handle);

	if (params->type == BT_GATT_DISCOVER_PRIMARY) {
		uuid = BT_UUID_GATT_PRIMARY;
	} else {
		uuid = BT_UUID_GATT_SECONDARY;
	}

	req->type = sys_cpu_to_le16(BT_UUID_16(uuid)->val);

	BT_DBG("uuid %s start_handle 0x%04x end_handle 0x%04x",
	       bt_uuid_str(params->uuid), params->start_handle,
	       params->end_handle);

	switch (params->uuid->type) {
	case BT_UUID_TYPE_16:
		net_buf_add_le16(buf, BT_UUID_16(params->uuid)->val);
		break;
	case BT_UUID_TYPE_128:
		net_buf_add_mem(buf, BT_UUID_128(params->uuid)->val, 16);
		break;
	default:
		BT_ERR("Unknown UUID type %u", params->uuid->type);
		net_buf_unref(buf);
		return -EINVAL;
	}

	return gatt_send(conn, buf, gatt_find_type_rsp, params, NULL);
}

static void read_included_uuid_cb(struct bt_conn *conn, u8_t err,
				  const void *pdu, u16_t length,
				  void *user_data)
{
	struct bt_gatt_discover_params *params = user_data;
	struct bt_gatt_include value;
	struct bt_gatt_attr *attr;
	union {
		struct bt_uuid uuid;
		struct bt_uuid_128 u128;
	} u;

	if (length != 16) {
		BT_ERR("Invalid data len %u", length);
		params->func(conn, NULL, params);
		return;
	}

	value.start_handle = params->_included.start_handle;
	value.end_handle = params->_included.end_handle;
	value.uuid = &u.uuid;
	u.uuid.type = BT_UUID_TYPE_128;
	memcpy(u.u128.val, pdu, length);

	BT_DBG("handle 0x%04x uuid %s start_handle 0x%04x "
	       "end_handle 0x%04x\n", params->_included.attr_handle,
	       bt_uuid_str(&u.uuid), value.start_handle, value.end_handle);

	/* Skip if UUID is set but doesn't match */
	if (params->uuid && bt_uuid_cmp(&u.uuid, params->uuid)) {
		goto next;
	}

	attr = (&(struct bt_gatt_attr) {
		.uuid = BT_UUID_GATT_INCLUDE,
		.user_data = &value, });
	attr->handle = params->_included.attr_handle;

	if (params->func(conn, attr, params) == BT_GATT_ITER_STOP) {
		return;
	}
next:
	gatt_discover_next(conn, params->start_handle, params);

	return;
}

static int read_included_uuid(struct bt_conn *conn,
			      struct bt_gatt_discover_params *params)
{
	struct net_buf *buf;
	struct bt_att_read_req *req;

	buf = bt_att_create_pdu(conn, BT_ATT_OP_READ_REQ, sizeof(*req));
	if (!buf) {
		return -ENOMEM;
	}

	req = net_buf_add(buf, sizeof(*req));
	req->handle = sys_cpu_to_le16(params->_included.start_handle);

	BT_DBG("handle 0x%04x", params->_included.start_handle);

	return gatt_send(conn, buf, read_included_uuid_cb, params, NULL);
}

static u16_t parse_include(struct bt_conn *conn, const void *pdu,
			   struct bt_gatt_discover_params *params,
			   u16_t length)
{
	const struct bt_att_read_type_rsp *rsp = pdu;
	u16_t handle = 0;
	struct bt_gatt_include value;
	union {
		struct bt_uuid uuid;
		struct bt_uuid_16 u16;
		struct bt_uuid_128 u128;
	} u;

	/* Data can be either in UUID16 or UUID128 */
	switch (rsp->len) {
	case 8: /* UUID16 */
		u.uuid.type = BT_UUID_TYPE_16;
		break;
	case 6: /* UUID128 */
		/* BLUETOOTH SPECIFICATION Version 4.2 [Vol 3, Part G] page 550
		 * To get the included service UUID when the included service
		 * uses a 128-bit UUID, the Read Request is used.
		 */
		u.uuid.type = BT_UUID_TYPE_128;
		break;
	default:
		BT_ERR("Invalid data len %u", rsp->len);
		goto done;
	}

	/* Parse include found */
	for (length--, pdu = rsp->data; length >= rsp->len;
	     length -= rsp->len, pdu = (const u8_t *)pdu + rsp->len) {
		struct bt_gatt_attr *attr;
		const struct bt_att_data *data = pdu;
		struct gatt_incl *incl = (void *)data->value;

		handle = sys_le16_to_cpu(data->handle);
		/* Handle 0 is invalid */
		if (!handle) {
			goto done;
		}

		/* Convert include data, bt_gatt_incl and gatt_incl
		 * have different formats so the conversion have to be done
		 * field by field.
		 */
		value.start_handle = sys_le16_to_cpu(incl->start_handle);
		value.end_handle = sys_le16_to_cpu(incl->end_handle);

		switch (u.uuid.type) {
		case BT_UUID_TYPE_16:
			value.uuid = &u.uuid;
			u.u16.val = sys_le16_to_cpu(incl->uuid16);
			break;
		case BT_UUID_TYPE_128:
			params->_included.attr_handle = handle;
			params->_included.start_handle = value.start_handle;
			params->_included.end_handle = value.end_handle;

			return read_included_uuid(conn, params);
		}

		BT_DBG("handle 0x%04x uuid %s start_handle 0x%04x "
		       "end_handle 0x%04x\n", handle, bt_uuid_str(&u.uuid),
		       value.start_handle, value.end_handle);

		/* Skip if UUID is set but doesn't match */
		if (params->uuid && bt_uuid_cmp(&u.uuid, params->uuid)) {
			continue;
		}

		attr = (&(struct bt_gatt_attr) {
			.uuid = BT_UUID_GATT_INCLUDE,
			.user_data = &value, });
		attr->handle = handle;

		if (params->func(conn, attr, params) == BT_GATT_ITER_STOP) {
			return 0;
		}
	}

	/* Whole PDU read without error */
	if (length == 0 && handle) {
		return handle;
	}

done:
	params->func(conn, NULL, params);
	return 0;
}

#define BT_GATT_CHRC(_uuid, _props)					\
	BT_GATT_ATTRIBUTE(BT_UUID_GATT_CHRC, BT_GATT_PERM_READ,		\
			  bt_gatt_attr_read_chrc, NULL,			\
			  (&(struct bt_gatt_chrc) { .uuid = _uuid,	\
						   .properties = _props, }))

static u16_t parse_characteristic(struct bt_conn *conn, const void *pdu,
				  struct bt_gatt_discover_params *params,
				  u16_t length)
{
	const struct bt_att_read_type_rsp *rsp = pdu;
	u16_t handle = 0;
	union {
		struct bt_uuid uuid;
		struct bt_uuid_16 u16;
		struct bt_uuid_128 u128;
	} u;

	/* Data can be either in UUID16 or UUID128 */
	switch (rsp->len) {
	case 7: /* UUID16 */
		u.uuid.type = BT_UUID_TYPE_16;
		break;
	case 21: /* UUID128 */
		u.uuid.type = BT_UUID_TYPE_128;
		break;
	default:
		BT_ERR("Invalid data len %u", rsp->len);
		goto done;
	}

	/* Parse characteristics found */
	for (length--, pdu = rsp->data; length >= rsp->len;
	     length -= rsp->len, pdu = (const u8_t *)pdu + rsp->len) {
		struct bt_gatt_attr *attr;
		const struct bt_att_data *data = pdu;
		struct gatt_chrc *chrc = (void *)data->value;

		handle = sys_le16_to_cpu(data->handle);
		/* Handle 0 is invalid */
		if (!handle) {
			goto done;
		}

		switch (u.uuid.type) {
		case BT_UUID_TYPE_16:
			u.u16.val = sys_le16_to_cpu(chrc->uuid16);
			break;
		case BT_UUID_TYPE_128:
			memcpy(u.u128.val, chrc->uuid, sizeof(chrc->uuid));
			break;
		}

		BT_DBG("handle 0x%04x uuid %s properties 0x%02x", handle,
		       bt_uuid_str(&u.uuid), chrc->properties);

		/* Skip if UUID is set but doesn't match */
		if (params->uuid && bt_uuid_cmp(&u.uuid, params->uuid)) {
			continue;
		}

		attr = (&(struct bt_gatt_attr)BT_GATT_CHRC(&u.uuid,
							   chrc->properties));
		attr->handle = handle;

		if (params->func(conn, attr, params) == BT_GATT_ITER_STOP) {
			return 0;
		}
	}

	/* Whole PDU read without error */
	if (length == 0 && handle) {
		return handle;
	}

done:
	params->func(conn, NULL, params);
	return 0;
}

static void gatt_read_type_rsp(struct bt_conn *conn, u8_t err,
			       const void *pdu, u16_t length,
			       void *user_data)
{
	struct bt_gatt_discover_params *params = user_data;
	u16_t handle;

	BT_DBG("err 0x%02x", err);

	if (err) {
		params->func(conn, NULL, params);
		return;
	}

	if (params->type == BT_GATT_DISCOVER_INCLUDE) {
		handle = parse_include(conn, pdu, params, length);
	} else {
		handle = parse_characteristic(conn, pdu, params, length);
	}

	if (!handle) {
		return;
	}

	gatt_discover_next(conn, handle, params);
}

static int gatt_read_type(struct bt_conn *conn,
			  struct bt_gatt_discover_params *params)
{
	struct net_buf *buf;
	struct bt_att_read_type_req *req;

	buf = bt_att_create_pdu(conn, BT_ATT_OP_READ_TYPE_REQ, sizeof(*req));
	if (!buf) {
		return -ENOMEM;
	}

	req = net_buf_add(buf, sizeof(*req));
	req->start_handle = sys_cpu_to_le16(params->start_handle);
	req->end_handle = sys_cpu_to_le16(params->end_handle);

	if (params->type == BT_GATT_DISCOVER_INCLUDE) {
		net_buf_add_le16(buf, BT_UUID_16(BT_UUID_GATT_INCLUDE)->val);
	} else {
		net_buf_add_le16(buf, BT_UUID_16(BT_UUID_GATT_CHRC)->val);
	}

	BT_DBG("start_handle 0x%04x end_handle 0x%04x", params->start_handle,
	       params->end_handle);

	return gatt_send(conn, buf, gatt_read_type_rsp, params, NULL);
}

static u16_t parse_service(struct bt_conn *conn, const void *pdu,
				  struct bt_gatt_discover_params *params,
				  u16_t length)
{
	const struct bt_att_read_group_rsp *rsp = pdu;
	u16_t start_handle, end_handle = 0;
	union {
		struct bt_uuid uuid;
		struct bt_uuid_16 u16;
		struct bt_uuid_128 u128;
	} u;

	/* Data can be either in UUID16 or UUID128 */
	switch (rsp->len) {
	case 6: /* UUID16 */
		u.uuid.type = BT_UUID_TYPE_16;
		break;
	case 20: /* UUID128 */
		u.uuid.type = BT_UUID_TYPE_128;
		break;
	default:
		BT_ERR("Invalid data len %u", rsp->len);
		goto done;
	}

	/* Parse services found */
	for (length--, pdu = rsp->data; length >= rsp->len;
	     length -= rsp->len, pdu = (const u8_t *)pdu + rsp->len) {
		struct bt_gatt_attr attr = {};
		struct bt_gatt_service_val value;
		const struct bt_att_group_data *data = pdu;

		start_handle = sys_le16_to_cpu(data->start_handle);
		if (!start_handle) {
			goto done;
		}

		end_handle = sys_le16_to_cpu(data->end_handle);
		if (!end_handle || end_handle < start_handle) {
			goto done;
		}

		switch (u.uuid.type) {
		case BT_UUID_TYPE_16:
			memcpy(&u.u16.val, data->value, sizeof(u.u16.val));
			u.u16.val = sys_le16_to_cpu(u.u16.val);
			break;
		case BT_UUID_TYPE_128:
			memcpy(u.u128.val, data->value, sizeof(u.u128.val));
			break;
		}

		BT_DBG("start_handle 0x%04x end_handle 0x%04x uuid %s",
		       start_handle, end_handle, bt_uuid_str(&u.uuid));

		if (params->type == BT_GATT_DISCOVER_PRIMARY) {
			attr.uuid = BT_UUID_GATT_PRIMARY;
		} else {
			attr.uuid = BT_UUID_GATT_SECONDARY;
		}

		value.end_handle = end_handle;
		value.uuid = &u.uuid;

		attr.handle = start_handle;
		attr.user_data = &value;

		if (params->func(conn, &attr, params) == BT_GATT_ITER_STOP) {
			return 0;
		}
	}

	/* Whole PDU read without error */
	if (length == 0 && end_handle) {
		return end_handle;
	}

done:
	params->func(conn, NULL, params);
	return 0;
}

static void gatt_read_group_rsp(struct bt_conn *conn, u8_t err,
				const void *pdu, u16_t length,
				void *user_data)
{
	struct bt_gatt_discover_params *params = user_data;
	u16_t handle;

	BT_DBG("err 0x%02x", err);

	if (err) {
		params->func(conn, NULL, params);
		return;
	}

	handle = parse_service(conn, pdu, params, length);
	if (!handle) {
		return;
	}

	gatt_discover_next(conn, handle, params);
}

static int gatt_read_group(struct bt_conn *conn,
			   struct bt_gatt_discover_params *params)
{
	struct net_buf *buf;
	struct bt_att_read_group_req *req;

	buf = bt_att_create_pdu(conn, BT_ATT_OP_READ_GROUP_REQ, sizeof(*req));
	if (!buf) {
		return -ENOMEM;
	}

	req = net_buf_add(buf, sizeof(*req));
	req->start_handle = sys_cpu_to_le16(params->start_handle);
	req->end_handle = sys_cpu_to_le16(params->end_handle);

	if (params->type == BT_GATT_DISCOVER_PRIMARY) {
		net_buf_add_le16(buf, BT_UUID_16(BT_UUID_GATT_PRIMARY)->val);
	} else {
		net_buf_add_le16(buf, BT_UUID_16(BT_UUID_GATT_SECONDARY)->val);
	}

	BT_DBG("start_handle 0x%04x end_handle 0x%04x", params->start_handle,
	       params->end_handle);

	return gatt_send(conn, buf, gatt_read_group_rsp, params, NULL);
}

static void gatt_find_info_rsp(struct bt_conn *conn, u8_t err,
			       const void *pdu, u16_t length,
			       void *user_data)
{
	const struct bt_att_find_info_rsp *rsp = pdu;
	struct bt_gatt_discover_params *params = user_data;
	u16_t handle = 0;
	u8_t len;
	union {
		const struct bt_att_info_16 *i16;
		const struct bt_att_info_128 *i128;
	} info;
	union {
		struct bt_uuid uuid;
		struct bt_uuid_16 u16;
		struct bt_uuid_128 u128;
	} u;

	BT_DBG("err 0x%02x", err);

	if (err) {
		goto done;
	}

	/* Data can be either in UUID16 or UUID128 */
	switch (rsp->format) {
	case BT_ATT_INFO_16:
		u.uuid.type = BT_UUID_TYPE_16;
		len = sizeof(*info.i16);
		break;
	case BT_ATT_INFO_128:
		u.uuid.type = BT_UUID_TYPE_128;
		len = sizeof(*info.i128);
		break;
	default:
		BT_ERR("Invalid format %u", rsp->format);
		goto done;
	}

	/* Parse descriptors found */
	for (length--, pdu = rsp->info; length >= len;
	     length -= len, pdu = (const u8_t *)pdu + len) {
		struct bt_gatt_attr *attr;

		info.i16 = pdu;
		handle = sys_le16_to_cpu(info.i16->handle);

		switch (u.uuid.type) {
		case BT_UUID_TYPE_16:
			u.u16.val = sys_le16_to_cpu(info.i16->uuid);
			break;
		case BT_UUID_TYPE_128:
			memcpy(u.u128.val, info.i128->uuid, 16);
			break;
		}

		BT_DBG("handle 0x%04x uuid %s", handle, bt_uuid_str(&u.uuid));

		/* Skip if UUID is set but doesn't match */
		if (params->uuid && bt_uuid_cmp(&u.uuid, params->uuid)) {
			continue;
		}

		attr = (&(struct bt_gatt_attr)
			BT_GATT_DESCRIPTOR(&u.uuid, 0, NULL, NULL, NULL));
		attr->handle = handle;

		if (params->func(conn, attr, params) == BT_GATT_ITER_STOP) {
			return;
		}
	}

	/* Stop if could not parse the whole PDU */
	if (length > 0) {
		goto done;
	}

	gatt_discover_next(conn, handle, params);

	return;

done:
	params->func(conn, NULL, params);
}

static int gatt_find_info(struct bt_conn *conn,
			  struct bt_gatt_discover_params *params)
{
	struct net_buf *buf;
	struct bt_att_find_info_req *req;

	buf = bt_att_create_pdu(conn, BT_ATT_OP_FIND_INFO_REQ, sizeof(*req));
	if (!buf) {
		return -ENOMEM;
	}

	req = net_buf_add(buf, sizeof(*req));
	req->start_handle = sys_cpu_to_le16(params->start_handle);
	req->end_handle = sys_cpu_to_le16(params->end_handle);

	BT_DBG("start_handle 0x%04x end_handle 0x%04x", params->start_handle,
	       params->end_handle);

	return gatt_send(conn, buf, gatt_find_info_rsp, params, NULL);
}

int bt_gatt_discover(struct bt_conn *conn,
		     struct bt_gatt_discover_params *params)
{
	__ASSERT(conn, "invalid parameters\n");
	__ASSERT(params && params->func, "invalid parameters\n");
	__ASSERT((params->start_handle && params->end_handle),
		 "invalid parameters\n");
	__ASSERT((params->start_handle <= params->end_handle),
		 "invalid parameters\n");

	if (conn->state != BT_CONN_CONNECTED) {
		return -ENOTCONN;
	}

	switch (params->type) {
	case BT_GATT_DISCOVER_PRIMARY:
	case BT_GATT_DISCOVER_SECONDARY:
		if (params->uuid) {
			return gatt_find_type(conn, params);
		}
		return gatt_read_group(conn, params);
	case BT_GATT_DISCOVER_INCLUDE:
	case BT_GATT_DISCOVER_CHARACTERISTIC:
		return gatt_read_type(conn, params);
	case BT_GATT_DISCOVER_DESCRIPTOR:
		return gatt_find_info(conn, params);
	default:
		BT_ERR("Invalid discovery type: %u", params->type);
	}

	return -EINVAL;
}

static void gatt_read_rsp(struct bt_conn *conn, u8_t err, const void *pdu,
			  u16_t length, void *user_data)
{
	struct bt_gatt_read_params *params = user_data;

	BT_DBG("err 0x%02x", err);

	if (err || !length) {
		params->func(conn, err, params, NULL, 0);
		return;
	}

	if (params->func(conn, 0, params, pdu, length) == BT_GATT_ITER_STOP) {
		return;
	}

	/*
	 * Core Spec 4.2, Vol. 3, Part G, 4.8.1
	 * If the Characteristic Value is greater than (ATT_MTU - 1) octets
	 * in length, the Read Long Characteristic Value procedure may be used
	 * if the rest of the Characteristic Value is required.
	 */
	if (length < (bt_att_get_mtu(conn) - 1)) {
		params->func(conn, 0, params, NULL, 0);
		return;
	}

	params->single.offset += length;

	/* Continue reading the attribute */
	if (bt_gatt_read(conn, params) < 0) {
		params->func(conn, BT_ATT_ERR_UNLIKELY, params, NULL, 0);
	}
}

static int gatt_read_blob(struct bt_conn *conn,
			  struct bt_gatt_read_params *params)
{
	struct net_buf *buf;
	struct bt_att_read_blob_req *req;

	buf = bt_att_create_pdu(conn, BT_ATT_OP_READ_BLOB_REQ, sizeof(*req));
	if (!buf) {
		return -ENOMEM;
	}

	req = net_buf_add(buf, sizeof(*req));
	req->handle = sys_cpu_to_le16(params->single.handle);
	req->offset = sys_cpu_to_le16(params->single.offset);

	BT_DBG("handle 0x%04x offset 0x%04x", params->single.handle,
	       params->single.offset);

	return gatt_send(conn, buf, gatt_read_rsp, params, NULL);
}

#if defined(CONFIG_BT_GATT_READ_MULTIPLE)
static void gatt_read_multiple_rsp(struct bt_conn *conn, u8_t err,
				   const void *pdu, u16_t length,
				   void *user_data)
{
	struct bt_gatt_read_params *params = user_data;

	BT_DBG("err 0x%02x", err);

	if (err || !length) {
		params->func(conn, err, params, NULL, 0);
		return;
	}

	params->func(conn, 0, params, pdu, length);

	/* mark read as complete since read multiple is single response */
	params->func(conn, 0, params, NULL, 0);
}

static int gatt_read_multiple(struct bt_conn *conn,
			      struct bt_gatt_read_params *params)
{
	struct net_buf *buf;
	u8_t i;

	buf = bt_att_create_pdu(conn, BT_ATT_OP_READ_MULT_REQ,
				params->handle_count * sizeof(u16_t));
	if (!buf) {
		return -ENOMEM;
	}

	for (i = 0; i < params->handle_count; i++) {
		net_buf_add_le16(buf, params->handles[i]);
	}

	return gatt_send(conn, buf, gatt_read_multiple_rsp, params, NULL);
}
#else
static int gatt_read_multiple(struct bt_conn *conn,
			      struct bt_gatt_read_params *params)
{
	return -ENOTSUP;
}
#endif /* CONFIG_BT_GATT_READ_MULTIPLE */

int bt_gatt_read(struct bt_conn *conn, struct bt_gatt_read_params *params)
{
	struct net_buf *buf;
	struct bt_att_read_req *req;

	__ASSERT(conn, "invalid parameters\n");
	__ASSERT(params && params->func, "invalid parameters\n");
	__ASSERT(params->handle_count, "invalid parameters\n");

	if (conn->state != BT_CONN_CONNECTED) {
		return -ENOTCONN;
	}

	if (params->handle_count > 1) {
		return gatt_read_multiple(conn, params);
	}

	if (params->single.offset) {
		return gatt_read_blob(conn, params);
	}

	buf = bt_att_create_pdu(conn, BT_ATT_OP_READ_REQ, sizeof(*req));
	if (!buf) {
		return -ENOMEM;
	}

	req = net_buf_add(buf, sizeof(*req));
	req->handle = sys_cpu_to_le16(params->single.handle);

	BT_DBG("handle 0x%04x", params->single.handle);

	return gatt_send(conn, buf, gatt_read_rsp, params, NULL);
}

static void gatt_write_rsp(struct bt_conn *conn, u8_t err, const void *pdu,
			   u16_t length, void *user_data)
{
	struct bt_gatt_write_params *params = user_data;

	BT_DBG("err 0x%02x", err);

	params->func(conn, err, params);
}

int bt_gatt_write_without_response(struct bt_conn *conn, u16_t handle,
				   const void *data, u16_t length, bool sign)
{
	struct net_buf *buf;
	struct bt_att_write_cmd *cmd;

	__ASSERT(conn, "invalid parameters\n");
	__ASSERT(handle, "invalid parameters\n");

	if (conn->state != BT_CONN_CONNECTED) {
		return -ENOTCONN;
	}

#if defined(CONFIG_BT_SMP)
	if (conn->encrypt) {
		/* Don't need to sign if already encrypted */
		sign = false;
	}
#endif

	if (sign) {
		buf = bt_att_create_pdu(conn, BT_ATT_OP_SIGNED_WRITE_CMD,
					sizeof(*cmd) + length + 12);
	} else {
		buf = bt_att_create_pdu(conn, BT_ATT_OP_WRITE_CMD,
					sizeof(*cmd) + length);
	}
	if (!buf) {
		return -ENOMEM;
	}

	cmd = net_buf_add(buf, sizeof(*cmd));
	cmd->handle = sys_cpu_to_le16(handle);
	memcpy(cmd->value, data, length);
	net_buf_add(buf, length);

	BT_DBG("handle 0x%04x length %u", handle, length);

	return gatt_send(conn, buf, NULL, NULL, NULL);
}

static int gatt_exec_write(struct bt_conn *conn,
			   struct bt_gatt_write_params *params)
{
	struct net_buf *buf;
	struct bt_att_exec_write_req *req;

	buf = bt_att_create_pdu(conn, BT_ATT_OP_EXEC_WRITE_REQ, sizeof(*req));
	if (!buf) {
		return -ENOMEM;
	}

	req = net_buf_add(buf, sizeof(*req));
	req->flags = BT_ATT_FLAG_EXEC;

	BT_DBG("");

	return gatt_send(conn, buf, gatt_write_rsp, params, NULL);
}

static void gatt_prepare_write_rsp(struct bt_conn *conn, u8_t err,
				   const void *pdu, u16_t length,
				   void *user_data)
{
	struct bt_gatt_write_params *params = user_data;

	BT_DBG("err 0x%02x", err);


	/* Don't continue in case of error */
	if (err) {
		params->func(conn, err, params);
		return;
	}

	/* If there is no more data execute */
	if (!params->length) {
		gatt_exec_write(conn, params);
		return;
	}

	/* Write next chunk */
	bt_gatt_write(conn, params);
}

static int gatt_prepare_write(struct bt_conn *conn,
			      struct bt_gatt_write_params *params)
{
	struct net_buf *buf;
	struct bt_att_prepare_write_req *req;
	u16_t len;

	len = min(params->length, bt_att_get_mtu(conn) - sizeof(*req) - 1);

	buf = bt_att_create_pdu(conn, BT_ATT_OP_PREPARE_WRITE_REQ,
				sizeof(*req) + len);
	if (!buf) {
		return -ENOMEM;
	}

	req = net_buf_add(buf, sizeof(*req));
	req->handle = sys_cpu_to_le16(params->handle);
	req->offset = sys_cpu_to_le16(params->offset);
	memcpy(req->value, params->data, len);
	net_buf_add(buf, len);

	/* Update params */
	params->offset += len;
	params->data = (const u8_t *)params->data + len;
	params->length -= len;

	BT_DBG("handle 0x%04x offset %u len %u", params->handle, params->offset,
	       params->length);

	return gatt_send(conn, buf, gatt_prepare_write_rsp, params, NULL);
}

int bt_gatt_write(struct bt_conn *conn, struct bt_gatt_write_params *params)
{
	struct net_buf *buf;
	struct bt_att_write_req *req;

	__ASSERT(conn, "invalid parameters\n");
	__ASSERT(params && params->func, "invalid parameters\n");
	__ASSERT(params->handle, "invalid parameters\n");

	if (conn->state != BT_CONN_CONNECTED) {
		return -ENOTCONN;
	}

	/* Use Prepare Write if offset is set or Long Write is required */
	if (params->offset ||
	    params->length > (bt_att_get_mtu(conn) - sizeof(*req) - 1)) {
		return gatt_prepare_write(conn, params);
	}

	buf = bt_att_create_pdu(conn, BT_ATT_OP_WRITE_REQ,
				sizeof(*req) + params->length);
	if (!buf) {
		return -ENOMEM;
	}

	req = net_buf_add(buf, sizeof(*req));
	req->handle = sys_cpu_to_le16(params->handle);
	memcpy(req->value, params->data, params->length);
	net_buf_add(buf, params->length);

	BT_DBG("handle 0x%04x length %u", params->handle, params->length);

	return gatt_send(conn, buf, gatt_write_rsp, params, NULL);
}

static void gatt_subscription_add(struct bt_conn *conn,
				  struct bt_gatt_subscribe_params *params)
{
	bt_addr_le_copy(&params->_peer, &conn->le.dst);

	/* Prepend subscription */
	sys_slist_prepend(&subscriptions, &params->node);
}

static void gatt_write_ccc_rsp(struct bt_conn *conn, u8_t err,
			       const void *pdu, u16_t length,
			       void *user_data)
{
	struct bt_gatt_subscribe_params *params = user_data;

	BT_DBG("err 0x%02x", err);

	/* if write to CCC failed we remove subscription and notify app */
	if (err) {
		sys_snode_t *node, *tmp, *prev = NULL;

		SYS_SLIST_FOR_EACH_NODE_SAFE(&subscriptions, node, tmp) {
			if (node == &params->node) {
				gatt_subscription_remove(conn, tmp, params);
				break;
			}

			prev = node;
		}
	} else if (!params->value) {
		/* Notify with NULL data to complete unsubscribe */
		params->notify(conn, params, NULL, 0);
	}
}

static int gatt_write_ccc(struct bt_conn *conn, u16_t handle, u16_t value,
			  bt_att_func_t func,
			  struct bt_gatt_subscribe_params *params)
{
	struct net_buf *buf;
	struct bt_att_write_req *req;

	buf = bt_att_create_pdu(conn, BT_ATT_OP_WRITE_REQ,
				sizeof(*req) + sizeof(u16_t));
	if (!buf) {
		return -ENOMEM;
	}

	req = net_buf_add(buf, sizeof(*req));
	req->handle = sys_cpu_to_le16(handle);
	net_buf_add_le16(buf, value);

	BT_DBG("handle 0x%04x value 0x%04x", handle, value);

	return gatt_send(conn, buf, func, params, NULL);
}

int bt_gatt_subscribe(struct bt_conn *conn,
		      struct bt_gatt_subscribe_params *params)
{
	struct bt_gatt_subscribe_params *tmp;
	bool has_subscription = false;

	__ASSERT(conn, "invalid parameters\n");
	__ASSERT(params && params->notify,  "invalid parameters\n");
	__ASSERT(params->value, "invalid parameters\n");
	__ASSERT(params->ccc_handle, "invalid parameters\n");

	if (conn->state != BT_CONN_CONNECTED) {
		return -ENOTCONN;
	}

	/* Lookup existing subscriptions */
	SYS_SLIST_FOR_EACH_CONTAINER(&subscriptions, tmp, node) {
		/* Fail if entry already exists */
		if (tmp == params) {
			return -EALREADY;
		}

		/* Check if another subscription exists */
		if (!bt_conn_addr_le_cmp(conn, &tmp->_peer) &&
		    tmp->value_handle == params->value_handle &&
		    tmp->value >= params->value) {
			has_subscription = true;
		}
	}

	/* Skip write if already subscribed */
	if (!has_subscription) {
		int err;

		err = gatt_write_ccc(conn, params->ccc_handle, params->value,
				     gatt_write_ccc_rsp, params);
		if (err) {
			return err;
		}
	}

	/*
	 * Add subscription before write complete as some implementation were
	 * reported to send notification before reply to CCC write.
	 */
	gatt_subscription_add(conn, params);

	return 0;
}

int bt_gatt_unsubscribe(struct bt_conn *conn,
			struct bt_gatt_subscribe_params *params)
{
	struct bt_gatt_subscribe_params *tmp, *next;
	bool has_subscription = false, found = false;
	sys_snode_t *prev = NULL;

	__ASSERT(conn, "invalid parameters\n");
	__ASSERT(params, "invalid parameters\n");

	if (conn->state != BT_CONN_CONNECTED) {
		return -ENOTCONN;
	}

	/* Lookup existing subscriptions */
	SYS_SLIST_FOR_EACH_CONTAINER_SAFE(&subscriptions, tmp, next, node) {
		/* Remove subscription */
		if (params == tmp) {
			found = true;
			sys_slist_remove(&subscriptions, prev, &tmp->node);
			continue;
		} else {
			prev = &tmp->node;
		}

		/* Check if there still remains any other subscription */
		if (!bt_conn_addr_le_cmp(conn, &tmp->_peer) &&
		    tmp->value_handle == params->value_handle) {
			has_subscription = true;
		}
	}

	if (!found) {
		return -EINVAL;
	}

	if (has_subscription) {
		/* Notify with NULL data to complete unsubscribe */
		params->notify(conn, params, NULL, 0);
		return 0;
	}

	params->value = 0x0000;

	return gatt_write_ccc(conn, params->ccc_handle, params->value,
			      gatt_write_ccc_rsp, params);
}

void bt_gatt_cancel(struct bt_conn *conn, void *params)
{
	bt_att_req_cancel(conn, params);
}

static void add_subscriptions(struct bt_conn *conn)
{
	struct bt_gatt_subscribe_params *params;

	/* Lookup existing subscriptions */
	SYS_SLIST_FOR_EACH_CONTAINER(&subscriptions, params, node) {
		if (bt_conn_addr_le_cmp(conn, &params->_peer)) {
			continue;
		}

		/* Force write to CCC to workaround devices that don't track
		 * it properly.
		 */
		gatt_write_ccc(conn, params->ccc_handle, params->value,
			       gatt_write_ccc_rsp, params);
	}
}

#endif /* CONFIG_BT_GATT_CLIENT */

void bt_gatt_connected(struct bt_conn *conn)
{
	BT_DBG("conn %p", conn);
	bt_gatt_foreach_attr(0x0001, 0xffff, connected_cb, conn);
#if defined(CONFIG_BT_GATT_CLIENT)
	add_subscriptions(conn);
#endif /* CONFIG_BT_GATT_CLIENT */
}

void bt_gatt_disconnected(struct bt_conn *conn)
{
	BT_DBG("conn %p", conn);
	bt_gatt_foreach_attr(0x0001, 0xffff, disconnected_cb, conn);

#if defined(CONFIG_BT_SETTINGS_CCC_STORE_ON_WRITE)
	gatt_ccc_conn_unqueue(conn);

	if (gatt_ccc_conn_queue_is_empty()) {
		k_delayed_work_cancel(&gatt_ccc_store.work);
	}
#endif

	if (IS_ENABLED(CONFIG_BT_SETTINGS) &&
	    bt_addr_le_is_bonded(conn->id, &conn->le.dst)) {
		bt_gatt_store_ccc(conn->id, &conn->le.dst);
	}

#if defined(CONFIG_BT_GATT_CLIENT)
	remove_subscriptions(conn);
#endif /* CONFIG_BT_GATT_CLIENT */
}

#if defined(CONFIG_BT_SETTINGS)

#define CCC_STORE_MAX 48

static struct bt_gatt_ccc_cfg *ccc_find_cfg(struct _bt_gatt_ccc *ccc,
					    const bt_addr_le_t *addr)
{
	int i;

	for (i = 0; i < ccc->cfg_len; i++) {
		if (!bt_addr_le_cmp(&ccc->cfg[i].peer, addr)) {
			return &ccc->cfg[i];
		}
	}

	return NULL;
}

struct ccc_save {
	const bt_addr_le_t *addr;
	struct ccc_store store[CCC_STORE_MAX];
	size_t count;
};

static u8_t ccc_save(const struct bt_gatt_attr *attr, void *user_data)
{
	struct ccc_save *save = user_data;
	struct _bt_gatt_ccc *ccc;
	struct bt_gatt_ccc_cfg *cfg;

	/* Check if attribute is a CCC */
	if (attr->write != bt_gatt_attr_write_ccc) {
		return BT_GATT_ITER_CONTINUE;
	}

	ccc = attr->user_data;

	/* Check if there is a cfg for the peer */
	cfg = ccc_find_cfg(ccc, save->addr);
	if (!cfg) {
		return BT_GATT_ITER_CONTINUE;
	}

	BT_DBG("Storing CCCs handle 0x%04x value 0x%04x", attr->handle,
	       cfg->value);

	save->store[save->count].handle = attr->handle;
	save->store[save->count].value = cfg->value;
	save->count++;

	return BT_GATT_ITER_CONTINUE;
}

int bt_gatt_store_ccc(u8_t id, const bt_addr_le_t *addr)
{
	struct ccc_save save;
	char val[BT_SETTINGS_SIZE(sizeof(save.store))];
	char key[BT_SETTINGS_KEY_MAX];
	char *str;
	int err;

	save.addr = addr;
	save.count = 0;

	bt_gatt_foreach_attr(0x0001, 0xffff, ccc_save, &save);

	str = settings_str_from_bytes(save.store,
				      save.count * sizeof(*save.store),
				      val, sizeof(val));
	if (!str) {
		BT_ERR("Unable to encode CCC as handle:value");
		return -EINVAL;
	}

	if (id) {
		char id_str[4];

		snprintk(id_str, sizeof(id_str), "%u", id);
		bt_settings_encode_key(key, sizeof(key), "ccc",
				       (bt_addr_le_t *)addr, id_str);
	} else {
		bt_settings_encode_key(key, sizeof(key), "ccc",
				       (bt_addr_le_t *)addr, NULL);
	}

	err = settings_save_one(key, str);
	if (err) {
		BT_ERR("Failed to store CCCs (err %d)", err);
		return err;
	}

	BT_DBG("Stored CCCs for %s (%s) val %s", bt_addr_le_str(addr), key,
	       str);

	return 0;
}

int bt_gatt_clear_ccc(u8_t id, const bt_addr_le_t *addr)
{
	char key[BT_SETTINGS_KEY_MAX];

	if (id) {
		char id_str[4];

		snprintk(id_str, sizeof(id_str), "%u", id);
		bt_settings_encode_key(key, sizeof(key), "ccc",
				       (bt_addr_le_t *)addr, id_str);
	} else {
		bt_settings_encode_key(key, sizeof(key), "ccc",
				       (bt_addr_le_t *)addr, NULL);
	}

	return settings_save_one(key, NULL);
}

static void ccc_clear(struct _bt_gatt_ccc *ccc, bt_addr_le_t *addr)
{
	struct bt_gatt_ccc_cfg *cfg;

	cfg = ccc_find_cfg(ccc, addr);
	if (!cfg) {
		BT_DBG("Unable to clear CCC: cfg not found");
		return;
	}

	bt_addr_le_copy(&cfg->peer, BT_ADDR_LE_ANY);
	cfg->value = 0;
}

struct ccc_load {
	u8_t id;
	bt_addr_le_t addr;
	struct ccc_store *entry;
	size_t count;
};

static u8_t ccc_load(const struct bt_gatt_attr *attr, void *user_data)
{
	struct ccc_load *load = user_data;
	struct _bt_gatt_ccc *ccc;
	struct bt_gatt_ccc_cfg *cfg;

	/* Check if attribute is a CCC */
	if (attr->write != bt_gatt_attr_write_ccc) {
		return BT_GATT_ITER_CONTINUE;
	}

	ccc = attr->user_data;

	/* Clear if value was invalidade */
	if (!load->entry) {
		ccc_clear(ccc, &load->addr);
		return BT_GATT_ITER_CONTINUE;
	} else if (!load->count) {
		return BT_GATT_ITER_STOP;
	}

	/* Skip if value is not for the given attribute */
	if (load->entry->handle != attr->handle) {
		/* If attribute handle is bigger then it means
		 * the attribute no longer exists and cannot
		 * be restored.
		 */
		if (load->entry->handle < attr->handle) {
			BT_DBG("Unable to restore CCC: handle 0x%04x cannot be"
			       " found",  load->entry->handle);
			goto next;
		}
		return BT_GATT_ITER_CONTINUE;
	}

	BT_DBG("Restoring CCC: handle 0x%04x value 0x%04x", load->entry->handle,
	       load->entry->value);

	cfg = ccc_find_cfg(ccc, BT_ADDR_LE_ANY);
	if (!cfg) {
		BT_DBG("Unable to restore CCC: no cfg left");
		goto next;
	}

	bt_addr_le_copy(&cfg->peer, &load->addr);
	cfg->value = load->entry->value;

next:
	load->entry++;
	load->count--;

	return load->count ? BT_GATT_ITER_CONTINUE : BT_GATT_ITER_STOP;
}

static int ccc_set(int argc, char **argv, char *val)
{
	struct ccc_store ccc_store[CCC_STORE_MAX];
	struct ccc_load load;
	int len, err;

	if (argc < 1) {
		BT_ERR("Insufficient number of arguments");
		return -EINVAL;
	} else if (argc == 1) {
		load.id = BT_ID_DEFAULT;
	} else {
		load.id = strtol(argv[1], NULL, 10);
	}

	BT_DBG("argv[0] %s val %s", argv[0], val ? val : "(null)");

	err = bt_settings_decode_key(argv[0], &load.addr);
	if (err) {
		BT_ERR("Unable to decode address %s", argv[0]);
		return -EINVAL;
	}

	if (val) {
		len = sizeof(ccc_store);
		err = settings_bytes_from_str(val, ccc_store, &len);
		if (err) {
			BT_ERR("Failed to decode value (err %d)", err);
			return err;
		}

		load.entry = ccc_store;
		load.count = len / sizeof(*ccc_store);
	} else {
		load.entry = NULL;
		load.count = 0;
	}

	bt_gatt_foreach_attr(0x0001, 0xffff, ccc_load, &load);

	BT_DBG("Restored CCC for %s", bt_addr_le_str(&load.addr));

	return 0;
}

BT_SETTINGS_DEFINE(ccc, ccc_set, NULL, NULL);
#endif /* CONFIG_BT_SETTINGS */
