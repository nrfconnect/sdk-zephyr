/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_MODULE_NAME net_lwm2m_senml_cbor
#define LOG_LEVEL CONFIG_LWM2M_LOG_LEVEL

#include <logging/log.h>
LOG_MODULE_REGISTER(LOG_MODULE_NAME);

#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <inttypes.h>
#include <ctype.h>
#include <sys/util.h>

#include <zcbor_common.h>
#include <zcbor_decode.h>
#include <zcbor_encode.h>

#include "lwm2m_engine.h"
#include "lwm2m_object.h"
#include "lwm2m_rw_senml_cbor.h"
#include "lwm2m_senml_cbor_decode.h"
#include "lwm2m_senml_cbor_encode.h"
#include "lwm2m_senml_cbor_types.h"
#include "lwm2m_util.h"

struct cbor_out_fmt_data {
	/* Data */
	struct lwm2m_senml input;

	/* Storage for basenames and names ~ sizeof("/65535/999/") */
	struct {
		char names[CONFIG_LWM2M_RW_SENML_CBOR_RECORDS][sizeof("/65535/999/")];
		size_t name_sz; /* Name buff size */
		uint8_t name_cnt;
	};
};

struct cbor_in_fmt_data {
	/* Decoded data */
	struct lwm2m_senml dcd; /* Decoded data */
	struct record *current;
	char basename[sizeof("/65535/999/")]; /* Null terminated basename */
};

#define GET_CBOR_FD_NAME(fd) ((fd)->names[(fd)->name_cnt])
/* Get the current record */
#define GET_CBOR_FD_REC(fd) \
	&((fd)->input._lwm2m_senml__record[(fd)->input._lwm2m_senml__record_count])
/* Get a record */
#define GET_IN_FD_REC_I(fd, i) &((fd)->dcd._lwm2m_senml__record[i])
/* Consume the current record */
#define CONSUME_CBOR_FD_REC(fd) \
	&((fd)->input._lwm2m_senml__record[(fd)->input._lwm2m_senml__record_count++])
/* Get CBOR output formatter data */
#define LWM2M_OFD_CBOR(octx) ((struct cbor_out_fmt_data *)engine_get_out_user_data(octx))

static struct cbor_out_fmt_data *setup_out_fmt_data(struct lwm2m_message *msg)
{
	struct cbor_out_fmt_data *fd = malloc(sizeof(*fd));

	if (!fd) {
		return NULL;
	}

	(void)memset(fd, 0, sizeof(*fd));
	engine_set_out_user_data(&msg->out, fd);
	fd->name_sz = sizeof("/65535/999/");

	return fd;
}

static void clear_out_fmt_data(struct lwm2m_message *msg, struct cbor_out_fmt_data *fd)
{
	free(fd);
	fd = NULL;

	engine_clear_out_user_data(&msg->out);
}

static struct cbor_in_fmt_data *setup_in_fmt_data(struct lwm2m_message *msg)
{
	struct cbor_in_fmt_data *fd = malloc(sizeof(*fd));

	if (!fd) {
		return NULL;
	}

	(void)memset(fd, 0, sizeof(*fd));
	engine_set_in_user_data(&msg->in, fd);

	return fd;
}

static void clear_in_fmt_data(struct lwm2m_message *msg, struct cbor_in_fmt_data *fd)
{
	free(fd);
	fd = NULL;

	engine_clear_in_user_data(&msg->in);
}

