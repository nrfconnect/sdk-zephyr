/*
 * Copyright (c) 2021 Nordic Semiconductor
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <logging/log.h>
LOG_MODULE_REGISTER(net_test, CONFIG_NET_SOCKETS_LOG_LEVEL);

#include <ztest_assert.h>
#include <fcntl.h>
#include <net/socket.h>
#include <net/tls_credentials.h>

#include "../../socket_helpers.h"

#if defined(CONFIG_MBEDTLS_USE_PSA_CRYPTO)
#include <psa/crypto.h>
#endif /* CONFIG_MBEDTLS_USE_PSA_CRYPTO */

#define TEST_STR_SMALL "test"

#define ANY_PORT 0
#define SERVER_PORT 4242

#define PSK_TAG 1

#define MAX_CONNS 5

#define TCP_TEARDOWN_TIMEOUT K_SECONDS(1)

static const unsigned char psk[] = {
	0x01, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
	0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f
};
static const char psk_id[] = "test_identity";

#if defined(CONFIG_MBEDTLS_USE_PSA_CRYPTO)
static psa_key_id_t psk_opaque_key;

static void test_import_psk_psa(void)
{
	psa_key_attributes_t key_attributes = psa_key_attributes_init();

	psa_set_key_usage_flags(&key_attributes, PSA_KEY_USAGE_DERIVE);
	psa_set_key_algorithm(&key_attributes,
			      PSA_ALG_TLS12_PSK_TO_MS(PSA_ALG_SHA_256));
	psa_set_key_type(&key_attributes,
			 PSA_KEY_TYPE_DERIVE);

	zassert_equal(psa_crypto_init(), PSA_SUCCESS,
		      "Failed to initialize PSA Crypto %d");

	psa_destroy_key(psk_opaque_key);

	zassert_equal(psa_import_key(&key_attributes, psk, sizeof(psk),
		      &psk_opaque_key), PSA_SUCCESS,
		      "Failed to import PSK %d");
}
#endif

static void test_config_psk(int s_sock, int c_sock)
{
	sec_tag_t sec_tag_list[] = {
		PSK_TAG
	};

#if defined(CONFIG_MBEDTLS_USE_PSA_CRYPTO)
	(void)tls_credential_delete(PSK_TAG, TLS_CREDENTIAL_OPAQUE_PSK);
#else
	(void)tls_credential_delete(PSK_TAG, TLS_CREDENTIAL_PSK);
#endif
	(void)tls_credential_delete(PSK_TAG, TLS_CREDENTIAL_PSK_ID);

#if defined(CONFIG_MBEDTLS_USE_PSA_CRYPTO)
	test_import_psk_psa();

	zassert_equal(tls_credential_add(PSK_TAG, TLS_CREDENTIAL_OPAQUE_PSK,
					 &psk_opaque_key, sizeof(psa_key_id_t)),
		      0, "Failed to register PSK Opaque %d");
#else
	zassert_equal(tls_credential_add(PSK_TAG, TLS_CREDENTIAL_PSK,
					 psk, sizeof(psk)),
		      0, "Failed to register PSK %d");
#endif
	zassert_equal(tls_credential_add(PSK_TAG, TLS_CREDENTIAL_PSK_ID,
					 psk_id, strlen(psk_id)),
		      0, "Failed to register PSK ID");

	zassert_equal(setsockopt(s_sock, SOL_TLS, TLS_SEC_TAG_LIST,
				 sec_tag_list, sizeof(sec_tag_list)),
		      0, "Failed to set PSK on server socket");
	zassert_equal(setsockopt(c_sock, SOL_TLS, TLS_SEC_TAG_LIST,
				 sec_tag_list, sizeof(sec_tag_list)),
		      0, "Failed to set PSK on client socket");
}

static void test_bind(int sock, struct sockaddr *addr, socklen_t addrlen)
{
	zassert_equal(bind(sock, addr, addrlen),
		      0,
		      "bind failed");
}

static void test_listen(int sock)
{
	zassert_equal(listen(sock, MAX_CONNS),
		      0,
		      "listen failed");
}

static void test_connect(int sock, struct sockaddr *addr, socklen_t addrlen)
{
	k_yield();

	zassert_equal(connect(sock, addr, addrlen),
		      0,
		      "connect failed");

	if (IS_ENABLED(CONFIG_NET_TC_THREAD_PREEMPTIVE)) {
		/* Let the connection proceed */
		k_yield();
	}
}

