/** @file
 * @brief Network context API
 *
 * An API for applications to define a network connection.
 */

/*
 * Copyright (c) 2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#if defined(CONFIG_NET_DEBUG_CONTEXT)
#define SYS_LOG_DOMAIN "net/ctx"
#define NET_LOG_ENABLED 1
#endif

#include <kernel.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>

#include <net/net_pkt.h>
#include <net/net_ip.h>
#include <net/net_context.h>
#include <net/net_offload.h>

#include "connection.h"
#include "net_private.h"

#include "ipv6.h"
#include "ipv4.h"
#include "udp_internal.h"
#include "tcp.h"
#include "net_stats.h"

#ifndef EPFNOSUPPORT
/* Some old versions of newlib haven't got this defined in errno.h,
 * Just use EPROTONOSUPPORT in this case
 */
#define EPFNOSUPPORT EPROTONOSUPPORT
#endif

#define NET_MAX_CONTEXT CONFIG_NET_MAX_CONTEXTS

#if defined(CONFIG_NET_TCP_ACK_TIMEOUT)
#define ACK_TIMEOUT CONFIG_NET_TCP_ACK_TIMEOUT
#else
#define ACK_TIMEOUT K_SECONDS(1)
#endif

/* TODO: It should be 2 * MSL (Maximum segment lifetime) */
#define TIMEWAIT_TIMEOUT MSEC(250)

#define FIN_TIMEOUT K_SECONDS(1)

/* Declares a wrapper function for a net_conn callback that refs the
 * context around the invocation (to protect it from premature
 * deletion).  Long term would be nice to see this feature be part of
 * the connection type itself, but right now it has opaque "user_data"
 * pointers and doesn't understand what a net_context is.
 */
#define NET_CONN_CB(name) \
	static enum net_verdict _##name(struct net_conn *conn,	  \
					struct net_pkt *pkt,	  \
					void *user_data);	  \
	static enum net_verdict name(struct net_conn *conn,	  \
				     struct net_pkt *pkt,	  \
				     void *user_data)		  \
	{							  \
		enum net_verdict result;			  \
								  \
		net_context_ref(user_data);			  \
		result = _##name(conn, pkt, user_data);		  \
		net_context_unref(user_data);			  \
		return result;					  \
	}							  \
	static enum net_verdict _##name(struct net_conn *conn,    \
					struct net_pkt *pkt,      \
					void *user_data)	  \


static struct net_context contexts[NET_MAX_CONTEXT];

/* We need to lock the contexts array as these APIs are typically called
 * from applications which are usually run in task context.
 */
static struct k_sem contexts_lock;

static enum net_verdict packet_received(struct net_conn *conn,
					struct net_pkt *pkt,
					void *user_data);

static void set_appdata_values(struct net_pkt *pkt, enum net_ip_protocol proto);

#if defined(CONFIG_NET_TCP)
static int send_reset(struct net_context *context, struct sockaddr *local,
		      struct sockaddr *remote);

static struct tcp_backlog_entry {
	struct net_tcp *tcp;
	struct sockaddr remote;
	u32_t send_seq;
	u32_t send_ack;
	u16_t send_mss;
	struct k_delayed_work ack_timer;
} tcp_backlog[CONFIG_NET_TCP_BACKLOG_SIZE];

static void backlog_ack_timeout(struct k_work *work)
{
	struct tcp_backlog_entry *backlog =
		CONTAINER_OF(work, struct tcp_backlog_entry, ack_timer);

	NET_DBG("Did not receive ACK in %dms", ACK_TIMEOUT);

	/* TODO: If net_context is bound to unspecified IPv6 address
	 * and some port number, local address is not available.
	 * RST packet might be invalid. Cache local address
	 * and use it in RST message preparation.
	 */
	send_reset(backlog->tcp->context, NULL, &backlog->remote);

	memset(backlog, 0, sizeof(struct tcp_backlog_entry));
}

static int tcp_backlog_find(struct net_pkt *pkt, int *empty_slot)
{
	struct net_tcp_hdr hdr, *tcp_hdr;
	int i, empty = -1;

	for (i = 0; i < CONFIG_NET_TCP_BACKLOG_SIZE; i++) {
		if (tcp_backlog[i].tcp == NULL && empty < 0) {
			empty = i;
			continue;
		}

		if (net_pkt_family(pkt) != tcp_backlog[i].remote.sa_family) {
			continue;
		}

		tcp_hdr = net_tcp_get_hdr(pkt, &hdr);
		if (!tcp_hdr) {
			return -EINVAL;
		}

		switch (net_pkt_family(pkt)) {
#if defined(CONFIG_NET_IPV6)
		case AF_INET6:
			if (net_sin6(&tcp_backlog[i].remote)->sin6_port !=
			    tcp_hdr->src_port) {
				continue;
			}

			if (memcmp(&net_sin6(&tcp_backlog[i].remote)->sin6_addr,
				   &NET_IPV6_HDR(pkt)->src,
				   sizeof(struct in6_addr))) {
				continue;
			}

			break;
#endif
#if defined(CONFIG_NET_IPV4)
		case AF_INET:
			if (net_sin(&tcp_backlog[i].remote)->sin_port !=
			    tcp_hdr->src_port) {
				continue;
			}

			if (memcmp(&net_sin(&tcp_backlog[i].remote)->sin_addr,
				   &NET_IPV4_HDR(pkt)->src,
				   sizeof(struct in_addr))) {
				continue;
			}

			break;
#endif
		}

		return i;
	}

	if (empty_slot) {
		*empty_slot = empty;
	}

	return -EADDRNOTAVAIL;
}

static int tcp_backlog_syn(struct net_pkt *pkt, struct net_context *context,
			   u16_t send_mss)
{
	int empty_slot = -1;
	int ret;

	if (tcp_backlog_find(pkt, &empty_slot) >= 0) {
		return -EADDRINUSE;
	}

	if (empty_slot < 0) {
		return -ENOSPC;
	}

	tcp_backlog[empty_slot].tcp = context->tcp;

	ret = net_pkt_get_src_addr(pkt, &tcp_backlog[empty_slot].remote,
				   sizeof(tcp_backlog[empty_slot].remote));
	if (ret < 0) {
		/* Release the assigned empty slot */
		tcp_backlog[empty_slot].tcp = NULL;

		return ret;
	}

	tcp_backlog[empty_slot].send_seq = context->tcp->send_seq;
	tcp_backlog[empty_slot].send_ack = context->tcp->send_ack;
	tcp_backlog[empty_slot].send_mss = send_mss;

	k_delayed_work_init(&tcp_backlog[empty_slot].ack_timer,
			    backlog_ack_timeout);
	k_delayed_work_submit(&tcp_backlog[empty_slot].ack_timer, ACK_TIMEOUT);

	return 0;
}

static int tcp_backlog_ack(struct net_pkt *pkt, struct net_context *context)
{
	struct net_tcp_hdr hdr, *tcp_hdr;
	int r;

	r = tcp_backlog_find(pkt, NULL);

	if (r < 0) {
		return r;
	}

	tcp_hdr = net_tcp_get_hdr(pkt, &hdr);
	if (!tcp_hdr) {
		return -EINVAL;
	}

	/* Sent SEQ + 1 needs to be the same as the received ACK */
	if (tcp_backlog[r].send_seq + 1 != sys_get_be32(tcp_hdr->ack)) {
		return -EINVAL;
	}

	memcpy(&context->remote, &tcp_backlog[r].remote,
		sizeof(struct sockaddr));
	context->tcp->send_seq = tcp_backlog[r].send_seq + 1;
	context->tcp->send_ack = tcp_backlog[r].send_ack;
	context->tcp->send_mss = tcp_backlog[r].send_mss;

	k_delayed_work_cancel(&tcp_backlog[r].ack_timer);
	memset(&tcp_backlog[r], 0, sizeof(struct tcp_backlog_entry));

	return 0;
}

static int tcp_backlog_rst(struct net_pkt *pkt)
{
	struct net_tcp_hdr hdr, *tcp_hdr;
	int r;

	r = tcp_backlog_find(pkt, NULL);
	if (r < 0) {
		return r;
	}

	tcp_hdr = net_tcp_get_hdr(pkt, &hdr);
	if (!tcp_hdr) {
		return -EINVAL;
	}

	/* The ACK sent needs to be the same as the received SEQ */
	if (tcp_backlog[r].send_ack != sys_get_be32(tcp_hdr->seq)) {
		return -EINVAL;
	}

	k_delayed_work_cancel(&tcp_backlog[r].ack_timer);
	memset(&tcp_backlog[r], 0, sizeof(struct tcp_backlog_entry));

	return 0;
}

static void handle_fin_timeout(struct k_work *work)
{
	struct net_tcp *tcp =
		CONTAINER_OF(work, struct net_tcp, fin_timer);

	NET_DBG("Did not receive FIN in %dms", FIN_TIMEOUT);

	net_context_unref(tcp->context);
}

static void handle_ack_timeout(struct k_work *work)
{
	/* This means that we did not receive ACK response in time. */
	struct net_tcp *tcp = CONTAINER_OF(work, struct net_tcp, ack_timer);

	NET_DBG("Did not receive ACK in %dms while in %s", ACK_TIMEOUT,
		net_tcp_state_str(net_tcp_get_state(tcp)));

	if (net_tcp_get_state(tcp) == NET_TCP_LAST_ACK) {
		/* We did not receive the last ACK on time. We can only
		 * close the connection at this point. We will not send
		 * anything to peer in this last state, but will go directly
		 * to to CLOSED state.
		 */
		net_tcp_change_state(tcp, NET_TCP_CLOSED);

		net_context_unref(tcp->context);
	}
}

