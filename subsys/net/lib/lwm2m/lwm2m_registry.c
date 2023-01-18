/*
 * Copyright (c) 2017 Linaro Limited
 * Copyright (c) 2018-2019 Foundries.io
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Uses some original concepts by:
 *         Joakim Eriksson <joakime@sics.se>
 *         Niclas Finne <nfi@sics.se>
 *         Joel Hoglund <joel@sics.se>
 */

#define LOG_MODULE_NAME net_lwm2m_registry
#define LOG_LEVEL	CONFIG_LWM2M_LOG_LEVEL

#include <zephyr/logging/log.h>
#include <zephyr/sys/ring_buffer.h>
LOG_MODULE_REGISTER(LOG_MODULE_NAME);

#include "lwm2m_engine.h"
#include "lwm2m_object.h"
#include "lwm2m_obj_access_control.h"
#include "lwm2m_util.h"

#include <ctype.h>
#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <zephyr/types.h>

#include <fcntl.h>
#ifdef CONFIG_LWM2M_RD_CLIENT_SUPPORT
#include "lwm2m_rd_client.h"
#endif

#define BINDING_OPT_MAX_LEN 3 /* "UQ" */
#define QUEUE_OPT_MAX_LEN   2 /* "Q" */

/* Thread safety */
static K_MUTEX_DEFINE(registry_lock);

void lwm2m_registry_lock(void)
{
	(void)k_mutex_lock(&registry_lock, K_FOREVER);
}

void lwm2m_registry_unlock(void)
{
	(void)k_mutex_unlock(&registry_lock);
}
/* Resources */
static sys_slist_t engine_obj_list;
static sys_slist_t engine_obj_inst_list;

/* Resource wrappers */
sys_slist_t *lwm2m_engine_obj_list(void) { return &engine_obj_list; }

sys_slist_t *lwm2m_engine_obj_inst_list(void) { return &engine_obj_inst_list; }

#if defined(CONFIG_LWM2M_RESOURCE_DATA_CACHE_SUPPORT)
static void lwm2m_engine_cache_write(struct lwm2m_engine_obj_field *obj_field, const char *pathstr,
				     const void *value, uint16_t len);
#endif
/* Engine object */

void lwm2m_register_obj(struct lwm2m_engine_obj *obj)
{
	k_mutex_lock(&registry_lock, K_FOREVER);
#if defined(CONFIG_LWM2M_ACCESS_CONTROL_ENABLE)
	/* If bootstrap, then bootstrap server should create the ac obj instances */
#if !IS_ENABLED(CONFIG_LWM2M_RD_CLIENT_SUPPORT_BOOTSTRAP)
	int server_obj_inst_id = lwm2m_server_short_id_to_inst(CONFIG_LWM2M_SERVER_DEFAULT_SSID);

	access_control_add_obj(obj->obj_id, server_obj_inst_id);
#endif /* CONFIG_LWM2M_RD_CLIENT_SUPPORT_BOOTSTRAP */
#endif /* CONFIG_LWM2M_ACCESS_CONTROL_ENABLE */
	sys_slist_append(&engine_obj_list, &obj->node);
	k_mutex_unlock(&registry_lock);
}

void lwm2m_unregister_obj(struct lwm2m_engine_obj *obj)
{
	k_mutex_lock(&registry_lock, K_FOREVER);
#if defined(CONFIG_LWM2M_ACCESS_CONTROL_ENABLE)
	access_control_remove_obj(obj->obj_id);
#endif
	engine_remove_observer_by_id(obj->obj_id, -1);
	sys_slist_find_and_remove(&engine_obj_list, &obj->node);
	k_mutex_unlock(&registry_lock);
}

struct lwm2m_engine_obj *get_engine_obj(int obj_id)
{
	struct lwm2m_engine_obj *obj;

	SYS_SLIST_FOR_EACH_CONTAINER(&engine_obj_list, obj, node) {
		if (obj->obj_id == obj_id) {
			return obj;
		}
	}

	return NULL;
}

struct lwm2m_engine_obj_field *lwm2m_get_engine_obj_field(struct lwm2m_engine_obj *obj, int res_id)
{
	int i;

	if (obj && obj->fields && obj->field_count > 0) {
		for (i = 0; i < obj->field_count; i++) {
			if (obj->fields[i].res_id == res_id) {
				return &obj->fields[i];
			}
		}
	}

	return NULL;
}

struct lwm2m_engine_obj *lwm2m_engine_get_obj(const struct lwm2m_obj_path *path)
{
	if (path->level < LWM2M_PATH_LEVEL_OBJECT) {
		return NULL;
	}

	return get_engine_obj(path->obj_id);
}
/* Engine object instance */

static void engine_register_obj_inst(struct lwm2m_engine_obj_inst *obj_inst)
{
#if defined(CONFIG_LWM2M_ACCESS_CONTROL_ENABLE)
	/* If bootstrap, then bootstrap server should create the ac obj instances */
#if !IS_ENABLED(CONFIG_LWM2M_RD_CLIENT_SUPPORT_BOOTSTRAP)
	int server_obj_inst_id = lwm2m_server_short_id_to_inst(CONFIG_LWM2M_SERVER_DEFAULT_SSID);

	access_control_add(obj_inst->obj->obj_id, obj_inst->obj_inst_id, server_obj_inst_id);
#endif /* CONFIG_LWM2M_RD_CLIENT_SUPPORT_BOOTSTRAP */
#endif /* CONFIG_LWM2M_ACCESS_CONTROL_ENABLE */
	sys_slist_append(&engine_obj_inst_list, &obj_inst->node);
}

static void engine_unregister_obj_inst(struct lwm2m_engine_obj_inst *obj_inst)
{
#if defined(CONFIG_LWM2M_ACCESS_CONTROL_ENABLE)
	access_control_remove(obj_inst->obj->obj_id, obj_inst->obj_inst_id);
#endif
	engine_remove_observer_by_id(obj_inst->obj->obj_id, obj_inst->obj_inst_id);
	sys_slist_find_and_remove(&engine_obj_inst_list, &obj_inst->node);
}

struct lwm2m_engine_obj_inst *get_engine_obj_inst(int obj_id, int obj_inst_id)
{
	struct lwm2m_engine_obj_inst *obj_inst;

	SYS_SLIST_FOR_EACH_CONTAINER(&engine_obj_inst_list, obj_inst, node) {
		if (obj_inst->obj->obj_id == obj_id && obj_inst->obj_inst_id == obj_inst_id) {
			return obj_inst;
		}
	}

	return NULL;
}

struct lwm2m_engine_obj_inst *next_engine_obj_inst(int obj_id, int obj_inst_id)
{
	struct lwm2m_engine_obj_inst *obj_inst, *next = NULL;

	SYS_SLIST_FOR_EACH_CONTAINER(&engine_obj_inst_list, obj_inst, node) {
		if (obj_inst->obj->obj_id == obj_id && obj_inst->obj_inst_id > obj_inst_id &&
		    (!next || next->obj_inst_id > obj_inst->obj_inst_id)) {
			next = obj_inst;
		}
	}

	return next;
}

int lwm2m_create_obj_inst(uint16_t obj_id, uint16_t obj_inst_id,
			  struct lwm2m_engine_obj_inst **obj_inst)
{
	k_mutex_lock(&registry_lock, K_FOREVER);
	struct lwm2m_engine_obj *obj;
	int ret;

