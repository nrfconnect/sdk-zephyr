/*
 * Copyright (c) 2017-2021 Nordic Semiconductor ASA
 * Copyright (c) 2015-2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <sys/types.h>

#include <sys/byteorder.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/buf.h>

#include "hci_core.h"
#include "conn_internal.h"
#include "id.h"
#include "scan.h"

#define BT_DBG_ENABLED IS_ENABLED(CONFIG_BT_DEBUG_HCI_CORE)
#define LOG_MODULE_NAME bt_adv
#include "common/log.h"


#if defined(CONFIG_BT_EXT_ADV)
static struct bt_le_ext_adv adv_pool[CONFIG_BT_EXT_ADV_MAX_ADV_SET];
#endif /* defined(CONFIG_BT_EXT_ADV) */


#if defined(CONFIG_BT_EXT_ADV)
uint8_t bt_le_ext_adv_get_index(struct bt_le_ext_adv *adv)
{
	ptrdiff_t index = adv - adv_pool;

	__ASSERT(index >= 0 && index < ARRAY_SIZE(adv_pool),
		 "Invalid bt_adv pointer");
	return (uint8_t)index;
}

static struct bt_le_ext_adv *adv_new(void)
{
	struct bt_le_ext_adv *adv = NULL;
	int i;

	for (i = 0; i < ARRAY_SIZE(adv_pool); i++) {
		if (!atomic_test_bit(adv_pool[i].flags, BT_ADV_CREATED)) {
			adv = &adv_pool[i];
			break;
		}
	}

	if (!adv) {
		return NULL;
	}

	(void)memset(adv, 0, sizeof(*adv));
	atomic_set_bit(adv_pool[i].flags, BT_ADV_CREATED);
	adv->handle = i;

	return adv;
}

static void adv_delete(struct bt_le_ext_adv *adv)
{
	atomic_clear_bit(adv->flags, BT_ADV_CREATED);
}

#if defined(CONFIG_BT_BROADCASTER)
static struct bt_le_ext_adv *bt_adv_lookup_handle(uint8_t handle)
{
	if (handle < ARRAY_SIZE(adv_pool) &&
	    atomic_test_bit(adv_pool[handle].flags, BT_ADV_CREATED)) {
		return &adv_pool[handle];
	}

	return NULL;
}
#endif /* CONFIG_BT_BROADCASTER */
#endif /* defined(CONFIG_BT_EXT_ADV) */

void bt_le_ext_adv_foreach(void (*func)(struct bt_le_ext_adv *adv, void *data),
			   void *data)
{
#if defined(CONFIG_BT_EXT_ADV)
	for (size_t i = 0; i < ARRAY_SIZE(adv_pool); i++) {
		if (atomic_test_bit(adv_pool[i].flags, BT_ADV_CREATED)) {
			func(&adv_pool[i], data);
		}
	}
#else
	func(&bt_dev.adv, data);
#endif /* defined(CONFIG_BT_EXT_ADV) */
}

static struct bt_le_ext_adv *adv_new_legacy(void)
{
#if defined(CONFIG_BT_EXT_ADV)
	if (bt_dev.adv) {
		return NULL;
	}

	bt_dev.adv = adv_new();
	return bt_dev.adv;
#else
	return &bt_dev.adv;
#endif
}

void bt_le_adv_delete_legacy(void)
{
#if defined(CONFIG_BT_EXT_ADV)
	if (bt_dev.adv) {
		atomic_clear_bit(bt_dev.adv->flags, BT_ADV_CREATED);
		bt_dev.adv = NULL;
	}
#endif
}

struct bt_le_ext_adv *bt_le_adv_lookup_legacy(void)
{
#if defined(CONFIG_BT_EXT_ADV)
	return bt_dev.adv;
#else
	return &bt_dev.adv;
#endif
}

int bt_le_adv_set_enable_legacy(struct bt_le_ext_adv *adv, bool enable)
{
	struct net_buf *buf;
	struct bt_hci_cmd_state_set state;
	int err;

	buf = bt_hci_cmd_create(BT_HCI_OP_LE_SET_ADV_ENABLE, 1);
	if (!buf) {
		return -ENOBUFS;
	}

	if (enable) {
		net_buf_add_u8(buf, BT_HCI_LE_ADV_ENABLE);
	} else {
		net_buf_add_u8(buf, BT_HCI_LE_ADV_DISABLE);
	}

	bt_hci_cmd_state_set_init(buf, &state, adv->flags, BT_ADV_ENABLED, enable);

	err = bt_hci_cmd_send_sync(BT_HCI_OP_LE_SET_ADV_ENABLE, buf, NULL);
	if (err) {
		return err;
	}

	return 0;
}

int bt_le_adv_set_enable_ext(struct bt_le_ext_adv *adv,
			 bool enable,
			 const struct bt_le_ext_adv_start_param *param)
{
	struct net_buf *buf;
	struct bt_hci_cmd_state_set state;
	int err;

	buf = bt_hci_cmd_create(BT_HCI_OP_LE_SET_EXT_ADV_ENABLE, 6);
	if (!buf) {
		return -ENOBUFS;
	}

	if (enable) {
		net_buf_add_u8(buf, BT_HCI_LE_ADV_ENABLE);
	} else {
		net_buf_add_u8(buf, BT_HCI_LE_ADV_DISABLE);
	}

	net_buf_add_u8(buf, 1);

	net_buf_add_u8(buf, adv->handle);
	net_buf_add_le16(buf, param ? sys_cpu_to_le16(param->timeout) : 0);
	net_buf_add_u8(buf, param ? param->num_events : 0);

	bt_hci_cmd_state_set_init(buf, &state, adv->flags, BT_ADV_ENABLED, enable);

	err = bt_hci_cmd_send_sync(BT_HCI_OP_LE_SET_EXT_ADV_ENABLE, buf, NULL);
	if (err) {
		return err;
	}

	return 0;
}

int bt_le_adv_set_enable(struct bt_le_ext_adv *adv, bool enable)
{
	if (IS_ENABLED(CONFIG_BT_EXT_ADV) &&
	    BT_FEAT_LE_EXT_ADV(bt_dev.le.features)) {
		return bt_le_adv_set_enable_ext(adv, enable, NULL);
	}

	return bt_le_adv_set_enable_legacy(adv, enable);
}