static void handle_timewait_timeout(struct k_work *work)
{
	struct net_tcp *tcp = CONTAINER_OF(work, struct net_tcp,
					   timewait_timer);

	NET_DBG("Timewait expired in %dms", TIMEWAIT_TIMEOUT);

	if (net_tcp_get_state(tcp) == NET_TCP_TIME_WAIT) {
		net_tcp_change_state(tcp, NET_TCP_CLOSED);

		if (tcp->context->recv_cb) {
			tcp->context->recv_cb(tcp->context, NULL, 0,
					      tcp->recv_user_data);
		}

		net_context_unref(tcp->context);
	}
}

#endif /* CONFIG_NET_TCP */

static int check_used_port(enum net_ip_protocol ip_proto,
			   u16_t local_port,
			   const struct sockaddr *local_addr)

{
	int i;

	for (i = 0; i < NET_MAX_CONTEXT; i++) {
		if (!net_context_is_used(&contexts[i])) {
			continue;
		}

		if (!(net_context_get_ip_proto(&contexts[i]) == ip_proto &&
		      net_sin((struct sockaddr *)&
			      contexts[i].local)->sin_port == local_port)) {
			continue;
		}

		if (local_addr->sa_family == AF_INET6) {
			if (net_ipv6_addr_cmp(
				    net_sin6_ptr(&contexts[i].local)->
							     sin6_addr,
				    &((struct sockaddr_in6 *)
				      local_addr)->sin6_addr)) {
				return -EEXIST;
			}
		} else {
			if (net_ipv4_addr_cmp(
				    net_sin_ptr(&contexts[i].local)->
							      sin_addr,
				    &((struct sockaddr_in *)
				      local_addr)->sin_addr)) {
				return -EEXIST;
			}
		}
	}

	return 0;
}

static u16_t find_available_port(struct net_context *context,
				    const struct sockaddr *addr)
{
	u16_t local_port;

	do {
		local_port = sys_rand32_get() | 0x8000;
		if (local_port <= 1023) {
			/* 0 - 1023 ports are reserved */
			continue;
		}
	} while (check_used_port(
				net_context_get_ip_proto(context),
				htons(local_port), addr) == -EEXIST);

	return htons(local_port);
}

int net_context_get(sa_family_t family,
		    enum net_sock_type type,
		    enum net_ip_protocol ip_proto,
		    struct net_context **context)
{
	int i, ret = -ENOENT;

#if defined(CONFIG_NET_CONTEXT_CHECK)

#if !defined(CONFIG_NET_IPV4)
	if (family == AF_INET) {
		NET_ASSERT_INFO(family != AF_INET, "IPv4 disabled");
		return -EPFNOSUPPORT;
	}
#endif

#if !defined(CONFIG_NET_IPV6)
	if (family == AF_INET6) {
		NET_ASSERT_INFO(family != AF_INET6, "IPv6 disabled");
		return -EPFNOSUPPORT;
	}
#endif

#if !defined(CONFIG_NET_UDP)
	if (type == SOCK_DGRAM) {
		NET_ASSERT_INFO(type != SOCK_DGRAM,
				"Datagram context disabled");
		return -EPROTOTYPE;
	}

	if (ip_proto == IPPROTO_UDP) {
		NET_ASSERT_INFO(ip_proto != IPPROTO_UDP, "UDP disabled");
		return -EPROTONOSUPPORT;
	}
#endif

#if !defined(CONFIG_NET_TCP)
	if (type == SOCK_STREAM) {
		NET_ASSERT_INFO(type != SOCK_STREAM,
				"Stream context disabled");
		return -EPROTOTYPE;
	}

	if (ip_proto == IPPROTO_TCP) {
		NET_ASSERT_INFO(ip_proto != IPPROTO_TCP, "TCP disabled");
		return -EPROTONOSUPPORT;
	}
#endif

	if (family != AF_INET && family != AF_INET6) {
		NET_ASSERT_INFO(family == AF_INET || family == AF_INET6,
				"Unknown address family %d", family);
		return -EAFNOSUPPORT;
	}

	if (type != SOCK_DGRAM && type != SOCK_STREAM) {
		NET_ASSERT_INFO(type == SOCK_DGRAM || type == SOCK_STREAM,
				"Unknown context type");
		return -EPROTOTYPE;
	}

	if (ip_proto != IPPROTO_UDP && ip_proto != IPPROTO_TCP) {
		NET_ASSERT_INFO(ip_proto == IPPROTO_UDP ||
				ip_proto == IPPROTO_TCP,
				"Unknown IP protocol %d", ip_proto);
		return -EPROTONOSUPPORT;
	}

	if ((type == SOCK_STREAM && ip_proto == IPPROTO_UDP) ||
	    (type == SOCK_DGRAM && ip_proto == IPPROTO_TCP)) {
		NET_ASSERT_INFO(\
			(type != SOCK_STREAM || ip_proto != IPPROTO_UDP) &&
			(type != SOCK_DGRAM || ip_proto != IPPROTO_TCP),
			"Context type and protocol mismatch, type %d proto %d",
			type, ip_proto);
		return -EOPNOTSUPP;
	}

	if (!context) {
		NET_ASSERT_INFO(context, "Invalid context");
		return -EINVAL;
	}
#endif /* CONFIG_NET_CONTEXT_CHECK */

	k_sem_take(&contexts_lock, K_FOREVER);

	for (i = 0; i < NET_MAX_CONTEXT; i++) {
		if (net_context_is_used(&contexts[i])) {
			continue;
		}

#if defined(CONFIG_NET_TCP)
		if (ip_proto == IPPROTO_TCP) {
			contexts[i].tcp = net_tcp_alloc(&contexts[i]);
			if (!contexts[i].tcp) {
				NET_ASSERT_INFO(contexts[i].tcp,
						"Cannot allocate TCP context");
				ret = -ENOBUFS;
				break;
			}

			k_delayed_work_init(&contexts[i].tcp->ack_timer,
					    handle_ack_timeout);
			k_delayed_work_init(&contexts[i].tcp->fin_timer,
					    handle_fin_timeout);
			k_delayed_work_init(&contexts[i].tcp->timewait_timer,
					    handle_timewait_timeout);
		}
#endif /* CONFIG_NET_TCP */

		contexts[i].iface = 0;
		contexts[i].flags = 0;
		atomic_set(&contexts[i].refcount, 1);

		net_context_set_family(&contexts[i], family);
		net_context_set_type(&contexts[i], type);
		net_context_set_ip_proto(&contexts[i], ip_proto);

		memset(&contexts[i].remote, 0, sizeof(struct sockaddr));
		memset(&contexts[i].local, 0, sizeof(struct sockaddr_ptr));

#if defined(CONFIG_NET_IPV6)
		if (family == AF_INET6) {
			struct sockaddr_in6 *addr6 = (struct sockaddr_in6
						      *)&contexts[i].local;
			addr6->sin6_port = find_available_port(&contexts[i],
						    (struct sockaddr *)addr6);

			if (!addr6->sin6_port) {
				ret = -EADDRINUSE;
				break;
			}
		}
#endif

#if defined(CONFIG_NET_IPV4)
		if (family == AF_INET) {
			struct sockaddr_in *addr = (struct sockaddr_in
						      *)&contexts[i].local;
			addr->sin_port = find_available_port(&contexts[i],
						    (struct sockaddr *)addr);

			if (!addr->sin_port) {
				ret = -EADDRINUSE;
				break;
			}
		}
#endif

#if defined(CONFIG_NET_CONTEXT_SYNC_RECV)
		k_sem_init(&contexts[i].recv_data_wait, 1, UINT_MAX);
#endif /* CONFIG_NET_CONTEXT_SYNC_RECV */

		contexts[i].flags |= NET_CONTEXT_IN_USE;
		*context = &contexts[i];

		ret = 0;
		break;
	}

	k_sem_give(&contexts_lock);

#if defined(CONFIG_NET_OFFLOAD)
	/* FIXME - Figure out a way to get the correct network interface
	 * as it is not known at this point yet.
	 */
	if (!ret && net_if_is_ip_offloaded(net_if_get_default())) {
		ret = net_offload_get(net_if_get_default(),
				      family,
				      type,
				      ip_proto,
				      context);
		if (ret < 0) {
			(*context)->flags &= ~NET_CONTEXT_IN_USE;
			*context = NULL;
		}

		return ret;
	}
#endif /* CONFIG_NET_OFFLOAD */

	return ret;
}

#if defined(CONFIG_NET_TCP)
static void queue_fin(struct net_context *ctx)
{
	struct net_pkt *pkt = NULL;
	int ret;

	ret = net_tcp_prepare_segment(ctx->tcp, NET_TCP_FIN, NULL, 0,
				      NULL, &ctx->remote, &pkt);
	if (ret || !pkt) {
		return;
	}

	ret = net_tcp_send_pkt(pkt);
	if (ret < 0) {
		net_pkt_unref(pkt);
	}
}

#endif /* CONFIG_NET_TCP */

int net_context_ref(struct net_context *context)
{
	int old_rc = atomic_inc(&context->refcount);

	return old_rc + 1;
}

