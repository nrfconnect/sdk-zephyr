/** @file
 * @brief Generic connection related functions
 */

/*
 * Copyright (c) 2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <logging/log.h>
LOG_MODULE_REGISTER(net_conn, CONFIG_NET_CONN_LOG_LEVEL);

#include <errno.h>
#include <misc/util.h>

#include <net/net_core.h>
#include <net/net_pkt.h>
#include <net/udp.h>
#include <net/ethernet.h>
#include <net/socket_can.h>

#include "net_private.h"
#include "icmpv6.h"
#include "icmpv4.h"
#include "udp_internal.h"
#include "tcp_internal.h"
#include "connection.h"
#include "net_stats.h"

/** Is this connection used or not */
#define NET_CONN_IN_USE			BIT(0)

/** Remote address set */
#define NET_CONN_REMOTE_ADDR_SET	BIT(1)

/** Local address set */
#define NET_CONN_LOCAL_ADDR_SET		BIT(2)

/** Local port set */
#define NET_CONN_REMOTE_PORT_SPEC	BIT(3)

/** Remote port set */
#define NET_CONN_LOCAL_PORT_SPEC	BIT(4)

/** Local address specified */
#define NET_CONN_REMOTE_ADDR_SPEC	BIT(5)

/** Remote address specified */
#define NET_CONN_LOCAL_ADDR_SPEC	BIT(6)

#define NET_CONN_RANK(_flags)		(_flags & 0x78)

static struct net_conn conns[CONFIG_NET_MAX_CONN];

static sys_slist_t conn_unused;
static sys_slist_t conn_used;

#if (CONFIG_NET_CONN_LOG_LEVEL >= LOG_LEVEL_DBG)
static inline
void conn_register_debug(struct net_conn *conn,
			 u16_t remote_port, u16_t local_port)
{
	char dst[NET_IPV6_ADDR_LEN];
	char src[NET_IPV6_ADDR_LEN];

	if (conn->flags & NET_CONN_REMOTE_ADDR_SET) {
		if (IS_ENABLED(CONFIG_NET_IPV6) &&
		    conn->family == AF_INET6) {
			snprintk(dst, sizeof(dst), "%s",
				 log_strdup(net_sprint_ipv6_addr(
				    &net_sin6(&conn->remote_addr)->sin6_addr)));
		} else if (IS_ENABLED(CONFIG_NET_IPV4) &&
			   conn->family == AF_INET) {
			snprintk(dst, sizeof(dst), "%s",
				 log_strdup(net_sprint_ipv4_addr(
				    &net_sin(&conn->remote_addr)->sin_addr)));
		} else {
			snprintk(dst, sizeof(dst), "%s", "?");
		}
	} else {
		snprintk(dst, sizeof(dst), "%s", "-");
	}

	if (conn->flags & NET_CONN_LOCAL_ADDR_SET) {
		if (IS_ENABLED(CONFIG_NET_IPV6) &&
		    conn->family == AF_INET6) {
			snprintk(src, sizeof(src), "%s",
				 log_strdup(net_sprint_ipv6_addr(
				    &net_sin6(&conn->local_addr)->sin6_addr)));
		} else if (IS_ENABLED(CONFIG_NET_IPV4) &&
			   conn->family == AF_INET) {
			snprintk(src, sizeof(src), "%s",
				 log_strdup(net_sprint_ipv4_addr(
				    &net_sin(&conn->local_addr)->sin_addr)));
		} else {
			snprintk(src, sizeof(src), "%s", "?");
		}
	} else {
		snprintk(src, sizeof(src), "%s", "-");
	}

	NET_DBG("[%p/%d/%u/0x%02x] remote %s/%u ",
		conn, conn->proto, conn->family, conn->flags,
		log_strdup(dst), remote_port);
	NET_DBG("  local %s/%u cb %p ud %p",
		log_strdup(src), local_port, conn->cb, conn->user_data);
}
#else
#define conn_register_debug(...)
#endif /* (CONFIG_NET_CONN_LOG_LEVEL >= LOG_LEVEL_DBG) */

static struct net_conn *conn_get_unused(void)
{
	sys_snode_t *node;

	node = sys_slist_peek_head(&conn_unused);
	if (!node) {
		return NULL;
	}

	sys_slist_remove(&conn_unused, NULL, node);

	return CONTAINER_OF(node, struct net_conn, node);
}

