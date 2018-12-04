/* main.c - Application main entry point */

/*
 * Copyright (c) 2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_MODULE_NAME net_test
#define NET_LOG_LEVEL CONFIG_NET_IF_LOG_LEVEL

#include <zephyr/types.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>
#include <misc/printk.h>
#include <linker/sections.h>

#include <ztest.h>

#include <net/ethernet.h>
#include <net/dummy.h>
#include <net/buf.h>
#include <net/net_ip.h>
#include <net/net_if.h>

#define NET_LOG_ENABLED 1
#include "net_private.h"

#if defined(CONFIG_NET_IF_LOG_LEVEL_DBG)
#define DBG(fmt, ...) printk(fmt, ##__VA_ARGS__)
#else
#define DBG(fmt, ...)
#endif

/* Interface 1 addresses */
static struct in6_addr my_addr1 = { { { 0x20, 0x01, 0x0d, 0xb8, 1, 0, 0, 0,
					0, 0, 0, 0, 0, 0, 0, 0x1 } } };
static struct in_addr my_ipv4_addr1 = { { { 192, 0, 2, 1 } } };

/* Interface 2 addresses */
static struct in6_addr my_addr2 = { { { 0x20, 0x01, 0x0d, 0xb8, 2, 0, 0, 0,
					0, 0, 0, 0, 0, 0, 0, 0x1 } } };

/* Interface 3 addresses */
static struct in6_addr my_addr3 = { { { 0x20, 0x01, 0x0d, 0xb8, 3, 0, 0, 0,
					0, 0, 0, 0, 0, 0, 0, 0x1 } } };

/* Extra address is assigned to ll_addr */
static struct in6_addr ll_addr = { { { 0xfe, 0x80, 0x43, 0xb8, 0, 0, 0, 0,
				       0, 0, 0, 0xf2, 0xaa, 0x29, 0x02,
				       0x04 } } };

static struct in6_addr in6addr_mcast = { { { 0x20, 0x01, 0x0d, 0xb8, 0, 0, 0, 0,
					     0, 0, 0, 0, 0, 0, 0, 0x1 } } };

static struct net_if *iface1;
static struct net_if *iface2;
static struct net_if *iface3;
static struct net_if *iface4;

static bool test_failed;
static bool test_started;
static struct k_sem wait_data;

#define WAIT_TIME 250

struct net_if_test {
	u8_t idx;
	u8_t mac_addr[sizeof(struct net_eth_addr)];
	struct net_linkaddr ll_addr;
};

static int net_iface_dev_init(struct device *dev)
{
	return 0;
}

static u8_t *net_iface_get_mac(struct device *dev)
{
	struct net_if_test *data = dev->driver_data;

	if (data->mac_addr[2] == 0x00) {
		/* 00-00-5E-00-53-xx Documentation RFC 7042 */
		data->mac_addr[0] = 0x00;
		data->mac_addr[1] = 0x00;
		data->mac_addr[2] = 0x5E;
		data->mac_addr[3] = 0x00;
		data->mac_addr[4] = 0x53;
		data->mac_addr[5] = sys_rand32_get();
	}

	data->ll_addr.addr = data->mac_addr;
	data->ll_addr.len = 6;

	return data->mac_addr;
}

static void net_iface_init(struct net_if *iface)
{
	u8_t *mac = net_iface_get_mac(net_if_get_device(iface));

	net_if_set_link_addr(iface, mac, sizeof(struct net_eth_addr),
			     NET_LINK_ETHERNET);
}

static int sender_iface(struct device *dev, struct net_pkt *pkt)
{
	if (!pkt->frags) {
		DBG("No data to send!\n");
		return -ENODATA;
	}

	if (test_started) {
		struct net_if_test *data = dev->driver_data;

		DBG("Sending at iface %d %p\n",
		    net_if_get_by_iface(net_pkt_iface(pkt)),
		    net_pkt_iface(pkt));

		if (net_if_get_by_iface(net_pkt_iface(pkt)) != data->idx) {
			DBG("Invalid interface %d index, expecting %d\n",
			    data->idx, net_if_get_by_iface(net_pkt_iface(pkt)));
			test_failed = true;
		}
	}

	k_sem_give(&wait_data);

	return 0;
}

