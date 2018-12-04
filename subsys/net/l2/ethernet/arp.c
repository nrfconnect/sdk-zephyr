/** @file
 * @brief ARP related functions
 */

/*
 * Copyright (c) 2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_MODULE_NAME net_arp
#define NET_LOG_LEVEL CONFIG_NET_ARP_LOG_LEVEL

#include <errno.h>
#include <net/net_core.h>
#include <net/net_pkt.h>
#include <net/net_if.h>
#include <net/net_stats.h>

#include "arp.h"
#include "net_private.h"

#define NET_BUF_TIMEOUT K_MSEC(100)
#define ARP_REQUEST_TIMEOUT K_SECONDS(2)

static bool arp_cache_initialized;
static struct arp_entry arp_entries[CONFIG_NET_ARP_TABLE_SIZE];

static sys_slist_t arp_free_entries;
static sys_slist_t arp_pending_entries;
static sys_slist_t arp_table;

struct k_delayed_work arp_request_timer;

static void arp_entry_cleanup(struct arp_entry *entry, bool pending)
{
	NET_DBG("%p", entry);

	if (pending) {
		NET_DBG("Releasing pending pkt %p (ref %d)",
			entry->pending, entry->pending->ref - 1);
		net_pkt_unref(entry->pending);
		entry->pending = NULL;
	}

	entry->iface = NULL;

	(void)memset(&entry->ip, 0, sizeof(struct in_addr));
	(void)memset(&entry->eth, 0, sizeof(struct net_eth_addr));
}

static struct arp_entry *arp_entry_find(sys_slist_t *list,
					struct net_if *iface,
					struct in_addr *dst,
					sys_snode_t **previous)
{
	struct arp_entry *entry;

	SYS_SLIST_FOR_EACH_CONTAINER(list, entry, node) {
		NET_DBG("iface %p dst %s",
			iface, log_strdup(net_sprint_ipv4_addr(&entry->ip)));

		if (entry->iface == iface &&
		    net_ipv4_addr_cmp(&entry->ip, dst)) {
			return entry;
		}

		if (previous) {
			*previous = &entry->node;
		}
	}

	return NULL;
}

static inline struct arp_entry *arp_entry_find_move_first(struct net_if *iface,
							  struct in_addr *dst)
{
	sys_snode_t *prev = NULL;
	struct arp_entry *entry;

	NET_DBG("dst %s", log_strdup(net_sprint_ipv4_addr(dst)));

	entry = arp_entry_find(&arp_table, iface, dst, &prev);
	if (entry) {
		/* Let's assume the target is going to be accessed
		 * more than once here in a short time frame. So we
		 * place the entry first in position into the table
		 * in order to reduce subsequent find.
		 */
		if (&entry->node != sys_slist_peek_head(&arp_table)) {
			sys_slist_remove(&arp_table, prev, &entry->node);
			sys_slist_prepend(&arp_table, &entry->node);
		}
	}

	return entry;
}

static inline
struct arp_entry *arp_entry_find_pending(struct net_if *iface,
					 struct in_addr *dst)
{
	NET_DBG("dst %s", log_strdup(net_sprint_ipv4_addr(dst)));

	return arp_entry_find(&arp_pending_entries, iface, dst, NULL);
}

static struct arp_entry *arp_entry_get_pending(struct net_if *iface,
					       struct in_addr *dst)
{
	sys_snode_t *prev = NULL;
	struct arp_entry *entry;

	NET_DBG("dst %s", log_strdup(net_sprint_ipv4_addr(dst)));

	entry = arp_entry_find(&arp_pending_entries, iface, dst, &prev);
	if (entry) {
		/* We remove the entry from the pending list */
		sys_slist_remove(&arp_pending_entries, prev, &entry->node);
	}

	if (sys_slist_is_empty(&arp_pending_entries)) {
		k_delayed_work_cancel(&arp_request_timer);
	}

	return entry;
}

static struct arp_entry *arp_entry_get_free(void)
{
	sys_snode_t *node;

	node = sys_slist_peek_head(&arp_free_entries);
	if (!node) {
		return NULL;
	}