int net_context_unref(struct net_context *context)
{
	int old_rc = atomic_dec(&context->refcount);

	if (old_rc != 1) {
		return old_rc - 1;
	}

	k_sem_take(&contexts_lock, K_FOREVER);

#if defined(CONFIG_NET_TCP)
	if (context->tcp) {
		int i;

		/* Clear the backlog for this TCP context. */
		for (i = 0; i < CONFIG_NET_TCP_BACKLOG_SIZE; i++) {
			if (tcp_backlog[i].tcp != context->tcp) {
				continue;
			}

			k_delayed_work_cancel(&tcp_backlog[i].ack_timer);
			memset(&tcp_backlog[i], 0, sizeof(tcp_backlog[i]));
		}

		net_tcp_release(context->tcp);
		context->tcp = NULL;
	}
#endif /* CONFIG_NET_TCP */

	if (context->conn_handler) {
		net_conn_unregister(context->conn_handler);
		context->conn_handler = NULL;
	}

	net_context_set_state(context, NET_CONTEXT_UNCONNECTED);

	context->flags &= ~NET_CONTEXT_IN_USE;

	NET_DBG("Context %p released", context);

	k_sem_give(&contexts_lock);

	return 0;
}

int net_context_put(struct net_context *context)
{
	NET_ASSERT(context);

	if (!PART_OF_ARRAY(contexts, context)) {
		return -EINVAL;
	}

#if defined(CONFIG_NET_OFFLOAD)
	if (net_if_is_ip_offloaded(net_context_get_iface(context))) {
		k_sem_take(&contexts_lock, K_FOREVER);
		context->flags &= ~NET_CONTEXT_IN_USE;
		k_sem_give(&contexts_lock);
		return net_offload_put(
			net_context_get_iface(context), context);
	}
#endif /* CONFIG_NET_OFFLOAD */

	context->connect_cb = NULL;
	context->recv_cb = NULL;
	context->send_cb = NULL;

#if defined(CONFIG_NET_TCP)
	if (net_context_get_ip_proto(context) == IPPROTO_TCP) {
		if ((net_context_get_state(context) == NET_CONTEXT_CONNECTED ||
		     net_context_get_state(context) == NET_CONTEXT_LISTENING)
		    && !context->tcp->fin_rcvd) {
			NET_DBG("TCP connection in active close, not "
				"disposing yet (waiting %dms)", FIN_TIMEOUT);
			k_delayed_work_submit(&context->tcp->fin_timer,
					      FIN_TIMEOUT);
			queue_fin(context);
			return 0;
		}
	}
#endif /* CONFIG_NET_TCP */

	net_context_unref(context);
	return 0;
}

/* If local address is not bound, bind it to INADDR_ANY and random port. */
static int bind_default(struct net_context *context)
{
	sa_family_t family = net_context_get_family(context);

#if defined(CONFIG_NET_IPV6)
	if (family == AF_INET6) {
		struct sockaddr_in6 addr6;

		if (net_sin6_ptr(&context->local)->sin6_addr) {
			return 0;
		}

		addr6.sin6_family = AF_INET6;
		memcpy(&addr6.sin6_addr, net_ipv6_unspecified_address(),
		       sizeof(addr6.sin6_addr));
		addr6.sin6_port =
			find_available_port(context,
					    (struct sockaddr *)&addr6);

		return net_context_bind(context, (struct sockaddr *)&addr6,
					sizeof(addr6));
	}
#endif

#if defined(CONFIG_NET_IPV4)
	if (family == AF_INET) {
		struct sockaddr_in addr4;

		if (net_sin_ptr(&context->local)->sin_addr) {
			return 0;
		}

		addr4.sin_family = AF_INET;
		addr4.sin_addr.s_addr = INADDR_ANY;
		addr4.sin_port =
			find_available_port(context,
					    (struct sockaddr *)&addr4);

		return net_context_bind(context, (struct sockaddr *)&addr4,
					sizeof(addr4));
	}
#endif

	return -EINVAL;
}

int net_context_bind(struct net_context *context, const struct sockaddr *addr,
		     socklen_t addrlen)
{
	NET_ASSERT(addr);
	NET_ASSERT(PART_OF_ARRAY(contexts, context));

	/* If we already have connection handler, then it effectively
	 * means that it's already bound to an interface/port, and we
	 * don't support rebinding connection to new address/port in
	 * the code below.
	 * TODO: Support rebinding.
	 */
	if (context->conn_handler) {
		return -EISCONN;
	}

#if defined(CONFIG_NET_IPV6)
	if (addr->sa_family == AF_INET6) {
		struct net_if *iface = NULL;
		struct in6_addr *ptr;
		struct sockaddr_in6 *addr6 = (struct sockaddr_in6 *)addr;
		int ret;

		if (addrlen < sizeof(struct sockaddr_in6)) {
			return -EINVAL;
		}

		if (net_is_ipv6_addr_mcast(&addr6->sin6_addr)) {
			struct net_if_mcast_addr *maddr;

			maddr = net_if_ipv6_maddr_lookup(&addr6->sin6_addr,
							 &iface);
			if (!maddr) {
				return -ENOENT;
			}

			ptr = &maddr->address.in6_addr;

		} else if (net_is_ipv6_addr_unspecified(&addr6->sin6_addr)) {
			iface = net_if_get_default();

			ptr = (struct in6_addr *)net_ipv6_unspecified_address();
		} else {
			struct net_if_addr *ifaddr;

			ifaddr = net_if_ipv6_addr_lookup(&addr6->sin6_addr,
							 &iface);
			if (!ifaddr) {
				return -ENOENT;
			}

			ptr = &ifaddr->address.in6_addr;
		}

		if (!iface) {
			NET_ERR("Cannot bind to %s",
				net_sprint_ipv6_addr(&addr6->sin6_addr));

			return -EADDRNOTAVAIL;
		}

#if defined(CONFIG_NET_OFFLOAD)
		if (net_if_is_ip_offloaded(iface)) {
			net_context_set_iface(context, iface);

			return net_offload_bind(iface,
						context,
						addr,
						addrlen);
		}
#endif /* CONFIG_NET_OFFLOAD */

		net_context_set_iface(context, iface);

		net_sin6_ptr(&context->local)->sin6_family = AF_INET6;
		net_sin6_ptr(&context->local)->sin6_addr = ptr;
		if (addr6->sin6_port) {
			ret = check_used_port(AF_INET6, addr6->sin6_port,
					      addr);
			if (!ret) {
				net_sin6_ptr(&context->local)->sin6_port =
					addr6->sin6_port;
			} else {
				NET_ERR("Port %d is in use!",
					ntohs(addr6->sin6_port));
				return ret;
			}
		} else {
			addr6->sin6_port =
				net_sin6_ptr(&context->local)->sin6_port;
		}

		NET_DBG("Context %p binding to %s [%s]:%d iface %p",
			context,
			net_proto2str(net_context_get_ip_proto(context)),
			net_sprint_ipv6_addr(ptr), ntohs(addr6->sin6_port),
			iface);

		return 0;
	}
#endif

#if defined(CONFIG_NET_IPV4)
	if (addr->sa_family == AF_INET) {
		struct sockaddr_in *addr4 = (struct sockaddr_in *)addr;
		struct net_if *iface = NULL;
		struct net_if_addr *ifaddr;
		struct in_addr *ptr;
		int ret;

		if (addrlen < sizeof(struct sockaddr_in)) {
			return -EINVAL;
		}

		if (net_is_ipv4_addr_mcast(&addr4->sin_addr)) {
			struct net_if_mcast_addr *maddr;

			maddr = net_if_ipv4_maddr_lookup(&addr4->sin_addr,
							 &iface);
			if (!maddr) {
				return -ENOENT;
			}

			ptr = &maddr->address.in_addr;

		} else if (addr4->sin_addr.s_addr == INADDR_ANY) {
			iface = net_if_get_default();

			ptr = (struct in_addr *)net_ipv4_unspecified_address();
		} else {
			ifaddr = net_if_ipv4_addr_lookup(&addr4->sin_addr,
							 &iface);
			if (!ifaddr) {
				return -ENOENT;
			}

			ptr = &ifaddr->address.in_addr;
		}

		if (!iface) {
			NET_ERR("Cannot bind to %s",
				net_sprint_ipv4_addr(&addr4->sin_addr));

			return -EADDRNOTAVAIL;
		}

#if defined(CONFIG_NET_OFFLOAD)
		if (net_if_is_ip_offloaded(iface)) {
			net_context_set_iface(context, iface);

			return net_offload_bind(iface,
						context,
						addr,
						addrlen);
		}
#endif /* CONFIG_NET_OFFLOAD */

		net_context_set_iface(context, iface);

		net_sin_ptr(&context->local)->sin_family = AF_INET;
		net_sin_ptr(&context->local)->sin_addr = ptr;
		if (addr4->sin_port) {
			ret = check_used_port(AF_INET, addr4->sin_port,
					      addr);
			if (!ret) {
				net_sin_ptr(&context->local)->sin_port =
					addr4->sin_port;
			} else {
				NET_ERR("Port %d is in use!",
					ntohs(addr4->sin_port));
				return ret;
			}
		} else {
			addr4->sin_port =
				net_sin_ptr(&context->local)->sin_port;
		}

		NET_DBG("Context %p binding to %s %s:%d iface %p",
			context,
			net_proto2str(net_context_get_ip_proto(context)),
			net_sprint_ipv4_addr(ptr),
			ntohs(addr4->sin_port), iface);

		return 0;
	}
#endif

	return -EINVAL;
}

static inline struct net_context *find_context(void *conn_handler)
{
	int i;

	for (i = 0; i < NET_MAX_CONTEXT; i++) {
		if (!net_context_is_used(&contexts[i])) {
			continue;
		}

		if (contexts[i].conn_handler == conn_handler) {
			return &contexts[i];
		}
	}

	return NULL;
}

