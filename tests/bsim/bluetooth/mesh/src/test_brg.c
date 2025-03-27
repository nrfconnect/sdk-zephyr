/*
 * Copyright (c) 2024 Nordic Semiconductor
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Subnet bridge test
 */

#include "mesh_test.h"
#include <zephyr/bluetooth/mesh.h>
#include "mesh/net.h"
#include "mesh/keys.h"
#include "bsim_args_runner.h"
#include "common/bt_str.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(test_brg, LOG_LEVEL_INF);

#define WAIT_TIME          32  /*seconds*/
#define WAIT_TIME_IVU_TEST 240 /* seconds */
#define BEACON_INTERVAL    10  /*seconds */

#define PROV_ADDR         0x0001
/* Bridge address must be less than DEVICE_ADDR_START */
#define BRIDGE_ADDR       0x0002
#define DEVICE_ADDR_START 0x0003

#define REMOTE_NODES 2

static const uint8_t prov_dev_key[16] = {0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
					 0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef};

static const uint8_t subnet_keys[][16] = {
	{0xaa, 0xbb, 0xcc},
	{0xdd, 0xee, 0xff},
	{0x11, 0x22, 0x33},
};

static uint8_t prov_uuid[16] = {0x6c, 0x69, 0x6e, 0x67, 0x61, 0xaa};
static uint8_t bridge_uuid[16] = {0x6c, 0x69, 0x6e, 0x67, 0x61, 0xbb};
static uint8_t dev_uuid[16] = {0x6c, 0x69, 0x6e, 0x67, 0x61, 0xcc};

static int test_ividx = 0x123456;

extern const struct bt_mesh_comp comp;
static bool tester_ready;

enum {
	MSG_TYPE_DATA = 0,
	MSG_TYPE_GET = 1,
	MSG_TYPE_STATUS = 2,
};

static uint8_t recvd_msgs[10];
static uint8_t recvd_msgs_cnt;

BUILD_ASSERT((2 /* opcode */ + 1 /* type */ + 1 /* msgs cnt */ + sizeof(recvd_msgs) +
	      BT_MESH_MIC_SHORT) <= BT_MESH_RX_SDU_MAX,
	     "Status message does not fit into the maximum incoming SDU size.");
BUILD_ASSERT((2 /* opcode */ + 1 /* type */ + 1 /* msgs cnt */ + sizeof(recvd_msgs) +
	      BT_MESH_MIC_SHORT) <= BT_MESH_TX_SDU_MAX,
	     "Status message does not fit into the maximum outgoing SDU size.");

static K_SEM_DEFINE(status_msg_recvd_sem, 0, 1);
static K_SEM_DEFINE(prov_sem, 0, 1);

static void test_tester_init(void)
{
	/* Stub */
}

static void test_bridge_init(void)
{
	/* Bridge device must always be the second device. */
	ASSERT_EQUAL(1, get_device_nbr());
}

static void test_device_init(void)
{
	ASSERT_TRUE_MSG(get_device_nbr() >= 2, "Regular devices must be initialized after "
					       "tester and Bridge devices.");

	/* Regular devices addresses starts from address 0x0003.*/
	dev_uuid[6] = get_device_nbr() + 1;

	/* Regular devices are provisioned into subnets starting with idx 1. */
	dev_uuid[8] = get_device_nbr() - 1;
}

static void unprovisioned_beacon(uint8_t uuid[16], bt_mesh_prov_oob_info_t oob_info,
				 uint32_t *uri_hash)
{
	int err;

	/* Subnet may not be ready yet when tester receives a beacon. */
	if (!tester_ready) {
		LOG_INF("tester is not ready yet");
		return;
	}

	LOG_INF("Received unprovisioned beacon, uuid %s", bt_hex(uuid, 16));

	if (!memcmp(uuid, bridge_uuid, 16)) {
		err = bt_mesh_provision_adv(uuid, 0, BRIDGE_ADDR, 0);
		if (!err) {
			LOG_INF("Provisioning bridge at address 0x%04x", BRIDGE_ADDR);
		}

		return;
	}

	/* UUID[6] - address to be used for provisioning.
	 * UUID[8] - subnet to be used for provisioning.
	 */
	uint16_t addr = uuid[6];
	int subnet_idx = uuid[8];

	err = bt_mesh_provision_adv(uuid, subnet_idx, addr, 0);
	if (!err) {
		LOG_INF("Provisioning device at address 0x%04x with NetKeyIdx 0x%04x", addr,
			subnet_idx);
	}
}

static void prov_node_added(uint16_t net_idx, uint8_t uuid[16], uint16_t addr, uint8_t num_elem)
{
	LOG_INF("Device 0x%04x provisioned, NetKeyIdx 0x%04x", addr, net_idx);
	k_sem_give(&prov_sem);
}