static bool valid_adv_ext_param(const struct bt_le_adv_param *param)
{
	if (IS_ENABLED(CONFIG_BT_EXT_ADV) &&
	    BT_FEAT_LE_EXT_ADV(bt_dev.le.features)) {
		if (param->peer &&
		    !(param->options & BT_LE_ADV_OPT_EXT_ADV) &&
		    !(param->options & BT_LE_ADV_OPT_CONNECTABLE)) {
			/* Cannot do directed non-connectable advertising
			 * without extended advertising.
			 */
			return false;
		}

		if (param->peer &&
		    (param->options & BT_LE_ADV_OPT_EXT_ADV) &&
		    !(param->options & BT_LE_ADV_OPT_DIR_MODE_LOW_DUTY)) {
			/* High duty cycle directed connectable advertising
			 * shall not be used with Extended Advertising.
			 */
			return false;
		}

		if (!(param->options & BT_LE_ADV_OPT_EXT_ADV) &&
		    param->options & (BT_LE_ADV_OPT_EXT_ADV |
				      BT_LE_ADV_OPT_NO_2M |
				      BT_LE_ADV_OPT_CODED |
				      BT_LE_ADV_OPT_ANONYMOUS |
				      BT_LE_ADV_OPT_USE_TX_POWER)) {
			/* Extended options require extended advertising. */
			return false;
		}
	}

	if (IS_ENABLED(CONFIG_BT_PRIVACY) &&
	    param->peer &&
	    (param->options & BT_LE_ADV_OPT_USE_IDENTITY) &&
	    (param->options & BT_LE_ADV_OPT_DIR_ADDR_RPA)) {
		/* own addr type used for both RPAs in directed advertising. */
		return false;
	}

	if (param->id >= bt_dev.id_count ||
	    !bt_addr_le_cmp(&bt_dev.id_addr[param->id], BT_ADDR_LE_ANY)) {
		return false;
	}

	if (!(param->options & BT_LE_ADV_OPT_CONNECTABLE)) {
		/*
		 * BT Core 4.2 [Vol 2, Part E, 7.8.5]
		 * The Advertising_Interval_Min and Advertising_Interval_Max
		 * shall not be set to less than 0x00A0 (100 ms) if the
		 * Advertising_Type is set to ADV_SCAN_IND or ADV_NONCONN_IND.
		 */
		if (bt_dev.hci_version < BT_HCI_VERSION_5_0 &&
		    param->interval_min < 0x00a0) {
			return false;
		}
	}

	if ((param->options & (BT_LE_ADV_OPT_DIR_MODE_LOW_DUTY |
			       BT_LE_ADV_OPT_DIR_ADDR_RPA)) &&
	    !param->peer) {
		return false;
	}

	if ((param->options & BT_LE_ADV_OPT_DIR_MODE_LOW_DUTY) ||
	    !param->peer) {
		if (param->interval_min > param->interval_max ||
		    param->interval_min < 0x0020 ||
		    param->interval_max > 0x4000) {
			return false;
		}
	}

	if ((param->options & BT_LE_ADV_OPT_DISABLE_CHAN_37) &&
	    (param->options & BT_LE_ADV_OPT_DISABLE_CHAN_38) &&
	    (param->options & BT_LE_ADV_OPT_DISABLE_CHAN_39)) {
		return false;
	}

	return true;
}

static bool valid_adv_param(const struct bt_le_adv_param *param)
{
	if (param->options & BT_LE_ADV_OPT_EXT_ADV) {
		return false;
	}

	if (param->peer && !(param->options & BT_LE_ADV_OPT_CONNECTABLE)) {
		return false;
	}

	return valid_adv_ext_param(param);
}


struct bt_ad {
	const struct bt_data *data;
	size_t len;
};

static int set_data_add(uint8_t *set_data, uint8_t set_data_len_max,
			const struct bt_ad *ad, size_t ad_len, uint8_t *data_len)
{
	uint8_t set_data_len = 0;

	for (size_t i = 0; i < ad_len; i++) {
		const struct bt_data *data = ad[i].data;

		for (size_t j = 0; j < ad[i].len; j++) {
			size_t len = data[j].data_len;
			uint8_t type = data[j].type;

			/* Check if ad fit in the remaining buffer */
			if ((set_data_len + len + 2) > set_data_len_max) {
				ssize_t shortened_len = set_data_len_max -
							(set_data_len + 2);

				if (!(type == BT_DATA_NAME_COMPLETE &&
				      shortened_len > 0)) {
					BT_ERR("Too big advertising data");
					return -EINVAL;
				}

				type = BT_DATA_NAME_SHORTENED;
				len = shortened_len;
			}

			set_data[set_data_len++] = len + 1;
			set_data[set_data_len++] = type;

			memcpy(&set_data[set_data_len], data[j].data, len);
			set_data_len += len;
		}
	}

	*data_len = set_data_len;
	return 0;
}

static int hci_set_ad(uint16_t hci_op, const struct bt_ad *ad, size_t ad_len)
{
	struct bt_hci_cp_le_set_adv_data *set_data;
	struct net_buf *buf;
	int err;

	buf = bt_hci_cmd_create(hci_op, sizeof(*set_data));
	if (!buf) {
		return -ENOBUFS;
	}

	set_data = net_buf_add(buf, sizeof(*set_data));
	(void)memset(set_data, 0, sizeof(*set_data));

	err = set_data_add(set_data->data, BT_GAP_ADV_MAX_ADV_DATA_LEN,
			   ad, ad_len, &set_data->len);
	if (err) {
		net_buf_unref(buf);
		return err;
	}

	return bt_hci_cmd_send_sync(hci_op, buf, NULL);
}

/* Set legacy data using Extended Advertising HCI commands */
static int hci_set_ad_ext(struct bt_le_ext_adv *adv, uint16_t hci_op,
			  const struct bt_ad *ad, size_t ad_len)
{
	struct bt_hci_cp_le_set_ext_adv_data *set_data;
	struct net_buf *buf;
	int err;

	buf = bt_hci_cmd_create(hci_op, sizeof(*set_data));
	if (!buf) {
		return -ENOBUFS;
	}

	set_data = net_buf_add(buf, sizeof(*set_data));
	(void)memset(set_data, 0, sizeof(*set_data));

	err = set_data_add(set_data->data, BT_HCI_LE_EXT_ADV_FRAG_MAX_LEN,
			   ad, ad_len, &set_data->len);
	if (err) {
		net_buf_unref(buf);
		return err;
	}

	set_data->handle = adv->handle;
	set_data->op = BT_HCI_LE_EXT_ADV_OP_COMPLETE_DATA;
	set_data->frag_pref = BT_HCI_LE_EXT_ADV_FRAG_DISABLED;

	return bt_hci_cmd_send_sync(hci_op, buf, NULL);
}

static int set_ad(struct bt_le_ext_adv *adv, const struct bt_ad *ad,
		  size_t ad_len)
{
	if (IS_ENABLED(CONFIG_BT_EXT_ADV) &&
	    BT_FEAT_LE_EXT_ADV(bt_dev.le.features)) {
		return hci_set_ad_ext(adv, BT_HCI_OP_LE_SET_EXT_ADV_DATA,
				      ad, ad_len);
	}

	return hci_set_ad(BT_HCI_OP_LE_SET_ADV_DATA, ad, ad_len);
}

static int set_sd(struct bt_le_ext_adv *adv, const struct bt_ad *sd,
		  size_t sd_len)
{
	if (IS_ENABLED(CONFIG_BT_EXT_ADV) &&
	    BT_FEAT_LE_EXT_ADV(bt_dev.le.features)) {
		return hci_set_ad_ext(adv, BT_HCI_OP_LE_SET_EXT_SCAN_RSP_DATA,
				      sd, sd_len);
	}

	return hci_set_ad(BT_HCI_OP_LE_SET_SCAN_RSP_DATA, sd, sd_len);
}

static inline bool ad_has_name(const struct bt_data *ad, size_t ad_len)
{
	size_t i;

	for (i = 0; i < ad_len; i++) {
		if (ad[i].type == BT_DATA_NAME_COMPLETE ||
		    ad[i].type == BT_DATA_NAME_SHORTENED) {
			return true;
		}
	}

	return false;
}