int net_context_listen(struct net_context *context, int backlog)
{
	ARG_UNUSED(backlog);

	NET_ASSERT(PART_OF_ARRAY(contexts, context));

	if (!net_context_is_used(context)) {
		return -EBADF;
	}

#if defined(CONFIG_NET_OFFLOAD)
	if (net_if_is_ip_offloaded(net_context_get_iface(context))) {
		return net_offload_listen(
			net_context_get_iface(context), context, backlog);
	}
#endif /* CONFIG_NET_OFFLOAD */

#if defined(CONFIG_NET_TCP)
	if (net_context_get_ip_proto(context) == IPPROTO_TCP) {
		net_tcp_change_state(context->tcp, NET_TCP_LISTEN);
		net_context_set_state(context, NET_CONTEXT_LISTENING);

		return 0;
	}
#endif

	return -EOPNOTSUPP;
}

#if defined(CONFIG_NET_TCP)
#if defined(CONFIG_NET_DEBUG_CONTEXT)
#define net_tcp_print_recv_info(str, pkt, port)				\
	do {								\
		if (net_context_get_family(context) == AF_INET6) {	\
			NET_DBG("%s received from %s port %d", str,	\
				net_sprint_ipv6_addr(&NET_IPV6_HDR(pkt)->src),\
				ntohs(port));				\
		} else if (net_context_get_family(context) == AF_INET) {\
			NET_DBG("%s received from %s port %d", str,	\
				net_sprint_ipv4_addr(&NET_IPV4_HDR(pkt)->src),\
				ntohs(port));				\
		}							\
	} while (0)

#define net_tcp_print_send_info(str, pkt, port)				\
	do {								\
		struct net_context *ctx = net_pkt_context(pkt);		\
		if (net_context_get_family(ctx) == AF_INET6) {		\
			NET_DBG("%s sent to %s port %d", str,		\
				net_sprint_ipv6_addr(&NET_IPV6_HDR(pkt)->dst),\
				ntohs(port));				\
		} else if (net_context_get_family(ctx) == AF_INET) {	\
			NET_DBG("%s sent to %s port %d", str,		\
				net_sprint_ipv4_addr(&NET_IPV4_HDR(pkt)->dst),\
				ntohs(port));				\
		}							\
	} while (0)

#else
#define net_tcp_print_recv_info(...)
#define net_tcp_print_send_info(...)
#endif /* CONFIG_NET_DEBUG_CONTEXT */

static void print_send_info(struct net_pkt *pkt, const char *msg)
{
	if (IS_ENABLED(CONFIG_NET_DEBUG_CONTEXT)) {
		struct net_tcp_hdr hdr, *tcp_hdr;

		tcp_hdr = net_tcp_get_hdr(pkt, &hdr);
		if (tcp_hdr) {
			net_tcp_print_send_info(msg, pkt, tcp_hdr->dst_port);
		}
	}
}

/* Send SYN or SYN/ACK. */
static inline int send_syn_segment(struct net_context *context,
				       const struct sockaddr_ptr *local,
				       const struct sockaddr *remote,
				       int flags, const char *msg)
{
	struct net_pkt *pkt = NULL;
	int ret;

	ret = net_tcp_prepare_segment(context->tcp, flags, NULL, 0,
				      local, remote, &pkt);
	if (ret) {
		return ret;
	}

	ret = net_send_data(pkt);
	if (ret < 0) {
		net_pkt_unref(pkt);
		return ret;
	}

	context->tcp->send_seq++;

	print_send_info(pkt, msg);

	return ret;
}

static inline int send_syn(struct net_context *context,
			   const struct sockaddr *remote)
{
	net_tcp_change_state(context->tcp, NET_TCP_SYN_SENT);

	return send_syn_segment(context, NULL, remote, NET_TCP_SYN, "SYN");
}

static inline int send_syn_ack(struct net_context *context,
			       struct sockaddr_ptr *local,
			       struct sockaddr *remote)
{
	return send_syn_segment(context, local, remote,
				    NET_TCP_SYN | NET_TCP_ACK,
				    "SYN_ACK");
}

static int send_ack(struct net_context *context,
		    struct sockaddr *remote, bool force)
{
	struct net_pkt *pkt = NULL;
	int ret;

	/* Something (e.g. a data transmission under the user
	 * callback) already sent the ACK, no need
	 */
	if (!force && context->tcp->send_ack == context->tcp->sent_ack) {
		return 0;
	}

	ret = net_tcp_prepare_ack(context->tcp, remote, &pkt);
	if (ret) {
		return ret;
	}

	print_send_info(pkt, "ACK");

	ret = net_tcp_send_pkt(pkt);
	if (ret < 0) {
		net_pkt_unref(pkt);
	}

	return ret;
}

static int send_reset(struct net_context *context,
		      struct sockaddr *local,
		      struct sockaddr *remote)
{
	struct net_pkt *pkt = NULL;
	int ret;

	ret = net_tcp_prepare_reset(context->tcp, local, remote, &pkt);
	if (ret || !pkt) {
		return ret;
	}

	print_send_info(pkt, "RST");

	ret = net_send_data(pkt);
	if (ret < 0) {
		net_pkt_unref(pkt);
	}

	return ret;
}

static int tcp_hdr_len(struct net_pkt *pkt)
{
	struct net_tcp_hdr hdr, *tcp_hdr;

	tcp_hdr = net_tcp_get_hdr(pkt, &hdr);
	if (tcp_hdr) {
		return NET_TCP_HDR_LEN(tcp_hdr);
	}

	return 0;
}

/* This is called when we receive data after the connection has been
 * established. The core TCP logic is located here.
 */
NET_CONN_CB(tcp_established)
{
	struct net_context *context = (struct net_context *)user_data;
	struct net_tcp_hdr hdr, *tcp_hdr;
	enum net_verdict ret = NET_OK;
	u8_t tcp_flags;
	u16_t data_len;

	NET_ASSERT(context && context->tcp);

	tcp_hdr = net_tcp_get_hdr(pkt, &hdr);
	if (!tcp_hdr) {
		return NET_DROP;
	}

	if (net_tcp_get_state(context->tcp) < NET_TCP_ESTABLISHED) {
		NET_ERR("Context %p in wrong state %d",
			context, net_tcp_get_state(context->tcp));
		return NET_DROP;
	}

	net_tcp_print_recv_info("DATA", pkt, tcp_hdr->src_port);

	tcp_flags = NET_TCP_FLAGS(tcp_hdr);

	if (net_tcp_seq_cmp(sys_get_be32(tcp_hdr->seq),
			    context->tcp->send_ack) < 0) {
		/* Peer sent us packet we've already seen. Apparently,
		 * our ack was lost.
		 */

		/* RFC793 specifies that "highest" (i.e. current from our PoV)
		 * ack # value can/should be sent, so we just force resend.
		 */
		send_ack(context, &conn->remote_addr, true);
		return NET_DROP;
	}

	if (net_tcp_seq_cmp(sys_get_be32(tcp_hdr->seq),
			    context->tcp->send_ack) > 0) {
		/* Don't try to reorder packets.  If it doesn't
		 * match the next segment exactly, drop and wait for
		 * retransmit
		 */
		return NET_DROP;
	}

	/*
	 * If we receive RST here, we close the socket. See RFC 793 chapter
	 * called "Reset Processing" for details.
	 */
	if (tcp_flags & NET_TCP_RST) {
		/* We only accept RST packet that has valid seq field. */
		if (!net_tcp_validate_seq(context->tcp, pkt)) {
			net_stats_update_tcp_seg_rsterr();
			return NET_DROP;
		}

		net_stats_update_tcp_seg_rst();

		net_tcp_print_recv_info("RST", pkt, tcp_hdr->src_port);

		if (context->recv_cb) {
			context->recv_cb(context, NULL, -ECONNRESET,
					 context->tcp->recv_user_data);
		}

		net_context_unref(context);

		return NET_DROP;
	}

	/* Handle TCP state transition */
	if (tcp_flags & NET_TCP_ACK) {
		if (!net_tcp_ack_received(context,
				     sys_get_be32(tcp_hdr->ack))) {
			return NET_DROP;
		}

		/* TCP state might be changed after maintaining the sent pkt
		 * list, e.g., an ack of FIN is received.
		 */

		if (net_tcp_get_state(context->tcp)
			   == NET_TCP_FIN_WAIT_1) {
			/* Received FIN on FIN_WAIT1, so cancel the timer */
			k_delayed_work_cancel(&context->tcp->fin_timer);
			/* Active close: step to FIN_WAIT_2 */
			net_tcp_change_state(context->tcp, NET_TCP_FIN_WAIT_2);
		} else if (net_tcp_get_state(context->tcp)
			   == NET_TCP_LAST_ACK) {
			/* Passive close: step to CLOSED */
			net_tcp_change_state(context->tcp, NET_TCP_CLOSED);
			/* Release the pkt before clean up */
			net_pkt_unref(pkt);
			goto clean_up;
		}
	}

	if (tcp_flags & NET_TCP_FIN) {
		if (net_tcp_get_state(context->tcp) == NET_TCP_ESTABLISHED) {
			/* Passive close: step to CLOSE_WAIT */
			net_tcp_change_state(context->tcp, NET_TCP_CLOSE_WAIT);

			/* We should receive ACK next in order to get rid of
			 * LAST_ACK state that we are entering in a short while.
			 * But we need to be prepared to NOT to receive it as
			 * otherwise the connection would be stuck forever.
			 */
			k_delayed_work_submit(&context->tcp->ack_timer, ACK_TIMEOUT);
		} else if (net_tcp_get_state(context->tcp)
			   == NET_TCP_FIN_WAIT_2) {
			/* Active close: step to TIME_WAIT */
			net_tcp_change_state(context->tcp, NET_TCP_TIME_WAIT);
		}

		context->tcp->fin_rcvd = 1;
	}

	set_appdata_values(pkt, IPPROTO_TCP);

	data_len = net_pkt_appdatalen(pkt);
	if (data_len > net_tcp_get_recv_wnd(context->tcp)) {
		NET_ERR("Context %p: overflow of recv window (%d vs %d), pkt dropped",
			context, net_tcp_get_recv_wnd(context->tcp), data_len);
		return NET_DROP;
	}

	/* If the pkt has appdata, notify the recv callback which should
	 * release the pkt. Otherwise, release the pkt immediately.
	 */
	if (data_len > 0) {
		ret = packet_received(conn, pkt, context->tcp->recv_user_data);
	} else if (data_len == 0) {
		net_pkt_unref(pkt);
	}

	/* Increment the ack */
	context->tcp->send_ack += data_len;
	if (tcp_flags & NET_TCP_FIN) {
		context->tcp->send_ack += 1;
	}

	send_ack(context, &conn->remote_addr, false);

clean_up:
	if (net_tcp_get_state(context->tcp) == NET_TCP_TIME_WAIT) {
		k_delayed_work_submit(&context->tcp->timewait_timer,
				      TIMEWAIT_TIMEOUT);
	}

	if (net_tcp_get_state(context->tcp) == NET_TCP_CLOSED) {
		if (context->recv_cb) {
			context->recv_cb(context, NULL, 0,
					 context->tcp->recv_user_data);
		}

		net_context_unref(context);
	}

	return ret;
}


