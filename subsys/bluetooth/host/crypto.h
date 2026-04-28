/*
 * Copyright (c) 2016-2017 Nordic Semiconductor ASA
 * Copyright (c) 2015-2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#if defined(CONFIG_BT_HOST_NORDIC)
#include "host/host/crypto.h"
#else

int bt_crypto_init(void);

#endif /* !defined(CONFIG_BT_HOST_NORDIC) */
