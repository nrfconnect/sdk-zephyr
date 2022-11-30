/*
 * Copyright (c) 2017 Intel Corporation
 * Copyright (c) 2020 Lingao Meng
 * Copyright (c) 2021 Manulytica Limited
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <errno.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/sys/util.h>
#include <zephyr/sys/byteorder.h>

#include <zephyr/net/buf.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/mesh.h>
#include <zephyr/bluetooth/uuid.h>

#include "common/bt_str.h"

#include "host/ecc.h"
#include "host/testing.h"

#include "crypto.h"
#include "adv.h"
#include "mesh.h"
#include "net.h"
#include "rpl.h"
#include "beacon.h"
#include "access.h"
#include "foundation.h"
#include "proxy.h"
#include "prov.h"
#include "settings.h"

#define LOG_LEVEL CONFIG_BT_MESH_PROVISIONER_LOG_LEVEL
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(bt_mesh_provisioner);

static struct {
	struct bt_mesh_cdb_node *node;
	uint16_t addr;
	uint16_t net_idx;
	uint8_t attention_duration;
	uint8_t uuid[16];
} prov_device;

static void send_pub_key(void);
static void prov_dh_key_gen(void);
static void pub_key_ready(const uint8_t *pkey);

static int reset_state(void)
{
	if (prov_device.node != NULL) {
		bt_mesh_cdb_node_del(prov_device.node, false);
	}

	return bt_mesh_prov_reset_state(pub_key_ready);
}

static void prov_link_close(enum prov_bearer_link_status status)
{
	LOG_DBG("%u", status);
	bt_mesh_prov_link.expect = PROV_NO_PDU;

	bt_mesh_prov_link.bearer->link_close(status);
}

static void prov_fail(uint8_t reason)
{
	/* According to Bluetooth Mesh Specification v1.0.1, Section 5.4.4, the
	 * provisioner just closes the link when something fails, while the
	 * provisionee sends the fail message, and waits for the provisioner to
	 * close the link.
	 */
	prov_link_close(PROV_BEARER_LINK_STATUS_FAIL);
}

static void send_invite(void)
{
	PROV_BUF(inv, PDU_LEN_INVITE);

	LOG_DBG("");

	bt_mesh_prov_buf_init(&inv, PROV_INVITE);
	net_buf_simple_add_u8(&inv, prov_device.attention_duration);

	memcpy(bt_mesh_prov_link.conf_inputs.invite, &prov_device.attention_duration,
	       PDU_LEN_INVITE);

	if (bt_mesh_prov_send(&inv, NULL)) {
		LOG_ERR("Failed to send invite");
		return;
	}

	bt_mesh_prov_link.expect = PROV_CAPABILITIES;
}

static void start_sent(int err, void *cb_data)
{
	if (!bt_pub_key_get()) {
		atomic_set_bit(bt_mesh_prov_link.flags, WAIT_PUB_KEY);
		LOG_WRN("Waiting for local public key");
	} else {
		send_pub_key();
	}
}

static void send_start(void)
{
	LOG_DBG("");

	PROV_BUF(start, PDU_LEN_START);

	bool oob_pub_key = bt_mesh_prov_link.conf_inputs.capabilities[3] == PUB_KEY_OOB;

	bt_mesh_prov_buf_init(&start, PROV_START);
	net_buf_simple_add_u8(&start, PROV_ALG_P256);

	if (atomic_test_bit(bt_mesh_prov_link.flags, REMOTE_PUB_KEY) && oob_pub_key) {
		net_buf_simple_add_u8(&start, PUB_KEY_OOB);
		atomic_set_bit(bt_mesh_prov_link.flags, OOB_PUB_KEY);
	} else {
		net_buf_simple_add_u8(&start, PUB_KEY_NO_OOB);
	}

	net_buf_simple_add_u8(&start, bt_mesh_prov_link.oob_method);

	net_buf_simple_add_u8(&start, bt_mesh_prov_link.oob_action);

	net_buf_simple_add_u8(&start, bt_mesh_prov_link.oob_size);

	memcpy(bt_mesh_prov_link.conf_inputs.start, &start.data[1], PDU_LEN_START);

	if (bt_mesh_prov_auth(true, bt_mesh_prov_link.oob_method,
		bt_mesh_prov_link.oob_action, bt_mesh_prov_link.oob_size) < 0) {
		LOG_ERR("Invalid authentication method: 0x%02x; "
		       "action: 0x%02x; size: 0x%02x", bt_mesh_prov_link.oob_method,
		       bt_mesh_prov_link.oob_action, bt_mesh_prov_link.oob_size);
		return;
	}

	if (bt_mesh_prov_send(&start, start_sent)) {
		LOG_ERR("Failed to send Provisioning Start");
		return;
	}
}