NET_CONN_CB(tcp_synack_received)
{
	struct net_context *context = (struct net_context *)user_data;
	struct net_tcp_hdr hdr, *tcp_hdr;
	int ret;

	NET_ASSERT(context && context->tcp);

	switch (net_tcp_get_state(context->tcp)) {
	case NET_TCP_SYN_SENT:
		net_context_set_iface(context, net_pkt_iface(pkt));
		break;
	default:
		NET_DBG("Context %p in wrong state %d",
			context, net_tcp_get_state(context->tcp));
		return NET_DROP;
	}

	net_pkt_set_context(pkt, context);

	NET_ASSERT(net_pkt_iface(pkt));

	tcp_hdr = net_tcp_get_hdr(pkt, &hdr);
	if (!tcp_hdr) {
		return NET_DROP;
	}

	if (NET_TCP_FLAGS(tcp_hdr) & NET_TCP_RST) {
		/* We only accept RST packet that has valid seq field. */
		if (!net_tcp_validate_seq(context->tcp, pkt)) {
			net_stats_update_tcp_seg_rsterr();
			return NET_DROP;
		}

		net_stats_update_tcp_seg_rst();

		if (context->connect_cb) {
			context->connect_cb(context, -ECONNREFUSED,
					    context->user_data);
		}

		return NET_DROP;
	}

	if (NET_TCP_FLAGS(tcp_hdr) & NET_TCP_SYN) {
		context->tcp->send_ack =
			sys_get_be32(tcp_hdr->seq) + 1;
	}
	/*
	 * If we receive SYN, we send SYN-ACK and go to SYN_RCVD state.
	 */
	if (NET_TCP_FLAGS(tcp_hdr) == (NET_TCP_SYN | NET_TCP_ACK)) {
		/* Remove the temporary connection handler and register
		 * a proper now as we have an established connection.
		 */
		struct sockaddr local_addr;
		struct sockaddr remote_addr;

		if (net_pkt_get_src_addr(
			pkt, &remote_addr, sizeof(remote_addr)) < 0) {
			NET_DBG("Cannot parse remote address"
				" from received pkt");
			return NET_DROP;
		}

		if (net_pkt_get_dst_addr(
			pkt, &local_addr, sizeof(local_addr)) < 0) {
			NET_DBG("Cannot parse local address from received pkt");
			return NET_DROP;
		}

		net_tcp_unregister(context->conn_handler);

		ret = net_tcp_register(&remote_addr,
				       &local_addr,
				       ntohs(tcp_hdr->src_port),
				       ntohs(tcp_hdr->dst_port),
				       tcp_established,
				       context,
				       &context->conn_handler);
		if (ret < 0) {
			NET_DBG("Cannot register TCP handler (%d)", ret);
			send_reset(context, &local_addr, &remote_addr);
			return NET_DROP;
		}

		net_tcp_change_state(context->tcp, NET_TCP_ESTABLISHED);
		net_context_set_state(context, NET_CONTEXT_CONNECTED);

		send_ack(context, &remote_addr, false);

		k_sem_give(&context->tcp->connect_wait);

		if (context->connect_cb) {
			context->connect_cb(context, 0, context->user_data);
		}
	}

	return NET_DROP;
}

#endif /* CONFIG_NET_TCP */

int net_context_connect(struct net_context *context,
			const struct sockaddr *addr,
			socklen_t addrlen,
			net_context_connect_cb_t cb,
			s32_t timeout,
			void *user_data)
{
#if defined(CONFIG_NET_TCP)
	struct sockaddr *laddr = NULL;
	struct sockaddr local_addr;
	u16_t lport, rport;
#endif
	int ret;

	NET_ASSERT(addr);
	NET_ASSERT(PART_OF_ARRAY(contexts, context));
#if defined(CONFIG_NET_TCP)
	NET_ASSERT(context->tcp);
#endif

	if (!net_context_is_used(context)) {
		return -EBADF;
	}

	ret = bind_default(context);
	if (ret) {
		return ret;
	}

	if (addr->sa_family != net_context_get_family(context)) {
		NET_ASSERT_INFO(addr->sa_family == \
				net_context_get_family(context),
				"Family mismatch %d should be %d",
				addr->sa_family,
				net_context_get_family(context));
		return -EINVAL;
	}

#if defined(CONFIG_NET_OFFLOAD)
	if (net_if_is_ip_offloaded(net_context_get_iface(context))) {
		return net_offload_connect(
			net_context_get_iface(context),
			context,
			addr,
			addrlen,
			cb,
			timeout,
			user_data);
	}
#endif /* CONFIG_NET_OFFLOAD */

	if (net_context_get_state(context) == NET_CONTEXT_LISTENING) {
		return -EOPNOTSUPP;
	}

#if defined(CONFIG_NET_IPV6)
	if (net_context_get_family(context) == AF_INET6) {
		struct sockaddr_in6 *addr6 = (struct sockaddr_in6 *)
							&context->remote;

		if (addrlen < sizeof(struct sockaddr_in6)) {
			return -EINVAL;
		}

#if defined(CONFIG_NET_TCP)
		if (net_is_ipv6_addr_mcast(&addr6->sin6_addr)) {
			return -EADDRNOTAVAIL;
		}
#endif /* CONFIG_NET_TCP */

		memcpy(&addr6->sin6_addr, &net_sin6(addr)->sin6_addr,
		       sizeof(struct in6_addr));

		addr6->sin6_port = net_sin6(addr)->sin6_port;
		addr6->sin6_family = AF_INET6;

		if (!net_is_ipv6_addr_unspecified(&addr6->sin6_addr)) {
			context->flags |= NET_CONTEXT_REMOTE_ADDR_SET;
		} else {
			context->flags &= ~NET_CONTEXT_REMOTE_ADDR_SET;
		}

#if defined(CONFIG_NET_TCP)
		rport = addr6->sin6_port;

		net_sin6_ptr(&context->local)->sin6_family = AF_INET6;
		net_sin6(&local_addr)->sin6_family = AF_INET6;
		net_sin6(&local_addr)->sin6_port = lport =
			net_sin6((struct sockaddr *)&context->local)->sin6_port;

		if (net_sin6_ptr(&context->local)->sin6_addr) {
			net_ipaddr_copy(&net_sin6(&local_addr)->sin6_addr,
				     net_sin6_ptr(&context->local)->sin6_addr);

			laddr = &local_addr;
		}
#endif
	} else
#endif /* CONFIG_NET_IPV6 */

#if defined(CONFIG_NET_IPV4)
	if (net_context_get_family(context) == AF_INET) {
		struct sockaddr_in *addr4 = (struct sockaddr_in *)
							&context->remote;

		if (addrlen < sizeof(struct sockaddr_in)) {
			return -EINVAL;
		}

#if defined(CONFIG_NET_TCP)
		/* FIXME - Add multicast and broadcast address check */
#endif /* CONFIG_NET_TCP */

		memcpy(&addr4->sin_addr, &net_sin(addr)->sin_addr,
		       sizeof(struct in_addr));

		addr4->sin_port = net_sin(addr)->sin_port;
		addr4->sin_family = AF_INET;

		if (addr4->sin_addr.s_addr) {
			context->flags |= NET_CONTEXT_REMOTE_ADDR_SET;
		} else {
			context->flags &= ~NET_CONTEXT_REMOTE_ADDR_SET;
		}

#if defined(CONFIG_NET_TCP)
		rport = addr4->sin_port;

		net_sin_ptr(&context->local)->sin_family = AF_INET;
		net_sin(&local_addr)->sin_family = AF_INET;
		net_sin(&local_addr)->sin_port = lport =
			net_sin((struct sockaddr *)&context->local)->sin_port;

		if (net_sin_ptr(&context->local)->sin_addr) {
			net_ipaddr_copy(&net_sin(&local_addr)->sin_addr,
				       net_sin_ptr(&context->local)->sin_addr);

			laddr = &local_addr;
		}
#endif
	} else
#endif /* CONFIG_NET_IPV4 */

	{
		return -EINVAL; /* Not IPv4 or IPv6 */
	}

#if defined(CONFIG_NET_UDP)
	if (net_context_get_type(context) == SOCK_DGRAM) {
		if (cb) {
			cb(context, 0, user_data);
		}

		return 0;
	}
#endif /* CONFIG_NET_UDP */

#if defined(CONFIG_NET_TCP)
	if (net_context_get_type(context) != SOCK_STREAM) {
		return -ENOTSUP;
	}

	/* We need to register a handler, otherwise the SYN-ACK
	 * packet would not be received.
	 */
	ret = net_tcp_register(addr,
			       laddr,
			       ntohs(rport),
			       ntohs(lport),
			       tcp_synack_received,
			       context,
			       &context->conn_handler);
	if (ret < 0) {
		return ret;
	}

	context->connect_cb = cb;
	context->user_data = user_data;

	net_context_set_state(context, NET_CONTEXT_CONNECTING);

	send_syn(context, addr);


	/* in tcp_synack_received() we give back this semaphore */
	if (timeout != 0 && k_sem_take(&context->tcp->connect_wait, timeout)) {
		return -ETIMEDOUT;
	}
#endif

	return 0;
}

