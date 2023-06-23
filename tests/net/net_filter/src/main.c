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
struct nf_hook_entry ipv_hook_table[] = {
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
static volatile int hooks_call_cnt = 0;
static int drop_pkt_by_call_cnt = 0;
/* Expected order */
static int expected_hooks_call_order[ARRAY_SIZE(ipv_hook_table)] = {
	CALLBACK_ID_FOO, CALLBACK_ID_NAT_PREROUTING, CALLBACK_ID_FILTER, CALLBACK_ID_SEC
};

static void register_cb_exec(int func_id)
{
	zassert_true(hooks_call_cnt < ARRAY_SIZE(ipv_hook_table));
	hooks_call_order[hooks_call_cnt++] = func_id;
}

static void reset_cb_calls()
{
	hooks_call_cnt = 0;
	memset(hooks_call_order, 0, sizeof(hooks_call_order));
}

static volatile bool drop_pkt_now = false;

/**
 * Hook functions - callbacks.
 */
static enum net_verdict nf_ip_filter(struct net_pkt *pkt)
{
	/* Register callback execution. */
	register_cb_exec(CALLBACK_ID_FILTER);
	/* The simplest way to control return value. */
	if (drop_pkt_by_call_cnt && (drop_pkt_by_call_cnt == hooks_call_cnt)) {
		return NET_DROP;
	}
	return NET_CONTINUE;
}

static enum net_verdict nf_ip_nat_prerouting(struct net_pkt *pkt)
{
	register_cb_exec(CALLBACK_ID_NAT_PREROUTING);
	if (drop_pkt_by_call_cnt && (drop_pkt_by_call_cnt == hooks_call_cnt)) {
		return NET_DROP;
	}
	return NET_CONTINUE;
}

static enum net_verdict nf_ip_sec(struct net_pkt *pkt)
{
	register_cb_exec(CALLBACK_ID_SEC);
	if (drop_pkt_by_call_cnt && (drop_pkt_by_call_cnt == hooks_call_cnt)) {
		return NET_DROP;
	}
	return NET_CONTINUE;
}

static enum net_verdict foo(struct net_pkt *pkt)
{
	register_cb_exec(CALLBACK_ID_FOO);
	if (drop_pkt_by_call_cnt && (drop_pkt_by_call_cnt == hooks_call_cnt)) {
		return NET_DROP;
	}
	return NET_CONTINUE;
}

static void fire_all_hooks(uint8_t pf, struct net_pkt *pkt)
{
	zassert_equal(nf_prerouting_hook(pf, pkt), NET_CONTINUE);
	zassert_equal(nf_postrouting_hook(pkt), NET_CONTINUE);
	zassert_equal(nf_input_hook(pf, pkt), NET_CONTINUE);
}

static void drop_by_callback_id(int callback_ID)
{
	int test_hooks_call_order[ARRAY_SIZE(ipv_hook_table)];
	struct net_pkt *pkt = net_pkt_alloc_with_buffer(NULL, 32,
							AF_UNSPEC, 0, K_NO_WAIT);

	zassert_true(pkt != NULL, "Packet allocation fail");

	/* Prepare test */
	drop_pkt_by_call_cnt = callback_ID;
	reset_cb_calls();
	zassert_equal(hooks_call_cnt, 0);
	zassert_mem_equal(hooks_call_order, hooks_call_order_zero, sizeof(hooks_call_order));
	memset(test_hooks_call_order, 0, sizeof(test_hooks_call_order));
	memcpy(test_hooks_call_order, expected_hooks_call_order, callback_ID * sizeof(test_hooks_call_order[0]));

	/* hook_fn's are registered only for prerouting. */
	zassert_equal(nf_prerouting_hook(PF_INET, pkt), NET_DROP);
	/* Check also another hooks */
	zassert_equal(nf_postrouting_hook(pkt), NET_CONTINUE);
	zassert_equal(nf_input_hook(PF_INET, pkt), NET_CONTINUE);
	fire_all_hooks(PF_INET6, pkt);

	zassert_equal(hooks_call_cnt, callback_ID);
	zassert_mem_equal(test_hooks_call_order, hooks_call_order, sizeof(hooks_call_order));

	net_pkt_unref(pkt);
}

static void before_test(void *unused)
{
	ARG_UNUSED(unused);
	reset_cb_calls();
	drop_pkt_by_call_cnt = 0;
}

static void after_test(void *unused)
{
	ARG_UNUSED(unused);
	reset_cb_calls();
	drop_pkt_by_call_cnt = 0;
	nf_unregister_net_hooks(ipv_hook_table, ARRAY_SIZE(ipv_hook_table));
}

ZTEST(net_filter_test_suite, test_hooks_register_unregister){
	struct net_pkt *pkt = net_pkt_alloc_with_buffer(NULL, 32,
							AF_UNSPEC, 0, K_NO_WAIT);

	zassert_true(pkt != NULL, "Packet allocation fail");

	/* No hook_fn registered yet thus nothing should have been captured. */
	zassert_mem_equal(hooks_call_order, hooks_call_order_zero, sizeof(hooks_call_order));
	fire_all_hooks(PF_INET, pkt);
	fire_all_hooks(PF_INET6, pkt);
	zassert_equal(hooks_call_cnt, 0);
	zassert_mem_equal(hooks_call_order, hooks_call_order_zero, sizeof(hooks_call_order));

	/* Register all hook_fn's from the ipv_hook_table array. */
	zassert_equal(nf_register_net_hooks(ipv_hook_table, ARRAY_SIZE(ipv_hook_table)), 0);

	fire_all_hooks(PF_INET6, pkt);
	zassert_equal(hooks_call_cnt, 0);
	zassert_mem_equal(hooks_call_order, hooks_call_order_zero, sizeof(hooks_call_order));

	fire_all_hooks(PF_INET, pkt);
	zassert_equal(hooks_call_cnt, ARRAY_SIZE(ipv_hook_table));
	zassert_mem_equal(expected_hooks_call_order, hooks_call_order, sizeof(hooks_call_order));

	reset_cb_calls();
	zassert_equal(hooks_call_cnt, 0);
	zassert_mem_equal(hooks_call_order, hooks_call_order_zero, sizeof(hooks_call_order));

	nf_unregister_net_hooks(ipv_hook_table, ARRAY_SIZE(ipv_hook_table));

	fire_all_hooks(PF_INET, pkt);
	fire_all_hooks(PF_INET6, pkt);
	zassert_equal(hooks_call_cnt, 0);
	zassert_mem_equal(hooks_call_order, hooks_call_order_zero, sizeof(hooks_call_order));

	net_pkt_unref(pkt);
}

ZTEST(net_filter_test_suite, test_drop_pkt){
	zassert_equal(nf_register_net_hooks(ipv_hook_table, ARRAY_SIZE(ipv_hook_table)), 0);

	for (int it = 0; it < ARRAY_SIZE(ipv_hook_table); it++)
		drop_by_callback_id(it + 1);
}

ZTEST_SUITE(net_filter_test_suite, NULL, NULL, before_test, after_test, NULL);
