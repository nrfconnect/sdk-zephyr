/*
 * Copyright (C) 2022, Nordic Semiconductor ASA
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef INCLUDE_ZEPHYR_SYS_LINEAR_RANGE_H_
#define INCLUDE_ZEPHYR_SYS_LINEAR_RANGE_H_

#include <errno.h>
#include <stdint.h>
#include <stdlib.h>

#include <zephyr/sys/util.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup linear_range Linear Range
 *
 * The linear range API maps values in a linear range to a range index. A linear
 * range can be fully defined by four parameters:
 *
 * - Minimum value
 * - Step value
 * - Minimum index value
 * - Maximum index value
 *
 * For example, in a voltage regulator, supported voltages typically map to a
 * register index value like this:
 *
 * - 1000uV: 0x00
 * - 1250uV: 0x01
 * - 1500uV: 0x02
 * - ...
 * - 3000uV: 0x08
 *
 * In this case, we have:
 *
 * - Minimum value: 1000uV
 * - Step value: 250uV
 * - Minimum index value: 0x00
 * - Maximum index value: 0x08
 *
 * A linear range may also be constant, that is, step set to zero.
 *
 * It is often the case where the same device has discontinuous linear ranges.
 * The API offers utility functions to deal with groups of linear ranges as
 * well.
 *
 * Implementation uses fixed-width integers. Range is limited to [INT32_MIN,
 * INT32_MAX], while number of indices is limited to UINT16_MAX.
 *
 * Original idea borrowed from Linux.
 * @{
 */

/** @brief Linear range. */
struct linear_range {
	/** Minimum value. */
	int32_t min;
	/** Step value. */
	uint32_t step;
	/** Minimum index (must be <= maximum index). */
	uint16_t min_idx;
	/** Maximum index (must be >= minimum index). */
	uint16_t max_idx;
};

/**
 * @brief Initializer for @ref linear_range.
 *
 * @param _min Minimum value in range.
 * @param _step Step value.
 * @param _min_idx Minimum index.
 * @param _max_idx Maximum index.
 */
#define LINEAR_RANGE_INIT(_min, _step, _min_idx, _max_idx)                     \
	{                                                                      \
		.min = (_min),                                                 \
		.step = (_step),                                               \
		.min_idx = (_min_idx),                                         \
		.max_idx = (_max_idx),                                         \
	}

/**
 * @brief Obtain the number of values representable in a linear range.
 *
 * @param[in] r Linear range instance.
 *
 * @return Number of ranges representable by @p r.
 */
static inline uint32_t linear_range_values_count(const struct linear_range *r)
{
	return r->max_idx - r->min_idx + 1U;
}

/**
 * @brief Obtain the number of values representable by a group of linear ranges.
 *
 * @param[in] r Array of linear range instances.
 * @param r_cnt Number of linear range instances.
 *
 * @return Number of ranges representable by the @p r group.
 */
static inline uint32_t linear_range_group_values_count(
	const struct linear_range *r, size_t r_cnt)
{
	uint32_t values = 0U;

	for (size_t i = 0U; i < r_cnt; i++) {
		values += linear_range_values_count(&r[i]);
	}

	return values;
}

/**
 * @brief Obtain the maximum value representable by a linear range.
 *
 * @param[in] r Linear range instance.
 *
 * @return Maximum value representable by @p r.
 */
static inline int32_t linear_range_get_max_value(const struct linear_range *r)
{
	return r->min + (int32_t)(r->step * (r->max_idx - r->min_idx));
}

/**
 * @brief Obtain value given a linear range index.
 *
 * @param[in] r Linear range instance.
 * @param idx Range index.
 * @param[out] val Where value will be stored.
 *
 * @retval 0 If successful
 * @retval -EINVAL If index is out of range.
 */
static inline int linear_range_get_value(const struct linear_range *r,
					 uint16_t idx, int32_t *val)
{
	if ((idx < r->min_idx) || (idx > r->max_idx)) {
		return -EINVAL;
	}

	*val = r->min + (int32_t)(r->step * (idx - r->min_idx));

	return 0;
}