static int put_basename(struct lwm2m_output_context *out, struct lwm2m_obj_path *path)
{
	struct cbor_out_fmt_data *fd = LWM2M_OFD_CBOR(out);
	int len;

	if (fd->name_cnt >= CONFIG_LWM2M_RW_SENML_CBOR_RECORDS) {
		LOG_ERR("CONFIG_LWM2M_RW_SENML_CBOR_RECORDS too small");
		return -ENOMEM;
	}

	char *basename = GET_CBOR_FD_NAME(fd);

	len = lwm2m_path_to_string(basename, fd->name_sz, path, LWM2M_PATH_LEVEL_OBJECT_INST);

	if (len < 0) {
		return len;
	}

	/* Tell CBOR encoder where to find the name */
	struct record *record = GET_CBOR_FD_REC(fd);

	record->_record_bn._record_bn.value = basename;
	record->_record_bn._record_bn.len = len;
	record->_record_bn_present = 1;

	if ((len < sizeof("0/0") - 1) || (len >= sizeof("65535/999"))) {
		__ASSERT_NO_MSG(false);
		return -EINVAL;
	}

	fd->name_cnt++;

	return 0;
}

static int put_empty_array(struct lwm2m_output_context *out)
{
	int len = 1;

	memset(CPKT_BUF_W_PTR(out->out_cpkt), 0x80, len); /* 80 # array(0) */
	out->out_cpkt->offset += len;

	return len;
}

static int put_end(struct lwm2m_output_context *out, struct lwm2m_obj_path *path)
{
	size_t len;
	struct lwm2m_senml *input = &(LWM2M_OFD_CBOR(out)->input);

	if (!input->_lwm2m_senml__record_count) {
		len = put_empty_array(out);

		return len;
	}

	uint_fast8_t ret =
		cbor_encode_lwm2m_senml(CPKT_BUF_W_REGION(out->out_cpkt), input, &len);

	if (ret != ZCBOR_SUCCESS) {
		LOG_ERR("unable to encode senml cbor msg");

		return -E2BIG;
	}

	out->out_cpkt->offset += len;

	return len;
}

static int put_begin_oi(struct lwm2m_output_context *out, struct lwm2m_obj_path *path)
{
	int ret;
	uint8_t tmp = path->level;

	/* In case path level is set to 'none' or 'object' and we have only default oi */
	path->level = LWM2M_PATH_LEVEL_OBJECT_INST;

	ret = put_basename(out, path);
	path->level = tmp;

	return ret;
}

static int put_begin_r(struct lwm2m_output_context *out, struct lwm2m_obj_path *path)
{
	struct cbor_out_fmt_data *fd = LWM2M_OFD_CBOR(out);
	int len;

	if (fd->name_cnt >= CONFIG_LWM2M_RW_SENML_CBOR_RECORDS) {
		LOG_ERR("CONFIG_LWM2M_RW_SENML_CBOR_RECORDS too small");
		return -ENOMEM;
	}

	char *name = GET_CBOR_FD_NAME(fd);

	/* Write resource name */
	len = snprintk(name, sizeof("65535"), "%" PRIu16 "", path->res_id);

	if (len < sizeof("0") - 1) {
		__ASSERT_NO_MSG(false);
		return -EINVAL;
	}

	/* Check if we could use an already existing name
	 * -> latest name slot is used as a scratchpad
	 */
	for (int idx = 0; idx < fd->name_cnt; idx++) {
		if (strncmp(name, fd->names[idx], len) == 0) {
			name = fd->names[idx];
			break;
		}
	}

	/* Tell CBOR encoder where to find the name */
	struct record *record = GET_CBOR_FD_REC(fd);

	record->_record_n._record_n.value = name;
	record->_record_n._record_n.len = len;
	record->_record_n_present = 1;

	/* Makes possible to use same slot for storing r/ri name combination.
	 * No need to increase the name count if an existing name has been used
	 */
	if (path->level < LWM2M_PATH_LEVEL_RESOURCE_INST && name == GET_CBOR_FD_NAME(fd)) {
		fd->name_cnt++;
	}

	return 0;
}

