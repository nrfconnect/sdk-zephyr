/*
 * Copyright (c) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <string.h>
#include <sys/printk.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/mesh.h>
#include <bluetooth/hci.h>

#include <drivers/sensor.h>

#include "mesh.h"
#include "board.h"

#define MOD_LF            0x0000
#define OP_HELLO          0xbb
#define OP_HEARTBEAT      0xbc
#define OP_BADUSER        0xbd
#define OP_VND_HELLO      BT_MESH_MODEL_OP_3(OP_HELLO, BT_COMP_ID_LF)
#define OP_VND_HEARTBEAT  BT_MESH_MODEL_OP_3(OP_HEARTBEAT, BT_COMP_ID_LF)
#define OP_VND_BADUSER    BT_MESH_MODEL_OP_3(OP_BADUSER, BT_COMP_ID_LF)

#define IV_INDEX          0
#define DEFAULT_TTL       31
#define GROUP_ADDR        0xc123
#define NET_IDX           0x000
#define APP_IDX           0x000
#define FLAGS             0

/* Maximum characters in "hello" message */
#define HELLO_MAX         8

#define MAX_SENS_STATUS_LEN 8

#define SENS_PROP_ID_PRESENT_DEVICE_TEMP 0x0054

enum {
	SENSOR_HDR_A = 0,
	SENSOR_HDR_B = 1,
};

struct sensor_hdr_a {
	u16_t prop_id:11;
	u16_t length:4;
	u16_t format:1;
} __packed;

struct sensor_hdr_b {
	u8_t length:7;
	u8_t format:1;
	u16_t prop_id;
} __packed;

static struct k_work hello_work;
static struct k_work baduser_work;
static struct k_work mesh_start_work;

/* Definitions of models user data (Start) */
static struct led_onoff_state led_onoff_state[] = {
	{ .dev_id = DEV_IDX_LED0 },
};

static void heartbeat(u8_t hops, u16_t feat)
{
	board_show_text("Heartbeat Received", false, K_SECONDS(2));
}

static struct bt_mesh_cfg_srv cfg_srv = {
	.relay = BT_MESH_RELAY_ENABLED,
	.beacon = BT_MESH_BEACON_DISABLED,
	.default_ttl = DEFAULT_TTL,

	/* 3 transmissions with 20ms interval */
	.net_transmit = BT_MESH_TRANSMIT(2, 20),
	.relay_retransmit = BT_MESH_TRANSMIT(3, 20),

	.hb_sub.func = heartbeat,
};

static struct bt_mesh_cfg_cli cfg_cli = {
};

static void attention_on(struct bt_mesh_model *model)
{
	board_show_text("Attention!", false, K_SECONDS(2));
}

static void attention_off(struct bt_mesh_model *model)
{
	board_refresh_display();
}

static const struct bt_mesh_health_srv_cb health_srv_cb = {
	.attn_on = attention_on,
	.attn_off = attention_off,
};

static struct bt_mesh_health_srv health_srv = {
	.cb = &health_srv_cb,
};

/* Generic OnOff Server message handlers */
static void gen_onoff_get(struct bt_mesh_model *model,
			  struct bt_mesh_msg_ctx *ctx,
			  struct net_buf_simple *buf)
{
	NET_BUF_SIMPLE_DEFINE(msg, 2 + 1 + 4);
	struct led_onoff_state *state = model->user_data;

	printk("addr 0x%04x onoff 0x%02x\n",
	       bt_mesh_model_elem(model)->addr, state->current);
	bt_mesh_model_msg_init(&msg, BT_MESH_MODEL_OP_GEN_ONOFF_STATUS);
	net_buf_simple_add_u8(&msg, state->current);

	if (bt_mesh_model_send(model, ctx, &msg, NULL, NULL)) {
		printk("Unable to send On Off Status response\n");
	}
}

static void gen_onoff_set_unack(struct bt_mesh_model *model,
				struct bt_mesh_msg_ctx *ctx,
				struct net_buf_simple *buf)
{
	struct net_buf_simple *msg = model->pub->msg;
	struct led_onoff_state *state = model->user_data;
	int err;
	u8_t tid, onoff;
	s64_t now;

