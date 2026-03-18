/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT bosch_bmi270

#include <errno.h>
#include <stddef.h>

#include <zephyr/drivers/sensor.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/util.h>
#include <zephyr/logging/log.h>

#include "bmi270_decoder.h"

LOG_MODULE_REGISTER(bmi270_decoder, CONFIG_SENSOR_LOG_LEVEL);

/*
 * BMI270 FIFO header byte layout
 *   Bits [7:6] = fh_mode  (10 = regular, 01 = control)
 *   Bits [5:2] = fh_parm  (sensor presence for regular; opcode for control)
 *   Bits [1:0] = fh_ext   (INT tag bits)
 *
 * fh_parm for regular frames:
 *   bit 0 = ACC, bit 1 = GYR, bit 2 = AUX, bit 3 = reserved
 */
#define BMI270_FIFO_HDR_MODE(h)             (((h) >> 6) & 0x03)
#define BMI270_FIFO_HDR_PARM(h)             (((h) >> 2) & 0x0F)
#define BMI270_FIFO_MODE_REGULAR            0x02
#define BMI270_FIFO_MODE_CONTROL            0x01
#define BMI270_FIFO_PARM_ACC                BIT(0)
#define BMI270_FIFO_PARM_GYR                BIT(1)
#define BMI270_FIFO_CTRL_PARM_SKIP_FRAME    0x00
#define BMI270_FIFO_CTRL_PARM_SENSORTIME    0x01
#define BMI270_FIFO_CTRL_PARM_CONFIG_CHANGE 0x02

#define BMI270_FIFO_SENSOR_BYTES           6
#define BMI270_FIFO_PAYLOAD_ACC_GYR        12
#define BMI270_FIFO_CTRL_LEN_SKIP_FRAME    2
#define BMI270_FIFO_CTRL_LEN_SENSORTIME    4
#define BMI270_FIFO_CTRL_LEN_CONFIG_CHANGE 5

#define BMI270_ACC_SHIFT_BASE 5
#define BMI270_GYR_SHIFT_BASE 6

static inline uint8_t bmi270_fifo_control_frame_size(uint8_t parm)
{
	switch (parm) {
	case BMI270_FIFO_CTRL_PARM_SKIP_FRAME:
		return BMI270_FIFO_CTRL_LEN_SKIP_FRAME;
	case BMI270_FIFO_CTRL_PARM_SENSORTIME:
		return BMI270_FIFO_CTRL_LEN_SENSORTIME;
	case BMI270_FIFO_CTRL_PARM_CONFIG_CHANGE:
		return BMI270_FIFO_CTRL_LEN_CONFIG_CHANGE;
	default:
		return BMI270_FIFO_CTRL_LEN_SKIP_FRAME;
	}
}

static inline uint32_t bmi270_sample_period_ns(const struct bmi270_decoder_header *header,
					       enum sensor_channel chan)
{
	const uint16_t odr_hz =
		(chan == SENSOR_CHAN_ACCEL_XYZ) ? header->acc_odr_hz : header->gyr_odr_hz;

	return (uint32_t)(1000000000ULL / odr_hz);
}

static int bmi270_decoder_get_frame_count(const uint8_t *buffer, struct sensor_chan_spec chan_spec,
					  uint16_t *frame_count)
{
	const struct bmi270_fifo_encoded_data *edata =
		(const struct bmi270_fifo_encoded_data *)buffer;

	if (chan_spec.chan_idx != 0) {
		return -EINVAL;
	}

	if (!edata->header.is_fifo) {
		return -ENODATA;
	}

	if (chan_spec.chan_type != SENSOR_CHAN_ACCEL_XYZ &&
	    chan_spec.chan_type != SENSOR_CHAN_GYRO_XYZ) {
		return -ENOTSUP;
	}

	if (edata->header.is_headerless) {
		*frame_count = edata->fifo_byte_count / BMI270_FIFO_PAYLOAD_ACC_GYR;
		return 0;
	}

	uint16_t count = 0;
	const uint8_t *p = edata->fifo_data;
	const uint8_t *end = p + edata->fifo_byte_count;
	bool want_acc = (chan_spec.chan_type == SENSOR_CHAN_ACCEL_XYZ);

	while (p < end) {
		uint8_t h = *p;
		uint8_t mode = BMI270_FIFO_HDR_MODE(h);
		uint8_t parm = BMI270_FIFO_HDR_PARM(h);

		if (mode == BMI270_FIFO_MODE_REGULAR) {
			if (parm == 0) {
				p += 2;
				continue;
			}
			int payload = 0;

			if (parm & BMI270_FIFO_PARM_ACC) {
				payload += BMI270_FIFO_SENSOR_BYTES;
			}
			if (parm & BMI270_FIFO_PARM_GYR) {
				payload += BMI270_FIFO_SENSOR_BYTES;
			}
			if (payload == 0) {
				p++;
				continue;
			}
			if (want_acc && (parm & BMI270_FIFO_PARM_ACC)) {
				count++;
			}
			if (!want_acc && (parm & BMI270_FIFO_PARM_GYR)) {
				count++;
			}
			p += 1 + payload;
		} else if (mode == BMI270_FIFO_MODE_CONTROL) {
			p += bmi270_fifo_control_frame_size(parm);
		} else {
			p++;
		}
	}

	*frame_count = count;
	return 0;
}