static bool prov_check_method(struct bt_mesh_dev_capabilities *caps)
{
	if (bt_mesh_prov_link.oob_method == AUTH_METHOD_STATIC) {
		if (!caps->static_oob) {
			LOG_WRN("Device not support OOB static authentication provisioning");
			return false;
		}
	} else if (bt_mesh_prov_link.oob_method == AUTH_METHOD_INPUT) {
		if (bt_mesh_prov_link.oob_size > caps->input_size) {
			LOG_WRN("The required input length (0x%02x) "
				"exceeds the device capacity (0x%02x)",
				bt_mesh_prov_link.oob_size, caps->input_size);
			return false;
		}

		if (!(BIT(bt_mesh_prov_link.oob_action) & caps->input_actions)) {
			LOG_WRN("The required input action (0x%04x) "
				"not supported by the device (0x%02x)",
				(uint16_t)BIT(bt_mesh_prov_link.oob_action), caps->input_actions);
			return false;
		}

		if (bt_mesh_prov_link.oob_action == INPUT_OOB_STRING) {
			if (!bt_mesh_prov->output_string) {
				LOG_WRN("Not support output string");
				return false;
			}
		} else {
			if (!bt_mesh_prov->output_number) {
				LOG_WRN("Not support output number");
				return false;
			}
		}
	} else if (bt_mesh_prov_link.oob_method == AUTH_METHOD_OUTPUT) {
		if (bt_mesh_prov_link.oob_size > caps->output_size) {
			LOG_WRN("The required output length (0x%02x) "
				"exceeds the device capacity (0x%02x)",
				bt_mesh_prov_link.oob_size, caps->output_size);
			return false;
		}

		if (!(BIT(bt_mesh_prov_link.oob_action) & caps->output_actions)) {
			LOG_WRN("The required output action (0x%04x) "
				"not supported by the device (0x%02x)",
				(uint16_t)BIT(bt_mesh_prov_link.oob_action), caps->output_actions);
			return false;
		}

		if (!bt_mesh_prov->input) {
			LOG_WRN("Not support input");
			return false;
		}
	}

	return true;
}

static void prov_capabilities(const uint8_t *data)
{
	struct bt_mesh_dev_capabilities caps;

	caps.elem_count = data[0];
	LOG_DBG("Elements:          %u", caps.elem_count);

	caps.algorithms = sys_get_be16(&data[1]);
	LOG_DBG("Algorithms:        %u", caps.algorithms);

	caps.pub_key_type = data[3];
	caps.static_oob = data[4];
	caps.output_size = data[5];
	LOG_DBG("Public Key Type:   0x%02x", caps.pub_key_type);
	LOG_DBG("Static OOB Type:   0x%02x", caps.static_oob);
	LOG_DBG("Output OOB Size:   %u", caps.output_size);

	caps.output_actions = (bt_mesh_output_action_t)
					(sys_get_be16(&data[6]));
	caps.input_size = data[8];
	caps.input_actions = (bt_mesh_input_action_t)
					(sys_get_be16(&data[9]));
	LOG_DBG("Output OOB Action: 0x%04x", caps.output_actions);
	LOG_DBG("Input OOB Size:    %u", caps.input_size);
	LOG_DBG("Input OOB Action:  0x%04x", caps.input_actions);

	if (data[0] == 0) {
		LOG_ERR("Invalid number of elements");
		prov_fail(PROV_ERR_NVAL_FMT);
		return;
	}

	prov_device.node =
		bt_mesh_cdb_node_alloc(prov_device.uuid,
				       prov_device.addr, data[0],
				       prov_device.net_idx);
	if (prov_device.node == NULL) {
		LOG_ERR("Failed allocating node 0x%04x", prov_device.addr);
		prov_fail(PROV_ERR_RESOURCES);
		return;
	}

	memcpy(bt_mesh_prov_link.conf_inputs.capabilities, data, PDU_LEN_CAPABILITIES);

	if (bt_mesh_prov->capabilities) {
		bt_mesh_prov->capabilities(&caps);
	}

	if (!prov_check_method(&caps)) {
		prov_fail(PROV_ERR_UNEXP_ERR);
		return;
	}

	send_start();
}