	/* We remove the node from the free list */
	sys_slist_remove(&arp_free_entries, NULL, node);

	return CONTAINER_OF(node, struct arp_entry, node);
}

static struct arp_entry *arp_entry_get_last_from_table(void)
{
	sys_snode_t *node;

	/* We assume last entry is the oldest one,
	 * so is the preferred one to be taken out.
	 */

	node = sys_slist_peek_tail(&arp_table);
	if (!node) {
		return NULL;
	}

	sys_slist_find_and_remove(&arp_table, node);

	return CONTAINER_OF(node, struct arp_entry, node);
}


static void arp_entry_register_pending(struct arp_entry *entry)
{
	NET_DBG("dst %s", log_strdup(net_sprint_ipv4_addr(&entry->ip)));

	sys_slist_append(&arp_pending_entries, &entry->node);

	entry->req_start = k_uptime_get();

	/* Let's start the timer if necessary */
	if (!k_delayed_work_remaining_get(&arp_request_timer)) {
		k_delayed_work_submit(&arp_request_timer,
				      ARP_REQUEST_TIMEOUT);
	}
}

static void arp_request_timeout(struct k_work *work)
{
	s64_t current = k_uptime_get();
	struct arp_entry *entry, *next;

	ARG_UNUSED(work);

	SYS_SLIST_FOR_EACH_CONTAINER_SAFE(&arp_pending_entries,
					  entry, next, node) {
		if ((entry->req_start + ARP_REQUEST_TIMEOUT - current) > 0) {
			break;
		}

		arp_entry_cleanup(entry, true);

		sys_slist_remove(&arp_pending_entries, NULL, &entry->node);
		sys_slist_append(&arp_free_entries, &entry->node);

		entry = NULL;
	}

	if (entry) {
		k_delayed_work_submit(&arp_request_timer,
				      entry->req_start +
				      ARP_REQUEST_TIMEOUT - current);
	}
}

static inline struct in_addr *if_get_addr(struct net_if *iface,
					  struct in_addr *addr)
{
	struct net_if_ipv4 *ipv4 = iface->config.ip.ipv4;
	int i;

	if (!ipv4) {
		return NULL;
	}

	for (i = 0; i < NET_IF_MAX_IPV4_ADDR; i++) {
		if (ipv4->unicast[i].is_used &&
		    ipv4->unicast[i].address.family == AF_INET &&
		    ipv4->unicast[i].addr_state == NET_ADDR_PREFERRED &&
		    (!addr ||
		     net_ipv4_addr_cmp(addr,
				       &ipv4->unicast[i].address.in_addr))) {
			return &ipv4->unicast[i].address.in_addr;
		}
	}

	return NULL;
}

static inline struct net_pkt *arp_prepare(struct net_if *iface,
					  struct in_addr *next_addr,
					  struct arp_entry *entry,
					  struct net_pkt *pending,
					  struct in_addr *current_ip)
{
	struct ethernet_context *ctx = net_if_l2_data(iface);
	int eth_hdr_len = sizeof(struct net_eth_hdr);
	struct net_pkt *pkt;
	struct net_arp_hdr *hdr;
	struct in_addr *my_addr;

	if (net_eth_is_vlan_enabled(ctx, iface) &&
	    net_eth_get_vlan_tag(iface) != NET_VLAN_TAG_UNSPEC) {
		eth_hdr_len = sizeof(struct net_eth_vlan_hdr);
	}

	if (current_ip) {
		/* This is the IPv4 autoconf case where we have already
		 * things setup so no need to allocate new net_pkt
		 */
		pkt = pending;
	} else {
		struct net_buf *frag;

		pkt = net_pkt_get_reserve_tx(eth_hdr_len, NET_BUF_TIMEOUT);
		if (!pkt) {
			return NULL;
		}

		frag = net_pkt_get_frag(pkt, NET_BUF_TIMEOUT);
		if (!frag) {
			net_pkt_unref(pkt);
			return NULL;
		}

		net_pkt_frag_add(pkt, frag);
		net_pkt_set_iface(pkt, iface);
		net_pkt_set_family(pkt, AF_UNSPEC);
	}