	onoff = net_buf_simple_pull_u8(buf);
	tid = net_buf_simple_pull_u8(buf);

	if (onoff > STATE_ON) {
		printk("Wrong state received\n");

		return;
	}

	now = k_uptime_get();
	if (state->last_tid == tid && state->last_tx_addr == ctx->addr &&
	    (now - state->last_msg_timestamp <= K_SECONDS(6))) {
		printk("Already received message\n");
	}

	state->current = onoff;
	state->last_tid = tid;
	state->last_tx_addr = ctx->addr;
	state->last_msg_timestamp = now;

	printk("addr 0x%02x state 0x%02x\n",
	       bt_mesh_model_elem(model)->addr, state->current);

	/* Pin set low turns on LED's on the reel board */
	if (set_led_state(state->dev_id, onoff)) {
		printk("Failed to set led state\n");

		return;
	}

	/*
	 * If a server has a publish address, it is required to
	 * publish status on a state change
	 *
	 * See Mesh Profile Specification 3.7.6.1.2
	 *
	 * Only publish if there is an assigned address
	 */

	if (state->previous != state->current &&
	    model->pub->addr != BT_MESH_ADDR_UNASSIGNED) {
		printk("publish last 0x%02x cur 0x%02x\n",
		       state->previous, state->current);
		state->previous = state->current;
		bt_mesh_model_msg_init(msg,
				       BT_MESH_MODEL_OP_GEN_ONOFF_STATUS);
		net_buf_simple_add_u8(msg, state->current);
		err = bt_mesh_model_publish(model);
		if (err) {
			printk("bt_mesh_model_publish err %d\n", err);
		}
	}
}

static void gen_onoff_set(struct bt_mesh_model *model,
			  struct bt_mesh_msg_ctx *ctx,
			  struct net_buf_simple *buf)
{
	gen_onoff_set_unack(model, ctx, buf);
	gen_onoff_get(model, ctx, buf);
}

static void sensor_desc_get(struct bt_mesh_model *model,
			    struct bt_mesh_msg_ctx *ctx,
			    struct net_buf_simple *buf)
{
	/* TODO */
}

static void sens_temperature_celsius_fill(struct net_buf_simple *msg)
{
	struct sensor_hdr_a hdr;
	/* TODO Get only temperature from sensor */
	struct sensor_value val[2];
	s16_t temp_degrees;

	hdr.format = SENSOR_HDR_A;
	hdr.length = sizeof(temp_degrees);
	hdr.prop_id = SENS_PROP_ID_PRESENT_DEVICE_TEMP;

	get_hdc1010_val(val);
	temp_degrees = sensor_value_to_double(&val[0]) * 100;

	net_buf_simple_add_mem(msg, &hdr, sizeof(hdr));
	net_buf_simple_add_le16(msg, temp_degrees);
}

static void sens_unknown_fill(u16_t id, struct net_buf_simple *msg)
{
	struct sensor_hdr_b hdr;

	/*
	 * When the message is a response to a Sensor Get message that
	 * identifies a sensor property that does not exist on the element, the
	 * Length field shall represent the value of zero and the Raw Value for
	 * that property shall be omitted. (Mesh model spec 1.0, 4.2.14).
	 *
	 * The length zero is represented using the format B and the special
	 * value 0x7F.
	 */
	hdr.format = SENSOR_HDR_B;
	hdr.length = 0x7FU;
	hdr.prop_id = id;

	net_buf_simple_add_mem(msg, &hdr, sizeof(hdr));
}

static void sensor_create_status(u16_t id, struct net_buf_simple *msg)
{
	bt_mesh_model_msg_init(msg, BT_MESH_MODEL_OP_SENS_STATUS);

	switch (id) {
	case SENS_PROP_ID_PRESENT_DEVICE_TEMP:
		sens_temperature_celsius_fill(msg);
		break;
	default:
		sens_unknown_fill(id, msg);
		break;
	}
}