static void send_confirm(void)
{
	PROV_BUF(cfm, PDU_LEN_CONFIRM);
	uint8_t *inputs = (uint8_t *)&bt_mesh_prov_link.conf_inputs;

	LOG_DBG("ConfInputs[0]   %s", bt_hex(inputs, 64));
	LOG_DBG("ConfInputs[64]  %s", bt_hex(&inputs[64], 64));
	LOG_DBG("ConfInputs[128] %s", bt_hex(&inputs[128], 17));

	if (bt_mesh_prov_conf_salt(inputs, bt_mesh_prov_link.conf_salt)) {
		LOG_ERR("Unable to generate confirmation salt");
		prov_fail(PROV_ERR_UNEXP_ERR);
		return;
	}

	LOG_DBG("ConfirmationSalt: %s", bt_hex(bt_mesh_prov_link.conf_salt, 16));

	if (bt_mesh_prov_conf_key(bt_mesh_prov_link.dhkey,
				  bt_mesh_prov_link.conf_salt, bt_mesh_prov_link.conf_key)) {
		LOG_ERR("Unable to generate confirmation key");
		prov_fail(PROV_ERR_UNEXP_ERR);
		return;
	}

	LOG_DBG("ConfirmationKey: %s", bt_hex(bt_mesh_prov_link.conf_key, 16));

	if (bt_rand(bt_mesh_prov_link.rand, 16)) {
		LOG_ERR("Unable to generate random number");
		prov_fail(PROV_ERR_UNEXP_ERR);
		return;
	}

	LOG_DBG("LocalRandom: %s", bt_hex(bt_mesh_prov_link.rand, 16));

	bt_mesh_prov_buf_init(&cfm, PROV_CONFIRM);

	if (bt_mesh_prov_conf(bt_mesh_prov_link.conf_key,
			      bt_mesh_prov_link.rand, bt_mesh_prov_link.auth,
			      bt_mesh_prov_link.conf)) {
		LOG_ERR("Unable to generate confirmation value");
		prov_fail(PROV_ERR_UNEXP_ERR);
		return;
	}

	net_buf_simple_add_mem(&cfm, bt_mesh_prov_link.conf, 16);

	if (bt_mesh_prov_send(&cfm, NULL)) {
		LOG_ERR("Failed to send Provisioning Confirm");
		return;
	}

	bt_mesh_prov_link.expect = PROV_CONFIRM;
}

static void public_key_sent(int err, void *cb_data)
{
	atomic_set_bit(bt_mesh_prov_link.flags, PUB_KEY_SENT);

	if (atomic_test_bit(bt_mesh_prov_link.flags, OOB_PUB_KEY) &&
	    atomic_test_bit(bt_mesh_prov_link.flags, REMOTE_PUB_KEY)) {
		prov_dh_key_gen();
		return;
	}
}