static int le_adv_update(struct bt_le_ext_adv *adv,
			 const struct bt_data *ad, size_t ad_len,
			 const struct bt_data *sd, size_t sd_len,
			 bool ext_adv, bool scannable, bool use_name,
			 bool force_name_in_ad)
{
	struct bt_ad d[2] = {};
	struct bt_data data;
	size_t d_len;
	int err;

	if (use_name) {
		const char *name = bt_get_name();

		if ((ad && ad_has_name(ad, ad_len)) ||
		    (sd && ad_has_name(sd, sd_len))) {
			/* Cannot use name if name is already set */
			return -EINVAL;
		}

		data = (struct bt_data)BT_DATA(
			BT_DATA_NAME_COMPLETE,
			name, strlen(name));
	}

	if (!(ext_adv && scannable) || force_name_in_ad) {
		d_len = 1;
		d[0].data = ad;
		d[0].len = ad_len;

		if (use_name && (!scannable || force_name_in_ad)) {
			d[1].data = &data;
			d[1].len = 1;
			d_len = 2;
		}

		err = set_ad(adv, d, d_len);
		if (err) {
			return err;
		}
	}

	if (scannable) {
		d_len = 1;
		d[0].data = sd;
		d[0].len = sd_len;

		if (use_name && !force_name_in_ad) {
			d[1].data = &data;
			d[1].len = 1;
			d_len = 2;
		}

		err = set_sd(adv, d, d_len);
		if (err) {
			return err;
		}
	}

	atomic_set_bit(adv->flags, BT_ADV_DATA_SET);
	return 0;
}

int bt_le_adv_update_data(const struct bt_data *ad, size_t ad_len,
			  const struct bt_data *sd, size_t sd_len)
{
	struct bt_le_ext_adv *adv = bt_le_adv_lookup_legacy();
	bool scannable, use_name, force_name_in_ad;

	if (!adv) {
		return -EINVAL;
	}

	if (!atomic_test_bit(adv->flags, BT_ADV_ENABLED)) {
		return -EAGAIN;
	}

	scannable = atomic_test_bit(adv->flags, BT_ADV_SCANNABLE);
	use_name = atomic_test_bit(adv->flags, BT_ADV_INCLUDE_NAME);
	force_name_in_ad = atomic_test_bit(adv->flags, BT_ADV_FORCE_NAME_IN_AD);

	return le_adv_update(adv, ad, ad_len, sd, sd_len, false, scannable,
			     use_name, force_name_in_ad);
}

static uint8_t get_filter_policy(uint32_t options)
{
	if (!IS_ENABLED(CONFIG_BT_WHITELIST)) {
		return BT_LE_ADV_FP_NO_WHITELIST;
	} else if ((options & BT_LE_ADV_OPT_FILTER_SCAN_REQ) &&
		   (options & BT_LE_ADV_OPT_FILTER_CONN)) {
		return BT_LE_ADV_FP_WHITELIST_BOTH;
	} else if (options & BT_LE_ADV_OPT_FILTER_SCAN_REQ) {
		return BT_LE_ADV_FP_WHITELIST_SCAN_REQ;
	} else if (options & BT_LE_ADV_OPT_FILTER_CONN) {
		return BT_LE_ADV_FP_WHITELIST_CONN_IND;
	} else {
		return BT_LE_ADV_FP_NO_WHITELIST;
	}
}

static uint8_t get_adv_channel_map(uint32_t options)
{
	uint8_t channel_map = 0x07;

	if (options & BT_LE_ADV_OPT_DISABLE_CHAN_37) {
		channel_map &= ~0x01;
	}

	if (options & BT_LE_ADV_OPT_DISABLE_CHAN_38) {
		channel_map &= ~0x02;
	}

	if (options & BT_LE_ADV_OPT_DISABLE_CHAN_39) {
		channel_map &= ~0x04;
	}

	return channel_map;
}

static int le_adv_start_add_conn(const struct bt_le_ext_adv *adv,
				 struct bt_conn **out_conn)
{
	struct bt_conn *conn;

	bt_dev.adv_conn_id = adv->id;

	if (!bt_addr_le_cmp(&adv->target_addr, BT_ADDR_LE_ANY)) {
		/* Undirected advertising */
		conn = bt_conn_add_le(adv->id, BT_ADDR_LE_NONE);
		if (!conn) {
			return -ENOMEM;
		}

		bt_conn_set_state(conn, BT_CONN_CONNECT_ADV);
		*out_conn = conn;
		return 0;
	}

	if (bt_conn_exists_le(adv->id, &adv->target_addr)) {
		return -EINVAL;
	}

	conn = bt_conn_add_le(adv->id, &adv->target_addr);
	if (!conn) {
		return -ENOMEM;
	}

	bt_conn_set_state(conn, BT_CONN_CONNECT_DIR_ADV);
	*out_conn = conn;
	return 0;
}

static void le_adv_stop_free_conn(const struct bt_le_ext_adv *adv, uint8_t status)
{
	struct bt_conn *conn;

	if (!bt_addr_le_cmp(&adv->target_addr, BT_ADDR_LE_ANY)) {
		conn = bt_conn_lookup_state_le(adv->id, BT_ADDR_LE_NONE,
					       BT_CONN_CONNECT_ADV);
	} else {
		conn = bt_conn_lookup_state_le(adv->id, &adv->target_addr,
					       BT_CONN_CONNECT_DIR_ADV);
	}

	if (conn) {
		conn->err = status;
		bt_conn_set_state(conn, BT_CONN_DISCONNECTED);
		bt_conn_unref(conn);
	}
}

