/** @file
 * @brief Network shell module
 *
 * Provide some networking shell commands that can be useful to applications.
 */

/*
 * Copyright (c) 2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <stdlib.h>
#include <shell/shell.h>

#include <net/net_if.h>
#include <net/dns_resolve.h>
#include <misc/printk.h>

#include "route.h"
#include "icmpv6.h"
#include "icmpv4.h"
#include "connection.h"

#if defined(CONFIG_NET_TCP)
#include "tcp.h"
#endif

#if defined(CONFIG_NET_IPV6)
#include "ipv6.h"
#endif

#if defined(CONFIG_HTTP)
#include <net/http.h>
#endif

#if defined(CONFIG_NET_APP)
#include <net/net_app.h>
#endif

#if defined(CONFIG_NET_RPL)
#include "rpl.h"
#endif

#if defined(CONFIG_NET_ARP)
#include <net/arp.h>
#endif

#include "net_shell.h"
#include "net_stats.h"

/*
 * Set NET_LOG_ENABLED in order to activate address printing functions
 * in net_private.h
 */
#define NET_LOG_ENABLED 1
#include "net_private.h"

#define NET_SHELL_MODULE "net"

/* net_stack dedicated section limiters */
extern struct net_stack_info __net_stack_start[];
extern struct net_stack_info __net_stack_end[];

static inline const char *addrtype2str(enum net_addr_type addr_type)
{
	switch (addr_type) {
	case NET_ADDR_ANY:
		return "<unknown type>";
	case NET_ADDR_AUTOCONF:
		return "autoconf";
	case NET_ADDR_DHCP:
		return "DHCP";
	case NET_ADDR_MANUAL:
		return "manual";
	case NET_ADDR_OVERRIDABLE:
		return "overridable";
	}

	return "<invalid type>";
}

static inline const char *addrstate2str(enum net_addr_state addr_state)
{
	switch (addr_state) {
	case NET_ADDR_ANY_STATE:
		return "<unknown state>";
	case NET_ADDR_TENTATIVE:
		return "tentative";
	case NET_ADDR_PREFERRED:
		return "preferred";
	case NET_ADDR_DEPRECATED:
		return "deprecated";
	}

	return "<invalid state>";
}

static const char *iface2str(struct net_if *iface, const char **extra)
{
#ifdef CONFIG_NET_L2_IEEE802154
	if (iface->l2 == &NET_L2_GET_NAME(IEEE802154)) {
		if (extra) {
			*extra = "=============";
		}

		return "IEEE 802.15.4";
	}
#endif

#ifdef CONFIG_NET_L2_ETHERNET
	if (iface->l2 == &NET_L2_GET_NAME(ETHERNET)) {
		if (extra) {
			*extra = "========";
		}

		return "Ethernet";
	}
#endif

#ifdef CONFIG_NET_L2_DUMMY
	if (iface->l2 == &NET_L2_GET_NAME(DUMMY)) {
		if (extra) {
			*extra = "=====";
		}

		return "Dummy";
	}
#endif

#ifdef CONFIG_NET_L2_BT
	if (iface->l2 == &NET_L2_GET_NAME(BLUETOOTH)) {
		if (extra) {
			*extra = "=========";
		}

		return "Bluetooth";
	}
#endif

#ifdef CONFIG_NET_OFFLOAD
	if (iface->l2 == &NET_L2_GET_NAME(OFFLOAD_IP)) {
		if (extra) {
			*extra = "==========";
		}

		return "IP Offload";
	}
#endif

	if (extra) {
		*extra = "==============";
	}

	return "<unknown type>";
}

static void iface_cb(struct net_if *iface, void *user_data)
{
#if defined(CONFIG_NET_IPV6)
	struct net_if_ipv6_prefix *prefix;
	struct net_if_router *router;
#endif
	struct net_if_addr *unicast;
	struct net_if_mcast_addr *mcast;
	const char *extra;
	int i, count;

	ARG_UNUSED(user_data);

	printk("\nInterface %p (%s)\n", iface, iface2str(iface, &extra));
	printk("=======================%s\n", extra);

	if (!net_if_is_up(iface)) {
		printk("Interface is down.\n");
		return;
	}

	printk("Link addr : %s\n", net_sprint_ll_addr(iface->link_addr.addr,
						      iface->link_addr.len));
	printk("MTU       : %d\n", iface->mtu);

#if defined(CONFIG_NET_IPV6)
	count = 0;

	printk("IPv6 unicast addresses (max %d):\n", NET_IF_MAX_IPV6_ADDR);
	for (i = 0; i < NET_IF_MAX_IPV6_ADDR; i++) {
		unicast = &iface->ipv6.unicast[i];

		if (!unicast->is_used) {
			continue;
		}

		printk("\t%s %s %s%s\n",
		       net_sprint_ipv6_addr(&unicast->address.in6_addr),
		       addrtype2str(unicast->addr_type),
		       addrstate2str(unicast->addr_state),
		       unicast->is_infinite ? " infinite" : "");
		count++;
	}

	if (count == 0) {
		printk("\t<none>\n");
	}

	count = 0;

	printk("IPv6 multicast addresses (max %d):\n", NET_IF_MAX_IPV6_MADDR);
	for (i = 0; i < NET_IF_MAX_IPV6_MADDR; i++) {
		mcast = &iface->ipv6.mcast[i];

		if (!mcast->is_used) {
			continue;
		}

		printk("\t%s\n",
		       net_sprint_ipv6_addr(&mcast->address.in6_addr));

		count++;
	}

	if (count == 0) {
		printk("\t<none>\n");
	}

	count = 0;

	printk("IPv6 prefixes (max %d):\n", NET_IF_MAX_IPV6_PREFIX);
	for (i = 0; i < NET_IF_MAX_IPV6_PREFIX; i++) {
		prefix = &iface->ipv6.prefix[i];

		if (!prefix->is_used) {
			continue;
		}

		printk("\t%s/%d%s\n",
		       net_sprint_ipv6_addr(&prefix->prefix),
		       prefix->len,
		       prefix->is_infinite ? " infinite" : "");

		count++;
	}

	if (count == 0) {
		printk("\t<none>\n");
	}

	router = net_if_ipv6_router_find_default(iface, NULL);
	if (router) {
		printk("IPv6 default router :\n");
		printk("\t%s%s\n",
		       net_sprint_ipv6_addr(&router->address.in6_addr),
		       router->is_infinite ? " infinite" : "");
	}

	printk("IPv6 hop limit           : %d\n", iface->ipv6.hop_limit);
	printk("IPv6 base reachable time : %d\n",
	       iface->ipv6.base_reachable_time);
	printk("IPv6 reachable time      : %d\n", iface->ipv6.reachable_time);
	printk("IPv6 retransmit timer    : %d\n", iface->ipv6.retrans_timer);
#endif /* CONFIG_NET_IPV6 */

#if defined(CONFIG_NET_IPV4)
	/* No need to print IPv4 information for interface that does not
	 * support that protocol.
	 */
	if (
#if defined(CONFIG_NET_L2_IEEE802154)
		(iface->l2 == &NET_L2_GET_NAME(IEEE802154)) ||
#endif
#if defined(CONFIG_NET_L2_BT)
		 (iface->l2 == &NET_L2_GET_NAME(BLUETOOTH)) ||
#endif
		 0) {
		printk("IPv4 not supported for this interface.\n");
		return;
	}

	count = 0;

	printk("IPv4 unicast addresses (max %d):\n", NET_IF_MAX_IPV4_ADDR);
	for (i = 0; i < NET_IF_MAX_IPV4_ADDR; i++) {
		unicast = &iface->ipv4.unicast[i];

		if (!unicast->is_used) {
			continue;
		}

		printk("\t%s %s %s%s\n",
		       net_sprint_ipv4_addr(&unicast->address.in_addr),
		       addrtype2str(unicast->addr_type),
		       addrstate2str(unicast->addr_state),
		       unicast->is_infinite ? " infinite" : "");

		count++;
	}

	if (count == 0) {
		printk("\t<none>\n");
	}

	count = 0;

	printk("IPv4 multicast addresses (max %d):\n", NET_IF_MAX_IPV4_MADDR);
	for (i = 0; i < NET_IF_MAX_IPV4_MADDR; i++) {
		mcast = &iface->ipv4.mcast[i];

		if (!mcast->is_used) {
			continue;
		}

		printk("\t%s\n",
		       net_sprint_ipv4_addr(&mcast->address.in_addr));

		count++;
	}

	if (count == 0) {
		printk("\t<none>\n");
	}

	printk("IPv4 gateway : %s\n",
	       net_sprint_ipv4_addr(&iface->ipv4.gw));
	printk("IPv4 netmask : %s\n",
	       net_sprint_ipv4_addr(&iface->ipv4.netmask));
#endif /* CONFIG_NET_IPV4 */

#if defined(CONFIG_NET_DHCPV4)
	printk("DHCPv4 lease time : %u\n", iface->dhcpv4.lease_time);
	printk("DHCPv4 renew time : %u\n", iface->dhcpv4.renewal_time);
	printk("DHCPv4 server     : %s\n",
	       net_sprint_ipv4_addr(&iface->dhcpv4.server_id));
	printk("DHCPv4 requested  : %s\n",
	       net_sprint_ipv4_addr(&iface->dhcpv4.requested_ip));
	printk("DHCPv4 state      : %s\n",
	       net_dhcpv4_state_name(iface->dhcpv4.state));
	printk("DHCPv4 attempts   : %d\n", iface->dhcpv4.attempts);
#endif /* CONFIG_NET_DHCPV4 */
}

#if defined(CONFIG_NET_ROUTE)
static void route_cb(struct net_route_entry *entry, void *user_data)
{
	struct net_if *iface = user_data;
	struct net_route_nexthop *nexthop_route;
	int count;

	if (entry->iface != iface) {
		return;
	}

	printk("IPv6 prefix : %s/%d\n",
	       net_sprint_ipv6_addr(&entry->addr),
	       entry->prefix_len);

	count = 0;

	SYS_SLIST_FOR_EACH_CONTAINER(&entry->nexthop, nexthop_route, node) {
		struct net_linkaddr_storage *lladdr;

		if (!nexthop_route->nbr) {
			continue;
		}

		printk("\tneighbor : %p\t", nexthop_route->nbr);

		if (nexthop_route->nbr->idx == NET_NBR_LLADDR_UNKNOWN) {
			printk("addr : <unknown>\n");
		} else {
			lladdr = net_nbr_get_lladdr(nexthop_route->nbr->idx);

			printk("addr : %s\n",
			       net_sprint_ll_addr(lladdr->addr,
						  lladdr->len));
		}

		count++;
	}

	if (count == 0) {
		printk("\t<none>\n");
	}
}

