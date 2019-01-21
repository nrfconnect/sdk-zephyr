/*
 * Copyright (c) 2017 Linaro Limited
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* libc headers */
#include <fcntl.h>

/* Zephyr headers */
#include <logging/log.h>
LOG_MODULE_REGISTER(net_sock, CONFIG_NET_SOCKETS_LOG_LEVEL);

#include <kernel.h>
#include <net/net_context.h>
#include <net/net_pkt.h>
#include <net/socket.h>
#include <syscall_handler.h>
#include <misc/fdtable.h>

#include "sockets_internal.h"

#define SET_ERRNO(x) \
	{ int _err = x; if (_err < 0) { errno = -_err; return -1; } }

#define VTABLE_CALL(fn, sock, ...) \
	do { \
		const struct socket_op_vtable *vtable; \
		void *ctx = get_sock_vtable(sock, &vtable); \
		if (ctx == NULL) { \
			return -1; \
		} \
		return vtable->fn(ctx, __VA_ARGS__); \
	} while (0)

const struct socket_op_vtable sock_fd_op_vtable;

static inline void *get_sock_vtable(
			int sock, const struct socket_op_vtable **vtable)
{
	return z_get_fd_obj_and_vtable(sock,
				       (const struct fd_op_vtable **)vtable);
}

static void zsock_received_cb(struct net_context *ctx, struct net_pkt *pkt,
			      int status, void *user_data);

static inline int _k_fifo_wait_non_empty(struct k_fifo *fifo, int32_t timeout)
{
	struct k_poll_event events[] = {
		K_POLL_EVENT_INITIALIZER(K_POLL_TYPE_FIFO_DATA_AVAILABLE,
					 K_POLL_MODE_NOTIFY_ONLY, fifo),
	};

	return k_poll(events, ARRAY_SIZE(events), timeout);
}

static void zsock_flush_queue(struct net_context *ctx)
{
	bool is_listen = net_context_get_state(ctx) == NET_CONTEXT_LISTENING;
	void *p;

	/* recv_q and accept_q are shared via a union */
	while ((p = k_fifo_get(&ctx->recv_q, K_NO_WAIT)) != NULL) {
		if (is_listen) {
			NET_DBG("discarding ctx %p", p);
			net_context_put(p);
		} else {
			NET_DBG("discarding pkt %p", p);
			net_pkt_unref(p);
		}
	}

	/* Some threads might be waiting on recv, cancel the wait */
	k_fifo_cancel_wait(&ctx->recv_q);
}

int zsock_socket_internal(int family, int type, int proto)
{
	int fd = z_reserve_fd();
	struct net_context *ctx;
	int res;

	if (fd < 0) {
		return -1;
	}

	res = net_context_get(family, type, proto, &ctx);
	if (res < 0) {
		z_free_fd(fd);
		errno = -res;
		return -1;
	}

	/* Initialize user_data, all other calls will preserve it */
	ctx->user_data = NULL;

	/* recv_q and accept_q are in union */
	k_fifo_init(&ctx->recv_q);

#ifdef CONFIG_USERSPACE
	/* Set net context object as initialized and grant access to the
	 * calling thread (and only the calling thread)
	 */
	_k_object_recycle(ctx);
#endif

	z_finalize_fd(fd, ctx, (const struct fd_op_vtable *)&sock_fd_op_vtable);

	return fd;
}

int _impl_zsock_socket(int family, int type, int proto)
{
#if defined(CONFIG_NET_SOCKETS_SOCKOPT_TLS)
	if (((proto >= IPPROTO_TLS_1_0) && (proto <= IPPROTO_TLS_1_2)) ||
	    (proto >= IPPROTO_DTLS_1_0 && proto <= IPPROTO_DTLS_1_2)) {
		return ztls_socket(family, type, proto);
	}
#endif

	return zsock_socket_internal(family, type, proto);
}

#ifdef CONFIG_USERSPACE
Z_SYSCALL_HANDLER(zsock_socket, family, type, proto)
{
	/* implementation call to net_context_get() should do all necessary
	 * checking
	 */
	return _impl_zsock_socket(family, type, proto);
}
#endif /* CONFIG_USERSPACE */