int bt_le_adv_start_legacy(struct bt_le_ext_adv *adv,
			   const struct bt_le_adv_param *param,
			   const struct bt_data *ad, size_t ad_len,
			   const struct bt_data *sd, size_t sd_len)
{
	struct bt_hci_cp_le_set_adv_param set_param;
	struct bt_conn *conn = NULL;
	struct net_buf *buf;
	bool dir_adv = (param->peer != NULL), scannable;
	int err;

	if (!atomic_test_bit(bt_dev.flags, BT_DEV_READY)) {
		return -EAGAIN;
	}

	if (!valid_adv_param(param)) {
		return -EINVAL;
	}

	if (!bt_id_adv_random_addr_check(param)) {
		return -EINVAL;
	}

	if (atomic_test_bit(adv->flags, BT_ADV_ENABLED)) {
		return -EALREADY;
	}

	(void)memset(&set_param, 0, sizeof(set_param));

	set_param.min_interval = sys_cpu_to_le16(param->interval_min);
	set_param.max_interval = sys_cpu_to_le16(param->interval_max);
	set_param.channel_map  = get_adv_channel_map(param->options);
	set_param.filter_policy = get_filter_policy(param->options);

	if (adv->id != param->id) {
		atomic_clear_bit(bt_dev.flags, BT_DEV_RPA_VALID);
	}

	adv->id = param->id;
	bt_dev.adv_conn_id = adv->id;

	err = bt_id_set_adv_own_addr(adv, param->options, dir_adv,
				     &set_param.own_addr_type);
	if (err) {
		return err;
	}

	if (dir_adv) {
		bt_addr_le_copy(&adv->target_addr, param->peer);
	} else {
		bt_addr_le_copy(&adv->target_addr, BT_ADDR_LE_ANY);
	}

	if (param->options & BT_LE_ADV_OPT_CONNECTABLE) {
		scannable = true;

		if (dir_adv) {
			if (param->options & BT_LE_ADV_OPT_DIR_MODE_LOW_DUTY) {
				set_param.type = BT_HCI_ADV_DIRECT_IND_LOW_DUTY;
			} else {
				set_param.type = BT_HCI_ADV_DIRECT_IND;
			}

			bt_addr_le_copy(&set_param.direct_addr, param->peer);
		} else {
			set_param.type = BT_HCI_ADV_IND;
		}
	} else {
		scannable = sd || (param->options & BT_LE_ADV_OPT_USE_NAME);

		set_param.type = scannable ? BT_HCI_ADV_SCAN_IND :
					     BT_HCI_ADV_NONCONN_IND;
	}

	buf = bt_hci_cmd_create(BT_HCI_OP_LE_SET_ADV_PARAM, sizeof(set_param));
	if (!buf) {
		return -ENOBUFS;
	}

	net_buf_add_mem(buf, &set_param, sizeof(set_param));

	err = bt_hci_cmd_send_sync(BT_HCI_OP_LE_SET_ADV_PARAM, buf, NULL);
	if (err) {
		return err;
	}

	if (!dir_adv) {
		err = le_adv_update(adv, ad, ad_len, sd, sd_len, false,
				    scannable,
				    param->options & BT_LE_ADV_OPT_USE_NAME,
				    param->options & BT_LE_ADV_OPT_FORCE_NAME_IN_AD);
		if (err) {
			return err;
		}
	}

	if (IS_ENABLED(CONFIG_BT_PERIPHERAL) &&
	    (param->options & BT_LE_ADV_OPT_CONNECTABLE)) {
		err = le_adv_start_add_conn(adv, &conn);
		if (err) {
			if (err == -ENOMEM && !dir_adv &&
			    !(param->options & BT_LE_ADV_OPT_ONE_TIME)) {
				goto set_adv_state;
			}

			return err;
		}
	}

	err = bt_le_adv_set_enable(adv, true);
	if (err) {
		BT_ERR("Failed to start advertiser");
		if (IS_ENABLED(CONFIG_BT_PERIPHERAL) && conn) {
			bt_conn_set_state(conn, BT_CONN_DISCONNECTED);
			bt_conn_unref(conn);
		}

		return err;
	}

	if (IS_ENABLED(CONFIG_BT_PERIPHERAL) && conn) {
		/* If undirected connectable advertiser we have created a
		 * connection object that we don't yet give to the application.
		 * Since we don't give the application a reference to manage in
		 * this case, we need to release this reference here
		 */
		bt_conn_unref(conn);
	}

set_adv_state:
	atomic_set_bit_to(adv->flags, BT_ADV_PERSIST, !dir_adv &&
			  !(param->options & BT_LE_ADV_OPT_ONE_TIME));

	atomic_set_bit_to(adv->flags, BT_ADV_INCLUDE_NAME,
			  param->options & BT_LE_ADV_OPT_USE_NAME);

	atomic_set_bit_to(adv->flags, BT_ADV_FORCE_NAME_IN_AD,
			  param->options & BT_LE_ADV_OPT_FORCE_NAME_IN_AD);

	atomic_set_bit_to(adv->flags, BT_ADV_CONNECTABLE,
			  param->options & BT_LE_ADV_OPT_CONNECTABLE);

	atomic_set_bit_to(adv->flags, BT_ADV_SCANNABLE, scannable);

	atomic_set_bit_to(adv->flags, BT_ADV_USE_IDENTITY,
			  param->options & BT_LE_ADV_OPT_USE_IDENTITY);

	return 0;
}

static int le_ext_adv_param_set(struct bt_le_ext_adv *adv,
				const struct bt_le_adv_param *param,
				bool  has_scan_data)
{
	struct bt_hci_cp_le_set_ext_adv_param *cp;
	bool dir_adv = param->peer != NULL, scannable;
	struct net_buf *buf, *rsp;
	int err;

	buf = bt_hci_cmd_create(BT_HCI_OP_LE_SET_EXT_ADV_PARAM, sizeof(*cp));
	if (!buf) {
		return -ENOBUFS;
	}

	cp = net_buf_add(buf, sizeof(*cp));
	(void)memset(cp, 0, sizeof(*cp));

	err = bt_id_set_adv_own_addr(adv, param->options, dir_adv,
				     &cp->own_addr_type);
	if (err) {
		return err;
	}

	if (dir_adv) {
		bt_addr_le_copy(&adv->target_addr, param->peer);
	} else {
		bt_addr_le_copy(&adv->target_addr, BT_ADDR_LE_ANY);
	}

	cp->handle = adv->handle;
	sys_put_le24(param->interval_min, cp->prim_min_interval);
	sys_put_le24(param->interval_max, cp->prim_max_interval);
	cp->prim_channel_map = get_adv_channel_map(param->options);
	cp->filter_policy = get_filter_policy(param->options);
	cp->tx_power = BT_HCI_LE_ADV_TX_POWER_NO_PREF;

	cp->prim_adv_phy = BT_HCI_LE_PHY_1M;
	if (param->options & BT_LE_ADV_OPT_EXT_ADV) {
		if (param->options & BT_LE_ADV_OPT_NO_2M) {
			cp->sec_adv_phy = BT_HCI_LE_PHY_1M;
		} else {
			cp->sec_adv_phy = BT_HCI_LE_PHY_2M;
		}
	}

	if (param->options & BT_LE_ADV_OPT_CODED) {
		cp->prim_adv_phy = BT_HCI_LE_PHY_CODED;
		cp->sec_adv_phy = BT_HCI_LE_PHY_CODED;
	}

	if (!(param->options & BT_LE_ADV_OPT_EXT_ADV)) {
		cp->props |= BT_HCI_LE_ADV_PROP_LEGACY;
	}

	if (param->options & BT_LE_ADV_OPT_USE_TX_POWER) {
		cp->props |= BT_HCI_LE_ADV_PROP_TX_POWER;
	}

	if (param->options & BT_LE_ADV_OPT_ANONYMOUS) {
		cp->props |= BT_HCI_LE_ADV_PROP_ANON;
	}

	if (param->options & BT_LE_ADV_OPT_NOTIFY_SCAN_REQ) {
		cp->scan_req_notify_enable = BT_HCI_LE_ADV_SCAN_REQ_ENABLE;
	}

	if (param->options & BT_LE_ADV_OPT_CONNECTABLE) {
		cp->props |= BT_HCI_LE_ADV_PROP_CONN;
		if (!dir_adv && !(param->options & BT_LE_ADV_OPT_EXT_ADV)) {
			/* When using non-extended adv packets then undirected
			 * advertising has to be scannable as well.
			 * We didn't require this option to be set before, so
			 * it is implicitly set instead in this case.
			 */
			cp->props |= BT_HCI_LE_ADV_PROP_SCAN;
		}
	}

	if ((param->options & BT_LE_ADV_OPT_SCANNABLE) || has_scan_data) {
		cp->props |= BT_HCI_LE_ADV_PROP_SCAN;
	}

	scannable = !!(cp->props & BT_HCI_LE_ADV_PROP_SCAN);

	if (dir_adv) {
		cp->props |= BT_HCI_LE_ADV_PROP_DIRECT;
		if (!(param->options & BT_LE_ADV_OPT_DIR_MODE_LOW_DUTY)) {
			cp->props |= BT_HCI_LE_ADV_PROP_HI_DC_CONN;
		}

		bt_addr_le_copy(&cp->peer_addr, param->peer);
	}

	cp->sid = param->sid;

	err = bt_hci_cmd_send_sync(BT_HCI_OP_LE_SET_EXT_ADV_PARAM, buf, &rsp);
	if (err) {
		return err;
	}

#if defined(CONFIG_BT_EXT_ADV)
	struct bt_hci_rp_le_set_ext_adv_param *rp = (void *)rsp->data;

	adv->tx_power = rp->tx_power;
#endif /* defined(CONFIG_BT_EXT_ADV) */

	net_buf_unref(rsp);

	atomic_set_bit(adv->flags, BT_ADV_PARAMS_SET);

	if (atomic_test_and_clear_bit(adv->flags, BT_ADV_RANDOM_ADDR_PENDING)) {
		err = bt_id_set_adv_random_addr(adv, &adv->random_addr.a);
		if (err) {
			return err;
		}
	}

	/* Flag only used by bt_le_adv_start API. */
	atomic_set_bit_to(adv->flags, BT_ADV_PERSIST, false);

	atomic_set_bit_to(adv->flags, BT_ADV_INCLUDE_NAME,
			  param->options & BT_LE_ADV_OPT_USE_NAME);

	atomic_set_bit_to(adv->flags, BT_ADV_FORCE_NAME_IN_AD,
			  param->options & BT_LE_ADV_OPT_FORCE_NAME_IN_AD);

	atomic_set_bit_to(adv->flags, BT_ADV_CONNECTABLE,
			  param->options & BT_LE_ADV_OPT_CONNECTABLE);

	atomic_set_bit_to(adv->flags, BT_ADV_SCANNABLE, scannable);

	atomic_set_bit_to(adv->flags, BT_ADV_USE_IDENTITY,
			  param->options & BT_LE_ADV_OPT_USE_IDENTITY);

	atomic_set_bit_to(adv->flags, BT_ADV_EXT_ADV,
			  param->options & BT_LE_ADV_OPT_EXT_ADV);

	return 0;
}