static void iface_per_route_cb(struct net_if *iface, void *user_data)
{
	const char *extra;

	ARG_UNUSED(user_data);

	printk("\nIPv6 routes for interface %p (%s)\n", iface,
	       iface2str(iface, &extra));
	printk("=======================================%s\n", extra);

	net_route_foreach(route_cb, iface);
}
#endif /* CONFIG_NET_ROUTE */

#if defined(CONFIG_NET_ROUTE_MCAST)
static void route_mcast_cb(struct net_route_entry_mcast *entry,
			   void *user_data)
{
	struct net_if *iface = user_data;
	const char *extra;

	if (entry->iface != iface) {
		return;
	}

	printk("IPv6 multicast route %p for interface %p (%s)\n", entry,
	       iface, iface2str(iface, &extra));
	printk("==========================================================="
	       "%s\n", extra);

	printk("IPv6 group : %s\n", net_sprint_ipv6_addr(&entry->group));
	printk("Lifetime   : %u\n", entry->lifetime);
}

static void iface_per_mcast_route_cb(struct net_if *iface, void *user_data)
{
	net_route_mcast_foreach(route_mcast_cb, NULL, iface);
}
#endif /* CONFIG_NET_ROUTE_MCAST */

#if defined(CONFIG_NET_STATISTICS)

static inline void net_shell_print_statistics(void)
{
#if defined(CONFIG_NET_IPV6)
	printk("IPv6 recv      %d\tsent\t%d\tdrop\t%d\tforwarded\t%d\n",
	       GET_STAT(ipv6.recv),
	       GET_STAT(ipv6.sent),
	       GET_STAT(ipv6.drop),
	       GET_STAT(ipv6.forwarded));
#if defined(CONFIG_NET_IPV6_ND)
	printk("IPv6 ND recv   %d\tsent\t%d\tdrop\t%d\n",
	       GET_STAT(ipv6_nd.recv),
	       GET_STAT(ipv6_nd.sent),
	       GET_STAT(ipv6_nd.drop));
#endif /* CONFIG_NET_IPV6_ND */
#if defined(CONFIG_NET_STATISTICS_MLD)
	printk("IPv6 MLD recv  %d\tsent\t%d\tdrop\t%d\n",
	       GET_STAT(ipv6_mld.recv),
	       GET_STAT(ipv6_mld.sent),
	       GET_STAT(ipv6_mld.drop));
#endif /* CONFIG_NET_STATISTICS_MLD */
#endif /* CONFIG_NET_IPV6 */

#if defined(CONFIG_NET_IPV4)
	printk("IPv4 recv      %d\tsent\t%d\tdrop\t%d\tforwarded\t%d\n",
	       GET_STAT(ipv4.recv),
	       GET_STAT(ipv4.sent),
	       GET_STAT(ipv4.drop),
	       GET_STAT(ipv4.forwarded));
#endif /* CONFIG_NET_IPV4 */

	printk("IP vhlerr      %d\thblener\t%d\tlblener\t%d\n",
	       GET_STAT(ip_errors.vhlerr),
	       GET_STAT(ip_errors.hblenerr),
	       GET_STAT(ip_errors.lblenerr));
	printk("IP fragerr     %d\tchkerr\t%d\tprotoer\t%d\n",
	       GET_STAT(ip_errors.fragerr),
	       GET_STAT(ip_errors.chkerr),
	       GET_STAT(ip_errors.protoerr));

	printk("ICMP recv      %d\tsent\t%d\tdrop\t%d\n",
	       GET_STAT(icmp.recv),
	       GET_STAT(icmp.sent),
	       GET_STAT(icmp.drop));
	printk("ICMP typeer    %d\tchkerr\t%d\n",
	       GET_STAT(icmp.typeerr),
	       GET_STAT(icmp.chkerr));

#if defined(CONFIG_NET_UDP)
	printk("UDP recv       %d\tsent\t%d\tdrop\t%d\n",
	       GET_STAT(udp.recv),
	       GET_STAT(udp.sent),
	       GET_STAT(udp.drop));
	printk("UDP chkerr     %d\n",
	       GET_STAT(udp.chkerr));
#endif

#if defined(CONFIG_NET_STATISTICS_TCP)
	printk("TCP bytes recv %u\tsent\t%d\n",
	       GET_STAT(tcp.bytes.received),
	       GET_STAT(tcp.bytes.sent));
	printk("TCP seg recv   %d\tsent\t%d\tdrop\t%d\n",
	       GET_STAT(tcp.recv),
	       GET_STAT(tcp.sent),
	       GET_STAT(tcp.drop));
	printk("TCP seg resent %d\tchkerr\t%d\tackerr\t%d\n",
	       GET_STAT(tcp.resent),
	       GET_STAT(tcp.chkerr),
	       GET_STAT(tcp.ackerr));
	printk("TCP seg rsterr %d\trst\t%d\tre-xmit\t%d\n",
	       GET_STAT(tcp.rsterr),
	       GET_STAT(tcp.rst),
	       GET_STAT(tcp.rexmit));
	printk("TCP conn drop  %d\tconnrst\t%d\n",
	       GET_STAT(tcp.conndrop),
	       GET_STAT(tcp.connrst));
#endif

#if defined(CONFIG_NET_STATISTICS_RPL)
	printk("RPL DIS recv   %d\tsent\t%d\tdrop\t%d\n",
	       GET_STAT(rpl.dis.recv),
	       GET_STAT(rpl.dis.sent),
	       GET_STAT(rpl.dis.drop));
	printk("RPL DIO recv   %d\tsent\t%d\tdrop\t%d\n",
	       GET_STAT(rpl.dio.recv),
	       GET_STAT(rpl.dio.sent),
	       GET_STAT(rpl.dio.drop));
	printk("RPL DAO recv   %d\tsent\t%d\tdrop\t%d\tforwarded\t%d\n",
	       GET_STAT(rpl.dao.recv),
	       GET_STAT(rpl.dao.sent),
	       GET_STAT(rpl.dao.drop),
	      GET_STAT(rpl.dao.forwarded));
	printk("RPL DAOACK rcv %d\tsent\t%d\tdrop\t%d\n",
	       GET_STAT(rpl.dao_ack.recv),
	       GET_STAT(rpl.dao_ack.sent),
	       GET_STAT(rpl.dao_ack.drop));
	printk("RPL overflows  %d\tl-repairs\t%d\tg-repairs\t%d\n",
	       GET_STAT(rpl.mem_overflows),
	       GET_STAT(rpl.local_repairs),
	       GET_STAT(rpl.global_repairs));
	printk("RPL malformed  %d\tresets   \t%d\tp-switch\t%d\n",
	       GET_STAT(rpl.malformed_msgs),
	       GET_STAT(rpl.resets),
	       GET_STAT(rpl.parent_switch));
	printk("RPL f-errors   %d\tl-errors\t%d\tl-warnings\t%d\n",
	       GET_STAT(rpl.forward_errors),
	       GET_STAT(rpl.loop_errors),
	       GET_STAT(rpl.loop_warnings));
	printk("RPL r-repairs  %d\n",
	       GET_STAT(rpl.root_repairs));
#endif

	printk("Bytes received %u\n", GET_STAT(bytes.received));
	printk("Bytes sent     %u\n", GET_STAT(bytes.sent));
	printk("Processing err %d\n", GET_STAT(processing_error));
}
#endif /* CONFIG_NET_STATISTICS */

static void get_addresses(struct net_context *context,
			  char addr_local[], int local_len,
			  char addr_remote[], int remote_len)
{
#if defined(CONFIG_NET_IPV6)
	if (context->local.family == AF_INET6) {
		snprintk(addr_local, local_len, "[%s]:%u",
			 net_sprint_ipv6_addr(
				 net_sin6_ptr(&context->local)->sin6_addr),
			 ntohs(net_sin6_ptr(&context->local)->sin6_port));
		snprintk(addr_remote, remote_len, "[%s]:%u",
			 net_sprint_ipv6_addr(
				 &net_sin6(&context->remote)->sin6_addr),
			 ntohs(net_sin6(&context->remote)->sin6_port));
	} else
#endif
#if defined(CONFIG_NET_IPV4)
	if (context->local.family == AF_INET) {
		snprintk(addr_local, local_len, "%s:%d",
			 net_sprint_ipv4_addr(
				 net_sin_ptr(&context->local)->sin_addr),
			 ntohs(net_sin_ptr(&context->local)->sin_port));
		snprintk(addr_remote, remote_len, "%s:%d",
			 net_sprint_ipv4_addr(
				 &net_sin(&context->remote)->sin_addr),
			 ntohs(net_sin(&context->remote)->sin_port));
	} else
#endif
	if (context->local.family == AF_UNSPEC) {
		snprintk(addr_local, local_len, "AF_UNSPEC");
	} else {
		snprintk(addr_local, local_len, "AF_UNK(%d)",
			 context->local.family);
	}
}

static void context_cb(struct net_context *context, void *user_data)
{
#if defined(CONFIG_NET_IPV6) && !defined(CONFIG_NET_IPV4)
#define ADDR_LEN NET_IPV6_ADDR_LEN
#elif defined(CONFIG_NET_IPV4) && !defined(CONFIG_NET_IPV6)
#define ADDR_LEN NET_IPV4_ADDR_LEN
#else
#define ADDR_LEN NET_IPV6_ADDR_LEN
#endif

	int *count = user_data;
	/* +7 for []:port */
	char addr_local[ADDR_LEN + 7];
	char addr_remote[ADDR_LEN + 7] = "";

	get_addresses(context, addr_local, sizeof(addr_local),
		      addr_remote, sizeof(addr_remote));

	printk("[%2d] %p\t%p    %c%c%c   %16s\t%16s\n",
	       (*count) + 1, context,
	       net_context_get_iface(context),
	       net_context_get_family(context) == AF_INET6 ? '6' : '4',
	       net_context_get_type(context) == SOCK_DGRAM ? 'D' : 'S',
	       net_context_get_ip_proto(context) == IPPROTO_UDP ?
							     'U' : 'T',
	       addr_local, addr_remote);

	(*count)++;
}