int zsock_close_ctx(struct net_context *ctx)
{
#ifdef CONFIG_USERSPACE
	_k_object_uninit(ctx);
#endif
	/* Reset callbacks to avoid any race conditions while
	 * flushing queues. No need to check return values here,
	 * as these are fail-free operations and we're closing
	 * socket anyway.
	 */
	if (net_context_get_state(ctx) == NET_CONTEXT_LISTENING) {
		(void)net_context_accept(ctx, NULL, K_NO_WAIT, NULL);
	} else {
		(void)net_context_recv(ctx, NULL, K_NO_WAIT, NULL);
	}

	zsock_flush_queue(ctx);

	SET_ERRNO(net_context_put(ctx));

	return 0;
}

int _impl_zsock_close(int sock)
{
	const struct fd_op_vtable *vtable;
	void *ctx = z_get_fd_obj_and_vtable(sock, &vtable);

	if (ctx == NULL) {
		return -1;
	}

	z_free_fd(sock);

	return z_fdtable_call_ioctl(vtable, ctx, ZFD_IOCTL_CLOSE);
}

#ifdef CONFIG_USERSPACE
Z_SYSCALL_HANDLER(zsock_close, sock)
{
	return _impl_zsock_close(sock);
}
#endif /* CONFIG_USERSPACE */

static void zsock_accepted_cb(struct net_context *new_ctx,
			      struct sockaddr *addr, socklen_t addrlen,
			      int status, void *user_data) {
	struct net_context *parent = user_data;

	NET_DBG("parent=%p, ctx=%p, st=%d", parent, new_ctx, status);

	if (status == 0) {
		/* This just installs a callback, so cannot fail. */
		(void)net_context_recv(new_ctx, zsock_received_cb, K_NO_WAIT,
				       NULL);
		k_fifo_init(&new_ctx->recv_q);

		k_fifo_put(&parent->accept_q, new_ctx);
	}
}

static void zsock_received_cb(struct net_context *ctx, struct net_pkt *pkt,
			      int status, void *user_data) {
	unsigned int header_len;

	NET_DBG("ctx=%p, pkt=%p, st=%d, user_data=%p", ctx, pkt, status,
		user_data);

	/* if pkt is NULL, EOF */
	if (!pkt) {
		struct net_pkt *last_pkt = k_fifo_peek_tail(&ctx->recv_q);

		if (!last_pkt) {
			/* If there're no packets in the queue, recv() may
			 * be blocked waiting on it to become non-empty,
			 * so cancel that wait.
			 */
			sock_set_eof(ctx);
			k_fifo_cancel_wait(&ctx->recv_q);
			NET_DBG("Marked socket %p as peer-closed", ctx);
		} else {
			net_pkt_set_eof(last_pkt, true);
			NET_DBG("Set EOF flag on pkt %p", ctx);
		}
		return;
	}

	/* Normal packet */
	net_pkt_set_eof(pkt, false);

	if (net_context_get_type(ctx) == SOCK_STREAM) {
		/* TCP: we don't care about packet header, get rid of it asap.
		 * UDP: keep packet header to support recvfrom().
		 */
		header_len = net_pkt_appdata(pkt) - pkt->frags->data;
		net_buf_pull(pkt->frags, header_len);
		net_context_update_recv_wnd(ctx, -net_pkt_appdatalen(pkt));
	}

	k_fifo_put(&ctx->recv_q, pkt);
}

int zsock_bind_ctx(struct net_context *ctx, const struct sockaddr *addr,
		   socklen_t addrlen)
{
	SET_ERRNO(net_context_bind(ctx, addr, addrlen));
	/* For DGRAM socket, we expect to receive packets after call to
	 * bind(), but for STREAM socket, next expected operation is
	 * listen(), which doesn't work if recv callback is set.
	 */
	if (net_context_get_type(ctx) == SOCK_DGRAM) {
		SET_ERRNO(net_context_recv(ctx, zsock_received_cb, K_NO_WAIT,
					   ctx->user_data));
	}

	return 0;
}

int _impl_zsock_bind(int sock, const struct sockaddr *addr, socklen_t addrlen)
{
	VTABLE_CALL(bind, sock, addr, addrlen);
}

