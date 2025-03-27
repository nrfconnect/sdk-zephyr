/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/fff.h>
#include <zephyr/logging/log.h>
#include <zephyr/misc/lorem_ipsum.h>
#include <zephyr/ztest.h>

#include "stubs.h"

LOG_MODULE_REGISTER(coap_client_test);

DEFINE_FFF_GLOBALS;
#define FFF_FAKES_LIST(FAKE)

#define LONG_ACK_TIMEOUT_MS                 200
#define MORE_THAN_EXCHANGE_LIFETIME_MS      4 * CONFIG_COAP_INIT_ACK_TIMEOUT_MS
#define MORE_THAN_LONG_EXCHANGE_LIFETIME_MS 4 * LONG_ACK_TIMEOUT_MS
#define MORE_THAN_ACK_TIMEOUT_MS                                                                   \
	(CONFIG_COAP_INIT_ACK_TIMEOUT_MS + CONFIG_COAP_INIT_ACK_TIMEOUT_MS / 2)

#define VALID_MESSAGE_ID BIT(31)

static int16_t last_response_code;
static const char *test_path = "test";

static uint32_t messages_needing_response[2];

static struct coap_client client;

static char *short_payload = "testing";
static char *long_payload = LOREM_IPSUM_SHORT;

static uint16_t get_next_pending_message_id(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(messages_needing_response); i++) {
		if (messages_needing_response[i] & VALID_MESSAGE_ID) {
			messages_needing_response[i] &= ~VALID_MESSAGE_ID;
			return messages_needing_response[i];
		}
	}

	return UINT16_MAX;
}

static void set_next_pending_message_id(uint16_t id)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(messages_needing_response); i++) {
		if (!(messages_needing_response[i] & VALID_MESSAGE_ID)) {
			messages_needing_response[i] = id;
			messages_needing_response[i] |= VALID_MESSAGE_ID;
			return;
		}
	}
}

static ssize_t z_impl_zsock_recvfrom_custom_fake(int sock, void *buf, size_t max_len, int flags,
						 struct sockaddr *src_addr, socklen_t *addrlen)
{
	uint16_t last_message_id = 0;