struct net_if_test net_iface1_data;
struct net_if_test net_iface2_data;
struct net_if_test net_iface3_data;

static struct dummy_api net_iface_api = {
	.iface_api.init = net_iface_init,
	.send = sender_iface,
};

#define _ETH_L2_LAYER DUMMY_L2
#define _ETH_L2_CTX_TYPE NET_L2_GET_CTX_TYPE(DUMMY_L2)

NET_DEVICE_INIT_INSTANCE(net_iface1_test,
			 "iface1",
			 iface1,
			 net_iface_dev_init,
			 &net_iface1_data,
			 NULL,
			 CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,
			 &net_iface_api,
			 _ETH_L2_LAYER,
			 _ETH_L2_CTX_TYPE,
			 127);

NET_DEVICE_INIT_INSTANCE(net_iface2_test,
			 "iface2",
			 iface2,
			 net_iface_dev_init,
			 &net_iface2_data,
			 NULL,
			 CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,
			 &net_iface_api,
			 _ETH_L2_LAYER,
			 _ETH_L2_CTX_TYPE,
			 127);

NET_DEVICE_INIT_INSTANCE(net_iface3_test,
			 "iface3",
			 iface3,
			 net_iface_dev_init,
			 &net_iface3_data,
			 NULL,
			 CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,
			 &net_iface_api,
			 _ETH_L2_LAYER,
			 _ETH_L2_CTX_TYPE,
			 127);

struct eth_fake_context {
	struct net_if *iface;
	u8_t mac_address[6];
	bool promisc_mode;
};

static struct eth_fake_context eth_fake_data;

static void eth_fake_iface_init(struct net_if *iface)
{
	struct device *dev = net_if_get_device(iface);
	struct eth_fake_context *ctx = dev->driver_data;

	ctx->iface = iface;

	net_if_set_link_addr(iface, ctx->mac_address,
			     sizeof(ctx->mac_address),
			     NET_LINK_ETHERNET);

	ethernet_init(iface);
}

static int eth_fake_send(struct device *dev,
			 struct net_pkt *pkt)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(pkt);

	return 0;
}

static enum ethernet_hw_caps eth_fake_get_capabilities(struct device *dev)
{
	return ETHERNET_PROMISC_MODE;
}

static int eth_fake_set_config(struct device *dev,
			       enum ethernet_config_type type,
			       const struct ethernet_config *config)
{
	struct eth_fake_context *ctx = dev->driver_data;

	switch (type) {
	case ETHERNET_CONFIG_TYPE_PROMISC_MODE:
		if (config->promisc_mode == ctx->promisc_mode) {
			return -EALREADY;
		}

		ctx->promisc_mode = config->promisc_mode;

		break;

	default:
		return -EINVAL;
	}

	return 0;
}

static struct ethernet_api eth_fake_api_funcs = {
	.iface_api.init = eth_fake_iface_init,

	.get_capabilities = eth_fake_get_capabilities,
	.set_config = eth_fake_set_config,
	.send = eth_fake_send,
};

static int eth_fake_init(struct device *dev)
{
	struct eth_fake_context *ctx = dev->driver_data;

	ctx->promisc_mode = false;

	return 0;
}

ETH_NET_DEVICE_INIT(eth_fake, "eth_fake", eth_fake_init, &eth_fake_data,
		    NULL, CONFIG_ETH_INIT_PRIORITY, &eth_fake_api_funcs, 1500);

#if NET_LOG_LEVEL >= LOG_LEVEL_DBG
static const char *iface2str(struct net_if *iface)
{
	if (net_if_l2(iface) == &NET_L2_GET_NAME(ETHERNET)) {
		return "Ethernet";
	}

	if (net_if_l2(iface) == &NET_L2_GET_NAME(DUMMY)) {
		return "Dummy";
	}

	return "<unknown type>";
}
#endif