#ifdef CONFIG_USERSPACE
Z_SYSCALL_HANDLER(zsock_bind, sock, addr, addrlen)
{
	struct sockaddr_storage dest_addr_copy;

	Z_OOPS(Z_SYSCALL_VERIFY(addrlen <= sizeof(dest_addr_copy)));
	Z_OOPS(z_user_from_copy(&dest_addr_copy, (void *)addr, addrlen));

	return _impl_zsock_bind(sock, (struct sockaddr *)&dest_addr_copy,
				addrlen);
}
#endif /* CONFIG_USERSPACE */

int zsock_connect_ctx(struct net_context *ctx, const struct sockaddr *addr,
		      socklen_t addrlen)
{
	SET_ERRNO(net_context_connect(ctx, addr, addrlen, NULL, K_FOREVER,
				      NULL));
	SET_ERRNO(net_context_recv(ctx, zsock_received_cb, K_NO_WAIT,
				   ctx->user_data));

	return 0;
}

int _impl_zsock_connect(int sock, const struct sockaddr *addr,
			socklen_t addrlen)
{
	VTABLE_CALL(connect, sock, addr, addrlen);
}

#ifdef CONFIG_USERSPACE
Z_SYSCALL_HANDLER(zsock_connect, sock, addr, addrlen)
{
	struct sockaddr_storage dest_addr_copy;

	Z_OOPS(Z_SYSCALL_VERIFY(addrlen <= sizeof(dest_addr_copy)));
	Z_OOPS(z_user_from_copy(&dest_addr_copy, (void *)addr, addrlen));

	return _impl_zsock_connect(sock, (struct sockaddr *)&dest_addr_copy,
				   addrlen);
}
#endif /* CONFIG_USERSPACE */

int zsock_listen_ctx(struct net_context *ctx, int backlog)
{
	SET_ERRNO(net_context_listen(ctx, backlog));
	SET_ERRNO(net_context_accept(ctx, zsock_accepted_cb, K_NO_WAIT, ctx));

	return 0;
}

int _impl_zsock_listen(int sock, int backlog)
{
	VTABLE_CALL(listen, sock, backlog);
}

#ifdef CONFIG_USERSPACE
Z_SYSCALL_HANDLER(zsock_listen, sock, backlog)
{
	return _impl_zsock_listen(sock, backlog);
}
#endif /* CONFIG_USERSPACE */

int zsock_accept_ctx(struct net_context *parent, struct sockaddr *addr,
		     socklen_t *addrlen)
{
	int fd;

	fd = z_reserve_fd();
	if (fd < 0) {
		return -1;
	}

	struct net_context *ctx = k_fifo_get(&parent->accept_q, K_FOREVER);

#ifdef CONFIG_USERSPACE
	_k_object_recycle(ctx);
#endif

	if (addr != NULL && addrlen != NULL) {
		int len = min(*addrlen, sizeof(ctx->remote));

		memcpy(addr, &ctx->remote, len);
		/* addrlen is a value-result argument, set to actual
		 * size of source address
		 */
		if (ctx->remote.sa_family == AF_INET) {
			*addrlen = sizeof(struct sockaddr_in);
		} else if (ctx->remote.sa_family == AF_INET6) {
			*addrlen = sizeof(struct sockaddr_in6);
		} else {
			errno = ENOTSUP;
			return -1;
		}
	}

	z_finalize_fd(fd, ctx, (const struct fd_op_vtable *)&sock_fd_op_vtable);

	return fd;
}

int _impl_zsock_accept(int sock, struct sockaddr *addr, socklen_t *addrlen)
{
	VTABLE_CALL(accept, sock, addr, addrlen);
}

#ifdef CONFIG_USERSPACE
Z_SYSCALL_HANDLER(zsock_accept, sock, addr, addrlen)
{
	socklen_t addrlen_copy;
	int ret;

	Z_OOPS(z_user_from_copy(&addrlen_copy, (void *)addrlen,
			     sizeof(socklen_t)));

	if (Z_SYSCALL_MEMORY_WRITE(addr, addrlen_copy)) {
		errno = EFAULT;
		return -1;
	}

	ret = _impl_zsock_accept(sock, (struct sockaddr *)addr, &addrlen_copy);

	if (ret >= 0 &&
	    z_user_to_copy((void *)addrlen, &addrlen_copy,
			   sizeof(socklen_t))) {
		errno = EINVAL;
		return -1;
	}

	return ret;
}
#endif /* CONFIG_USERSPACE */