static struct bt_mesh_prov tester_prov = {.uuid = prov_uuid,
					  .unprovisioned_beacon = unprovisioned_beacon,
					  .node_added = prov_node_added};

static void prov_complete(uint16_t net_idx, uint16_t addr)
{
	LOG_INF("Device 0x%04x provisioning is complete, NetKeyIdx 0x%04x", addr, net_idx);
	k_sem_give(&prov_sem);
}

static struct bt_mesh_prov device_prov = {
	.uuid = dev_uuid,
	.complete = prov_complete,
};

static struct bt_mesh_prov bridge_prov = {
	.uuid = bridge_uuid,
	.complete = prov_complete,
};

static void tester_setup(void)
{
	uint8_t status;
	int err;

	ASSERT_OK(bt_mesh_cdb_create(test_net_key));
	ASSERT_OK(bt_mesh_provision(test_net_key, 0, 0, test_ividx, PROV_ADDR, prov_dev_key));

	err = bt_mesh_cfg_cli_app_key_add(0, PROV_ADDR, 0, 0, test_app_key, &status);
	if (err || status) {
		FAIL("AppKey add failed (err %d, status %u)", err, status);
		return;
	}

	err = bt_mesh_cfg_cli_mod_app_bind(0, PROV_ADDR, PROV_ADDR, 0, TEST_MOD_ID, &status);
	if (err || status) {
		FAIL("Mod app bind failed (err %d, status %u)", err, status);
		return;
	}

	for (int i = 0; i < REMOTE_NODES; i++) {
		LOG_INF("Creating subnet idx %d", i);

		ASSERT_OK(
			bt_mesh_cfg_cli_net_key_add(0, PROV_ADDR, i + 1, subnet_keys[i], &status));
		if (status) {
			FAIL("NetKey add failed (status %u)", status);
			return;
		}

		struct bt_mesh_cdb_subnet *subnet = bt_mesh_cdb_subnet_alloc(i + 1);

		ASSERT_TRUE(subnet != NULL);

		ASSERT_OK(bt_mesh_cdb_subnet_key_import(subnet, 0, subnet_keys[i]));

		bt_mesh_cdb_subnet_store(subnet);
	}

	uint8_t transmit;

	ASSERT_OK(bt_mesh_cfg_cli_relay_set(0, PROV_ADDR, BT_MESH_RELAY_DISABLED,
					    BT_MESH_TRANSMIT(2, 20), &status, &transmit));
	if (status) {
		FAIL("Relay set failed (status %u)", status);
		return;
	}

	tester_ready = true;
}

static void bridge_entry_add(uint16_t src, uint16_t dst, uint16_t net_idx1, uint16_t net_idx2,
			     uint8_t dir)
{
	struct bt_mesh_brg_cfg_table_entry entry;
	struct bt_mesh_brg_cfg_table_status rsp;
	int err;

	entry.directions = dir;
	entry.net_idx1 = net_idx1;
	entry.net_idx2 = net_idx2;
	entry.addr1 = src;
	entry.addr2 = dst;

	err = bt_mesh_brg_cfg_cli_table_add(0, BRIDGE_ADDR, &entry, &rsp);
	if (err || rsp.status || rsp.entry.directions != dir || rsp.entry.net_idx1 != net_idx1 ||
	    rsp.entry.net_idx2 != net_idx2 || rsp.entry.addr1 != src || rsp.entry.addr2 != dst) {
		FAIL("Bridging table add failed (err %d) (status %u)", err, rsp.status);
		return;
	}
}

static void bridge_entry_remove(uint16_t src, uint16_t dst, uint16_t net_idx1, uint16_t net_idx2)
{
	struct bt_mesh_brg_cfg_table_status rsp;

	ASSERT_OK(bt_mesh_brg_cfg_cli_table_remove(0, BRIDGE_ADDR, net_idx1, net_idx2, src, dst,
						   &rsp));
	if (rsp.status) {
		FAIL("Bridging table remove failed (status %u)", rsp.status);
		return;
	}
}

static void tester_bridge_configure(int subnets)
{
	uint8_t status;
	int err;

	LOG_INF("Configuring bridge...");

	for (int i = 0; i < subnets; i++) {
		err = bt_mesh_cfg_cli_net_key_add(0, BRIDGE_ADDR, i + 1, subnet_keys[i], &status);
		if (err || status) {
			FAIL("NetKey add failed (err %d, status %u)", err, status);
			return;
		}
	}

	ASSERT_OK(bt_mesh_brg_cfg_cli_set(0, BRIDGE_ADDR, BT_MESH_BRG_CFG_ENABLED, &status));
	if (status != BT_MESH_BRG_CFG_ENABLED) {
		FAIL("Subnet bridge set failed (status %u)", status);
		return;
	}

	/* Disable Relay feature to avoid interference in the test. */
	uint8_t transmit;

	ASSERT_OK(bt_mesh_cfg_cli_relay_set(0, BRIDGE_ADDR, BT_MESH_RELAY_DISABLED,
					    BT_MESH_TRANSMIT(2, 20), &status, &transmit));
	if (status) {
		FAIL("Relay set failed (status %u)", status);
		return;
	}

	LOG_INF("Bridge configured");
}