#if defined(CONFIG_NET_TCP)

static void pkt_get_sockaddr(sa_family_t family, struct net_pkt *pkt,
			     struct sockaddr_ptr *addr)
{
	struct net_tcp_hdr hdr, *tcp_hdr;

	tcp_hdr = net_tcp_get_hdr(pkt, &hdr);
	if (!tcp_hdr) {
		return;
	}

	memset(addr, 0, sizeof(*addr));

#if defined(CONFIG_NET_IPV4)
	if (family == AF_INET) {
		struct sockaddr_in_ptr *addr4 = net_sin_ptr(addr);

		addr4->sin_family = AF_INET;
		addr4->sin_port = tcp_hdr->dst_port;
		addr4->sin_addr = &NET_IPV4_HDR(pkt)->dst;
	}
#endif

#if defined(CONFIG_NET_IPV6)
	if (family == AF_INET6) {
		struct sockaddr_in6_ptr *addr6 = net_sin6_ptr(addr);

		addr6->sin6_family = AF_INET6;
		addr6->sin6_port = tcp_hdr->dst_port;
		addr6->sin6_addr = &NET_IPV6_HDR(pkt)->dst;
	}
#endif
}

#if defined(CONFIG_NET_CONTEXT_NET_PKT_POOL)
static inline void copy_pool_vars(struct net_context *new_context,
				  struct net_context *listen_context)
{
	new_context->tx_slab = listen_context->tx_slab;
	new_context->data_pool = listen_context->data_pool;
}
#else
#define copy_pool_vars(...)
#endif /* CONFIG_NET_CONTEXT_NET_PKT_POOL */

/* This callback is called when we are waiting connections and we receive
 * a packet. We need to check if we are receiving proper msg (SYN) here.
 * The ACK could also be received, in which case we have an established
 * connection.
 */
NET_CONN_CB(tcp_syn_rcvd)
{
	struct net_context *context = (struct net_context *)user_data;
	struct net_tcp_hdr hdr, *tcp_hdr;
	struct net_tcp *tcp;
	struct sockaddr_ptr pkt_src_addr;
	struct sockaddr local_addr;
	struct sockaddr remote_addr;

	NET_ASSERT(context && context->tcp);

	tcp = context->tcp;

	switch (net_tcp_get_state(tcp)) {
	case NET_TCP_LISTEN:
		net_context_set_iface(context, net_pkt_iface(pkt));
		break;
	case NET_TCP_SYN_RCVD:
		if (net_pkt_iface(pkt) != net_context_get_iface(context)) {
			return NET_DROP;
		}
		break;
	default:
		NET_DBG("Context %p in wrong state %d",
			context, tcp->state);
		return NET_DROP;
	}

	net_pkt_set_context(pkt, context);

	NET_ASSERT(net_pkt_iface(pkt));

	tcp_hdr = net_tcp_get_hdr(pkt, &hdr);
	if (!tcp_hdr) {
		return NET_DROP;
	}

	if (net_pkt_get_src_addr(pkt, &remote_addr, sizeof(remote_addr)) < 0) {
		NET_DBG("Cannot parse remote address from received pkt");
		return NET_DROP;
	}

	if (net_pkt_get_dst_addr(pkt, &local_addr, sizeof(local_addr)) < 0) {
		NET_DBG("Cannot parse local address from received pkt");
		return NET_DROP;
	}

	/*
	 * If we receive SYN, we send SYN-ACK and go to SYN_RCVD state.
	 */
	if (NET_TCP_FLAGS(tcp_hdr) == NET_TCP_SYN) {
		int r;
		int opt_totlen;
		struct net_tcp_options tcp_opts = {
			.mss = NET_TCP_DEFAULT_MSS,
		};

		net_tcp_print_recv_info("SYN", pkt, tcp_hdr->src_port);

		opt_totlen = NET_TCP_HDR_LEN(tcp_hdr)
			     - sizeof(struct net_tcp_hdr);
		/* We expect MSS option to be present (opt_totlen > 0),
		 * so call unconditionally.
		 */
		if (net_tcp_parse_opts(pkt, opt_totlen, &tcp_opts) < 0) {
			return NET_DROP;
		}

		net_tcp_change_state(tcp, NET_TCP_SYN_RCVD);

		/* Set TCP seq and ack which are then stored in the backlog */
		context->tcp->send_seq = tcp_init_isn();
		context->tcp->send_ack =
			sys_get_be32(tcp_hdr->seq) + 1;

		/* Get MSS from TCP options here*/

		r = tcp_backlog_syn(pkt, context, tcp_opts.mss);
		if (r < 0) {
			if (r == -EADDRINUSE) {
				NET_DBG("TCP connection already exists");
			} else {
				NET_DBG("No free TCP backlog entries");
			}

			return NET_DROP;
		}

		pkt_get_sockaddr(net_context_get_family(context),
				 pkt, &pkt_src_addr);
		send_syn_ack(context, &pkt_src_addr, &remote_addr);

		return NET_DROP;
	}

	/*
	 * See RFC 793 chapter 3.4 "Reset Processing" and RFC 793, page 65
	 * for more details.
	 */
	if (NET_TCP_FLAGS(tcp_hdr) == NET_TCP_RST) {

		if (tcp_backlog_rst(pkt) < 0) {
			net_stats_update_tcp_seg_rsterr();
			return NET_DROP;
		}

		net_stats_update_tcp_seg_rst();

		net_tcp_print_recv_info("RST", pkt, tcp_hdr->src_port);

		return NET_DROP;
	}

	/*
	 * If we receive ACK, we go to ESTABLISHED state.
	 */
	if (NET_TCP_FLAGS(tcp_hdr) == NET_TCP_ACK) {
		struct net_context *new_context;
		struct net_tcp *new_tcp;
		socklen_t addrlen;
		int ret;

		net_tcp_print_recv_info("ACK", pkt, tcp_hdr->src_port);

		if (!context->tcp->accept_cb) {
			NET_DBG("No accept callback, connection reset.");
			goto reset;
		}

		/* We create a new context that starts to wait data.
		 */
		ret = net_context_get(net_pkt_family(pkt),
				      SOCK_STREAM, IPPROTO_TCP,
				      &new_context);
		if (ret < 0) {
			NET_DBG("Cannot get accepted context, "
				"connection reset");
			goto conndrop;
		}

		ret = tcp_backlog_ack(pkt, new_context);
		if (ret < 0) {
			NET_DBG("Cannot find context from TCP backlog");

			net_context_unref(new_context);

			goto conndrop;
		}

		ret = net_context_bind(new_context, &local_addr,
				       sizeof(local_addr));
		if (ret < 0) {
			NET_DBG("Cannot bind accepted context, "
				"connection reset");
			net_context_unref(new_context);
			goto conndrop;
		}

		new_context->flags |= NET_CONTEXT_REMOTE_ADDR_SET;

		memcpy(&new_context->remote, &remote_addr,
		       sizeof(remote_addr));

		ret = net_tcp_register(&new_context->remote,
			       &local_addr,
			       ntohs(net_sin(&new_context->remote)->sin_port),
			       ntohs(net_sin(&local_addr)->sin_port),
			       tcp_established,
			       new_context,
			       &new_context->conn_handler);
		if (ret < 0) {
			NET_DBG("Cannot register accepted TCP handler (%d)",
				ret);
			net_context_unref(new_context);
			goto conndrop;
		}

		/* Swap the newly-created TCP states with the one that
		 * was used to establish this connection. The old TCP
		 * must be listening to accept other connections.
		 */
		new_tcp = new_context->tcp;
		new_tcp->accept_cb = NULL;
		copy_pool_vars(new_context, context);

		net_tcp_change_state(tcp, NET_TCP_LISTEN);

		/* We cannot use net_tcp_change_state() here as that will
		 * check the state transitions. So set the state directly.
		 */
		new_context->tcp->state = NET_TCP_ESTABLISHED;

		net_context_set_state(new_context, NET_CONTEXT_CONNECTED);

		if (new_context->remote.sa_family == AF_INET) {
			addrlen = sizeof(struct sockaddr_in);
		} else if (new_context->remote.sa_family == AF_INET6) {
			addrlen = sizeof(struct sockaddr_in6);
		} else {
			NET_ASSERT_INFO(false, "Invalid protocol family %d",
					new_context->remote.sa_family);
			net_context_unref(new_context);
			return NET_DROP;
		}

		context->tcp->accept_cb(new_context,
					&new_context->remote,
					addrlen,
					0,
					context->user_data);
	}

	return NET_DROP;

conndrop:
	net_stats_update_tcp_seg_conndrop();

reset:
	send_reset(tcp->context, &local_addr, &remote_addr);

	return NET_DROP;
}

#endif /* CONFIG_NET_TCP */