static int bmi270_decoder_get_size_info(struct sensor_chan_spec chan_spec, size_t *base_size,
					size_t *frame_size)
{
	if (chan_spec.chan_idx != 0) {
		return -EINVAL;
	}

	switch (chan_spec.chan_type) {
	case SENSOR_CHAN_ACCEL_XYZ:
	case SENSOR_CHAN_GYRO_XYZ:
		*base_size = sizeof(struct sensor_three_axis_data);
		*frame_size = sizeof(struct sensor_three_axis_sample_data);
		return 0;
	default:
		return -ENOTSUP;
	}
}

/* Accel: raw -> m/s^2 in Q31 with shift. range in G (2,4,8,16) */
static void decode_accel_frame(const uint8_t *payload, uint8_t range_g, int8_t shift,
			       struct sensor_three_axis_sample_data *out)
{
	int16_t x = (int16_t)sys_get_le16(&payload[0]);
	int16_t y = (int16_t)sys_get_le16(&payload[2]);
	int16_t z = (int16_t)sys_get_le16(&payload[4]);

	int64_t scale = (int64_t)SENSOR_G * range_g * (1LL << (31 - shift)) / INT16_MAX;

	out->timestamp_delta = 0;
	out->x = (q31_t)((x * scale) / 1000000LL);
	out->y = (q31_t)((y * scale) / 1000000LL);
	out->z = (q31_t)((z * scale) / 1000000LL);
}

/* Gyro: raw -> rad/s in Q31 with shift. range_dps in degrees/s */
static void decode_gyro_frame(const uint8_t *payload, uint16_t range_dps, int8_t shift,
			      struct sensor_three_axis_sample_data *out)
{
	int16_t x = (int16_t)sys_get_le16(&payload[0]);
	int16_t y = (int16_t)sys_get_le16(&payload[2]);
	int16_t z = (int16_t)sys_get_le16(&payload[4]);

	int64_t scale =
		(int64_t)range_dps * SENSOR_PI * (1LL << (31 - shift)) / (180LL * INT16_MAX);

	out->timestamp_delta = 0;
	out->x = (q31_t)((x * scale) / 1000000LL);
	out->y = (q31_t)((y * scale) / 1000000LL);
	out->z = (q31_t)((z * scale) / 1000000LL);
}

/* Accel range register value to G (2,4,8,16) */
static uint8_t acc_range_reg_to_g(uint8_t reg)
{
	static const uint8_t g[] = {2, 4, 8, 16};

	return reg < ARRAY_SIZE(g) ? g[reg] : 2;
}

/* Gyro range register index to dps (2000,1000,500,250,125) */
static uint16_t gyr_range_idx_to_dps(uint8_t idx)
{
	static const uint16_t dps[] = {2000, 1000, 500, 250, 125};

	return idx < ARRAY_SIZE(dps) ? dps[idx] : 2000;
}