static void tester_device_configure(uint16_t net_key_idx, uint16_t addr)
{
	int err;
	uint8_t status;

	err = bt_mesh_cfg_cli_app_key_add(net_key_idx, addr, net_key_idx, 0, test_app_key, &status);
	if (err || status) {
		FAIL("AppKey add failed (err %d, status %u)", err, status);
		return;
	}

	err = bt_mesh_cfg_cli_mod_app_bind(net_key_idx, addr, addr, 0, TEST_MOD_ID, &status);
	if (err || status) {
		FAIL("Mod app bind failed (err %d, status %u)", err, status);
		return;
	}

	/* Disable SNB on devices to let Subnet Bridge propagate new IV index value. */
	err = bt_mesh_cfg_cli_beacon_set(net_key_idx, addr, BT_MESH_BEACON_DISABLED, &status);
	if (err || status) {
		FAIL("Beacon set failed (err %d, status %u)", err, status);
		return;
	}

	LOG_INF("Device 0x%04x configured", addr);
}

static void tester_ra_cb(uint8_t *data, size_t length)
{
	uint8_t type = data[0];

	LOG_HEXDUMP_DBG(data, length, "tester received message");

	ASSERT_TRUE_MSG(length > 1, "Too short message");
	ASSERT_EQUAL(type, MSG_TYPE_STATUS);

	recvd_msgs_cnt = data[1];
	ASSERT_EQUAL(recvd_msgs_cnt * sizeof(recvd_msgs[0]), length - 2);
	memcpy(recvd_msgs, &data[2], recvd_msgs_cnt * sizeof(recvd_msgs[0]));

	k_sem_give(&status_msg_recvd_sem);
}

static int send_data(uint16_t dst, uint8_t payload)
{
	uint8_t data[2] = {MSG_TYPE_DATA, payload};

	return bt_mesh_test_send_ra(dst, data, sizeof(data), NULL, NULL);
}

static int send_get(uint16_t dst)
{
	uint8_t data[1] = {MSG_TYPE_GET};

	return bt_mesh_test_send_ra(dst, data, sizeof(data), NULL, NULL);
}

struct bridged_addresses_entry {
	uint16_t addr1;
	uint16_t addr2;
	uint8_t dir;
};

static void bridge_table_verify(uint16_t net_idx1, uint16_t net_idx2, uint16_t start_idx,
				struct bridged_addresses_entry *list, size_t list_len)
{
	struct bt_mesh_brg_cfg_table_list rsp = {
		.list = NET_BUF_SIMPLE(BT_MESH_RX_SDU_MAX),
	};

	net_buf_simple_init(rsp.list, 0);

	ASSERT_OK(
		bt_mesh_brg_cfg_cli_table_get(0, BRIDGE_ADDR, net_idx1, net_idx2, start_idx, &rsp));
	ASSERT_EQUAL(rsp.status, 0);
	ASSERT_EQUAL(rsp.net_idx1, net_idx1);
	ASSERT_EQUAL(rsp.net_idx2, net_idx2);
	ASSERT_EQUAL(rsp.start_idx, start_idx);

	LOG_HEXDUMP_DBG(rsp.list->data, rsp.list->len, "Received table");

	ASSERT_EQUAL(rsp.list->len / 5, list_len);
	ASSERT_EQUAL(rsp.list->len % 5, 0);

	for (int i = 0; i < list_len; i++) {
		struct bridged_addresses_entry entry;

		entry.addr1 = net_buf_simple_pull_le16(rsp.list);
		entry.addr2 = net_buf_simple_pull_le16(rsp.list);
		entry.dir = net_buf_simple_pull_u8(rsp.list);

		ASSERT_EQUAL(entry.addr1, list[i].addr1);
		ASSERT_EQUAL(entry.addr2, list[i].addr2);
		ASSERT_EQUAL(entry.dir, list[i].dir);
	}
}