static void send_pub_key(void)
{
	PROV_BUF(buf, PDU_LEN_PUB_KEY);
	const uint8_t *key;

	key = bt_pub_key_get();
	if (!key) {
		LOG_ERR("No public key available");
		prov_fail(PROV_ERR_UNEXP_ERR);
		return;
	}

	bt_mesh_prov_buf_init(&buf, PROV_PUB_KEY);

	/* Swap X and Y halves independently to big-endian */
	sys_memcpy_swap(net_buf_simple_add(&buf, BT_PUB_KEY_COORD_LEN), key, BT_PUB_KEY_COORD_LEN);
	sys_memcpy_swap(net_buf_simple_add(&buf, BT_PUB_KEY_COORD_LEN), &key[BT_PUB_KEY_COORD_LEN],
			BT_PUB_KEY_COORD_LEN);

	LOG_DBG("Local Public Key: %s", bt_hex(buf.data + 1, BT_PUB_KEY_LEN));

	/* PublicKeyProvisioner */
	memcpy(bt_mesh_prov_link.conf_inputs.pub_key_provisioner, &buf.data[1], PDU_LEN_PUB_KEY);

	if (bt_mesh_prov_send(&buf, public_key_sent)) {
		LOG_ERR("Failed to send Public Key");
		return;
	}

	bt_mesh_prov_link.expect = PROV_PUB_KEY;
}

static void prov_dh_key_cb(const uint8_t dhkey[BT_DH_KEY_LEN])
{
	LOG_DBG("%p", dhkey);

	if (!dhkey) {
		LOG_ERR("DHKey generation failed");
		prov_fail(PROV_ERR_UNEXP_ERR);
		return;
	}

	sys_memcpy_swap(bt_mesh_prov_link.dhkey, dhkey, BT_DH_KEY_LEN);

	LOG_DBG("DHkey: %s", bt_hex(bt_mesh_prov_link.dhkey, BT_DH_KEY_LEN));

	if (atomic_test_bit(bt_mesh_prov_link.flags, WAIT_STRING) ||
	    atomic_test_bit(bt_mesh_prov_link.flags, WAIT_NUMBER) ||
	    atomic_test_bit(bt_mesh_prov_link.flags, NOTIFY_INPUT_COMPLETE)) {
		atomic_set_bit(bt_mesh_prov_link.flags, WAIT_CONFIRM);
		return;
	}

	send_confirm();
}

static void prov_dh_key_gen(void)
{
	uint8_t remote_pk_le[BT_PUB_KEY_LEN];
	const uint8_t *remote_pk;
	const uint8_t *local_pk;

	local_pk = bt_mesh_prov_link.conf_inputs.pub_key_provisioner;
	remote_pk = bt_mesh_prov_link.conf_inputs.pub_key_device;

	/* Copy remote key in little-endian for bt_dh_key_gen().
	 * X and Y halves are swapped independently. The bt_dh_key_gen()
	 * will also take care of validating the remote public key.
	 */
	sys_memcpy_swap(remote_pk_le, remote_pk, BT_PUB_KEY_COORD_LEN);
	sys_memcpy_swap(&remote_pk_le[BT_PUB_KEY_COORD_LEN], &remote_pk[BT_PUB_KEY_COORD_LEN],
			BT_PUB_KEY_COORD_LEN);

	if (!memcmp(local_pk, remote_pk, BT_PUB_KEY_LEN)) {
		LOG_ERR("Public keys are identical");
		prov_fail(PROV_ERR_NVAL_FMT);
		return;
	}

	if (bt_dh_key_gen(remote_pk_le, prov_dh_key_cb)) {
		LOG_ERR("Failed to generate DHKey");
		prov_fail(PROV_ERR_UNEXP_ERR);
	}

	if (atomic_test_bit(bt_mesh_prov_link.flags, NOTIFY_INPUT_COMPLETE)) {
		bt_mesh_prov_link.expect = PROV_INPUT_COMPLETE;
	}
}

static void prov_pub_key(const uint8_t *data)
{
	LOG_DBG("Remote Public Key: %s", bt_hex(data, BT_PUB_KEY_LEN));

	atomic_set_bit(bt_mesh_prov_link.flags, REMOTE_PUB_KEY);

	/* PublicKeyDevice */
	memcpy(bt_mesh_prov_link.conf_inputs.pub_key_device, data, BT_PUB_KEY_LEN);
	bt_mesh_prov_link.bearer->clear_tx();

	prov_dh_key_gen();
}

