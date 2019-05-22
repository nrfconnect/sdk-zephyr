/* IEEE 802.15.4 settings header */

/*
 * Copyright (c) 2017 Intel Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#if defined(CONFIG_NET_L2_IEEE802154) && defined(CONFIG_NET_CONFIG_SETTINGS)
int z_net_config_ieee802154_setup(void);
#else
#define z_net_config_ieee802154_setup(...) 0
#endif