static void conn_set_used(struct net_conn *conn)
{
	conn->flags |= NET_CONN_IN_USE;

	sys_slist_prepend(&conn_used, &conn->node);
}

static void conn_set_unused(struct net_conn *conn)
{
	(void)memset(conn, 0, sizeof(*conn));

	sys_slist_prepend(&conn_unused, &conn->node);
}

/* Check if we already have identical connection handler installed. */
static struct net_conn *conn_find_handler(u16_t proto, u8_t family,
					  const struct sockaddr *remote_addr,
					  const struct sockaddr *local_addr,
					  u16_t remote_port,
					  u16_t local_port)
{
	struct net_conn *conn;

	SYS_SLIST_FOR_EACH_CONTAINER(&conn_used, conn, node) {
		if (conn->proto != proto) {
			continue;
		}

		if (conn->family != family) {
			continue;
		}

		if (remote_addr) {
			if (!(conn->flags & NET_CONN_REMOTE_ADDR_SET)) {
				continue;
			}

			if (IS_ENABLED(CONFIG_NET_IPV6) &&
			    remote_addr->sa_family == AF_INET6 &&
			    remote_addr->sa_family ==
			    conn->remote_addr.sa_family) {
				if (!net_ipv6_addr_cmp(
					    &net_sin6(remote_addr)->sin6_addr,
					    &net_sin6(&conn->remote_addr)->
								sin6_addr)) {
					continue;
				}
			} else if (IS_ENABLED(CONFIG_NET_IPV4) &&
				   remote_addr->sa_family == AF_INET &&
				   remote_addr->sa_family ==
				   conn->remote_addr.sa_family) {
				if (!net_ipv4_addr_cmp(
					    &net_sin(remote_addr)->sin_addr,
					    &net_sin(&conn->remote_addr)->
								sin_addr)) {
					continue;
				}
			} else {
				continue;
			}
		} else if (conn->flags & NET_CONN_REMOTE_ADDR_SET) {
			continue;
		}

		if (local_addr) {
			if (!(conn->flags & NET_CONN_LOCAL_ADDR_SET)) {
				continue;
			}

			if (IS_ENABLED(CONFIG_NET_IPV6) &&
			    local_addr->sa_family == AF_INET6 &&
			    local_addr->sa_family ==
			    conn->local_addr.sa_family) {
				if (!net_ipv6_addr_cmp(
					    &net_sin6(local_addr)->sin6_addr,
					    &net_sin6(&conn->local_addr)->
								sin6_addr)) {
					continue;
				}
			} else if (IS_ENABLED(CONFIG_NET_IPV4) &&
				   local_addr->sa_family == AF_INET &&
				   local_addr->sa_family ==
				   conn->local_addr.sa_family) {
				if (!net_ipv4_addr_cmp(
					    &net_sin(local_addr)->sin_addr,
					    &net_sin(&conn->local_addr)->
								sin_addr)) {
					continue;
				}
			} else {
				continue;
			}
		} else if (conn->flags & NET_CONN_LOCAL_ADDR_SET) {
			continue;
		}

		if (net_sin(&conn->remote_addr)->sin_port !=
		    htons(remote_port)) {
			continue;
		}

		if (net_sin(&conn->local_addr)->sin_port !=
		    htons(local_port)) {
			continue;
		}

		return conn;
	}

	return NULL;
}

int net_conn_register(u16_t proto, u8_t family,
		      const struct sockaddr *remote_addr,
		      const struct sockaddr *local_addr,
		      u16_t remote_port,
		      u16_t local_port,
		      net_conn_cb_t cb,
		      void *user_data,
		      struct net_conn_handle **handle)
{
	struct net_conn *conn;
	u8_t flags = 0U;

	conn = conn_find_handler(proto, family, remote_addr, local_addr,
				 remote_port, local_port);
	if (conn) {
		NET_ERR("Identical connection handler %p already found.", conn);
		return -EALREADY;
	}

	conn = conn_get_unused();
	if (!conn) {
		return -ENOENT;
	}

