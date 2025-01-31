/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/logging/log.h>
#include <stubs.h>

LOG_MODULE_DECLARE(coap_client_test, LOG_LEVEL_DBG);

DEFINE_FAKE_VALUE_FUNC(uint32_t, z_impl_sys_rand32_get);
DEFINE_FAKE_VALUE_FUNC(ssize_t, z_impl_zsock_recvfrom, int, void *, size_t, int, struct sockaddr *,
		       socklen_t *);
DEFINE_FAKE_VALUE_FUNC(ssize_t, z_impl_zsock_sendto, int, void *, size_t, int,
		       const struct sockaddr *, socklen_t);

struct zvfs_pollfd {
	int fd;
	short events;
	short revents;
};

static short my_events[NUM_FD];

void set_socket_events(int fd, short events)
{
	__ASSERT_NO_MSG(fd < NUM_FD);
	my_events[fd] |= events;
}

void clear_socket_events(int fd, short events)
{
	my_events[fd] &= ~events;
}

int z_impl_zvfs_poll(struct zvfs_pollfd *fds, int nfds, int poll_timeout)
{
	int events = 0;
	k_sleep(K_MSEC(1));
	for (int i = 0; i < nfds; i++) {
		fds[i].revents =
			my_events[fds[i].fd] & (fds[i].events | ZSOCK_POLLERR | ZSOCK_POLLHUP);
		if (fds[i].revents) {
			events++;
		}
	}
	if (events == 0) {
		k_sleep(K_MSEC(poll_timeout));
	}

	return events;
}