#if defined(CONFIG_NET_DEBUG_CONN)
static void conn_handler_cb(struct net_conn *conn, void *user_data)
{
#if defined(CONFIG_NET_IPV6) && !defined(CONFIG_NET_IPV4)
#define ADDR_LEN NET_IPV6_ADDR_LEN
#elif defined(CONFIG_NET_IPV4) && !defined(CONFIG_NET_IPV6)
#define ADDR_LEN NET_IPV4_ADDR_LEN
#else
#define ADDR_LEN NET_IPV6_ADDR_LEN
#endif

	int *count = user_data;
	/* +7 for []:port */
	char addr_local[ADDR_LEN + 7];
	char addr_remote[ADDR_LEN + 7] = "";

#if defined(CONFIG_NET_IPV6)
	if (conn->local_addr.sa_family == AF_INET6) {
		snprintk(addr_local, sizeof(addr_local), "[%s]:%u",
			 net_sprint_ipv6_addr(
				 &net_sin6(&conn->local_addr)->sin6_addr),
			 ntohs(net_sin6(&conn->local_addr)->sin6_port));
		snprintk(addr_remote, sizeof(addr_remote), "[%s]:%u",
			 net_sprint_ipv6_addr(
				 &net_sin6(&conn->remote_addr)->sin6_addr),
			 ntohs(net_sin6(&conn->remote_addr)->sin6_port));
	} else
#endif
#if defined(CONFIG_NET_IPV4)
	if (conn->local_addr.sa_family == AF_INET) {
		snprintk(addr_local, sizeof(addr_local), "%s:%d",
			 net_sprint_ipv4_addr(
				 &net_sin(&conn->local_addr)->sin_addr),
			 ntohs(net_sin(&conn->local_addr)->sin_port));
		snprintk(addr_remote, sizeof(addr_remote), "%s:%d",
			 net_sprint_ipv4_addr(
				 &net_sin(&conn->remote_addr)->sin_addr),
			 ntohs(net_sin(&conn->remote_addr)->sin_port));
	} else
#endif
	if (conn->local_addr.sa_family == AF_UNSPEC) {
		snprintk(addr_local, sizeof(addr_local), "AF_UNSPEC");
	} else {
		snprintk(addr_local, sizeof(addr_local), "AF_UNK(%d)",
			 conn->local_addr.sa_family);
	}

	printk("[%2d] %p %p\t%s\t%16s\t%16s\n",
	       (*count) + 1, conn, conn->cb, net_proto2str(conn->proto),
	       addr_local, addr_remote);

	(*count)++;
}
#endif /* CONFIG_NET_DEBUG_CONN */

#if defined(CONFIG_NET_TCP)
static void tcp_cb(struct net_tcp *tcp, void *user_data)
{
	int *count = user_data;
	u16_t recv_mss = net_tcp_get_recv_mss(tcp);

	printk("%p %p   %5u    %5u %10u %10u %5u   %s\n",
	       tcp, tcp->context,
	       ntohs(net_sin6_ptr(&tcp->context->local)->sin6_port),
	       ntohs(net_sin6(&tcp->context->remote)->sin6_port),
	       tcp->send_seq, tcp->send_ack, recv_mss,
	       net_tcp_state_str(net_tcp_get_state(tcp)));

	(*count)++;
}

#if defined(CONFIG_NET_DEBUG_TCP)
static void tcp_sent_list_cb(struct net_tcp *tcp, void *user_data)
{
	int *printed = user_data;
	struct net_pkt *pkt;
	struct net_pkt *tmp;

	if (sys_slist_is_empty(&tcp->sent_list)) {
		return;
	}

	if (!*printed) {
		printk("\nTCP packets waiting ACK:\n");
		printk("TCP             net_pkt[ref/totlen]->net_buf[ref/len]...\n");
	}

	printk("%p      ", tcp);

	SYS_SLIST_FOR_EACH_CONTAINER_SAFE(&tcp->sent_list, pkt, tmp,
					  sent_list) {
		struct net_buf *frag = pkt->frags;

		if (!*printed) {
			printk("%p[%d/%zd]", pkt, pkt->ref,
			       net_pkt_get_len(pkt));
			*printed = true;
		} else {
			printk("                %p[%d/%zd]", pkt, pkt->ref,
			       net_pkt_get_len(pkt));
		}

		if (frag) {
			printk("->");
		}

		while (frag) {
			printk("%p[%d/%d]", frag, frag->ref, frag->len);

			frag = frag->frags;
			if (frag) {
				printk("->");
			}
		}

		printk("\n");
	}

	*printed = true;
}
#endif /* CONFIG_NET_DEBUG_TCP */
#endif

#if defined(CONFIG_NET_IPV6_FRAGMENT)
static void ipv6_frag_cb(struct net_ipv6_reassembly *reass,
			 void *user_data)
{
	int *count = user_data;
	char src[ADDR_LEN];
	int i;

	if (!*count) {
		printk("\nIPv6 reassembly Id         Remain Src             \tDst\n");
	}

	snprintk(src, ADDR_LEN, "%s", net_sprint_ipv6_addr(&reass->src));

	printk("%p      0x%08x  %5d %16s\t%16s\n",
	       reass, reass->id, k_delayed_work_remaining_get(&reass->timer),
	       src, net_sprint_ipv6_addr(&reass->dst));

	for (i = 0; i < NET_IPV6_FRAGMENTS_MAX_PKT; i++) {
		if (reass->pkt[i]) {
			struct net_buf *frag = reass->pkt[i]->frags;

			printk("[%d] pkt %p->", i, reass->pkt[i]);

			while (frag) {
				printk("%p", frag);

				frag = frag->frags;
				if (frag) {
					printk("->");
				}
			}

			printk("\n");
		}
	}

	(*count)++;
}
#endif /* CONFIG_NET_IPV6_FRAGMENT */

#if defined(CONFIG_NET_DEBUG_NET_PKT)
static void allocs_cb(struct net_pkt *pkt,
		      struct net_buf *buf,
		      const char *func_alloc,
		      int line_alloc,
		      const char *func_free,
		      int line_free,
		      bool in_use,
		      void *user_data)
{
	const char *str;

	if (in_use) {
		str = "used";
	} else {
		if (func_alloc) {
			str = "free";
		} else {
			str = "avail";
		}
	}

	if (buf) {
		goto buf;
	}

	if (func_alloc) {
		if (in_use) {
			printk("%p/%d\t%5s\t%5s\t%s():%d\n", pkt, pkt->ref,
			       str, net_pkt_slab2str(pkt->slab), func_alloc,
			       line_alloc);
		} else {
			printk("%p\t%5s\t%5s\t%s():%d -> %s():%d\n", pkt,
			       str, net_pkt_slab2str(pkt->slab), func_alloc,
			       line_alloc, func_free, line_free);
		}
	}

	return;
buf:
	if (func_alloc) {
		struct net_buf_pool *pool = net_buf_pool_get(buf->pool_id);

		if (in_use) {
			printk("%p/%d\t%5s\t%5s\t%s():%d\n", buf, buf->ref,
			       str, net_pkt_pool2str(pool), func_alloc,
			       line_alloc);
		} else {
			printk("%p\t%5s\t%5s\t%s():%d -> %s():%d\n", buf,
			       str, net_pkt_pool2str(pool), func_alloc,
			       line_alloc, func_free, line_free);
		}
	}
}
#endif /* CONFIG_NET_DEBUG_NET_PKT */

/* Put the actual shell commands after this */

int net_shell_cmd_allocs(int argc, char *argv[])
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

#if defined(CONFIG_NET_DEBUG_NET_PKT)
	printk("Network memory allocations\n\n");
	printk("memory\t\tStatus\tPool\tFunction alloc -> freed\n");
	net_pkt_allocs_foreach(allocs_cb, NULL);
#else
	printk("Enable CONFIG_NET_DEBUG_NET_PKT to see allocations.\n");
#endif /* CONFIG_NET_DEBUG_NET_PKT */

	return 0;
}

#if defined(CONFIG_NET_DEBUG_APP) && \
	(defined(CONFIG_NET_APP_SERVER) || defined(CONFIG_NET_APP_CLIENT))

#if defined(CONFIG_NET_APP_TLS) || defined(CONFIG_NET_APP_DTLS)
static void print_app_sec_info(struct net_app_ctx *ctx, const char *sec_type)
{
	printk("     Security: %s  Thread id: %p\n", sec_type, ctx->tls.tid);

#if defined(CONFIG_INIT_STACKS)
	{
		unsigned int pcnt, unused;

		net_analyze_stack_get_values(
			K_THREAD_STACK_BUFFER(ctx->tls.stack),
			ctx->tls.stack_size,
			&pcnt, &unused);
		printk("     Stack: %p  Size: %d bytes unused %u usage "
		       "%u/%d (%u %%)\n",
		       ctx->tls.stack, ctx->tls.stack_size,
		       unused, ctx->tls.stack_size - unused,
		       ctx->tls.stack_size, pcnt);
	}
#endif /* CONFIG_INIT_STACKS */

	if (ctx->tls.cert_host) {
		printk("     Cert host: %s\n", ctx->tls.cert_host);
	}
}
#endif /* CONFIG_NET_APP_TLS || CONFIG_NET_APP_DTLS */