	net_pkt_set_vlan_tag(pkt, net_eth_get_vlan_tag(iface));

	net_buf_add(pkt->frags, sizeof(struct net_arp_hdr));

	hdr = NET_ARP_HDR(pkt);

	/* If entry is not set, then we are just about to send
	 * an ARP request using the data in pending net_pkt.
	 * This can happen if there is already a pending ARP
	 * request and we want to send it again.
	 */
	if (entry) {
		entry->pending = net_pkt_ref(pending);
		entry->iface = net_pkt_iface(pkt);

		net_ipaddr_copy(&entry->ip, next_addr);

		net_pkt_lladdr_src(pkt)->addr =
			(u8_t *)net_if_get_link_addr(entry->iface)->addr;

		arp_entry_register_pending(entry);
	} else {
		net_pkt_lladdr_src(pkt)->addr =
			(u8_t *)net_if_get_link_addr(iface)->addr;
	}

	net_pkt_lladdr_src(pkt)->len = sizeof(struct net_eth_addr);

	net_pkt_lladdr_dst(pkt)->addr = (u8_t *)net_eth_broadcast_addr();
	net_pkt_lladdr_dst(pkt)->len = sizeof(struct net_eth_addr);

	hdr->hwtype = htons(NET_ARP_HTYPE_ETH);
	hdr->protocol = htons(NET_ETH_PTYPE_IP);
	hdr->hwlen = sizeof(struct net_eth_addr);
	hdr->protolen = sizeof(struct in_addr);
	hdr->opcode = htons(NET_ARP_REQUEST);

	(void)memset(&hdr->dst_hwaddr.addr, 0x00, sizeof(struct net_eth_addr));

	net_ipaddr_copy(&hdr->dst_ipaddr, next_addr);

	memcpy(hdr->src_hwaddr.addr, net_pkt_lladdr_src(pkt)->addr,
	       sizeof(struct net_eth_addr));

	if (entry) {
		my_addr = if_get_addr(entry->iface, current_ip);
	} else {
		my_addr = current_ip;
	}

	if (my_addr) {
		net_ipaddr_copy(&hdr->src_ipaddr, my_addr);
	} else {
		(void)memset(&hdr->src_ipaddr, 0, sizeof(struct in_addr));
	}

	return pkt;
}

struct net_pkt *net_arp_prepare(struct net_pkt *pkt,
				struct in_addr *request_ip,
				struct in_addr *current_ip)
{
	struct ethernet_context *ctx;
	struct arp_entry *entry;
	struct in_addr *addr;

	if (!pkt || !pkt->frags) {
		return NULL;
	}

	ctx = net_if_l2_data(net_pkt_iface(pkt));

	/* Is the destination in the local network, if not route via
	 * the gateway address.
	 */
	if (!current_ip &&
	    !net_if_ipv4_addr_mask_cmp(net_pkt_iface(pkt), request_ip)) {
		struct net_if_ipv4 *ipv4 = net_pkt_iface(pkt)->config.ip.ipv4;

		if (ipv4) {
			addr = &ipv4->gw;
			if (net_ipv4_is_addr_unspecified(addr)) {
				NET_ERR("Gateway not set for iface %p",
					net_pkt_iface(pkt));

				return NULL;
			}
		} else {
			addr = request_ip;
		}
	} else {
		addr = request_ip;
	}

	/* If the destination address is already known, we do not need
	 * to send any ARP packet.
	 */
	entry = arp_entry_find_move_first(net_pkt_iface(pkt), addr);
	if (!entry) {
		struct net_pkt *req;

		entry = arp_entry_find_pending(net_pkt_iface(pkt), addr);
		if (!entry) {
			/* No pending, let's try to get a new entry */
			entry = arp_entry_get_free();
			if (!entry) {
				/* Then let's take one from table? */
				entry = arp_entry_get_last_from_table();
			}
		} else {
			/* There is a pending already */
			entry = NULL;
		}

		req = arp_prepare(net_pkt_iface(pkt), addr, entry, pkt,
				  current_ip);

		if (!entry) {
			/* We cannot send the packet, the ARP cache is full
			 * or there is already a pending query to this IP
			 * address, so this packet must be discarded.
			 */
			NET_DBG("Resending ARP %p", req);
		}

		return req;
	}