	*obj_inst = NULL;
	obj = get_engine_obj(obj_id);
	if (!obj) {
		LOG_ERR("unable to find obj: %u", obj_id);
		k_mutex_unlock(&registry_lock);
		return -ENOENT;
	}

	if (!obj->create_cb) {
		LOG_ERR("obj %u has no create_cb", obj_id);
		k_mutex_unlock(&registry_lock);
		return -EINVAL;
	}

	if (obj->instance_count + 1 > obj->max_instance_count) {
		LOG_ERR("no more instances available for obj %u", obj_id);
		k_mutex_unlock(&registry_lock);
		return -ENOMEM;
	}

	*obj_inst = obj->create_cb(obj_inst_id);
	if (!*obj_inst) {
		LOG_ERR("unable to create obj %u instance %u", obj_id, obj_inst_id);
		/*
		 * Already checked for instance count total.
		 * This can only be an error if the object instance exists.
		 */
		k_mutex_unlock(&registry_lock);
		return -EEXIST;
	}

	obj->instance_count++;
	(*obj_inst)->obj = obj;
	(*obj_inst)->obj_inst_id = obj_inst_id;
	engine_register_obj_inst(*obj_inst);

	if (obj->user_create_cb) {
		ret = obj->user_create_cb(obj_inst_id);
		if (ret < 0) {
			LOG_ERR("Error in user obj create %u/%u: %d", obj_id, obj_inst_id, ret);
			k_mutex_unlock(&registry_lock);
			lwm2m_delete_obj_inst(obj_id, obj_inst_id);
			return ret;
		}
	}
	k_mutex_unlock(&registry_lock);
	return 0;
}

int lwm2m_delete_obj_inst(uint16_t obj_id, uint16_t obj_inst_id)
{
	k_mutex_lock(&registry_lock, K_FOREVER);
	int i, ret = 0;
	struct lwm2m_engine_obj *obj;
	struct lwm2m_engine_obj_inst *obj_inst;

	obj = get_engine_obj(obj_id);
	if (!obj) {
		k_mutex_unlock(&registry_lock);
		return -ENOENT;
	}

	obj_inst = get_engine_obj_inst(obj_id, obj_inst_id);
	if (!obj_inst) {
		k_mutex_unlock(&registry_lock);
		return -ENOENT;
	}

	if (obj->user_delete_cb) {
		ret = obj->user_delete_cb(obj_inst_id);
		if (ret < 0) {
			LOG_ERR("Error in user obj delete %u/%u: %d", obj_id, obj_inst_id, ret);
			/* don't return error */
		}
	}

	engine_unregister_obj_inst(obj_inst);
	obj->instance_count--;

	if (obj->delete_cb) {
		ret = obj->delete_cb(obj_inst_id);
	}

	/* reset obj_inst and res_inst data structure */
	for (i = 0; i < obj_inst->resource_count; i++) {
		clear_attrs(&obj_inst->resources[i]);
		(void)memset(obj_inst->resources + i, 0, sizeof(struct lwm2m_engine_res));
	}

	clear_attrs(obj_inst);
	(void)memset(obj_inst, 0, sizeof(struct lwm2m_engine_obj_inst));
	k_mutex_unlock(&registry_lock);
	return ret;
}

int lwm2m_engine_create_obj_inst(const char *pathstr)
{
	struct lwm2m_obj_path path;
	struct lwm2m_engine_obj_inst *obj_inst;
	int ret = 0;

	LOG_DBG("path:%s", pathstr);

	/* translate path -> path_obj */
	ret = lwm2m_string_to_path(pathstr, &path, '/');
	if (ret < 0) {
		return ret;
	}

	if (path.level != 2U) {
		LOG_ERR("path must have 2 parts");
		return -EINVAL;
	}

	ret = lwm2m_create_obj_inst(path.obj_id, path.obj_inst_id, &obj_inst);
	if (ret < 0) {
		return ret;
	}

#if defined(CONFIG_LWM2M_RD_CLIENT_SUPPORT)
	engine_trigger_update(true);
#endif

	return 0;
}

int lwm2m_engine_delete_obj_inst(const char *pathstr)
{
	struct lwm2m_obj_path path;
	int ret = 0;

	LOG_DBG("path: %s", pathstr);

	/* translate path -> path_obj */
	ret = lwm2m_string_to_path(pathstr, &path, '/');
	if (ret < 0) {
		return ret;
	}

	if (path.level != 2U) {
		LOG_ERR("path must have 2 parts");
		return -EINVAL;
	}

	ret = lwm2m_delete_obj_inst(path.obj_id, path.obj_inst_id);
	if (ret < 0) {
		return ret;
	}

#if defined(CONFIG_LWM2M_RD_CLIENT_SUPPORT)
	engine_trigger_update(true);
#endif

	return 0;
}

struct lwm2m_engine_obj_inst *lwm2m_engine_get_obj_inst(const struct lwm2m_obj_path *path)
{
	if (path->level < LWM2M_PATH_LEVEL_OBJECT_INST) {
		return NULL;
	}

	return get_engine_obj_inst(path->obj_id, path->obj_inst_id);
}

int path_to_objs(const struct lwm2m_obj_path *path, struct lwm2m_engine_obj_inst **obj_inst,
		 struct lwm2m_engine_obj_field **obj_field, struct lwm2m_engine_res **res,
		 struct lwm2m_engine_res_inst **res_inst)
{
	struct lwm2m_engine_obj_inst *oi;
	struct lwm2m_engine_obj_field *of;
	struct lwm2m_engine_res *r = NULL;
	struct lwm2m_engine_res_inst *ri = NULL;
	int i;

	if (!path) {
		return -EINVAL;
	}

	oi = get_engine_obj_inst(path->obj_id, path->obj_inst_id);
	if (!oi) {
		LOG_ERR("obj instance %d/%d not found", path->obj_id, path->obj_inst_id);
		return -ENOENT;
	}

	if (!oi->resources || oi->resource_count == 0U) {
		LOG_ERR("obj instance has no resources");
		return -EINVAL;
	}

	of = lwm2m_get_engine_obj_field(oi->obj, path->res_id);
	if (!of) {
		LOG_ERR("obj field %d not found", path->res_id);
		return -ENOENT;
	}

	for (i = 0; i < oi->resource_count; i++) {
		if (oi->resources[i].res_id == path->res_id) {
			r = &oi->resources[i];
			break;
		}
	}

	if (!r) {
		if (LWM2M_HAS_PERM(of, BIT(LWM2M_FLAG_OPTIONAL))) {
			LOG_DBG("resource %d not found", path->res_id);
		} else {
			LOG_ERR("resource %d not found", path->res_id);
		}

		return -ENOENT;
	}

	for (i = 0; i < r->res_inst_count; i++) {
		if (r->res_instances[i].res_inst_id == path->res_inst_id) {
			ri = &r->res_instances[i];
			break;
		}
	}

	/* specifically don't complain about missing resource instance */

	if (obj_inst) {
		*obj_inst = oi;
	}

	if (obj_field) {
		*obj_field = of;
	}

	if (res) {
		*res = r;
	}

	if (ri && res_inst) {
		*res_inst = ri;
	}

	return 0;
}
/* User data setter functions */