static int put_begin_ri(struct lwm2m_output_context *out, struct lwm2m_obj_path *path)
{
	struct cbor_out_fmt_data *fd = LWM2M_OFD_CBOR(out);
	char *name = GET_CBOR_FD_NAME(fd);
	struct record *record = GET_CBOR_FD_REC(fd);

	if (fd->name_cnt >= CONFIG_LWM2M_RW_SENML_CBOR_RECORDS) {
		LOG_ERR("CONFIG_LWM2M_RW_SENML_CBOR_RECORDS too small");
		return -ENOMEM;
	}

	/* Forms name from resource id and resource instance id */
	int len = snprintk(name, sizeof("65535/999"),
			   "%" PRIu16 "/%" PRIu16 "",
			   path->res_id, path->res_inst_id);

	if (len < sizeof("0/0") - 1) {
		__ASSERT_NO_MSG(false);
		return -EINVAL;
	}

	/* Check if we could use an already existing name
	 * -> latest name slot is used as a scratchpad
	 */
	for (int idx = 0; idx < fd->name_cnt; idx++) {
		if (strncmp(name, fd->names[idx], len) == 0) {
			name = fd->names[idx];
			break;
		}
	}

	/* Tell CBOR encoder where to find the name */
	record->_record_n._record_n.value = name;
	record->_record_n._record_n.len = len;
	record->_record_n_present = 1;

	/* No need to increase the name count if an existing name has been used */
	if (name == GET_CBOR_FD_NAME(fd)) {
		fd->name_cnt++;
	}

	return 0;
}

static int put_name_nth_ri(struct lwm2m_output_context *out, struct lwm2m_obj_path *path)
{
	int ret = 0;

	/* With the first ri the resource name (and ri name) are already in place */
	if (path->res_inst_id > 0) {
		ret = put_begin_ri(out, path);
	}

	return ret;
}

static int put_value(struct lwm2m_output_context *out, struct lwm2m_obj_path *path, int64_t value)
{
	int ret = put_name_nth_ri(out, path);

	if (ret < 0) {
		return ret;
	}

	struct record *record = CONSUME_CBOR_FD_REC(LWM2M_OFD_CBOR(out));

	/* Write the value */
	record->_record_union._record_union_choice = _union_v;
	record->_record_union._union_v._numeric_choice = _numeric_int;
	record->_record_union._union_v._numeric_int = value;
	record->_record_union_present = 1;

	return 0;
}

static int put_s8(struct lwm2m_output_context *out, struct lwm2m_obj_path *path, int8_t value)
{
	return put_value(out, path, value);
}

static int put_s16(struct lwm2m_output_context *out, struct lwm2m_obj_path *path, int16_t value)
{
	return put_value(out, path, value);
}

static int put_s32(struct lwm2m_output_context *out, struct lwm2m_obj_path *path, int32_t value)
{
	return put_value(out, path, value);
}

static int put_s64(struct lwm2m_output_context *out, struct lwm2m_obj_path *path, int64_t value)
{
	return put_value(out, path, value);
}

static int put_time(struct lwm2m_output_context *out, struct lwm2m_obj_path *path, int64_t value)
{
	int ret = put_name_nth_ri(out, path);

	if (ret < 0) {
		return ret;
	}

	struct record *record = CONSUME_CBOR_FD_REC(LWM2M_OFD_CBOR(out));

	/* Write the value */
	record->_record_union._record_union_choice = _union_v;
	record->_record_union._union_v._numeric_choice = _numeric_int;
	record->_record_union._union_v._numeric_int = value;
	record->_record_union_present = 1;

	return 0;
}

static int put_float(struct lwm2m_output_context *out, struct lwm2m_obj_path *path, double *value)
{
	int ret = put_name_nth_ri(out, path);

	if (ret < 0) {
		return ret;
	}

	struct record *record = CONSUME_CBOR_FD_REC(LWM2M_OFD_CBOR(out));

	/* Write the value */
	record->_record_union._record_union_choice = _union_v;
	record->_record_union._union_v._numeric_choice = _numeric_float;
	record->_record_union._union_v._numeric_float = *value;
	record->_record_union_present = 1;

	return 0;
}