static void sensor_get(struct bt_mesh_model *model,
		       struct bt_mesh_msg_ctx *ctx,
		       struct net_buf_simple *buf)
{
	NET_BUF_SIMPLE_DEFINE(msg, 1 + MAX_SENS_STATUS_LEN + 4);
	u16_t sensor_id;

	sensor_id = net_buf_simple_pull_le16(buf);
	sensor_create_status(sensor_id, &msg);

	if (bt_mesh_model_send(model, ctx, &msg, NULL, NULL)) {
		printk("Unable to send Sensor get status response\n");
	}
}

static void sensor_col_get(struct bt_mesh_model *model,
			   struct bt_mesh_msg_ctx *ctx,
			   struct net_buf_simple *buf)
{
	/* TODO */
}

static void sensor_series_get(struct bt_mesh_model *model,
			      struct bt_mesh_msg_ctx *ctx,
			      struct net_buf_simple *buf)
{
	/* TODO */
}

/* Definitions of models publication context (Start) */
BT_MESH_HEALTH_PUB_DEFINE(health_pub, 0);
BT_MESH_MODEL_PUB_DEFINE(gen_onoff_srv_pub_root, NULL, 2 + 3);

/* Mapping of message handlers for Generic OnOff Server (0x1000) */
static const struct bt_mesh_model_op gen_onoff_srv_op[] = {
	{ BT_MESH_MODEL_OP_GEN_ONOFF_GET, 0, gen_onoff_get },
	{ BT_MESH_MODEL_OP_GEN_ONOFF_SET, 2, gen_onoff_set },
	{ BT_MESH_MODEL_OP_GEN_ONOFF_SET_UNACK, 2, gen_onoff_set_unack },
	BT_MESH_MODEL_OP_END,
};

/* Mapping of message handlers for Sensor Server (0x1100) */
static const struct bt_mesh_model_op sensor_srv_op[] = {
	{ BT_MESH_MODEL_OP_SENS_DESC_GET, 0, sensor_desc_get },
	{ BT_MESH_MODEL_OP_SENS_GET, 0, sensor_get },
	{ BT_MESH_MODEL_OP_SENS_COL_GET, 2, sensor_col_get },
	{ BT_MESH_MODEL_OP_SENS_SERIES_GET, 2, sensor_series_get },
};

static struct bt_mesh_model root_models[] = {
	BT_MESH_MODEL_CFG_SRV(&cfg_srv),
	BT_MESH_MODEL_CFG_CLI(&cfg_cli),
	BT_MESH_MODEL_HEALTH_SRV(&health_srv, &health_pub),
	BT_MESH_MODEL(BT_MESH_MODEL_ID_GEN_ONOFF_SRV,
		      gen_onoff_srv_op, &gen_onoff_srv_pub_root,
		      &led_onoff_state[0]),
	BT_MESH_MODEL(BT_MESH_MODEL_ID_SENSOR_SRV,
		      sensor_srv_op, NULL, NULL),
};

static void vnd_hello(struct bt_mesh_model *model,
		      struct bt_mesh_msg_ctx *ctx,
		      struct net_buf_simple *buf)
{
	char str[32];
	size_t len;

	printk("Hello message from 0x%04x\n", ctx->addr);

	if (ctx->addr == bt_mesh_model_elem(model)->addr) {
		printk("Ignoring message from self\n");
		return;
	}

	len = MIN(buf->len, HELLO_MAX);
	memcpy(str, buf->data, len);
	str[len] = '\0';

	board_add_hello(ctx->addr, str);

	strcat(str, " says hi!");
	board_show_text(str, false, K_SECONDS(3));

	board_blink_leds();
}

static void vnd_baduser(struct bt_mesh_model *model,
			struct bt_mesh_msg_ctx *ctx,
			struct net_buf_simple *buf)
{
	char str[32];
	size_t len;

	printk("\"Bad user\" message from 0x%04x\n", ctx->addr);

