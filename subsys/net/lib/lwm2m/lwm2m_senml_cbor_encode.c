/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */
/*
 * Generated using zcbor version 0.4.0
 * https://github.com/zephyrproject-rtos/zcbor
 * Generated with a --default-max-qty of 99
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include "zcbor_encode.h"
#include "lwm2m_senml_cbor_encode.h"
#include "lwm2m_senml_cbor_types.h"

static bool encode_repeated_record_bn(zcbor_state_t *state, const struct record_bn *input)
{
	zcbor_print("%s\r\n", __func__);

	bool tmp_result = ((((zcbor_int32_put(state, (-2)))) &&
			    (zcbor_tstr_encode(state, (&(*input)._record_bn)))));

	if (!tmp_result)
		zcbor_trace();

	return tmp_result;
}

static bool encode_repeated_record_n(zcbor_state_t *state, const struct record_n *input)
{
	zcbor_print("%s\r\n", __func__);

	bool tmp_result = ((((zcbor_uint32_put(state, (0)))) &&
			    (zcbor_tstr_encode(state, (&(*input)._record_n)))));

	if (!tmp_result)
		zcbor_trace();

	return tmp_result;
}

static bool encode_repeated_record_union(zcbor_state_t *state, const struct record_union_ *input)
{
	zcbor_print("%s\r\n", __func__);

	bool tmp_result =
		(((((*input)._record_union_choice == _union_vi) ?
			(((zcbor_uint32_put(state, (2)))) &&
			((((*input)._union_vi >= -9223372036854775807LL) &&
			((*input)._union_vi <= 9223372036854775807LL)) ||
			(zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false)) &&
			(zcbor_int64_encode(state, (&(*input)._union_vi)))) :
		(((*input)._record_union_choice == _union_vf) ?
			(((zcbor_uint32_put(state, (2)))) &&
			(zcbor_float64_encode(state, (&(*input)._union_vf)))) :
		(((*input)._record_union_choice == _union_vs) ?
			(((zcbor_uint32_put(state, (3)))) &&
			(zcbor_tstr_encode(state, (&(*input)._union_vs)))) :
		(((*input)._record_union_choice == _union_vb) ?
			(((zcbor_uint32_put(state, (4)))) &&
			(zcbor_bool_encode(state, (&(*input)._union_vb)))) :
		(((*input)._record_union_choice == _union_vd) ?
			(((zcbor_uint32_put(state, (8)))) &&
			(zcbor_bstr_encode(state, (&(*input)._union_vd)))) : false)))))));

	if (!tmp_result)
		zcbor_trace();

	return tmp_result;
}

static bool encode_value(zcbor_state_t *state, const struct value_ *input)
{
	zcbor_print("%s\r\n", __func__);

	bool tmp_result = ((
		(((*input)._value_choice == _value_tstr) ?
			((zcbor_tstr_encode(state, (&(*input)._value_tstr)))) :
		(((*input)._value_choice == _value_bstr) ?
			((zcbor_bstr_encode(state, (&(*input)._value_bstr)))) :
		(((*input)._value_choice == _value_int) ?
			(((((*input)._value_int >= -9223372036854775807LL) &&
			((*input)._value_int <= 9223372036854775807LL)) ||
			(zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false)) &&
			(zcbor_int64_encode(state, (&(*input)._value_int)))) :
		(((*input)._value_choice == _value_float) ?
			((zcbor_float64_encode(state, (&(*input)._value_float)))) :
		(((*input)._value_choice == _value_bool) ?
			((zcbor_bool_encode(state, (&(*input)._value_bool)))) : false)))))));

	if (!tmp_result)
		zcbor_trace();

	return tmp_result;
}

static bool encode_key_value_pair(zcbor_state_t *state, const struct key_value_pair *input)
{
	zcbor_print("%s\r\n", __func__);

	bool tmp_result = ((((zcbor_int32_encode(state, (&(*input)._key_value_pair_key)))) &&
			    (encode_value(state, (&(*input)._key_value_pair)))));

	if (!tmp_result)
		zcbor_trace();

	return tmp_result;
}

static bool encode_repeated_record__key_value_pair(zcbor_state_t *state,
						   const struct record__key_value_pair *input)
{
	zcbor_print("%s\r\n", __func__);

	bool tmp_result = (((encode_key_value_pair(state, (&(*input)._record__key_value_pair)))));

	if (!tmp_result)
		zcbor_trace();

	return tmp_result;
}

static bool encode_record(zcbor_state_t *state, const struct record *input)
{
	zcbor_print("%s\r\n", __func__);

	int max_keys = ARRAY_SIZE(input->_record__key_value_pair);

	bool tmp_result = (((zcbor_map_start_encode(state, max_keys) &&
			     ((zcbor_present_encode(&((*input)._record_bn_present),
						    (zcbor_encoder_t *)encode_repeated_record_bn,
						    state, (&(*input)._record_bn)) &&
			       zcbor_present_encode(&((*input)._record_n_present),
						    (zcbor_encoder_t *)encode_repeated_record_n,
						    state, (&(*input)._record_n)) &&
			       zcbor_present_encode(&((*input)._record_union_present),
						    (zcbor_encoder_t *)encode_repeated_record_union,
						    state, (&(*input)._record_union)) &&
			       zcbor_multi_encode_minmax(
				       0, max_keys,
				       &(*input)._record__key_value_pair_count,
				       (zcbor_encoder_t *)encode_repeated_record__key_value_pair,
				       state, (&(*input)._record__key_value_pair),
				       sizeof(struct record__key_value_pair))) ||
			      (zcbor_list_map_end_force_encode(state), false)) &&
			     zcbor_map_end_encode(state, max_keys))));

	if (!tmp_result)
		zcbor_trace();

	return tmp_result;
}

static bool encode_lwm2m_senml(zcbor_state_t *state, const struct lwm2m_senml *input)
{
	zcbor_print("%s\r\n", __func__);
	size_t max_records = ARRAY_SIZE(input->_lwm2m_senml__record);

	bool tmp_result =
		(((zcbor_list_start_encode(state, max_records) &&
		   ((zcbor_multi_encode_minmax(1, max_records, &(*input)._lwm2m_senml__record_count,
					       (zcbor_encoder_t *)encode_record, state,
					       (&(*input)._lwm2m_senml__record),
					       sizeof(struct record))) ||
		    (zcbor_list_map_end_force_encode(state), false)) &&
		   zcbor_list_end_encode(state, max_records))));

	if (!tmp_result)
		zcbor_trace();

	return tmp_result;
}

int cbor_encode_lwm2m_senml(uint8_t *payload, size_t payload_len,
				     const struct lwm2m_senml *input, size_t *payload_len_out)
{
	zcbor_state_t states[5];

	zcbor_new_state(states, sizeof(states) / sizeof(zcbor_state_t), payload, payload_len, 1);

	bool ret = encode_lwm2m_senml(states, input);

	if (ret && (payload_len_out != NULL)) {
		*payload_len_out = MIN(payload_len, (size_t)states[0].payload - (size_t)payload);
	}

	if (!ret) {
		int ret = zcbor_pop_error(states);

		return (ret == ZCBOR_SUCCESS) ? ZCBOR_ERR_UNKNOWN : ret;
	}
	return ZCBOR_SUCCESS;
}
