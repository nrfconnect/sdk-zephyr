/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_MODULE_NAME net_lwm2m_cbor
#define LOG_LEVEL CONFIG_LWM2M_LOG_LEVEL

#include <logging/log.h>
#include <sys/util.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
LOG_MODULE_REGISTER(LOG_MODULE_NAME);

#include "cbor_common.h"
#include "cbor_decode.h"
#include "cbor_encode.h"
#include "lwm2m_object.h"
#include "lwm2m_rw_cbor.h"
#include "lwm2m_engine.h"
#include "lwm2m_util.h"

#define CPKT_CBOR_W_SZ(pos, cpkt) \
	((size_t)(pos) - (size_t)(cpkt)->data - (size_t)(cpkt)->offset)

#define ICTX_CBOR_R_SZ(pos, ictx) \
		((size_t)pos - (size_t)(ictx)->in_cpkt->data - (ictx)->offset)

enum  cbor_tag { /* www.iana.org/assignments/cbor-tags/cbor-tags.xhtml */
	CBOR_TAG_TIME_TSTR =    0, /* text string	Standard date/time string */
	CBOR_TAG_TIME_NUM =     1, /* integer or float	Epoch-based date/time */
	CBOR_TAG_UBIGNUM_BSTR = 2, /* byte string	Unsigned bignum */
	CBOR_TAG_BIGNUM_BSTR =  3, /* byte string	Negative bignum */
	CBOR_TAG_DECFRAC_ARR =  4, /* array		Decimal fraction */
	CBOR_TAG_BIGFLOAT_ARR = 5, /* array		Bigfloat */
	CBOR_TAG_2BASE64URL =  21, /* (any)		Expected conversion to base64url encoding */
	CBOR_TAG_2BASE64 =     22, /* (any)		Expected conversion to base64 encoding */
	CBOR_TAG_2BASE16 =     23, /* (any)		Expected conversion to base16 encoding */
	CBOR_TAG_BSTR =        24, /* byte string	Encoded CBOR data item */
	CBOR_TAG_URI_TSTR =    32, /* text string	URI */
	CBOR_TAG_BASE64URL_TSTR = 33, /* text string	base64url */
	CBOR_TAG_BASE64_TSTR =    34, /* text string	base64 */
	CBOR_TAG_MIME_TSTR =      36, /* text string	MIME message */
	CBOR_TAG_CBOR =        55799, /* (any)	Self-described CBOR */
};

static int put_time(struct lwm2m_output_context *out,
		      struct lwm2m_obj_path *path, int64_t value)
{
	/* CBOR time output format is unspecified but SenML CBOR uses string format.
	 * Let's stick into the same format with plain CBOR
	 */
	struct tm pTime;

	if (gmtime_r((time_t *)&value, &pTime) != &pTime) {
		LOG_WRN("unable to convert from secs since Epoch to a date/time construct");
		return -EINVAL;
	}

	/* Time construct to a string. Time in UTC, offset to local time not known */
	char time_str[sizeof("1970-01-01T00:00:00-00:00")] = {0};
	int time_str_len = snprintk(time_str, sizeof(time_str),
			"%04d-%02d-%02dT%02d:%02d:%02d-00:00",
			pTime.tm_year+1900,
			pTime.tm_mon+1,
			pTime.tm_mday,
			pTime.tm_hour,
			pTime.tm_min,
			pTime.tm_sec);

	if (time_str_len < 0 || time_str_len > sizeof(time_str)-1) {
		LOG_WRN("unable to form a date/time string");
		return -EINVAL;
	}

	cbor_state_t states[1];

	new_state(states, 1, CPKT_BUF_W_REGION(out->out_cpkt), 1);

	/* Are tags required? Spec V1.1 leaves this unspecified but some servers require tags */
	bool ret = tag_encode(states, CBOR_TAG_TIME_TSTR);

	if (!ret) {
		LOG_WRN("unable to encode date/time string tag");
		return -ENOMEM;
	}

	int cbor_tag_sz = CPKT_CBOR_W_SZ(states[0].payload, out->out_cpkt);

	out->out_cpkt->offset += cbor_tag_sz;