int lwm2m_engine_set_res_buf(const char *pathstr, void *buffer_ptr, uint16_t buffer_len,
			     uint16_t data_len, uint8_t data_flags)
{
	struct lwm2m_obj_path path;
	struct lwm2m_engine_res_inst *res_inst = NULL;
	int ret = 0;

	/* translate path -> path_obj */
	ret = lwm2m_string_to_path(pathstr, &path, '/');
	if (ret < 0) {
		return ret;
	}

	if (path.level < 3) {
		LOG_ERR("path must have at least 3 parts");
		return -EINVAL;
	}

	k_mutex_lock(&registry_lock, K_FOREVER);
	/* look up resource obj */
	ret = path_to_objs(&path, NULL, NULL, NULL, &res_inst);
	if (ret < 0) {
		k_mutex_unlock(&registry_lock);
		return ret;
	}

	if (!res_inst) {
		LOG_ERR("res instance %d not found", path.res_inst_id);
		k_mutex_unlock(&registry_lock);
		return -ENOENT;
	}

	/* assign data elements */
	res_inst->data_ptr = buffer_ptr;
	res_inst->data_len = data_len;
	res_inst->max_data_len = buffer_len;
	res_inst->data_flags = data_flags;

	k_mutex_unlock(&registry_lock);
	return ret;
}

int lwm2m_engine_set_res_data(const char *pathstr, void *data_ptr, uint16_t data_len,
			      uint8_t data_flags)
{
	return lwm2m_engine_set_res_buf(pathstr, data_ptr, data_len, data_len, data_flags);
}

static bool lwm2m_validate_time_resource_lenghts(uint16_t resource_length, uint16_t buf_length)
{
	if (resource_length != sizeof(time_t) && resource_length != sizeof(uint32_t)) {
		return false;
	}

	if (buf_length != sizeof(time_t) && buf_length != sizeof(uint32_t)) {
		return false;
	}

	return true;
}

static int lwm2m_engine_set(const char *pathstr, const void *value, uint16_t len)
{
	struct lwm2m_obj_path path;
	struct lwm2m_engine_obj_inst *obj_inst;
	struct lwm2m_engine_obj_field *obj_field;
	struct lwm2m_engine_res *res = NULL;
	struct lwm2m_engine_res_inst *res_inst = NULL;
	void *data_ptr = NULL;
	size_t max_data_len = 0;
	int ret = 0;
	bool changed = false;

	LOG_DBG("path:%s, value:%p, len:%d", pathstr, value, len);

	/* translate path -> path_obj */
	ret = lwm2m_string_to_path(pathstr, &path, '/');
	if (ret < 0) {
		return ret;
	}

	if (path.level < 3) {
		LOG_ERR("path must have at least 3 parts");
		return -EINVAL;
	}

	k_mutex_lock(&registry_lock, K_FOREVER);
	/* look up resource obj */
	ret = path_to_objs(&path, &obj_inst, &obj_field, &res, &res_inst);
	if (ret < 0) {
		k_mutex_unlock(&registry_lock);
		return ret;
	}

	if (!res_inst) {
		LOG_ERR("res instance %d not found", path.res_inst_id);
		k_mutex_unlock(&registry_lock);
		return -ENOENT;
	}

	if (LWM2M_HAS_RES_FLAG(res_inst, LWM2M_RES_DATA_FLAG_RO)) {
		LOG_ERR("res instance data pointer is read-only "
			"[%u/%u/%u/%u:%u]",
			path.obj_id, path.obj_inst_id, path.res_id, path.res_inst_id, path.level);
		k_mutex_unlock(&registry_lock);
		return -EACCES;
	}

	/* setup initial data elements */
	data_ptr = res_inst->data_ptr;
	max_data_len = res_inst->max_data_len;

	/* allow user to override data elements via callback */
	if (res->pre_write_cb) {
		data_ptr = res->pre_write_cb(obj_inst->obj_inst_id, res->res_id,
					     res_inst->res_inst_id, &max_data_len);
	}

	if (!data_ptr) {
		LOG_ERR("res instance data pointer is NULL [%u/%u/%u/%u:%u]", path.obj_id,
			path.obj_inst_id, path.res_id, path.res_inst_id, path.level);
		k_mutex_unlock(&registry_lock);
		return -EINVAL;
	}

	/* check length (note: we add 1 to string length for NULL pad) */
	if (len > max_data_len - (obj_field->data_type == LWM2M_RES_TYPE_STRING ? 1 : 0)) {
		LOG_ERR("length %u is too long for res instance %d data", len, path.res_id);
		k_mutex_unlock(&registry_lock);
		return -ENOMEM;
	}

	if (memcmp(data_ptr, value, len) != 0) {
		changed = true;
	}

#if CONFIG_LWM2M_ENGINE_VALIDATION_BUFFER_SIZE > 0
	if (res->validate_cb) {
		ret = res->validate_cb(obj_inst->obj_inst_id, res->res_id, res_inst->res_inst_id,
				       (uint8_t *)value, len, false, 0);
		if (ret < 0) {
			k_mutex_unlock(&registry_lock);
			return -EINVAL;
		}
	}
#endif /* CONFIG_LWM2M_ENGINE_VALIDATION_BUFFER_SIZE > 0 */

	switch (obj_field->data_type) {

	case LWM2M_RES_TYPE_OPAQUE:
		memcpy((uint8_t *)data_ptr, value, len);
		break;

	case LWM2M_RES_TYPE_STRING:
		memcpy((uint8_t *)data_ptr, value, len);
		((uint8_t *)data_ptr)[len] = '\0';
		break;

	case LWM2M_RES_TYPE_U32:
		*((uint32_t *)data_ptr) = *(uint32_t *)value;
		break;

	case LWM2M_RES_TYPE_U16:
		*((uint16_t *)data_ptr) = *(uint16_t *)value;
		break;

	case LWM2M_RES_TYPE_U8:
		*((uint8_t *)data_ptr) = *(uint8_t *)value;
		break;

	case LWM2M_RES_TYPE_TIME:
		if (!lwm2m_validate_time_resource_lenghts(max_data_len, len)) {
			LOG_ERR("Time Set: buffer length %u  max data len %u not supported", len,
				max_data_len);
			return -EINVAL;
		}

		if (max_data_len == sizeof(time_t)) {
			if (len == sizeof(time_t)) {
				*((time_t *)data_ptr) = *(time_t *)value;
			} else {
				*((time_t *)data_ptr) = (time_t) *((uint32_t *)value);
			}
		} else {
			LOG_WRN("Converting time to 32bit may cause integer overflow on resource "
				"%s",
				pathstr);
			if (len == sizeof(uint32_t)) {
				*((uint32_t *)data_ptr) = *(uint32_t *)value;
			} else {
				*((uint32_t *)data_ptr) = (uint32_t) *((time_t *)value);
			}
		}

		break;

	case LWM2M_RES_TYPE_S64:
		*((int64_t *)data_ptr) = *(int64_t *)value;
		break;

	case LWM2M_RES_TYPE_S32:
		*((int32_t *)data_ptr) = *(int32_t *)value;
		break;

	case LWM2M_RES_TYPE_S16:
		*((int16_t *)data_ptr) = *(int16_t *)value;
		break;

	case LWM2M_RES_TYPE_S8:
		*((int8_t *)data_ptr) = *(int8_t *)value;
		break;

	case LWM2M_RES_TYPE_BOOL:
		*((bool *)data_ptr) = *(bool *)value;
		break;

	case LWM2M_RES_TYPE_FLOAT:
		*(double *)data_ptr = *(double *)value;
		break;

	case LWM2M_RES_TYPE_OBJLNK:
		*((struct lwm2m_objlnk *)data_ptr) = *(struct lwm2m_objlnk *)value;
		break;

	default:
		LOG_ERR("unknown obj data_type %d", obj_field->data_type);
		k_mutex_unlock(&registry_lock);
		return -EINVAL;
	}

	res_inst->data_len = len;

	/* Cache Data Write */
#if defined(CONFIG_LWM2M_RESOURCE_DATA_CACHE_SUPPORT)
	lwm2m_engine_cache_write(obj_field, pathstr, value, len);
#endif

	if (res->post_write_cb) {
		ret = res->post_write_cb(obj_inst->obj_inst_id, res->res_id, res_inst->res_inst_id,
					 data_ptr, len, false, 0);
	}

	if (changed && LWM2M_HAS_PERM(obj_field, LWM2M_PERM_R)) {
		lwm2m_notify_observer_path(&path);
	}
	k_mutex_unlock(&registry_lock);
	return ret;
}

