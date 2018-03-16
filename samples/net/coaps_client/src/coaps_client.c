/*  Sample CoAP over DTLS client using mbedTLS.
 *  (Meant to be used with config-coap.h)
 *
 *  Copyright (C) 2006-2015, ARM Limited, All Rights Reserved
 *
 *  SPDX-License-Identifier: Apache-2.0
 *
 *  This file is part of mbed TLS (https://tls.mbed.org)
 */

#include <zephyr.h>
#include <stdio.h>
#include <errno.h>
#include <misc/printk.h>

#if !defined(CONFIG_MBEDTLS_CFG_FILE)
#include "mbedtls/config.h"
#else
#include CONFIG_MBEDTLS_CFG_FILE
#endif

#if defined(MBEDTLS_PLATFORM_C)
#include "mbedtls/platform.h"
#else
#include <stdlib.h>
#define mbedtls_time_t       time_t
#define MBEDTLS_EXIT_SUCCESS EXIT_SUCCESS
#define MBEDTLS_EXIT_FAILURE EXIT_FAILURE
#endif

#include <string.h>
#include <net/net_context.h>
#include <net/net_if.h>
#include <net/buf.h>
#include <net/net_pkt.h>
#include "udp.h"
#include "udp_cfg.h"

#include "mbedtls/net.h"
#include "mbedtls/ssl.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"

#include <net/coap.h>

#if defined(MBEDTLS_DEBUG_C)
#include "mbedtls/debug.h"
#define DEBUG_THRESHOLD 0
#endif

#if defined(MBEDTLS_MEMORY_BUFFER_ALLOC_C)
#include "mbedtls/memory_buffer_alloc.h"
static unsigned char heap[8192];
#endif

/*
 * Hardcoded values for server host and port
 */

#if defined(MBEDTLS_KEY_EXCHANGE__SOME__PSK_ENABLED)
const unsigned char psk[] = "passwd\0";
const char psk_id[] = "Client_identity\0";
#endif

const char *pers = "mini_client";
static unsigned char payload[128];

#define NUM_REPLIES 3
struct coap_reply replies[NUM_REPLIES];

#define COAP_BUF_SIZE 128

NET_PKT_TX_SLAB_DEFINE(coap_pkt_slab, 4);
NET_BUF_POOL_DEFINE(coap_data_pool, 4, COAP_BUF_SIZE, 0, NULL);

static const char *const test_path[] = { "test", NULL };

static struct in6_addr mcast_addr = MCAST_IP_ADDR;

struct dtls_timing_context {
	u32_t snapshot;
	u32_t int_ms;
	u32_t fin_ms;
};

static void msg_dump(const char *s, u8_t *data, unsigned int len)
{
	unsigned int i;

	printk("%s: ", s);
	for (i = 0; i < len; i++) {
		printk("%02x ", data[i]);
	}

	printk("(%u bytes)\n", len);
}

static int resource_reply_cb(const struct coap_packet *response,
			     struct coap_reply *reply,
			     const struct sockaddr *from)
{

	struct net_buf *frag = response->pkt->frags;

	while (frag) {
		msg_dump("reply", frag->data, frag->len);
		frag = frag->frags;
	}

	return 0;
}

static void my_debug(void *ctx, int level,
		     const char *file, int line, const char *str)
{
	const char *p, *basename;

	/* Extract basename from file */
	for (p = basename = file; *p != '\0'; p++) {
		if (*p == '/' || *p == '\\') {
			basename = p + 1;
		}
	}

	mbedtls_printf("%s:%04d: |%d| %s", basename, line, level, str);
}

void dtls_timing_set_delay(void *data, u32_t int_ms, u32_t fin_ms)
{
	struct dtls_timing_context *ctx = (struct dtls_timing_context *)data;

	ctx->int_ms = int_ms;
	ctx->fin_ms = fin_ms;

	if (fin_ms != 0) {
		ctx->snapshot = k_uptime_get_32();
	}
}

int dtls_timing_get_delay(void *data)
{
	struct dtls_timing_context *ctx = (struct dtls_timing_context *)data;
	unsigned long elapsed_ms;

	if (ctx->fin_ms == 0) {
		return -1;
	}

	elapsed_ms = k_uptime_get_32() - ctx->snapshot;

	if (elapsed_ms >= ctx->fin_ms) {
		return 2;
	}

	if (elapsed_ms >= ctx->int_ms) {
		return 1;
	}

	return 0;
}

