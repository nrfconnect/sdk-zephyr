/* rpa.h - Bluetooth Resolvable Private Addresses (RPA) generation and
 * resolution
 */

#if defined(CONFIG_BT_HOST_NORDIC)
#include "host/common/rpa.h"
#else

/*
 * Copyright (c) 2017 Nordic Semiconductor ASA
 * Copyright (c) 2015-2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>

bool bt_rpa_irk_matches(const uint8_t irk[16], const bt_addr_t *addr);
int bt_rpa_create(const uint8_t irk[16], bt_addr_t *rpa);

#endif /* !defined(CONFIG_BT_HOST_NORDIC) */