static int bmi270_decoder_decode(const uint8_t *buffer, struct sensor_chan_spec chan_spec,
				 uint32_t *fit, uint16_t max_count, void *data_out)
{
	const struct bmi270_fifo_encoded_data *edata =
		(const struct bmi270_fifo_encoded_data *)buffer;
	struct sensor_three_axis_data *out = data_out;
	uint8_t acc_g = acc_range_reg_to_g(edata->header.acc_range);
	uint16_t gyr_dps = gyr_range_idx_to_dps(edata->header.gyr_range_idx);
	int8_t acc_shift =
		BMI270_ACC_SHIFT_BASE + (edata->header.acc_range > 0 ? edata->header.acc_range : 0);
	int8_t gyr_shift = BMI270_GYR_SHIFT_BASE;
	uint32_t sample_period_ns = bmi270_sample_period_ns(&edata->header, chan_spec.chan_type);
	uint32_t skip = *fit;
	uint16_t decoded = 0;
	const uint8_t *p = edata->fifo_data;
	const uint8_t *end = p + edata->fifo_byte_count;

	if (!edata->header.is_fifo || chan_spec.chan_idx != 0) {
		return -EINVAL;
	}

	if (chan_spec.chan_type != SENSOR_CHAN_ACCEL_XYZ &&
	    chan_spec.chan_type != SENSOR_CHAN_GYRO_XYZ) {
		return -ENOTSUP;
	}

	out->header.base_timestamp_ns = edata->header.timestamp;
	out->header.reading_count = 0;

	/* Headerless: fixed 12-byte frames, payload order GYR then ACC (same as header mode) */
	if (edata->header.is_headerless) {
		while (p + BMI270_FIFO_PAYLOAD_ACC_GYR <= end && decoded < max_count) {
			if (skip > 0) {
				skip--;
				p += BMI270_FIFO_PAYLOAD_ACC_GYR;
				continue;
			}
			if (chan_spec.chan_type == SENSOR_CHAN_ACCEL_XYZ) {
				decode_accel_frame(&p[6], acc_g, acc_shift,
						   &out->readings[decoded]);
			} else {
				decode_gyro_frame(p, gyr_dps, gyr_shift, &out->readings[decoded]);
			}
			out->readings[decoded].timestamp_delta =
				(uint32_t)(*fit + decoded) * sample_period_ns;
			decoded++;
			p += BMI270_FIFO_PAYLOAD_ACC_GYR;
		}
		*fit += decoded;
		out->shift = (chan_spec.chan_type == SENSOR_CHAN_ACCEL_XYZ) ? acc_shift : gyr_shift;
		out->header.reading_count = decoded;
		return (int)decoded;
	}

	while (p < end && decoded < max_count) {
		uint8_t h = *p;
		uint8_t mode = BMI270_FIFO_HDR_MODE(h);
		uint8_t parm = BMI270_FIFO_HDR_PARM(h);

		if (mode == BMI270_FIFO_MODE_REGULAR) {
			if (parm == 0) {
				p += 2;
				continue;
			}
			bool has_gyr = (parm & BMI270_FIFO_PARM_GYR) != 0;
			bool has_acc = (parm & BMI270_FIFO_PARM_ACC) != 0;
			int payload = 0;

			if (has_gyr) {
				payload += BMI270_FIFO_SENSOR_BYTES;
			}
			if (has_acc) {
				payload += BMI270_FIFO_SENSOR_BYTES;
			}
			if (payload == 0 || p + 1 + payload > end) {
				p++;
				continue;
			}

			bool want_this =
				(chan_spec.chan_type == SENSOR_CHAN_ACCEL_XYZ) ? has_acc : has_gyr;

			if (!want_this) {
				p += 1 + payload;
				continue;
			}
			if (skip > 0) {
				skip--;
				p += 1 + payload;
				continue;
			}

			/*
			 * Payload order (datasheet): GYR (6B if present) then ACC (6B if present).
			 * ACC offset = 0 when GYR absent, 6 when GYR present.
			 */
			const uint8_t *frame = p + 1;

			if (chan_spec.chan_type == SENSOR_CHAN_ACCEL_XYZ) {
				int acc_off = has_gyr ? BMI270_FIFO_SENSOR_BYTES : 0;

				decode_accel_frame(&frame[acc_off], acc_g, acc_shift,
						   &out->readings[decoded]);
			} else {
				decode_gyro_frame(frame, gyr_dps, gyr_shift,
						  &out->readings[decoded]);
			}
			out->readings[decoded].timestamp_delta =
				(uint32_t)(*fit + decoded) * sample_period_ns;
			decoded++;
			p += 1 + payload;
		} else if (mode == BMI270_FIFO_MODE_CONTROL) {
			p += bmi270_fifo_control_frame_size(parm);
		} else {
			p++;
		}
	}

	*fit += decoded;
	out->shift = (chan_spec.chan_type == SENSOR_CHAN_ACCEL_XYZ) ? acc_shift : gyr_shift;
	out->header.reading_count = decoded;
	return (int)decoded;
}

static bool bmi270_decoder_has_trigger(const uint8_t *buffer, enum sensor_trigger_type trigger)
{
	ARG_UNUSED(buffer);
	ARG_UNUSED(trigger);
	return false;
}

static const struct sensor_decoder_api bmi270_decoder_api = {
	.get_frame_count = bmi270_decoder_get_frame_count,
	.get_size_info = bmi270_decoder_get_size_info,
	.decode = bmi270_decoder_decode,
	.has_trigger = bmi270_decoder_has_trigger,
};

int bmi270_get_decoder(const struct device *dev, const struct sensor_decoder_api **decoder)
{
	ARG_UNUSED(dev);
	*decoder = &bmi270_decoder_api;
	return 0;
}