int bt_le_adv_start_ext(struct bt_le_ext_adv *adv,
			const struct bt_le_adv_param *param,
			const struct bt_data *ad, size_t ad_len,
			const struct bt_data *sd, size_t sd_len)
{
	struct bt_le_ext_adv_start_param start_param = {
		.timeout = 0,
		.num_events = 0,
	};
	bool dir_adv = (param->peer != NULL);
	struct bt_conn *conn = NULL;
	int err;

	if (!atomic_test_bit(bt_dev.flags, BT_DEV_READY)) {
		return -EAGAIN;
	}

	if (!valid_adv_param(param)) {
		return -EINVAL;
	}

	if (atomic_test_bit(adv->flags, BT_ADV_ENABLED)) {
		return -EALREADY;
	}

	adv->id = param->id;
	err = le_ext_adv_param_set(adv, param, sd ||
				   (param->options & BT_LE_ADV_OPT_USE_NAME));
	if (err) {
		return err;
	}

	if (!dir_adv) {
		err = bt_le_ext_adv_set_data(adv, ad, ad_len, sd, sd_len);
		if (err) {
			return err;
		}
	} else {
		if (!(param->options & BT_LE_ADV_OPT_DIR_MODE_LOW_DUTY)) {
			start_param.timeout =
				BT_GAP_ADV_HIGH_DUTY_CYCLE_MAX_TIMEOUT;
			atomic_set_bit(adv->flags, BT_ADV_LIMITED);
		}
	}

	if (IS_ENABLED(CONFIG_BT_PERIPHERAL) &&
	    (param->options & BT_LE_ADV_OPT_CONNECTABLE)) {
		err = le_adv_start_add_conn(adv, &conn);
		if (err) {
			if (err == -ENOMEM && !dir_adv &&
			    !(param->options & BT_LE_ADV_OPT_ONE_TIME)) {
				goto set_adv_state;
			}

			return err;
		}
	}

	err = bt_le_adv_set_enable_ext(adv, true, &start_param);
	if (err) {
		BT_ERR("Failed to start advertiser");
		if (IS_ENABLED(CONFIG_BT_PERIPHERAL) && conn) {
			bt_conn_set_state(conn, BT_CONN_DISCONNECTED);
			bt_conn_unref(conn);
		}

		return err;
	}

	if (IS_ENABLED(CONFIG_BT_PERIPHERAL) && conn) {
		/* If undirected connectable advertiser we have created a
		 * connection object that we don't yet give to the application.
		 * Since we don't give the application a reference to manage in
		 * this case, we need to release this reference here
		 */
		bt_conn_unref(conn);
	}

set_adv_state:
	/* Flag always set to false by le_ext_adv_param_set */
	atomic_set_bit_to(adv->flags, BT_ADV_PERSIST, !dir_adv &&
			  !(param->options & BT_LE_ADV_OPT_ONE_TIME));

	return 0;
}

int bt_le_adv_start(const struct bt_le_adv_param *param,
		    const struct bt_data *ad, size_t ad_len,
		    const struct bt_data *sd, size_t sd_len)
{
	struct bt_le_ext_adv *adv = adv_new_legacy();
	int err;

	if (!adv) {
		return -ENOMEM;
	}

	if (IS_ENABLED(CONFIG_BT_EXT_ADV) &&
	    BT_FEAT_LE_EXT_ADV(bt_dev.le.features)) {
		err = bt_le_adv_start_ext(adv, param, ad, ad_len, sd, sd_len);
	} else {
		err = bt_le_adv_start_legacy(adv, param, ad, ad_len, sd, sd_len);
	}

	if (err) {
		bt_le_adv_delete_legacy();
	}

	return err;
}

int bt_le_adv_stop(void)
{
	struct bt_le_ext_adv *adv = bt_le_adv_lookup_legacy();
	int err;

	if (!adv) {
		BT_ERR("No valid legacy adv");
		return 0;
	}

	/* Make sure advertising is not re-enabled later even if it's not
	 * currently enabled (i.e. BT_DEV_ADVERTISING is not set).
	 */
	atomic_clear_bit(adv->flags, BT_ADV_PERSIST);

	if (!atomic_test_bit(adv->flags, BT_ADV_ENABLED)) {
		/* Legacy advertiser exists, but is not currently advertising.
		 * This happens when keep advertising behavior is active but
		 * no conn object is available to do connectable advertising.
		 */
		bt_le_adv_delete_legacy();
		return 0;
	}

	if (IS_ENABLED(CONFIG_BT_PERIPHERAL) &&
	    atomic_test_bit(adv->flags, BT_ADV_CONNECTABLE)) {
		le_adv_stop_free_conn(adv, 0);
	}

	if (IS_ENABLED(CONFIG_BT_EXT_ADV) &&
	    BT_FEAT_LE_EXT_ADV(bt_dev.le.features)) {
		err = bt_le_adv_set_enable_ext(adv, false, NULL);
		if (err) {
			return err;
		}
	} else {
		err = bt_le_adv_set_enable_legacy(adv, false);
		if (err) {
			return err;
		}
	}

	bt_le_adv_delete_legacy();

#if defined(CONFIG_BT_OBSERVER)
	if (!(IS_ENABLED(CONFIG_BT_EXT_ADV) &&
	      BT_FEAT_LE_EXT_ADV(bt_dev.le.features)) &&
	    !IS_ENABLED(CONFIG_BT_PRIVACY) &&
	    !IS_ENABLED(CONFIG_BT_SCAN_WITH_IDENTITY)) {
		/* If scan is ongoing set back NRPA */
		if (atomic_test_bit(bt_dev.flags, BT_DEV_SCANNING)) {
			bt_le_scan_set_enable(BT_HCI_LE_SCAN_DISABLE);
			bt_id_set_private_addr(BT_ID_DEFAULT);
			bt_le_scan_set_enable(BT_HCI_LE_SCAN_ENABLE);
		}
	}
#endif /* defined(CONFIG_BT_OBSERVER) */

	return 0;
}