	LOG_INF("Recvfrom");
	uint8_t ack_data[] = {0x68, 0x40, 0x00, 0x00, 0x00, 0x00,
			      0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

	last_message_id = get_next_pending_message_id();

	ack_data[2] = (uint8_t)(last_message_id >> 8);
	ack_data[3] = (uint8_t)last_message_id;

	memcpy(buf, ack_data, sizeof(ack_data));

	clear_socket_events();

	return sizeof(ack_data);
}

static ssize_t z_impl_zsock_sendto_custom_fake(int sock, void *buf, size_t len, int flags,
					       const struct sockaddr *dest_addr, socklen_t addrlen)
{
	uint16_t last_message_id = 0;
	uint8_t type;

	last_message_id |= ((uint8_t *)buf)[2] << 8;
	last_message_id |= ((uint8_t *)buf)[3];

	type = (((uint8_t *)buf)[0] & 0x30) >> 4;

	set_next_pending_message_id(last_message_id);
	LOG_INF("Latest message ID: %d", last_message_id);

	if (type == 0) {
		set_socket_events(ZSOCK_POLLIN);
	}

	return 1;
}

static ssize_t z_impl_zsock_sendto_custom_fake_no_reply(int sock, void *buf, size_t len, int flags,
							const struct sockaddr *dest_addr,
							socklen_t addrlen)
{
	uint16_t last_message_id = 0;

	last_message_id |= ((uint8_t *)buf)[2] << 8;
	last_message_id |= ((uint8_t *)buf)[3];

	set_next_pending_message_id(last_message_id);
	LOG_INF("Latest message ID: %d", last_message_id);

	return 1;
}

static ssize_t z_impl_zsock_sendto_custom_fake_echo(int sock, void *buf, size_t len, int flags,
						    const struct sockaddr *dest_addr,
						    socklen_t addrlen)
{
	uint16_t last_message_id = 0;
	struct coap_packet response = {0};
	struct coap_option option = {0};

	last_message_id |= ((uint8_t *)buf)[2] << 8;
	last_message_id |= ((uint8_t *)buf)[3];

	set_next_pending_message_id(last_message_id);
	LOG_INF("Latest message ID: %d", last_message_id);

	int ret = coap_packet_parse(&response, buf, len, NULL, 0);
	if (ret < 0) {
		LOG_ERR("Invalid data received");
	}

	ret = coap_find_options(&response, COAP_OPTION_ECHO, &option, 1);

	zassert_equal(ret, 1, "Coap echo option not found, %d", ret);
	zassert_mem_equal(option.value, "echo_value", option.len, "Incorrect echo data");

	z_impl_zsock_sendto_fake.custom_fake = z_impl_zsock_sendto_custom_fake;

	set_socket_events(ZSOCK_POLLIN);

	return 1;
}

static ssize_t z_impl_zsock_sendto_custom_fake_echo_next_req(int sock, void *buf, size_t len,
							     int flags,
							     const struct sockaddr *dest_addr,
							     socklen_t addrlen)
{
	uint16_t last_message_id = 0;
	struct coap_packet response = {0};
	struct coap_option option = {0};

	last_message_id |= ((uint8_t *)buf)[2] << 8;
	last_message_id |= ((uint8_t *)buf)[3];

	set_next_pending_message_id(last_message_id);
	LOG_INF("Latest message ID: %d", last_message_id);

	int ret = coap_packet_parse(&response, buf, len, NULL, 0);
	if (ret < 0) {
		LOG_ERR("Invalid data received");
	}

	ret = coap_header_get_code(&response);
	zassert_equal(ret, COAP_METHOD_POST, "Incorrect method, %d", ret);

	uint16_t payload_len;

	const uint8_t *payload = coap_packet_get_payload(&response, &payload_len);

	zassert_mem_equal(payload, "echo testing", payload_len, "Incorrect payload");

	ret = coap_find_options(&response, COAP_OPTION_ECHO, &option, 1);
	zassert_equal(ret, 1, "Coap echo option not found, %d", ret);
	zassert_mem_equal(option.value, "echo_value", option.len, "Incorrect echo data");

	z_impl_zsock_sendto_fake.custom_fake = z_impl_zsock_sendto_custom_fake;

	set_socket_events(ZSOCK_POLLIN);

	return 1;
}

static ssize_t z_impl_zsock_recvfrom_custom_fake_response(int sock, void *buf, size_t max_len,
							  int flags, struct sockaddr *src_addr,
							  socklen_t *addrlen)
{
	uint16_t last_message_id = 0;

	static uint8_t ack_data[] = {0x48, 0x40, 0x00, 0x00, 0x00, 0x00,
				     0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

	last_message_id = get_next_pending_message_id();

	ack_data[2] = (uint8_t)(last_message_id >> 8);
	ack_data[3] = (uint8_t)last_message_id;

	memcpy(buf, ack_data, sizeof(ack_data));

	clear_socket_events();

	return sizeof(ack_data);
}

static ssize_t z_impl_zsock_recvfrom_custom_fake_empty_ack(int sock, void *buf, size_t max_len,
							   int flags, struct sockaddr *src_addr,
							   socklen_t *addrlen)
{
	uint16_t last_message_id = 0;

	static uint8_t ack_data[] = {0x68, 0x00, 0x00, 0x00, 0x00, 0x00,
				     0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

	last_message_id = get_next_pending_message_id();

	ack_data[2] = (uint8_t)(last_message_id >> 8);
	ack_data[3] = (uint8_t)last_message_id;

	memcpy(buf, ack_data, sizeof(ack_data));

	z_impl_zsock_recvfrom_fake.custom_fake = z_impl_zsock_recvfrom_custom_fake_response;

	return sizeof(ack_data);
}

static ssize_t z_impl_zsock_recvfrom_custom_fake_unmatching(int sock, void *buf, size_t max_len,
							    int flags, struct sockaddr *src_addr,
							    socklen_t *addrlen)
{
	uint16_t last_message_id = 0;

	static uint8_t ack_data[] = {0x68, 0x40, 0x00, 0x00, 0x00, 0x00,
				     0x00, 0x00, 0x00, 0x00, 0x00, 0x01};

	last_message_id = get_next_pending_message_id();

	ack_data[2] = (uint8_t)(last_message_id >> 8);
	ack_data[3] = (uint8_t)last_message_id;

	memcpy(buf, ack_data, sizeof(ack_data));

	clear_socket_events();

	return sizeof(ack_data);
}

static ssize_t z_impl_zsock_recvfrom_custom_fake_echo(int sock, void *buf, size_t max_len,
						      int flags, struct sockaddr *src_addr,
						      socklen_t *addrlen)
{
	uint16_t last_message_id = 0;

	LOG_INF("Recvfrom");
	uint8_t ack_data[] = {0x68, 0x81, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			      0x00, 0x00, 0x00, 0x00, 0xda, 0xef, 'e',  'c',
			      'h',  'o',  '_',  'v',  'a',  'l',  'u',  'e'};

	last_message_id = get_next_pending_message_id();

	ack_data[2] = (uint8_t)(last_message_id >> 8);
	ack_data[3] = (uint8_t)last_message_id;

	memcpy(buf, ack_data, sizeof(ack_data));

	z_impl_zsock_recvfrom_fake.custom_fake = z_impl_zsock_recvfrom_custom_fake_response;
	z_impl_zsock_sendto_fake.custom_fake = z_impl_zsock_sendto_custom_fake_echo;

	clear_socket_events();

	return sizeof(ack_data);
}

static ssize_t z_impl_zsock_recvfrom_custom_fake_echo_next_req(int sock, void *buf, size_t max_len,
							       int flags, struct sockaddr *src_addr,
							       socklen_t *addrlen)
{
	uint16_t last_message_id = 0;

	LOG_INF("Recvfrom");
	uint8_t ack_data[] = {0x68, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			      0x00, 0x00, 0x00, 0x00, 0xda, 0xef, 'e',  'c',
			      'h',  'o',  '_',  'v',  'a',  'l',  'u',  'e'};

	last_message_id = get_next_pending_message_id();

	ack_data[2] = (uint8_t)(last_message_id >> 8);
	ack_data[3] = (uint8_t)last_message_id;

	memcpy(buf, ack_data, sizeof(ack_data));

	z_impl_zsock_recvfrom_fake.custom_fake = z_impl_zsock_recvfrom_custom_fake_response;
	z_impl_zsock_sendto_fake.custom_fake = z_impl_zsock_sendto_custom_fake_echo_next_req;

	clear_socket_events();

	return sizeof(ack_data);
}

static void *suite_setup(void)
{
	coap_client_init(&client, NULL);

	return NULL;
}

static void test_setup(void *data)
{
	int i;

	/* Register resets */
	DO_FOREACH_FAKE(RESET_FAKE);
	/* reset common FFF internal structures */
	FFF_RESET_HISTORY();

	z_impl_zsock_recvfrom_fake.custom_fake = z_impl_zsock_recvfrom_custom_fake;
	z_impl_zsock_sendto_fake.custom_fake = z_impl_zsock_sendto_custom_fake;

	for (i = 0; i < ARRAY_SIZE(messages_needing_response); i++) {
		messages_needing_response[i] = 0;
	}

	last_response_code = 0;
}

void coap_callback(int16_t code, size_t offset, const uint8_t *payload, size_t len, bool last_block,
		   void *user_data)
{
	LOG_INF("CoAP response callback, %d", code);
	last_response_code = code;
}

ZTEST_SUITE(coap_client, NULL, suite_setup, test_setup, NULL, NULL);

ZTEST(coap_client, test_get_request)
{
	int ret = 0;
	struct sockaddr address = {0};
	struct coap_client_request client_request = {
		.method = COAP_METHOD_GET,
		.confirmable = true,
		.path = test_path,
		.fmt = COAP_CONTENT_FORMAT_TEXT_PLAIN,
		.cb = coap_callback,
		.payload = NULL,
		.len = 0
	};

	client_request.payload = short_payload;
	client_request.len = strlen(short_payload);

	k_sleep(K_MSEC(1));

	LOG_INF("Send request");
	ret = coap_client_req(&client, 0, &address, &client_request, NULL);
	zassert_true(ret >= 0, "Sending request failed, %d", ret);

	k_sleep(K_MSEC(MORE_THAN_EXCHANGE_LIFETIME_MS));
	zassert_equal(last_response_code, COAP_RESPONSE_CODE_OK, "Unexpected response");
	LOG_INF("Test done");
}

ZTEST(coap_client, test_resend_request)
{
	int ret = 0;
	struct sockaddr address = {0};
	struct coap_client_request client_request = {
		.method = COAP_METHOD_GET,
		.confirmable = true,
		.path = test_path,
		.fmt = COAP_CONTENT_FORMAT_TEXT_PLAIN,
		.cb = coap_callback,
		.payload = NULL,
		.len = 0
	};

	client_request.payload = short_payload;
	client_request.len = strlen(short_payload);

	z_impl_zsock_sendto_fake.custom_fake = z_impl_zsock_sendto_custom_fake_no_reply;

	k_sleep(K_MSEC(1));

	LOG_INF("Send request");
	ret = coap_client_req(&client, 0, &address, &client_request, NULL);
	zassert_true(ret >= 0, "Sending request failed, %d", ret);
	k_sleep(K_MSEC(MORE_THAN_ACK_TIMEOUT_MS));
	set_socket_events(ZSOCK_POLLIN);

	k_sleep(K_MSEC(MORE_THAN_EXCHANGE_LIFETIME_MS));
	zassert_equal(last_response_code, COAP_RESPONSE_CODE_OK, "Unexpected response");
	zassert_equal(z_impl_zsock_sendto_fake.call_count, 2);
	LOG_INF("Test done");
}

ZTEST(coap_client, test_echo_option)
{
	int ret = 0;
	struct sockaddr address = {0};
	struct coap_client_request client_request = {
		.method = COAP_METHOD_GET,
		.confirmable = true,
		.path = test_path,
		.fmt = COAP_CONTENT_FORMAT_TEXT_PLAIN,
		.cb = coap_callback,
		.payload = NULL,
		.len = 0
	};

	client_request.payload = short_payload;
	client_request.len = strlen(short_payload);

	z_impl_zsock_recvfrom_fake.custom_fake = z_impl_zsock_recvfrom_custom_fake_echo;

	k_sleep(K_MSEC(1));

	LOG_INF("Send request");
	ret = coap_client_req(&client, 0, &address, &client_request, NULL);
	zassert_true(ret >= 0, "Sending request failed, %d", ret);

	k_sleep(K_MSEC(MORE_THAN_EXCHANGE_LIFETIME_MS));
	zassert_equal(last_response_code, COAP_RESPONSE_CODE_OK, "Unexpected response");
	LOG_INF("Test done");
}

ZTEST(coap_client, test_echo_option_next_req)
{
	int ret = 0;
	struct sockaddr address = {0};
	struct coap_client_request client_request = {
		.method = COAP_METHOD_GET,
		.confirmable = true,
		.path = test_path,
		.fmt = COAP_CONTENT_FORMAT_TEXT_PLAIN,
		.cb = coap_callback,
		.payload = NULL,
		.len = 0
	};

	client_request.payload = short_payload;
	client_request.len = strlen(short_payload);

	z_impl_zsock_recvfrom_fake.custom_fake = z_impl_zsock_recvfrom_custom_fake_echo_next_req;

	k_sleep(K_MSEC(1));

	LOG_INF("Send request");
	ret = coap_client_req(&client, 0, &address, &client_request, NULL);
	zassert_true(ret >= 0, "Sending request failed, %d", ret);

	k_sleep(K_MSEC(MORE_THAN_EXCHANGE_LIFETIME_MS));
	zassert_equal(last_response_code, COAP_RESPONSE_CODE_OK, "Unexpected response");

	char *payload = "echo testing";

	client_request.method = COAP_METHOD_POST;
	client_request.payload = payload;
	client_request.len = strlen(payload);

	LOG_INF("Send next request");
	ret = coap_client_req(&client, 0, &address, &client_request, NULL);
	zassert_true(ret >= 0, "Sending request failed, %d", ret);

	k_sleep(K_MSEC(MORE_THAN_EXCHANGE_LIFETIME_MS));
	zassert_equal(last_response_code, COAP_RESPONSE_CODE_OK, "Unexpected response");
}

ZTEST(coap_client, test_get_no_path)
{
	int ret = 0;
	struct sockaddr address = {0};
	struct coap_client_request client_request = {
		.method = COAP_METHOD_GET,
		.confirmable = true,
		.path = NULL,
		.fmt = COAP_CONTENT_FORMAT_TEXT_PLAIN,
		.cb = coap_callback,
		.payload = NULL,
		.len = 0
	};

	client_request.payload = short_payload;
	client_request.len = strlen(short_payload);

	k_sleep(K_MSEC(1));

	LOG_INF("Send request");
	ret = coap_client_req(&client, 0, &address, &client_request, NULL);

	zassert_equal(ret, -EINVAL, "Get request without path");
}

ZTEST(coap_client, test_send_large_data)
{
	int ret = 0;
	struct sockaddr address = {0};
	struct coap_client_request client_request = {
		.method = COAP_METHOD_GET,
		.confirmable = true,
		.path = test_path,
		.fmt = COAP_CONTENT_FORMAT_TEXT_PLAIN,
		.cb = coap_callback,
		.payload = NULL,
		.len = 0
	};

	client_request.payload = long_payload;
	client_request.len = strlen(long_payload);

	k_sleep(K_MSEC(1));

	LOG_INF("Send request");
	ret = coap_client_req(&client, 0, &address, &client_request, NULL);
	zassert_true(ret >= 0, "Sending request failed, %d", ret);

	k_sleep(K_MSEC(MORE_THAN_EXCHANGE_LIFETIME_MS));
	zassert_equal(last_response_code, COAP_RESPONSE_CODE_OK, "Unexpected response");
}

ZTEST(coap_client, test_no_response)
{
	int ret = 0;
	struct sockaddr address = {0};
	struct coap_client_request client_request = {
		.method = COAP_METHOD_GET,
		.confirmable = true,
		.path = test_path,
		.fmt = COAP_CONTENT_FORMAT_TEXT_PLAIN,
		.cb = coap_callback,
		.payload = NULL,
		.len = 0
	};
	struct coap_transmission_parameters params = {
		.ack_timeout = LONG_ACK_TIMEOUT_MS,
		.coap_backoff_percent = 200,
		.max_retransmission = 0
	};

	client_request.payload = short_payload;
	client_request.len = strlen(short_payload);

	z_impl_zsock_sendto_fake.custom_fake = z_impl_zsock_sendto_custom_fake_no_reply;

	k_sleep(K_MSEC(1));

	LOG_INF("Send request");
	ret = coap_client_req(&client, 0, &address, &client_request, &params);

	zassert_true(ret >= 0, "Sending request failed, %d", ret);

	k_sleep(K_MSEC(MORE_THAN_LONG_EXCHANGE_LIFETIME_MS));
	zassert_equal(last_response_code, -ETIMEDOUT, "Unexpected response");
}

ZTEST(coap_client, test_separate_response)
{
	int ret = 0;
	struct sockaddr address = {0};
	struct coap_client_request client_request = {
		.method = COAP_METHOD_GET,
		.confirmable = true,
		.path = test_path,
		.fmt = COAP_CONTENT_FORMAT_TEXT_PLAIN,
		.cb = coap_callback,
		.payload = NULL,
		.len = 0
	};

	client_request.payload = short_payload;
	client_request.len = strlen(short_payload);

	z_impl_zsock_recvfrom_fake.custom_fake = z_impl_zsock_recvfrom_custom_fake_empty_ack;

	k_sleep(K_MSEC(1));

	LOG_INF("Send request");
	ret = coap_client_req(&client, 0, &address, &client_request, NULL);
	zassert_true(ret >= 0, "Sending request failed, %d", ret);

	k_sleep(K_MSEC(MORE_THAN_EXCHANGE_LIFETIME_MS));
	zassert_equal(last_response_code, COAP_RESPONSE_CODE_OK, "Unexpected response");
}

ZTEST(coap_client, test_multiple_requests)
{
	int ret = 0;
	int retry = MORE_THAN_EXCHANGE_LIFETIME_MS;

	struct sockaddr address = {0};
	struct coap_client_request client_request = {
		.method = COAP_METHOD_GET,
		.confirmable = true,
		.path = test_path,
		.fmt = COAP_CONTENT_FORMAT_TEXT_PLAIN,
		.cb = coap_callback,
		.payload = NULL,
		.len = 0
	};

	client_request.payload = short_payload;
	client_request.len = strlen(short_payload);

	z_impl_zsock_sendto_fake.custom_fake = z_impl_zsock_sendto_custom_fake_no_reply;

	k_sleep(K_MSEC(1));

	LOG_INF("Send request");
	ret = coap_client_req(&client, 0, &address, &client_request, NULL);
	zassert_true(ret >= 0, "Sending request failed, %d", ret);

	ret = coap_client_req(&client, 0, &address, &client_request, NULL);
	zassert_true(ret >= 0, "Sending request failed, %d", ret);

	set_socket_events(ZSOCK_POLLIN);
	while (last_response_code == 0 && retry > 0) {
		retry--;
		k_sleep(K_MSEC(1));
	}
	zassert_equal(last_response_code, COAP_RESPONSE_CODE_OK, "Unexpected response");

	set_socket_events(ZSOCK_POLLIN);
	k_sleep(K_MSEC(MORE_THAN_EXCHANGE_LIFETIME_MS));
	zassert_equal(last_response_code, COAP_RESPONSE_CODE_OK, "Unexpected response");
}

ZTEST(coap_client, test_unmatching_tokens)
{
	int ret = 0;
	struct sockaddr address = {0};
	struct coap_client_request client_request = {
		.method = COAP_METHOD_GET,
		.confirmable = true,
		.path = test_path,
		.fmt = COAP_CONTENT_FORMAT_TEXT_PLAIN,
		.cb = coap_callback,
		.payload = NULL,
		.len = 0
	};
	struct coap_transmission_parameters params = {
		.ack_timeout = LONG_ACK_TIMEOUT_MS,
		.coap_backoff_percent = 200,
		.max_retransmission = 0
	};

	client_request.payload = short_payload;
	client_request.len = strlen(short_payload);

	z_impl_zsock_recvfrom_fake.custom_fake = z_impl_zsock_recvfrom_custom_fake_unmatching;

	k_sleep(K_MSEC(1));

	LOG_INF("Send request");
	ret = coap_client_req(&client, 0, &address, &client_request, &params);
	zassert_true(ret >= 0, "Sending request failed, %d", ret);

	k_sleep(K_MSEC(MORE_THAN_LONG_EXCHANGE_LIFETIME_MS));
	zassert_equal(last_response_code, -ETIMEDOUT, "Unexpected response");
}
