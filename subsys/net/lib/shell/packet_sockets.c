/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(net_shell);

#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <zephyr/net/socket.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/ethernet.h>

#include "net_shell_private.h"

#if defined(CONFIG_NET_SOCKETS_PACKET)

static int raw_sockfd = -1;
static const struct shell *raw_shell;
static struct sockaddr_ll raw_sockaddr;
static bool raw_socket_bound = false;

/* Buffer for receiving packets */
static uint8_t raw_recv_buffer[1500];

static void raw_rcvd_cb(struct k_work *work)
{
	ARG_UNUSED(work);

	ssize_t len;
	struct sockaddr_ll src_addr;
	socklen_t addr_len = sizeof(src_addr);

	if (raw_sockfd < 0) {
		return;
	}

	len = recvfrom(raw_sockfd, raw_recv_buffer, sizeof(raw_recv_buffer), MSG_DONTWAIT,
		       (struct sockaddr *)&src_addr, &addr_len);

	if (len > 0) {
		PR_SHELL(raw_shell, "Received raw packet (%d bytes): ", len);
		for (int i = 0; i < len && i < 64; i++) {
			PR_SHELL(raw_shell, "%02x ", raw_recv_buffer[i]);
			if ((i + 1) % 16 == 0) {
				PR_SHELL(raw_shell, "\n");
			}
		}
		if (len > 64) {
			PR_SHELL(raw_shell, "... (truncated)\n");
		} else {
			PR_SHELL(raw_shell, "\n");
		}
	}
}

static K_WORK_DEFINE(raw_recv_work, raw_rcvd_cb);

#endif

static int cmd_net_raw_bind(const struct shell *sh, size_t argc, char *argv[])
{
#if !defined(CONFIG_NET_SOCKETS_PACKET)
	ARG_UNUSED(sh);
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	PR_WARNING("Raw packet sockets not supported\n");
	return -EOPNOTSUPP;
#else
	struct net_if *iface = NULL;
	char *iface_arg = NULL;
	int ret;

	if (raw_sockfd >= 0) {
		PR_WARNING("Raw socket already bound\n");
		return -EALREADY;
	}

	/* Optional interface parameter (name or index) */
	if (argc > 1) {
		iface_arg = argv[1];
		char *endptr;
		int iface_idx = strtol(iface_arg, &endptr, 10);

		if (*endptr == '\0' && iface_idx > 0) {
			iface = net_if_get_by_index(iface_idx);
		} else {
			PR_WARNING("Invalid interface index: %s\n", iface_arg);
			return -EINVAL;
		}
	} else {
		/* Use default interface */
		iface = net_if_get_default();
	}

	if (!iface) {
		PR_WARNING("No network interface available\n");
		return -ENODEV;
	}

	/* Create raw packet socket */
	raw_sockfd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
	if (raw_sockfd < 0) {
		PR_WARNING("Cannot create raw socket (%d)\n", errno);
		return -errno;
	}

	raw_shell = sh;

	/* Set up sockaddr_ll structure */
	memset(&raw_sockaddr, 0, sizeof(raw_sockaddr));
	raw_sockaddr.sll_family = AF_PACKET;
	raw_sockaddr.sll_ifindex = net_if_get_by_iface(iface);
	raw_sockaddr.sll_protocol = htons(ETH_P_ALL);

	/* Bind the socket to the interface */
	ret = bind(raw_sockfd, (struct sockaddr *)&raw_sockaddr, sizeof(raw_sockaddr));
	if (ret < 0) {
		PR_WARNING("Binding raw socket failed (%d)\n", errno);
		close(raw_sockfd);
		raw_sockfd = -1;
		return -errno;
	}

	raw_socket_bound = true;
	PR_SHELL(sh, "Raw socket bound to interface %d\n", raw_sockaddr.sll_ifindex);

	return 0;
#endif
}

static int cmd_net_raw_close(const struct shell *sh, size_t argc, char *argv[])
{
#if !defined(CONFIG_NET_SOCKETS_PACKET)
	ARG_UNUSED(sh);
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	return -EOPNOTSUPP;
#else
	if (raw_sockfd < 0) {
		PR_WARNING("No raw socket to close\n");
		return -EINVAL;
	}

	close(raw_sockfd);
	raw_sockfd = -1;
	raw_socket_bound = false;

	PR_SHELL(sh, "Raw socket closed\n");
	return 0;
#endif
}