static void device_ra_cb(uint8_t *data, size_t length)
{
	uint8_t type = data[0];

	LOG_HEXDUMP_DBG(data, length, "Device received message");

	switch (type) {
	case MSG_TYPE_DATA:
		ASSERT_EQUAL(2, length);
		ASSERT_TRUE_MSG(recvd_msgs_cnt < ARRAY_SIZE(recvd_msgs), "Too many messages");

		recvd_msgs[recvd_msgs_cnt] = data[1];
		recvd_msgs_cnt++;

		break;

	case MSG_TYPE_GET: {
		uint8_t test_data[1 /*type */ + 1 /* msgs cnt */ + sizeof(recvd_msgs)] = {
			MSG_TYPE_STATUS, recvd_msgs_cnt};

		memcpy(&test_data[2], recvd_msgs, recvd_msgs_cnt * sizeof(recvd_msgs[0]));

		ASSERT_OK(bt_mesh_test_send_ra(PROV_ADDR, test_data,
					       2 + recvd_msgs_cnt * sizeof(recvd_msgs[0]), NULL,
					       NULL));

		memset(recvd_msgs, 0, sizeof(recvd_msgs));
		recvd_msgs_cnt = 0;

		break;
	}

	case MSG_TYPE_STATUS:
		ASSERT_TRUE_MSG(false, "Unexpected message");
		break;
	}
}

/**
 * This is a workaround that removes secondary subnets from the tester to avoid message cache
 * hit when the devices send STATUS message encrypted with the subnet key known by the tester,
 * but with different app key pair (app key is the same, but net key <-> app key pair is different).
 */
static void tester_workaround(void)
{
	uint8_t status;
	int err;

	LOG_INF("Applying subnet's workaround for tester...");

	for (int i = 0; i < REMOTE_NODES; i++) {
		err = bt_mesh_cfg_cli_net_key_del(0, PROV_ADDR, i + 1, &status);
		if (err || status) {
			FAIL("NetKey del failed (err %d, status %u)", err, status);
			return;
		}
	}
}

static void send_and_receive(void)
{
	const int msgs_cnt = 3;

	LOG_INF("Sending data...");

	for (int i = 0; i < REMOTE_NODES; i++) {
		uint8_t payload = i | i << 4;

		for (int j = 0; j < msgs_cnt; j++) {
			ASSERT_OK(send_data(DEVICE_ADDR_START + i, payload + j));
		}
	}

	LOG_INF("Checking data...");

	for (int i = 0; i < REMOTE_NODES; i++) {
		uint8_t payload = i | i << 4;

		ASSERT_OK(send_get(DEVICE_ADDR_START + i));
		ASSERT_OK(k_sem_take(&status_msg_recvd_sem, K_SECONDS(5)));

		ASSERT_EQUAL(recvd_msgs_cnt, msgs_cnt);
		for (int j = 0; j < recvd_msgs_cnt; j++) {
			ASSERT_EQUAL(recvd_msgs[j], payload + j);
		}
	}
}

static void test_tester_simple(void)
{
	uint8_t status;
	int err;

	bt_mesh_test_cfg_set(NULL, WAIT_TIME);
	bt_mesh_device_setup(&tester_prov, &comp);

	tester_setup();

	for (int i = 0; i < 1 /* bridge */ + REMOTE_NODES; i++) {
		LOG_INF("Waiting for a device to provision...");
		ASSERT_OK(k_sem_take(&prov_sem, K_SECONDS(40)));
	}

	tester_bridge_configure(REMOTE_NODES);

	/* Adding devices to bridge table */
	for (int i = 0; i < REMOTE_NODES; i++) {
		bridge_entry_add(PROV_ADDR, DEVICE_ADDR_START + i, 0, i + 1,
				 BT_MESH_BRG_CFG_DIR_TWOWAY);
	}

	for (int i = 0; i < REMOTE_NODES; i++) {
		tester_device_configure(i + 1, DEVICE_ADDR_START + i);
	}

	tester_workaround();

	bt_mesh_test_ra_cb_setup(tester_ra_cb);

	LOG_INF("Step 1: Checking bridging table...");

	send_and_receive();

	LOG_INF("Step 2: Disabling bridging...");

	err = bt_mesh_brg_cfg_cli_set(0, BRIDGE_ADDR, BT_MESH_BRG_CFG_DISABLED, &status);
	if (err || status != BT_MESH_BRG_CFG_DISABLED) {
		FAIL("Subnet bridge set failed (err %d) (status %u)", err, status);
		return;
	}

	LOG_INF("Sending data...");
	for (int i = 0; i < REMOTE_NODES; i++) {
		uint8_t payload = i | i << 4;

		for (int j = 0; j < 3; j++) {
			ASSERT_OK(send_data(DEVICE_ADDR_START + i, payload + j));
		}
	}

	LOG_INF("Step3: Enabling bridging...");
	err = bt_mesh_brg_cfg_cli_set(0, BRIDGE_ADDR, BT_MESH_BRG_CFG_ENABLED, &status);
	if (err || status != BT_MESH_BRG_CFG_ENABLED) {
		FAIL("Subnet bridge set failed (err %d) (status %u)", err, status);
		return;
	}

	LOG_INF("Checking data...");
	for (int i = 0; i < REMOTE_NODES; i++) {
		ASSERT_OK(send_get(DEVICE_ADDR_START + i));
		ASSERT_OK(k_sem_take(&status_msg_recvd_sem, K_SECONDS(5)));

		ASSERT_EQUAL(recvd_msgs_cnt, 0);
	}

	PASS();
}