int net_context_accept(struct net_context *context,
		       net_tcp_accept_cb_t cb,
		       s32_t timeout,
		       void *user_data)
{
#if defined(CONFIG_NET_TCP)
	struct sockaddr local_addr;
	struct sockaddr *laddr = NULL;
	u16_t lport = 0;
	int ret;
#endif /* CONFIG_NET_TCP */

	NET_ASSERT(PART_OF_ARRAY(contexts, context));

	if (!net_context_is_used(context)) {
		return -EBADF;
	}

#if defined(CONFIG_NET_OFFLOAD)
	if (net_if_is_ip_offloaded(net_context_get_iface(context))) {
		return net_offload_accept(
			net_context_get_iface(context),
			context,
			cb,
			timeout,
			user_data);
	}
#endif /* CONFIG_NET_OFFLOAD */

	if ((net_context_get_state(context) != NET_CONTEXT_LISTENING) &&
	    (net_context_get_type(context) != SOCK_STREAM)) {
		NET_DBG("Invalid socket, state %d type %d",
			net_context_get_state(context),
			net_context_get_type(context));
		return -EINVAL;
	}

#if defined(CONFIG_NET_TCP)
	if (net_context_get_ip_proto(context) == IPPROTO_TCP) {
		NET_ASSERT(context->tcp);

		if (net_tcp_get_state(context->tcp) != NET_TCP_LISTEN) {
			NET_DBG("Context %p in wrong state %d, should be %d",
				context, context->tcp->state, NET_TCP_LISTEN);
			return -EINVAL;
		}
	}

	local_addr.sa_family = net_context_get_family(context);

#if defined(CONFIG_NET_IPV6)
	if (net_context_get_family(context) == AF_INET6) {
		if (net_sin6_ptr(&context->local)->sin6_addr) {
			net_ipaddr_copy(&net_sin6(&local_addr)->sin6_addr,
				     net_sin6_ptr(&context->local)->sin6_addr);

			laddr = &local_addr;
		}

		net_sin6(&local_addr)->sin6_port = lport =
			net_sin6((struct sockaddr *)&context->local)->sin6_port;
	}
#endif /* CONFIG_NET_IPV6 */

#if defined(CONFIG_NET_IPV4)
	if (net_context_get_family(context) == AF_INET) {
		if (net_sin_ptr(&context->local)->sin_addr) {
			net_ipaddr_copy(&net_sin(&local_addr)->sin_addr,
				      net_sin_ptr(&context->local)->sin_addr);

			laddr = &local_addr;
		}

		net_sin(&local_addr)->sin_port = lport =
			net_sin((struct sockaddr *)&context->local)->sin_port;
	}
#endif /* CONFIG_NET_IPV4 */

	ret = net_tcp_register(context->flags & NET_CONTEXT_REMOTE_ADDR_SET ?
			       &context->remote : NULL,
			       laddr,
			       ntohs(net_sin(&context->remote)->sin_port),
			       ntohs(lport),
			       tcp_syn_rcvd,
			       context,
			       &context->conn_handler);
	if (ret < 0) {
		return ret;
	}

	context->user_data = user_data;

	/* accept callback is only valid for TCP contexts */
	if (net_context_get_ip_proto(context) == IPPROTO_TCP) {
		context->tcp->accept_cb = cb;
	}
#endif /* CONFIG_NET_TCP */

	return 0;
}

static int send_data(struct net_context *context,
		     struct net_pkt *pkt,
		     net_context_send_cb_t cb,
		     s32_t timeout,
		     void *token,
		     void *user_data)
{
	context->send_cb = cb;
	context->user_data = user_data;
	net_pkt_set_token(pkt, token);

	if (net_context_get_ip_proto(context) == IPPROTO_UDP) {
		return net_send_data(pkt);
	}

#if defined(CONFIG_NET_TCP)
	if (net_context_get_ip_proto(context) == IPPROTO_TCP) {
		int ret = net_tcp_send_data(context);

		/* Just make the callback synchronously even if it didn't
		 * go over the wire.  In theory it would be nice to track
		 * specific ACK locations in the stream and make the
		 * callback at that time, but there's nowhere to store the
		 * potentially-separate token/user_data values right now.
		 */
		if (cb) {
			cb(context, ret, token, user_data);
		}

		return ret;
	}
#endif

	return -EPROTONOSUPPORT;
}

#if defined(CONFIG_NET_UDP)
static int create_udp_packet(struct net_context *context,
			     struct net_pkt *pkt,
			     const struct sockaddr *dst_addr,
			     struct net_pkt **out_pkt)
{
	int r = 0;
	struct net_pkt *tmp;

#if defined(CONFIG_NET_IPV6)
	if (net_pkt_family(pkt) == AF_INET6) {
		struct sockaddr_in6 *addr6 = (struct sockaddr_in6 *)dst_addr;

		pkt = net_ipv6_create(context, pkt, NULL, &addr6->sin6_addr);
		tmp = net_udp_insert(context, pkt,
				     net_pkt_ip_hdr_len(pkt) +
				     net_pkt_ipv6_ext_len(pkt),
				     addr6->sin6_port);
		if (!tmp) {
			return -ENOMEM;
		}

		pkt = tmp;

		r = net_ipv6_finalize(context, pkt);
	} else
#endif /* CONFIG_NET_IPV6 */

#if defined(CONFIG_NET_IPV4)
	if (net_pkt_family(pkt) == AF_INET) {
		struct sockaddr_in *addr4 = (struct sockaddr_in *)dst_addr;

		pkt = net_ipv4_create(context, pkt, NULL, &addr4->sin_addr);
		tmp = net_udp_insert(context, pkt, net_pkt_ip_hdr_len(pkt),
				     addr4->sin_port);
		if (!tmp) {
			return -ENOMEM;
		}

		pkt = tmp;

		r = net_ipv4_finalize(context, pkt);
	} else
#endif /* CONFIG_NET_IPV4 */
	{
		return -EPROTONOSUPPORT;
	}

	*out_pkt = pkt;

	return r;
}
#endif /* CONFIG_NET_UDP */

static int sendto(struct net_pkt *pkt,
		  const struct sockaddr *dst_addr,
		  socklen_t addrlen,
		  net_context_send_cb_t cb,
		  s32_t timeout,
		  void *token,
		  void *user_data)
{
	struct net_context *context = net_pkt_context(pkt);
	int ret;

	if (!net_context_is_used(context)) {
		return -EBADF;
	}

#if defined(CONFIG_NET_TCP)
	if (net_context_get_ip_proto(context) == IPPROTO_TCP) {
		if (net_context_get_state(context) != NET_CONTEXT_CONNECTED) {
			return -ENOTCONN;
		}

		NET_ASSERT(context->tcp);
		if (context->tcp->flags & NET_TCP_IS_SHUTDOWN) {
			return -ESHUTDOWN;
		}
	}
#endif /* CONFIG_NET_TCP */

#if defined(CONFIG_NET_UDP)
	/* Bind default address and port only if UDP */
	if (net_context_get_ip_proto(context) == IPPROTO_UDP) {
		ret = bind_default(context);
		if (ret) {
			return ret;
		}
	}
#endif /* CONFIG_NET_UDP */

	if (!dst_addr) {
		return -EDESTADDRREQ;
	}

#if defined(CONFIG_NET_OFFLOAD)
	if (net_if_is_ip_offloaded(net_pkt_iface(pkt))) {
		return net_offload_sendto(
			net_pkt_iface(pkt),
			pkt, dst_addr, addrlen,
			cb, timeout, token, user_data);
	}
#endif /* CONFIG_NET_OFFLOAD */

#if defined(CONFIG_NET_IPV6)
	if (net_pkt_family(pkt) == AF_INET6) {
		struct sockaddr_in6 *addr6 = (struct sockaddr_in6 *)dst_addr;

		if (addrlen < sizeof(struct sockaddr_in6)) {
			return -EINVAL;
		}

		if (net_is_ipv6_addr_unspecified(&addr6->sin6_addr)) {
			return -EDESTADDRREQ;
		}
	} else
#endif /* CONFIG_NET_IPV6 */

#if defined(CONFIG_NET_IPV4)
	if (net_pkt_family(pkt) == AF_INET) {
		struct sockaddr_in *addr4 = (struct sockaddr_in *)dst_addr;

		if (addrlen < sizeof(struct sockaddr_in)) {
			return -EINVAL;
		}

		if (!addr4->sin_addr.s_addr) {
			return -EDESTADDRREQ;
		}
	} else
#endif /* CONFIG_NET_IPV4 */
	{
		NET_DBG("Invalid protocol family %d", net_pkt_family(pkt));
		return -EINVAL;
	}

#if defined(CONFIG_NET_UDP)
	if (net_context_get_ip_proto(context) == IPPROTO_UDP) {
		ret = create_udp_packet(context, pkt, dst_addr, &pkt);
	} else
#endif /* CONFIG_NET_UDP */

#if defined(CONFIG_NET_TCP)
	if (net_context_get_ip_proto(context) == IPPROTO_TCP) {
		net_pkt_set_appdatalen(pkt, net_pkt_get_len(pkt));
		ret = net_tcp_queue_data(context, pkt);
	} else
#endif /* CONFIG_NET_TCP */
	{
		NET_DBG("Unknown protocol while sending packet: %d",
			net_context_get_ip_proto(context));
		return -EPROTONOSUPPORT;
	}

	if (ret < 0) {
		NET_DBG("Could not create network packet to send (%d)", ret);
		return ret;
	}

	return send_data(context, pkt, cb, timeout, token, user_data);
}