int lwm2m_engine_set_opaque(const char *pathstr, const char *data_ptr, uint16_t data_len)
{
	return lwm2m_engine_set(pathstr, data_ptr, data_len);
}

int lwm2m_engine_set_string(const char *pathstr, const char *data_ptr)
{
	return lwm2m_engine_set(pathstr, data_ptr, strlen(data_ptr));
}

int lwm2m_engine_set_u8(const char *pathstr, uint8_t value)
{
	return lwm2m_engine_set(pathstr, &value, 1);
}

int lwm2m_engine_set_u16(const char *pathstr, uint16_t value)
{
	return lwm2m_engine_set(pathstr, &value, 2);
}

int lwm2m_engine_set_u32(const char *pathstr, uint32_t value)
{
	return lwm2m_engine_set(pathstr, &value, 4);
}

int lwm2m_engine_set_u64(const char *pathstr, uint64_t value)
{
	return lwm2m_engine_set(pathstr, &value, 8);
}

int lwm2m_engine_set_s8(const char *pathstr, int8_t value)
{
	return lwm2m_engine_set(pathstr, &value, 1);
}

int lwm2m_engine_set_s16(const char *pathstr, int16_t value)
{
	return lwm2m_engine_set(pathstr, &value, 2);
}

int lwm2m_engine_set_s32(const char *pathstr, int32_t value)
{
	return lwm2m_engine_set(pathstr, &value, 4);
}

int lwm2m_engine_set_s64(const char *pathstr, int64_t value)
{
	return lwm2m_engine_set(pathstr, &value, 8);
}

int lwm2m_engine_set_bool(const char *pathstr, bool value)
{
	uint8_t temp = (value != 0 ? 1 : 0);

	return lwm2m_engine_set(pathstr, &temp, 1);
}

int lwm2m_engine_set_float(const char *pathstr, const double *value)
{
	return lwm2m_engine_set(pathstr, value, sizeof(double));
}

int lwm2m_engine_set_objlnk(const char *pathstr, const struct lwm2m_objlnk *value)
{
	return lwm2m_engine_set(pathstr, value, sizeof(struct lwm2m_objlnk));
}

int lwm2m_engine_set_time(const char *pathstr, time_t value)
{
	return lwm2m_engine_set(pathstr, &value, sizeof(time_t));
}

int lwm2m_engine_set_res_data_len(const char *pathstr, uint16_t data_len)
{
	int ret;
	void *buffer_ptr;
	uint16_t buffer_len;
	uint16_t old_len;
	uint8_t data_flags;

	ret = lwm2m_engine_get_res_buf(pathstr, &buffer_ptr, &buffer_len, &old_len, &data_flags);
	if (ret) {
		return ret;
	}
	return lwm2m_engine_set_res_buf(pathstr, buffer_ptr, buffer_len, data_len, data_flags);
}
/* User data getter functions */

int lwm2m_engine_get_res_buf(const char *pathstr, void **buffer_ptr, uint16_t *buffer_len,
			     uint16_t *data_len, uint8_t *data_flags)
{
	struct lwm2m_obj_path path;
	struct lwm2m_engine_res_inst *res_inst = NULL;
	int ret = 0;

	/* translate path -> path_obj */
	ret = lwm2m_string_to_path(pathstr, &path, '/');
	if (ret < 0) {
		return ret;
	}

	if (path.level < 3) {
		LOG_ERR("path must have at least 3 parts");
		return -EINVAL;
	}

	k_mutex_lock(&registry_lock, K_FOREVER);
	/* look up resource obj */
	ret = path_to_objs(&path, NULL, NULL, NULL, &res_inst);
	if (ret < 0) {
		k_mutex_unlock(&registry_lock);
		return ret;
	}

	if (!res_inst) {
		LOG_ERR("res instance %d not found", path.res_inst_id);
		k_mutex_unlock(&registry_lock);
		return -ENOENT;
	}

	if (buffer_ptr) {
		*buffer_ptr = res_inst->data_ptr;
	}
	if (buffer_len) {
		*buffer_len = res_inst->max_data_len;
	}
	if (data_len) {
		*data_len = res_inst->data_len;
	}
	if (data_flags) {
		*data_flags = res_inst->data_flags;
	}

	k_mutex_unlock(&registry_lock);
	return 0;
}

int lwm2m_engine_get_res_data(const char *pathstr, void **data_ptr, uint16_t *data_len,
			      uint8_t *data_flags)
{
	return lwm2m_engine_get_res_buf(pathstr, data_ptr, NULL, data_len, data_flags);
}

static int lwm2m_check_buf_sizes(uint8_t data_type, uint16_t resource_length, uint16_t buf_length)
{
	switch (data_type) {
	case LWM2M_RES_TYPE_OPAQUE:
	case LWM2M_RES_TYPE_STRING:
		if (resource_length > buf_length) {
			return -ENOMEM;
		}
		break;
	case LWM2M_RES_TYPE_U32:
	case LWM2M_RES_TYPE_U8:
	case LWM2M_RES_TYPE_S64:
	case LWM2M_RES_TYPE_S32:
	case LWM2M_RES_TYPE_S16:
	case LWM2M_RES_TYPE_S8:
	case LWM2M_RES_TYPE_BOOL:
	case LWM2M_RES_TYPE_FLOAT:
	case LWM2M_RES_TYPE_OBJLNK:
		if (resource_length != buf_length) {
			return -EINVAL;
		}
		break;
	default:
		return 0;
	}
	return 0;
}