static void iface_cb(struct net_if *iface, void *user_data)
{
	static int if_count;

	DBG("Interface %p (%s) [%d]\n", iface, iface2str(iface),
	    net_if_get_by_iface(iface));

	if (net_if_l2(iface) == &NET_L2_GET_NAME(ETHERNET)) {
		const struct ethernet_api *api =
			net_if_get_device(iface)->driver_api;

		/* As native_posix board will introduce another ethernet
		 * interface, make sure that we only use our own in this test.
		 */
		if (api->get_capabilities ==
		    eth_fake_api_funcs.get_capabilities) {
			iface4 = iface;
		}
	} else {
		switch (if_count) {
		case 0:
			iface1 = iface;
			break;
		case 1:
			iface2 = iface;
			break;
		case 2:
			iface3 = iface;
			break;
		}

		if_count++;
	}
}

static void iface_setup(void)
{
	struct net_if_mcast_addr *maddr;
	struct net_if_addr *ifaddr;
	int idx;

	/* The semaphore is there to wait the data to be received. */
	k_sem_init(&wait_data, 0, UINT_MAX);

	net_if_foreach(iface_cb, NULL);

	idx = net_if_get_by_iface(iface1);
	((struct net_if_test *)
	 net_if_get_device(iface1)->driver_data)->idx = idx;

	idx = net_if_get_by_iface(iface2);
	((struct net_if_test *)
	 net_if_get_device(iface2)->driver_data)->idx = idx;

	idx = net_if_get_by_iface(iface3);
	((struct net_if_test *)
	 net_if_get_device(iface3)->driver_data)->idx = idx;

	DBG("Interfaces: [%d] iface1 %p, [%d] iface2 %p, [%d] iface3 %p\n",
	    net_if_get_by_iface(iface1), iface1,
	    net_if_get_by_iface(iface2), iface2,
	    net_if_get_by_iface(iface3), iface3);

	zassert_not_null(iface1, "Interface 1");
	zassert_not_null(iface2, "Interface 2");
	zassert_not_null(iface3, "Interface 3");

	ifaddr = net_if_ipv6_addr_add(iface1, &my_addr1,
				      NET_ADDR_MANUAL, 0);
	if (!ifaddr) {
		DBG("Cannot add IPv6 address %s\n",
		       net_sprint_ipv6_addr(&my_addr1));
		zassert_not_null(ifaddr, "addr1");
	}

	ifaddr = net_if_ipv4_addr_add(iface1, &my_ipv4_addr1,
				      NET_ADDR_MANUAL, 0);
	if (!ifaddr) {
		DBG("Cannot add IPv4 address %s\n",
		       net_sprint_ipv4_addr(&my_ipv4_addr1));
		zassert_not_null(ifaddr, "ipv4 addr1");
	}

	/* For testing purposes we need to set the adddresses preferred */
	ifaddr->addr_state = NET_ADDR_PREFERRED;

	ifaddr = net_if_ipv6_addr_add(iface1, &ll_addr,
				      NET_ADDR_MANUAL, 0);
	if (!ifaddr) {
		DBG("Cannot add IPv6 address %s\n",
		       net_sprint_ipv6_addr(&ll_addr));
		zassert_not_null(ifaddr, "ll_addr");
	}

	ifaddr->addr_state = NET_ADDR_PREFERRED;

	ifaddr = net_if_ipv6_addr_add(iface2, &my_addr2,
				      NET_ADDR_MANUAL, 0);
	if (!ifaddr) {
		DBG("Cannot add IPv6 address %s\n",
		       net_sprint_ipv6_addr(&my_addr2));
		zassert_not_null(ifaddr, "addr2");
	}

	ifaddr->addr_state = NET_ADDR_PREFERRED;

	ifaddr = net_if_ipv6_addr_add(iface2, &my_addr3,
				      NET_ADDR_MANUAL, 0);
	if (!ifaddr) {
		DBG("Cannot add IPv6 address %s\n",
		       net_sprint_ipv6_addr(&my_addr3));
		zassert_not_null(ifaddr, "addr3");
	}

	ifaddr->addr_state = NET_ADDR_PREFERRED;

	net_ipv6_addr_create(&in6addr_mcast, 0xff02, 0, 0, 0, 0, 0, 0, 0x0001);

	maddr = net_if_ipv6_maddr_add(iface1, &in6addr_mcast);
	if (!maddr) {
		DBG("Cannot add multicast IPv6 address %s\n",
		       net_sprint_ipv6_addr(&in6addr_mcast));
		zassert_not_null(maddr, "mcast");
	}

	net_if_up(iface1);
	net_if_up(iface2);
	net_if_up(iface3);
	net_if_up(iface4);

	/* The interface might receive data which might fail the checks
	 * in the iface sending function, so we need to reset the failure
	 * flag.
	 */
	test_failed = false;

	test_started = true;
}