	if (remote_addr) {
		if (IS_ENABLED(CONFIG_NET_IPV6) &&
		    remote_addr->sa_family == AF_INET6) {
			memcpy(&conn->remote_addr, remote_addr,
			       sizeof(struct sockaddr_in6));

			if (!net_ipv6_is_addr_unspecified(
				    &net_sin6(remote_addr)->
				    sin6_addr)) {
				flags |= NET_CONN_REMOTE_ADDR_SPEC;
			}
		} else if (IS_ENABLED(CONFIG_NET_IPV4) &&
			   remote_addr->sa_family == AF_INET) {
			memcpy(&conn->remote_addr, remote_addr,
			       sizeof(struct sockaddr_in));

			if (net_sin(remote_addr)->sin_addr.s_addr) {
				flags |= NET_CONN_REMOTE_ADDR_SPEC;
			}
		} else {
			NET_ERR("Remote address family not set");
			goto error;
		}

		flags |= NET_CONN_REMOTE_ADDR_SET;
	}

	if (local_addr) {
		if (IS_ENABLED(CONFIG_NET_IPV6) &&
		    local_addr->sa_family == AF_INET6) {
			memcpy(&conn->local_addr, local_addr,
			       sizeof(struct sockaddr_in6));

			if (!net_ipv6_is_addr_unspecified(
				    &net_sin6(local_addr)->
				    sin6_addr)) {
				flags |= NET_CONN_LOCAL_ADDR_SPEC;
			}
		} else if (IS_ENABLED(CONFIG_NET_IPV4) &&
			   local_addr->sa_family == AF_INET) {
			memcpy(&conn->local_addr, local_addr,
			       sizeof(struct sockaddr_in));

			if (net_sin(local_addr)->sin_addr.s_addr) {
				flags |= NET_CONN_LOCAL_ADDR_SPEC;
			}
		} else {
			NET_ERR("Local address family not set");
			goto error;
		}

		flags |= NET_CONN_LOCAL_ADDR_SET;
	}

	if (remote_addr && local_addr) {
		if (remote_addr->sa_family != local_addr->sa_family) {
			NET_ERR("Address families different");
			goto error;
		}
	}

	if (remote_port) {
		flags |= NET_CONN_REMOTE_PORT_SPEC;
		net_sin(&conn->remote_addr)->sin_port = htons(remote_port);
	}

	if (local_port) {
		flags |= NET_CONN_LOCAL_PORT_SPEC;
		net_sin(&conn->local_addr)->sin_port = htons(local_port);
	}

	conn->cb = cb;
	conn->user_data = user_data;
	conn->flags = flags;
	conn->proto = proto;
	conn->family = family;

	if (handle) {
		*handle = (struct net_conn_handle *)conn;
	}

	conn_set_used(conn);

	conn_register_debug(conn, remote_port, local_port);

	return 0;
error:
	conn_set_unused(conn);
	return -EINVAL;
}

int net_conn_unregister(struct net_conn_handle *handle)
{
	struct net_conn *conn = (struct net_conn *)handle;

	if (conn < &conns[0] || conn > &conns[CONFIG_NET_MAX_CONN]) {
		return -EINVAL;
	}

	if (!(conn->flags & NET_CONN_IN_USE)) {
		return -ENOENT;
	}

	NET_DBG("Connection handler %p removed", conn);

	sys_slist_find_and_remove(&conn_used, &conn->node);

	conn_set_unused(conn);

	return 0;
}

int net_conn_change_callback(struct net_conn_handle *handle,
			     net_conn_cb_t cb, void *user_data)
{
	struct net_conn *conn = (struct net_conn *)handle;

	if (conn < &conns[0] || conn > &conns[CONFIG_NET_MAX_CONN]) {
		return -EINVAL;
	}

	if (!(conn->flags & NET_CONN_IN_USE)) {
		return -ENOENT;
	}

	NET_DBG("[%zu] connection handler %p changed callback",
		conn - conns, conn);

	conn->cb = cb;
	conn->user_data = user_data;

	return 0;
}

static bool conn_addr_cmp(struct net_pkt *pkt,
			  union net_ip_header *ip_hdr,
			  struct sockaddr *addr,
			  bool is_remote)
{
	if (addr->sa_family != net_pkt_family(pkt)) {
		return false;
	}

	if (IS_ENABLED(CONFIG_NET_IPV6) &&
	    net_pkt_family(pkt) == AF_INET6 &&
	    addr->sa_family == AF_INET6) {
		struct in6_addr *addr6;

		if (is_remote) {
			addr6 = &ip_hdr->ipv6->src;
		} else {
			addr6 = &ip_hdr->ipv6->dst;
		}

		if (!net_ipv6_is_addr_unspecified(
			    &net_sin6(addr)->sin6_addr)) {
			if (!net_ipv6_addr_cmp(&net_sin6(addr)->sin6_addr,
					       addr6)) {
				return false;
			}
		}

		return true;
	} else if (IS_ENABLED(CONFIG_NET_IPV4) &&
		   net_pkt_family(pkt) == AF_INET &&
		   addr->sa_family == AF_INET) {
		struct in_addr *addr4;

		if (is_remote) {
			addr4 = &ip_hdr->ipv4->src;
		} else {
			addr4 = &ip_hdr->ipv4->dst;
		}

		if (net_sin(addr)->sin_addr.s_addr) {
			if (!net_ipv4_addr_cmp(&net_sin(addr)->sin_addr,
					       addr4)) {
				return false;
			}
		}
	}

	return true;
}