static void net_app_cb(struct net_app_ctx *ctx, void *user_data)
{
	int *count = user_data;
	char *sec_type = "none";
	char *app_type = "unknown";
	char *proto = "unknown";
	bool printed = false;

#if defined(CONFIG_NET_IPV6) && !defined(CONFIG_NET_IPV4)
#define ADDR_LEN NET_IPV6_ADDR_LEN
#elif defined(CONFIG_NET_IPV4) && !defined(CONFIG_NET_IPV6)
#define ADDR_LEN NET_IPV4_ADDR_LEN
#else
#define ADDR_LEN NET_IPV6_ADDR_LEN
#endif
	/* +7 for []:port */
	char addr_local[ADDR_LEN + 7];
	char addr_remote[ADDR_LEN + 7] = "";

	if (*count == 0) {
		if (ctx->app_type == NET_APP_SERVER) {
			printk("Network application server instances\n\n");
		} else if (ctx->app_type == NET_APP_CLIENT) {
			printk("Network application client instances\n\n");
		} else {
			printk("Invalid network application type %d\n",
			       ctx->app_type);
		}
	}

	if (IS_ENABLED(CONFIG_NET_APP_TLS) && ctx->is_tls) {
		if (ctx->sock_type == SOCK_STREAM) {
			sec_type = "TLS";
		}
	}

	if (IS_ENABLED(CONFIG_NET_APP_DTLS) && ctx->is_tls) {
		if (ctx->sock_type == SOCK_DGRAM) {
			sec_type = "DTLS";
		}
	}

	if (ctx->app_type == NET_APP_SERVER) {
		app_type = "server";
	} else if (ctx->app_type == NET_APP_CLIENT) {
		app_type = "client";
	}

	if (ctx->proto == IPPROTO_UDP) {
#if defined(CONFIG_NET_UDP)
		proto = "UDP";
#else
		proto = "<UDP not configured>";
#endif
	}

	if (ctx->proto == IPPROTO_TCP) {
#if defined(CONFIG_NET_TCP)
		proto = "TCP";
#else
		proto = "<TCP not configured>";
#endif
	}

	printk("[%2d] App-ctx: %p  Status: %s  Type: %s  Protocol: %s\n",
	       *count, ctx, ctx->is_enabled ? "enabled" : "disabled",
	       app_type, proto);

#if defined(CONFIG_NET_APP_TLS) || defined(CONFIG_NET_APP_DTLS)
	if (ctx->is_tls) {
		print_app_sec_info(ctx, sec_type);
	}
#endif /* CONFIG_NET_APP_TLS || CONFIG_NET_APP_DTLS */

#if defined(CONFIG_NET_IPV6)
	if (ctx->app_type == NET_APP_SERVER) {
		if (ctx->ipv6.ctx && ctx->ipv6.ctx->local.family == AF_INET6) {
			get_addresses(ctx->ipv6.ctx,
				      addr_local, sizeof(addr_local),
				      addr_remote, sizeof(addr_remote));

			printk("     Listen IPv6: %16s <- %16s\n",
			       addr_local, addr_remote);
		} else {
			printk("     Not listening IPv6 connections.\n");
		}
	} else if (ctx->app_type == NET_APP_CLIENT) {
		if (ctx->ipv6.ctx && ctx->ipv6.ctx->local.family == AF_INET6) {
			get_addresses(ctx->ipv6.ctx,
				      addr_local, sizeof(addr_local),
				      addr_remote, sizeof(addr_remote));

			printk("     Connect IPv6: %16s -> %16s\n",
			       addr_local, addr_remote);
		}
	} else {
		printk("Invalid application type %d\n", ctx->app_type);
		printed = true;
	}
#else
	printk("     IPv6 connections not enabled.\n");
#endif

#if defined(CONFIG_NET_IPV4)
	if (ctx->app_type == NET_APP_SERVER) {
		if (ctx->ipv4.ctx && ctx->ipv4.ctx->local.family == AF_INET) {
			get_addresses(ctx->ipv4.ctx,
				      addr_local, sizeof(addr_local),
				      addr_remote, sizeof(addr_remote));

			printk("     Listen IPv4: %16s <- %16s\n",
			       addr_local, addr_remote);
		} else {
			printk("     Not listening IPv4 connections.\n");
		}
	} else if (ctx->app_type == NET_APP_CLIENT) {
		if (ctx->ipv4.ctx && ctx->ipv4.ctx->local.family == AF_INET) {
			get_addresses(ctx->ipv4.ctx,
				      addr_local, sizeof(addr_local),
				      addr_remote, sizeof(addr_remote));

			printk("     Connect IPv4: %16s -> %16s\n",
			       addr_local, addr_remote);
		}
	} else {
		if (!printed) {
			printk("Invalid application type %d\n", ctx->app_type);
		}
	}
#else
	printk("     IPv4 connections not enabled.\n");
#endif

#if defined(CONFIG_NET_APP_SERVER)
#if defined(CONFIG_NET_TCP)
	{
		int i, found = 0;

		for (i = 0; i < CONFIG_NET_APP_SERVER_NUM_CONN; i++) {
			if (!ctx->server.net_ctxs[i] ||
			    !net_context_is_used(ctx->server.net_ctxs[i])) {
				continue;
			}

			get_addresses(ctx->server.net_ctxs[i],
				      addr_local, sizeof(addr_local),
				      addr_remote, sizeof(addr_remote));

			printk("     Active: %16s <- %16s\n",
			       addr_local, addr_remote);
			found++;
		}

		if (!found) {
			printk("     No active connections to this server.\n");
		}
	}
#else
	printk("     TCP not enabled for this server.\n");
#endif
#endif /* CONFIG_NET_APP_SERVER */

	(*count)++;

	return;
}
#elif defined(CONFIG_NET_DEBUG_APP)
static void net_app_cb(struct net_app_ctx *ctx, void *user_data) {}
#endif

int net_shell_cmd_app(int argc, char *argv[])
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

#if defined(CONFIG_NET_DEBUG_APP)
	int i = 0;

	if (IS_ENABLED(CONFIG_NET_APP_SERVER)) {
		net_app_server_foreach(net_app_cb, &i);

		if (i == 0) {
			printk("No net app server instances found.\n");
			i = -1;
		}
	}

	if (IS_ENABLED(CONFIG_NET_APP_CLIENT)) {
		if (i) {
			printk("\n");
			i = 0;
		}

		net_app_client_foreach(net_app_cb, &i);

		if (i == 0) {
			printk("No net app client instances found.\n");
		}
	}
#else
	printk("Enable CONFIG_NET_DEBUG_APP and either CONFIG_NET_APP_CLIENT "
	       "or CONFIG_NET_APP_SERVER to see client/server instance "
	       "information.\n");
#endif

	return 0;
}

#if defined(CONFIG_NET_ARP)
static void arp_cb(struct arp_entry *entry, void *user_data)
{
	int *count = user_data;

	if (*count == 0) {
		printk("     Interface  Link              Address\n");
	}

	printk("[%2d] %p %s %s\n", *count, entry->iface,
	       net_sprint_ll_addr(entry->eth.addr,
				  sizeof(struct net_eth_addr)),
	       net_sprint_ipv4_addr(&entry->ip));

	(*count)++;
}
#endif /* CONFIG_NET_ARP */

int net_shell_cmd_arp(int argc, char *argv[])
{
	ARG_UNUSED(argc);

#if defined(CONFIG_NET_ARP)
	int arg = 1;

	if (!argv[arg]) {
		/* ARP cache content */
		int count = 0;

		if (net_arp_foreach(arp_cb, &count) == 0) {
			printk("ARP cache is empty.\n");
		}

		return 0;
	}

	if (strcmp(argv[arg], "flush") == 0) {
		printk("Flushing ARP cache.\n");
		net_arp_clear_cache();
		return 0;
	}
#else
	printk("Enable CONFIG_NET_ARP, CONFIG_NET_IPV4 and "
	       "CONFIG_NET_L2_ETHERNET to see ARP information.\n");
#endif

	return 0;
}

int net_shell_cmd_conn(int argc, char *argv[])
{
	int count = 0;

	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	printk("     Context   \tIface         Flags "
	       "Local           \tRemote\n");

	net_context_foreach(context_cb, &count);

	if (count == 0) {
		printk("No connections\n");
	}

#if defined(CONFIG_NET_DEBUG_CONN)
	printk("\n     Handler    Callback  \tProto\t"
	       "Local           \tRemote\n");

	count = 0;

	net_conn_foreach(conn_handler_cb, &count);

	if (count == 0) {
		printk("No connection handlers found.\n");
	}
#endif

#if defined(CONFIG_NET_TCP)
	printk("\nTCP        Context   Src port Dst port   Send-Seq   Send-Ack  MSS"
	       "%s\n", IS_ENABLED(CONFIG_NET_DEBUG_TCP) ? "    State" : "");

	count = 0;

	net_tcp_foreach(tcp_cb, &count);

	if (count == 0) {
		printk("No TCP connections\n");
	} else {
#if defined(CONFIG_NET_DEBUG_TCP)
		/* Print information about pending packets */
		count = 0;
		net_tcp_foreach(tcp_sent_list_cb, &count);
#endif /* CONFIG_NET_DEBUG_TCP */
	}
#endif

#if defined(CONFIG_NET_IPV6_FRAGMENT)
	count = 0;

	net_ipv6_frag_foreach(ipv6_frag_cb, &count);

	/* Do not print anything if no fragments are pending atm */
#endif

	return 0;
}

#if defined(CONFIG_DNS_RESOLVER)
static void dns_result_cb(enum dns_resolve_status status,
			  struct dns_addrinfo *info,
			  void *user_data)
{
	bool *first = user_data;

	if (status == DNS_EAI_CANCELED) {
		printk("\nTimeout while resolving name.\n");
		*first = false;
		return;
	}

	if (status == DNS_EAI_INPROGRESS && info) {
		char addr[NET_IPV6_ADDR_LEN];

		if (*first) {
			printk("\n");
			*first = false;
		}

		if (info->ai_family == AF_INET) {
			net_addr_ntop(AF_INET,
				      &net_sin(&info->ai_addr)->sin_addr,
				      addr, NET_IPV4_ADDR_LEN);
		} else if (info->ai_family == AF_INET6) {
			net_addr_ntop(AF_INET6,
				      &net_sin6(&info->ai_addr)->sin6_addr,
				      addr, NET_IPV6_ADDR_LEN);
		} else {
			strncpy(addr, "Invalid protocol family",
				sizeof(addr));
		}

		printk("\t%s\n", addr);
		return;
	}

	if (status == DNS_EAI_ALLDONE) {
		printk("All results received\n");
		*first = false;
		return;
	}

	if (status == DNS_EAI_FAIL) {
		printk("No such name found.\n");
		*first = false;
		return;
	}

	printk("Unhandled status %d received\n", status);
}

static void print_dns_info(struct dns_resolve_context *ctx)
{
	int i;

	printk("DNS servers:\n");

	for (i = 0; i < CONFIG_DNS_RESOLVER_MAX_SERVERS + MDNS_SERVER_COUNT; i++) {
		if (ctx->servers[i].dns_server.sa_family == AF_INET) {
			printk("\t%s:%u\n",
			       net_sprint_ipv4_addr(
				       &net_sin(&ctx->servers[i].dns_server)->
				       sin_addr),
			       ntohs(net_sin(&ctx->servers[i].
					     dns_server)->sin_port));
		} else if (ctx->servers[i].dns_server.sa_family == AF_INET6) {
			printk("\t[%s]:%u\n",
			       net_sprint_ipv6_addr(
				       &net_sin6(&ctx->servers[i].dns_server)->
				       sin6_addr),
			       ntohs(net_sin6(&ctx->servers[i].
					     dns_server)->sin6_port));
		}
	}

	printk("Pending queries:\n");

	for (i = 0; i < CONFIG_DNS_NUM_CONCUR_QUERIES; i++) {
		s32_t remaining;

		if (!ctx->queries[i].cb) {
			continue;
		}

		remaining =
			k_delayed_work_remaining_get(&ctx->queries[i].timer);

		if (ctx->queries[i].query_type == DNS_QUERY_TYPE_A) {
			printk("\tIPv4[%u]: %s remaining %d\n",
			       ctx->queries[i].id,
			       ctx->queries[i].query,
			       remaining);
		} else if (ctx->queries[i].query_type == DNS_QUERY_TYPE_AAAA) {
			printk("\tIPv6[%u]: %s remaining %d\n",
			       ctx->queries[i].id,
			       ctx->queries[i].query,
			       remaining);
		}
	}
}
#endif

