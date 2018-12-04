/*
 * Copyright (c) 2016 Intel Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_MODULE_NAME net_ieee802154_aloha
#define NET_LOG_LEVEL CONFIG_NET_L2_IEEE802154_LOG_LEVEL

#include <net/net_core.h>
#include <net/net_if.h>

#include <errno.h>

#include "ieee802154_frame.h"
#include "ieee802154_utils.h"
#include "ieee802154_radio_utils.h"

static inline int aloha_radio_send(struct net_if *iface,
				   struct net_pkt *pkt,
				   struct net_buf *frag)
{
	u8_t retries = CONFIG_NET_L2_IEEE802154_RADIO_TX_RETRIES;
	struct ieee802154_context *ctx = net_if_l2_data(iface);
	bool ack_required = prepare_for_ack(ctx, pkt, frag);
	int ret = -EIO;

	NET_DBG("frag %p", frag);

	while (retries) {
		retries--;

		ret = ieee802154_tx(iface, pkt, frag);
		if (ret) {
			continue;
		}

		ret = wait_for_ack(iface, ack_required);
		if (!ret) {
			break;
		}
	}

	return ret;
}

static enum net_verdict aloha_radio_handle_ack(struct net_if *iface,
					       struct net_pkt *pkt)
{
	struct ieee802154_context *ctx = net_if_l2_data(iface);

	return handle_ack(ctx, pkt);
}

/* Declare the public Radio driver function used by the HW drivers */
FUNC_ALIAS(aloha_radio_send,
	   ieee802154_radio_send, int);

FUNC_ALIAS(aloha_radio_handle_ack,
	   ieee802154_radio_handle_ack, enum net_verdict);
