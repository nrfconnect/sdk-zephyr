/*
 * Copyright (c) 2018 Intel Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Ethernet Management interface public header
 */

#ifndef __ETHERNET_MGMT_H__
#define __ETHERNET_MGMT_H__

#include <net/ethernet.h>
#include <net/net_mgmt.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Ethernet library
 * @defgroup ethernet_mgmt Ethernet Library
 * @ingroup networking
 * @{
 */

#define _NET_ETHERNET_LAYER	NET_MGMT_LAYER_L2
#define _NET_ETHERNET_CODE	0x208
#define _NET_ETHERNET_BASE	(NET_MGMT_IFACE_BIT |			\
				 NET_MGMT_LAYER(_NET_ETHERNET_LAYER) |	\
				 NET_MGMT_LAYER_CODE(_NET_ETHERNET_CODE))
#define _NET_ETHERNET_EVENT	(_NET_ETHERNET_BASE | NET_MGMT_EVENT_BIT)

enum net_request_ethernet_cmd {
	NET_REQUEST_ETHERNET_CMD_SET_AUTO_NEGOTIATION = 1,
	NET_REQUEST_ETHERNET_CMD_SET_LINK,
	NET_REQUEST_ETHERNET_CMD_SET_DUPLEX,
	NET_REQUEST_ETHERNET_CMD_SET_MAC_ADDRESS,
	NET_REQUEST_ETHERNET_CMD_SET_QAV_DELTA_BANDWIDTH,
	NET_REQUEST_ETHERNET_CMD_SET_QAV_IDLE_SLOPE,
};

#define NET_REQUEST_ETHERNET_SET_AUTO_NEGOTIATION			\
	(_NET_ETHERNET_BASE | NET_REQUEST_ETHERNET_CMD_SET_AUTO_NEGOTIATION)

NET_MGMT_DEFINE_REQUEST_HANDLER(NET_REQUEST_ETHERNET_SET_AUTO_NEGOTIATION);

#define NET_REQUEST_ETHERNET_SET_LINK					\
	(_NET_ETHERNET_BASE | NET_REQUEST_ETHERNET_CMD_SET_LINK)

NET_MGMT_DEFINE_REQUEST_HANDLER(NET_REQUEST_ETHERNET_SET_LINK);

#define NET_REQUEST_ETHERNET_SET_DUPLEX					\
	(_NET_ETHERNET_BASE | NET_REQUEST_ETHERNET_CMD_SET_DUPLEX)

NET_MGMT_DEFINE_REQUEST_HANDLER(NET_REQUEST_ETHERNET_SET_DUPLEX);

#define NET_REQUEST_ETHERNET_SET_MAC_ADDRESS				\
	(_NET_ETHERNET_BASE | NET_REQUEST_ETHERNET_CMD_SET_MAC_ADDRESS)

NET_MGMT_DEFINE_REQUEST_HANDLER(NET_REQUEST_ETHERNET_SET_MAC_ADDRESS);

#define NET_REQUEST_ETHERNET_SET_QAV_DELTA_BANDWIDTH				\
	(_NET_ETHERNET_BASE | NET_REQUEST_ETHERNET_CMD_SET_QAV_DELTA_BANDWIDTH)

NET_MGMT_DEFINE_REQUEST_HANDLER(NET_REQUEST_ETHERNET_SET_QAV_DELTA_BANDWIDTH);

#define NET_REQUEST_ETHERNET_SET_QAV_IDLE_SLOPE				\
	(_NET_ETHERNET_BASE | NET_REQUEST_ETHERNET_CMD_SET_QAV_IDLE_SLOPE)

NET_MGMT_DEFINE_REQUEST_HANDLER(NET_REQUEST_ETHERNET_SET_QAV_IDLE_SLOPE);

struct net_eth_addr;
struct ethernet_qav_queue_param;

struct ethernet_req_params {
	union {
		bool auto_negotiation;
		bool full_duplex;

		struct {
			bool link_10bt;
			bool link_100bt;
			bool link_1000bt;
		} l;

		struct net_eth_addr mac_address;

		struct ethernet_qav_queue_param qav_queue_param;
	};
};

enum net_event_ethernet_cmd {
	NET_EVENT_ETHERNET_CMD_CARRIER_ON = 1,
	NET_EVENT_ETHERNET_CMD_CARRIER_OFF,
	NET_EVENT_ETHERNET_CMD_VLAN_TAG_ENABLED,
	NET_EVENT_ETHERNET_CMD_VLAN_TAG_DISABLED,
};

#define NET_EVENT_ETHERNET_CARRIER_ON					\
	(_NET_ETHERNET_EVENT | NET_EVENT_ETHERNET_CMD_CARRIER_ON)

#define NET_EVENT_ETHERNET_CARRIER_OFF					\
	(_NET_ETHERNET_EVENT | NET_EVENT_ETHERNET_CMD_CARRIER_OFF)

#define NET_EVENT_ETHERNET_VLAN_TAG_ENABLED				\
	(_NET_ETHERNET_EVENT | NET_EVENT_ETHERNET_CMD_VLAN_TAG_ENABLED)

#define NET_EVENT_ETHERNET_VLAN_TAG_DISABLED				\
	(_NET_ETHERNET_EVENT | NET_EVENT_ETHERNET_CMD_VLAN_TAG_DISABLED)

struct net_if;

#if defined(CONFIG_NET_L2_ETHERNET_MGMT)
void ethernet_mgmt_raise_carrier_on_event(struct net_if *iface);

void ethernet_mgmt_raise_carrier_off_event(struct net_if *iface);
void ethernet_mgmt_raise_vlan_enabled_event(struct net_if *iface, u16_t tag);
void ethernet_mgmt_raise_vlan_disabled_event(struct net_if *iface, u16_t tag);
#else
static inline void ethernet_mgmt_raise_carrier_on_event(struct net_if *iface)
{
	ARG_UNUSED(iface);
}

static inline void ethernet_mgmt_raise_carrier_off_event(struct net_if *iface)
{
	ARG_UNUSED(iface);
}

static inline void ethernet_mgmt_raise_vlan_enabled_event(struct net_if *iface,
							  u16_t tag)
{
	ARG_UNUSED(iface);
	ARG_UNUSED(tag);
}

static inline void ethernet_mgmt_raise_vlan_disabled_event(struct net_if *iface,
							   u16_t tag)
{
	ARG_UNUSED(iface);
	ARG_UNUSED(tag);
}
#endif
/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif /* __ETHERNET_MGMT_H__ */