	net_pkt_lladdr_src(pkt)->addr =
		(u8_t *)net_if_get_link_addr(entry->iface)->addr;
	net_pkt_lladdr_src(pkt)->len = sizeof(struct net_eth_addr);

	net_pkt_lladdr_dst(pkt)->addr = (u8_t *)&entry->eth;
	net_pkt_lladdr_dst(pkt)->len = sizeof(struct net_eth_addr);

	NET_DBG("ARP using ll %s for IP %s",
		log_strdup(net_sprint_ll_addr(net_pkt_lladdr_dst(pkt)->addr,
					      sizeof(struct net_eth_addr))),
		log_strdup(net_sprint_ipv4_addr(&NET_IPV4_HDR(pkt)->dst)));

	return pkt;
}

static void arp_gratuitous(struct net_if *iface,
			   struct in_addr *src,
			   struct net_eth_addr *hwaddr)
{
	sys_snode_t *prev = NULL;
	struct arp_entry *entry;

	entry = arp_entry_find(&arp_table, iface, src, &prev);
	if (entry) {
		NET_DBG("Gratuitous ARP hwaddr %s -> %s",
			log_strdup(net_sprint_ll_addr(
					   (const u8_t *)&entry->eth,
					   sizeof(struct net_eth_addr))),
			log_strdup(net_sprint_ll_addr(
					   (const u8_t *)hwaddr,
					   sizeof(struct net_eth_addr))));

		memcpy(&entry->eth, hwaddr, sizeof(struct net_eth_addr));
	}
}

static void arp_update(struct net_if *iface,
		       struct in_addr *src,
		       struct net_eth_addr *hwaddr,
		       bool gratuitous)
{
	struct arp_entry *entry;
	struct net_pkt *pkt;

	NET_DBG("src %s", log_strdup(net_sprint_ipv4_addr(src)));

	entry = arp_entry_get_pending(iface, src);
	if (!entry) {
		if (IS_ENABLED(CONFIG_NET_ARP_GRATUITOUS) && gratuitous) {
			arp_gratuitous(iface, src, hwaddr);
		}

		return;
	}

	/* Set the dst in the pending packet */
	net_pkt_lladdr_dst(entry->pending)->len = sizeof(struct net_eth_addr);
	net_pkt_lladdr_dst(entry->pending)->addr =
		(u8_t *) &NET_ETH_HDR(entry->pending)->dst.addr;

	NET_DBG("dst %s pending %p frag %p",
		log_strdup(net_sprint_ipv4_addr(&entry->ip)),
		entry->pending, entry->pending->frags);

	pkt = entry->pending;
	entry->pending = NULL;

	memcpy(&entry->eth, hwaddr, sizeof(struct net_eth_addr));

	/* Inserting entry into the table */
	sys_slist_prepend(&arp_table, &entry->node);

	net_if_queue_tx(iface, pkt);
}

static inline struct net_pkt *arp_prepare_reply(struct net_if *iface,
						struct net_pkt *req)
{
	struct ethernet_context *ctx = net_if_l2_data(iface);
	int eth_hdr_len = sizeof(struct net_eth_hdr);
	struct net_pkt *pkt;
	struct net_buf *frag;
	struct net_arp_hdr *hdr, *query;
	struct net_eth_hdr *eth_query;

	if (net_eth_is_vlan_enabled(ctx, iface) &&
	    net_eth_get_vlan_tag(iface) != NET_VLAN_TAG_UNSPEC) {
		eth_hdr_len = sizeof(struct net_eth_vlan_hdr);
	}

	pkt = net_pkt_get_reserve_tx(eth_hdr_len, NET_BUF_TIMEOUT);
	if (!pkt) {
		goto fail;
	}

	net_pkt_set_iface(pkt, iface);
	net_pkt_set_family(pkt, AF_UNSPEC);