static void pub_key_ready(const uint8_t *pkey)
{
	if (!pkey) {
		LOG_WRN("Public key not available");
		return;
	}

	LOG_DBG("Local public key ready");

	if (atomic_test_and_clear_bit(bt_mesh_prov_link.flags, WAIT_PUB_KEY)) {
		send_pub_key();
	}
}

static void notify_input_complete(void)
{
	if (atomic_test_and_clear_bit(bt_mesh_prov_link.flags,
				      NOTIFY_INPUT_COMPLETE) &&
	    bt_mesh_prov->input_complete) {
		bt_mesh_prov->input_complete();
	}
}

static void prov_input_complete(const uint8_t *data)
{
	LOG_DBG("");

	notify_input_complete();

	if (atomic_test_and_clear_bit(bt_mesh_prov_link.flags, WAIT_CONFIRM)) {
		send_confirm();
	}
}

static void send_prov_data(void)
{
	PROV_BUF(pdu, PDU_LEN_DATA);
	struct bt_mesh_cdb_subnet *sub;
	uint8_t session_key[16];
	uint8_t nonce[13];
	int err;

	err = bt_mesh_session_key(bt_mesh_prov_link.dhkey,
				  bt_mesh_prov_link.prov_salt, session_key);
	if (err) {
		LOG_ERR("Unable to generate session key");
		prov_fail(PROV_ERR_UNEXP_ERR);
		return;
	}

	LOG_DBG("SessionKey: %s", bt_hex(session_key, 16));

	err = bt_mesh_prov_nonce(bt_mesh_prov_link.dhkey,
				 bt_mesh_prov_link.prov_salt, nonce);
	if (err) {
		LOG_ERR("Unable to generate session nonce");
		prov_fail(PROV_ERR_UNEXP_ERR);
		return;
	}

	LOG_DBG("Nonce: %s", bt_hex(nonce, 13));

	err = bt_mesh_dev_key(bt_mesh_prov_link.dhkey,
			      bt_mesh_prov_link.prov_salt, prov_device.node->dev_key);
	if (err) {
		LOG_ERR("Unable to generate device key");
		prov_fail(PROV_ERR_UNEXP_ERR);
		return;
	}

	LOG_DBG("DevKey: %s", bt_hex(prov_device.node->dev_key, 16));

	sub = bt_mesh_cdb_subnet_get(prov_device.node->net_idx);
	if (sub == NULL) {
		LOG_ERR("No subnet with net_idx %u", prov_device.node->net_idx);
		prov_fail(PROV_ERR_UNEXP_ERR);
		return;
	}

	bt_mesh_prov_buf_init(&pdu, PROV_DATA);
	net_buf_simple_add_mem(&pdu, sub->keys[SUBNET_KEY_TX_IDX(sub)].net_key, 16);
	net_buf_simple_add_be16(&pdu, prov_device.node->net_idx);
	net_buf_simple_add_u8(&pdu, bt_mesh_cdb_subnet_flags(sub));
	net_buf_simple_add_be32(&pdu, bt_mesh_cdb.iv_index);
	net_buf_simple_add_be16(&pdu, prov_device.node->addr);
	net_buf_simple_add(&pdu, 8); /* For MIC */

	LOG_DBG("net_idx %u, iv_index 0x%08x, addr 0x%04x", prov_device.node->net_idx,
		bt_mesh.iv_index, prov_device.node->addr);

	err = bt_mesh_prov_encrypt(session_key, nonce, &pdu.data[1],
				   &pdu.data[1]);
	if (err) {
		LOG_ERR("Unable to encrypt provisioning data");
		prov_fail(PROV_ERR_DECRYPT);
		return;
	}

	if (bt_mesh_prov_send(&pdu, NULL)) {
		LOG_ERR("Failed to send Provisioning Data");
		return;
	}

	bt_mesh_prov_link.expect = PROV_COMPLETE;
}