ssize_t zsock_sendto_ctx(struct net_context *ctx, const void *buf, size_t len,
			 int flags,
			 const struct sockaddr *dest_addr, socklen_t addrlen)
{
	int err;
	struct net_pkt *send_pkt;
	s32_t timeout = K_FOREVER;

	if ((flags & ZSOCK_MSG_DONTWAIT) || sock_is_nonblock(ctx)) {
		timeout = K_NO_WAIT;
	}

	send_pkt = net_pkt_get_tx(ctx, timeout);
	if (!send_pkt) {
		errno = EAGAIN;
		return -1;
	}

	len = net_pkt_append(send_pkt, len, buf, timeout);
	if (!len) {
		net_pkt_unref(send_pkt);
		errno = EAGAIN;
		return -1;
	}

	/* Register the callback before sending in order to receive the response
	 * from the peer.
	 */
	err = net_context_recv(ctx, zsock_received_cb, K_NO_WAIT, ctx->user_data);
	if (err < 0) {
		net_pkt_unref(send_pkt);
		errno = -err;
		return -1;
	}

	if (dest_addr) {
		err = net_context_sendto(send_pkt, dest_addr, addrlen, NULL,
					 timeout, NULL, ctx->user_data);
	} else {
		err = net_context_send(send_pkt, NULL, timeout, NULL, ctx->user_data);
	}

	if (err < 0) {
		net_pkt_unref(send_pkt);
		errno = -err;
		return -1;
	}

	return len;
}

ssize_t _impl_zsock_sendto(int sock, const void *buf, size_t len, int flags,
			   const struct sockaddr *dest_addr, socklen_t addrlen)
{
	VTABLE_CALL(sendto, sock, buf, len, flags, dest_addr, addrlen);
}

#ifdef CONFIG_USERSPACE
Z_SYSCALL_HANDLER(zsock_sendto, sock, buf, len, flags, dest_addr, addrlen)
{
	struct sockaddr_storage dest_addr_copy;

	Z_OOPS(Z_SYSCALL_MEMORY_READ(buf, len));
	if (dest_addr) {
		Z_OOPS(Z_SYSCALL_VERIFY(addrlen <= sizeof(dest_addr_copy)));
		Z_OOPS(z_user_from_copy(&dest_addr_copy, (void *)dest_addr,
					addrlen));
	}

	return _impl_zsock_sendto(sock, (const void *)buf, len, flags,
			dest_addr ? (struct sockaddr *)&dest_addr_copy : NULL,
			addrlen);
}
#endif /* CONFIG_USERSPACE */

static inline ssize_t zsock_recv_dgram(struct net_context *ctx,
				       void *buf,
				       size_t max_len,
				       int flags,
				       struct sockaddr *src_addr,
				       socklen_t *addrlen)
{
	size_t recv_len = 0;
	s32_t timeout = K_FOREVER;
	unsigned int header_len;
	struct net_pkt *pkt;

	if ((flags & ZSOCK_MSG_DONTWAIT) || sock_is_nonblock(ctx)) {
		timeout = K_NO_WAIT;
	}

	if (flags & ZSOCK_MSG_PEEK) {
		int res;

		res = _k_fifo_wait_non_empty(&ctx->recv_q, timeout);
		/* EAGAIN when timeout expired, EINTR when cancelled */
		if (res && res != -EAGAIN && res != -EINTR) {
			errno = -res;
			return -1;
		}

		pkt = k_fifo_peek_head(&ctx->recv_q);
	} else {
		pkt = k_fifo_get(&ctx->recv_q, timeout);
	}

	if (!pkt) {
		errno = EAGAIN;
		return -1;
	}

	if (src_addr && addrlen) {
		int rv;

		rv = net_pkt_get_src_addr(pkt, src_addr, *addrlen);
		if (rv < 0) {
			errno = rv;
			return -1;
		}

		/* addrlen is a value-result argument, set to actual
		 * size of source address
		 */
		if (src_addr->sa_family == AF_INET) {
			*addrlen = sizeof(struct sockaddr_in);
		} else if (src_addr->sa_family == AF_INET6) {
			*addrlen = sizeof(struct sockaddr_in6);
		} else {
			errno = ENOTSUP;
			return -1;
		}
	}

	/* Set starting point behind packet header since we've
	 * handled src addr and port.
	 */
	header_len = net_pkt_appdata(pkt) - pkt->frags->data;

	recv_len = net_pkt_appdatalen(pkt);
	if (recv_len > max_len) {
		recv_len = max_len;
	}

	/* Length passed as arguments are all based on packet data size
	 * and output buffer size, so return value is invariantly == recv_len,
	 * and we just ignore it.
	 */
	(void)net_frag_linearize(buf, recv_len, pkt, header_len, recv_len);

	if (!(flags & ZSOCK_MSG_PEEK)) {
		net_pkt_unref(pkt);
	}

	return recv_len;
}