int net_shell_cmd_dns(int argc, char *argv[])
{
#if defined(CONFIG_DNS_RESOLVER)
#define DNS_TIMEOUT 2000 /* ms */

	struct dns_resolve_context *ctx;
	enum dns_query_type qtype = DNS_QUERY_TYPE_A;
	char *host, *type = NULL;
	bool first = true;
	int arg = 1;
	int ret, i;

	if (!argv[arg]) {
		/* DNS status */
		ctx = dns_resolve_get_default();
		if (!ctx) {
			printk("No default DNS context found.\n");
			return 0;
		}

		print_dns_info(ctx);
		return 0;
	}

	if (strcmp(argv[arg], "cancel") == 0) {
		ctx = dns_resolve_get_default();
		if (!ctx) {
			printk("No default DNS context found.\n");
			return 0;
		}

		for (ret = 0, i = 0; i < CONFIG_DNS_NUM_CONCUR_QUERIES; i++) {
			if (!ctx->queries[i].cb) {
				continue;
			}

			if (!dns_resolve_cancel(ctx, ctx->queries[i].id)) {
				ret++;
			}
		}

		if (ret) {
			printk("Cancelled %d pending requests.\n", ret);
		} else {
			printk("No pending DNS requests.\n");
		}

		return 0;
	}

	host = argv[arg++];

	if (argv[arg]) {
		type = argv[arg];
	}

	if (type) {
		if (strcmp(type, "A") == 0) {
			qtype = DNS_QUERY_TYPE_A;
			printk("IPv4 address type\n");
		} else if (strcmp(type, "AAAA") == 0) {
			qtype = DNS_QUERY_TYPE_AAAA;
			printk("IPv6 address type\n");
		} else {
			printk("Unknown query type, specify either "
			       "A or AAAA\n");
			return 0;
		}
	}

	ret = dns_get_addr_info(host, qtype, NULL, dns_result_cb, &first,
				DNS_TIMEOUT);
	if (ret < 0) {
		printk("Cannot resolve '%s' (%d)\n", host, ret);
	} else {
		printk("Query for '%s' sent.\n", host);
	}

#else
	printk("DNS resolver not supported.\n");
#endif
	return 0;
}

#if defined(CONFIG_NET_DEBUG_HTTP_CONN) && defined(CONFIG_HTTP_SERVER)
#define MAX_HTTP_OUTPUT_LEN 64
static char *http_str_output(char *output, int outlen, const char *str, int len)
{
	if (len > outlen) {
		len = outlen;
	}

	if (len == 0) {
		memset(output, 0, outlen);
	} else {
		memcpy(output, str, len);
		output[len] = '\0';
	}

	return output;
}

static void http_server_cb(struct http_ctx *entry, void *user_data)
{
	int *count = user_data;
	static char output[MAX_HTTP_OUTPUT_LEN];
	int i;

	/* +7 for []:port */
	char addr_local[ADDR_LEN + 7];
	char addr_remote[ADDR_LEN + 7] = "";

	if (*count == 0) {
		printk("        HTTP ctx    Local           \t"
		       "Remote          \tURL\n");
	}

	(*count)++;

	for (i = 0; i < CONFIG_NET_APP_SERVER_NUM_CONN; i++) {
		if (!entry->app_ctx.server.net_ctxs[i] ||
		    !net_context_is_used(entry->app_ctx.server.net_ctxs[i])) {
			continue;
		}

		get_addresses(entry->app_ctx.server.net_ctxs[i],
			      addr_local, sizeof(addr_local),
			      addr_remote, sizeof(addr_remote));

		printk("[%2d] %c%c %p  %16s\t%16s\t%s\n",
		       *count,
		       entry->app_ctx.is_enabled ? 'E' : 'D',
		       entry->is_tls ? 'S' : ' ',
		       entry, addr_local, addr_remote,
		       http_str_output(output, sizeof(output) - 1,
				       entry->http.url, entry->http.url_len));
	}
}
#endif /* CONFIG_NET_DEBUG_HTTP_CONN && CONFIG_HTTP_SERVER */

int net_shell_cmd_http(int argc, char *argv[])
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

#if defined(CONFIG_NET_DEBUG_HTTP_CONN) && defined(CONFIG_HTTP_SERVER)
	static int count;
	int arg = 1;

	count = 0;

	/* Turn off monitoring if it was enabled */
	http_server_conn_monitor(NULL, NULL);

	if (strcmp(argv[0], "http")) {
		arg++;
	}

	if (argv[arg]) {
		if (strcmp(argv[arg], "monitor") == 0) {
			printk("Activating HTTP monitor. Type \"net http\" "
			       "to disable HTTP connection monitoring.\n");
			http_server_conn_monitor(http_server_cb, &count);
		}
	} else {
		http_server_conn_foreach(http_server_cb, &count);
	}
#else
	printk("Enable CONFIG_NET_DEBUG_HTTP_CONN and CONFIG_HTTP_SERVER "
	       "to get HTTP server connection information\n");
#endif

	return 0;
}

int net_shell_cmd_iface(int argc, char *argv[])
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

#if defined(CONFIG_NET_HOSTNAME_ENABLE)
	printk("Hostname: %s\n\n", net_hostname_get());
#endif

	net_if_foreach(iface_cb, NULL);

	return 0;
}

struct ctx_info {
	int pos;
	bool are_external_pools;
	struct k_mem_slab *tx_slabs[CONFIG_NET_MAX_CONTEXTS];
	struct net_buf_pool *data_pools[CONFIG_NET_MAX_CONTEXTS];
};

#if defined(CONFIG_NET_CONTEXT_NET_PKT_POOL)
static bool slab_pool_found_already(struct ctx_info *info,
				    struct k_mem_slab *slab,
				    struct net_buf_pool *pool)
{
	int i;

	for (i = 0; i < CONFIG_NET_MAX_CONTEXTS; i++) {
		if (slab) {
			if (info->tx_slabs[i] == slab) {
				return true;
			}
		} else {
			if (info->data_pools[i] == pool) {
				return true;
			}
		}
	}

	return false;
}
#endif

static void context_info(struct net_context *context, void *user_data)
{
#if defined(CONFIG_NET_CONTEXT_NET_PKT_POOL)
	struct ctx_info *info = user_data;
	struct k_mem_slab *slab;
	struct net_buf_pool *pool;

	if (!net_context_is_used(context)) {
		return;
	}

	if (context->tx_slab) {
		slab = context->tx_slab();

		if (slab_pool_found_already(info, slab, NULL)) {
			return;
		}

#if defined(CONFIG_NET_DEBUG_NET_PKT)
		printk("%p\t%u\t%u\tETX\n",
		       slab, slab->num_blocks, k_mem_slab_num_free_get(slab));
#else
		printk("%p\t%d\tETX\n", slab, slab->num_blocks);
#endif
		info->are_external_pools = true;
		info->tx_slabs[info->pos] = slab;
	}

	if (context->data_pool) {
		pool = context->data_pool();

		if (slab_pool_found_already(info, NULL, pool)) {
			return;
		}

#if defined(CONFIG_NET_DEBUG_NET_PKT)
		printk("%p\t%d\t%d\tEDATA (%s)\n",
		       pool, pool->buf_count,
		       pool->avail_count, pool->name);
#else
		printk("%p\t%d\tEDATA\n", pool, pool->buf_count);
#endif
		info->are_external_pools = true;
		info->data_pools[info->pos] = pool;
	}

	info->pos++;
#endif /* CONFIG_NET_CONTEXT_NET_PKT_POOL */
}

int net_shell_cmd_mem(int argc, char *argv[])
{
	struct k_mem_slab *rx, *tx;
	struct net_buf_pool *rx_data, *tx_data;

	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	net_pkt_get_info(&rx, &tx, &rx_data, &tx_data);

	printk("Fragment length %d bytes\n", CONFIG_NET_BUF_DATA_SIZE);

	printk("Network buffer pools:\n");

#if defined(CONFIG_NET_BUF_POOL_USAGE)
	printk("Address\t\tTotal\tAvail\tName\n");

	printk("%p\t%d\t%u\tRX\n",
	       rx, rx->num_blocks, k_mem_slab_num_free_get(rx));

	printk("%p\t%d\t%u\tTX\n",
	       tx, tx->num_blocks, k_mem_slab_num_free_get(tx));

	printk("%p\t%d\t%d\tRX DATA (%s)\n",
	       rx_data, rx_data->buf_count,
	       rx_data->avail_count, rx_data->name);

	printk("%p\t%d\t%d\tTX DATA (%s)\n",
	       tx_data, tx_data->buf_count,
	       tx_data->avail_count, tx_data->name);
#else
	printk("(CONFIG_NET_BUF_POOL_USAGE to see free #s)\n");
	printk("Address\t\tTotal\tName\n");

	printk("%p\t%d\tRX\n", rx, rx->num_blocks);
	printk("%p\t%d\tTX\n", tx, tx->num_blocks);
	printk("%p\t%d\tRX DATA\n", rx_data, rx_data->buf_count);
	printk("%p\t%d\tTX DATA\n", tx_data, tx_data->buf_count);
#endif /* CONFIG_NET_BUF_POOL_USAGE */

	if (IS_ENABLED(CONFIG_NET_CONTEXT_NET_PKT_POOL)) {
		struct ctx_info info;

		memset(&info, 0, sizeof(info));
		net_context_foreach(context_info, &info);

		if (!info.are_external_pools) {
			printk("No external memory pools found.\n");
		}
	}

	return 0;
}