static void test_tester_table_state_change(void)
{
	int err;

	bt_mesh_test_cfg_set(NULL, WAIT_TIME);
	bt_mesh_device_setup(&tester_prov, &comp);

	tester_setup();

	for (int i = 0; i < 1 /* bridge */ + REMOTE_NODES; i++) {
		LOG_INF("Waiting for a device to provision...");
		ASSERT_OK(k_sem_take(&prov_sem, K_SECONDS(40)));
	}

	tester_bridge_configure(REMOTE_NODES);

	for (int i = 0; i < REMOTE_NODES; i++) {
		tester_device_configure(i + 1, DEVICE_ADDR_START + i);
	}

	tester_workaround();

	bt_mesh_test_ra_cb_setup(tester_ra_cb);

	/* Bridge Table is empty, will not get any message back. */
	ASSERT_OK(send_get(DEVICE_ADDR_START));
	err = k_sem_take(&status_msg_recvd_sem, K_SECONDS(5));
	ASSERT_EQUAL(err, -EAGAIN);

	/* DATA and GET messages should reach Device 1, but STATUS message won't be received. */
	bridge_entry_add(PROV_ADDR, DEVICE_ADDR_START, 0, 1, BT_MESH_BRG_CFG_DIR_ONEWAY);

	ASSERT_OK(send_data(DEVICE_ADDR_START, 0xAA));

	ASSERT_OK(send_get(DEVICE_ADDR_START));
	err = k_sem_take(&status_msg_recvd_sem, K_SECONDS(5));
	ASSERT_EQUAL(err, -EAGAIN);

	/* Sending DATA message again before adding a new entry as the previous GET message resets
	 * received messages counter on Devices
	 */
	ASSERT_OK(send_data(DEVICE_ADDR_START, 0xAA));
	/* Adding a reverse entry. This should be added to the bridge table as a separate entry as
	 * the addresses and net keys indexs are provided in the opposite order.
	 */
	bridge_entry_add(DEVICE_ADDR_START, PROV_ADDR, 1, 0, BT_MESH_BRG_CFG_DIR_ONEWAY);
	bridge_table_verify(0, 1, 0,
			    (struct bridged_addresses_entry[]){
				    {PROV_ADDR, DEVICE_ADDR_START, BT_MESH_BRG_CFG_DIR_ONEWAY},
			    },
			    1);
	bridge_table_verify(1, 0, 0,
			    (struct bridged_addresses_entry[]){
				    {DEVICE_ADDR_START, PROV_ADDR, BT_MESH_BRG_CFG_DIR_ONEWAY},
			    },
			    1);

	k_sleep(K_SECONDS(1));

	/* Now we should receive STATUS message. */
	ASSERT_OK(send_get(DEVICE_ADDR_START));
	ASSERT_OK(k_sem_take(&status_msg_recvd_sem, K_SECONDS(5)));

	ASSERT_EQUAL(recvd_msgs_cnt, 1);
	ASSERT_EQUAL(recvd_msgs[0], 0xAA);

	/* Removing the reverse entry and changing direction on the first entry.
	 * tester should still receive STATUS message.
	 */
	bridge_entry_remove(DEVICE_ADDR_START, PROV_ADDR, 1, 0);
	bridge_entry_add(PROV_ADDR, DEVICE_ADDR_START, 0, 1, BT_MESH_BRG_CFG_DIR_TWOWAY);
	bridge_table_verify(0, 1, 0,
			    (struct bridged_addresses_entry[]){
				    {PROV_ADDR, DEVICE_ADDR_START, BT_MESH_BRG_CFG_DIR_TWOWAY},
			    },
			    1);
	bridge_table_verify(1, 0, 0, NULL, 0);

	ASSERT_OK(send_get(DEVICE_ADDR_START));
	ASSERT_OK(k_sem_take(&status_msg_recvd_sem, K_SECONDS(5)));
	ASSERT_EQUAL(recvd_msgs_cnt, 0);

	PASS();
}

