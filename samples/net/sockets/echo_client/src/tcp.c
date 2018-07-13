/* tcp.c - TCP specific code for echo client */

/*
 * Copyright (c) 2017 Intel Corporation.
 * Copyright (c) 2018 Nordic Semiconductor ASA.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#if 1
#define SYS_LOG_DOMAIN "echo-client"
#define NET_SYS_LOG_LEVEL SYS_LOG_LEVEL_DEBUG
#define NET_LOG_ENABLED 1
#endif

#include <zephyr.h>
#include <errno.h>
#include <stdio.h>

#include <net/socket.h>

#include "common.h"

#define RECV_BUF_SIZE 128

static ssize_t sendall(int sock, const void *buf, size_t len)
{
	while (len) {
		ssize_t out_len = send(sock, buf, len, 0);

		if (out_len < 0) {
			return out_len;
		}
		buf = (const char *)buf + out_len;
		len -= out_len;
	}

	return 0;
}

static int send_tcp_data(struct data *data)
{
	int ret;

	do {
		data->tcp.expecting = sys_rand32_get() % ipsum_len;
	} while (data->tcp.expecting == 0);

	data->tcp.received = 0;

	ret =  sendall(data->tcp.sock, lorem_ipsum, data->tcp.expecting);

	if (ret < 0) {
		NET_ERR("%s TCP: Failed to send data, errno %d", data->proto,
			errno);
	} else {
		NET_DBG("%s TCP: Sent %d bytes", data->proto,
			data->tcp.expecting);
	}

	return ret;
}

static int compare_tcp_data(struct data *data, const char *buf, u32_t received)
{
	if (data->tcp.received + received > data->tcp.expecting) {
		NET_ERR("Too much data received: TCP %s", data->proto);
		return -EIO;
	}

	if (memcmp(buf, lorem_ipsum + data->tcp.received, received) != 0) {
		NET_ERR("Invalid data received: TCP %s", data->proto);
		return -EIO;
	}

	return 0;
}

static int start_tcp_proto(struct data *data, struct sockaddr *addr,
			   socklen_t addrlen)
{
	int ret;

	data->tcp.sock = socket(addr->sa_family, SOCK_STREAM, IPPROTO_TCP);
	if (data->tcp.sock < 0) {
		NET_ERR("Failed to create TCP socket (%s): %d", data->proto,
			errno);
		return -errno;
	}

	ret = connect(data->tcp.sock, addr, addrlen);
	if (ret < 0) {
		NET_ERR("Cannot connect to TCP remote (%s): %d", data->proto,
			errno);
		ret = -errno;
	}

	return ret;
}

static int process_tcp_proto(struct data *data)
{
	int ret, received;
	char buf[RECV_BUF_SIZE];

	do {
		received = recv(data->tcp.sock, buf, sizeof(buf), MSG_DONTWAIT);

		/* No data or error. */
		if (received == 0) {
			ret = -EIO;
			continue;
		} else if (received < 0) {
			if (errno == EAGAIN || errno == EWOULDBLOCK) {
				ret = 0;
			} else {
				ret = -errno;
			}
			continue;
		}

		ret = compare_tcp_data(data, buf, received);
		if (ret != 0) {
			break;
		}

		/* Successful comparison. */
		data->tcp.received += received;
		if (data->tcp.received < data->tcp.expecting) {
			continue;
		}

		/* Response complete */
		NET_DBG("%s TCP: Received and compared %d bytes, all ok",
			data->proto, data->tcp.received);


		if (++data->tcp.counter % 1000 == 0) {
			NET_INFO("%s TCP: Exchanged %u packets", data->proto,
				 data->tcp.counter);
		}

		ret = send_tcp_data(data);
		break;
	} while (received > 0);

	return ret;
}

int start_tcp(void)
{
	int ret = 0;
	struct sockaddr_in addr4;
	struct sockaddr_in6 addr6;

	if (IS_ENABLED(CONFIG_NET_IPV6)) {
		addr6.sin6_family = AF_INET6;
		addr6.sin6_port = htons(PEER_PORT);
		inet_pton(AF_INET6, CONFIG_NET_APP_PEER_IPV6_ADDR,
			  &addr6.sin6_addr);

		ret = start_tcp_proto(&conf.ipv6, (struct sockaddr *)&addr6,
				      sizeof(addr6));
		if (ret < 0) {
			return ret;
		}
	}

	if (IS_ENABLED(CONFIG_NET_IPV4)) {
		addr4.sin_family = AF_INET;
		addr4.sin_port = htons(PEER_PORT);
		inet_pton(AF_INET, CONFIG_NET_APP_PEER_IPV4_ADDR,
			  &addr4.sin_addr);

		ret = start_tcp_proto(&conf.ipv4, (struct sockaddr *)&addr4,
				      sizeof(addr4));
		if (ret < 0) {
			return ret;
		}
	}

	if (IS_ENABLED(CONFIG_NET_IPV6)) {
		ret = send_tcp_data(&conf.ipv6);
		if (ret < 0) {
			return ret;
		}
	}

	if (IS_ENABLED(CONFIG_NET_IPV4)) {
		ret = send_tcp_data(&conf.ipv4);
	}

	return ret;
}

int process_tcp(void)
{
	int ret = 0;

	if (IS_ENABLED(CONFIG_NET_IPV6)) {
		ret = process_tcp_proto(&conf.ipv6);
		if (ret < 0) {
			return ret;
		}
	}

	if (IS_ENABLED(CONFIG_NET_IPV4)) {
		ret = process_tcp_proto(&conf.ipv4);
		if (ret < 0) {
			return ret;
		}
	}

	return ret;
}

void stop_tcp(void)
{
	if (IS_ENABLED(CONFIG_NET_IPV6)) {
		if (conf.ipv6.tcp.sock > 0) {
			close(conf.ipv6.tcp.sock);
		}
	}

	if (IS_ENABLED(CONFIG_NET_IPV4)) {
		if (conf.ipv4.tcp.sock > 0) {
			close(conf.ipv4.tcp.sock);
		}
	}
}