#if defined(CONFIG_NET_IPV6)
static void nbr_cb(struct net_nbr *nbr, void *user_data)
{
	int *count = user_data;
	char *padding = "";
	char *state_pad = "";
	const char *state_str;

#if defined(CONFIG_NET_L2_IEEE802154)
	padding = "      ";
#endif

	if (*count == 0) {
		printk("     Neighbor   Interface        Flags State     "
		       "Remain  Link              %sAddress\n", padding);
	}

	(*count)++;

	state_str = net_ipv6_nbr_state2str(net_ipv6_nbr_data(nbr)->state);

	/* This is not a proper way but the minimal libc does not honor
	 * string lengths in %s modifier so in order the output to look
	 * nice, do it like this.
	 */
	if (strlen(state_str) == 5) {
		state_pad = "    ";
	}

	printk("[%2d] %p %p %5d/%d/%d/%d %s%s %6d  %17s%s %s\n",
	       *count, nbr, nbr->iface,
	       net_ipv6_nbr_data(nbr)->link_metric,
	       nbr->ref,
	       net_ipv6_nbr_data(nbr)->ns_count,
	       net_ipv6_nbr_data(nbr)->is_router,
	       state_str,
	       state_pad,
#if defined(CONFIG_NET_IPV6_ND)
	       k_delayed_work_remaining_get(
		       &net_ipv6_nbr_data(nbr)->reachable),
#else
	       0,
#endif
	       nbr->idx == NET_NBR_LLADDR_UNKNOWN ? "?" :
	       net_sprint_ll_addr(
		       net_nbr_get_lladdr(nbr->idx)->addr,
		       net_nbr_get_lladdr(nbr->idx)->len),
	       net_nbr_get_lladdr(nbr->idx)->len == 8 ? "" : padding,
	       net_sprint_ipv6_addr(&net_ipv6_nbr_data(nbr)->addr));
}
#endif

int net_shell_cmd_nbr(int argc, char *argv[])
{
#if defined(CONFIG_NET_IPV6)
	int count = 0;
	int arg = 1;

	if (argv[arg]) {
		struct in6_addr addr;
		int ret;

		if (strcmp(argv[arg], "rm")) {
			printk("Unknown command '%s'\n", argv[arg]);
			return 0;
		}

		if (!argv[++arg]) {
			printk("Neighbor IPv6 address missing.\n");
			return 0;
		}

		ret = net_addr_pton(AF_INET6, argv[arg], &addr);
		if (ret < 0) {
			printk("Cannot parse '%s'\n", argv[arg]);
			return 0;
		}

		if (!net_ipv6_nbr_rm(NULL, &addr)) {
			printk("Cannot remove neighbor %s\n",
			       net_sprint_ipv6_addr(&addr));
		} else {
			printk("Neighbor %s removed.\n",
			       net_sprint_ipv6_addr(&addr));
		}
	}

	net_ipv6_nbr_foreach(nbr_cb, &count);

	if (count == 0) {
		printk("No neighbors.\n");
	}
#else
	printk("IPv6 not enabled.\n");
#endif /* CONFIG_NET_IPV6 */

	return 0;
}

#if defined(CONFIG_NET_IPV6) || defined(CONFIG_NET_IPV4)

K_SEM_DEFINE(ping_timeout, 0, 1);

#if defined(CONFIG_NET_IPV6)

static enum net_verdict _handle_ipv6_echo_reply(struct net_pkt *pkt);

static struct net_icmpv6_handler ping6_handler = {
	.type = NET_ICMPV6_ECHO_REPLY,
	.code = 0,
	.handler = _handle_ipv6_echo_reply,
};

static inline void _remove_ipv6_ping_handler(void)
{
	net_icmpv6_unregister_handler(&ping6_handler);
}

static enum net_verdict _handle_ipv6_echo_reply(struct net_pkt *pkt)
{
	char addr[NET_IPV6_ADDR_LEN];

	snprintk(addr, sizeof(addr), "%s",
		 net_sprint_ipv6_addr(&NET_IPV6_HDR(pkt)->dst));

	printk("Received echo reply from %s to %s\n",
	       net_sprint_ipv6_addr(&NET_IPV6_HDR(pkt)->src), addr);

	k_sem_give(&ping_timeout);
	_remove_ipv6_ping_handler();

	net_pkt_unref(pkt);
	return NET_OK;
}

static int _ping_ipv6(char *host)
{
	struct in6_addr ipv6_target;
	struct net_if *iface = net_if_get_default();
	struct net_nbr *nbr;
	int ret;

#if defined(CONFIG_NET_ROUTE)
	struct net_route_entry *route;
#endif

	if (net_addr_pton(AF_INET6, host, &ipv6_target) < 0) {
		return -EINVAL;
	}

	net_icmpv6_register_handler(&ping6_handler);

	nbr = net_ipv6_nbr_lookup(NULL, &ipv6_target);
	if (nbr) {
		iface = nbr->iface;
	}

#if defined(CONFIG_NET_ROUTE)
	route = net_route_lookup(NULL, &ipv6_target);
	if (route) {
		iface = route->iface;
	}
#endif

	ret = net_icmpv6_send_echo_request(iface,
					   &ipv6_target,
					   sys_rand32_get(),
					   sys_rand32_get());
	if (ret) {
		_remove_ipv6_ping_handler();
	} else {
		printk("Sent a ping to %s\n", host);
	}

	return ret;
}
#else
#define _ping_ipv6(...) -EINVAL
#define _remove_ipv6_ping_handler()
#endif /* CONFIG_NET_IPV6 */

#if defined(CONFIG_NET_IPV4)

static enum net_verdict _handle_ipv4_echo_reply(struct net_pkt *pkt);

static struct net_icmpv4_handler ping4_handler = {
	.type = NET_ICMPV4_ECHO_REPLY,
	.code = 0,
	.handler = _handle_ipv4_echo_reply,
};

static inline void _remove_ipv4_ping_handler(void)
{
	net_icmpv4_unregister_handler(&ping4_handler);
}

static enum net_verdict _handle_ipv4_echo_reply(struct net_pkt *pkt)
{
	char addr[NET_IPV4_ADDR_LEN];

	snprintk(addr, sizeof(addr), "%s",
		 net_sprint_ipv4_addr(&NET_IPV4_HDR(pkt)->dst));

	printk("Received echo reply from %s to %s\n",
	       net_sprint_ipv4_addr(&NET_IPV4_HDR(pkt)->src), addr);

	k_sem_give(&ping_timeout);
	_remove_ipv4_ping_handler();

	net_pkt_unref(pkt);
	return NET_OK;
}

static int _ping_ipv4(char *host)
{
	struct in_addr ipv4_target;
	int ret;

	if (net_addr_pton(AF_INET, host, &ipv4_target) < 0) {
		return -EINVAL;
	}

	net_icmpv4_register_handler(&ping4_handler);

	ret = net_icmpv4_send_echo_request(net_if_get_default(),
					   &ipv4_target,
					   sys_rand32_get(),
					   sys_rand32_get());
	if (ret) {
		_remove_ipv4_ping_handler();
	} else {
		printk("Sent a ping to %s\n", host);
	}

	return ret;
}
#else
#define _ping_ipv4(...) -EINVAL
#define _remove_ipv4_ping_handler()
#endif /* CONFIG_NET_IPV4 */
#endif /* CONFIG_NET_IPV6 || CONFIG_NET_IPV4 */

int net_shell_cmd_ping(int argc, char *argv[])
{
	char *host;
	int ret;

	ARG_UNUSED(argc);

	if (!strcmp(argv[0], "ping")) {
		host = argv[1];
	} else {
		host = argv[2];
	}

	if (!host) {
		printk("Target host missing\n");
		return 0;
	}

	ret = _ping_ipv6(host);
	if (!ret) {
		goto wait_reply;
	} else if (ret == -EIO) {
		printk("Cannot send IPv6 ping\n");
		return 0;
	}

	ret = _ping_ipv4(host);
	if (ret) {
		if (ret == -EIO) {
			printk("Cannot send IPv4 ping\n");
		} else if (ret == -EINVAL) {
			printk("Invalid IP address\n");
		}

		return 0;
	}

wait_reply:
	ret = k_sem_take(&ping_timeout, K_SECONDS(2));
	if (ret == -EAGAIN) {
		printk("Ping timeout\n");
		_remove_ipv6_ping_handler();
		_remove_ipv4_ping_handler();
	}

	return 0;
}

int net_shell_cmd_route(int argc, char *argv[])
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

#if defined(CONFIG_NET_ROUTE)
	net_if_foreach(iface_per_route_cb, NULL);
#else
	printk("Network route support not compiled in.\n");
#endif

#if defined(CONFIG_NET_ROUTE_MCAST)
	net_if_foreach(iface_per_mcast_route_cb, NULL);
#endif

	return 0;
}

#if defined(CONFIG_NET_RPL)
static int power(int base, unsigned int exp)
{
	int i, result = 1;

	for (i = 0; i < exp; i++) {
		result *= base;
	}

	return result;
}

static void rpl_parent(struct net_rpl_parent *parent, void *user_data)
{
	int *count = user_data;

	if (*count == 0) {
		printk("      Parent     Last TX   Rank  DTSN  Flags DAG\t\t\t"
		       "Address\n");
	}

	(*count)++;

	if (parent->dag) {
		struct net_ipv6_nbr_data *data;
		char addr[NET_IPV6_ADDR_LEN];

		data = net_rpl_get_ipv6_nbr_data(parent);
		if (data) {
			snprintk(addr, sizeof(addr), "%s",
				 net_sprint_ipv6_addr(&data->addr));
		} else {
			snprintk(addr, sizeof(addr), "<unknown>");
		}

		printk("[%2d]%s %p %7d  %5d   %3d  0x%02x  %s\t%s\n",
		       *count,
		       parent->dag->preferred_parent == parent ? "*" : " ",
		       parent, parent->last_tx_time, parent->rank,
		       parent->dtsn, parent->flags,
		       net_sprint_ipv6_addr(&parent->dag->dag_id),
		       addr);
	}
}

#endif /* CONFIG_NET_RPL */

int net_shell_cmd_rpl(int argc, char *argv[])
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