static void test_send(int sock, const void *buf, size_t len, int flags)
{
	zassert_equal(send(sock, buf, len, flags),
		      len,
		      "send failed");
}

static void test_accept(int sock, int *new_sock, struct sockaddr *addr,
			socklen_t *addrlen)
{
	zassert_not_null(new_sock, "null newsock");

	*new_sock = accept(sock, addr, addrlen);
	zassert_true(*new_sock >= 0, "accept failed");
}

static void test_close(int sock)
{
	zassert_equal(close(sock),
		      0,
		      "close failed");
}

#define CLIENT_CONNECT_STACK_SIZE 2048

/* Helper thread for the connect operation - need to handle client/server in
 * parallell due to handshake.
 */
struct k_thread client_connect_thread;
K_THREAD_STACK_DEFINE(client_connect_stack, CLIENT_CONNECT_STACK_SIZE);

static void client_connect_entry(void *p1, void *p2, void *p3)
{
	int sock = POINTER_TO_INT(p1);
	struct sockaddr *addr = p2;

	test_connect(sock, addr, addr->sa_family == AF_INET ?
		     sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6));
}

static void spawn_client_connect_thread(int sock, struct sockaddr *addr)
{
	k_thread_create(&client_connect_thread, client_connect_stack,
			K_THREAD_STACK_SIZEOF(client_connect_stack),
			client_connect_entry, INT_TO_POINTER(sock), addr, NULL,
			K_LOWEST_APPLICATION_THREAD_PRIO, 0, K_NO_WAIT);

	k_thread_start(&client_connect_thread);
}

void test_so_type(void)
{
	struct sockaddr_in bind_addr4;
	struct sockaddr_in6 bind_addr6;
	int sock1, sock2, rv;
	int optval;
	socklen_t optlen = sizeof(optval);

	prepare_sock_tls_v4(CONFIG_NET_CONFIG_MY_IPV4_ADDR, ANY_PORT,
			    &sock1, &bind_addr4, IPPROTO_TLS_1_2);
	prepare_sock_tls_v6(CONFIG_NET_CONFIG_MY_IPV6_ADDR, ANY_PORT,
			    &sock2, &bind_addr6, IPPROTO_TLS_1_2);

	rv = getsockopt(sock1, SOL_SOCKET, SO_TYPE, &optval, &optlen);
	zassert_equal(rv, 0, "getsockopt failed (%d)", errno);
	zassert_equal(optval, SOCK_STREAM, "getsockopt got invalid type");
	zassert_equal(optlen, sizeof(optval), "getsockopt got invalid size");

	rv = getsockopt(sock2, SOL_SOCKET, SO_TYPE, &optval, &optlen);
	zassert_equal(rv, 0, "getsockopt failed (%d)", errno);
	zassert_equal(optval, SOCK_STREAM, "getsockopt got invalid type");
	zassert_equal(optlen, sizeof(optval), "getsockopt got invalid size");

	test_close(sock1);
	test_close(sock2);
	k_sleep(TCP_TEARDOWN_TIMEOUT);
}

void test_so_protocol(void)
{
	struct sockaddr_in bind_addr4;
	struct sockaddr_in6 bind_addr6;
	int sock1, sock2, rv;
	int optval;
	socklen_t optlen = sizeof(optval);

	prepare_sock_tls_v4(CONFIG_NET_CONFIG_MY_IPV4_ADDR, ANY_PORT,
			    &sock1, &bind_addr4, IPPROTO_TLS_1_2);
	prepare_sock_tls_v6(CONFIG_NET_CONFIG_MY_IPV6_ADDR, ANY_PORT,
			    &sock2, &bind_addr6, IPPROTO_TLS_1_1);

	rv = getsockopt(sock1, SOL_SOCKET, SO_PROTOCOL, &optval, &optlen);
	zassert_equal(rv, 0, "getsockopt failed (%d)", errno);
	zassert_equal(optval, IPPROTO_TLS_1_2,
		      "getsockopt got invalid protocol");
	zassert_equal(optlen, sizeof(optval), "getsockopt got invalid size");

	rv = getsockopt(sock2, SOL_SOCKET, SO_PROTOCOL, &optval, &optlen);
	zassert_equal(rv, 0, "getsockopt failed (%d)", errno);
	zassert_equal(optval, IPPROTO_TLS_1_1,
		      "getsockopt got invalid protocol");
	zassert_equal(optlen, sizeof(optval), "getsockopt got invalid size");

	test_close(sock1);
	test_close(sock2);
	k_sleep(TCP_TEARDOWN_TIMEOUT);
}