int net_context_send(struct net_pkt *pkt,
		     net_context_send_cb_t cb,
		     s32_t timeout,
		     void *token,
		     void *user_data)
{
	struct net_context *context = net_pkt_context(pkt);
	socklen_t addrlen;

	NET_ASSERT(PART_OF_ARRAY(contexts, context));

#if defined(CONFIG_NET_OFFLOAD)
	if (net_if_is_ip_offloaded(net_pkt_iface(pkt))) {
		return net_offload_send(
			net_pkt_iface(pkt),
			pkt, cb, timeout,
			token, user_data);
	}
#endif /* CONFIG_NET_OFFLOAD */

	if (!(context->flags & NET_CONTEXT_REMOTE_ADDR_SET) ||
	    !net_sin(&context->remote)->sin_port) {
		return -EDESTADDRREQ;
	}

#if defined(CONFIG_NET_IPV6)
	if (net_pkt_family(pkt) == AF_INET6) {
		addrlen = sizeof(struct sockaddr_in6);
	} else
#endif /* CONFIG_NET_IPV6 */

#if defined(CONFIG_NET_IPV4)
	if (net_pkt_family(pkt) == AF_INET) {
		addrlen = sizeof(struct sockaddr_in);
	} else
#endif /* CONFIG_NET_IPV4 */
	{
		addrlen = 0;
	}

	return sendto(pkt, &context->remote, addrlen, cb, timeout, token,
		      user_data);
}

int net_context_sendto(struct net_pkt *pkt,
		       const struct sockaddr *dst_addr,
		       socklen_t addrlen,
		       net_context_send_cb_t cb,
		       s32_t timeout,
		       void *token,
		       void *user_data)
{
#if defined(CONFIG_NET_TCP)
	struct net_context *context = net_pkt_context(pkt);

	NET_ASSERT(PART_OF_ARRAY(contexts, context));

	if (net_context_get_ip_proto(context) == IPPROTO_TCP) {
		/* Match POSIX behavior and ignore dst_address and addrlen */
		return net_context_send(pkt, cb, timeout, token, user_data);
	}
#endif /* CONFIG_NET_TCP */

	return sendto(pkt, dst_addr, addrlen, cb, timeout, token, user_data);
}

static void set_appdata_values(struct net_pkt *pkt, enum net_ip_protocol proto)
{
	size_t total_len = net_pkt_get_len(pkt);
	u16_t proto_len = 0;
	struct net_buf *frag;
	u16_t offset;

#if defined(CONFIG_NET_UDP)
	if (proto == IPPROTO_UDP) {
		proto_len = sizeof(struct net_udp_hdr);
	}
#endif /* CONFIG_NET_UDP */

#if defined(CONFIG_NET_TCP)
	if (proto == IPPROTO_TCP) {
		proto_len = tcp_hdr_len(pkt);
	}
#endif /* CONFIG_NET_TCP */

	frag = net_frag_get_pos(pkt, net_pkt_ip_hdr_len(pkt) +
				net_pkt_ipv6_ext_len(pkt) +
				proto_len,
				&offset);
	if (frag) {
		net_pkt_set_appdata(pkt, frag->data + offset);
	}

	net_pkt_set_appdatalen(pkt, total_len - net_pkt_ip_hdr_len(pkt) -
			       net_pkt_ipv6_ext_len(pkt) - proto_len);

	NET_ASSERT_INFO(net_pkt_appdatalen(pkt) < total_len,
			"Wrong appdatalen %u, total %zu",
			net_pkt_appdatalen(pkt), total_len);
}

static enum net_verdict packet_received(struct net_conn *conn,
					struct net_pkt *pkt,
					void *user_data)
{
	struct net_context *context = find_context(conn);

	NET_ASSERT(context);
	NET_ASSERT(net_pkt_iface(pkt));

	net_context_set_iface(context, net_pkt_iface(pkt));
	net_pkt_set_context(pkt, context);

	/* If there is no callback registered, then we can only drop
	 * the packet.
	 */

	if (!context->recv_cb) {
		return NET_DROP;
	}

	if (net_context_get_ip_proto(context) != IPPROTO_TCP) {
		/* TCP packets get appdata earlier in tcp_established(). */
		set_appdata_values(pkt, IPPROTO_UDP);
	}

	NET_DBG("Set appdata %p to len %u (total %zu)",
		net_pkt_appdata(pkt), net_pkt_appdatalen(pkt),
		net_pkt_get_len(pkt));

	net_stats_update_tcp_recv(net_pkt_appdatalen(pkt));

	context->recv_cb(context, pkt, 0, user_data);

#if defined(CONFIG_NET_CONTEXT_SYNC_RECV)
	k_sem_give(&context->recv_data_wait);
#endif /* CONFIG_NET_CONTEXT_SYNC_RECV */

	return NET_OK;
}

#if defined(CONFIG_NET_UDP)
static int recv_udp(struct net_context *context,
		    net_context_recv_cb_t cb,
		    s32_t timeout,
		    void *user_data)
{
	struct sockaddr local_addr = {
		.sa_family = net_context_get_family(context),
	};
	struct sockaddr *laddr = NULL;
	u16_t lport = 0;
	int ret;

	ARG_UNUSED(timeout);

	if (context->conn_handler) {
		net_conn_unregister(context->conn_handler);
		context->conn_handler = NULL;
	}

	ret = bind_default(context);
	if (ret) {
		return ret;
	}

#if defined(CONFIG_NET_IPV6)
	if (net_context_get_family(context) == AF_INET6) {
		if (net_sin6_ptr(&context->local)->sin6_addr) {
			net_ipaddr_copy(&net_sin6(&local_addr)->sin6_addr,
				     net_sin6_ptr(&context->local)->sin6_addr);

			laddr = &local_addr;
		}

		net_sin6(&local_addr)->sin6_port =
			net_sin6((struct sockaddr *)&context->local)->sin6_port;
		lport = net_sin6((struct sockaddr *)&context->local)->sin6_port;
	}
#endif /* CONFIG_NET_IPV6 */

#if defined(CONFIG_NET_IPV4)
	if (net_context_get_family(context) == AF_INET) {
		if (net_sin_ptr(&context->local)->sin_addr) {
			net_ipaddr_copy(&net_sin(&local_addr)->sin_addr,
				      net_sin_ptr(&context->local)->sin_addr);

			laddr = &local_addr;
		}

		lport = net_sin((struct sockaddr *)&context->local)->sin_port;
	}
#endif /* CONFIG_NET_IPV4 */

	context->recv_cb = cb;

	ret = net_conn_register(net_context_get_ip_proto(context),
				context->flags & NET_CONTEXT_REMOTE_ADDR_SET ?
							&context->remote : NULL,
				laddr,
				ntohs(net_sin(&context->remote)->sin_port),
				ntohs(lport),
				packet_received,
				user_data,
				&context->conn_handler);

	return ret;
}
#endif /* CONFIG_NET_UDP */

int net_context_recv(struct net_context *context,
		     net_context_recv_cb_t cb,
		     s32_t timeout,
		     void *user_data)
{
	NET_ASSERT(context);

	if (!net_context_is_used(context)) {
		return -EBADF;
	}

#if defined(CONFIG_NET_OFFLOAD)
	if (net_if_is_ip_offloaded(net_context_get_iface(context))) {
		return net_offload_recv(
			net_context_get_iface(context),
			context, cb, timeout, user_data);
	}
#endif /* CONFIG_NET_OFFLOAD */

#if defined(CONFIG_NET_UDP)
	if (net_context_get_ip_proto(context) == IPPROTO_UDP) {
		int ret = recv_udp(context, cb, timeout, user_data);
		if (ret < 0) {
			return ret;
		}
	} else
#endif /* CONFIG_NET_UDP */

#if defined(CONFIG_NET_TCP)
	if (net_context_get_ip_proto(context) == IPPROTO_TCP) {
		NET_ASSERT(context->tcp);

		if (context->tcp->flags & NET_TCP_IS_SHUTDOWN) {
			return -ESHUTDOWN;
		} else if (net_context_get_state(context)
			   != NET_CONTEXT_CONNECTED) {
			return -ENOTCONN;
		}

		context->recv_cb = cb;
		context->tcp->recv_user_data = user_data;
	} else
#endif /* CONFIG_NET_TCP */
	{
		return -EPROTOTYPE;
	}

#if defined(CONFIG_NET_CONTEXT_SYNC_RECV)
	if (timeout) {
		int ret;

		/* Make sure we have the lock, then the packet_received()
		 * callback will release the semaphore when data has been
		 * received.
		 */
		k_sem_reset(&context->recv_data_wait);

		ret = k_sem_take(&context->recv_data_wait, timeout);
		if (ret == -EAGAIN) {
			return -ETIMEDOUT;
		}
	}
#endif /* CONFIG_NET_CONTEXT_SYNC_RECV */

	return 0;
}

int net_context_update_recv_wnd(struct net_context *context,
				s32_t delta)
{
#if defined(CONFIG_NET_TCP)
	s32_t new_win;

	if (!context->tcp) {
		NET_ERR("context->tcp == NULL");
		return -EPROTOTYPE;
	}

	new_win = context->tcp->recv_wnd + delta;
	if (new_win < 0 || new_win > UINT16_MAX) {
		return -EINVAL;
	}

	context->tcp->recv_wnd = new_win;

	return 0;
#else
	return -EPROTOTYPE;
#endif
}
void net_context_foreach(net_context_cb_t cb, void *user_data)
{
	int i;

	k_sem_take(&contexts_lock, K_FOREVER);

	for (i = 0; i < NET_MAX_CONTEXT; i++) {
		if (!net_context_is_used(&contexts[i])) {
			continue;
		}

		cb(&contexts[i], user_data);
	}

	k_sem_give(&contexts_lock);
}

void net_context_init(void)
{
	k_sem_init(&contexts_lock, 1, UINT_MAX);
}