#if defined(CONFIG_NET_RPL)
	struct net_rpl_instance *instance;
	enum net_rpl_mode mode;
	int i, count;

	mode = net_rpl_get_mode();
	printk("RPL Configuration\n");
	printk("=================\n");
	printk("RPL mode                     : %s\n",
	       mode == NET_RPL_MODE_MESH ? "mesh" :
	       (mode == NET_RPL_MODE_FEATHER ? "feather" :
		(mode == NET_RPL_MODE_LEAF ? "leaf" : "<unknown>")));
	printk("Used objective function      : %s\n",
	       IS_ENABLED(CONFIG_NET_RPL_MRHOF) ? "MRHOF" :
	       (IS_ENABLED(CONFIG_NET_RPL_OF0) ? "OF0" : "<unknown>"));
	printk("Used routing metric          : %s\n",
	       IS_ENABLED(CONFIG_NET_RPL_MC_NONE) ? "none" :
	       (IS_ENABLED(CONFIG_NET_RPL_MC_ETX) ? "estimated num of TX" :
		(IS_ENABLED(CONFIG_NET_RPL_MC_ENERGY) ? "energy based" :
		 "<unknown>")));
	printk("Mode of operation (MOP)      : %s\n",
	       IS_ENABLED(CONFIG_NET_RPL_MOP2) ? "Storing, no mcast (MOP2)" :
	       (IS_ENABLED(CONFIG_NET_RPL_MOP3) ? "Storing (MOP3)" :
		"<unknown>"));
	printk("Send probes to nodes         : %s\n",
	       IS_ENABLED(CONFIG_NET_RPL_PROBING) ? "enabled" : "disabled");
	printk("Max instances                : %d\n",
	       CONFIG_NET_RPL_MAX_INSTANCES);
	printk("Max DAG / instance           : %d\n",
	       CONFIG_NET_RPL_MAX_DAG_PER_INSTANCE);

	printk("Min hop rank increment       : %d\n",
	       CONFIG_NET_RPL_MIN_HOP_RANK_INC);
	printk("Initial link metric          : %d\n",
	       CONFIG_NET_RPL_INIT_LINK_METRIC);
	printk("RPL preference value         : %d\n",
	       CONFIG_NET_RPL_PREFERENCE);
	printk("DAG grounded by default      : %s\n",
	       IS_ENABLED(CONFIG_NET_RPL_GROUNDED) ? "yes" : "no");
	printk("Default instance id          : %d (0x%02x)\n",
	       CONFIG_NET_RPL_DEFAULT_INSTANCE,
	       CONFIG_NET_RPL_DEFAULT_INSTANCE);
	printk("Insert Hop-by-hop option     : %s\n",
	       IS_ENABLED(CONFIG_NET_RPL_INSERT_HBH_OPTION) ? "yes" : "no");

	printk("Specify DAG when sending DAO : %s\n",
	       IS_ENABLED(CONFIG_NET_RPL_DAO_SPECIFY_DAG) ? "yes" : "no");
	printk("DIO min interval             : %d (%d ms)\n",
	       CONFIG_NET_RPL_DIO_INTERVAL_MIN,
	       power(2, CONFIG_NET_RPL_DIO_INTERVAL_MIN));
	printk("DIO doublings interval       : %d\n",
	       CONFIG_NET_RPL_DIO_INTERVAL_DOUBLINGS);
	printk("DIO redundancy value         : %d\n",
	       CONFIG_NET_RPL_DIO_REDUNDANCY);

	printk("DAO sending timer value      : %d sec\n",
	       CONFIG_NET_RPL_DAO_TIMER);
	printk("DAO max retransmissions      : %d\n",
	       CONFIG_NET_RPL_DAO_MAX_RETRANSMISSIONS);
	printk("Node expecting DAO ack       : %s\n",
	       IS_ENABLED(CONFIG_NET_RPL_DAO_ACK) ? "yes" : "no");

	printk("Send DIS periodically        : %s\n",
	       IS_ENABLED(CONFIG_NET_RPL_DIS_SEND) ? "yes" : "no");
#if defined(CONFIG_NET_RPL_DIS_SEND)
	printk("DIS interval                 : %d sec\n",
	       CONFIG_NET_RPL_DIS_INTERVAL);
#endif

	printk("Default route lifetime unit  : %d sec\n",
	       CONFIG_NET_RPL_DEFAULT_LIFETIME_UNIT);
	printk("Default route lifetime       : %d\n",
	       CONFIG_NET_RPL_DEFAULT_LIFETIME);
#if defined(CONFIG_NET_RPL_MOP3)
	printk("Multicast route lifetime     : %d\n",
	       CONFIG_NET_RPL_MCAST_LIFETIME);
#endif
	printk("\nRuntime status\n");
	printk("==============\n");

	instance = net_rpl_get_default_instance();
	if (!instance) {
		printk("No default RPL instance found.\n");
		return 0;
	}

	printk("Default instance (id %d) : %p (%s)\n", instance->instance_id,
	       instance, instance->is_used ? "active" : "disabled");

	if (instance->default_route) {
		printk("Default route   : %s\n",
		       net_sprint_ipv6_addr(
			       &instance->default_route->address.in6_addr));
	}

#if defined(CONFIG_NET_STATISTICS_RPL)
	printk("DIO statistics  : intervals %d sent %d recv %d\n",
	       instance->dio_intervals, instance->dio_send_pkt,
	       instance->dio_recv_pkt);
#endif /* CONFIG_NET_STATISTICS_RPL */

	printk("Instance DAGs   :\n");
	for (i = 0, count = 0; i < CONFIG_NET_RPL_MAX_DAG_PER_INSTANCE; i++) {
		char prefix[NET_IPV6_ADDR_LEN];

		if (!instance->dags[i].is_used) {
			continue;
		}

		snprintk(prefix, sizeof(prefix), "%s",
			 net_sprint_ipv6_addr(
				 &instance->dags[i].prefix_info.prefix));

		printk("[%2d]%s %s prefix %s/%d rank %d/%d ver %d flags %c%c "
		       "parent %p\n",
		       ++count,
		       &instance->dags[i] == instance->current_dag ? "*" : " ",
		       net_sprint_ipv6_addr(&instance->dags[i].dag_id),
		       prefix, instance->dags[i].prefix_info.length,
		       instance->dags[i].rank, instance->dags[i].min_rank,
		       instance->dags[i].version,
		       instance->dags[i].is_grounded ? 'G' : 'g',
		       instance->dags[i].is_joined ? 'J' : 'j',
		       instance->dags[i].preferred_parent);
	}
	printk("\n");

	count = 0;
	i = net_rpl_foreach_parent(rpl_parent, &count);
	if (i == 0) {
		printk("No parents found.\n");
	}

	printk("\n");
#else
	printk("RPL not enabled, set CONFIG_NET_RPL to enable it.\n");
#endif

	return 0;
}

#if defined(CONFIG_INIT_STACKS)
extern K_THREAD_STACK_DEFINE(_main_stack, CONFIG_MAIN_STACK_SIZE);
extern K_THREAD_STACK_DEFINE(_interrupt_stack, CONFIG_ISR_STACK_SIZE);
extern K_THREAD_STACK_DEFINE(sys_work_q_stack,
			     CONFIG_SYSTEM_WORKQUEUE_STACK_SIZE);
#endif

int net_shell_cmd_stacks(int argc, char *argv[])
{
#if defined(CONFIG_INIT_STACKS)
	unsigned int pcnt, unused;
#endif
	struct net_stack_info *info;

	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	for (info = __net_stack_start; info != __net_stack_end; info++) {
		net_analyze_stack_get_values(K_THREAD_STACK_BUFFER(info->stack),
					     info->size, &pcnt, &unused);

#if defined(CONFIG_INIT_STACKS)
		printk("%s [%s] stack size %zu/%zu bytes unused %u usage"
		       " %zu/%zu (%u %%)\n",
		       info->pretty_name, info->name, info->orig_size,
		       info->size, unused,
		       info->size - unused, info->size, pcnt);
#else
		printk("%s [%s] stack size %zu usage not available\n",
		       info->pretty_name, info->name, info->orig_size);
#endif
	}

#if defined(CONFIG_INIT_STACKS)
	net_analyze_stack_get_values(K_THREAD_STACK_BUFFER(_main_stack),
				     K_THREAD_STACK_SIZEOF(_main_stack),
				     &pcnt, &unused);
	printk("%s [%s] stack size %d/%d bytes unused %u usage"
	       " %d/%d (%u %%)\n",
	       "main", "_main_stack", CONFIG_MAIN_STACK_SIZE,
	       CONFIG_MAIN_STACK_SIZE, unused,
	       CONFIG_MAIN_STACK_SIZE - unused, CONFIG_MAIN_STACK_SIZE, pcnt);

	net_analyze_stack_get_values(K_THREAD_STACK_BUFFER(_interrupt_stack),
				     K_THREAD_STACK_SIZEOF(_interrupt_stack),
				     &pcnt, &unused);
	printk("%s [%s] stack size %d/%d bytes unused %u usage"
	       " %d/%d (%u %%)\n",
	       "ISR", "_interrupt_stack", CONFIG_ISR_STACK_SIZE,
	       CONFIG_ISR_STACK_SIZE, unused,
	       CONFIG_ISR_STACK_SIZE - unused, CONFIG_ISR_STACK_SIZE, pcnt);

	net_analyze_stack_get_values(K_THREAD_STACK_BUFFER(sys_work_q_stack),
				     K_THREAD_STACK_SIZEOF(sys_work_q_stack),
				     &pcnt, &unused);
	printk("%s [%s] stack size %d/%d bytes unused %u usage"
	       " %d/%d (%u %%)\n",
	       "WORKQ", "system workqueue",
	       CONFIG_SYSTEM_WORKQUEUE_STACK_SIZE,
	       CONFIG_SYSTEM_WORKQUEUE_STACK_SIZE, unused,
	       CONFIG_SYSTEM_WORKQUEUE_STACK_SIZE - unused,
	       CONFIG_SYSTEM_WORKQUEUE_STACK_SIZE, pcnt);
#else
	printk("Enable CONFIG_INIT_STACKS to see usage information.\n");
#endif

	return 0;
}

int net_shell_cmd_stats(int argc, char *argv[])
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

#if defined(CONFIG_NET_STATISTICS)
	net_shell_print_statistics();
#else
	printk("Network statistics not compiled in.\n");
#endif

	return 0;
}

#if defined(CONFIG_NET_TCP)
static struct net_context *tcp_ctx;

#define TCP_CONNECT_TIMEOUT K_SECONDS(5) /* ms */
#define TCP_TIMEOUT K_SECONDS(2) /* ms */

static void tcp_connected(struct net_context *context,
			  int status,
			  void *user_data)
{
	if (status < 0) {
		printk("TCP connection failed (%d)\n", status);

		net_context_put(context);

		tcp_ctx = NULL;
	} else {
		printk("TCP connected\n");
	}
}