struct test_msg_waitall_data {
	struct k_work_delayable tx_work;
	int sock;
	const uint8_t *data;
	size_t offset;
	int retries;
};

static void test_msg_waitall_tx_work_handler(struct k_work *work)
{
	struct k_work_delayable *dwork = k_work_delayable_from_work(work);
	struct test_msg_waitall_data *test_data =
		CONTAINER_OF(dwork, struct test_msg_waitall_data, tx_work);

	if (test_data->retries > 0) {
		test_send(test_data->sock, test_data->data + test_data->offset, 1, 0);
		test_data->offset++;
		test_data->retries--;
		k_work_reschedule(&test_data->tx_work, K_MSEC(10));
	}
}

void test_v4_msg_waitall(void)
{
	struct test_msg_waitall_data test_data = {
		.data = TEST_STR_SMALL,
	};
	int c_sock;
	int s_sock;
	int new_sock;
	struct sockaddr_in c_saddr;
	struct sockaddr_in s_saddr;
	struct sockaddr addr;
	socklen_t addrlen = sizeof(addr);
	int ret;
	uint8_t rx_buf[sizeof(TEST_STR_SMALL) - 1] = { 0 };
	struct timeval timeo_optval = {
		.tv_sec = 0,
		.tv_usec = 500000,
	};

	prepare_sock_tls_v4(CONFIG_NET_CONFIG_MY_IPV4_ADDR, ANY_PORT,
			    &c_sock, &c_saddr, IPPROTO_TLS_1_2);
	prepare_sock_tls_v4(CONFIG_NET_CONFIG_MY_IPV4_ADDR, ANY_PORT,
			    &s_sock, &s_saddr, IPPROTO_TLS_1_2);

	test_config_psk(s_sock, c_sock);

	test_bind(s_sock, (struct sockaddr *)&s_saddr, sizeof(s_saddr));
	test_listen(s_sock);

	spawn_client_connect_thread(c_sock, (struct sockaddr *)&s_saddr);

	test_accept(s_sock, &new_sock, &addr, &addrlen);
	zassert_equal(addrlen, sizeof(struct sockaddr_in), "Wrong addrlen");

	k_thread_join(&client_connect_thread, K_FOREVER);

	/* Regular MSG_WAITALL - make sure recv returns only after
	 * requested amount is received.
	 */
	test_data.offset = 0;
	test_data.retries = sizeof(rx_buf);
	test_data.sock = c_sock;
	k_work_init_delayable(&test_data.tx_work,
			      test_msg_waitall_tx_work_handler);
	k_work_reschedule(&test_data.tx_work, K_MSEC(10));

	ret = recv(new_sock, rx_buf, sizeof(rx_buf), MSG_WAITALL);
	zassert_equal(ret, sizeof(rx_buf), "Invalid length received");
	zassert_mem_equal(rx_buf, TEST_STR_SMALL, sizeof(rx_buf),
			  "Invalid data received");
	k_work_cancel_delayable(&test_data.tx_work);

	/* MSG_WAITALL + SO_RCVTIMEO - make sure recv returns the amount of data
	 * received so far
	 */
	ret = setsockopt(new_sock, SOL_SOCKET, SO_RCVTIMEO, &timeo_optval,
			 sizeof(timeo_optval));
	zassert_equal(ret, 0, "setsockopt failed (%d)", errno);

	memset(rx_buf, 0, sizeof(rx_buf));
	test_data.offset = 0;
	test_data.retries = sizeof(rx_buf) - 1;
	test_data.sock = c_sock;
	k_work_init_delayable(&test_data.tx_work,
			      test_msg_waitall_tx_work_handler);
	k_work_reschedule(&test_data.tx_work, K_MSEC(10));

	ret = recv(new_sock, rx_buf, sizeof(rx_buf) - 1, MSG_WAITALL);
	zassert_equal(ret, sizeof(rx_buf) - 1, "Invalid length received");
	zassert_mem_equal(rx_buf, TEST_STR_SMALL, sizeof(rx_buf) - 1,
			  "Invalid data received");
	k_work_cancel_delayable(&test_data.tx_work);

	test_close(new_sock);
	test_close(s_sock);
	test_close(c_sock);
}