static int entropy_source(void *data, unsigned char *output, size_t len,
			  size_t *olen)
{
	u32_t seed;

	ARG_UNUSED(data);

	seed = sys_rand32_get();

	if (len > sizeof(seed)) {
		len = sizeof(seed);
	}

	memcpy(output, &seed, len);

	*olen = len;

	return 0;
}

void dtls_client(void)
{
	int ret;
	struct udp_context ctx;
	struct dtls_timing_context timer;
	struct coap_packet request, cpkt;
	struct coap_reply *reply;
	struct net_pkt *pkt;
	struct net_buf *frag;
	u8_t observe = 0;
	const char *const *p;
	u16_t len;

	mbedtls_entropy_context entropy;
	mbedtls_ctr_drbg_context ctr_drbg;
	mbedtls_ssl_context ssl;
	mbedtls_ssl_config conf;

	mbedtls_ctr_drbg_init(&ctr_drbg);
	mbedtls_platform_set_printf(printk);
	mbedtls_ssl_init(&ssl);
	mbedtls_ssl_config_init(&conf);
	mbedtls_entropy_init(&entropy);
	mbedtls_entropy_add_source(&entropy, entropy_source, NULL,
				   MBEDTLS_ENTROPY_MAX_GATHER,
				   MBEDTLS_ENTROPY_SOURCE_STRONG);

	ret = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy,
				    (const unsigned char *)pers, strlen(pers));
	if (ret != 0) {
		mbedtls_printf("mbedtls_ctr_drbg_seed failed returned -0x%x\n",
			       -ret);
		goto exit;
	}

	ret = mbedtls_ssl_config_defaults(&conf,
					  MBEDTLS_SSL_IS_CLIENT,
					  MBEDTLS_SSL_TRANSPORT_DATAGRAM,
					  MBEDTLS_SSL_PRESET_DEFAULT);
	if (ret != 0) {
		mbedtls_printf("mbedtls_ssl_config_defaults"
			       " failed! returned -0x%x\n", -ret);
		goto exit;
	}

/* Modify this to change the default timeouts for the DTLS handshake */
/*        mbedtls_ssl_conf_handshake_timeout( &conf, min, max ); */

#if defined(MBEDTLS_DEBUG_C)
	mbedtls_debug_set_threshold(DEBUG_THRESHOLD);
#endif

	mbedtls_ssl_conf_rng(&conf, mbedtls_ctr_drbg_random, &ctr_drbg);
	mbedtls_ssl_conf_dbg(&conf, my_debug, NULL);

#if defined(MBEDTLS_MEMORY_BUFFER_ALLOC_C)
	mbedtls_memory_buffer_alloc_init(heap, sizeof(heap));
#endif

	ret = mbedtls_ssl_setup(&ssl, &conf);

	if (ret != 0) {
		mbedtls_printf("mbedtls_ssl_setup failed returned -0x%x\n",
			       -ret);
		goto exit;
	}

	ret = udp_init(&ctx);
	if (ret != 0) {
		mbedtls_printf("udp_init failed returned 0x%x\n", ret);
		goto exit;
	}

	udp_tx(&ctx, payload, 32);

#if defined(MBEDTLS_KEY_EXCHANGE__SOME__PSK_ENABLED)
	ret = mbedtls_ssl_conf_psk(&conf, psk, strlen((char *)psk),
				   (unsigned char *)psk_id,
				   strlen(psk_id));
	if (ret != 0) {
		mbedtls_printf("  failed\n  mbedtls_ssl_conf_psk"
			       " returned -0x%x\n", -ret);
		goto exit;
	}
#endif

	mbedtls_ssl_set_timer_cb(&ssl, &timer, dtls_timing_set_delay,
				 dtls_timing_get_delay);

	mbedtls_ssl_set_bio(&ssl, &ctx, udp_tx, udp_rx, NULL);

	do {
		ret = mbedtls_ssl_handshake(&ssl);
	} while (ret == MBEDTLS_ERR_SSL_WANT_READ ||
		 ret == MBEDTLS_ERR_SSL_WANT_WRITE);

	if (ret != 0) {
		mbedtls_printf("mbedtls_ssl_handshake failed returned -0x%x\n",
			       -ret);
		goto exit;
	}

	/* Write to server */
