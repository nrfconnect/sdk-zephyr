/* IEEE 802.15.4 settings header */

/*
 * Copyright (c) 2017 Intel Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#if defined(CONFIG_NET_L2_BT) && defined(CONFIG_NET_CONFIG_SETTINGS)
int z_net_config_bt_setup(void);
#else
#define z_net_config_bt_setup(...) 0
#endif