static bool send_iface(struct net_if *iface, int val, bool expect_fail)
{
	static u8_t data[] = { 't', 'e', 's', 't', '\0' };
	struct net_pkt *pkt;
	int ret;

	pkt = net_pkt_get_reserve_tx(0, K_FOREVER);
	net_pkt_set_iface(pkt, iface);

	net_pkt_append_all(pkt, sizeof(data), data, K_FOREVER);

	ret = net_send_data(pkt);
	if (!expect_fail && ret < 0) {
		DBG("Cannot send test packet (%d)\n", ret);
		return false;
	}

	if (!expect_fail && k_sem_take(&wait_data, WAIT_TIME)) {
		DBG("Timeout while waiting interface %d data\n", val);
		return false;
	}

	return true;
}

static void send_iface1(void)
{
	bool ret;

	DBG("Sending data to iface 1 %p\n", iface1);

	ret = send_iface(iface1, 1, false);

	zassert_true(ret, "iface 1");
}

static void send_iface2(void)
{
	bool ret;

	DBG("Sending data to iface 2 %p\n", iface2);

	ret = send_iface(iface2, 2, false);

	zassert_true(ret, "iface 2");
}

static void send_iface3(void)
{
	bool ret;

	DBG("Sending data to iface 3 %p\n", iface3);

	ret = send_iface(iface3, 3, false);

	zassert_true(ret, "iface 3");
}

static void send_iface1_down(void)
{
	bool ret;

	DBG("Sending data to iface 1 %p while down\n", iface1);

	net_if_down(iface1);

	ret = send_iface(iface1, 1, true);

	zassert_true(ret, "iface 1 down");
}

static void send_iface1_up(void)
{
	bool ret;

	DBG("Sending data to iface 1 %p again\n", iface1);

	net_if_up(iface1);

	ret = send_iface(iface1, 1, false);

	zassert_true(ret, "iface 1 up again");
}

