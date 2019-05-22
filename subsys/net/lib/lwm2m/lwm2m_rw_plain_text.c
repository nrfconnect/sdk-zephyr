/*
 * Copyright (c) 2017 Linaro Limited
 * Copyright (c) 2018-2019 Foundries.io
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Copyright (c) 2015, Yanzi Networks AB.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Original Authors:
 *         Joakim Eriksson <joakime@sics.se>
 *         Niclas Finne <nfi@sics.se>
 */

/*
 * Zephyr Contribution by Michael Scott <michael.scott@linaro.org>
 * - Zephyr code style changes / code cleanup
 * - Move to Zephyr APIs where possible
 * - Convert to Zephyr int/uint types
 * - Remove engine dependency (replace with writer context)
 * - Add write int64 function
 */

/*
 * TODO:
 * - Type cleanups
 * - Cleanup integer parsing
 */

#define LOG_MODULE_NAME net_lwm2m_plain_text
#define LOG_LEVEL CONFIG_LWM2M_LOG_LEVEL

#include <logging/log.h>
LOG_MODULE_REGISTER(LOG_MODULE_NAME);

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "lwm2m_object.h"
#include "lwm2m_rw_plain_text.h"
#include "lwm2m_engine.h"

/* some temporary buffer space for format conversions */
static char pt_buffer[42]; /* can handle float64 format */

size_t plain_text_put_format(struct lwm2m_output_context *out,
			     const char *format, ...)
{
	va_list vargs;
	int n;

	va_start(vargs, format);
	n = vsnprintk(pt_buffer, sizeof(pt_buffer), format, vargs);
	va_end(vargs);
	if (n < 0) {
		/* TODO: generate an error? */
		return 0;
	}

	n = strlen(pt_buffer);
	if (buf_append(CPKT_BUF_WRITE(out->out_cpkt), pt_buffer, n) < 0) {
		return 0;
	}

	return (size_t)n;
}

static size_t put_s32(struct lwm2m_output_context *out,
		      struct lwm2m_obj_path *path, s32_t value)
{
	return plain_text_put_format(out, "%d", value);
}

static size_t put_s8(struct lwm2m_output_context *out,
		     struct lwm2m_obj_path *path, s8_t value)
{
	return plain_text_put_format(out, "%d", value);
}

static size_t put_s16(struct lwm2m_output_context *out,
		      struct lwm2m_obj_path *path, s16_t value)
{
	return plain_text_put_format(out, "%d", value);
}

static size_t put_s64(struct lwm2m_output_context *out,
		      struct lwm2m_obj_path *path, s64_t value)
{
	return plain_text_put_format(out, "%lld", value);
}

static size_t put_float32fix(struct lwm2m_output_context *out,
			     struct lwm2m_obj_path *path,
			     float32_value_t *value)
{
	return plain_text_put_format(out, "%d.%d",
				     value->val1, value->val2);
}

static size_t put_float64fix(struct lwm2m_output_context *out,
			     struct lwm2m_obj_path *path,
			     float64_value_t *value)
{
	return plain_text_put_format(out, "%lld.%lld",
				     value->val1, value->val2);
}

static size_t put_string(struct lwm2m_output_context *out,
			 struct lwm2m_obj_path *path,
			 char *buf, size_t buflen)
{
	if (buf_append(CPKT_BUF_WRITE(out->out_cpkt), buf, buflen) < 0) {
		return 0;
	}

	return buflen;
}

static size_t put_bool(struct lwm2m_output_context *out,
		       struct lwm2m_obj_path *path,
		       bool value)
{
	if (value) {
		return plain_text_put_format(out, "%u", 1);
	} else {
		return plain_text_put_format(out, "%u", 0);
	}
}

static int get_length_left(struct lwm2m_input_context *in)
{
	return in->in_cpkt->offset - in->offset;
}

static size_t plain_text_read_number(struct lwm2m_input_context *in,
				     s64_t *value1,
				     s64_t *value2,
				     bool accept_sign, bool accept_dot)
{
	s64_t *counter = value1;
	int i = 0;
	bool neg = false;
	bool dot_found = false;
	u8_t tmp;

	/* initialize values to 0 */
	*value1 = 0;
	if (value2) {
		*value2 = 0;
	}

	while (in->offset < in->in_cpkt->offset) {
		if (buf_read_u8(&tmp, CPKT_BUF_READ(in->in_cpkt),
				&in->offset) < 0) {
			break;
		}

		if (tmp == '-' && accept_sign && i == 0) {
			neg = true;
		} else if (tmp == '.' && i > 0 && accept_dot && !dot_found &&
			   value2) {
			dot_found = true;
			counter = value2;
		} else if (isdigit(tmp)) {
			*counter = *counter * 10 + (tmp - '0');
		} else {
			/* anything else stop reading */
			in->offset--;
			break;
		}

		i++;
	}

	if (neg) {
		*value1 = -*value1;
	}

	return i;
}

