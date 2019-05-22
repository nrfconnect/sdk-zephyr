/*
 * Copyright (c) 2016 Intel Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <logging/log.h>
LOG_MODULE_REGISTER(net_ieee802154_test, LOG_LEVEL_DBG);

#include <zephyr.h>
#include <ztest.h>

#include <net/net_core.h>
#include "net_private.h"

#include <net/net_ip.h>
#include <net/net_pkt.h>

#include <ieee802154_frame.h>
#include <ipv6.h>

struct ieee802154_pkt_test {
	char *name;
	struct in6_addr src;
	struct in6_addr dst;
	u8_t *pkt;
	u8_t length;
	struct {
		struct ieee802154_fcf_seq *fc_seq;
		struct ieee802154_address_field *dst_addr;
		struct ieee802154_address_field *src_addr;
	} mhr_check;
};

u8_t ns_pkt[] = {
	0x41, 0xd8, 0x3e, 0xcd, 0xab, 0xff, 0xff, 0xc2, 0xa3, 0x9e, 0x00,
	0x00, 0x4b, 0x12, 0x00, 0x7b, 0x09, 0x3a, 0x20, 0x01, 0x0d, 0xb8,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x02, 0x02, 0x01, 0xff, 0x00, 0x00, 0x01, 0x87, 0x00, 0x2e, 0xad,
	0x00, 0x00, 0x00, 0x00, 0x20, 0x01, 0x0d, 0xb8, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x02,
	0x00, 0x12, 0x4b, 0x00, 0x00, 0x9e, 0xa3, 0xc2, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x3d, 0x74
};

struct ieee802154_pkt_test test_ns_pkt = {
	.name = "NS frame",
	.src =  { { { 0x20, 0x01, 0xdb, 0x08, 0x00, 0x00, 0x00, 0x00,
		      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01 } } },
	.dst =  { { { 0xff, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		      0x00, 0x00, 0x00, 0x01, 0xff, 0x00, 0x00, 0x01 } } },
	.pkt = ns_pkt,
	.length = sizeof(ns_pkt),
	.mhr_check.fc_seq = (struct ieee802154_fcf_seq *)ns_pkt,
	.mhr_check.dst_addr = (struct ieee802154_address_field *)(ns_pkt + 3),
	.mhr_check.src_addr = (struct ieee802154_address_field *)(ns_pkt + 7),
};

u8_t ack_pkt[] = { 0x02, 0x10, 0x16 };

struct ieee802154_pkt_test test_ack_pkt = {
	.name = "ACK frame",
	.pkt = ack_pkt,
	.length = sizeof(ack_pkt),
	.mhr_check.fc_seq = (struct ieee802154_fcf_seq *)ack_pkt,
	.mhr_check.dst_addr = NULL,
	.mhr_check.src_addr = NULL,
};

u8_t beacon_pkt[] = {
	0x00, 0xd0, 0x11, 0xcd, 0xab, 0xc2, 0xa3, 0x9e, 0x00, 0x00, 0x4b,
	0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

struct ieee802154_pkt_test test_beacon_pkt = {
	.name = "Empty beacon frame",
	.pkt = beacon_pkt,
	.length = sizeof(beacon_pkt),
	.mhr_check.fc_seq = (struct ieee802154_fcf_seq *)beacon_pkt,
	.mhr_check.dst_addr = NULL,
	.mhr_check.src_addr =
	(struct ieee802154_address_field *) (beacon_pkt + 3),
};

u8_t sec_data_pkt[] = {
	0x49, 0xd8, 0x03, 0xcd, 0xab, 0xff, 0xff, 0x02, 0x6d, 0xbb, 0xa7,
	0x00, 0x4b, 0x12, 0x00, 0x05, 0x00, 0x00, 0x00, 0x00, 0xd3, 0x8e,
	0x49, 0xa7, 0xe2, 0x00, 0x67, 0xd4, 0x00, 0x42, 0x52, 0x6f, 0x01,
	0x02, 0x00, 0x12, 0x4b, 0x00, 0xa7, 0xbb, 0x6d, 0x02, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x19, 0x7f, 0x91, 0xcf, 0x73, 0xf0
};

struct ieee802154_pkt_test test_sec_data_pkt = {
	.name = "Secured data frame",
	.pkt = sec_data_pkt,
	.length = sizeof(sec_data_pkt),
	.mhr_check.fc_seq = (struct ieee802154_fcf_seq *)sec_data_pkt,
	.mhr_check.dst_addr =
	(struct ieee802154_address_field *)(sec_data_pkt + 3),
	.mhr_check.src_addr =
	(struct ieee802154_address_field *)(sec_data_pkt + 7),
};

struct net_pkt *current_pkt;
struct net_if *iface;
K_SEM_DEFINE(driver_lock, 0, UINT_MAX);

static void pkt_hexdump(u8_t *pkt, u8_t length)
{
	int i;

	printk(" -> Packet content:\n");

	for (i = 0; i < length;) {
		int j;

		printk("\t");

		for (j = 0; j < 10 && i < length; j++, i++) {
			printk("%02x ", *pkt);
			pkt++;
		}

		printk("\n");
	}
}

static void ieee_addr_hexdump(u8_t *addr, u8_t length)
{
	int i;

	printk(" -> IEEE 802.15.4 Address: ");

	for (i = 0; i < length-1; i++) {
		printk("%02x:", *addr);
		addr++;
	}

	printk("%02x\n", *addr);
}

static bool test_packet_parsing(struct ieee802154_pkt_test *t)
{
	struct ieee802154_mpdu mpdu;

	NET_INFO("- Parsing packet 0x%p of frame %s\n", t->pkt, t->name);

	if (!ieee802154_validate_frame(t->pkt, t->length, &mpdu)) {
		NET_ERR("*** Could not validate frame %s\n", t->name);
		return false;
	}

	if (mpdu.mhr.fs != t->mhr_check.fc_seq ||
	    mpdu.mhr.dst_addr != t->mhr_check.dst_addr ||
	    mpdu.mhr.src_addr != t->mhr_check.src_addr) {
		NET_INFO("d: %p vs %p -- s: %p vs %p\n",
			 mpdu.mhr.dst_addr, t->mhr_check.dst_addr,
			 mpdu.mhr.src_addr, t->mhr_check.src_addr);
		NET_ERR("*** Wrong MPDU information on frame %s\n",
			t->name);

		return false;
	}

	return true;
}

static bool test_ns_sending(struct ieee802154_pkt_test *t)
{
	struct ieee802154_mpdu mpdu;

	NET_INFO("- Sending NS packet\n");

	if (net_ipv6_send_ns(iface, NULL, &t->src, &t->dst, &t->dst, false)) {
		NET_ERR("*** Could not create IPv6 NS packet\n");
		return false;
	}

	k_yield();
	k_sem_take(&driver_lock, K_SECONDS(1));

	if (!current_pkt->frags) {
		NET_ERR("*** Could not send IPv6 NS packet\n");
		return false;
	}

	pkt_hexdump(net_pkt_data(current_pkt), net_pkt_get_len(current_pkt));

	if (!ieee802154_validate_frame(net_pkt_data(current_pkt),
				       net_pkt_get_len(current_pkt), &mpdu)) {
		NET_ERR("*** Sent packet is not valid\n");
		net_pkt_unref(current_pkt);

		return false;
	}

	net_pkt_frag_unref(current_pkt->frags);
	current_pkt->frags = NULL;

	return true;
}

static bool test_ack_reply(struct ieee802154_pkt_test *t)
{
	static u8_t data_pkt[] = {
		0x61, 0xdc, 0x16, 0xcd, 0xab, 0x26, 0x11, 0x32, 0x00, 0x00, 0x4b,
		0x12, 0x00, 0x26, 0x18, 0x32, 0x00, 0x00, 0x4b, 0x12, 0x00, 0x7b,
		0x00, 0x3a, 0x20, 0x01, 0x0d, 0xb8, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x20, 0x01, 0x0d, 0xb8,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x02, 0x87, 0x00, 0x8b, 0x00, 0x00, 0x00, 0x00, 0x00, 0x20, 0x01,
		0x0d, 0xb8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff,
		0x16, 0xf0, 0x02, 0xff, 0x16, 0xf0, 0x12, 0xff, 0x16, 0xf0, 0x32,
		0xff, 0x16, 0xf0, 0x00, 0xff, 0x16, 0xf0, 0x00, 0xff, 0x16
	};
	struct ieee802154_mpdu mpdu;
	struct net_pkt *pkt;
	struct net_buf *frag;

	NET_INFO("- Sending ACK reply to a data packet\n");

	pkt = net_pkt_rx_alloc(K_FOREVER);
	frag = net_pkt_get_frag(pkt, K_FOREVER);

	memcpy(frag->data, data_pkt, sizeof(data_pkt));
	frag->len = sizeof(data_pkt);

	net_pkt_frag_add(pkt, frag);

	if (net_recv_data(iface, pkt) == NET_DROP) {
		NET_ERR("Packet dropped");
		return false;
	}

	k_yield();
	k_sem_take(&driver_lock, K_SECONDS(1));

	/* an ACK packet should be in current_pkt */
	if (!current_pkt->frags) {
		NET_ERR("*** No ACK reply sent\n");
		return false;
	}

	pkt_hexdump(net_pkt_data(current_pkt), net_pkt_get_len(current_pkt));

	if (!ieee802154_validate_frame(net_pkt_data(current_pkt),
				       net_pkt_get_len(current_pkt), &mpdu)) {
		NET_ERR("*** ACK Reply is invalid\n");
		return false;
	}

	if (memcmp(mpdu.mhr.fs, t->mhr_check.fc_seq,
		   sizeof(struct ieee802154_fcf_seq))) {
		NET_ERR("*** ACK Reply does not compare\n");
		return false;
	}

	net_pkt_frag_unref(current_pkt->frags);
	current_pkt->frags = NULL;

	return true;
}