#if defined(CONFIG_BT_PERIPHERAL)
void bt_le_adv_resume(void)
{
	struct bt_le_ext_adv *adv = bt_le_adv_lookup_legacy();
	struct bt_conn *conn;
	bool persist_paused = false;
	int err;

	if (!adv) {
		BT_DBG("No valid legacy adv");
		return;
	}

	if (!(atomic_test_bit(adv->flags, BT_ADV_PERSIST) &&
	      !atomic_test_bit(adv->flags, BT_ADV_ENABLED))) {
		return;
	}

	if (!atomic_test_bit(adv->flags, BT_ADV_CONNECTABLE)) {
		return;
	}

	err = le_adv_start_add_conn(adv, &conn);
	if (err) {
		BT_DBG("Host cannot resume connectable advertising (%d)", err);
		return;
	}

	BT_DBG("Resuming connectable advertising");

	if (IS_ENABLED(CONFIG_BT_PRIVACY) &&
	    !atomic_test_bit(adv->flags, BT_ADV_USE_IDENTITY)) {
		bt_id_set_adv_private_addr(adv);
	}

	err = bt_le_adv_set_enable(adv, true);
	if (err) {
		BT_DBG("Controller cannot resume connectable advertising (%d)",
		       err);
		bt_conn_set_state(conn, BT_CONN_DISCONNECTED);

		/* Temporarily clear persist flag to avoid recursion in
		 * bt_conn_unref if the flag is still set.
		 */
		persist_paused = atomic_test_and_clear_bit(adv->flags,
							   BT_ADV_PERSIST);
	}

	/* Since we don't give the application a reference to manage in
	 * this case, we need to release this reference here.
	 */
	bt_conn_unref(conn);
	if (persist_paused) {
		atomic_set_bit(adv->flags, BT_ADV_PERSIST);
	}
}
#endif /* defined(CONFIG_BT_PERIPHERAL) */

#if defined(CONFIG_BT_EXT_ADV)
int bt_le_ext_adv_get_info(const struct bt_le_ext_adv *adv,
			   struct bt_le_ext_adv_info *info)
{
	info->id = adv->id;
	info->tx_power = adv->tx_power;

	return 0;
}

int bt_le_ext_adv_create(const struct bt_le_adv_param *param,
			 const struct bt_le_ext_adv_cb *cb,
			 struct bt_le_ext_adv **out_adv)
{
	struct bt_le_ext_adv *adv;
	int err;

	if (!atomic_test_bit(bt_dev.flags, BT_DEV_READY)) {
		return -EAGAIN;
	}

	if (!valid_adv_ext_param(param)) {
		return -EINVAL;
	}

	adv = adv_new();
	if (!adv) {
		return -ENOMEM;
	}

	adv->id = param->id;
	adv->cb = cb;

	err = le_ext_adv_param_set(adv, param, false);
	if (err) {
		adv_delete(adv);
		return err;
	}

	*out_adv = adv;
	return 0;
}

int bt_le_ext_adv_update_param(struct bt_le_ext_adv *adv,
			       const struct bt_le_adv_param *param)
{
	if (!valid_adv_ext_param(param)) {
		return -EINVAL;
	}

	if (IS_ENABLED(CONFIG_BT_PER_ADV) &&
	    atomic_test_bit(adv->flags, BT_PER_ADV_PARAMS_SET)) {
		/* If params for per adv has been set, do not allow setting
		 * connectable, scanable or use legacy adv
		 */
		if (param->options & BT_LE_ADV_OPT_CONNECTABLE ||
		    param->options & BT_LE_ADV_OPT_SCANNABLE ||
		    !(param->options & BT_LE_ADV_OPT_EXT_ADV) ||
		    param->options & BT_LE_ADV_OPT_ANONYMOUS) {
			return -EINVAL;
		}
	}

	if (atomic_test_bit(adv->flags, BT_ADV_ENABLED)) {
		return -EINVAL;
	}

	if (param->id != adv->id) {
		atomic_clear_bit(adv->flags, BT_ADV_RPA_VALID);
	}

	return le_ext_adv_param_set(adv, param, false);
}

int bt_le_ext_adv_start(struct bt_le_ext_adv *adv,
			struct bt_le_ext_adv_start_param *param)
{
	struct bt_conn *conn = NULL;
	int err;

	if (atomic_test_bit(adv->flags, BT_ADV_ENABLED)) {
		return -EALREADY;
	}

	if (IS_ENABLED(CONFIG_BT_PERIPHERAL) &&
	    atomic_test_bit(adv->flags, BT_ADV_CONNECTABLE)) {
		err = le_adv_start_add_conn(adv, &conn);
		if (err) {
			return err;
		}
	}

	atomic_set_bit_to(adv->flags, BT_ADV_LIMITED, param &&
			  (param->timeout > 0 || param->num_events > 0));

	if (atomic_test_bit(adv->flags, BT_ADV_CONNECTABLE)) {
		if (IS_ENABLED(CONFIG_BT_PRIVACY) &&
		    !atomic_test_bit(adv->flags, BT_ADV_USE_IDENTITY)) {
			bt_id_set_adv_private_addr(adv);
		}
	} else {
		if (!atomic_test_bit(adv->flags, BT_ADV_USE_IDENTITY)) {
			bt_id_set_adv_private_addr(adv);
		}
	}

	if (atomic_test_bit(adv->flags, BT_ADV_INCLUDE_NAME) &&
	    !atomic_test_bit(adv->flags, BT_ADV_DATA_SET)) {
		/* Set the advertiser name */
		bt_le_ext_adv_set_data(adv, NULL, 0, NULL, 0);
	}

	err = bt_le_adv_set_enable_ext(adv, true, param);
	if (err) {
		BT_ERR("Failed to start advertiser");
		if (IS_ENABLED(CONFIG_BT_PERIPHERAL) && conn) {
			bt_conn_set_state(conn, BT_CONN_DISCONNECTED);
			bt_conn_unref(conn);
		}

		return err;
	}

	if (IS_ENABLED(CONFIG_BT_PERIPHERAL) && conn) {
		/* If undirected connectable advertiser we have created a
		 * connection object that we don't yet give to the application.
		 * Since we don't give the application a reference to manage in
		 * this case, we need to release this reference here
		 */
		bt_conn_unref(conn);
	}

	return 0;
}