/**
 * @brief Obtain value in a group given a linear range index.
 *
 * @param[in] r Array of linear range instances.
 * @param r_cnt Number of linear range instances.
 * @param idx Range index.
 * @param[out] val Where value will be stored.
 *
 * @retval 0 If successful
 * @retval -EINVAL If index is out of range.
 */
static inline int linear_range_group_get_value(const struct linear_range *r,
					       size_t r_cnt, uint16_t idx,
					       int32_t *val)
{
	int ret = -EINVAL;

	for (size_t i = 0U; (ret != 0) && (i < r_cnt); i++) {
		ret = linear_range_get_value(&r[i], idx, val);
	}

	return ret;
}

/**
 * @brief Obtain index given a value.
 *
 * @param[in] r Linear range instance.
 * @param val Value.
 * @param[out] idx Where index will be stored.
 *
 * @retval 0 If a valid index is found within the linear range.
 * @retval -EINVAL If value is out of range.
 */
static inline int linear_range_get_index(const struct linear_range *r,
					 int32_t val, uint16_t *idx)
{
	if ((val < r->min) || (val > linear_range_get_max_value(r))) {
		return -EINVAL;
	}

	if (r->step == 0U) {
		*idx = r->max_idx;
		return 0;
	}

	*idx = r->min_idx + ceiling_fraction((uint32_t)(val - r->min), r->step);

	return 0;
}

/**
 * @brief Obtain index in a group given a value.
 *
 * @param[in] r Linear range instances.
 * @param r_cnt Number of linear range instances.
 * @param val Value.
 * @param[out] idx Where index will be stored.
 *
 * @retval 0 If a valid index is found.
 * @retval -EINVAL If value is out of range, or if the given window of values is
 * too narrow.
 */
static inline int linear_range_group_get_index(const struct linear_range *r,
					       size_t r_cnt, int32_t val,
					       uint16_t *idx)
{
	int ret = -EINVAL;

	for (size_t i = 0U; (ret != 0) && (i < r_cnt); i++) {
		ret = linear_range_get_index(&r[i], val, idx);
	}

	return ret;
}

/**
 * @brief Obtain index given a window of values.
 *
 * @param[in] r Linear range instance.
 * @param val_min Minimum window value.
 * @param val_max Maximum window value.
 * @param[out] idx Where index will be stored.
 *
 * @retval 0 If a valid index is found within window.
 * @retval -EINVAL If value is out of range, or if the given window of values is
 * too narrow.
 */
static inline int linear_range_get_win_index(const struct linear_range *r,
					     int32_t val_min, int32_t val_max,
					     uint16_t *idx)
{
	if ((val_min < r->min) || (val_max > linear_range_get_max_value(r))) {
		return -EINVAL;
	}

	if (r->step == 0U) {
		*idx = r->max_idx;
		return 0;
	}

	*idx = r->min_idx + ceiling_fraction((uint32_t)(val_min - r->min),
					     r->step);

	if ((r->min + r->step * (*idx - r->min_idx)) > val_max) {
		return -EINVAL;
	}

	return 0;
}

/**
 * @brief Obtain index in a group given a value that must be within a window of
 * values.
 *
 * @param[in] r Linear range instances.
 * @param r_cnt Number of linear range instances.
 * @param val_min Minimum window value.
 * @param val_max Maximum window value.
 * @param[out] idx Where index will be stored.
 *
 * @retval 0 If a valid index is found within window.
 * @retval -EINVAL If value is out of range, or if the given window of values is
 * too narrow.
 */
static inline int linear_range_group_get_win_index(const struct linear_range *r,
						   size_t r_cnt,
						   int32_t val_min,
						   int32_t val_max,
						   uint16_t *idx)
{
	int ret = -EINVAL;

	for (size_t i = 0U; (ret != 0) && (i < r_cnt); i++) {
		int32_t r_val_max = linear_range_get_max_value(&r[i]);

		ret = linear_range_get_win_index(
			&r[i], val_min, MAX(val_min, MIN(r_val_max, val_max)),
			idx);
	}

	return ret;
}

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* INCLUDE_ZEPHYR_SYS_LINEAR_RANGE_H_ */