static int put_string(struct lwm2m_output_context *out, struct lwm2m_obj_path *path, char *buf,
		      size_t buflen)
{
	int ret = put_name_nth_ri(out, path);

	if (ret < 0) {
		return ret;
	}

	struct record *record = CONSUME_CBOR_FD_REC(LWM2M_OFD_CBOR(out));

	/* Write the value */
	record->_record_union._record_union_choice = _union_vs;
	record->_record_union._union_vs.value = buf;
	record->_record_union._union_vs.len = buflen;
	record->_record_union_present = 1;

	return 0;
}

static int put_bool(struct lwm2m_output_context *out, struct lwm2m_obj_path *path, bool value)
{
	int ret = put_name_nth_ri(out, path);

	if (ret < 0) {
		return ret;
	}

	struct record *record = CONSUME_CBOR_FD_REC(LWM2M_OFD_CBOR(out));

	/* Write the value */
	record->_record_union._record_union_choice = _union_vb;
	record->_record_union._union_vb = value;
	record->_record_union_present = 1;

	return 0;
}

static int put_opaque(struct lwm2m_output_context *out, struct lwm2m_obj_path *path, char *buf,
		      size_t buflen)
{
	int ret = put_name_nth_ri(out, path);

	if (ret < 0) {
		return ret;
	}

	struct record *record = CONSUME_CBOR_FD_REC(LWM2M_OFD_CBOR(out));

	/* Write the value */
	record->_record_union._record_union_choice = _union_vd;
	record->_record_union._union_vd.value = buf;
	record->_record_union._union_vd.len = buflen;
	record->_record_union_present = 1;

	return 0;
}

static int put_objlnk(struct lwm2m_output_context *out, struct lwm2m_obj_path *path,
		      struct lwm2m_objlnk *value)
{
	int32_t value_s32 = (value->obj_id << 16) | value->obj_inst;

	return put_s32(out, path, value_s32);
}

static int get_opaque(struct lwm2m_input_context *in,
			 uint8_t *value, size_t buflen,
			 struct lwm2m_opaque_context *opaque,
			 bool *last_block)
{
	struct cbor_in_fmt_data *fd;

	/* Get the CBOR header only on first read. */
	if (opaque->remaining == 0) {

		fd = engine_get_in_user_data(in);
		if (!fd || !fd->current) {
			return -EINVAL;
		}

		/* TODO: get the opaque data and it's length -
		 * now plain zero
		 */

		opaque->len = fd->current->_record_union._union_vd.len;
		opaque->remaining = fd->current->_record_union._union_vd.len;

		fd->current = NULL;
		goto not_supported;
	}

	return lwm2m_engine_get_opaque_more(in, value, buflen,
					    opaque, last_block);
not_supported:
	return -ENOTSUP;
}

static int get_s32(struct lwm2m_input_context *in, int32_t *value)
{
	struct cbor_in_fmt_data *fd;

	fd = engine_get_in_user_data(in);
	if (!fd || !fd->current) {
		return -EINVAL;
	}

	*value = fd->current->_record_union._union_v._numeric_int;
	fd->current = NULL;

	return 0;
}

static int get_s64(struct lwm2m_input_context *in, int64_t *value)
{
	struct cbor_in_fmt_data *fd;

	fd = engine_get_in_user_data(in);
	if (!fd || !fd->current) {
		return -EINVAL;
	}

	*value = fd->current->_record_union._union_v._numeric_int;
	fd->current = NULL;

	return 0;
}

static int get_float(struct lwm2m_input_context *in, double *value)
{
	struct cbor_in_fmt_data *fd;

	fd = engine_get_in_user_data(in);
	if (!fd || !fd->current) {
		return -EINVAL;
	}

	*value = fd->current->_record_union._union_v._numeric_float;
	fd->current = NULL;

	return 0;
}

static int get_string(struct lwm2m_input_context *in, uint8_t *buf, size_t buflen)
{
	struct cbor_in_fmt_data *fd;
	int len;

	fd = engine_get_in_user_data(in);
	if (!fd || !fd->current) {
		return -EINVAL;
	}

	len = MIN(buflen-1, fd->current->_record_union._union_vs.len);

	memcpy(buf, fd->current->_record_union._union_vs.value, len);
	buf[len] = '\0';

	fd->current = NULL;

	return 0;
}

