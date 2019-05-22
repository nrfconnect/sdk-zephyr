/** @file
 @brief LLDP definitions and handler

 This is not to be included by the application.
 */

/*
 * Copyright (c) 2017 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_INCLUDE_NET_LLDP_H_
#define ZEPHYR_INCLUDE_NET_LLDP_H_

/**
 * @brief LLDP definitions and helpers
 * @defgroup lldp Link Layer Discovery Protocol definitions and helpers
 * @ingroup networking
 * @{
 */

#ifdef __cplusplus
extern "C" {
#endif

/** @cond INTERNAL_HIDDEN */

#define LLDP_TLV_GET_LENGTH(type_length)	(type_length & BIT_MASK(9))
#define LLDP_TLV_GET_TYPE(type_length)		((u8_t)(type_length >> 9))

/* LLDP Definitions */

/* According to the spec, End of LLDPDU TLV value is constant. */
#define NET_LLDP_END_LLDPDU_VALUE 0x0000

/*
 * For the Chassis ID TLV Value, if subtype is a MAC address then we must
 * use values from CONFIG_NET_LLDP_CHASSIS_ID_MAC0 through
 * CONFIG_NET_LLDP_CHASSIS_ID_MAC5. If not, we use CONFIG_NET_LLDP_CHASSIS_ID.
 *
 * FIXME: implement a similar scheme for subtype 5 (network address).
 */
#if (CONFIG_NET_LLDP_CHASSIS_ID_SUBTYPE == 4)
#define NET_LLDP_CHASSIS_ID_VALUE		\
	{					\
	  CONFIG_NET_LLDP_CHASSIS_ID_MAC0,	\
	  CONFIG_NET_LLDP_CHASSIS_ID_MAC1,	\
	  CONFIG_NET_LLDP_CHASSIS_ID_MAC2,	\
	  CONFIG_NET_LLDP_CHASSIS_ID_MAC3,	\
	  CONFIG_NET_LLDP_CHASSIS_ID_MAC4,	\
	  CONFIG_NET_LLDP_CHASSIS_ID_MAC5 	\
	}

#define NET_LLDP_CHASSIS_ID_VALUE_LEN (6)
#else
#define NET_LLDP_CHASSIS_ID_VALUE CONFIG_NET_LLDP_CHASSIS_ID
#define NET_LLDP_CHASSIS_ID_VALUE_LEN (sizeof(CONFIG_NET_LLDP_CHASSIS_ID) - 1)
#endif

/*
 * For the Port ID TLV Value, if subtype is a MAC address then we must
 * use values from CONFIG_NET_LLDP_PORT_ID_MAC0 through
 * CONFIG_NET_LLDP_PORT_ID_MAC5. If not, we use CONFIG_NET_LLDP_PORT_ID.
 *
 * FIXME: implement a similar scheme for subtype 4 (network address).
 */
#if (CONFIG_NET_LLDP_PORT_ID_SUBTYPE == 3)
#define NET_LLDP_PORT_ID_VALUE		\
	{				\
	  CONFIG_NET_LLDP_PORT_ID_MAC0,	\
	  CONFIG_NET_LLDP_PORT_ID_MAC1, \
	  CONFIG_NET_LLDP_PORT_ID_MAC2, \
	  CONFIG_NET_LLDP_PORT_ID_MAC3, \
	  CONFIG_NET_LLDP_PORT_ID_MAC4, \
	  CONFIG_NET_LLDP_PORT_ID_MAC5  \
	}

#define NET_LLDP_PORT_ID_VALUE_LEN (6)
#else
#define NET_LLDP_PORT_ID_VALUE CONFIG_NET_LLDP_PORT_ID
#define NET_LLDP_PORT_ID_VALUE_LEN (sizeof(CONFIG_NET_LLDP_PORT_ID) - 1)
#endif

/*
 * TLVs Length.
 * Note that TLVs that have a subtype must have a byte added to their length.
 */
#define NET_LLDP_CHASSIS_ID_TLV_LEN (NET_LLDP_CHASSIS_ID_VALUE_LEN + 1)
#define NET_LLDP_PORT_ID_TLV_LEN (NET_LLDP_PORT_ID_VALUE_LEN + 1)
#define NET_LLDP_TTL_TLV_LEN (2)

/*
 * Time to Live value.
 * Calculate based on section 9.2.5.22 from LLDP spec.
 *
 * FIXME: when the network interface is about to be ‘disabled’ TTL shall be set
 * to zero so LLDP Rx agents can invalidate the entry related to this node.
 */
#define NET_LLDP_TTL \
	MIN((CONFIG_NET_LLDP_TX_INTERVAL * CONFIG_NET_LLDP_TX_HOLD) + 1, 65535)


struct net_if;

/** @endcond */

/** TLV Types. Please refer to table 8-1 from IEEE 802.1AB standard. */
enum net_lldp_tlv_type {
	LLDP_TLV_END_LLDPDU          = 0, /**< End Of LLDPDU (optional)      */
	LLDP_TLV_CHASSIS_ID          = 1, /**< Chassis ID (mandatory)        */
	LLDP_TLV_PORT_ID             = 2, /**< Port ID (mandatory)           */
	LLDP_TLV_TTL                 = 3, /**< Time To Live (mandatory)      */
	LLDP_TLV_PORT_DESC           = 4, /**< Port Description (optional)   */
	LLDP_TLV_SYSTEM_NAME         = 5, /**< System Name (optional)        */
	LLDP_TLV_SYSTEM_DESC         = 6, /**< System Description (optional) */
	LLDP_TLV_SYSTEM_CAPABILITIES = 7, /**< System Capability (optional)  */
	LLDP_TLV_MANAGEMENT_ADDR     = 8, /**< Management Address (optional) */
	/* Types 9 - 126 are reserved. */
	LLDP_TLV_ORG_SPECIFIC       = 127, /**< Org specific TLVs (optional) */
};

/** Chassis ID TLV, see chapter 8.5.2 in IEEE 802.1AB */
struct net_lldp_chassis_tlv {
	/** 7 bits for type, 9 bits for length */
	u16_t type_length;
	/** ID subtype */
	u8_t subtype;
	/** Chassis ID value */
	u8_t value[NET_LLDP_CHASSIS_ID_VALUE_LEN];
} __packed;

/** Port ID TLV, see chapter 8.5.3 in IEEE 802.1AB */
struct net_lldp_port_tlv {
	/** 7 bits for type, 9 bits for length */
	u16_t type_length;
	/** ID subtype */
	u8_t subtype;
	/** Port ID value */
	u8_t value[NET_LLDP_PORT_ID_VALUE_LEN];
} __packed;

/** Time To Live TLV, see chapter 8.5.4 in IEEE 802.1AB */
struct net_lldp_time_to_live_tlv {
	/** 7 bits for type, 9 bits for length */
	u16_t type_length;
	/** Time To Live (TTL) value */
	u16_t ttl;
} __packed;

/**
 * LLDP Data Unit (LLDPDU) shall contain the following ordered TLVs
 * as stated in "8.2 LLDPDU format" from the IEEE 802.1AB
 */
struct net_lldpdu {
	struct net_lldp_chassis_tlv chassis_id;	/**< Mandatory Chassis TLV */
	struct net_lldp_port_tlv port_id;	/**< Mandatory Port TLV */
	struct net_lldp_time_to_live_tlv ttl;	/**< Mandatory TTL TLV */
} __packed;

/**
 * @brief Set the LLDP data unit for a network interface.
 *
 * @param iface Network interface
 * @param lldpdu LLDP data unit struct
 *
 * @return 0 if ok, <0 if error
 */
int net_lldp_config(struct net_if *iface, const struct net_lldpdu *lldpdu);

/**
 * @brief Set the Optional LLDP TLVs for a network interface.
 *
 * @param iface Network interface
 * @param tlv LLDP optional TLVs following mandatory part
 * @param len Length of the optional TLVs
 *
 * @return 0 if ok, <0 if error
 */
int net_lldp_config_optional(struct net_if *iface, const u8_t *tlv, size_t len);

/**
 * @brief Initialize LLDP engine.
 */
void net_lldp_init(void);

/**
 * @brief LLDP Receive packet callback
 *
 * Callback gets called upon receiving packet. It is responsible for
 * freeing packet or indicating to the stack that it needs to free packet
 * by returning correct net_verdict.
 *
 * Returns:
 *  - NET_DROP, if packet was invalid, rejected or we want the stack to free it.
 *    In this case the core stack will free the packet.
 *  - NET_OK, if the packet was accepted, in this case the ownership of the
 *    net_pkt goes to callback and core network stack will forget it.
 */
typedef enum net_verdict (*net_lldp_recv_cb_t)(struct net_if *iface,
					       struct net_pkt *pkt);

/**
 * @brief Register LLDP Rx callback function
 *
 * @param iface Network interface
 * @param cb Callback function
 *
 * @return 0 if ok, < 0 if error
 */
int net_lldp_register_callback(struct net_if *iface, net_lldp_recv_cb_t cb);

/**
 * @brief Parse LLDP packet
 *
 * @param iface Network interface
 * @param pkt Network packet
 *
 * @return Return the policy for network buffer
 */
enum net_verdict net_lldp_recv(struct net_if *iface, struct net_pkt *pkt);

/**
 * @brief Set LLDP protocol data unit (LLDPDU) for the network interface.
 *
 * @param iface Network interface
 *
 * @return <0 if error, index in lldp array if iface is found there
 */
#if defined(CONFIG_NET_LLDP)
int net_lldp_set_lldpdu(struct net_if *iface);
#else
#define net_lldp_set_lldpdu(iface)
#endif

/**
 * @brief Unset LLDP protocol data unit (LLDPDU) for the network interface.
 *
 * @param iface Network interface
 */
#if defined(CONFIG_NET_LLDP)
void net_lldp_unset_lldpdu(struct net_if *iface);
#else
#define net_lldp_unset_lldpdu(iface)
#endif

#ifdef __cplusplus
}
#endif

/**
 * @}
 */

#endif /* ZEPHYR_INCLUDE_NET_LLDP_H_ */