	ret  = tstrx_encode(states,
			&(cbor_string_type_t){.value = time_str, .len = strlen(time_str)});
	if (!ret) {
		LOG_WRN("unable to encode date/time string");
		return -ENOMEM;
	}

	int cbor_str_sz = CPKT_CBOR_W_SZ(states[0].payload, out->out_cpkt);

	out->out_cpkt->offset += cbor_str_sz;

	return cbor_tag_sz + cbor_str_sz;
}

static int put_s64(struct lwm2m_output_context *out,
		      struct lwm2m_obj_path *path, int64_t value)
{
	cbor_state_t states[1];
	int payload_len = 0;

	/* TODO: remove this check once the CBOR library supports 64-bit values */
	if (value != (value & 0xFFFFFFFF)) {
		LOG_WRN("64-bit values are not supported");
		return -EINVAL;
	}


	new_state(states, 1, CPKT_BUF_W_REGION(out->out_cpkt), 1);

	/* TODO: use intx64_encode once/if available */
	uint32_t valueu32 = (uint32_t)value;
	bool ret = uintx32_encode(states, &valueu32);

	if (ret) {
		payload_len = CPKT_CBOR_W_SZ(states[0].payload, out->out_cpkt);
		out->out_cpkt->offset += payload_len;
	} else {
		payload_len = -ENOMEM;
	}

	return payload_len;
}

static int put_s32(struct lwm2m_output_context *out,
		      struct lwm2m_obj_path *path, int32_t value)
{
	cbor_state_t states[1];
	int payload_len = 0;

	new_state(states, 1, CPKT_BUF_W_REGION(out->out_cpkt), 1);

	bool ret = intx32_encode(states, &value);

	if (ret) {
		payload_len = CPKT_CBOR_W_SZ(states[0].payload, out->out_cpkt);
		out->out_cpkt->offset += payload_len;
	} else {
		payload_len = -ENOMEM;
	}

	return payload_len;
}

static int put_s16(struct lwm2m_output_context *out,
		      struct lwm2m_obj_path *path, int16_t value)
{
	return put_s32(out, path, value);
}

static int put_s8(struct lwm2m_output_context *out,
		     struct lwm2m_obj_path *path, int8_t value)
{
	return put_s32(out, path, value);
}

static int put_string(struct lwm2m_output_context *out,
			 struct lwm2m_obj_path *path,
			 char *buf, size_t buflen)
{
	cbor_state_t states[1];
	int payload_len = 0;

	new_state(states, 1, CPKT_BUF_W_REGION(out->out_cpkt), 1);

	bool ret = tstrx_encode(states, &(cbor_string_type_t){.value = buf, .len = buflen});

	if (ret) {
		payload_len = CPKT_CBOR_W_SZ(states[0].payload, out->out_cpkt);
		out->out_cpkt->offset += payload_len;
	} else {
		payload_len = -ENOMEM;
	}

	return payload_len;
}

static int put_opaque(struct lwm2m_output_context *out,
			 struct lwm2m_obj_path *path,
			 char *buf, size_t buflen)
{
	cbor_state_t states[1];
	int payload_len = 0;

	new_state(states, 1, CPKT_BUF_W_REGION(out->out_cpkt), 1);

	bool ret = bstrx_encode(states, &(cbor_string_type_t){.value = buf, .len = buflen});

	if (ret) {
		payload_len = CPKT_CBOR_W_SZ(states[0].payload, out->out_cpkt);
		out->out_cpkt->offset += payload_len;
	} else {
		payload_len = -ENOMEM;
	}

	return payload_len;
}

static int put_bool(struct lwm2m_output_context *out,
		       struct lwm2m_obj_path *path,
		       bool value)
{
	cbor_state_t states[1];
	int payload_len = 0;

	new_state(states, 1, CPKT_BUF_W_REGION(out->out_cpkt), 1);

	bool ret = boolx_encode(states, &value);

	if (ret) {
		payload_len = CPKT_CBOR_W_SZ(states[0].payload, out->out_cpkt);
		out->out_cpkt->offset += payload_len;
	} else {
		payload_len = -ENOMEM;
	}

	return payload_len;
}

static int put_objlnk(struct lwm2m_output_context *out,
			 struct lwm2m_obj_path *path,
			 struct lwm2m_objlnk *value)
{
	char objlnk[sizeof("65535:65535")] = {0};