static int get_objlnk(struct lwm2m_input_context *in,
			 struct lwm2m_objlnk *value)
{
	char objlnk[sizeof("65535:65535")] = {0};
	unsigned long id;
	int ret;

	ret = get_string(in, objlnk, sizeof(objlnk));
	if (ret < 0) {
		return ret;
	}

	value->obj_id = LWM2M_OBJLNK_MAX_ID;
	value->obj_inst = LWM2M_OBJLNK_MAX_ID;

	char *end;
	char *idp = objlnk;

	for (int idx = 0; idx < 2; idx++) {

		errno = 0;
		id = strtoul(idp, &end, 10);

		idp = end;

		if ((id == 0 && errno == ERANGE) || id > 65535) {
			LOG_WRN("decoded id %lu out of range[0..65535]", id);
			return -EBADMSG;
		}

		switch (idx) {
		case 0:
			value->obj_id = id;
			continue;
		case 1:
			value->obj_inst = id;
			continue;
		}
	}

	if (value->obj_inst != LWM2M_OBJLNK_MAX_ID && (value->obj_id == LWM2M_OBJLNK_MAX_ID)) {
		LOG_WRN("decoded obj inst id without obj id");
		return -EBADMSG;
	}

	return ret;
}

static int get_bool(struct lwm2m_input_context *in, bool *value)
{
	struct cbor_in_fmt_data *fd;

	fd = engine_get_in_user_data(in);
	if (!fd || !fd->current) {
		return -EINVAL;
	}

	*value = fd->current->_record_union._union_vb;
	fd->current = NULL;

	return 0;
}

static int do_write_op_item(struct lwm2m_message *msg, struct record *rec)
{
	struct lwm2m_engine_obj_inst *obj_inst = NULL;
	struct lwm2m_engine_obj_field *obj_field;
	struct lwm2m_engine_res *res = NULL;
	struct lwm2m_engine_res_inst *res_inst = NULL;
	int ret;
	uint8_t created = 0U;
	struct cbor_in_fmt_data *fd;

	fd = engine_get_in_user_data(&msg->in);
	if (!fd) {
		return -EINVAL;
	}

	/* Composite op - name with basename */
	if (rec->_record_n_present) {
		char name[sizeof("65535/999")]; /* Null terminated name */
		int len = MIN(sizeof(name) - 1, rec->_record_n._record_n.len);
		char fqn[MAX_RESOURCE_LEN + 1] = {0};

		snprintk(name, len + 1, "%s", rec->_record_n._record_n.value);

		/* Form fully qualified path name */
		snprintk(fqn, sizeof(fqn), "%s%s", fd->basename, name);

		/* Set path on record basis */
		ret = lwm2m_string_to_path(fqn, &msg->path, '/');
		if (ret < 0) {
			__ASSERT_NO_MSG(false);
			return ret;
		}
	}

	fd->current = rec;

	ret = lwm2m_get_or_create_engine_obj(msg, &obj_inst, &created);
	if (ret < 0) {
		return ret;
	}

	ret = lwm2m_engine_validate_write_access(msg, obj_inst, &obj_field);
	if (ret < 0) {
		return ret;
	}

	ret = lwm2m_engine_get_create_res_inst(&msg->path, &res, &res_inst);
	if (ret < 0) {
		return -ENOENT;
	}

	return lwm2m_write_handler(obj_inst, res, res_inst, obj_field, msg);
}

const struct lwm2m_writer senml_cbor_writer = {
	.put_end = put_end,
	.put_begin_oi = put_begin_oi,
	.put_begin_r = put_begin_r,
	.put_begin_ri = put_begin_ri,
	.put_s8 = put_s8,
	.put_s16 = put_s16,
	.put_s32 = put_s32,
	.put_s64 = put_s64,
	.put_time = put_time,
	.put_string = put_string,
	.put_float = put_float,
	.put_bool = put_bool,
	.put_opaque = put_opaque,
	.put_objlnk = put_objlnk,
};