	if (ctx->addr == bt_mesh_model_elem(model)->addr) {
		printk("Ignoring message from self\n");
		return;
	}

	len = MIN(buf->len, HELLO_MAX);
	memcpy(str, buf->data, len);
	str[len] = '\0';

	strcat(str, " is misbehaving!");
	board_show_text(str, false, K_SECONDS(3));

	board_blink_leds();
}

static void vnd_heartbeat(struct bt_mesh_model *model,
			  struct bt_mesh_msg_ctx *ctx,
			  struct net_buf_simple *buf)
{
	u8_t init_ttl, hops;

	/* Ignore messages from self */
	if (ctx->addr == bt_mesh_model_elem(model)->addr) {
		return;
	}

	init_ttl = net_buf_simple_pull_u8(buf);
	hops = init_ttl - ctx->recv_ttl + 1;

	printk("Heartbeat from 0x%04x over %u hop%s\n", ctx->addr,
	       hops, hops == 1U ? "" : "s");

	board_add_heartbeat(ctx->addr, hops);
}

static const struct bt_mesh_model_op vnd_ops[] = {
	{ OP_VND_HELLO, 1, vnd_hello },
	{ OP_VND_HEARTBEAT, 1, vnd_heartbeat },
	{ OP_VND_BADUSER, 1, vnd_baduser },
	BT_MESH_MODEL_OP_END,
};

static int pub_update(struct bt_mesh_model *mod)
{
	struct net_buf_simple *msg = mod->pub->msg;

	printk("Preparing to send heartbeat\n");

	bt_mesh_model_msg_init(msg, OP_VND_HEARTBEAT);
	net_buf_simple_add_u8(msg, DEFAULT_TTL);

	return 0;
}

BT_MESH_MODEL_PUB_DEFINE(vnd_pub, pub_update, 3 + 1);

static struct bt_mesh_model vnd_models[] = {
	BT_MESH_MODEL_VND(BT_COMP_ID_LF, MOD_LF, vnd_ops, &vnd_pub, NULL),
};

static struct bt_mesh_elem elements[] = {
	BT_MESH_ELEM(0, root_models, vnd_models),
};

static const struct bt_mesh_comp comp = {
	.cid = BT_COMP_ID_LF,
	.elem = elements,
	.elem_count = ARRAY_SIZE(elements),
};

static size_t first_name_len(const char *name)
{
	size_t len;

	for (len = 0; *name; name++, len++) {
		switch (*name) {
		case ' ':
		case ',':
		case '\n':
			return len;
		}
	}

	return len;
}

static void send_hello(struct k_work *work)
{
	NET_BUF_SIMPLE_DEFINE(msg, 3 + HELLO_MAX + 4);
	struct bt_mesh_msg_ctx ctx = {
		.net_idx = NET_IDX,
		.app_idx = APP_IDX,
		.addr = GROUP_ADDR,
		.send_ttl = DEFAULT_TTL,
	};
	const char *name = bt_get_name();

	bt_mesh_model_msg_init(&msg, OP_VND_HELLO);
	net_buf_simple_add_mem(&msg, name,
			       MIN(HELLO_MAX, first_name_len(name)));

	if (bt_mesh_model_send(&vnd_models[0], &ctx, &msg, NULL, NULL) == 0) {
		board_show_text("Saying \"hi!\" to everyone", false,
				K_SECONDS(2));
	} else {
		board_show_text("Sending Failed!", false, K_SECONDS(2));
	}
}

void mesh_send_hello(void)
{
	k_work_submit(&hello_work);
}

static void send_baduser(struct k_work *work)
{
	NET_BUF_SIMPLE_DEFINE(msg, 3 + HELLO_MAX + 4);
	struct bt_mesh_msg_ctx ctx = {
		.net_idx = NET_IDX,
		.app_idx = APP_IDX,
		.addr = GROUP_ADDR,
		.send_ttl = DEFAULT_TTL,
	};
	const char *name = bt_get_name();

	bt_mesh_model_msg_init(&msg, OP_VND_BADUSER);
	net_buf_simple_add_mem(&msg, name,
			       MIN(HELLO_MAX, first_name_len(name)));

	if (bt_mesh_model_send(&vnd_models[0], &ctx, &msg, NULL, NULL) == 0) {
		board_show_text("Bad user!", false, K_SECONDS(2));
	} else {
		board_show_text("Sending Failed!", false, K_SECONDS(2));
	}
}

