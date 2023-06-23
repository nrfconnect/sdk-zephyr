/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/** @file
 * @brief Public API for network filter
 *
 * The simple network filter allows to network-related operations
 * which can be implemented in the form of hooks - custom callbacks.
 */

#ifndef ZEPHYR_INCLUDE_NET_FILTER_H_
#define ZEPHYR_INCLUDE_NET_FILTER_H_

#include <zephyr/net/net_core.h>
#include <zephyr/net/net_ip.h>
#include <zephyr/net/net_pkt.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Network Filter API
 * @defgroup net_filter Network Filter API
 * @ingroup networking
 * @{
 */

/** @brief hook callback type. */
typedef enum net_verdict (nf_hook_fn_t)(struct net_pkt *pkt);

/** @brief Hook type categories for IP. */
enum nf_ip_hooks {
	NF_IP_PRE_ROUTING = 0,
	NF_IP_LOCAL_IN,
	NF_IP_FORWARD,
	NF_IP_LOCAL_OUT,
	NF_IP_POST_ROUTING,
	NF_IP_NUMHOOKS
};

/** @brief Hook entry structure.
 * The structure contains metadata for hook point (node).
 *
 * Several hook functions (callbacks) can be registered with the same hook.
 * The priority value determines the order of hook function and
 * their calling order. The hook function for which priority has
 * been set to lower value (e.g. -100) will be called before hook
 * function with higher priority value (e.g. 0).
 */
struct nf_hook_entry {
	sys_snode_t node;
	nf_hook_fn_t *hook_fn;  /**< hook function */
	unsigned int hooknum;   /**< hook type category */
	uint8_t pf;             /**< protocol family */
	int priority;           /**< callback priority in hook list */
	struct net_if *iface;   /**< network interface */
};

/**
 * @brief Register a new hook function.
 *
 * @param hook pointer to static member of hook point to register.
 * @return 0 when success or error code otherwise.
 */
int nf_register_net_hook(struct nf_hook_entry *hook);

/**
 * @brief Register multiple hook functions to hook or multiple hooks.
 *
 * @param hook pointer to static table with hook points to register.
 * @param count number of hooks to register.
 * @return 0 when success or error code otherwise.
 */
int nf_register_net_hooks(struct nf_hook_entry *hook, unsigned int count);

/**
 * @brief Unregister hook function from a hook.
 *
 * @param hook pointer to static member of hook point to unregister.
 * @return 0 when success or error code otherwise.
 */
int nf_unregister_net_hook(struct nf_hook_entry *hook);

/**
 * @brief Unregister multiple hook functions from a hook(s).
 *
 * @param hook pointer to static table with hooks to unregister.
 * @param count number of hooks to register.
 */
void nf_unregister_net_hooks(struct nf_hook_entry *hook, unsigned int count);

/**
 * @brief Network filter hook.
 *
 * @param pf protocol family.
 * @param hooknum hook ID number.
 * @param pkt network packet.
 * @return verdict about the packet.
 */
enum net_verdict nf_hook(uint8_t pf, unsigned int hooknum, struct net_pkt *pkt);

/**
 * @brief Prerouting hook.
 *
 * @param pf protocol family.
 * @param pkt network packet.
 * @return verdict about the packet.
 */
#if defined(CONFIG_NET_FILTER)
static inline enum net_verdict nf_prerouting_hook(uint8_t pf, struct net_pkt *pkt)
{
	return nf_hook(pf, NF_IP_PRE_ROUTING, pkt);
}
#else
static inline enum net_verdict nf_prerouting_hook(uint8_t pf, struct net_pkt *pkt)
{
	ARG_UNUSED(pkt);
	ARG_UNUSED(pf);

	return NET_CONTINUE;
}
#endif

/**
 * @brief Postrouting hook.
 *
 * @param pkt network packet.
 * @return verdict about the packet.
 */
#if defined(CONFIG_NET_FILTER)
static inline enum net_verdict nf_postrouting_hook(struct net_pkt *pkt)
{
	__ASSERT_NO_MSG(pkt != NULL);
	return nf_hook(net_pkt_family(pkt), NF_IP_POST_ROUTING, pkt);
}
#else
static inline enum net_verdict nf_postrouting_hook(struct net_pkt *pkt)
{
	ARG_UNUSED(pkt);

	return NET_CONTINUE;
}
#endif

/**
 * @brief Local input hook.
 *
 * @param pf protocol family.
 * @param pkt network packet.
 * @return verdict about the packet.
 */
#if defined(CONFIG_NET_FILTER)
static inline enum net_verdict nf_input_hook(uint8_t pf, struct net_pkt *pkt)
{
	return nf_hook(pf, NF_IP_LOCAL_IN, pkt);
}
#else
static inline enum net_verdict nf_input_hook(uint8_t pf, struct net_pkt *pkt)
{
	ARG_UNUSED(pkt);
	ARG_UNUSED(pf);

	return NET_CONTINUE;
}
#endif

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* ZEPHYR_INCLUDE_NET_PKT_FILTER_H_ */