int bt_le_ext_adv_stop(struct bt_le_ext_adv *adv)
{
	atomic_clear_bit(adv->flags, BT_ADV_PERSIST);

	if (!atomic_test_bit(adv->flags, BT_ADV_ENABLED)) {
		return 0;
	}

	if (atomic_test_and_clear_bit(adv->flags, BT_ADV_LIMITED)) {
		atomic_clear_bit(adv->flags, BT_ADV_RPA_VALID);

#if defined(CONFIG_BT_SMP)
		bt_id_pending_keys_update();
#endif
	}

	if (IS_ENABLED(CONFIG_BT_PERIPHERAL) &&
	    atomic_test_bit(adv->flags, BT_ADV_CONNECTABLE)) {
		le_adv_stop_free_conn(adv, 0);
	}

	return bt_le_adv_set_enable_ext(adv, false, NULL);
}

int bt_le_ext_adv_set_data(struct bt_le_ext_adv *adv,
			   const struct bt_data *ad, size_t ad_len,
			   const struct bt_data *sd, size_t sd_len)
{
	bool ext_adv, scannable, use_name, force_name_in_ad;

	ext_adv = atomic_test_bit(adv->flags, BT_ADV_EXT_ADV);
	scannable = atomic_test_bit(adv->flags, BT_ADV_SCANNABLE);
	use_name = atomic_test_bit(adv->flags, BT_ADV_INCLUDE_NAME);
	force_name_in_ad = atomic_test_bit(adv->flags, BT_ADV_FORCE_NAME_IN_AD);

	return le_adv_update(adv, ad, ad_len, sd, sd_len, ext_adv, scannable,
			     use_name, force_name_in_ad);
}

int bt_le_ext_adv_delete(struct bt_le_ext_adv *adv)
{
	struct bt_hci_cp_le_remove_adv_set *cp;
	struct net_buf *buf;
	int err;

	if (!BT_FEAT_LE_EXT_ADV(bt_dev.le.features)) {
		return -ENOTSUP;
	}

	/* Advertising set should be stopped first */
	if (atomic_test_bit(adv->flags, BT_ADV_ENABLED)) {
		return -EINVAL;
	}

	buf = bt_hci_cmd_create(BT_HCI_OP_LE_REMOVE_ADV_SET, sizeof(*cp));
	if (!buf) {
		BT_WARN("No HCI buffers");
		return -ENOBUFS;
	}

	cp = net_buf_add(buf, sizeof(*cp));
	cp->handle = adv->handle;

	err = bt_hci_cmd_send_sync(BT_HCI_OP_LE_REMOVE_ADV_SET, buf, NULL);
	if (err) {
		return err;
	}

	adv_delete(adv);

	return 0;
}
#endif /* defined(CONFIG_BT_EXT_ADV) */


#if defined(CONFIG_BT_PER_ADV)
int bt_le_per_adv_set_param(struct bt_le_ext_adv *adv,
			    const struct bt_le_per_adv_param *param)
{
	struct bt_hci_cp_le_set_per_adv_param *cp;
	struct net_buf *buf;
	int err;

	if (!BT_FEAT_LE_EXT_PER_ADV(bt_dev.le.features)) {
		return -ENOTSUP;
	}

	if (atomic_test_bit(adv->flags, BT_ADV_SCANNABLE)) {
		return -EINVAL;
	} else if (atomic_test_bit(adv->flags, BT_ADV_CONNECTABLE)) {
		return -EINVAL;
	} else if (!atomic_test_bit(adv->flags, BT_ADV_EXT_ADV)) {
		return -EINVAL;
	}

	if (param->interval_min < BT_GAP_PER_ADV_MIN_INTERVAL ||
	    param->interval_max > BT_GAP_PER_ADV_MAX_INTERVAL ||
	    param->interval_min > param->interval_max) {
		return -EINVAL;
	}

	buf = bt_hci_cmd_create(BT_HCI_OP_LE_SET_PER_ADV_PARAM, sizeof(*cp));
	if (!buf) {
		return -ENOBUFS;
	}

	cp = net_buf_add(buf, sizeof(*cp));
	(void)memset(cp, 0, sizeof(*cp));

	cp->handle = adv->handle;
	cp->min_interval = sys_cpu_to_le16(param->interval_min);
	cp->max_interval = sys_cpu_to_le16(param->interval_max);

	if (param->options & BT_LE_PER_ADV_OPT_USE_TX_POWER) {
		cp->props |= BT_HCI_LE_ADV_PROP_TX_POWER;
	}

	err = bt_hci_cmd_send_sync(BT_HCI_OP_LE_SET_PER_ADV_PARAM, buf, NULL);
	if (err) {
		return err;
	}

	atomic_set_bit(adv->flags, BT_PER_ADV_PARAMS_SET);

	return 0;
}

int bt_le_per_adv_set_data(const struct bt_le_ext_adv *adv,
			   const struct bt_data *ad, size_t ad_len)
{
	struct bt_hci_cp_le_set_per_adv_data *cp;
	struct net_buf *buf;
	struct bt_ad d = { .data = ad, .len = ad_len };
	int err;

	if (!BT_FEAT_LE_EXT_PER_ADV(bt_dev.le.features)) {
		return -ENOTSUP;
	}

	if (!atomic_test_bit(adv->flags, BT_PER_ADV_PARAMS_SET)) {
		return -EINVAL;
	}

	if (!ad_len || !ad) {
		return -EINVAL;
	}

	if (ad_len > BT_HCI_LE_PER_ADV_FRAG_MAX_LEN) {
		return -EINVAL;
	}


	buf = bt_hci_cmd_create(BT_HCI_OP_LE_SET_PER_ADV_DATA, sizeof(*cp));
	if (!buf) {
		return -ENOBUFS;
	}

	cp = net_buf_add(buf, sizeof(*cp));
	(void)memset(cp, 0, sizeof(*cp));

	cp->handle = adv->handle;

	/* TODO: If data is longer than what the controller can manage,
	 * split the data. Read size from controller on boot.
	 */
	cp->op = BT_HCI_LE_PER_ADV_OP_COMPLETE_DATA;

	err = set_data_add(cp->data, BT_HCI_LE_PER_ADV_FRAG_MAX_LEN, &d, 1,
			   &cp->len);
	if (err) {
		return err;
	}

	err = bt_hci_cmd_send_sync(BT_HCI_OP_LE_SET_PER_ADV_DATA, buf, NULL);
	if (err) {
		return err;
	}

	return 0;
}

static int bt_le_per_adv_enable(struct bt_le_ext_adv *adv, bool enable)
{
	struct bt_hci_cp_le_set_per_adv_enable *cp;
	struct net_buf *buf;
	struct bt_hci_cmd_state_set state;
	int err;

	if (!BT_FEAT_LE_EXT_PER_ADV(bt_dev.le.features)) {
		return -ENOTSUP;
	}

	/* TODO: We could setup some default ext adv params if not already set*/
	if (!atomic_test_bit(adv->flags, BT_PER_ADV_PARAMS_SET)) {
		return -EINVAL;
	}

	if (atomic_test_bit(adv->flags, BT_PER_ADV_ENABLED) == enable) {
		return -EALREADY;
	}

	buf = bt_hci_cmd_create(BT_HCI_OP_LE_SET_PER_ADV_ENABLE, sizeof(*cp));
	if (!buf) {
		return -ENOBUFS;
	}

	cp = net_buf_add(buf, sizeof(*cp));
	(void)memset(cp, 0, sizeof(*cp));

	cp->handle = adv->handle;
	cp->enable = enable ? 1 : 0;

	bt_hci_cmd_state_set_init(buf, &state, adv->flags,
				  BT_PER_ADV_ENABLED, enable);

	err = bt_hci_cmd_send_sync(BT_HCI_OP_LE_SET_PER_ADV_ENABLE, buf, NULL);
	if (err) {
		return err;
	}

	return 0;
}