static int lwm2m_engine_get(const char *pathstr, void *buf, uint16_t buflen)
{
	int ret = 0;
	struct lwm2m_obj_path path;
	struct lwm2m_engine_obj_inst *obj_inst;
	struct lwm2m_engine_obj_field *obj_field;
	struct lwm2m_engine_res *res = NULL;
	struct lwm2m_engine_res_inst *res_inst = NULL;
	void *data_ptr = NULL;
	size_t data_len = 0;

	LOG_DBG("path:%s, buf:%p, buflen:%d", pathstr, buf, buflen);

	/* translate path -> path_obj */
	ret = lwm2m_string_to_path(pathstr, &path, '/');
	if (ret < 0) {
		return ret;
	}

	if (path.level < 3) {
		LOG_ERR("path must have at least 3 parts");
		return -EINVAL;
	}
	k_mutex_lock(&registry_lock, K_FOREVER);
	/* look up resource obj */
	ret = path_to_objs(&path, &obj_inst, &obj_field, &res, &res_inst);
	if (ret < 0) {
		k_mutex_unlock(&registry_lock);
		return ret;
	}

	if (!res_inst) {
		LOG_ERR("res instance %d not found", path.res_inst_id);
		k_mutex_unlock(&registry_lock);
		return -ENOENT;
	}

	/* setup initial data elements */
	data_ptr = res_inst->data_ptr;
	data_len = res_inst->data_len;

	/* allow user to override data elements via callback */
	if (res->read_cb) {
		data_ptr = res->read_cb(obj_inst->obj_inst_id, res->res_id, res_inst->res_inst_id,
					&data_len);
	}

	if (data_ptr && data_len > 0) {
		ret = lwm2m_check_buf_sizes(obj_field->data_type, data_len, buflen);
		if (ret) {
			LOG_ERR("Incorrect resource data length %u. Buffer length %u", data_len,
				buflen);
			k_mutex_unlock(&registry_lock);
			return ret;
		}

		switch (obj_field->data_type) {

		case LWM2M_RES_TYPE_OPAQUE:
			memcpy(buf, data_ptr, data_len);
			break;

		case LWM2M_RES_TYPE_STRING:
			strncpy((uint8_t *)buf, (uint8_t *)data_ptr, buflen);
			break;

		case LWM2M_RES_TYPE_U32:
			*(uint32_t *)buf = *(uint32_t *)data_ptr;
			break;
		case LWM2M_RES_TYPE_TIME:
			if (!lwm2m_validate_time_resource_lenghts(data_len, buflen)) {
				LOG_ERR("Time get buffer length %u  data len %u not supported",
					buflen, data_len);
				return -EINVAL;
			}

			if (data_len == sizeof(time_t)) {
				if (buflen == sizeof(time_t)) {
					*((time_t *)buf) = *(time_t *)data_ptr;
				} else {
					/* In this case get operation may not got correct value */
					LOG_WRN("Converting time to 32bit may cause integer "
						"overflow:%s",
						pathstr);
					*((uint32_t *)buf) = (uint32_t) *((time_t *)data_ptr);
				}
			} else {
				LOG_WRN("Converting time to 32bit may cause integer overflow:%s",
					pathstr);
				if (buflen == sizeof(uint32_t)) {
					*((uint32_t *)buf) = *(uint32_t *)data_ptr;
				} else {
					*((time_t *)buf) = (time_t) *((uint32_t *)data_ptr);
				}
			}
			break;

		case LWM2M_RES_TYPE_U16:
			*(uint16_t *)buf = *(uint16_t *)data_ptr;
			break;

		case LWM2M_RES_TYPE_U8:
			*(uint8_t *)buf = *(uint8_t *)data_ptr;
			break;

		case LWM2M_RES_TYPE_S64:
			*(int64_t *)buf = *(int64_t *)data_ptr;
			break;

		case LWM2M_RES_TYPE_S32:
			*(int32_t *)buf = *(int32_t *)data_ptr;
			break;

		case LWM2M_RES_TYPE_S16:
			*(int16_t *)buf = *(int16_t *)data_ptr;
			break;

		case LWM2M_RES_TYPE_S8:
			*(int8_t *)buf = *(int8_t *)data_ptr;
			break;

		case LWM2M_RES_TYPE_BOOL:
			*(bool *)buf = *(bool *)data_ptr;
			break;

		case LWM2M_RES_TYPE_FLOAT:
			*(double *)buf = *(double *)data_ptr;
			break;

		case LWM2M_RES_TYPE_OBJLNK:
			*(struct lwm2m_objlnk *)buf = *(struct lwm2m_objlnk *)data_ptr;
			break;

		default:
			LOG_ERR("unknown obj data_type %d", obj_field->data_type);
			k_mutex_unlock(&registry_lock);
			return -EINVAL;
		}
	}
	k_mutex_unlock(&registry_lock);
	return 0;
}

int lwm2m_engine_get_opaque(const char *pathstr, void *buf, uint16_t buflen)
{
	return lwm2m_engine_get(pathstr, buf, buflen);
}

int lwm2m_engine_get_string(const char *pathstr, void *buf, uint16_t buflen)
{
	return lwm2m_engine_get(pathstr, buf, buflen);
}

int lwm2m_engine_get_u8(const char *pathstr, uint8_t *value)
{
	return lwm2m_engine_get(pathstr, value, 1);
}

int lwm2m_engine_get_u16(const char *pathstr, uint16_t *value)
{
	return lwm2m_engine_get(pathstr, value, 2);
}

int lwm2m_engine_get_u32(const char *pathstr, uint32_t *value)
{
	return lwm2m_engine_get(pathstr, value, 4);
}

int lwm2m_engine_get_u64(const char *pathstr, uint64_t *value)
{
	return lwm2m_engine_get(pathstr, value, 8);
}

int lwm2m_engine_get_s8(const char *pathstr, int8_t *value)
{
	return lwm2m_engine_get(pathstr, value, 1);
}

int lwm2m_engine_get_s16(const char *pathstr, int16_t *value)
{
	return lwm2m_engine_get(pathstr, value, 2);
}

int lwm2m_engine_get_s32(const char *pathstr, int32_t *value)
{
	return lwm2m_engine_get(pathstr, value, 4);
}

int lwm2m_engine_get_s64(const char *pathstr, int64_t *value)
{
	return lwm2m_engine_get(pathstr, value, 8);
}

int lwm2m_engine_get_bool(const char *pathstr, bool *value)
{
	int ret = 0;
	int8_t temp = 0;

	ret = lwm2m_engine_get_s8(pathstr, &temp);
	if (!ret) {
		*value = temp != 0;
	}

	return ret;
}

int lwm2m_engine_get_float(const char *pathstr, double *buf)
{
	return lwm2m_engine_get(pathstr, buf, sizeof(double));
}

int lwm2m_engine_get_objlnk(const char *pathstr, struct lwm2m_objlnk *buf)
{
	return lwm2m_engine_get(pathstr, buf, sizeof(struct lwm2m_objlnk));
}

int lwm2m_engine_get_time(const char *pathstr, time_t *buf)
{
	return lwm2m_engine_get(pathstr, buf, sizeof(time_t));
}

int lwm2m_engine_get_resource(const char *pathstr, struct lwm2m_engine_res **res)
{
	int ret;
	struct lwm2m_obj_path path;

	ret = lwm2m_string_to_path(pathstr, &path, '/');
	if (ret < 0) {
		return ret;
	}

	if (path.level < 3) {
		LOG_ERR("path must have 3 parts");
		return -EINVAL;
	}

	return path_to_objs(&path, NULL, NULL, res, NULL);
}

size_t lwm2m_engine_get_opaque_more(struct lwm2m_input_context *in, uint8_t *buf, size_t buflen,
				    struct lwm2m_opaque_context *opaque, bool *last_block)
{
	uint32_t in_len = opaque->remaining;
	uint16_t remaining = in->in_cpkt->max_len - in->offset;