static void net_key_remove(uint16_t dst, uint16_t net_idx, uint16_t net_idx_to_remove)
{
	uint8_t status;
	int err;

	err = bt_mesh_cfg_cli_net_key_del(net_idx, dst, net_idx_to_remove, &status);
	if (err || status) {
		FAIL("NetKey del failed (err %d, status %u)", err, status);
		return;
	}
}

static void test_tester_net_key_remove(void)
{
	int err;

	bt_mesh_test_cfg_set(NULL, WAIT_TIME);
	bt_mesh_device_setup(&tester_prov, &comp);

	tester_setup();

	for (int i = 0; i < 1 /* bridge */ + REMOTE_NODES; i++) {
		LOG_INF("Waiting for a device to provision...");
		ASSERT_OK(k_sem_take(&prov_sem, K_SECONDS(40)));
	}

	tester_bridge_configure(REMOTE_NODES);

	for (int i = 0; i < REMOTE_NODES; i++) {
		tester_device_configure(i + 1, DEVICE_ADDR_START + i);
	}

	tester_workaround();

	bt_mesh_test_ra_cb_setup(tester_ra_cb);

	/* Adding devices to bridge table */
	for (int i = 0; i < REMOTE_NODES; i++) {
		bridge_entry_add(PROV_ADDR, DEVICE_ADDR_START + i, 0, i + 1,
				 BT_MESH_BRG_CFG_DIR_TWOWAY);
	}

	ASSERT_OK(send_data(DEVICE_ADDR_START, 0xAA));
	ASSERT_OK(send_get(DEVICE_ADDR_START));
	ASSERT_OK(k_sem_take(&status_msg_recvd_sem, K_SECONDS(5)));
	ASSERT_EQUAL(recvd_msgs_cnt, 1);
	ASSERT_EQUAL(recvd_msgs[0], 0xAA);

	/* Removing subnet 1 from Subnet Bridge. */
	net_key_remove(BRIDGE_ADDR, 0, 1);

	ASSERT_OK(send_get(DEVICE_ADDR_START));
	err = k_sem_take(&status_msg_recvd_sem, K_SECONDS(5));
	ASSERT_EQUAL(err, -EAGAIN);

	bridge_table_verify(0, 2, 0,
			    (struct bridged_addresses_entry[]){
				    {PROV_ADDR, DEVICE_ADDR_START + 1, BT_MESH_BRG_CFG_DIR_TWOWAY},
			    },
			    1);

	/* Bridging Table Get message will return Invalid NetKey Index error because Subnet 1 is
	 * removed.
	 */
	struct bt_mesh_brg_cfg_table_list rsp = {
		.list = NULL,
	};

	ASSERT_OK(bt_mesh_brg_cfg_cli_table_get(0, BRIDGE_ADDR, 0, 1, 0, &rsp));
	ASSERT_EQUAL(rsp.status, 4);

	PASS();
}

#if CONFIG_BT_SETTINGS
static void test_tester_persistence(void)
{
	bt_mesh_test_cfg_set(NULL, WAIT_TIME);

	bt_mesh_device_setup(&tester_prov, &comp);

	if (bt_mesh_is_provisioned()) {
		uint8_t status;

		LOG_INF("Already provisioned, skipping provisioning");

		ASSERT_OK(bt_mesh_brg_cfg_cli_get(0, BRIDGE_ADDR, &status));
		if (status != BT_MESH_BRG_CFG_ENABLED) {
			FAIL("Subnet bridge set failed (status %u)", status);
			return;
		}

		bridge_table_verify(
			0, 1, 0,
			(struct bridged_addresses_entry[]){
				{PROV_ADDR, DEVICE_ADDR_START, BT_MESH_BRG_CFG_DIR_TWOWAY},
			},
			1);

		bridge_table_verify(
			0, 2, 0,
			(struct bridged_addresses_entry[]){
				{PROV_ADDR, DEVICE_ADDR_START + 1, BT_MESH_BRG_CFG_DIR_TWOWAY},
			},
			1);

		bridge_table_verify(
			1, 0, 0,
			(struct bridged_addresses_entry[]){
				{DEVICE_ADDR_START, PROV_ADDR, BT_MESH_BRG_CFG_DIR_ONEWAY},
			},
			1);

		bridge_table_verify(
			2, 0, 0,
			(struct bridged_addresses_entry[]){
				{DEVICE_ADDR_START + 1, PROV_ADDR, BT_MESH_BRG_CFG_DIR_ONEWAY},
			},
			1);
	} else {
		tester_setup();

		LOG_INF("Waiting for a bridge to provision...");
		ASSERT_OK(k_sem_take(&prov_sem, K_SECONDS(40)));

		LOG_INF("Configuring bridge...");
		tester_bridge_configure(REMOTE_NODES);

		/* Adding devices to bridge table */
		for (int i = 0; i < REMOTE_NODES; i++) {
			bridge_entry_add(PROV_ADDR, DEVICE_ADDR_START + i, 0, i + 1,
					 BT_MESH_BRG_CFG_DIR_TWOWAY);
			bridge_entry_add(DEVICE_ADDR_START + i, PROV_ADDR, i + 1, 0,
					 BT_MESH_BRG_CFG_DIR_ONEWAY);
		}

		k_sleep(K_SECONDS(CONFIG_BT_MESH_STORE_TIMEOUT));
	}

	PASS();
}
#endif