static void prov_complete(const uint8_t *data)
{
	struct bt_mesh_cdb_node *node = prov_device.node;

	LOG_DBG("key %s, net_idx %u, num_elem %u, addr 0x%04x", bt_hex(node->dev_key, 16),
		node->net_idx, node->num_elem, node->addr);

	if (IS_ENABLED(CONFIG_BT_SETTINGS)) {
		bt_mesh_cdb_node_store(node);
	}

	prov_device.node = NULL;
	prov_link_close(PROV_BEARER_LINK_STATUS_SUCCESS);

	if (bt_mesh_prov->node_added) {
		bt_mesh_prov->node_added(node->net_idx, node->uuid, node->addr,
					 node->num_elem);
	}
}

static void send_random(void)
{
	PROV_BUF(rnd, PDU_LEN_RANDOM);

	bt_mesh_prov_buf_init(&rnd, PROV_RANDOM);
	net_buf_simple_add_mem(&rnd, bt_mesh_prov_link.rand, 16);

	if (bt_mesh_prov_send(&rnd, NULL)) {
		LOG_ERR("Failed to send Provisioning Random");
		return;
	}

	bt_mesh_prov_link.expect = PROV_RANDOM;
}

static void prov_random(const uint8_t *data)
{
	uint8_t conf_verify[16];

	LOG_DBG("Remote Random: %s", bt_hex(data, 16));
	if (!memcmp(data, bt_mesh_prov_link.rand, 16)) {
		LOG_ERR("Random value is identical to ours, rejecting.");
		prov_fail(PROV_ERR_CFM_FAILED);
		return;
	}

	if (bt_mesh_prov_conf(bt_mesh_prov_link.conf_key,
			      data, bt_mesh_prov_link.auth, conf_verify)) {
		LOG_ERR("Unable to calculate confirmation verification");
		prov_fail(PROV_ERR_UNEXP_ERR);
		return;
	}

	if (memcmp(conf_verify, bt_mesh_prov_link.conf, 16)) {
		LOG_ERR("Invalid confirmation value");
		LOG_DBG("Received:   %s", bt_hex(bt_mesh_prov_link.conf, 16));
		LOG_DBG("Calculated: %s",  bt_hex(conf_verify, 16));
		prov_fail(PROV_ERR_CFM_FAILED);
		return;
	}

	if (bt_mesh_prov_salt(bt_mesh_prov_link.conf_salt,
			      bt_mesh_prov_link.rand, data, bt_mesh_prov_link.prov_salt)) {
		LOG_ERR("Failed to generate provisioning salt");
		prov_fail(PROV_ERR_UNEXP_ERR);
		return;
	}

	LOG_DBG("ProvisioningSalt: %s", bt_hex(bt_mesh_prov_link.prov_salt, 16));

	send_prov_data();
}

static void prov_confirm(const uint8_t *data)
{
	LOG_DBG("Remote Confirm: %s", bt_hex(data, 16));

	if (!memcmp(data, bt_mesh_prov_link.conf, 16)) {
		LOG_ERR("Confirm value is identical to ours, rejecting.");
		prov_fail(PROV_ERR_CFM_FAILED);
		return;
	}

	memcpy(bt_mesh_prov_link.conf, data, 16);

	send_random();
}

static void prov_failed(const uint8_t *data)
{
	LOG_WRN("Error: 0x%02x", data[0]);
	reset_state();
}

static void local_input_complete(void)
{
	if (atomic_test_and_clear_bit(bt_mesh_prov_link.flags, WAIT_CONFIRM)) {
		send_confirm();
	}
}

static void prov_link_closed(void)
{
	reset_state();
}

static void prov_link_opened(void)
{
	send_invite();
}

static const struct bt_mesh_prov_role role_provisioner = {
	.input_complete = local_input_complete,
	.link_opened = prov_link_opened,
	.link_closed = prov_link_closed,
	.error = prov_fail,
	.op = {
		[PROV_CAPABILITIES] = prov_capabilities,
		[PROV_PUB_KEY] = prov_pub_key,
		[PROV_INPUT_COMPLETE] = prov_input_complete,
		[PROV_CONFIRM] = prov_confirm,
		[PROV_RANDOM] = prov_random,
		[PROV_COMPLETE] = prov_complete,
		[PROV_FAILED] = prov_failed,
	},
};

