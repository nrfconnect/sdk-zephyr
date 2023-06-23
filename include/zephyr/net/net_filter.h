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

/** @brief Hook IDs for IP. */
typedef enum {
	NF_IP_PRE_ROUTING = 0,
	NF_IP_LOCAL_IN,
	NF_IP_FORWARD,
	NF_IP_LOCAL_OUT,
	NF_IP_POST_ROUTING,
	NF_IP_NUMHOOKS
} nf_ip_hooks;

/** @brief Hook configuration structure with metadata.
 *
 * Several callback functions can be registered with the same hook.
 * The priority value determines the order of callbacks function and
 * their calling order. The callback function for which priority has
 * been set to lower value (e.g. -100) will be called before callback
 * function with higher priority value (e.g. 0).
 */
struct nf_hook_cfg {
	nf_hook_fn_t *hook_fn;  /**< callback function */
	unsigned int hooknum;   /**< hook ID number */
	uint8_t pf;             /**< protocol family */
	int priority;           /**< callback priority in hook list */
};

/**
 * @brief Register new callback function to a hook.
 *
 * @param cfg pointer with hook configuration.
 * @return 0 when success or error code otherwise.
 */
int nf_register_net_hook(const struct nf_hook_cfg *cfg);

/**
 * @brief Register multiple callback functions to hook or multiple hooks.
 *
 * @param cfg pointer to table with hooks configuration.
 * @param num number of hooks to register.
 * @return 0 when success or error code otherwise.
 */
int nf_register_net_hooks(const struct nf_hook_cfg *cfg, unsigned int count);

/**
 * @brief Unregister callback function from a hook.
 *
 * @param cfg pointer to hook configuration.
 * @return 0 when success or error code otherwise.
 */
int nf_unregister_net_hook(const struct nf_hook_cfg *cfg);

/**
 * @brief Unregister multiple callback functions from a hook(s).
 *
 * @param cfg pointer to table with hooks configuration.
 * @param num number of hooks to register.
 * @return 0 when success or error code otherwise.
 */
void nf_unregister_net_hooks(const struct nf_hook_cfg *cfg, unsigned int count);

/**
 * @brief Network filter hook.
 *
 * @param pf protocol family.
 * @param hooknum hook ID number.
 * @param pkt network packet.
 * @return verdict about the packet.
 */
enum net_verdict nf_hook(uint8_t pf, unsigned int hooknum, struct net_pkt *pkt);

static inline enum net_verdict nf_prerouting_hook(uint8_t pf, struct net_pkt *pkt)
{
	return nf_hook(pf, NF_IP_PRE_ROUTING, pkt);
}

static inline enum net_verdict nf_postrouting_hook(uint8_t pf, struct net_pkt *pkt)
{
	return nf_hook(pf, NF_IP_POST_ROUTING, pkt);
}

static inline enum net_verdict nf_input_hook(uint8_t pf, struct net_pkt *pkt)
{
	return nf_hook(pf, NF_IP_LOCAL_IN, pkt);
}

static inline enum net_verdict nf_output_hook(uint8_t pf, struct net_pkt *pkt)
{
	return nf_hook(pf, NF_IP_LOCAL_OUT, pkt);
}

static inline enum net_verdict nf_forward_hook(uint8_t pf, struct net_pkt *pkt)
{
	return nf_hook(pf, NF_IP_FORWARD, pkt);
}

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* ZEPHYR_INCLUDE_NET_PKT_FILTER_H_ */
