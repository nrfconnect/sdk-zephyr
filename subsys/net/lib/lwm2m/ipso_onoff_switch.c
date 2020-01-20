/*
 * Copyright (c) 2019 Foundries.io
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Source material for IPSO On/Off Switch object (3342):
 * http://www.openmobilealliance.org/tech/profiles/lwm2m/3342.xml
 */

#define LOG_MODULE_NAME net_ipso_onoff_switch
#define LOG_LEVEL CONFIG_LWM2M_LOG_LEVEL

#include <logging/log.h>
LOG_MODULE_REGISTER(LOG_MODULE_NAME);

#include <stdint.h>
#include <init.h>

#include "lwm2m_object.h"
#include "lwm2m_engine.h"

#ifdef CONFIG_LWM2M_IPSO_ONOFF_SWITCH_TIMESTAMP
#define ADD_TIMESTAMPS 1
#else
#define ADD_TIMESTAMPS 0
#endif

/* resource IDs */
#define SWITCH_DIGITAL_STATE_ID		5500
#define SWITCH_DIGITAL_INPUT_COUNTER_ID	5501
#define SWITCH_ON_TIME_ID		5852
#define SWITCH_OFF_TIME_ID		5854
#define SWITCH_APPLICATION_TYPE_ID	5750
#if ADD_TIMESTAMPS
#define SWITCH_TIMESTAMP_ID		5518

#define SWITCH_MAX_ID			6
#else
#define SWITCH_MAX_ID			5
#endif

#define MAX_INSTANCE_COUNT	CONFIG_LWM2M_IPSO_ONOFF_SWITCH_INSTANCE_COUNT

/*
 * Calculate resource instances as follows:
 * start with SWITCH_MAX_ID
 */
#define RESOURCE_INSTANCE_COUNT        (SWITCH_MAX_ID)

/* resource state */
struct ipso_switch_data {
	u64_t trigger_offset;
	u64_t on_time_sec;
	u64_t off_time_sec;
	u64_t counter;
	u16_t obj_inst_id;
	bool last_state;
	bool state;
};

static struct ipso_switch_data switch_data[MAX_INSTANCE_COUNT];

static struct lwm2m_engine_obj onoff_switch;
static struct lwm2m_engine_obj_field fields[] = {
	OBJ_FIELD_DATA(SWITCH_DIGITAL_STATE_ID, R, BOOL),
	OBJ_FIELD_DATA(SWITCH_DIGITAL_INPUT_COUNTER_ID, R_OPT, U64),
	OBJ_FIELD_DATA(SWITCH_ON_TIME_ID, RW_OPT, U64),
	OBJ_FIELD_DATA(SWITCH_OFF_TIME_ID, RW_OPT, U64),
	OBJ_FIELD_DATA(SWITCH_APPLICATION_TYPE_ID, RW_OPT, STRING),
#if ADD_TIMESTAMPS
	OBJ_FIELD_DATA(SWITCH_TIMESTAMP_ID, RW_OPT, TIME),
#endif
};

static struct lwm2m_engine_obj_inst inst[MAX_INSTANCE_COUNT];
static struct lwm2m_engine_res res[MAX_INSTANCE_COUNT][SWITCH_MAX_ID];
static struct lwm2m_engine_res_inst
		res_inst[MAX_INSTANCE_COUNT][RESOURCE_INSTANCE_COUNT];

static int get_switch_index(u16_t obj_inst_id)
{
	int i, ret = -ENOENT;

	for (i = 0; i < ARRAY_SIZE(inst); i++) {
		if (!inst[i].obj || inst[i].obj_inst_id != obj_inst_id) {
			continue;
		}

		ret = i;
		break;
	}

	return ret;
}

static int state_post_write_cb(u16_t obj_inst_id,
			       u16_t res_id, u16_t res_inst_id,
			       u8_t *data, u16_t data_len,
			       bool last_block, size_t total_size)
{
	int i;

	i = get_switch_index(obj_inst_id);
	if (i < 0) {
		return i;
	}

	if (switch_data[i].state) {
		/* reset off time */
		switch_data[i].off_time_sec = 0U;
		if (!switch_data[i].last_state) {
			/* off to on transition */
			switch_data[i].counter++;
		}
	} else {
		/* reset on time */
		switch_data[i].on_time_sec = 0U;
	}

	switch_data[i].last_state = switch_data[i].state;
	switch_data[i].trigger_offset = k_uptime_get();
	return 0;
}