#if defined(CONFIG_NET_IPV6)
static void get_my_ipv6_addr(struct net_if *iface,
			     struct sockaddr *myaddr)
{
	const struct in6_addr *my6addr;

	my6addr = net_if_ipv6_select_src_addr(iface,
					      &net_sin6(myaddr)->sin6_addr);

	memcpy(&net_sin6(myaddr)->sin6_addr, my6addr, sizeof(struct in6_addr));

	net_sin6(myaddr)->sin6_port = 0; /* let the IP stack to select */
}
#endif

#if defined(CONFIG_NET_IPV4)
static void get_my_ipv4_addr(struct net_if *iface,
			     struct sockaddr *myaddr)
{
	/* Just take the first IPv4 address of an interface. */
	memcpy(&net_sin(myaddr)->sin_addr,
	       &iface->ipv4.unicast[0].address.in_addr,
	       sizeof(struct in_addr));

	net_sin(myaddr)->sin_port = 0; /* let the IP stack to select */
}
#endif

static void print_connect_info(int family,
			       struct sockaddr *myaddr,
			       struct sockaddr *addr)
{
	switch (family) {
	case AF_INET:
#if defined(CONFIG_NET_IPV4)
		printk("Connecting from %s:%u ",
		       net_sprint_ipv4_addr(&net_sin(myaddr)->sin_addr),
		       ntohs(net_sin(myaddr)->sin_port));
		printk("to %s:%u\n",
		       net_sprint_ipv4_addr(&net_sin(addr)->sin_addr),
		       ntohs(net_sin(addr)->sin_port));
#else
		printk("IPv4 not supported\n");
#endif
		break;

	case AF_INET6:
#if defined(CONFIG_NET_IPV6)
		printk("Connecting from [%s]:%u ",
		       net_sprint_ipv6_addr(&net_sin6(myaddr)->sin6_addr),
		       ntohs(net_sin6(myaddr)->sin6_port));
		printk("to [%s]:%u\n",
		       net_sprint_ipv6_addr(&net_sin6(addr)->sin6_addr),
		       ntohs(net_sin6(addr)->sin6_port));
#else
		printk("IPv6 not supported\n");
#endif
		break;

	default:
		printk("Unknown protocol family (%d)\n", family);
		break;
	}
}

static int tcp_connect(char *host, u16_t port, struct net_context **ctx)
{
	struct sockaddr addr;
	struct sockaddr myaddr;
	struct net_nbr *nbr;
	struct net_if *iface = net_if_get_default();
	int addrlen;
	int family;
	int ret;

#if defined(CONFIG_NET_IPV6) && !defined(CONFIG_NET_IPV4)
	ret = net_addr_pton(AF_INET6, host, &net_sin6(&addr)->sin6_addr);
	if (ret < 0) {
		printk("Invalid IPv6 address\n");
		return 0;
	}

	net_sin6(&addr)->sin6_port = htons(port);
	addrlen = sizeof(struct sockaddr_in6);

	nbr = net_ipv6_nbr_lookup(NULL, &net_sin6(&addr)->sin6_addr);
	if (nbr) {
		iface = nbr->iface;
	}

	get_my_ipv6_addr(iface, &myaddr);
	family = addr.sa_family = myaddr.sa_family = AF_INET6;
#endif

#if defined(CONFIG_NET_IPV4) && !defined(CONFIG_NET_IPV6)
	ARG_UNUSED(nbr);

	ret = net_addr_pton(AF_INET, host, &net_sin(&addr)->sin_addr);
	if (ret < 0) {
		printk("Invalid IPv4 address\n");
		return 0;
	}

	get_my_ipv4_addr(iface, &myaddr);
	net_sin(&addr)->sin_port = htons(port);
	addrlen = sizeof(struct sockaddr_in);
	family = addr.sa_family = myaddr.sa_family = AF_INET;
#endif

#if defined(CONFIG_NET_IPV6) && defined(CONFIG_NET_IPV4)
	ret = net_addr_pton(AF_INET6, host, &net_sin6(&addr)->sin6_addr);
	if (ret < 0) {
		ret = net_addr_pton(AF_INET, host, &net_sin(&addr)->sin_addr);
		if (ret < 0) {
			printk("Invalid IP address\n");
			return 0;
		}

		net_sin(&addr)->sin_port = htons(port);
		addrlen = sizeof(struct sockaddr_in);

		get_my_ipv4_addr(iface, &myaddr);
		family = addr.sa_family = myaddr.sa_family = AF_INET;
	} else {
		net_sin6(&addr)->sin6_port = htons(port);
		addrlen = sizeof(struct sockaddr_in6);

		nbr = net_ipv6_nbr_lookup(NULL, &net_sin6(&addr)->sin6_addr);
		if (nbr) {
			iface = nbr->iface;
		}

		get_my_ipv6_addr(iface, &myaddr);
		family = addr.sa_family = myaddr.sa_family = AF_INET6;
	}
#endif

	print_connect_info(family, &myaddr, &addr);

	ret = net_context_get(family, SOCK_STREAM, IPPROTO_TCP, ctx);
	if (ret < 0) {
		printk("Cannot get TCP context (%d)\n", ret);
		return ret;
	}

	ret = net_context_bind(*ctx, &myaddr, addrlen);
	if (ret < 0) {
		printk("Cannot bind TCP (%d)\n", ret);
		return ret;
	}

	return net_context_connect(*ctx, &addr, addrlen, tcp_connected,
				   K_NO_WAIT, NULL);
}

static void tcp_sent_cb(struct net_context *context,
			int status,
			void *token,
			void *user_data)
{
	printk("Message sent\n");
}
#endif

int net_shell_cmd_tcp(int argc, char *argv[])
{
#if defined(CONFIG_NET_TCP)
	int arg = 1;
	int ret;

	if (argv[arg]) {
		if (!strcmp(argv[arg], "connect")) {
			/* tcp connect <ip> port */
			char *ip;
			u16_t port;

			if (tcp_ctx && net_context_is_used(tcp_ctx)) {
				printk("Already connected\n");
				return 0;
			}

			if (!argv[++arg]) {
				printk("Peer IP address missing.\n");
				return 0;
			}

			ip = argv[arg];

			if (!argv[++arg]) {
				printk("Peer port missing.\n");
				return 0;
			}

			port = strtol(argv[arg], NULL, 10);

			return tcp_connect(ip, port, &tcp_ctx);
		}

		if (!strcmp(argv[arg], "send")) {
			/* tcp send <data> */
			struct net_pkt *pkt;

			if (!tcp_ctx || !net_context_is_used(tcp_ctx)) {
				printk("Not connected\n");
				return 0;
			}

			if (!argv[++arg]) {
				printk("No data to send.\n");
				return 0;
			}

			pkt = net_pkt_get_tx(tcp_ctx, TCP_TIMEOUT);
			if (!pkt) {
				printk("Out of pkts, msg cannot be sent.\n");
				return 0;
			}

			ret = net_pkt_append_all(pkt, strlen(argv[arg]),
						 (u8_t *)argv[arg],
						 TCP_TIMEOUT);
			if (!ret) {
				printk("Cannot build msg (out of pkts)\n");
				net_pkt_unref(pkt);
				return 0;
			}

			ret = net_context_send(pkt, tcp_sent_cb, TCP_TIMEOUT,
					       NULL, NULL);
			if (ret < 0) {
				printk("Cannot send msg (%d)\n", ret);
				net_pkt_unref(pkt);
				return 0;
			}

			return 0;
		}

		if (!strcmp(argv[arg], "close")) {
			/* tcp close */
			if (!tcp_ctx || !net_context_is_used(tcp_ctx)) {
				printk("Not connected\n");
				return 0;
			}

			ret = net_context_put(tcp_ctx);
			if (ret < 0) {
				printk("Cannot close the connection (%d)\n",
				       ret);
				return 0;
			}

			printk("Connection closed.\n");
			tcp_ctx = NULL;

			return 0;
		}

		printk("Unknown command '%s'\n", argv[arg]);
		goto usage;
	} else {
		printk("Invalid command.\n");
	usage:
		printk("Usage:\n");
		printk("\ttcp connect <ipaddr> port\n");
		printk("\ttcp send <data>\n");
		printk("\ttcp close\n");
	}
#else
	printk("TCP not enabled.\n");
#endif /* CONFIG_NET_TCP */

	return 0;
}

static struct shell_cmd net_commands[] = {
	/* Keep the commands in alphabetical order */
	{ "allocs", net_shell_cmd_allocs,
		"\n\tPrint network memory allocations" },
	{ "app", net_shell_cmd_app,
		"\n\tPrint network application API usage information" },
	{ "arp", net_shell_cmd_arp,
		"\n\tPrint information about IPv4 ARP cache\n"
		"arp flush\n\tRemove all entries from ARP cache" },
	{ "conn", net_shell_cmd_conn,
		"\n\tPrint information about network connections" },
	{ "dns", net_shell_cmd_dns, "\n\tShow how DNS is configured\n"
		"dns cancel\n\tCancel all pending requests\n"
		"dns <hostname> [A or AAAA]\n\tQuery IPv4 address (default) or "
		"IPv6 address for a  host name" },
	{ "http", net_shell_cmd_http,
		"\n\tPrint information about active HTTP connections\n"
		"http monitor\n\tStart monitoring HTTP connections\n"
		"http\n\tTurn off HTTP connection monitoring" },
	{ "iface", net_shell_cmd_iface,
		"\n\tPrint information about network interfaces" },
	{ "mem", net_shell_cmd_mem,
		"\n\tPrint information about network interfaces" },
	{ "nbr", net_shell_cmd_nbr, "\n\tPrint neighbor information\n"
		"nbr rm <IPv6 address>\n\tRemove neighbor from cache" },
	{ "ping", net_shell_cmd_ping, "<host>\n\tPing a network host" },
	{ "route", net_shell_cmd_route, "\n\tShow network route" },
	{ "rpl", net_shell_cmd_rpl, "\n\tShow RPL mesh routing status" },
	{ "stacks", net_shell_cmd_stacks,
		"\n\tShow network stacks information" },
	{ "stats", net_shell_cmd_stats, "\n\tShow network statistics" },
	{ "tcp", net_shell_cmd_tcp, "connect <ip> port\n\tConnect to TCP peer\n"
		"tcp send <data>\n\tSend data to peer using TCP\n"
		"tcp close\n\tClose TCP connection" },
	{ NULL, NULL, NULL }
};

SHELL_REGISTER(NET_SHELL_MODULE, net_commands);