const struct lwm2m_reader senml_cbor_reader = {
	.get_s32 = get_s32,
	.get_s64 = get_s64,
	.get_time = get_s64,
	.get_string = get_string,
	.get_float = get_float,
	.get_bool = get_bool,
	.get_opaque = get_opaque,
	.get_objlnk = get_objlnk,
};

int do_read_op_senml_cbor(struct lwm2m_message *msg)
{
	int ret;
	struct cbor_out_fmt_data *fd = setup_out_fmt_data(msg);

	if (!fd) {
		return -ENOMEM;
	}

	ret = lwm2m_perform_read_op(msg, LWM2M_FORMAT_APP_SENML_CBOR);

	clear_out_fmt_data(msg, fd);

	return ret;
}

static uint8_t parse_composite_read_paths(struct lwm2m_message *msg,
		sys_slist_t *lwm2m_path_list,
		sys_slist_t *lwm2m_path_free_list)
{
	char basename[MAX_RESOURCE_LEN + 1] = {0}; /* to include terminating null */
	char name[MAX_RESOURCE_LEN + 1] = {0}; /* to include terminating null */
	char fqn[MAX_RESOURCE_LEN + 1] = {0};
	struct lwm2m_obj_path path;
	struct cbor_in_fmt_data *fd;
	uint8_t paths = 0;
	uint32_t isize;
	uint_fast8_t dret;
	int len;
	int ret;

	fd = setup_in_fmt_data(msg);
	if (!fd) {
		LOG_ERR("unable to decode composite read paths, out of memory");
		return -ENOMEM;
	}

	dret = cbor_decode_lwm2m_senml(ICTX_BUF_R_REGION(&msg->in), &fd->dcd, &isize);

	if (dret != ZCBOR_SUCCESS) {
		__ASSERT_NO_MSG(false);
		goto out;
	}

	msg->in.offset += isize;

	for (int idx = 0; idx < fd->dcd._lwm2m_senml__record_count; idx++) {

		/* Where to find the basenames and names */
		struct record *record = GET_IN_FD_REC_I(fd, idx);

		/* Set null terminated effective basename */
		if (record->_record_bn_present) {
			len = MIN(sizeof(basename)-1, record->_record_bn._record_bn.len);
			snprintk(basename, len + 1, "%s", record->_record_bn._record_bn.value);
			basename[len] = '\0';
		}

		/* Best effort with read, skip if no proper name is available */
		if (!record->_record_n_present) {
			if (strcmp(basename, "") == 0) {
				continue;
			}
		}

		/* Set null terminated name */
		if (record->_record_n_present) {
			len = MIN(sizeof(name)-1, record->_record_n._record_n.len);
			snprintk(name, len + 1, "%s", record->_record_n._record_n.value);
			name[len] = '\0';
		}

		/* Form fully qualified path name */
		snprintk(fqn, sizeof(fqn), "%s%s", basename, name);

		ret = lwm2m_string_to_path(fqn, &path, '/');

		/* invalid path is forgiven with read */
		if (ret < 0) {
			continue;
		}

		ret = lwm2m_engine_add_path_to_list(lwm2m_path_list, lwm2m_path_free_list, &path);

		if (ret < 0) {
			continue;
		}

		paths++;
	}

out:
	clear_in_fmt_data(msg, fd);

	return paths;
}