static void *on_time_read_cb(u16_t obj_inst_id,
			     u16_t res_id, u16_t res_inst_id,
			     size_t *data_len)
{
	int i = get_switch_index(obj_inst_id);

	if (i < 0) {
		return NULL;
	}

	if (switch_data[i].state) {
		switch_data[i].on_time_sec =
			(k_uptime_get() - switch_data[i].trigger_offset) / 1000;
	}

	*data_len = sizeof(switch_data[i].on_time_sec);
	return &switch_data[i].on_time_sec;
}

static void *off_time_read_cb(u16_t obj_inst_id,
			      u16_t res_id, u16_t res_inst_id,
			      size_t *data_len)
{
	int i = get_switch_index(obj_inst_id);

	if (i < 0) {
		return NULL;
	}

	if (!switch_data[i].state) {
		switch_data[i].off_time_sec =
			(k_uptime_get() - switch_data[i].trigger_offset) / 1000;
	}

	*data_len = sizeof(switch_data[i].off_time_sec);
	return &switch_data[i].off_time_sec;
}

static int time_post_write_cb(u16_t obj_inst_id,
			      u16_t res_id, u16_t res_inst_id,
			      u8_t *data, u16_t data_len,
			      bool last_block, size_t total_size)
{
	int i = get_switch_index(obj_inst_id);

	if (i < 0) {
		return i;
	}

	switch_data[i].trigger_offset = 0U;
	return 0;
}

static struct lwm2m_engine_obj_inst *switch_create(u16_t obj_inst_id)
{
	int index, avail = -1, i = 0, j = 0;

	/* Check that there is no other instance with this ID */
	for (index = 0; index < ARRAY_SIZE(inst); index++) {
		if (inst[index].obj && inst[index].obj_inst_id == obj_inst_id) {
			LOG_ERR("Can not create instance - "
				"already existing: %u", obj_inst_id);
			return NULL;
		}

		/* Save first available slot index */
		if (avail < 0 && !inst[index].obj) {
			avail = index;
		}
	}

	if (avail < 0) {
		LOG_ERR("Can not create instance - no more room: %u",
			obj_inst_id);
		return NULL;
	}

	/* Set default values */
	(void)memset(&switch_data[avail], 0, sizeof(switch_data[avail]));
	switch_data[avail].obj_inst_id = obj_inst_id;

	(void)memset(res[avail], 0,
		     sizeof(res[avail][0]) * ARRAY_SIZE(res[avail]));
	init_res_instance(res_inst[avail], ARRAY_SIZE(res_inst[avail]));

	/* initialize instance resource data */
	INIT_OBJ_RES(SWITCH_DIGITAL_STATE_ID, res[avail], i,
		     res_inst[avail], j, 1, true,
		     &switch_data[avail].state,
		     sizeof(switch_data[avail].state),
		     NULL, NULL, state_post_write_cb, NULL);
	INIT_OBJ_RES_DATA(SWITCH_DIGITAL_INPUT_COUNTER_ID, res[avail], i,
			  res_inst[avail], j,
			  &switch_data[avail].counter,
			  sizeof(switch_data[avail].counter));
	INIT_OBJ_RES_OPT(SWITCH_ON_TIME_ID, res[avail], i,
		     res_inst[avail], j, 1, true,
		     on_time_read_cb, NULL, time_post_write_cb, NULL);
	INIT_OBJ_RES_OPT(SWITCH_OFF_TIME_ID, res[avail], i,
		     res_inst[avail], j, 1, true,
		     off_time_read_cb, NULL, time_post_write_cb, NULL);
	INIT_OBJ_RES_OPTDATA(SWITCH_APPLICATION_TYPE_ID, res[avail], i,
			     res_inst[avail], j);
#if ADD_TIMESTAMPS
	INIT_OBJ_RES_OPTDATA(SWITCH_TIMESTAMP_ID, res[avail], i,
			     res_inst[avail], j);
#endif

	inst[avail].resources = res[avail];
	inst[avail].resource_count = i;

	LOG_DBG("Create IPSO On/Off Switch instance: %d", obj_inst_id);

	return &inst[avail];
}

static int ipso_switch_init(struct device *dev)
{
	onoff_switch.obj_id = IPSO_OBJECT_ONOFF_SWITCH_ID;
	onoff_switch.fields = fields;
	onoff_switch.field_count = ARRAY_SIZE(fields);
	onoff_switch.max_instance_count = ARRAY_SIZE(inst);
	onoff_switch.create_cb = switch_create;
	lwm2m_register_obj(&onoff_switch);

	return 0;
}

SYS_INIT(ipso_switch_init, APPLICATION, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT);