	eth_query = NET_ETH_HDR(req);

	frag = net_pkt_get_frag(pkt, NET_BUF_TIMEOUT);
	if (!frag) {
		goto fail;
	}

	net_pkt_frag_add(pkt, frag);

	hdr = NET_ARP_HDR(pkt);
	query = NET_ARP_HDR(req);

	net_pkt_set_vlan_tag(pkt, net_pkt_vlan_tag(req));

	hdr->hwtype = htons(NET_ARP_HTYPE_ETH);
	hdr->protocol = htons(NET_ETH_PTYPE_IP);
	hdr->hwlen = sizeof(struct net_eth_addr);
	hdr->protolen = sizeof(struct in_addr);
	hdr->opcode = htons(NET_ARP_REPLY);

	memcpy(&hdr->dst_hwaddr.addr, &eth_query->src.addr,
	       sizeof(struct net_eth_addr));
	memcpy(&hdr->src_hwaddr.addr, net_if_get_link_addr(iface)->addr,
	       sizeof(struct net_eth_addr));

	net_ipaddr_copy(&hdr->dst_ipaddr, &query->src_ipaddr);
	net_ipaddr_copy(&hdr->src_ipaddr, &query->dst_ipaddr);

	net_pkt_lladdr_src(pkt)->addr = net_if_get_link_addr(iface)->addr;
	net_pkt_lladdr_src(pkt)->len = sizeof(struct net_eth_addr);

	net_pkt_lladdr_dst(pkt)->addr = (u8_t *)&hdr->dst_hwaddr.addr;
	net_pkt_lladdr_dst(pkt)->len = sizeof(struct net_eth_addr);

	net_buf_add(frag, sizeof(struct net_arp_hdr));

	return pkt;

fail:
	net_pkt_unref(pkt);
	return NULL;
}

static bool arp_hdr_check(struct net_arp_hdr *arp_hdr)
{
	if (ntohs(arp_hdr->hwtype) != NET_ARP_HTYPE_ETH ||
	    ntohs(arp_hdr->protocol) != NET_ETH_PTYPE_IP ||
	    arp_hdr->hwlen != sizeof(struct net_eth_addr) ||
	    arp_hdr->protolen != NET_ARP_IPV4_PTYPE_SIZE ||
	    net_ipv4_is_addr_loopback(&arp_hdr->src_ipaddr)) {
		NET_DBG("DROP: Invalid ARP header");
		return false;
	}

	return true;
}

enum net_verdict net_arp_input(struct net_pkt *pkt)
{
	struct net_eth_hdr *eth_hdr;
	struct net_arp_hdr *arp_hdr;
	struct net_pkt *reply;
	struct in_addr *addr;

	if (net_pkt_get_len(pkt) < (sizeof(struct net_arp_hdr) -
				    net_pkt_ll_reserve(pkt))) {
		NET_DBG("Invalid ARP header (len %zu, min %zu bytes)",
			net_pkt_get_len(pkt),
			sizeof(struct net_arp_hdr) -
			net_pkt_ll_reserve(pkt));
		return NET_DROP;
	}

	arp_hdr = NET_ARP_HDR(pkt);
	if (!arp_hdr_check(arp_hdr)) {
		return NET_DROP;
	}