int bt_le_per_adv_start(struct bt_le_ext_adv *adv)
{
	return bt_le_per_adv_enable(adv, true);
}

int bt_le_per_adv_stop(struct bt_le_ext_adv *adv)
{
	return bt_le_per_adv_enable(adv, false);
}

#if defined(CONFIG_BT_CONN)
int bt_le_per_adv_set_info_transfer(const struct bt_le_ext_adv *adv,
				    const struct bt_conn *conn,
				    uint16_t service_data)
{
	struct bt_hci_cp_le_per_adv_set_info_transfer *cp;
	struct net_buf *buf;


	if (!BT_FEAT_LE_EXT_PER_ADV(bt_dev.le.features)) {
		return -ENOTSUP;
	} else if (!BT_FEAT_LE_PAST_SEND(bt_dev.le.features)) {
		return -ENOTSUP;
	}

	buf = bt_hci_cmd_create(BT_HCI_OP_LE_PER_ADV_SET_INFO_TRANSFER,
				sizeof(*cp));
	if (!buf) {
		return -ENOBUFS;
	}

	cp = net_buf_add(buf, sizeof(*cp));
	(void)memset(cp, 0, sizeof(*cp));

	cp->conn_handle = sys_cpu_to_le16(conn->handle);
	cp->adv_handle = adv->handle;
	cp->service_data = sys_cpu_to_le16(service_data);

	return bt_hci_cmd_send_sync(BT_HCI_OP_LE_PER_ADV_SET_INFO_TRANSFER, buf,
				    NULL);
}
#endif /* CONFIG_BT_CONN */
#endif /* CONFIG_BT_PER_ADV */

#if defined(CONFIG_BT_EXT_ADV)
#if defined(CONFIG_BT_BROADCASTER)
void bt_hci_le_adv_set_terminated(struct net_buf *buf)
{
	struct bt_hci_evt_le_adv_set_terminated *evt;
	struct bt_le_ext_adv *adv;
	uint16_t conn_handle;

	evt = (void *)buf->data;
	adv = bt_adv_lookup_handle(evt->adv_handle);
	conn_handle = sys_le16_to_cpu(evt->conn_handle);

#if (CONFIG_BT_ID_MAX > 1) && (CONFIG_BT_EXT_ADV_MAX_ADV_SET > 1)
	bt_dev.adv_conn_id = adv->id;
	for (int i = 0; i < ARRAY_SIZE(bt_dev.cached_conn_complete); i++) {
		if (bt_dev.cached_conn_complete[i].valid &&
		    bt_dev.cached_conn_complete[i].evt.handle == evt->conn_handle) {
			if (atomic_test_bit(adv->flags, BT_ADV_ENABLED)) {
				/* Process the cached connection complete event
				 * now that the corresponding advertising set is known.
				 *
				 * If the advertiser has been stopped before the connection
				 * complete event has been raised to the application, we
				 * discard the event.
				 */
				bt_hci_le_enh_conn_complete(&bt_dev.cached_conn_complete[i].evt);
			}
			bt_dev.cached_conn_complete[i].valid = false;
		}
	}
#endif

	BT_DBG("status 0x%02x adv_handle %u conn_handle 0x%02x num %u",
	       evt->status, evt->adv_handle, conn_handle,
	       evt->num_completed_ext_adv_evts);

	if (!adv) {
		BT_ERR("No valid adv");
		return;
	}

	atomic_clear_bit(adv->flags, BT_ADV_ENABLED);

	if (evt->status && IS_ENABLED(CONFIG_BT_PERIPHERAL) &&
	    atomic_test_bit(adv->flags, BT_ADV_CONNECTABLE)) {
		/* Only set status for legacy advertising API.
		 * This will call connected callback for high duty cycle
		 * directed advertiser timeout.
		 */
		le_adv_stop_free_conn(adv, adv == bt_dev.adv ? evt->status : 0);
	}

	if (IS_ENABLED(CONFIG_BT_CONN) && !evt->status) {
		struct bt_conn *conn = bt_conn_lookup_handle(conn_handle);

		if (conn) {
			if (IS_ENABLED(CONFIG_BT_PRIVACY) &&
			    !atomic_test_bit(adv->flags, BT_ADV_USE_IDENTITY)) {
				/* Set Responder address unless already set */
				conn->le.resp_addr.type = BT_ADDR_LE_RANDOM;
				if (bt_addr_cmp(&conn->le.resp_addr.a,
						BT_ADDR_ANY) == 0) {
					bt_addr_copy(&conn->le.resp_addr.a,
						     &adv->random_addr.a);
				}
			} else {
				bt_addr_le_copy(&conn->le.resp_addr,
					&bt_dev.id_addr[conn->id]);
			}

			if (adv->cb && adv->cb->connected) {
				struct bt_le_ext_adv_connected_info info = {
					.conn = conn,
				};

				adv->cb->connected(adv, &info);
			}

			bt_conn_unref(conn);
		}
	}

	if (atomic_test_and_clear_bit(adv->flags, BT_ADV_LIMITED)) {
		atomic_clear_bit(adv->flags, BT_ADV_RPA_VALID);

#if defined(CONFIG_BT_SMP)
		bt_id_pending_keys_update();
#endif

		if (adv->cb && adv->cb->sent) {
			struct bt_le_ext_adv_sent_info info = {
				.num_sent = evt->num_completed_ext_adv_evts,
			};

			adv->cb->sent(adv, &info);
		}
	}

	if (!atomic_test_bit(adv->flags, BT_ADV_PERSIST) && adv == bt_dev.adv) {
		bt_le_adv_delete_legacy();
	}
}

void bt_hci_le_scan_req_received(struct net_buf *buf)
{
	struct bt_hci_evt_le_scan_req_received *evt;
	struct bt_le_ext_adv *adv;

	evt = (void *)buf->data;
	adv = bt_adv_lookup_handle(evt->handle);

	BT_DBG("handle %u peer %s", evt->handle, bt_addr_le_str(&evt->addr));

	if (!adv) {
		BT_ERR("No valid adv");
		return;
	}

	if (adv->cb && adv->cb->scanned) {
		struct bt_le_ext_adv_scanned_info info;
		bt_addr_le_t id_addr;

		if (evt->addr.type == BT_ADDR_LE_PUBLIC_ID ||
		    evt->addr.type == BT_ADDR_LE_RANDOM_ID) {
			bt_addr_le_copy(&id_addr, &evt->addr);
			id_addr.type -= BT_ADDR_LE_PUBLIC_ID;
		} else {
			bt_addr_le_copy(&id_addr,
					bt_lookup_id_addr(adv->id, &evt->addr));
		}

		info.addr = &id_addr;
		adv->cb->scanned(adv, &info);
	}
}
#endif /* defined(CONFIG_BT_BROADCASTER) */
#endif /* defined(CONFIG_BT_EXT_ADV) */
