/* echo-client.c - Networking echo client */

/*
 * Copyright (c) 2017 Intel Corporation.
 * Copyright (c) 2018 Nordic Semiconductor ASA.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * The echo-client application is acting as a client that is run in Zephyr OS,
 * and echo-server is run in the host acting as a server. The client will send
 * either unicast or multicast packets to the server which will reply the packet
 * back to the originator.
 *
 * In this sample application we create four threads that start to send data.
 * This might not be what you want to do in your app so caveat emptor.
 */

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(net_echo_client_sample, LOG_LEVEL_DBG);

#include <zephyr/kernel.h>
#include <errno.h>
#include <stdio.h>

#include <zephyr/net/socket.h>
#include <zephyr/net/tls_credentials.h>

#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/net_event.h>
#include <zephyr/net/conn_mgr.h>

#if defined(CONFIG_USERSPACE)
#include <zephyr/app_memory/app_memdomain.h>
K_APPMEM_PARTITION_DEFINE(app_partition);
struct k_mem_domain app_domain;
#endif

#include "common.h"
#include "ca_certificate.h"

#define APP_BANNER "Run echo client"

#define INVALID_SOCK (-1)

#define EVENT_MASK (NET_EVENT_L4_CONNECTED | \
		    NET_EVENT_L4_DISCONNECTED)

/* Generated by http://www.lipsum.com/
 * 2 paragraphs, 179 words, 1160 bytes of Lorem Ipsum
 */
const char lorem_ipsum[] =
	"Lorem ipsum dolor sit amet, consectetur adipiscing elit. Quisque "
	"sodales lorem lorem, sed congue enim vehicula a. Sed finibus diam sed "
	"odio ultrices pharetra. Nullam dictum arcu ultricies turpis congue, "
	"vel venenatis turpis venenatis. Nam tempus arcu eros, ac congue libero "
	"tristique congue. Proin velit lectus, euismod sit amet quam in, "
	"maximus condimentum urna. Cras vel erat luctus, mattis orci ut, varius "
	"urna. Nam eu lobortis velit."
	"\n"
	"Nullam sit amet diam vel odio sodales cursus vehicula eu arcu. Proin "
	"fringilla, enim nec consectetur mollis, lorem orci interdum nisi, "
	"vitae suscipit nisi mauris eu mi. Proin diam enim, mollis ac rhoncus "
	"vitae, placerat et eros. Suspendisse convallis, ipsum nec rhoncus "
	"aliquam, ex augue ultrices nisl, id aliquet mi diam quis ante. "
	"Pellentesque venenatis ornare ultrices. Quisque et porttitor lectus. "
	"Ut venenatis nunc et urna imperdiet porttitor non laoreet massa. Donec "
	"eleifend eros in mi sagittis egestas. Sed et mi nunc. Nunc vulputate, "
	"mauris non ullamcorper viverra, lorem nulla vulputate diam, et congue "
	"dui velit non erat. Duis interdum leo et ipsum tempor consequat. In "
	"faucibus enim quis purus vulputate nullam."
	"\n";

const int ipsum_len = sizeof(lorem_ipsum) - 1;

APP_DMEM struct configs conf = {
	.ipv4 = {
		.proto = "IPv4",
		.udp.sock = INVALID_SOCK,
		.tcp.sock = INVALID_SOCK,
	},
	.ipv6 = {
		.proto = "IPv6",
		.udp.sock = INVALID_SOCK,
		.tcp.sock = INVALID_SOCK,
	},
};

static APP_BMEM struct pollfd fds[4];
static APP_BMEM int nfds;

static APP_BMEM bool connected;
K_SEM_DEFINE(run_app, 0, 1);

static struct net_mgmt_event_callback mgmt_cb;

static void prepare_fds(void)
{
	if (conf.ipv4.udp.sock >= 0) {
		fds[nfds].fd = conf.ipv4.udp.sock;
		fds[nfds].events = POLLIN;
		nfds++;
	}

	if (conf.ipv4.tcp.sock >= 0) {
		fds[nfds].fd = conf.ipv4.tcp.sock;
		fds[nfds].events = POLLIN;
		nfds++;
	}

	if (conf.ipv6.udp.sock >= 0) {
		fds[nfds].fd = conf.ipv6.udp.sock;
		fds[nfds].events = POLLIN;
		nfds++;
	}

	if (conf.ipv6.tcp.sock >= 0) {
		fds[nfds].fd = conf.ipv6.tcp.sock;
		fds[nfds].events = POLLIN;
		nfds++;
	}
}

static void wait(void)
{
	/* Wait for event on any socket used. Once event occurs,
	 * we'll check them all.
	 */
	if (poll(fds, nfds, -1) < 0) {
		LOG_ERR("Error in poll:%d", errno);
	}
}

