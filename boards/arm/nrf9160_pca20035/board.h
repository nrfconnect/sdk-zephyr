/*
 * Copyright (c) 2018 Nordic Semiconductor ASA.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __INC_BOARD_H
#define __INC_BOARD_H

#include <zephyr.h>

/* Function to enable/disable 1v8 power rail */
int pca20035_power_1v8_set(bool enable);

/* Function to enable/disable 3v3 power rail */
int pca20035_power_3v3_set(bool enable);

#endif /* __INC_BOARD_H */