/* When testing IV Index update, after the IV Index incremented devices starts sending messages
 * with SEQ number 0 that is lower than the SEQ number of the last message received before IV Index.
 * The Network Message Cache is not cleared and thus will drop these messages.
 *
 * The workaround is to send GET message to each device to bump SEQ number and overflow the cache so
 * that after IV Index update there is no message with SEQ 0 in the cache.
 */
static void msg_cache_workaround(void)
{
	LOG_INF("Applying Msg Cache workaround...");

	for (int i = 0; i < REMOTE_NODES; i++) {
		for (int j = 0; j < CONFIG_BT_MESH_MSG_CACHE_SIZE; j++) {
			ASSERT_OK(send_get(DEVICE_ADDR_START + i));
			/* k_sem_take is needed to not overflow network buffer pool. The result
			 * of the semaphor is not important as we just need to bump sequence number
			 * enough to bypass message cache.
			 */
			(void)k_sem_take(&status_msg_recvd_sem, K_SECONDS(1));
		}
	}

	LOG_INF("Msg Cache workaround applied");
	k_sleep(K_SECONDS(10));
}

static int beacon_set(uint16_t dst, uint8_t val)
{
	uint8_t status;
	int err;

	err = bt_mesh_cfg_cli_beacon_set(0, dst, val, &status);
	if (err || status != val) {
		FAIL("Beacon set failed (err %d, status %u)", err, status);
		return -EINVAL;
	}

	return 0;
}

/* This function guarantees that IV Update procedure state is propagated to all nodes by togging off
 * Beacon features on Subnet Bridge and Tester nodes. When Beacon feature is disabled on Subnet
 * Bridge, Tester will be able to send beacon with new IVI flag and vice versa.
 *
 * Beacon feature is disabled on other nodes at the setup.
 */
static void propagate_ivi_update_state(void)
{
	/* Disable Beacon feature on subnet bridge to let tester send beacon first. */
	ASSERT_OK(beacon_set(BRIDGE_ADDR, BT_MESH_BEACON_DISABLED));

	LOG_INF("Waiting for IV Update state to propagate to Subnet Bridge");
	k_sleep(K_SECONDS(BEACON_INTERVAL * 2));

	/* Disable Beacon feature on tester and enable it on subnet bridge to let it send beacon. */
	ASSERT_OK(beacon_set(PROV_ADDR, BT_MESH_BEACON_DISABLED));
	ASSERT_OK(beacon_set(BRIDGE_ADDR, BT_MESH_BEACON_ENABLED));

	LOG_INF("Waiting for IV Update state to propagate to other nodes");
	k_sleep(K_SECONDS(BEACON_INTERVAL * 2));

	/* Restore Beacon feature on tester. */
	ASSERT_OK(beacon_set(PROV_ADDR, BT_MESH_BEACON_ENABLED));
}