static int start_udp_and_tcp(void)
{
	int ret;

	LOG_INF("Starting...");

	if (IS_ENABLED(CONFIG_NET_TCP)) {
		ret = start_tcp();
		if (ret < 0) {
			return ret;
		}
	}

	if (IS_ENABLED(CONFIG_NET_UDP)) {
		ret = start_udp();
		if (ret < 0) {
			return ret;
		}
	}

	prepare_fds();

	return 0;
}

static int run_udp_and_tcp(void)
{
	int ret;

	wait();

	if (IS_ENABLED(CONFIG_NET_TCP)) {
		ret = process_tcp();
		if (ret < 0) {
			return ret;
		}
	}

	if (IS_ENABLED(CONFIG_NET_UDP)) {
		ret = process_udp();
		if (ret < 0) {
			return ret;
		}
	}

	return 0;
}

static void stop_udp_and_tcp(void)
{
	LOG_INF("Stopping...");

	if (IS_ENABLED(CONFIG_NET_UDP)) {
		stop_udp();
	}

	if (IS_ENABLED(CONFIG_NET_TCP)) {
		stop_tcp();
	}
}

static void event_handler(struct net_mgmt_event_callback *cb,
			  uint32_t mgmt_event, struct net_if *iface)
{
	if ((mgmt_event & EVENT_MASK) != mgmt_event) {
		return;
	}

	if (mgmt_event == NET_EVENT_L4_CONNECTED) {
		LOG_INF("Network connected");

		connected = true;
		conf.ipv4.udp.mtu = net_if_get_mtu(iface);
		conf.ipv6.udp.mtu = conf.ipv4.udp.mtu;
		k_sem_give(&run_app);

		return;
	}

	if (mgmt_event == NET_EVENT_L4_DISCONNECTED) {
		LOG_INF("Network disconnected");

		connected = false;
		k_sem_reset(&run_app);

		return;
	}
}

static void init_app(void)
{
	LOG_INF(APP_BANNER);

#if defined(CONFIG_USERSPACE)
	struct k_mem_partition *parts[] = {
#if Z_LIBC_PARTITION_EXISTS
		&z_libc_partition,
#endif
		&app_partition
	};

	int ret = k_mem_domain_init(&app_domain, ARRAY_SIZE(parts), parts);

	__ASSERT(ret == 0, "k_mem_domain_init() failed %d", ret);
	ARG_UNUSED(ret);
#endif

#if defined(CONFIG_NET_SOCKETS_SOCKOPT_TLS)
	int err = tls_credential_add(CA_CERTIFICATE_TAG,
				    TLS_CREDENTIAL_CA_CERTIFICATE,
				    ca_certificate,
				    sizeof(ca_certificate));
	if (err < 0) {
		LOG_ERR("Failed to register public certificate: %d", err);
	}
#endif

#if defined(CONFIG_MBEDTLS_KEY_EXCHANGE_PSK_ENABLED)
	err = tls_credential_add(PSK_TAG,
				TLS_CREDENTIAL_PSK,
				psk,
				sizeof(psk));
	if (err < 0) {
		LOG_ERR("Failed to register PSK: %d", err);
	}
	err = tls_credential_add(PSK_TAG,
				TLS_CREDENTIAL_PSK_ID,
				psk_id,
				sizeof(psk_id) - 1);
	if (err < 0) {
		LOG_ERR("Failed to register PSK ID: %d", err);
	}
#endif

	if (IS_ENABLED(CONFIG_NET_CONNECTION_MANAGER)) {
		net_mgmt_init_event_callback(&mgmt_cb,
					     event_handler, EVENT_MASK);
		net_mgmt_add_event_callback(&mgmt_cb);

		conn_mgr_resend_status();
	}

	init_vlan();
	init_udp();
}

static int start_client(void)
{
	int iterations = CONFIG_NET_SAMPLE_SEND_ITERATIONS;
	int i = 0;
	int ret;

	while (iterations == 0 || i < iterations) {
		/* Wait for the connection. */
		k_sem_take(&run_app, K_FOREVER);

		ret = start_udp_and_tcp();

		while (connected && (ret == 0)) {
			ret = run_udp_and_tcp();

			if (iterations > 0) {
				i++;
				if (i >= iterations) {
					break;

				}
			}
		}

		stop_udp_and_tcp();
	}

	return ret;
}

int main(void)
{
	init_app();

	if (!IS_ENABLED(CONFIG_NET_CONNECTION_MANAGER)) {
		/* If the config library has not been configured to start the
		 * app only after we have a connection, then we can start
		 * it right away.
		 */
		k_sem_give(&run_app);
	}

	k_thread_priority_set(k_current_get(), THREAD_PRIORITY);

#if defined(CONFIG_USERSPACE)
	k_thread_access_grant(k_current_get(), &run_app);
	k_mem_domain_add_thread(&app_domain, k_current_get());

	k_thread_user_mode_enter((k_thread_entry_t)start_client, NULL, NULL,
				 NULL);
#else
	exit(start_client());
#endif
	return 0;
}