	switch (ntohs(arp_hdr->opcode)) {
	case NET_ARP_REQUEST:
		eth_hdr = NET_ETH_HDR(pkt);

		if (IS_ENABLED(CONFIG_NET_ARP_GRATUITOUS)) {
			if (memcmp(&eth_hdr->dst,
				   net_eth_broadcast_addr(),
				   sizeof(struct net_eth_addr)) == 0 &&
			    memcmp(&arp_hdr->dst_hwaddr,
				   net_eth_broadcast_addr(),
				   sizeof(struct net_eth_addr)) == 0 &&
			    memcmp(&arp_hdr->dst_ipaddr, &arp_hdr->src_ipaddr,
				   sizeof(struct in_addr)) == 0) {
				/* If the IP address is in our cache,
				 * then update it here.
				 */
				arp_update(net_pkt_iface(pkt),
					   &arp_hdr->src_ipaddr,
					   &arp_hdr->src_hwaddr,
					   true);
				break;
			}
		}

		/* Discard ARP request if Ethernet address is broadcast
		 * and Source IP address is Multicast address.
		 */
		if (memcmp(&eth_hdr->dst, net_eth_broadcast_addr(),
			   sizeof(struct net_eth_addr)) == 0 &&
		    net_ipv4_is_addr_mcast(&arp_hdr->src_ipaddr)) {
			NET_DBG("DROP: eth addr is bcast, src addr is mcast");
			return NET_DROP;
		}

		/* Someone wants to know our ll address */
		addr = if_get_addr(net_pkt_iface(pkt), &arp_hdr->dst_ipaddr);
		if (!addr) {
			/* Not for us so drop the packet silently */
			return NET_DROP;
		}

		NET_DBG("ARP request from %s [%s] for %s",
			log_strdup(net_sprint_ipv4_addr(&arp_hdr->src_ipaddr)),
			log_strdup(net_sprint_ll_addr(
					   (u8_t *)&arp_hdr->src_hwaddr,
					   arp_hdr->hwlen)),
			log_strdup(net_sprint_ipv4_addr(
					   &arp_hdr->dst_ipaddr)));

		/* Send reply */
		reply = arp_prepare_reply(net_pkt_iface(pkt), pkt);
		if (reply) {
			net_if_queue_tx(net_pkt_iface(reply), reply);
		} else {
			NET_DBG("Cannot send ARP reply");
		}
		break;

	case NET_ARP_REPLY:
		if (net_ipv4_is_my_addr(&arp_hdr->dst_ipaddr)) {
			arp_update(net_pkt_iface(pkt),
				   &arp_hdr->src_ipaddr,
				   &arp_hdr->src_hwaddr,
				   false);
		}

		break;
	}

	net_pkt_unref(pkt);

	return NET_OK;
}

void net_arp_clear_cache(struct net_if *iface)
{
	sys_snode_t *prev = NULL;
	struct arp_entry *entry, *next;

	NET_DBG("Flushing ARP table");

	SYS_SLIST_FOR_EACH_CONTAINER_SAFE(&arp_table, entry, next, node) {
		if (iface && iface != entry->iface) {
			prev = &entry->node;
			continue;
		}

		arp_entry_cleanup(entry, false);

		sys_slist_remove(&arp_table, prev, &entry->node);
		sys_slist_prepend(&arp_free_entries, &entry->node);
	}

	prev = NULL;

	NET_DBG("Flushing ARP pending requests");

	SYS_SLIST_FOR_EACH_CONTAINER_SAFE(&arp_pending_entries,
					  entry, next, node) {
		if (iface && iface != entry->iface) {
			prev = &entry->node;
			continue;
		}

		arp_entry_cleanup(entry, true);

		sys_slist_remove(&arp_pending_entries, prev, &entry->node);
		sys_slist_prepend(&arp_free_entries, &entry->node);
	}

	if (sys_slist_is_empty(&arp_pending_entries)) {
		k_delayed_work_cancel(&arp_request_timer);
	}
}

int net_arp_foreach(net_arp_cb_t cb, void *user_data)
{
	int ret = 0;
	struct arp_entry *entry;

	SYS_SLIST_FOR_EACH_CONTAINER(&arp_table, entry, node) {
		ret++;
		cb(entry, user_data);
	}

	return ret;
}

void net_arp_init(void)
{
	int i;

	if (arp_cache_initialized) {
		return;
	}

	sys_slist_init(&arp_free_entries);
	sys_slist_init(&arp_pending_entries);
	sys_slist_init(&arp_table);

	for (i = 0; i < CONFIG_NET_ARP_TABLE_SIZE; i++) {
		/* Inserting entry as free */
		sys_slist_prepend(&arp_free_entries, &arp_entries[i].node);
	}

	k_delayed_work_init(&arp_request_timer, arp_request_timeout);

	arp_cache_initialized = true;
}