static inline void conn_send_icmp_error(struct net_pkt *pkt)
{
	if (IS_ENABLED(CONFIG_NET_IPV6) && net_pkt_family(pkt) == AF_INET6) {
		net_icmpv6_send_error(pkt, NET_ICMPV6_DST_UNREACH,
				      NET_ICMPV6_DST_UNREACH_NO_PORT, 0);

	} else if (IS_ENABLED(CONFIG_NET_IPV4)) {
		net_icmpv4_send_error(pkt, NET_ICMPV4_DST_UNREACH,
				      NET_ICMPV4_DST_UNREACH_NO_PORT);
	}
}

static bool conn_are_end_points_valid(struct net_pkt *pkt,
				      union net_ip_header *ip_hdr,
				      u16_t src_port,
				      u16_t dst_port)
{
	bool my_src_addr = false;

	/* For AF_PACKET family, we are not parsing headers. */
	if (IS_ENABLED(CONFIG_NET_SOCKETS_PACKET) &&
	    net_pkt_family(pkt) == AF_PACKET) {
		return true;
	}

	if (IS_ENABLED(CONFIG_NET_SOCKETS_CAN) &&
	    net_pkt_family(pkt) == AF_CAN) {
		return true;
	}

	if (IS_ENABLED(CONFIG_NET_IPV4) && net_pkt_family(pkt) == AF_INET) {
		if (net_ipv4_addr_cmp(&ip_hdr->ipv4->src,
				      &ip_hdr->ipv4->dst) ||
		    net_ipv4_is_my_addr(&ip_hdr->ipv4->src)) {
			my_src_addr = true;
		}
	} else if (IS_ENABLED(CONFIG_NET_IPV6) &&
		   net_pkt_family(pkt) == AF_INET6) {
		if (net_ipv6_addr_cmp(&ip_hdr->ipv6->src,
				      &ip_hdr->ipv6->dst) ||
		    net_ipv6_is_my_addr(&ip_hdr->ipv6->src)) {
			my_src_addr = true;
		}
	}

	return !(my_src_addr && (src_port == dst_port));
}

enum net_verdict net_conn_input(struct net_pkt *pkt,
				union net_ip_header *ip_hdr,
				u8_t proto,
				union net_proto_header *proto_hdr)
{
	struct net_if *pkt_iface = net_pkt_iface(pkt);
	struct net_conn *best_match = NULL;
	s16_t best_rank = -1;
	struct net_conn *conn;
	u16_t src_port;
	u16_t dst_port;

	if (IS_ENABLED(CONFIG_NET_UDP) && proto == IPPROTO_UDP) {
		src_port = proto_hdr->udp->src_port;
		dst_port = proto_hdr->udp->dst_port;
	} else if (IS_ENABLED(CONFIG_NET_TCP) && proto == IPPROTO_TCP) {
		src_port = proto_hdr->tcp->src_port;
		dst_port = proto_hdr->tcp->dst_port;
	} else if (IS_ENABLED(CONFIG_NET_SOCKETS_PACKET)) {
		if (net_pkt_family(pkt) != AF_PACKET || proto != ETH_P_ALL) {
			return NET_DROP;
		}

		src_port = dst_port = 0U;
	} else if (IS_ENABLED(CONFIG_NET_SOCKETS_CAN) &&
		   net_pkt_family(pkt) == AF_CAN) {
		if (proto != CAN_RAW) {
			return NET_DROP;
		}

		src_port = dst_port = 0U;
	} else {
		NET_DBG("No suitable protocol handler configured");
		return NET_DROP;
	}

	if (!conn_are_end_points_valid(pkt, ip_hdr, src_port, dst_port)) {
		NET_DBG("Dropping invalid src/dst end-points packet");
		return NET_DROP;
	}