static size_t get_s32(struct lwm2m_input_context *in, s32_t *value)
{
	s64_t tmp = 0;
	size_t len = 0;

	len = plain_text_read_number(in, &tmp, NULL, true, false);
	if (len > 0) {
		*value = (s32_t)tmp;
	}

	return len;
}

static size_t get_s64(struct lwm2m_input_context *in, s64_t *value)
{
	return plain_text_read_number(in, value, NULL, true, false);
}

static size_t get_string(struct lwm2m_input_context *in,
			 u8_t *value, size_t buflen)
{
	u16_t in_len = get_length_left(in);

	if (in_len > buflen) {
		/* TODO: generate warning? */
		in_len = buflen - 1;
	}

	if (buf_read(value, in_len, CPKT_BUF_READ(in->in_cpkt),
		     &in->offset) < 0) {
		value[0] = '\0';
		return 0;
	}

	value[in_len] = '\0';
	return (size_t)in_len;
}

static size_t get_float32fix(struct lwm2m_input_context *in,
			     float32_value_t *value)
{
	s64_t tmp1, tmp2;
	size_t len = 0;

	len = plain_text_read_number(in, &tmp1, &tmp2, true, true);
	if (len > 0) {
		value->val1 = (s32_t)tmp1;
		value->val2 = (s32_t)tmp2;
	}

	return len;
}

static size_t get_float64fix(struct lwm2m_input_context *in,
			     float64_value_t *value)
{
	return plain_text_read_number(in, &value->val1, &value->val2,
				      true, true);
}

static size_t get_bool(struct lwm2m_input_context *in,
		       bool *value)
{
	u8_t tmp;

	if (buf_read_u8(&tmp, CPKT_BUF_READ(in->in_cpkt), &in->offset) < 0) {
		return 0;
	}

	if (tmp == '1' || tmp == '0') {
		*value = (tmp == '1') ? true : false;
		return 1;
	}

	return 0;
}

static size_t get_opaque(struct lwm2m_input_context *in,
			 u8_t *value, size_t buflen, bool *last_block)
{
	in->opaque_len = get_length_left(in);
	return lwm2m_engine_get_opaque_more(in, value, buflen, last_block);
}

const struct lwm2m_writer plain_text_writer = {
	.put_s8 = put_s8,
	.put_s16 = put_s16,
	.put_s32 = put_s32,
	.put_s64 = put_s64,
	.put_string = put_string,
	.put_float32fix = put_float32fix,
	.put_float64fix = put_float64fix,
	.put_bool = put_bool,
};

const struct lwm2m_reader plain_text_reader = {
	.get_s32 = get_s32,
	.get_s64 = get_s64,
	.get_string = get_string,
	.get_float32fix = get_float32fix,
	.get_float64fix = get_float64fix,
	.get_bool = get_bool,
	.get_opaque = get_opaque,
};

int do_read_op_plain_text(struct lwm2m_engine_obj *obj,
			  struct lwm2m_message *msg, int content_format)
{
	/* Plain text can only return single resource */
	if (msg->path.level != 3U) {
		return -EPERM; /* NOT_ALLOWED */
	}

	return lwm2m_perform_read_op(obj, msg, content_format);
}

int do_write_op_plain_text(struct lwm2m_engine_obj *obj,
			   struct lwm2m_message *msg)
{
	struct lwm2m_engine_obj_inst *obj_inst = NULL;
	struct lwm2m_engine_obj_field *obj_field;
	struct lwm2m_engine_res_inst *res = NULL;
	int ret, i;
	u8_t created = 0U;

	ret = lwm2m_get_or_create_engine_obj(msg, &obj_inst, &created);
	if (ret < 0) {
		return ret;
	}

	obj_field = lwm2m_get_engine_obj_field(obj, msg->path.res_id);
	if (!obj_field) {
		return -ENOENT;
	}

	if (!LWM2M_HAS_PERM(obj_field, LWM2M_PERM_W)) {
		return -EPERM;
	}

	if (!obj_inst->resources || obj_inst->resource_count == 0U) {
		return -EINVAL;
	}

	for (i = 0; i < obj_inst->resource_count; i++) {
		if (obj_inst->resources[i].res_id == msg->path.res_id) {
			res = &obj_inst->resources[i];
			break;
		}
	}

	if (!res) {
		return -ENOENT;
	}

	msg->path.level = 3U;
	return lwm2m_write_handler(obj_inst, res, obj_field, msg);
}