static void prov_set_method(uint8_t method, uint8_t action, uint8_t size)
{
	bt_mesh_prov_link.oob_method = method;
	bt_mesh_prov_link.oob_action = action;
	bt_mesh_prov_link.oob_size = size;
}

int bt_mesh_auth_method_set_input(bt_mesh_input_action_t action, uint8_t size)
{
	if (!action || !size || size > PROV_IO_OOB_SIZE_MAX) {
		return -EINVAL;
	}

	prov_set_method(AUTH_METHOD_INPUT, find_msb_set(action) - 1, size);
	return 0;
}

int bt_mesh_auth_method_set_output(bt_mesh_output_action_t action, uint8_t size)
{
	if (!action || !size || size > PROV_IO_OOB_SIZE_MAX) {
		return -EINVAL;
	}

	prov_set_method(AUTH_METHOD_OUTPUT, find_msb_set(action) - 1, size);
	return 0;
}

int bt_mesh_auth_method_set_static(const uint8_t *static_val, uint8_t size)
{
	if (!size || !static_val || size > 16) {
		return -EINVAL;
	}

	prov_set_method(AUTH_METHOD_STATIC, 0, 0);

	memcpy(bt_mesh_prov_link.auth, static_val, size);
	memset(bt_mesh_prov_link.auth + size, 0, sizeof(bt_mesh_prov_link.auth) - size);

	return 0;
}

int bt_mesh_auth_method_set_none(void)
{
	prov_set_method(AUTH_METHOD_NO_OOB, 0, 0);
	return 0;
}

int bt_mesh_prov_remote_pub_key_set(const uint8_t public_key[BT_PUB_KEY_LEN])
{
	if (public_key == NULL) {
		return -EINVAL;
	}

	if (atomic_test_and_set_bit(bt_mesh_prov_link.flags, REMOTE_PUB_KEY)) {
		return -EALREADY;
	}

	memcpy(bt_mesh_prov_link.conf_inputs.pub_key_device, public_key, PDU_LEN_PUB_KEY);

	return 0;
}

static int bt_mesh_provisioner_open(const struct prov_bearer *bearer,
				    const uint8_t uuid[16],
				    uint16_t net_idx, uint16_t addr,
				    uint8_t attention_duration)
{
	int err;

	if (atomic_test_and_set_bit(bt_mesh_prov_link.flags, LINK_ACTIVE)) {
		return -EBUSY;
	}

	struct bt_uuid_128 uuid_repr = { .uuid = { BT_UUID_TYPE_128 } };

	memcpy(uuid_repr.val, uuid, 16);
	LOG_DBG("Provisioning %s", bt_uuid_str(&uuid_repr.uuid));

	atomic_set_bit(bt_mesh_prov_link.flags, PROVISIONER);
	memcpy(prov_device.uuid, uuid, 16);
	prov_device.addr = addr;
	prov_device.net_idx = net_idx;
	prov_device.attention_duration = attention_duration;
	bt_mesh_prov_link.bearer = bearer;
	bt_mesh_prov_link.role = &role_provisioner;

	err = bt_mesh_prov_link.bearer->link_open(prov_device.uuid, PROTOCOL_TIMEOUT,
						  bt_mesh_prov_bearer_cb_get(), NULL);
	if (err) {
		atomic_clear_bit(bt_mesh_prov_link.flags, LINK_ACTIVE);
	}

	return err;
}

#if defined(CONFIG_BT_MESH_PB_ADV)
int bt_mesh_pb_adv_open(const uint8_t uuid[16], uint16_t net_idx, uint16_t addr,
			uint8_t attention_duration)
{
	return bt_mesh_provisioner_open(&bt_mesh_pb_adv, uuid,
					net_idx, addr, attention_duration);
}
#endif

#if defined(CONFIG_BT_MESH_PB_GATT_CLIENT)
int bt_mesh_pb_gatt_open(const uint8_t uuid[16], uint16_t net_idx, uint16_t addr,
			 uint8_t attention_duration)
{
	return bt_mesh_provisioner_open(&bt_mesh_pb_gatt, uuid,
					net_idx, addr, attention_duration);
}
#endif