void test_v6_msg_waitall(void)
{
	struct test_msg_waitall_data test_data = {
		.data = TEST_STR_SMALL,
	};
	int c_sock;
	int s_sock;
	int new_sock;
	struct sockaddr_in6 c_saddr;
	struct sockaddr_in6 s_saddr;
	struct sockaddr addr;
	socklen_t addrlen = sizeof(addr);
	int ret;
	uint8_t rx_buf[sizeof(TEST_STR_SMALL) - 1] = { 0 };
	struct timeval timeo_optval = {
		.tv_sec = 0,
		.tv_usec = 500000,
	};

	prepare_sock_tls_v6(CONFIG_NET_CONFIG_MY_IPV6_ADDR, ANY_PORT,
			    &c_sock, &c_saddr, IPPROTO_TLS_1_2);
	prepare_sock_tls_v6(CONFIG_NET_CONFIG_MY_IPV6_ADDR, ANY_PORT,
			    &s_sock, &s_saddr, IPPROTO_TLS_1_2);

	test_config_psk(s_sock, c_sock);

	test_bind(s_sock, (struct sockaddr *)&s_saddr, sizeof(s_saddr));
	test_listen(s_sock);

	spawn_client_connect_thread(c_sock, (struct sockaddr *)&s_saddr);

	test_accept(s_sock, &new_sock, &addr, &addrlen);
	zassert_equal(addrlen, sizeof(struct sockaddr_in6), "Wrong addrlen");

	k_thread_join(&client_connect_thread, K_FOREVER);

	/* Regular MSG_WAITALL - make sure recv returns only after
	 * requested amount is received.
	 */
	test_data.offset = 0;
	test_data.retries = sizeof(rx_buf);
	test_data.sock = c_sock;
	k_work_init_delayable(&test_data.tx_work,
			      test_msg_waitall_tx_work_handler);
	k_work_reschedule(&test_data.tx_work, K_MSEC(10));

	ret = recv(new_sock, rx_buf, sizeof(rx_buf), MSG_WAITALL);
	zassert_equal(ret, sizeof(rx_buf), "Invalid length received");
	zassert_mem_equal(rx_buf, TEST_STR_SMALL, sizeof(rx_buf),
			  "Invalid data received");
	k_work_cancel_delayable(&test_data.tx_work);

	/* MSG_WAITALL + SO_RCVTIMEO - make sure recv returns the amount of data
	 * received so far
	 */
	ret = setsockopt(new_sock, SOL_SOCKET, SO_RCVTIMEO, &timeo_optval,
			 sizeof(timeo_optval));
	zassert_equal(ret, 0, "setsockopt failed (%d)", errno);

	memset(rx_buf, 0, sizeof(rx_buf));
	test_data.offset = 0;
	test_data.retries = sizeof(rx_buf) - 1;
	test_data.sock = c_sock;
	k_work_init_delayable(&test_data.tx_work,
			      test_msg_waitall_tx_work_handler);
	k_work_reschedule(&test_data.tx_work, K_MSEC(10));

	ret = recv(new_sock, rx_buf, sizeof(rx_buf) - 1, MSG_WAITALL);
	zassert_equal(ret, sizeof(rx_buf) - 1, "Invalid length received");
	zassert_mem_equal(rx_buf, TEST_STR_SMALL, sizeof(rx_buf) - 1,
			  "Invalid data received");
	k_work_cancel_delayable(&test_data.tx_work);

	test_close(new_sock);
	test_close(s_sock);
	test_close(c_sock);
}

struct test_msg_trunc_data {
	struct k_work_delayable tx_work;
	int sock;
	const uint8_t *data;
	size_t datalen;
};

static void test_msg_trunc_tx_work_handler(struct k_work *work)
{
	struct k_work_delayable *dwork = k_work_delayable_from_work(work);
	struct test_msg_trunc_data *test_data =
		CONTAINER_OF(dwork, struct test_msg_trunc_data, tx_work);

	test_send(test_data->sock, test_data->data, test_data->datalen, 0);
}