	if (in_len > buflen) {
		in_len = buflen;
	}

	if (in_len > remaining) {
		in_len = remaining;
	}

	opaque->remaining -= in_len;
	remaining -= in_len;
	if (opaque->remaining == 0U || remaining == 0) {
		*last_block = true;
	}

	if (buf_read(buf, in_len, CPKT_BUF_READ(in->in_cpkt), &in->offset) < 0) {
		*last_block = true;
		return 0;
	}

	return (size_t)in_len;
}

void lwm2m_engine_get_queue_mode(char *queue)
{
	if (IS_ENABLED(CONFIG_LWM2M_QUEUE_MODE_ENABLED)) {
		strncpy(queue, "Q", QUEUE_OPT_MAX_LEN);
	} else {
		strncpy(queue, "", QUEUE_OPT_MAX_LEN);
	}
}

void lwm2m_engine_get_binding(char *binding)
{
	/* Defaults to UDP. */
	strncpy(binding, "U", BINDING_OPT_MAX_LEN);
#if CONFIG_LWM2M_VERSION_1_0
	/* In LwM2M 1.0 binding and queue mode are in same parameter */
	char queue[QUEUE_OPT_MAX_LEN];

	lwm2m_engine_get_queue_mode(queue);
	strncat(binding, queue, QUEUE_OPT_MAX_LEN);
#endif
}
/* Engine resource instance */

static int lwm2m_engine_allocate_resource_instance(struct lwm2m_engine_res *res,
						   struct lwm2m_engine_res_inst **res_inst,
						   uint8_t resource_instance_id)
{
	int i;

	if (!res->res_instances || res->res_inst_count == 0) {
		return -ENOMEM;
	}

	for (i = 0; i < res->res_inst_count; i++) {
		if (res->res_instances[i].res_inst_id == RES_INSTANCE_NOT_CREATED) {
			break;
		}
	}

	if (i >= res->res_inst_count) {
		return -ENOMEM;
	}

	res->res_instances[i].res_inst_id = resource_instance_id;
	*res_inst = &res->res_instances[i];
	return 0;
}

int lwm2m_engine_get_create_res_inst(struct lwm2m_obj_path *path, struct lwm2m_engine_res **res,
				     struct lwm2m_engine_res_inst **res_inst)
{
	int ret;
	struct lwm2m_engine_res *r = NULL;
	struct lwm2m_engine_res_inst *r_i = NULL;

	ret = path_to_objs(path, NULL, NULL, &r, &r_i);
	if (ret < 0) {
		return ret;
	}

	if (!r) {
		return -ENOENT;
	}
	/* Store resource pointer */
	*res = r;

	if (!r_i) {
		if (path->level < LWM2M_PATH_LEVEL_RESOURCE_INST) {
			return -EINVAL;
		}

		ret = lwm2m_engine_allocate_resource_instance(r, &r_i, path->res_inst_id);
		if (ret < 0) {
			return ret;
		}
	}

	/* Store resource instance pointer */
	*res_inst = r_i;
	return 0;
}

int lwm2m_engine_create_res_inst(const char *pathstr)
{
	int ret;
	struct lwm2m_engine_res *res = NULL;
	struct lwm2m_engine_res_inst *res_inst = NULL;
	struct lwm2m_obj_path path;

	ret = lwm2m_string_to_path(pathstr, &path, '/');
	if (ret < 0) {
		return ret;
	}

	if (path.level < 4) {
		LOG_ERR("path must have 4 parts");
		return -EINVAL;
	}
	k_mutex_lock(&registry_lock, K_FOREVER);
	ret = path_to_objs(&path, NULL, NULL, &res, &res_inst);
	if (ret < 0) {
		k_mutex_unlock(&registry_lock);
		return ret;
	}

	if (!res) {
		LOG_ERR("resource %u not found", path.res_id);
		k_mutex_unlock(&registry_lock);
		return -ENOENT;
	}

	if (res_inst && res_inst->res_inst_id != RES_INSTANCE_NOT_CREATED) {
		LOG_ERR("res instance %u already exists", path.res_inst_id);
		k_mutex_unlock(&registry_lock);
		return -EINVAL;
	}
	k_mutex_unlock(&registry_lock);
	return lwm2m_engine_allocate_resource_instance(res, &res_inst, path.res_inst_id);
}

int lwm2m_engine_delete_res_inst(const char *pathstr)
{
	int ret;
	struct lwm2m_engine_res_inst *res_inst = NULL;
	struct lwm2m_obj_path path;

	ret = lwm2m_string_to_path(pathstr, &path, '/');
	if (ret < 0) {
		return ret;
	}

	if (path.level < 4) {
		LOG_ERR("path must have 4 parts");
		return -EINVAL;
	}
	k_mutex_lock(&registry_lock, K_FOREVER);
	ret = path_to_objs(&path, NULL, NULL, NULL, &res_inst);
	if (ret < 0) {
		k_mutex_unlock(&registry_lock);
		return ret;
	}

	if (!res_inst) {
		LOG_ERR("res instance %u not found", path.res_inst_id);
		k_mutex_unlock(&registry_lock);
		return -ENOENT;
	}

	res_inst->data_ptr = NULL;
	res_inst->max_data_len = 0U;
	res_inst->data_len = 0U;
	res_inst->res_inst_id = RES_INSTANCE_NOT_CREATED;
	k_mutex_unlock(&registry_lock);
	return 0;
}
/* Register callbacks */

int lwm2m_engine_register_read_callback(const char *pathstr, lwm2m_engine_get_data_cb_t cb)
{
	int ret;
	struct lwm2m_engine_res *res = NULL;

	ret = lwm2m_engine_get_resource(pathstr, &res);
	if (ret < 0) {
		return ret;
	}

	res->read_cb = cb;
	return 0;
}

int lwm2m_engine_register_pre_write_callback(const char *pathstr, lwm2m_engine_get_data_cb_t cb)
{
	int ret;
	struct lwm2m_engine_res *res = NULL;

	ret = lwm2m_engine_get_resource(pathstr, &res);
	if (ret < 0) {
		return ret;
	}

	res->pre_write_cb = cb;
	return 0;
}

int lwm2m_engine_register_validate_callback(const char *pathstr, lwm2m_engine_set_data_cb_t cb)
{
#if CONFIG_LWM2M_ENGINE_VALIDATION_BUFFER_SIZE > 0
	int ret;
	struct lwm2m_engine_res *res = NULL;

	ret = lwm2m_engine_get_resource(pathstr, &res);
	if (ret < 0) {
		return ret;
	}

	res->validate_cb = cb;
	return 0;
#else
	ARG_UNUSED(pathstr);
	ARG_UNUSED(cb);

	LOG_ERR("Validation disabled. Set "
		"CONFIG_LWM2M_ENGINE_VALIDATION_BUFFER_SIZE > 0 to "
		"enable validation support.");
	return -ENOTSUP;
#endif /* CONFIG_LWM2M_ENGINE_VALIDATION_BUFFER_SIZE > 0 */
}