static void select_src_iface(void)
{
	struct in6_addr dst_addr1 = { { { 0x20, 0x01, 0x0d, 0xb8, 1, 0, 0, 0,
					  0, 0, 0, 0, 0, 0, 0, 0x2 } } };
	struct in6_addr ll_addr1 = { { { 0xfe, 0x80, 0x43, 0xb8, 0, 0, 0, 0,
					 0, 0, 0x09, 0x12, 0xaa, 0x29, 0x02,
					 0x88 } } };
	struct in6_addr dst_addr3 = { { { 0x20, 0x01, 0x0d, 0xb8, 3, 0, 0, 0,
					0, 0, 0, 0, 0, 0, 0, 0x99 } } };
	struct in6_addr in6addr_mcast1 = { { { 0x00 } } };
	struct in_addr dst_addr_2 = { { { 192, 0, 2, 2 } } };

	struct net_if_addr *ifaddr;
	struct net_if *iface;
	struct sockaddr_in ipv4;
	struct sockaddr_in6 ipv6;

	iface = net_if_ipv6_select_src_iface(&dst_addr1);
	zassert_equal_ptr(iface, iface1, "Invalid interface %p vs %p selected",
			  iface, iface1);

	iface = net_if_ipv6_select_src_iface(&ll_addr1);
	zassert_equal_ptr(iface, iface1, "Invalid interface %p vs %p selected",
			  iface, iface1);

	net_ipv6_addr_create(&in6addr_mcast1, 0xff02, 0, 0, 0, 0, 0, 0, 0x0002);

	iface = net_if_ipv6_select_src_iface(&in6addr_mcast1);
	zassert_equal_ptr(iface, iface1, "Invalid interface %p vs %p selected",
			  iface, iface1);

	iface = net_if_ipv6_select_src_iface(&dst_addr3);
	zassert_equal_ptr(iface, iface2, "Invalid interface %p vs %p selected",
			  iface, iface2);

	ifaddr = net_if_ipv6_addr_lookup(&ll_addr, NULL);
	zassert_not_null(ifaddr, "No such ll_addr found");

	ifaddr->addr_state = NET_ADDR_TENTATIVE;

	/* We should now get default interface */
	iface = net_if_ipv6_select_src_iface(&ll_addr1);
	zassert_equal_ptr(iface, net_if_get_default(),
			  "Invalid interface %p vs %p selected",
			  iface, net_if_get_default());

	net_ipaddr_copy(&ipv4.sin_addr, &dst_addr_2);
	ipv4.sin_family = AF_INET;
	ipv4.sin_port = 0;

	iface = net_if_select_src_iface((struct sockaddr *)&ipv4);
	zassert_equal_ptr(iface, iface1, "Invalid interface %p vs %p selected",
			  iface, iface1);

	net_ipaddr_copy(&ipv6.sin6_addr, &dst_addr1);
	ipv6.sin6_family = AF_INET6;
	ipv6.sin6_port = 0;

	iface = net_if_select_src_iface((struct sockaddr *)&ipv6);
	zassert_equal_ptr(iface, iface1, "Invalid interface %p vs %p selected",
			  iface, iface1);
}

static void check_promisc_mode_off(void)
{
	bool ret;

	DBG("Make sure promiscuous mode is OFF (%p)\n", iface4);

	ret = net_if_is_promisc(iface4);

	zassert_false(ret, "iface 1 promiscuous mode ON");
}

static void check_promisc_mode_on(void)
{
	bool ret;

	DBG("Make sure promiscuous mode is ON (%p)\n", iface4);

	ret = net_if_is_promisc(iface4);

	zassert_true(ret, "iface 1 promiscuous mode OFF");
}

static void set_promisc_mode_on_again(void)
{
	int ret;

	DBG("Make sure promiscuous mode is ON (%p)\n", iface4);

	ret = net_if_set_promisc(iface4);

	zassert_equal(ret, -EALREADY, "iface 1 promiscuous mode OFF");
}

static void set_promisc_mode_on(void)
{
	bool ret;

	DBG("Setting promiscuous mode ON (%p)\n", iface4);

	ret = net_if_set_promisc(iface4);

	zassert_equal(ret, 0, "iface 1 promiscuous mode set failed");
}

static void set_promisc_mode_off(void)
{
	DBG("Setting promiscuous mode OFF (%p)\n", iface4);

	net_if_unset_promisc(iface4);
}

void test_main(void)
{
	ztest_test_suite(net_iface_test,
			 ztest_unit_test(iface_setup),
			 ztest_unit_test(send_iface1),
			 ztest_unit_test(send_iface2),
			 ztest_unit_test(send_iface3),
			 ztest_unit_test(send_iface1_down),
			 ztest_unit_test(send_iface1_up),
			 ztest_unit_test(select_src_iface),
			 ztest_unit_test(check_promisc_mode_off),
			 ztest_unit_test(set_promisc_mode_on),
			 ztest_unit_test(check_promisc_mode_on),
			 ztest_unit_test(set_promisc_mode_on_again),
			 ztest_unit_test(set_promisc_mode_off),
			 ztest_unit_test(check_promisc_mode_off));

	ztest_run_test_suite(net_iface_test);
}
