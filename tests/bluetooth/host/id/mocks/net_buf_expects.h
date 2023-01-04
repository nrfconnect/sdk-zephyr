/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>

/*
 *  Validate expected behaviour when net_buf_unref() is called
 *
 *  Expected behaviour:
 *   - net_buf_unref() to be called once with correct parameters
 */
void expect_single_call_net_buf_unref(struct net_buf *buf);

/*
 *  Validate expected behaviour when net_buf_unref() isn't called
 *
 *  Expected behaviour:
 *   - net_buf_unref() isn't called at all
 */
void expect_not_called_net_buf_unref(void);