int lwm2m_engine_register_post_write_callback(const char *pathstr, lwm2m_engine_set_data_cb_t cb)
{
	int ret;
	struct lwm2m_engine_res *res = NULL;

	ret = lwm2m_engine_get_resource(pathstr, &res);
	if (ret < 0) {
		return ret;
	}

	res->post_write_cb = cb;
	return 0;
}

int lwm2m_engine_register_exec_callback(const char *pathstr, lwm2m_engine_execute_cb_t cb)
{
	int ret;
	struct lwm2m_engine_res *res = NULL;

	ret = lwm2m_engine_get_resource(pathstr, &res);
	if (ret < 0) {
		return ret;
	}

	res->execute_cb = cb;
	return 0;
}

int lwm2m_engine_register_create_callback(uint16_t obj_id, lwm2m_engine_user_cb_t cb)
{
	struct lwm2m_engine_obj *obj = NULL;

	obj = get_engine_obj(obj_id);
	if (!obj) {
		LOG_ERR("unable to find obj: %u", obj_id);
		return -ENOENT;
	}

	obj->user_create_cb = cb;
	return 0;
}

int lwm2m_engine_register_delete_callback(uint16_t obj_id, lwm2m_engine_user_cb_t cb)
{
	struct lwm2m_engine_obj *obj = NULL;

	obj = get_engine_obj(obj_id);
	if (!obj) {
		LOG_ERR("unable to find obj: %u", obj_id);
		return -ENOENT;
	}

	obj->user_delete_cb = cb;
	return 0;
}
/* Generic data handlers */

int lwm2m_get_or_create_engine_obj(struct lwm2m_message *msg,
				   struct lwm2m_engine_obj_inst **obj_inst, uint8_t *created)
{
	int ret = 0;

	if (created) {
		*created = 0U;
	}

	*obj_inst = get_engine_obj_inst(msg->path.obj_id, msg->path.obj_inst_id);
	if (!*obj_inst) {
		ret = lwm2m_create_obj_inst(msg->path.obj_id, msg->path.obj_inst_id, obj_inst);
		if (ret < 0) {
			return ret;
		}

		/* set created flag to one */
		if (created) {
			*created = 1U;
		}

#if defined(CONFIG_LWM2M_RD_CLIENT_SUPPORT)
		if (!msg->ctx->bootstrap_mode) {
			engine_trigger_update(true);
		}
#endif
	}

	return ret;
}

struct lwm2m_engine_res *lwm2m_engine_get_res(const struct lwm2m_obj_path *path)
{
	struct lwm2m_engine_res *res = NULL;
	int ret;

	if (path->level < LWM2M_PATH_LEVEL_RESOURCE) {
		return NULL;
	}

	ret = path_to_objs(path, NULL, NULL, &res, NULL);
	if (ret < 0) {
		return NULL;
	}

	return res;
}

struct lwm2m_engine_res_inst *lwm2m_engine_get_res_inst(const struct lwm2m_obj_path *path)
{
	struct lwm2m_engine_res_inst *res_inst = NULL;
	int ret;

	if (path->level != LWM2M_PATH_LEVEL_RESOURCE_INST) {
		return NULL;
	}

	ret = path_to_objs(path, NULL, NULL, NULL, &res_inst);
	if (ret < 0) {
		return NULL;
	}

	return res_inst;
}

bool lwm2m_engine_shall_report_obj_version(const struct lwm2m_engine_obj *obj)
{
	if (obj->is_core) {
		return obj->version_major != LWM2M_PROTOCOL_VERSION_MAJOR ||
		       obj->version_minor != LWM2M_PROTOCOL_VERSION_MINOR;
	}

	return obj->version_major != 1 || obj->version_minor != 0;
}

#if defined(CONFIG_LWM2M_RESOURCE_DATA_CACHE_SUPPORT)
static sys_slist_t lwm2m_timed_cache_list;
static struct lwm2m_time_series_resource lwm2m_cache_entries[CONFIG_LWM2M_MAX_CACHED_RESOURCES];

static void lwm2m_cache_add_path_to_list(struct lwm2m_time_series_resource *new_entry)
{
	struct lwm2m_time_series_resource *prev = NULL;
	struct lwm2m_time_series_resource *entry;

	if (!sys_slist_is_empty(&lwm2m_timed_cache_list)) {

		/* Keep list Alphabetical order */
		SYS_SLIST_FOR_EACH_CONTAINER(&lwm2m_timed_cache_list, entry, node) {

			if (strcmp(entry->path, new_entry->path) < 0) {
				/* Current entry have  */
				prev = entry;
				continue;
			}

			if (prev) {
				sys_slist_insert(&lwm2m_timed_cache_list, &prev->node,
						 &new_entry->node);
			} else {
				sys_slist_prepend(&lwm2m_timed_cache_list, &new_entry->node);
			}
			return;
		}
	}

	/* Add First or new tail entry */
	sys_slist_append(&lwm2m_timed_cache_list, &new_entry->node);
}

static struct lwm2m_time_series_resource *lwm2m_cache_entry_allocate(char const *resource_path)
{
	int i;
	struct lwm2m_time_series_resource *entry;

	entry = lwm2m_cache_entry_get_by_string(resource_path);
	if (entry) {
		return entry;
	}

	for (i = 0; i < ARRAY_SIZE(lwm2m_cache_entries); i++) {
		if (lwm2m_cache_entries[i].path == NULL) {
			lwm2m_cache_entries[i].path = resource_path;
			lwm2m_cache_add_path_to_list(&lwm2m_cache_entries[i]);
			return &lwm2m_cache_entries[i];
		}
	}

	return NULL;
}

static void lwm2m_engine_cache_write(struct lwm2m_engine_obj_field *obj_field, const char *pathstr,
				     const void *value, uint16_t len)
{
	struct lwm2m_time_series_resource *cache_entry;
	struct lwm2m_time_series_elem elements;

	cache_entry = lwm2m_cache_entry_get_by_string(pathstr);
	if (!cache_entry) {
		return;
	}

	elements.t = time(NULL);

	if (elements.t  <= 0) {
		LOG_WRN("Time() not available");
		return;
	}

	switch (obj_field->data_type) {
	case LWM2M_RES_TYPE_U32:
		elements.u32 = *(uint32_t *)value;
		break;

	case LWM2M_RES_TYPE_U16:
		elements.u16 = *(uint16_t *)value;
		break;

	case LWM2M_RES_TYPE_U8:
		elements.u8 = *(uint8_t *)value;
		break;

	case LWM2M_RES_TYPE_S64:
		elements.i64 = *(int64_t *)value;
		break;

	case LWM2M_RES_TYPE_TIME:
		if (len == sizeof(time_t)) {
			elements.time = *(time_t *)value;
		} else if (len == sizeof(uint32_t)) {
			elements.time = (time_t) *((uint32_t *)value);
		} else {
			LOG_ERR("Not supporting size %d bytes for time", len);
			return;
		}
		break;

	case LWM2M_RES_TYPE_S32:
		elements.i32 = *(int32_t *)value;
		break;

	case LWM2M_RES_TYPE_S16:
		elements.i16 = *(int16_t *)value;
		break;

	case LWM2M_RES_TYPE_S8:
		elements.i8 = *(int8_t *)value;
		break;

	case LWM2M_RES_TYPE_BOOL:
		elements.b = *(bool *)value;
		break;

	default:
		elements.f = *(double *)value;
		break;
	}

	if (!lwm2m_cache_write(cache_entry, &elements)) {
		LOG_WRN("Data cache full");
	}
}
#endif /* CONFIG_LWM2M_RESOURCE_DATA_CACHE_SUPPORT */