	snprintk(objlnk, sizeof(objlnk), "%" PRIu16":%" PRIu16"", value->obj_id, value->obj_inst);

	return put_string(out, path, objlnk, strlen(objlnk)+1);
}

static int get_s64(struct lwm2m_input_context *in, int64_t *value)
{
	cbor_state_t states[1];
	/* TODO: CBOR data item header + the variable
	 * 1 + 8 once 64-bit values are supported, 1 + 4 for now for uint32s.
	 */
	new_state(states, 1, ICTX_BUF_R_PTR(in), MIN(5, ICTX_BUF_R_LEFT_SZ(in)), 1);

	if (!uintx32_decode(states, (uint32_t *)value)) {
		LOG_WRN("unable to decode a 64-bit(uint32) integer value");
		return -EBADMSG;
	}

	int len = ICTX_CBOR_R_SZ(states[0].payload, in);

	in->offset += len;

	/* TODO: length of the original binary data or the decoded value? */
	return len;
}

static int get_s32(struct lwm2m_input_context *in, int32_t *value)
{
	cbor_state_t states[1];

	new_state(states, 1, ICTX_BUF_R_PTR(in), MIN(5, ICTX_BUF_R_LEFT_SZ(in)), 1);

	if (!intx32_decode(states, value)) {
		LOG_WRN("unable to decode a 32-bit integer value");
		return -EBADMSG;
	}

	int len = ICTX_CBOR_R_SZ(states[0].payload, in);

	in->offset += len;

	return len;
}

static int get_string(struct lwm2m_input_context *in,
			 uint8_t *value, size_t buflen)
{
	cbor_state_t states[1];
	cbor_string_type_t hndl = {.value = value, .len = buflen};

	new_state(states, 1, ICTX_BUF_R_REGION(in), 1);

	if (!tstrx_decode(states, &hndl)) {
		LOG_WRN("unable to decode a string");
		return -EBADMSG;
	}

	int len = ICTX_CBOR_R_SZ(states[0].payload, in);

	in->offset += len;

	return len;
}

static int get_time(struct lwm2m_input_context *in, int64_t *value)
{
	cbor_state_t states[1];
	uint32_t tag;
	int cbor_tag_sz = 0;
	char time_str[sizeof("4294967295")] = {0};
	cbor_string_type_t hndl = {.value = time_str, .len = sizeof(time_str) - 1};
	int data_len;

	new_state(states, 1, ICTX_BUF_R_REGION(in), 1);

	bool ret = tag_decode(states, &tag);

	if (ret) {
		cbor_tag_sz = ICTX_CBOR_R_SZ(states[0].payload, in);

		in->offset += cbor_tag_sz;

		switch (tag) {
		case CBOR_TAG_TIME_TSTR:
			goto tstring;
		case CBOR_TAG_TIME_NUM:
			goto tnumerical;
		default:
			LOG_WRN("expected tagged date/time, got tag %" PRIu32"", tag);
			return -EBADMSG;
		}
	} else { /* TODO: Tags are optional, right? */
		;
	}

tstring:
	if (!tstrx_decode(states, &hndl)) {
		/* Might be numerical */
		goto tnumerical;
	}

	char *end;
	unsigned long ival = strtoul(time_str, &end, 0);

	errno = 0;

	if (errno == ERANGE) {
		LOG_WRN("unable to decode a date/time string");
		errno = 0;
		return -EBADMSG;
	}

	*value = ival;

	goto advance;

tnumerical:
	if (!uintx32_decode(states, (uint32_t *)value)) {
		LOG_WRN("unable to decode seconds since Epoch");
		return -EBADMSG;
	}

advance:
	data_len = ICTX_CBOR_R_SZ(states[0].payload, in);
	in->offset += data_len;

	return cbor_tag_sz + data_len;
}

static int get_bool(struct lwm2m_input_context *in,
		       bool *value)
{
	cbor_state_t states[1];

	new_state(states, 1, ICTX_BUF_R_REGION(in), 1);

	if (!boolx_decode(states, value)) {
		LOG_WRN("unable to decode a boolean value");
		return -EBADMSG;
	}

	int len = ICTX_CBOR_R_SZ(states[0].payload, in);

	in->offset += len;

	return len;
}