void test_msg_trunc(int sock_c, int sock_s, struct sockaddr *addr_c,
		    socklen_t addrlen_c, struct sockaddr *addr_s,
		    socklen_t addrlen_s)
{
	int rv;
	uint8_t rx_buf[sizeof(TEST_STR_SMALL) - 1];
	int role = TLS_DTLS_ROLE_SERVER;
	struct test_msg_trunc_data test_data = {
		.data = TEST_STR_SMALL,
		.datalen = sizeof(TEST_STR_SMALL) - 1
	};

	test_config_psk(sock_s, sock_c);

	rv = setsockopt(sock_s, SOL_TLS, TLS_DTLS_ROLE, &role, sizeof(role));
	zassert_equal(rv, 0, "failed to set DTLS server role");

	rv = bind(sock_s, addr_s, addrlen_s);
	zassert_equal(rv, 0, "server bind failed");

	rv = bind(sock_c, addr_c, addrlen_c);
	zassert_equal(rv, 0, "client bind failed");

	rv = connect(sock_c, addr_s, addrlen_s);
	zassert_equal(rv, 0, "connect failed");

	/* MSG_TRUNC */

	test_data.sock = sock_c;
	k_work_init_delayable(&test_data.tx_work,
			      test_msg_trunc_tx_work_handler);
	k_work_reschedule(&test_data.tx_work, K_MSEC(10));

	memset(rx_buf, 0, sizeof(rx_buf));
	rv = recv(sock_s, rx_buf, 2, ZSOCK_MSG_TRUNC);
	zassert_equal(rv, sizeof(TEST_STR_SMALL) - 1, "MSG_TRUNC flag failed");
	zassert_mem_equal(rx_buf, TEST_STR_SMALL, 2, "invalid rx data");
	zassert_equal(rx_buf[2], 0, "received more than requested");

	/* The remaining data should've been discarded */
	rv = recv(sock_s, rx_buf, sizeof(rx_buf), ZSOCK_MSG_DONTWAIT);
	zassert_equal(rv, -1, "consecutive recv should've failed");
	zassert_equal(errno, EAGAIN, "incorrect errno value");

	/* MSG_PEEK not supported by DTLS socket */

	rv = close(sock_c);
	zassert_equal(rv, 0, "close failed");
	rv = close(sock_s);
	zassert_equal(rv, 0, "close failed");
}

void test_v4_msg_trunc(void)
{
	int client_sock;
	int server_sock;
	struct sockaddr_in client_addr;
	struct sockaddr_in server_addr;

	prepare_sock_dtls_v4(CONFIG_NET_CONFIG_MY_IPV4_ADDR, ANY_PORT,
			     &client_sock, &client_addr, IPPROTO_DTLS_1_2);
	prepare_sock_dtls_v4(CONFIG_NET_CONFIG_MY_IPV4_ADDR, ANY_PORT,
			     &server_sock, &server_addr, IPPROTO_DTLS_1_2);

	test_msg_trunc(client_sock, server_sock,
		       (struct sockaddr *)&client_addr, sizeof(client_addr),
		       (struct sockaddr *)&server_addr, sizeof(server_addr));
}

void test_v6_msg_trunc(void)
{
	int client_sock;
	int server_sock;
	struct sockaddr_in6 client_addr;
	struct sockaddr_in6 server_addr;

	prepare_sock_dtls_v6(CONFIG_NET_CONFIG_MY_IPV6_ADDR, ANY_PORT,
			     &client_sock, &client_addr, IPPROTO_DTLS_1_2);
	prepare_sock_dtls_v6(CONFIG_NET_CONFIG_MY_IPV6_ADDR, ANY_PORT,
			     &server_sock, &server_addr, IPPROTO_DTLS_1_2);

	test_msg_trunc(client_sock, server_sock,
		       (struct sockaddr *)&client_addr, sizeof(client_addr),
		       (struct sockaddr *)&server_addr, sizeof(server_addr));
}

void test_main(void)
{
	if (IS_ENABLED(CONFIG_NET_TC_THREAD_COOPERATIVE)) {
		k_thread_priority_set(k_current_get(),
				K_PRIO_COOP(CONFIG_NUM_COOP_PRIORITIES - 1));
	} else {
		k_thread_priority_set(k_current_get(), K_PRIO_PREEMPT(8));
	}

	ztest_test_suite(
		socket_tls,
		ztest_unit_test(test_so_type),
		ztest_unit_test(test_so_protocol),
		ztest_unit_test(test_v4_msg_waitall),
		ztest_unit_test(test_v6_msg_waitall),
		ztest_unit_test(test_v4_msg_trunc),
		ztest_unit_test(test_v6_msg_trunc)
		);

	ztest_run_test_suite(socket_tls);
}