static inline ssize_t zsock_recv_stream(struct net_context *ctx,
					void *buf,
					size_t max_len,
					int flags)
{
	size_t recv_len = 0;
	s32_t timeout = K_FOREVER;
	int res;

	if ((flags & ZSOCK_MSG_DONTWAIT) || sock_is_nonblock(ctx)) {
		timeout = K_NO_WAIT;
	}

	do {
		struct net_pkt *pkt;
		struct net_buf *frag;
		u32_t frag_len;

		if (sock_is_eof(ctx)) {
			return 0;
		}

		res = _k_fifo_wait_non_empty(&ctx->recv_q, timeout);
		/* EAGAIN when timeout expired, EINTR when cancelled */
		if (res && res != -EAGAIN && res != -EINTR) {
			errno = -res;
			return -1;
		}

		pkt = k_fifo_peek_head(&ctx->recv_q);
		if (!pkt) {
			/* Either timeout expired, or wait was cancelled
			 * due to connection closure by peer.
			 */
			NET_DBG("NULL return from fifo");
			if (sock_is_eof(ctx)) {
				return 0;
			} else {
				errno = EAGAIN;
				return -1;
			}
		}

		frag = pkt->frags;
		if (!frag) {
			NET_ERR("net_pkt has empty fragments on start!");
			errno = EAGAIN;
			return -1;
		}

		frag_len = frag->len;
		recv_len = frag_len;
		if (recv_len > max_len) {
			recv_len = max_len;
		}

		/* Actually copy data to application buffer */
		memcpy(buf, frag->data, recv_len);

		if (!(flags & ZSOCK_MSG_PEEK)) {
			if (recv_len != frag_len) {
				net_buf_pull(frag, recv_len);
			} else {
				frag = net_pkt_frag_del(pkt, NULL, frag);
				if (!frag) {
					/* Finished processing head pkt in
					 * the fifo. Drop it from there.
					 */
					k_fifo_get(&ctx->recv_q, K_NO_WAIT);
					if (net_pkt_eof(pkt)) {
						sock_set_eof(ctx);
					}

					net_pkt_unref(pkt);
				}
			}
		}
	} while (recv_len == 0);

	if (!(flags & ZSOCK_MSG_PEEK)) {
		net_context_update_recv_wnd(ctx, recv_len);
	}

	return recv_len;
}

ssize_t zsock_recvfrom_ctx(struct net_context *ctx, void *buf, size_t max_len,
			   int flags,
			   struct sockaddr *src_addr, socklen_t *addrlen)
{
	enum net_sock_type sock_type = net_context_get_type(ctx);

	if (sock_type == SOCK_DGRAM) {
		return zsock_recv_dgram(ctx, buf, max_len, flags, src_addr, addrlen);
	} else if (sock_type == SOCK_STREAM) {
		return zsock_recv_stream(ctx, buf, max_len, flags);
	} else {
		__ASSERT(0, "Unknown socket type");
	}

	return 0;
}

ssize_t _impl_zsock_recvfrom(int sock, void *buf, size_t max_len, int flags,
			     struct sockaddr *src_addr, socklen_t *addrlen)
{
	VTABLE_CALL(recvfrom, sock, buf, max_len, flags, src_addr, addrlen);
}

