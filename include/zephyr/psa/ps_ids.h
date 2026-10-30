/*
 * Copyright (c) 2025 Analog Devices, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef ZEPHYR_PSA_PS_IDS_H_
#define ZEPHYR_PSA_PS_IDS_H_

/**
 * @file zephyr/psa/ps_ids.h
 *
 * @brief This file defines the UID ranges of the existing users of the PSA PS API.
 *
 * @see @ref zephyr/psa/key_ids.h
 */

/** UID range to be used by the TF-M PSA PS Settings backend. */
#define ZEPHYR_PSA_SETTINGS_TFM_PS_UID_RANGE_BEGIN 0x2C000000
#define ZEPHYR_PSA_SETTINGS_TFM_PS_UID_RANGE_SIZE  0x10000 /**< 64 Ki */

#endif /* ZEPHYR_PSA_PS_IDS_H_ */