struct lwm2m_time_series_resource *lwm2m_cache_entry_get_by_string(char const *resource_path)
{
#if defined(CONFIG_LWM2M_RESOURCE_DATA_CACHE_SUPPORT)
	if (!sys_slist_is_empty(&lwm2m_timed_cache_list)) {
		int ret;
		struct lwm2m_time_series_resource *entry;

		/* Keep list Alphabetical order */
		SYS_SLIST_FOR_EACH_CONTAINER(&lwm2m_timed_cache_list, entry, node) {
			ret = strcmp(entry->path, resource_path);
			if (ret == 0) {
				return entry;
			} else if (ret > 0) {
				return NULL;
			}
		}
	}
#endif /* CONFIG_LWM2M_RESOURCE_DATA_CACHE_SUPPORT */
	return NULL;
}

struct lwm2m_time_series_resource *lwm2m_cache_entry_get_by_object(struct lwm2m_obj_path *obj_path)
{
#if defined(CONFIG_LWM2M_RESOURCE_DATA_CACHE_SUPPORT)
	char obj_path_str[25];
	char const *resource_path;

	if (!obj_path || obj_path->level < LWM2M_PATH_LEVEL_RESOURCE) {
		LOG_ERR("Path level wrong for cache %u", obj_path->level);
		return NULL;
	}

	/* Decode path to string */
	resource_path = lwm2m_path_log_buf(obj_path_str, obj_path);

	return lwm2m_cache_entry_get_by_string(resource_path);
#else
	return NULL;
#endif /* CONFIG_LWM2M_RESOURCE_DATA_CACHE_SUPPORT */
}

int lwm2m_engine_enable_cache(char const *resource_path, struct lwm2m_time_series_elem *data_cache,
			    size_t cache_len)
{
#if defined(CONFIG_LWM2M_RESOURCE_DATA_CACHE_SUPPORT)
	struct lwm2m_obj_path path;
	struct lwm2m_engine_obj_inst *obj_inst;
	struct lwm2m_engine_obj_field *obj_field;
	struct lwm2m_engine_res_inst *res_inst = NULL;
	struct lwm2m_time_series_resource *cache_entry;
	int ret = 0;
	size_t cache_entry_size = sizeof(struct lwm2m_time_series_elem);

	/* translate path -> path_obj */
	ret = lwm2m_string_to_path(resource_path, &path, '/');
	if (ret < 0) {
		return ret;
	}

	if (path.level < 3) {
		LOG_ERR("path must have at least 3 parts");
		return -EINVAL;
	}

	/* look up resource obj */
	ret = path_to_objs(&path, &obj_inst, &obj_field, NULL, &res_inst);
	if (ret < 0) {
		return ret;
	}

	if (!res_inst) {
		LOG_ERR("res instance %d not found", path.res_inst_id);
		return -ENOENT;
	}

	switch (obj_field->data_type) {
	case LWM2M_RES_TYPE_U32:
	case LWM2M_RES_TYPE_TIME:
	case LWM2M_RES_TYPE_U16:
	case LWM2M_RES_TYPE_U8:
	case LWM2M_RES_TYPE_S64:
	case LWM2M_RES_TYPE_S32:
	case LWM2M_RES_TYPE_S16:
	case LWM2M_RES_TYPE_S8:
	case LWM2M_RES_TYPE_BOOL:
	case LWM2M_RES_TYPE_FLOAT:
		/* Support only fixed width resource types */
		cache_entry = lwm2m_cache_entry_allocate(resource_path);
		break;
	default:
		cache_entry = NULL;
		break;
	}

	if (!cache_entry) {
		return -ENODATA;
	}

	ring_buf_init(&cache_entry->rb, cache_entry_size * cache_len, (uint8_t *)data_cache);

	return 0;
#else
	LOG_ERR("LwM2M resource cache is only supported for "
		"CONFIG_LWM2M_RESOURCE_DATA_CACHE_SUPPORT");
	return -ENOTSUP;
#endif /* CONFIG_LWM2M_RESOURCE_DATA_CACHE_SUPPORT */
}

int lwm2m_engine_data_cache_init(void)
{
#if defined(CONFIG_LWM2M_RESOURCE_DATA_CACHE_SUPPORT)
	int i;

	sys_slist_init(&lwm2m_timed_cache_list);

	for (i = 0; i < ARRAY_SIZE(lwm2m_cache_entries); i++) {
		lwm2m_cache_entries[i].path = NULL;
	}
#endif
	return 0;
}

bool lwm2m_cache_write(struct lwm2m_time_series_resource *cache_entry,
		       struct lwm2m_time_series_elem *buf)
{
#if defined(CONFIG_LWM2M_RESOURCE_DATA_CACHE_SUPPORT)
	uint32_t length;
	uint8_t *buf_ptr;
	uint32_t element_size = sizeof(struct lwm2m_time_series_elem);

	if (ring_buf_space_get(&cache_entry->rb) < element_size) {
		/* No space  */
		if (IS_ENABLED(CONFIG_LWM2M_CACHE_DROP_LATEST)) {
			return false;
		}
		/* Free entry */
		length = ring_buf_get_claim(&cache_entry->rb, &buf_ptr, element_size);
		ring_buf_get_finish(&cache_entry->rb, length);
	}

	length = ring_buf_put_claim(&cache_entry->rb, &buf_ptr, element_size);

	if (length != element_size) {
		ring_buf_put_finish(&cache_entry->rb, 0);
		LOG_ERR("Allocation failed %u", length);
		return false;
	}

	ring_buf_put_finish(&cache_entry->rb, length);
	/* Store data */
	memcpy(buf_ptr, buf, element_size);
	return true;
#else
	return NULL;
#endif
}

bool lwm2m_cache_read(struct lwm2m_time_series_resource *cache_entry,
		      struct lwm2m_time_series_elem *buf)
{
#if defined(CONFIG_LWM2M_RESOURCE_DATA_CACHE_SUPPORT)
	uint32_t length;
	uint8_t *buf_ptr;
	uint32_t element_size = sizeof(struct lwm2m_time_series_elem);

	if (ring_buf_is_empty(&cache_entry->rb)) {
		return false;
	}

	length = ring_buf_get_claim(&cache_entry->rb, &buf_ptr, element_size);

	if (length != element_size) {
		LOG_ERR("Cache read fail %u", length);
		ring_buf_get_finish(&cache_entry->rb, 0);
		return false;
	}

	/* Read Data */
	memcpy(buf, buf_ptr, element_size);
	ring_buf_get_finish(&cache_entry->rb, length);
	return true;

#else
	return NULL;
#endif
}

size_t lwm2m_cache_size(struct lwm2m_time_series_resource *cache_entry)
{
#if defined(CONFIG_LWM2M_RESOURCE_DATA_CACHE_SUPPORT)
	uint32_t bytes_available;

	if (ring_buf_is_empty(&cache_entry->rb)) {
		return 0;
	}

	bytes_available = ring_buf_size_get(&cache_entry->rb);

	return (bytes_available / sizeof(struct lwm2m_time_series_elem));
#else
	return 0;
#endif
}