#ifdef CONFIG_USERSPACE
Z_SYSCALL_HANDLER(zsock_recvfrom, sock, buf, max_len, flags, src_addr,
		  addrlen_param)
{
	socklen_t addrlen_copy;
	socklen_t *addrlen_ptr = (socklen_t *)addrlen_param;
	ssize_t ret;

	if (Z_SYSCALL_MEMORY_WRITE(buf, max_len)) {
		errno = EFAULT;
		return -1;
	}

	if (addrlen_param) {
		Z_OOPS(z_user_from_copy(&addrlen_copy,
					(socklen_t *)addrlen_param,
					sizeof(socklen_t)));
	}
	Z_OOPS(src_addr && Z_SYSCALL_MEMORY_WRITE(src_addr, addrlen_copy));

	ret = _impl_zsock_recvfrom(sock, (void *)buf, max_len, flags,
				   (struct sockaddr *)src_addr,
				   addrlen_param ? &addrlen_copy : NULL);

	if (addrlen_param) {
		Z_OOPS(z_user_to_copy(addrlen_ptr, &addrlen_copy,
				      sizeof(socklen_t)));
	}

	return ret;
}
#endif /* CONFIG_USERSPACE */

/* As this is limited function, we don't follow POSIX signature, with
 * "..." instead of last arg.
 */
int _impl_zsock_fcntl(int sock, int cmd, int flags)
{
	const struct fd_op_vtable *vtable;
	void *obj;

	obj = z_get_fd_obj_and_vtable(sock, &vtable);
	if (obj == NULL) {
		return -1;
	}

	return z_fdtable_call_ioctl(vtable, obj, cmd, flags);
}

#ifdef CONFIG_USERSPACE
Z_SYSCALL_HANDLER(zsock_fcntl, sock, cmd, flags)
{
	return _impl_zsock_fcntl(sock, cmd, flags);
}
#endif

static int zsock_poll_prepare_ctx(struct net_context *ctx,
				  struct zsock_pollfd *pfd,
				  struct k_poll_event **pev,
				  struct k_poll_event *pev_end)
{
	if (pfd->events & ZSOCK_POLLIN) {
		if (*pev == pev_end) {
			errno = ENOMEM;
			return -1;
		}

		(*pev)->obj = &ctx->recv_q;
		(*pev)->type = K_POLL_TYPE_FIFO_DATA_AVAILABLE;
		(*pev)->mode = K_POLL_MODE_NOTIFY_ONLY;
		(*pev)->state = K_POLL_STATE_NOT_READY;
		(*pev)++;
	}

	return 0;
}

static int zsock_poll_update_ctx(struct net_context *ctx,
				 struct zsock_pollfd *pfd,
				 struct k_poll_event **pev)
{
	ARG_UNUSED(ctx);

	/* For now, assume that socket is always writable */
	if (pfd->events & ZSOCK_POLLOUT) {
		pfd->revents |= ZSOCK_POLLOUT;
	}

	if (pfd->events & ZSOCK_POLLIN) {
		if ((*pev)->state != K_POLL_STATE_NOT_READY) {
			pfd->revents |= ZSOCK_POLLIN;
		}
		(*pev)++;
	}

	return 0;
}

static inline int time_left(u32_t start, u32_t timeout)
{
	u32_t elapsed = k_uptime_get_32() - start;

	return timeout - elapsed;
}

