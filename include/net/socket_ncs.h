/*
 * Copyright (c) 2021 Nordic Semiconductor
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_INCLUDE_NET_SOCKET_NCS_H_
#define ZEPHYR_INCLUDE_NET_SOCKET_NCS_H_

/**
 * @file
 * @brief NCS specific additions to the BSD sockets API definitions
 */

#ifdef __cplusplus
extern "C" {
#endif

/* NCS specific protocol/address families. */

#define PF_LTE 102 /**< Protocol family specific to LTE. */
#define AF_LTE PF_LTE /**< Address family specific to LTE. */

/* NCS specific protocol types. */

/** Protocol numbers for LTE protocols */
enum net_lte_protocol {
	NPROTO_AT = 513,
	NPROTO_PDN = 514
};

/** Protocol numbers for LOCAL protocols */
enum net_local_protocol {
	NPROTO_DFU = 515
};

/* NCS specific socket types. */

#define SOCK_MGMT 4 /**< Management socket type. */

/* NCS specific TLS options */

/** Socket option to control TLS session caching. Accepted values:
 *  - 0 - Disabled.
 *  - 1 - Enabled.
 */
#define TLS_SESSION_CACHE 8

/* Valid values for TLS_SESSION_CACHE option */
#define TLS_SESSION_CACHE_DISABLED 0 /**< Disable TLS session caching. */
#define TLS_SESSION_CACHE_ENABLED 1 /**< Enable TLS session caching. */

/* NCS specific socket options */

/** sockopt: disable all replies to unexpected traffics */
#define SO_SILENCE_ALL 30
/** sockopt: disable IPv4 ICMP replies */
#define SO_IP_ECHO_REPLY 31
/** sockopt: disable IPv6 ICMP replies */
#define SO_IPV6_ECHO_REPLY 32

/* NCS specific PDN options */

/** Protocol level for PDN. */
#define SOL_PDN 514

/* Socket options for SOL_PDN level */
#define SO_PDN_AF 1
#define SO_PDN_CONTEXT_ID 2
#define SO_PDN_STATE 3

/* NCS specific DFU options */

/** Protocol level for DFU. */
#define SOL_DFU 515

/* Socket options for SOL_DFU level */
#define SO_DFU_FW_VERSION 1
#define SO_DFU_RESOURCES 2
#define SO_DFU_TIMEO 3
#define SO_DFU_APPLY 4
#define SO_DFU_REVERT 5
#define SO_DFU_BACKUP_DELETE 6
#define SO_DFU_OFFSET 7
#define SO_DFU_ERROR 20

/* NCS specific gettaddrinfo() flags */

/** Assume `service` contains a Packet Data Network (PDN) ID.
 *  When specified together with the AI_NUMERICSERV flag,
 *  `service` shall be formatted as follows: "port:pdn_id"
 *  where "port" is the port number and "pdn_id" is the PDN ID.
 *  Example: "8080:1", port 8080 PDN ID 1.
 *  Example: "42:0", port 42 PDN ID 0.
 */
#define AI_PDNSERV 0x1000

#ifdef __cplusplus
}
#endif

#endif /* ZEPHYR_INCLUDE_NET_SOCKET_NCS_H_ */
