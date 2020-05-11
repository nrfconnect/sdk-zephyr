/*
 * Copyright (c) 2019,2020 Linaro Limited
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>

#include "tfm_api.h"
#include "tfm_ns_interface.h"

K_MUTEX_DEFINE(tfm_mutex);

s32_t tfm_ns_interface_dispatch(veneer_fn fn,
				u32_t arg0, u32_t arg1,
				u32_t arg2, u32_t arg3)
{
	s32_t result;

	/* TFM request protected by NS lock */
	if (k_mutex_lock(&tfm_mutex, K_FOREVER) != 0) {
		return (s32_t)TFM_ERROR_GENERIC;
	}

	result = fn(arg0, arg1, arg2, arg3);

	k_mutex_unlock(&tfm_mutex);

	return result;
}

enum tfm_status_e tfm_ns_interface_init(void)
{
	/* The static K_MUTEX_DEFINE handles mutex init, so just return. */

	return TFM_SUCCESS;
}
