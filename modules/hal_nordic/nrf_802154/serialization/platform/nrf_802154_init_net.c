/*
 * Copyright (c) 2020 - 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <init.h>

#include "nrf_802154.h"
#include "nrf_802154_serialization.h"

static int serialization_init(const struct device *dev)
{
	/* On NET core we don't use Zephyr's shim layer so we have to call inits manually */
	nrf_802154_init();

	nrf_802154_serialization_init();

	return 0;
}

SYS_INIT(serialization_init, POST_KERNEL, CONFIG_APPLICATION_INIT_PRIORITY);