int _impl_zsock_poll(struct zsock_pollfd *fds, int nfds, int timeout)
{
	bool retry;
	int ret = 0;
	int i, remaining_time;
	struct zsock_pollfd *pfd;
	struct k_poll_event poll_events[CONFIG_NET_SOCKETS_POLL_MAX];
	struct k_poll_event *pev;
	struct k_poll_event *pev_end = poll_events + ARRAY_SIZE(poll_events);
	const struct fd_op_vtable *vtable;
	u32_t entry_time = k_uptime_get_32();

	if (timeout < 0) {
		timeout = K_FOREVER;
	}

	pev = poll_events;
	for (pfd = fds, i = nfds; i--; pfd++) {
		struct net_context *ctx;

		/* Per POSIX, negative fd's are just ignored */
		if (pfd->fd < 0) {
			continue;
		}

		ctx = z_get_fd_obj_and_vtable(pfd->fd, &vtable);
		if (ctx == NULL) {
			/* Will set POLLNVAL in return loop */
			continue;
		}

		if (z_fdtable_call_ioctl(vtable, ctx, ZFD_IOCTL_POLL_PREPARE,
					 pfd, &pev, pev_end) < 0) {
			if (errno == EALREADY) {
				timeout = K_NO_WAIT;
				continue;
			}

			return -1;
		}
	}

	remaining_time = timeout;

	do {
		ret = k_poll(poll_events, pev - poll_events, remaining_time);
		/* EAGAIN when timeout expired, EINTR when cancelled (i.e. EOF) */
		if (ret != 0 && ret != -EAGAIN && ret != -EINTR) {
			errno = -ret;
			return -1;
		}

		retry = false;
		ret = 0;

		pev = poll_events;
		for (pfd = fds, i = nfds; i--; pfd++) {
			struct net_context *ctx;

			pfd->revents = 0;

			if (pfd->fd < 0) {
				continue;
			}

			ctx = z_get_fd_obj_and_vtable(pfd->fd, &vtable);
			if (ctx == NULL) {
				pfd->revents = ZSOCK_POLLNVAL;
				ret++;
				continue;
			}

			if (z_fdtable_call_ioctl(vtable, ctx, ZFD_IOCTL_POLL_UPDATE,
						 pfd, &pev) < 0) {
				if (errno == EAGAIN) {
					retry = true;
					continue;
				}

				return -1;
			}

			if (pfd->revents != 0) {
				ret++;
			}
		}

		if (retry) {
			if (ret > 0) {
				break;
			}

			if (timeout == K_NO_WAIT) {
				break;
			}

			if (timeout != K_FOREVER) {
				/* Recalculate the timeout value. */
				remaining_time = time_left(entry_time, timeout);
				if (remaining_time <= 0) {
					break;
				}
			}
		}
	} while (retry);

	return ret;
}

#ifdef CONFIG_USERSPACE
Z_SYSCALL_HANDLER(zsock_poll, fds, nfds, timeout)
{
	struct zsock_pollfd *fds_copy;
	unsigned int fds_size;
	int ret;

	/* Copy fds array from user mode */
	if (__builtin_umul_overflow(nfds, sizeof(struct zsock_pollfd),
				    &fds_size)) {
		errno = EFAULT;
		return -1;
	}
	fds_copy = z_user_alloc_from_copy((void *)fds, fds_size);
	if (!fds_copy) {
		errno = ENOMEM;
		return -1;
	}

	ret = _impl_zsock_poll(fds_copy, nfds, timeout);

	if (ret >= 0) {
		z_user_to_copy((void *)fds, fds_copy, fds_size);
	}
	k_free(fds_copy);

	return ret;
}
#endif

int _impl_zsock_inet_pton(sa_family_t family, const char *src, void *dst)
{
	if (net_addr_pton(family, src, dst) == 0) {
		return 1;
	} else {
		return 0;
	}
}

#ifdef CONFIG_USERSPACE
Z_SYSCALL_HANDLER(zsock_inet_pton, family, src, dst)
{
	int dst_size;
	char src_copy[NET_IPV6_ADDR_LEN];
	char dst_copy[sizeof(struct in6_addr)];
	int ret;

	switch (family) {
	case AF_INET:
		dst_size = sizeof(struct in_addr);
		break;
	case AF_INET6:
		dst_size = sizeof(struct in6_addr);
		break;
	default:
		errno = EAFNOSUPPORT;
		return -1;
	}

	Z_OOPS(z_user_string_copy(src_copy, (char *)src, sizeof(src_copy)));
	ret = _impl_zsock_inet_pton(family, src_copy, dst_copy);
	Z_OOPS(z_user_to_copy((void *)dst, dst_copy, dst_size));

	return ret;
}
#endif

int zsock_getsockopt_ctx(struct net_context *ctx, int level, int optname,
			 void *optval, socklen_t *optlen)
{
	errno = ENOPROTOOPT;
	return -1;
}

int zsock_getsockopt(int sock, int level, int optname,
		     void *optval, socklen_t *optlen)
{
	VTABLE_CALL(getsockopt, sock, level, optname, optval, optlen);
}

int zsock_setsockopt_ctx(struct net_context *ctx, int level, int optname,
			 const void *optval, socklen_t optlen)
{
	errno = ENOPROTOOPT;
	return -1;
}

int zsock_setsockopt(int sock, int level, int optname,
		     const void *optval, socklen_t optlen)
{
	VTABLE_CALL(setsockopt, sock, level, optname, optval, optlen);
}