static int cmd_net_raw_send(const struct shell *sh, size_t argc, char *argv[])
{
#if !defined(CONFIG_NET_SOCKETS_PACKET)
	ARG_UNUSED(sh);
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	return -EOPNOTSUPP;
#else
	char *dst_mac_str = NULL;
	char *payload_str = NULL;
	uint8_t dst_mac[6];
	uint8_t *payload = NULL;
	size_t payload_len;
	ssize_t sent_bytes;
	int ret;

	if (argc < 3) {
		PR_WARNING("Usage: net raw send <dst_mac> <payload_hex>\n");
		PR_WARNING("Example: net raw send ff:ff:ff:ff:ff:ff 48656c6c6f\n");
		return -EINVAL;
	}

	if (raw_sockfd < 0 || !raw_socket_bound) {
		PR_WARNING("Raw socket not bound. Use 'net raw bind' first\n");
		return -EINVAL;
	}

	dst_mac_str = argv[1];
	payload_str = argv[2];

	/* Parse destination MAC address */
	ret = sscanf(dst_mac_str, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", &dst_mac[0], &dst_mac[1],
		     &dst_mac[2], &dst_mac[3], &dst_mac[4], &dst_mac[5]);
	if (ret != 6) {
		PR_WARNING("Invalid MAC address format. Use xx:xx:xx:xx:xx:xx\n");
		return -EINVAL;
	}

	/* Parse hex payload */
	payload_len = strlen(payload_str) / 2;
	if (payload_len == 0 || (strlen(payload_str) % 2) != 0) {
		PR_WARNING("Invalid hex payload. Must be even number of hex digits\n");
		return -EINVAL;
	}

	payload = malloc(payload_len);
	if (!payload) {
		PR_WARNING("Cannot allocate memory for payload\n");
		return -ENOMEM;
	}

	/* Convert hex string to bytes */
	for (size_t i = 0; i < payload_len; i++) {
		char hex_byte[3] = {payload_str[i * 2], payload_str[i * 2 + 1], '\0'};
		payload[i] = (uint8_t)strtol(hex_byte, NULL, 16);
	}

	/* Update destination address in sockaddr_ll */
	memcpy(raw_sockaddr.sll_addr, dst_mac, 6);
	raw_sockaddr.sll_halen = 6;

	/* Send the packet */
	sent_bytes = sendto(raw_sockfd, payload, payload_len, 0, (struct sockaddr *)&raw_sockaddr,
			    sizeof(raw_sockaddr));

	free(payload);

	if (sent_bytes < 0) {
		PR_WARNING("Sending raw packet failed (%d)\n", errno);
		return -errno;
	}

	PR_SHELL(sh, "Sent %d bytes to %02x:%02x:%02x:%02x:%02x:%02x\n", (int)sent_bytes,
		 dst_mac[0], dst_mac[1], dst_mac[2], dst_mac[3], dst_mac[4], dst_mac[5]);

	return 0;
#endif
}

static int cmd_net_raw_recv(const struct shell *sh, size_t argc, char *argv[])
{
#if !defined(CONFIG_NET_SOCKETS_PACKET)
	ARG_UNUSED(sh);
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	return -EOPNOTSUPP;
#else
	if (raw_sockfd < 0 || !raw_socket_bound) {
		PR_WARNING("Raw socket not bound. Use 'net raw bind' first\n");
		return -EINVAL;
	}

	raw_shell = sh;

	/* Trigger a receive attempt */
	k_work_submit(&raw_recv_work);

	PR_SHELL(sh, "Listening for raw packets... (use Ctrl+C to stop)\n");
	return 0;
#endif
}

static int cmd_net_raw_status(const struct shell *sh, size_t argc, char *argv[])
{
#if !defined(CONFIG_NET_SOCKETS_PACKET)
	ARG_UNUSED(sh);
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	return -EOPNOTSUPP;
#else
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	if (raw_sockfd < 0) {
		PR_SHELL(sh, "Raw socket: Not created\n");
	} else if (!raw_socket_bound) {
		PR_SHELL(sh, "Raw socket: Created but not bound\n");
	} else {
		PR_SHELL(sh, "Raw socket: Bound to interface %d\n", raw_sockaddr.sll_ifindex);
	}

	return 0;
#endif
}

static int cmd_net_raw(const struct shell *sh, size_t argc, char *argv[])
{
	ARG_UNUSED(sh);
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(
	net_cmd_raw,
	SHELL_CMD(bind, NULL, "'net raw bind [interface_index]' binds to raw packet socket.",
		  cmd_net_raw_bind),
	SHELL_CMD(close, NULL, "'net raw close' closes the raw socket.", cmd_net_raw_close),
	SHELL_CMD(send, NULL,
		  "'net raw send <dst_mac> <payload_hex>' "
		  "sends raw packet to MAC address.",
		  cmd_net_raw_send),
	SHELL_CMD(recv, NULL, "'net raw recv' starts receiving raw packets.", cmd_net_raw_recv),
	SHELL_CMD(status, NULL, "'net raw status' shows raw socket status.", cmd_net_raw_status),
	SHELL_SUBCMD_SET_END);

SHELL_SUBCMD_ADD((net), raw, &net_cmd_raw, "Raw packet socket operations", cmd_net_raw, 1, 0);