	/* TODO: Make core part of networing subsystem less dependent on
	 * UDP, TCP, IPv4 or IPv6. So that we can add new features with
	 * less cross-module changes.
	 */
	NET_DBG("Check %s listener for pkt %p src port %u dst port %u"
		" family %d", net_proto2str(net_pkt_family(pkt), proto), pkt,
		ntohs(src_port), ntohs(dst_port), net_pkt_family(pkt));

	SYS_SLIST_FOR_EACH_CONTAINER(&conn_used, conn, node) {
		if (conn->proto != proto) {
			continue;
		}

		if (conn->family != AF_UNSPEC &&
		    conn->family != net_pkt_family(pkt)) {
			continue;
		}

		if (IS_ENABLED(CONFIG_NET_UDP) ||
		    IS_ENABLED(CONFIG_NET_TCP)) {
			if (net_sin(&conn->remote_addr)->sin_port) {
				if (net_sin(&conn->remote_addr)->sin_port !=
				    src_port) {
					continue;
				}
			}

			if (net_sin(&conn->local_addr)->sin_port) {
				if (net_sin(&conn->local_addr)->sin_port !=
				    dst_port) {
					continue;
				}
			}

			if (conn->flags & NET_CONN_REMOTE_ADDR_SET) {
				if (!conn_addr_cmp(pkt, ip_hdr,
						   &conn->remote_addr,
						   true)) {
					continue;
				}
			}

			if (conn->flags & NET_CONN_LOCAL_ADDR_SET) {
				if (!conn_addr_cmp(pkt, ip_hdr,
						   &conn->local_addr,
						   false)) {
					continue;
				}
			}

			/* If we have an existing best_match, and that one
			 * specifies a remote port, then we've matched to a
			 * LISTENING connection that should not override.
			 */
			if (best_match != NULL &&
			    best_match->flags & NET_CONN_REMOTE_PORT_SPEC) {
				continue;
			}

			if (best_rank < NET_CONN_RANK(conn->flags)) {
				best_rank = NET_CONN_RANK(conn->flags);
				best_match = conn;
			}
		} else if (IS_ENABLED(CONFIG_NET_SOCKETS_PACKET) ||
			   IS_ENABLED(CONFIG_NET_SOCKETS_CAN)) {
			best_rank = 0;
			best_match = conn;
		}
	}

	conn = best_match;
	if (conn) {
		NET_DBG("[%p] match found cb %p ud %p rank 0x%02x",
			conn, conn->cb, conn->user_data, conn->flags);

		if (conn->cb(conn, pkt, ip_hdr, proto_hdr,
			     conn->user_data) == NET_DROP) {
			goto drop;
		}

		net_stats_update_per_proto_recv(pkt_iface, proto);

		return NET_OK;
	}

	NET_DBG("No match found.");

	/* If the destination address is multicast address,
	 * we will not send an ICMP error as that makes no sense.
	 */
	if (IS_ENABLED(CONFIG_NET_IPV6) &&
	    net_pkt_family(pkt) == AF_INET6 &&
	    net_ipv6_is_addr_mcast(&ip_hdr->ipv6->dst)) {
		;
	} else if (IS_ENABLED(CONFIG_NET_IPV4) &&
		   net_pkt_family(pkt) == AF_INET &&
		   net_ipv4_is_addr_mcast(&ip_hdr->ipv4->dst)) {
		;
	} else if (IS_ENABLED(CONFIG_NET_SOCKETS_PACKET) &&
		    net_pkt_family(pkt) == AF_PACKET) {
		;
	} else {
		conn_send_icmp_error(pkt);

		if (IS_ENABLED(CONFIG_NET_TCP) && proto == IPPROTO_TCP) {
			net_stats_update_tcp_seg_connrst(net_pkt_iface(pkt));
		}
	}

drop:
	net_stats_update_per_proto_drop(pkt_iface, proto);

	return NET_DROP;
}

void net_conn_foreach(net_conn_foreach_cb_t cb, void *user_data)
{
	struct net_conn *conn;

	SYS_SLIST_FOR_EACH_CONTAINER(&conn_used, conn, node) {
		cb(conn, user_data);
	}
}

void net_conn_init(void)
{
	int i;

	sys_slist_init(&conn_unused);
	sys_slist_init(&conn_used);

	for (i = 0; i < CONFIG_NET_MAX_CONN; i++) {
		sys_slist_prepend(&conn_unused, &conns[i].node);
	}
}