static ssize_t sock_read_vmeth(void *obj, void *buffer, size_t count)
{
	return zsock_recvfrom_ctx(obj, buffer, count, 0, NULL, 0);
}

static ssize_t sock_write_vmeth(void *obj, const void *buffer, size_t count)
{
	return zsock_sendto_ctx(obj, buffer, count, 0, NULL, 0);
}

static int sock_ioctl_vmeth(void *obj, unsigned int request, va_list args)
{
	switch (request) {

	/* In Zephyr, fcntl() is just an alias of ioctl(). */
	case F_GETFL:
		if (sock_is_nonblock(obj)) {
		    return O_NONBLOCK;
		}

		return 0;

	case F_SETFL: {
		int flags;

		flags = va_arg(args, int);

		if (flags & O_NONBLOCK) {
			sock_set_flag(obj, SOCK_NONBLOCK, SOCK_NONBLOCK);
		} else {
			sock_set_flag(obj, SOCK_NONBLOCK, 0);
		}

		return 0;
	}

	case ZFD_IOCTL_CLOSE:
		return zsock_close_ctx(obj);

	case ZFD_IOCTL_POLL_PREPARE: {
		struct zsock_pollfd *pfd;
		struct k_poll_event **pev;
		struct k_poll_event *pev_end;

		pfd = va_arg(args, struct zsock_pollfd *);
		pev = va_arg(args, struct k_poll_event **);
		pev_end = va_arg(args, struct k_poll_event *);

		return zsock_poll_prepare_ctx(obj, pfd, pev, pev_end);
	}

	case ZFD_IOCTL_POLL_UPDATE: {
		struct zsock_pollfd *pfd;
		struct k_poll_event **pev;

		pfd = va_arg(args, struct zsock_pollfd *);
		pev = va_arg(args, struct k_poll_event **);

		return zsock_poll_update_ctx(obj, pfd, pev);
	}

	default:
		errno = EOPNOTSUPP;
		return -1;
	}
}

static int sock_bind_vmeth(void *obj, const struct sockaddr *addr,
			   socklen_t addrlen)
{
	return zsock_bind_ctx(obj, addr, addrlen);
}

static int sock_connect_vmeth(void *obj, const struct sockaddr *addr,
			      socklen_t addrlen)
{
	return zsock_connect_ctx(obj, addr, addrlen);
}

static int sock_listen_vmeth(void *obj, int backlog)
{
	return zsock_listen_ctx(obj, backlog);
}

static int sock_accept_vmeth(void *obj, struct sockaddr *addr,
			     socklen_t *addrlen)
{
	return zsock_accept_ctx(obj, addr, addrlen);
}

static ssize_t sock_sendto_vmeth(void *obj, const void *buf, size_t len,
				 int flags, const struct sockaddr *dest_addr,
				 socklen_t addrlen)
{
	return zsock_sendto_ctx(obj, buf, len, flags, dest_addr, addrlen);
}

static ssize_t sock_recvfrom_vmeth(void *obj, void *buf, size_t max_len,
				   int flags, struct sockaddr *src_addr,
				   socklen_t *addrlen)
{
	return zsock_recvfrom_ctx(obj, buf, max_len, flags,
				  src_addr, addrlen);
}

static int sock_getsockopt_vmeth(void *obj, int level, int optname,
				 void *optval, socklen_t *optlen)
{
	return zsock_getsockopt_ctx(obj, level, optname, optval, optlen);
}

static int sock_setsockopt_vmeth(void *obj, int level, int optname,
				 const void *optval, socklen_t optlen)
{
	return zsock_setsockopt_ctx(obj, level, optname, optval, optlen);
}


const struct socket_op_vtable sock_fd_op_vtable = {
	.fd_vtable = {
		.read = sock_read_vmeth,
		.write = sock_write_vmeth,
		.ioctl = sock_ioctl_vmeth,
	},
	.bind = sock_bind_vmeth,
	.connect = sock_connect_vmeth,
	.listen = sock_listen_vmeth,
	.accept = sock_accept_vmeth,
	.sendto = sock_sendto_vmeth,
	.recvfrom = sock_recvfrom_vmeth,
	.getsockopt = sock_getsockopt_vmeth,
	.setsockopt = sock_setsockopt_vmeth,
};