static bool initialize_test_environment(void)
{
	struct device *dev;

	k_sem_reset(&driver_lock);

	current_pkt = net_pkt_rx_alloc(K_FOREVER);
	if (!current_pkt) {
		NET_ERR("*** No buffer to allocate\n");
		return false;
	}

	dev = device_get_binding("fake_ieee802154");
	if (!dev) {
		NET_ERR("*** Could not get fake device\n");
		return false;
	}

	iface = net_if_lookup_by_dev(dev);
	if (!iface) {
		NET_ERR("*** Could not get fake iface\n");
		return false;
	}

	NET_INFO("Fake IEEE 802.15.4 network interface ready\n");

	ieee_addr_hexdump(net_if_get_link_addr(iface)->addr, 8);

	return true;
}

static void test_init(void)
{
	bool ret;

	ret = initialize_test_environment();

	zassert_true(ret, "Test initialization");
}


static void test_parsing_ns_pkt(void)
{
	bool ret;

	ret = test_packet_parsing(&test_ns_pkt);

	zassert_true(ret, "NS parsed");
}

static void test_sending_ns_pkt(void)
{
	bool ret;

	ret = test_ns_sending(&test_ns_pkt);

	zassert_true(ret, "NS sent");
}

static void test_parsing_ack_pkt(void)
{
	bool ret;

	ret = test_packet_parsing(&test_ack_pkt);

	zassert_true(ret, "ACK parsed");
}

static void test_replying_ack_pkt(void)
{
	bool ret;

	ret = test_ack_reply(&test_ack_pkt);

	zassert_true(ret, "ACK replied");
}

static void test_parsing_beacon_pkt(void)
{
	bool ret;

	ret = test_packet_parsing(&test_beacon_pkt);

	zassert_true(ret, "Beacon parsed");
}

static void test_parsing_sec_data_pkt(void)
{
	bool ret;

	ret = test_packet_parsing(&test_sec_data_pkt);

	zassert_true(ret, "Secured data frame parsed");
}

void test_main(void)
{
	ztest_test_suite(ieee802154_l2,
			 ztest_unit_test(test_init),
			 ztest_unit_test(test_parsing_ns_pkt),
			 ztest_unit_test(test_sending_ns_pkt),
			 ztest_unit_test(test_parsing_ack_pkt),
			 ztest_unit_test(test_replying_ack_pkt),
			 ztest_unit_test(test_parsing_beacon_pkt),
			 ztest_unit_test(test_parsing_sec_data_pkt)
		);

	ztest_run_test_suite(ieee802154_l2);
}