void mesh_send_baduser(void)
{
	k_work_submit(&baduser_work);
}

static int provision_and_configure(void)
{
	static const u8_t net_key[16] = {
		0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc,
		0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc,
	};
	static const u8_t app_key[16] = {
		0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
		0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
	};
	struct bt_mesh_cfg_mod_pub pub = {
		.addr = GROUP_ADDR,
		.app_idx = APP_IDX,
		.ttl = DEFAULT_TTL,
		.period = BT_MESH_PUB_PERIOD_SEC(10),
	};
	u8_t dev_key[16];
	u16_t addr;
	int err;

	err = bt_rand(dev_key, sizeof(dev_key));
	if (err) {
		return err;
	}

	do {
		err = bt_rand(&addr, sizeof(addr));
		if (err) {
			return err;
		}
	} while (!addr);

	/* Make sure it's a unicast address (highest bit unset) */
	addr &= ~0x8000;

	err = bt_mesh_provision(net_key, NET_IDX, FLAGS, IV_INDEX, addr,
				dev_key);
	if (err) {
		return err;
	}

	printk("Configuring...\n");

	/* Add Application Key */
	bt_mesh_cfg_app_key_add(NET_IDX, addr, NET_IDX, APP_IDX, app_key, NULL);

	/* Bind to vendor model */
	bt_mesh_cfg_mod_app_bind_vnd(NET_IDX, addr, addr, APP_IDX,
				     MOD_LF, BT_COMP_ID_LF, NULL);

	bt_mesh_cfg_mod_app_bind(NET_IDX, addr, addr, APP_IDX,
				 BT_MESH_MODEL_ID_GEN_ONOFF_SRV, NULL);

	bt_mesh_cfg_mod_app_bind(NET_IDX, addr, addr, APP_IDX,
				 BT_MESH_MODEL_ID_SENSOR_SRV, NULL);

	/* Bind to Health model */
	bt_mesh_cfg_mod_app_bind(NET_IDX, addr, addr, APP_IDX,
				 BT_MESH_MODEL_ID_HEALTH_SRV, NULL);

	/* Add model subscription */
	bt_mesh_cfg_mod_sub_add_vnd(NET_IDX, addr, addr, GROUP_ADDR,
				    MOD_LF, BT_COMP_ID_LF, NULL);

	bt_mesh_cfg_mod_pub_set_vnd(NET_IDX, addr, addr, MOD_LF, BT_COMP_ID_LF,
				    &pub, NULL);

	printk("Configuration complete\n");

	return addr;
}

static void start_mesh(struct k_work *work)
{
	int err;

	err = provision_and_configure();
	if (err < 0) {
		board_show_text("Starting Mesh Failed", false,
				K_SECONDS(2));
	} else {
		char buf[32];

		snprintk(buf, sizeof(buf),
			 "Mesh Started\nAddr: 0x%04x", err);
		board_show_text(buf, false, K_SECONDS(4));
	}
}

void mesh_start(void)
{
	k_work_submit(&mesh_start_work);
}

bool mesh_is_initialized(void)
{
	return elements[0].addr != BT_MESH_ADDR_UNASSIGNED;
}

u16_t mesh_get_addr(void)
{
	return elements[0].addr;
}

int mesh_init(void)
{
	static const u8_t dev_uuid[16] = { 0xc0, 0xff, 0xee };
	static const struct bt_mesh_prov prov = {
		.uuid = dev_uuid,
	};

	k_work_init(&hello_work, send_hello);
	k_work_init(&baduser_work, send_baduser);
	k_work_init(&mesh_start_work, start_mesh);

	return bt_mesh_init(&prov, &comp);
}