int do_composite_read_op_senml_cbor(struct lwm2m_message *msg)
{
	int ret;
	struct cbor_out_fmt_data *fd;
	struct lwm2m_obj_path_list lwm2m_path_list_buf[CONFIG_LWM2M_COMPOSITE_PATH_LIST_SIZE];
	sys_slist_t lwm_path_list;
	sys_slist_t lwm_path_free_list;
	uint8_t len;

	lwm2m_engine_path_list_init(&lwm_path_list,
				    &lwm_path_free_list,
				    lwm2m_path_list_buf,
				    CONFIG_LWM2M_COMPOSITE_PATH_LIST_SIZE);

	/* Parse paths */
	len = parse_composite_read_paths(msg, &lwm_path_list, &lwm_path_free_list);
	if (len == 0) {
		LOG_ERR("No Valid URL at msg");
		return -ESRCH;
	}

	lwm2m_engine_clear_duplicate_path(&lwm_path_list, &lwm_path_free_list);

	fd = setup_out_fmt_data(msg);
	if (!fd) {
		LOG_ERR("unable to encode composite read msg, out of memory");
		return -ENOMEM;
	}

	ret = lwm2m_perform_composite_read_op(msg, LWM2M_FORMAT_APP_SENML_CBOR, &lwm_path_list);

	clear_out_fmt_data(msg, fd);

	return ret;
}


int do_write_op_senml_cbor(struct lwm2m_message *msg)
{
	uint_fast8_t dret;
	int ret = 0;
	uint32_t decoded_sz;
	struct cbor_in_fmt_data *fd;

	/* With block-wise transfer consecutive blocks will not carry the content header -
	 * go directly to the message processing
	 */
	if (msg->in.block_ctx != NULL && msg->in.block_ctx->ctx.current > 0) {
		msg->path.res_id = msg->in.block_ctx->res_id;
		msg->path.level = msg->in.block_ctx->level;

		if (msg->path.level == LWM2M_PATH_LEVEL_RESOURCE_INST) {
			msg->path.res_inst_id = msg->in.block_ctx->res_inst_id;
		}

		return do_write_op_item(msg, NULL);
	}

	fd = setup_in_fmt_data(msg);
	if (!fd) {
		LOG_ERR("unable to decode msg, out of memory");
		return -ENOMEM;
	}

	dret = cbor_decode_lwm2m_senml(ICTX_BUF_R_PTR(&msg->in), ICTX_BUF_R_LEFT_SZ(&msg->in),
					   &fd->dcd, &decoded_sz);

	if (dret == ZCBOR_SUCCESS) {
		msg->in.offset += decoded_sz;

		for (int idx = 0; idx < fd->dcd._lwm2m_senml__record_count; idx++) {

			struct record *rec = &fd->dcd._lwm2m_senml__record[idx];

			/* Basename applies for current and succeeding records */
			if (rec->_record_bn_present) {
				int len = MIN(sizeof(fd->basename) - 1,
					      rec->_record_bn._record_bn.len);

				snprintk(fd->basename, len + 1, "%s",
					 rec->_record_bn._record_bn.value);
			}

			ret = do_write_op_item(msg, rec);

			/* Write isn't supposed to fail */
			if (ret < 0) {
				break;
			}
		}
	} else {
		ret = -EBADMSG;
	}

	clear_in_fmt_data(msg, fd);

	return ret < 0 ?  ret : decoded_sz;
}

int do_composite_observe_parse_path_senml_cbor(struct lwm2m_message *msg,
					       sys_slist_t *lwm2m_path_list,
					       sys_slist_t *lwm2m_path_free_list)
{
	uint16_t original_offset;
	uint8_t len;

	original_offset = msg->in.offset;

	/* Parse paths */
	len = parse_composite_read_paths(msg, lwm2m_path_list, lwm2m_path_free_list);

	if (len == 0) {
		LOG_ERR("No Valid URL at msg");
		return -ESRCH;
	}

	msg->in.offset = original_offset;
	return 0;
}

int do_send_op_senml_cbor(struct lwm2m_message *msg, sys_slist_t *lwm2m_path_list)
{
	int ret;
	struct cbor_out_fmt_data *fd = setup_out_fmt_data(msg);

	if (!fd) {
		LOG_ERR("Unable to complete SEND op, out of memory");
		return -ENOMEM;
	}

	ret = lwm2m_perform_composite_read_op(msg, LWM2M_FORMAT_APP_SENML_CBOR, lwm2m_path_list);

	clear_out_fmt_data(msg, fd);

	return ret;
}
