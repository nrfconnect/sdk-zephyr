/*
 * Copyright (c) 2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#if defined(CONFIG_BT_HOST_NORDIC)
#include "host/host/hci_raw_internal.h"
#else

#ifndef __BT_HCI_RAW_INTERNAL_H
#define __BT_HCI_RAW_INTERNAL_H

#include <zephyr/devicetree.h>
#include <zephyr/net_buf.h>

#ifdef __cplusplus
extern "C" {
#endif

struct bt_dev_raw {
	const struct device *hci;
};

int bt_hci_recv(const struct device *dev, struct net_buf *buf);

extern struct bt_dev_raw bt_dev;

#ifdef __cplusplus
}
#endif

#endif /* __BT_HCI_RAW_INTERNAL_H */

#endif /* !defined(CONFIG_BT_HOST_NORDIC) */
