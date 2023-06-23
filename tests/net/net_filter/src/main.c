/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/types.h>
#include <zephyr/ztest.h>
#include <zephyr/net/net_filter.h>

#define CALLBACK_ID_FOO (1)
#define CALLBACK_ID_NAT_PREROUTING (2)
#define CALLBACK_ID_FILTER (3)
#define CALLBACK_ID_SEC (4)

enum nf_ip_hook_priorities {
	NF_IP_PRI_FIRST         = INT_MIN,
	NF_IP_PRI_NAT_DST       = -100,
	NF_IP_PRI_FILTER        = 0,
	NF_IP_PRI_SECURITY      = 50,
	NF_IP_PRI_NAT_SRC       = 100,
	NF_IP_PRI_LAST          = INT_MAX,
};

static enum net_verdict nf_ip_filter(struct net_pkt *pkt);
static enum net_verdict nf_ip_nat_prerouting(struct net_pkt *pkt);
static enum net_verdict nf_ip_sec(struct net_pkt *pkt);
static enum net_verdict foo(struct net_pkt *pkt);

/**
 * Objects
 */
const struct nf_hook_cfg ipv_hook_table[] = {
	{
		.hook_fn = nf_ip_filter,
		.hooknum = NF_IP_PRE_ROUTING,
		.pf = PF_INET,
		.priority = NF_IP_PRI_FILTER,
	},
	{
		.hook_fn = nf_ip_nat_prerouting,
		.hooknum = NF_IP_PRE_ROUTING,
		.pf = PF_INET,
		.priority = NF_IP_PRI_NAT_DST,
	},
	{
		.hook_fn = nf_ip_sec,
		.hooknum = NF_IP_PRE_ROUTING,
		.pf = PF_INET,
		.priority = NF_IP_PRI_SECURITY,
	},
	{
		.hook_fn = foo,
		.hooknum = NF_IP_PRE_ROUTING,
		.pf = PF_INET,
		.priority = NF_IP_PRI_FIRST,
	},
};

static int hooks_call_order_zero[ARRAY_SIZE(ipv_hook_table)] = { 0, };
static int hooks_call_order[ARRAY_SIZE(ipv_hook_table)];
static int hooks_call_cnt = 0;
/* Expected order */
static int expected_hooks_call_order[ARRAY_SIZE(ipv_hook_table)] = {
	CALLBACK_ID_FOO, CALLBACK_ID_NAT_PREROUTING, CALLBACK_ID_FILTER, CALLBACK_ID_SEC
};

static void register_cb_call(int func_id)
{
	zassert_true(hooks_call_cnt < ARRAY_SIZE(ipv_hook_table));
	hooks_call_order[hooks_call_cnt++] = func_id;
}

static void reset_cb_calls()
{
	hooks_call_cnt = 0;
	memset(hooks_call_order, 0, ARRAY_SIZE(ipv_hook_table) * sizeof(int));
}

/**
 * Callbacks
 */
static enum net_verdict nf_ip_filter(struct net_pkt *pkt)
{
	register_cb_call(CALLBACK_ID_FILTER);
	return NET_CONTINUE;
}

static enum net_verdict nf_ip_nat_prerouting(struct net_pkt *pkt)
{
	register_cb_call(CALLBACK_ID_NAT_PREROUTING);
	return NET_CONTINUE;
}

static enum net_verdict nf_ip_sec(struct net_pkt *pkt)
{
	register_cb_call(CALLBACK_ID_SEC);
	return NET_CONTINUE;
}

static enum net_verdict foo(struct net_pkt *pkt)
{
	register_cb_call(CALLBACK_ID_FOO);
	return NET_CONTINUE;
}

static void fire_all_hooks(uint8_t pf, struct net_pkt *pkt){
	zassert_equal(nf_prerouting_hook(pf, pkt), NET_CONTINUE);
	zassert_equal(nf_postrouting_hook(pf, pkt), NET_CONTINUE);
	zassert_equal(nf_input_hook(pf, pkt), NET_CONTINUE);
	zassert_equal(nf_output_hook(pf, pkt), NET_CONTINUE);
	zassert_equal(nf_forward_hook(pf, pkt), NET_CONTINUE);
}

static void before_test(void *unused)
{
	ARG_UNUSED(unused);
	reset_cb_calls();
}

ZTEST(net_filter_test_suite, test_hooks_register_unregister){
	struct net_pkt *pkt = net_pkt_rx_alloc_with_buffer(
		NULL, 32, AF_UNSPEC, 0, K_NO_WAIT);

	zassert_mem_equal(hooks_call_order, hooks_call_order_zero, sizeof(hooks_call_order));
	/* Nothing registered yet. */
	fire_all_hooks(PF_INET, pkt);
	fire_all_hooks(PF_INET6, pkt);
	zassert_mem_equal(hooks_call_order, hooks_call_order_zero, sizeof(hooks_call_order));

	/* Register hooks */
	zassert_equal(nf_register_net_hooks(ipv_hook_table, ARRAY_SIZE(ipv_hook_table)), 0);

	fire_all_hooks(PF_INET6, pkt);
	zassert_mem_equal(hooks_call_order, hooks_call_order_zero, sizeof(hooks_call_order));
	fire_all_hooks(PF_INET, pkt);
	zassert_equal(hooks_call_cnt, ARRAY_SIZE(ipv_hook_table));
	zassert_mem_equal(expected_hooks_call_order, hooks_call_order, sizeof(hooks_call_order));

	reset_cb_calls();

	zassert_mem_equal(hooks_call_order, hooks_call_order_zero, sizeof(hooks_call_order));
	nf_unregister_net_hooks(ipv_hook_table, ARRAY_SIZE(ipv_hook_table));
	fire_all_hooks(PF_INET, pkt);
	fire_all_hooks(PF_INET6, pkt);
	zassert_mem_equal(hooks_call_order, hooks_call_order_zero, sizeof(hooks_call_order));
}

ZTEST_SUITE(net_filter_test_suite, NULL, NULL, before_test, NULL, NULL);