static void test_tester_ivu(void)
{
	bt_mesh_test_cfg_set(NULL, WAIT_TIME_IVU_TEST);
	bt_mesh_device_setup(&tester_prov, &comp);
	bt_mesh_iv_update_test(true);

	tester_setup();

	for (int i = 0; i < 1 /* bridge */ + REMOTE_NODES; i++) {
		LOG_INF("Waiting for a device to provision...");
		ASSERT_OK(k_sem_take(&prov_sem, K_SECONDS(40)));
	}

	tester_bridge_configure(REMOTE_NODES);

	/* Adding devices to bridge table */
	for (int i = 0; i < REMOTE_NODES; i++) {
		bridge_entry_add(PROV_ADDR, DEVICE_ADDR_START + i, 0, i + 1,
				 BT_MESH_BRG_CFG_DIR_TWOWAY);
	}

	for (int i = 0; i < REMOTE_NODES; i++) {
		tester_device_configure(i + 1, DEVICE_ADDR_START + i);
	}

	tester_workaround();

	bt_mesh_test_ra_cb_setup(tester_ra_cb);

	ASSERT_TRUE(!atomic_test_bit(bt_mesh.flags, BT_MESH_IVU_IN_PROGRESS));
	ASSERT_TRUE(bt_mesh.iv_index == test_ividx);

	LOG_INF("IV Update procedure state: Normal");

	k_sleep(K_SECONDS(BEACON_INTERVAL));

	send_and_receive();

	for (int i = 0; i < 2; i++) {
		uint32_t iv_index = bt_mesh.iv_index;

		LOG_INF("Round: %d", i);

		msg_cache_workaround();

		LOG_INF("Starting IV Update procedure, IVI %d -> %d", bt_mesh.iv_index,
			bt_mesh.iv_index + 1);

		iv_index = bt_mesh.iv_index;

		ASSERT_TRUE(bt_mesh_iv_update());
		ASSERT_TRUE(atomic_test_bit(bt_mesh.flags, BT_MESH_IVU_IN_PROGRESS));
		ASSERT_TRUE(bt_mesh.iv_index == iv_index + 1);

		send_and_receive();

		propagate_ivi_update_state();

		LOG_INF("Finishing IV Update procedure");

		ASSERT_TRUE(!bt_mesh_iv_update());
		ASSERT_TRUE(!atomic_test_bit(bt_mesh.flags, BT_MESH_IVU_IN_PROGRESS));
		ASSERT_TRUE(bt_mesh.iv_index == iv_index + 1);

		propagate_ivi_update_state();

		send_and_receive();
	}

	PASS();
}

static void bridge_setup(void)
{
	bt_mesh_device_setup(&bridge_prov, &comp);

	if (IS_ENABLED(CONFIG_BT_SETTINGS) && bt_mesh_is_provisioned()) {
		LOG_INF("Already provisioned, skipping provisioning");
	} else {
		ASSERT_OK(bt_mesh_prov_enable(BT_MESH_PROV_ADV));
		LOG_INF("Waiting for being provisioned...");
		ASSERT_OK(k_sem_take(&prov_sem, K_SECONDS(40)));
		LOG_INF("Bridge is provisioned");
	}
}

static void test_bridge_simple(void)
{
	bt_mesh_test_cfg_set(NULL, WAIT_TIME);

	bridge_setup();

	PASS();
}

static void test_bridge_simple_iv_test_mode(void)
{
	bt_mesh_test_cfg_set(NULL, WAIT_TIME_IVU_TEST);
	bt_mesh_iv_update_test(true);

	bridge_setup();

	PASS();
}

static void device_setup(void)
{
	bt_mesh_device_setup(&device_prov, &comp);

	ASSERT_OK(bt_mesh_prov_enable(BT_MESH_PROV_ADV));

	LOG_INF("Waiting for being provisioned...");
	ASSERT_OK(k_sem_take(&prov_sem, K_SECONDS(40)));
	LOG_INF("Node is provisioned");

	bt_mesh_test_ra_cb_setup(device_ra_cb);
}

static void test_device_simple(void)
{
	bt_mesh_test_cfg_set(NULL, WAIT_TIME);

	device_setup();

	PASS();
}

static void test_device_simple_iv_test_mode(void)
{
	bt_mesh_test_cfg_set(NULL, WAIT_TIME_IVU_TEST);
	bt_mesh_iv_update_test(true);

	device_setup();

	PASS();
}

#define TEST_CASE(role, name, description)                                                         \
	{                                                                                          \
		.test_id = "brg_" #role "_" #name,                                                 \
		.test_post_init_f = test_##role##_init,                                            \
		.test_descr = description,                                                         \
		.test_tick_f = bt_mesh_test_timeout,                                               \
		.test_main_f = test_##role##_##name,                                               \
	}

static const struct bst_test_instance test_brg[] = {
	TEST_CASE(tester, simple,
		  "Tester node: provisions network, exchanges messages with "
		  "mesh nodes"),
	TEST_CASE(tester, table_state_change,
		  "Tester node: tests changing bridging table "
		  "state"),
	TEST_CASE(tester, net_key_remove,
		  "Tester node: tests removing net key from Subnet "
		  "Bridge"),
#if CONFIG_BT_SETTINGS
	TEST_CASE(tester, persistence, "Tester node: test persistence of subnet bridge states"),
#endif
	TEST_CASE(tester, ivu, "Tester node: tests subnet bridge with IV Update procedure"),
	TEST_CASE(bridge, simple, "Subnet Bridge node"),
	TEST_CASE(device, simple, "A mesh node"),

	TEST_CASE(bridge, simple_iv_test_mode, "Subnet Bridge node with IV test mode enabled"),
	TEST_CASE(device, simple_iv_test_mode, "A mesh node with IV test mode enabled"),

	BSTEST_END_MARKER};

struct bst_test_list *test_brg_install(struct bst_test_list *tests)
{
	tests = bst_add_tests(tests, test_brg);
	return tests;
}