retry:
	pkt = net_pkt_get_reserve(&coap_pkt_slab, 0, K_NO_WAIT);
	if (!pkt) {
		goto exit;
	}

	frag = net_buf_alloc(&coap_data_pool, K_NO_WAIT);
	if (!frag) {
		goto exit;
	}

	net_pkt_frag_add(pkt, frag);

	ret = coap_packet_init(&request, pkt, 1, COAP_TYPE_CON,
			       0, NULL, COAP_METHOD_GET, coap_next_id());
	if (ret < 0) {
		goto exit;
	}

	/* Enable observing the resource. */
	ret = coap_packet_append_option(&request, COAP_OPTION_OBSERVE,
					&observe, sizeof(observe));
	if (ret < 0) {
		mbedtls_printf("Unable add option to request.\n");
		goto exit;
	}

	for (p = test_path; p && *p; p++) {
		ret = coap_packet_append_option(&request, COAP_OPTION_URI_PATH,
						*p, strlen(*p));
		if (ret < 0) {
			mbedtls_printf("Unable add option/path to request.\n");
			goto exit;
		}
	}

	reply = coap_reply_next_unused(replies, NUM_REPLIES);
	if (!reply) {
		mbedtls_printf("No resources for waiting for replies.\n");
		goto exit;
	}

	coap_reply_init(reply, &request);
	reply->reply = resource_reply_cb;
	len = frag->len;

	do {
		ret = mbedtls_ssl_write(&ssl, frag->data, len);
	} while (ret == MBEDTLS_ERR_SSL_WANT_READ ||
		 ret == MBEDTLS_ERR_SSL_WANT_WRITE);

	net_pkt_unref(pkt);

	if (ret <= 0) {
		mbedtls_printf("mbedtls_ssl_write failed returned 0x%x\n",
			       -ret);
		goto exit;
	}

	pkt = net_pkt_get_reserve(&coap_pkt_slab, 0, K_NO_WAIT);
	if (!pkt) {
		mbedtls_printf("Could not get packet from pool\n");
		goto exit;
	}

	frag = net_buf_alloc(&coap_data_pool, K_NO_WAIT);
	if (!frag) {
		mbedtls_printf("Could not get frag from pool\n");
		goto exit;
	}

	net_pkt_frag_add(pkt, frag);
	len = COAP_BUF_SIZE - 1;
	memset(frag->data, 0, COAP_BUF_SIZE);

	do {
		ret = mbedtls_ssl_read(&ssl, frag->data, COAP_BUF_SIZE - 1);
	} while (ret == MBEDTLS_ERR_SSL_WANT_READ ||
		 ret == MBEDTLS_ERR_SSL_WANT_WRITE);

	if (ret <= 0) {
		net_pkt_unref(pkt);

		switch (ret) {
		case MBEDTLS_ERR_SSL_TIMEOUT:
			mbedtls_printf(" timeout\n");
			goto retry;

		case MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY:
			mbedtls_printf(" connection was closed"
				       " gracefully\n");
			goto exit;

		default:
			mbedtls_printf(" mbedtls_ssl_read"
				       " returned -0x%x\n", -ret);
			goto exit;
		}
	}

	len = ret;
	frag->len = len;

	ret = coap_packet_parse(&cpkt, pkt, NULL, 0);
	if (ret) {
		mbedtls_printf("Could not parse packet\n");
		goto exit;
	}

	reply = coap_response_received(&cpkt, NULL, replies, NUM_REPLIES);
	if (!reply) {
		mbedtls_printf("No handler for response (%d)\n", ret);
	}

	net_pkt_unref(pkt);
	mbedtls_ssl_close_notify(&ssl);
exit:

	mbedtls_ssl_free(&ssl);
	mbedtls_ssl_config_free(&conf);
	mbedtls_ctr_drbg_free(&ctr_drbg);
	mbedtls_entropy_free(&entropy);
}

#define STACK_SIZE		4096
K_THREAD_STACK_DEFINE(stack, STACK_SIZE);
static struct k_thread thread_data;

static inline int init_app(void)
{
#if defined(CONFIG_NET_APP_MY_IPV6_ADDR)
	if (net_addr_pton(AF_INET6,
			  CONFIG_NET_APP_MY_IPV6_ADDR,
			  (struct sockaddr *)&client_addr) < 0) {
		mbedtls_printf("Invalid IPv6 address %s",
			       CONFIG_NET_APP_MY_IPV6_ADDR);
	}
#endif
	if (!net_if_ipv6_addr_add(net_if_get_default(), &client_addr,
				  NET_ADDR_MANUAL, 0)) {
		return -EIO;
	}

	net_if_ipv6_maddr_add(net_if_get_default(), &mcast_addr);

	return 0;
}

void main(void)
{
	if (init_app() != 0) {
		printk("Cannot initialize network\n");
		return;
	}

	k_thread_create(&thread_data, stack, STACK_SIZE,
			(k_thread_entry_t) dtls_client,
			NULL, NULL, NULL, K_PRIO_COOP(7), 0, 0);
}