static int get_opaque(struct lwm2m_input_context *in,
			 uint8_t *value, size_t buflen,
			 struct lwm2m_opaque_context *opaque,
			 bool *last_block)
{
	cbor_state_t states[1];
	cbor_string_type_t info = {0};
	int ret;

	/* Get the CBOR header only on first read. */
	if (opaque->remaining == 0) {

		/* FIXME: When data is spread between non-contiguous blocks this range setting will
		 * break the whole thing. Luckily we get the data length out though.
		 */
		new_state(states, 1, ICTX_BUF_R_REGION(in), 1);

		ret = bstrx_cbor_start_decode(states, &info);

		/* FIXME: Uncomment once the range check is working */
		(void)ret;
		/*
		 * if(!ret) {
		 *	LOG_WRN("unable to decode opaque data header");
		 *	return -EBADMSG;
		 * }
		 */

		opaque->len = info.len;
		opaque->remaining = info.len;

		/* FIXME: Workaround for the bad range check.
		 * Now it's CBOR data item header plus possible extended count
		 */
		if (info.len == 0) {
			LOG_WRN("unable to decode opaque data header");
			return -EBADMSG;
		} else if (info.len <= 23) {
			in->offset += 1;
		} else if (info.len <= UINT8_MAX) {
			in->offset += 1 + 1;
		} else if (info.len <= UINT16_MAX) {
			in->offset += 1 + 2;
		} else if (info.len <= UINT32_MAX) {
			in->offset += 1 + 4;
		}
	}

	return lwm2m_engine_get_opaque_more(in, value, buflen,
					    opaque, last_block);
}

static int get_objlnk(struct lwm2m_input_context *in,
			 struct lwm2m_objlnk *value)
{
	char objlnk[sizeof("65535:65535")] = {0};

	value->obj_id = LWM2M_OBJLNK_NULL;
	value->obj_inst = LWM2M_OBJLNK_NULL;

	int len = get_string(in, objlnk, sizeof(objlnk)-1);

	char *end;
	char *idp = objlnk;

	errno = 0;

	for (int idx = 0; idx < 2; idx++) {
		unsigned long id = strtoul(idp, &end, 10);

		idp = end;

		if (errno == ERANGE || id > 65535) {
			LOG_WRN("decoded id %lu out of range[0..65535]", id);
			errno = 0;
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

	if (value->obj_inst != LWM2M_OBJLNK_NULL && (value->obj_inst == LWM2M_OBJLNK_NULL)) {
		LOG_WRN("decoded obj inst id without obj id");
		return -EBADMSG;
	}

	return len;
}

const struct lwm2m_writer cbor_writer = {
	.put_s8 = put_s8,
	.put_s16 = put_s16,
	.put_s32 = put_s32,
	.put_s64 = put_s64,
	.put_string = put_string,
	.put_float = NULL,
	.put_time = put_time,
	.put_bool = put_bool,
	.put_opaque = put_opaque,
	.put_objlnk = put_objlnk,
};

const struct lwm2m_reader cbor_reader = {
	.get_s32 = get_s32,
	.get_s64 = get_s64,
	.get_time = get_time,
	.get_string = get_string,
	.get_float = NULL,
	.get_bool = get_bool,
	.get_opaque = get_opaque,
	.get_objlnk = get_objlnk,
};

int do_read_op_cbor(struct lwm2m_message *msg)
{
	/* Can only return single resource */
	if (msg->path.level < 3U) {
		return -EPERM;
	} else if (msg->path.level > 4U) {
		return -ENOENT;
	}

	return lwm2m_perform_read_op(msg, LWM2M_FORMAT_APP_CBOR);
}

int do_write_op_cbor(struct lwm2m_message *msg)
{
	struct lwm2m_engine_obj_inst *obj_inst = NULL;
	struct lwm2m_engine_obj_field *obj_field;
	struct lwm2m_engine_res *res = NULL;
	struct lwm2m_engine_res_inst *res_inst = NULL;
	int ret;
	uint8_t created = 0U;

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

	if (msg->path.level < 3) {
		msg->path.level = 3U;
	}

	return lwm2m_write_handler(obj_inst, res, res_inst, obj_field, msg);
}
