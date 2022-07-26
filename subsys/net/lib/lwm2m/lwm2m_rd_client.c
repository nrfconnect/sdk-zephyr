/*
 * Copyright (c) 2017 Linaro Limited
 * Copyright (c) 2017-2019 Foundries.io
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Copyright (c) 2015, Yanzi Networks AB.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Original authors:
 *         Joakim Eriksson <joakime@sics.se>
 *         Niclas Finne <nfi@sics.se>
 *         Joel Hoglund <joel@sics.se>
 */

#define LOG_MODULE_NAME net_lwm2m_rd_client
#define LOG_LEVEL CONFIG_LWM2M_LOG_LEVEL

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(LOG_MODULE_NAME);

#include <zephyr/types.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <zephyr/init.h>
#include <zephyr/sys/printk.h>

#include "lwm2m_object.h"
#include "lwm2m_engine.h"
#include "lwm2m_rw_link_format.h"

#define LWM2M_RD_CLIENT_URI "rd"

#define SECONDS_TO_UPDATE_EARLY	CONFIG_LWM2M_SECONDS_TO_UPDATE_EARLY
#define STATE_MACHINE_UPDATE_INTERVAL_MS 500

#define CLIENT_EP_LEN		CONFIG_LWM2M_RD_CLIENT_ENDPOINT_NAME_MAX_LENGTH

#define CLIENT_BINDING_LEN sizeof("U")
#define CLIENT_QUEUE_LEN sizeof("Q")

static void sm_handle_registration_update_failure(void);

/* The states for the RD client state machine */
/*
 * When node is unregistered it ends up in UNREGISTERED
 * and this is going to be there until use X or Y kicks it
 * back into INIT again
 */
enum sm_engine_state {
	ENGINE_IDLE,
	ENGINE_INIT,
#if defined(CONFIG_LWM2M_RD_CLIENT_SUPPORT_BOOTSTRAP)
	ENGINE_DO_BOOTSTRAP_REG,
	ENGINE_BOOTSTRAP_REG_SENT,
	ENGINE_BOOTSTRAP_REG_DONE,
	ENGINE_BOOTSTRAP_TRANS_DONE,
#endif
	ENGINE_DO_REGISTRATION,
	ENGINE_REGISTRATION_SENT,
	ENGINE_REGISTRATION_DONE,
	ENGINE_REGISTRATION_DONE_RX_OFF,
	ENGINE_UPDATE_SENT,
	ENGINE_DEREGISTER,
	ENGINE_DEREGISTER_SENT,
	ENGINE_DEREGISTERED,
	ENGINE_NETWORK_ERROR,
};

struct lwm2m_rd_client_info {
	struct k_mutex mutex;
	struct lwm2m_message rd_message;
	struct lwm2m_ctx *ctx;
	uint32_t lifetime;
	uint8_t engine_state;
	uint8_t retries;
	uint8_t retry_delay;

	int64_t last_update;
	int64_t last_tx;

	char ep_name[CLIENT_EP_LEN];
	char server_ep[CLIENT_EP_LEN];

	bool use_bootstrap : 1;

	bool trigger_update : 1;
	bool update_objects : 1;
} client;

/* Allocate some data for queries and updates. Make sure it's large enough to
 * hold the largest query string, which in most cases will be the endpoint
 * string. In other case, 32 bytes are enough to encode any other query string
 * documented in the LwM2M specification.
 */
static char query_buffer[MAX(32, sizeof("ep=") + CLIENT_EP_LEN)];

static struct lwm2m_message *rd_get_message(void)
{
	if (client.rd_message.ctx) {
		return NULL;
	}

	client.rd_message.ctx = client.ctx;
	return &client.rd_message;

}

struct lwm2m_message *lwm2m_get_ongoing_rd_msg(void)
{
	if (!client.ctx || !client.rd_message.ctx) {
		return NULL;
	}
	return &client.rd_message;
}

void engine_update_tx_time(void)
{
	client.last_tx = k_uptime_get();
}

static void set_sm_state(uint8_t sm_state)
{
	enum lwm2m_rd_client_event event = LWM2M_RD_CLIENT_EVENT_NONE;

	/* Determine if a callback to the app is needed */
#if defined(CONFIG_LWM2M_RD_CLIENT_SUPPORT_BOOTSTRAP)
	if (sm_state == ENGINE_BOOTSTRAP_REG_DONE) {
		event = LWM2M_RD_CLIENT_EVENT_BOOTSTRAP_REG_COMPLETE;
	} else if (client.engine_state == ENGINE_BOOTSTRAP_TRANS_DONE &&
		   sm_state == ENGINE_DO_REGISTRATION) {
		event = LWM2M_RD_CLIENT_EVENT_BOOTSTRAP_TRANSFER_COMPLETE;
	} else
#endif
	if (client.engine_state == ENGINE_UPDATE_SENT &&
	    (sm_state == ENGINE_REGISTRATION_DONE ||
	     sm_state == ENGINE_REGISTRATION_DONE_RX_OFF)) {
#if defined(CONFIG_LWM2M_QUEUE_MODE_ENABLED)
		lwm2m_push_queued_buffers(client.ctx);
#endif
		event = LWM2M_RD_CLIENT_EVENT_REG_UPDATE_COMPLETE;
	} else if (sm_state == ENGINE_REGISTRATION_DONE) {
#if defined(CONFIG_LWM2M_QUEUE_MODE_ENABLED)
		lwm2m_push_queued_buffers(client.ctx);
#endif
		event = LWM2M_RD_CLIENT_EVENT_REGISTRATION_COMPLETE;
	} else if (sm_state == ENGINE_REGISTRATION_DONE_RX_OFF) {
		event = LWM2M_RD_CLIENT_EVENT_QUEUE_MODE_RX_OFF;
#if defined(CONFIG_LWM2M_QUEUE_MODE_ENABLED)
		lwm2m_engine_close_socket_connection(client.ctx);
#endif
	} else if ((sm_state == ENGINE_INIT ||
		    sm_state == ENGINE_DEREGISTERED) &&
		   (client.engine_state >= ENGINE_DO_REGISTRATION &&
		    client.engine_state <= ENGINE_DEREGISTER_SENT)) {
		lwm2m_engine_context_close(client.ctx);
		event = LWM2M_RD_CLIENT_EVENT_DISCONNECT;
	} else if (sm_state == ENGINE_NETWORK_ERROR) {
		lwm2m_engine_context_close(client.ctx);
		client.retry_delay = 1 << client.retries;
		client.retries++;
		if (client.retries > CONFIG_LWM2M_RD_CLIENT_MAX_RETRIES) {
			client.retries = 0;
			event = LWM2M_RD_CLIENT_EVENT_NETWORK_ERROR;
		}
	}

	/* TODO: add locking? */
	client.engine_state = sm_state;

	if (event > LWM2M_RD_CLIENT_EVENT_NONE && client.ctx->event_cb) {
		client.ctx->event_cb(client.ctx, event);
	}
}

static bool sm_is_registered(void)
{
	return (client.engine_state >= ENGINE_REGISTRATION_DONE &&
		client.engine_state <= ENGINE_DEREGISTER_SENT);
}

static uint8_t get_sm_state(void)
{
	/* TODO: add locking? */
	return client.engine_state;
}

static void sm_handle_timeout_state(struct lwm2m_message *msg,
				    enum sm_engine_state sm_state)
{
	enum lwm2m_rd_client_event event = LWM2M_RD_CLIENT_EVENT_NONE;

#if defined(CONFIG_LWM2M_RD_CLIENT_SUPPORT_BOOTSTRAP)
	if (client.engine_state == ENGINE_BOOTSTRAP_REG_SENT) {
		event = LWM2M_RD_CLIENT_EVENT_BOOTSTRAP_REG_FAILURE;
	} else
#endif
	{
		if (client.engine_state == ENGINE_REGISTRATION_SENT) {
			event = LWM2M_RD_CLIENT_EVENT_REGISTRATION_FAILURE;
		} else if (client.engine_state == ENGINE_UPDATE_SENT) {
			event = LWM2M_RD_CLIENT_EVENT_REG_UPDATE_FAILURE;
		} else if (client.engine_state == ENGINE_DEREGISTER_SENT) {
			event = LWM2M_RD_CLIENT_EVENT_DEREGISTER_FAILURE;
		} else {
			/* TODO: unknown timeout state */
		}
	}

	set_sm_state(sm_state);

	if (event > LWM2M_RD_CLIENT_EVENT_NONE && client.ctx->event_cb) {
		client.ctx->event_cb(client.ctx, event);
	}
}

static void sm_handle_failure_state(enum sm_engine_state sm_state)
{
	enum lwm2m_rd_client_event event = LWM2M_RD_CLIENT_EVENT_NONE;

#if defined(CONFIG_LWM2M_RD_CLIENT_SUPPORT_BOOTSTRAP)
	if (client.engine_state == ENGINE_BOOTSTRAP_REG_SENT) {
		event = LWM2M_RD_CLIENT_EVENT_BOOTSTRAP_REG_FAILURE;
	} else
#endif
	if (client.engine_state == ENGINE_REGISTRATION_SENT) {
		event = LWM2M_RD_CLIENT_EVENT_REGISTRATION_FAILURE;
	} else if (client.engine_state == ENGINE_UPDATE_SENT) {
		sm_handle_registration_update_failure();
		return;
	} else if (client.engine_state == ENGINE_DEREGISTER_SENT) {
		event = LWM2M_RD_CLIENT_EVENT_DEREGISTER_FAILURE;
	}

	lwm2m_engine_context_close(client.ctx);
	set_sm_state(sm_state);

	if (event > LWM2M_RD_CLIENT_EVENT_NONE && client.ctx->event_cb) {
		client.ctx->event_cb(client.ctx, event);
	}
}

/* force state machine restart */
static void socket_fault_cb(int error)
{
	LOG_ERR("RD Client socket error: %d", error);

	lwm2m_engine_context_close(client.ctx);

	client.ctx->sec_obj_inst = -1;

	/* Jump directly to the registration phase. In case there is no valid
	 * security object for the LWM2M server, it will fall back to the
	 * bootstrap procedure.
	 */
	set_sm_state(ENGINE_DO_REGISTRATION);
}

/* force re-update with remote peer */
void engine_trigger_update(bool update_objects)
{
	if (client.engine_state < ENGINE_REGISTRATION_SENT ||
	    client.engine_state > ENGINE_UPDATE_SENT) {
		return;
	}

	/* TODO: add locking? */
	client.trigger_update = true;

	if (update_objects) {
		client.update_objects = true;
	}
}

static inline const char *code2str(uint8_t code)
{
	switch (code) {
	case COAP_RESPONSE_CODE_BAD_REQUEST:
		return "Bad Request";
	case COAP_RESPONSE_CODE_FORBIDDEN:
		return "Forbidden";
	case COAP_RESPONSE_CODE_NOT_FOUND:
		return "Not Found";
	case COAP_RESPONSE_CODE_PRECONDITION_FAILED:
		return "Precondition Failed";
	default:
		break;
	}

	return "Unknown";
}

/* state machine reply callbacks */

#if defined(CONFIG_LWM2M_RD_CLIENT_SUPPORT_BOOTSTRAP)
static int do_bootstrap_reply_cb(const struct coap_packet *response,
				 struct coap_reply *reply,
				 const struct sockaddr *from)
{
	uint8_t code;

	code = coap_header_get_code(response);
	LOG_DBG("Bootstrap callback (code:%u.%u)",
		COAP_RESPONSE_CODE_CLASS(code),
		COAP_RESPONSE_CODE_DETAIL(code));

	if (code == COAP_RESPONSE_CODE_CHANGED) {
		LOG_INF("Bootstrap registration done!");
		set_sm_state(ENGINE_BOOTSTRAP_REG_DONE);
		return 0;
	}

	LOG_ERR("Failed with code %u.%u (%s). Not Retrying.",
		COAP_RESPONSE_CODE_CLASS(code), COAP_RESPONSE_CODE_DETAIL(code),
		code2str(code));

	sm_handle_failure_state(ENGINE_IDLE);

	return 0;
}

static void do_bootstrap_reg_timeout_cb(struct lwm2m_message *msg)
{
	LOG_WRN("Bootstrap Timeout");

	/* TODO:
	 * Look for the "next" BOOTSTRAP server entry in our security info
	 */

	/* Restart from scratch */
	sm_handle_timeout_state(msg, ENGINE_INIT);
}
#endif

int engine_trigger_bootstrap(void)
{
#if defined(CONFIG_LWM2M_RD_CLIENT_SUPPORT_BOOTSTRAP)
	if (!sm_is_registered()) {
		/* Bootstrap is not possible to trig */
		LOG_WRN("Cannot trigger bootstrap from state %u", client.engine_state);
		return -EPERM;
	}

	LOG_INF("Server Initiated Bootstrap");
	client.use_bootstrap = true;
	client.engine_state = ENGINE_INIT;

	return 0;
#else
	return -EPERM;
#endif
}
static int do_registration_reply_cb(const struct coap_packet *response,
				    struct coap_reply *reply,
				    const struct sockaddr *from)
{
	struct coap_option options[2];
	uint8_t code;
	int ret;

	code = coap_header_get_code(response);
	LOG_DBG("Registration callback (code:%u.%u)",
		COAP_RESPONSE_CODE_CLASS(code),
		COAP_RESPONSE_CODE_DETAIL(code));

	/* check state and possibly set registration to done */
	if (code == COAP_RESPONSE_CODE_CREATED) {
		ret = coap_find_options(response, COAP_OPTION_LOCATION_PATH,
					options, 2);
		if (ret < 2) {
			LOG_ERR("Unexpected endpoint data returned.");
			return -EINVAL;
		}

		/* option[0] should be "rd" */

		if (options[1].len + 1 > sizeof(client.server_ep)) {
			LOG_ERR("Unexpected length of query: "
				    "%u (expected %zu)\n",
				    options[1].len,
				    sizeof(client.server_ep));
			return -EINVAL;
		}

		memcpy(client.server_ep, options[1].value,
		       options[1].len);
		client.server_ep[options[1].len] = '\0';
		set_sm_state(ENGINE_REGISTRATION_DONE);
		LOG_INF("Registration Done (EP='%s')",
			client.server_ep);

		return 0;
	}

	LOG_ERR("Failed with code %u.%u (%s). Not Retrying.",
		COAP_RESPONSE_CODE_CLASS(code), COAP_RESPONSE_CODE_DETAIL(code),
		code2str(code));

	sm_handle_failure_state(ENGINE_IDLE);

	return 0;
}

static void do_registration_timeout_cb(struct lwm2m_message *msg)
{
	LOG_WRN("Registration Timeout");

	/* Restart from scratch */
	sm_handle_timeout_state(msg, ENGINE_INIT);
}

static int do_update_reply_cb(const struct coap_packet *response,
			      struct coap_reply *reply,
			      const struct sockaddr *from)
{
	uint8_t code;

	code = coap_header_get_code(response);
	LOG_INF("Update callback (code:%u.%u)",
		COAP_RESPONSE_CODE_CLASS(code),
		COAP_RESPONSE_CODE_DETAIL(code));

	/* If NOT_FOUND just continue on */
	if ((code == COAP_RESPONSE_CODE_CHANGED) ||
	    (code == COAP_RESPONSE_CODE_CREATED)) {
		set_sm_state(ENGINE_REGISTRATION_DONE);
		LOG_INF("Update Done");
		return 0;
	}

	LOG_ERR("Failed with code %u.%u (%s). Retrying registration.",
		COAP_RESPONSE_CODE_CLASS(code), COAP_RESPONSE_CODE_DETAIL(code),
		code2str(code));

	sm_handle_failure_state(ENGINE_DO_REGISTRATION);

	return 0;
}

static void do_update_timeout_cb(struct lwm2m_message *msg)
{
	LOG_WRN("Registration Update Timeout");

	/* Re-do registration */
	sm_handle_timeout_state(msg, ENGINE_DO_REGISTRATION);
}

static int do_deregister_reply_cb(const struct coap_packet *response,
				  struct coap_reply *reply,
				  const struct sockaddr *from)
{
	uint8_t code;

	code = coap_header_get_code(response);
	LOG_DBG("Deregister callback (code:%u.%u)",
		COAP_RESPONSE_CODE_CLASS(code),
		COAP_RESPONSE_CODE_DETAIL(code));

	if (code == COAP_RESPONSE_CODE_DELETED) {
		LOG_INF("Deregistration success");
		set_sm_state(ENGINE_DEREGISTERED);
		return 0;
	}

	LOG_ERR("Failed with code %u.%u (%s). Not Retrying",
		COAP_RESPONSE_CODE_CLASS(code), COAP_RESPONSE_CODE_DETAIL(code),
		code2str(code));

	sm_handle_failure_state(ENGINE_IDLE);

	return 0;
}

static void do_deregister_timeout_cb(struct lwm2m_message *msg)
{
	LOG_WRN("De-Registration Timeout");

	/* Abort de-registration and start from scratch */
	sm_handle_timeout_state(msg, ENGINE_INIT);
}

static bool sm_bootstrap_verify(bool bootstrap_server, int sec_obj_inst)
{
	char pathstr[MAX_RESOURCE_LEN];
	bool bootstrap;
	int ret;

	snprintk(pathstr, sizeof(pathstr), "0/%d/1", sec_obj_inst);
	ret = lwm2m_engine_get_bool(pathstr, &bootstrap);
	if (ret < 0) {
		LOG_WRN("Failed to check bootstrap, err %d", ret);
		return false;
	}

	if (bootstrap == bootstrap_server) {
		return true;
	} else {
		return false;
	}
}

static bool sm_update_lifetime(int srv_obj_inst, uint32_t *lifetime)
{
	char pathstr[MAX_RESOURCE_LEN];
	uint32_t new_lifetime;

	snprintk(pathstr, sizeof(pathstr), "1/%d/1", srv_obj_inst);
	if (lwm2m_engine_get_u32(pathstr, &new_lifetime) < 0) {
		new_lifetime = CONFIG_LWM2M_ENGINE_DEFAULT_LIFETIME;
		LOG_INF("Using default lifetime: %u", new_lifetime);
	}

	if (new_lifetime != *lifetime) {
		*lifetime = new_lifetime;
		return true;
	}

	return false;
}

static int sm_select_server_inst(int sec_obj_inst, int *srv_obj_inst,
				 uint32_t *lifetime)
{
	char pathstr[MAX_RESOURCE_LEN];
	uint16_t server_id;
	int ret, obj_inst_id;

	snprintk(pathstr, sizeof(pathstr), "0/%d/10", sec_obj_inst);
	ret = lwm2m_engine_get_u16(pathstr, &server_id);
	if (ret < 0) {
		LOG_WRN("Failed to obtain Short Server ID, err %d", ret);
		return -EINVAL;
	}

	obj_inst_id = lwm2m_server_short_id_to_inst(server_id);
	if (obj_inst_id < 0) {
		LOG_WRN("Failed to obtain Server Object instance, err %d",
			obj_inst_id);
		return -EINVAL;
	}

	sm_update_lifetime(obj_inst_id, lifetime);
	*srv_obj_inst = obj_inst_id;

	return 0;
}

static int sm_select_security_inst(bool bootstrap_server, int *sec_obj_inst)
{
	int i, obj_inst_id = -1;

	/* lookup existing index */
	i = lwm2m_security_inst_id_to_index(*sec_obj_inst);
	if (i >= 0 && sm_bootstrap_verify(bootstrap_server, *sec_obj_inst)) {
		return 0;
	}

	*sec_obj_inst = -1;

	/* Iterate over all instances to find the correct one. */
	for (i = 0; i < CONFIG_LWM2M_SECURITY_INSTANCE_COUNT; i++) {
		obj_inst_id = lwm2m_security_index_to_inst_id(i);
		if (obj_inst_id < 0) {
			LOG_WRN("Failed to get inst id for %d", i);
			continue;
		}

		if (sm_bootstrap_verify(bootstrap_server, obj_inst_id)) {
			*sec_obj_inst = obj_inst_id;
			return 0;
		}
	}

	LOG_WRN("sec_obj_inst: No matching servers found.");

	return -ENOENT;
}

/* state machine step functions */

static int sm_do_init(void)
{
	client.ctx->sec_obj_inst = -1;
	client.ctx->srv_obj_inst = -1;
	client.trigger_update = false;
	client.lifetime = 0U;
	client.retries = 0U;

	/* Do bootstrap or registration */
#if defined(CONFIG_LWM2M_RD_CLIENT_SUPPORT_BOOTSTRAP)
	if (client.use_bootstrap) {
		set_sm_state(ENGINE_DO_BOOTSTRAP_REG);
	} else {
		set_sm_state(ENGINE_DO_REGISTRATION);
	}
#else
	set_sm_state(ENGINE_DO_REGISTRATION);
#endif
	return 0;
}

#if defined(CONFIG_LWM2M_RD_CLIENT_SUPPORT_BOOTSTRAP)
static int sm_send_bootstrap_registration(void)
{
	struct lwm2m_message *msg;
	int ret;

	msg = rd_get_message();
	if (!msg) {
		LOG_ERR("Unable to get a lwm2m message!");
		return -ENOMEM;
	}

	msg->type = COAP_TYPE_CON;
	msg->code = COAP_METHOD_POST;
	msg->mid = coap_next_id();
	msg->tkl = LWM2M_MSG_TOKEN_GENERATE_NEW;
	msg->reply_cb = do_bootstrap_reply_cb;
	msg->message_timeout_cb = do_bootstrap_reg_timeout_cb;

	ret = lwm2m_init_message(msg);
	if (ret) {
		goto cleanup;
	}

	/* TODO: handle return error */
	coap_packet_append_option(&msg->cpkt, COAP_OPTION_URI_PATH,
				  "bs", strlen("bs"));

	snprintk(query_buffer, sizeof(query_buffer) - 1, "ep=%s",
		 client.ep_name);
	/* TODO: handle return error */
	coap_packet_append_option(&msg->cpkt, COAP_OPTION_URI_QUERY,
				  query_buffer, strlen(query_buffer));


	if (IS_ENABLED(CONFIG_LWM2M_VERSION_1_1)) {
		int pct = LWM2M_FORMAT_OMA_TLV;

		if (IS_ENABLED(CONFIG_LWM2M_RW_SENML_CBOR_SUPPORT)) {
			pct = LWM2M_FORMAT_APP_SENML_CBOR;
		} else if (IS_ENABLED(CONFIG_LWM2M_RW_SENML_JSON_SUPPORT)) {
			pct = LWM2M_FORMAT_APP_SEML_JSON;
		}

		snprintk(query_buffer, sizeof(query_buffer) - 1, "pct=%d", pct);

		coap_packet_append_option(&msg->cpkt, COAP_OPTION_URI_QUERY,
				  query_buffer, strlen(query_buffer));
	}

	/* log the bootstrap attempt */
	LOG_DBG("Register ID with bootstrap server as '%s'",
		query_buffer);

	lwm2m_send_message_async(msg);

	return 0;

cleanup:
	lwm2m_reset_message(msg, true);
	return ret;
}

static int sm_do_bootstrap_reg(void)
{
	int ret;

	/* clear out existing connection data */
	if (client.ctx->sock_fd > -1) {
		lwm2m_engine_context_close(client.ctx);
	}

	client.ctx->bootstrap_mode = true;
	ret = sm_select_security_inst(client.ctx->bootstrap_mode,
				      &client.ctx->sec_obj_inst);
	if (ret < 0) {
		/* no bootstrap server found, let's move to registration */
		LOG_WRN("Bootstrap server not found! Try normal registration.");
		set_sm_state(ENGINE_DO_REGISTRATION);
		return ret;
	}

	LOG_INF("Bootstrap started with endpoint '%s' with client lifetime %d",
		client.ep_name, client.lifetime);

	ret = lwm2m_engine_start(client.ctx);
	if (ret < 0) {
		LOG_ERR("Cannot init LWM2M engine (%d)", ret);
		set_sm_state(ENGINE_NETWORK_ERROR);
		return ret;
	}

	ret = sm_send_bootstrap_registration();
	if (!ret) {
		set_sm_state(ENGINE_BOOTSTRAP_REG_SENT);
	} else {
		LOG_ERR("Bootstrap registration err: %d", ret);
		set_sm_state(ENGINE_NETWORK_ERROR);
	}

	return ret;
}

void engine_bootstrap_finish(void)
{
	LOG_INF("Bootstrap data transfer done!");
	set_sm_state(ENGINE_BOOTSTRAP_TRANS_DONE);
}

static int sm_bootstrap_trans_done(void)
{
	/* close down context resources */
	lwm2m_engine_context_close(client.ctx);

	/* reset security object instance */
	client.ctx->sec_obj_inst = -1;
	client.use_bootstrap = false;

	set_sm_state(ENGINE_DO_REGISTRATION);

	return 0;
}
#endif

static int sm_send_registration(bool send_obj_support_data,
				coap_reply_t reply_cb,
				lwm2m_message_timeout_cb_t timeout_cb)
{
	struct lwm2m_message *msg;
	int ret;
	char binding[CLIENT_BINDING_LEN];
	char queue[CLIENT_QUEUE_LEN];

	msg = rd_get_message();
	if (!msg) {
		LOG_ERR("Unable to get a lwm2m message!");
		return -ENOMEM;
	}

	/* remember the last reg time */
	client.last_update = k_uptime_get();

	msg->type = COAP_TYPE_CON;
	msg->code = COAP_METHOD_POST;
	msg->mid = coap_next_id();
	msg->tkl = LWM2M_MSG_TOKEN_GENERATE_NEW;
	msg->reply_cb = reply_cb;
	msg->message_timeout_cb = timeout_cb;

	ret = lwm2m_init_message(msg);
	if (ret) {
		goto cleanup;
	}

	ret = coap_packet_append_option(&msg->cpkt, COAP_OPTION_URI_PATH,
					LWM2M_RD_CLIENT_URI,
					strlen(LWM2M_RD_CLIENT_URI));
	if (ret < 0) {
		goto cleanup;
	}

	if (sm_is_registered()) {
		ret = coap_packet_append_option(
			&msg->cpkt, COAP_OPTION_URI_PATH,
			client.server_ep, strlen(client.server_ep));
		if (ret < 0) {
			goto cleanup;
		}
	}

	if (send_obj_support_data) {
		ret = coap_append_option_int(
			&msg->cpkt, COAP_OPTION_CONTENT_FORMAT,
			LWM2M_FORMAT_APP_LINK_FORMAT);
		if (ret < 0) {
			goto cleanup;
		}
	}

	if (!sm_is_registered()) {
		snprintk(query_buffer, sizeof(query_buffer) - 1,
			"lwm2m=%s", LWM2M_PROTOCOL_VERSION_STRING);
		ret = coap_packet_append_option(
			&msg->cpkt, COAP_OPTION_URI_QUERY,
			query_buffer, strlen(query_buffer));
		if (ret < 0) {
			goto cleanup;
		}

		snprintk(query_buffer, sizeof(query_buffer) - 1,
			 "ep=%s", client.ep_name);
		ret = coap_packet_append_option(
			&msg->cpkt, COAP_OPTION_URI_QUERY,
			query_buffer, strlen(query_buffer));
		if (ret < 0) {
			goto cleanup;
		}
	}

	/* Send lifetime only if changed or on initial registration.*/
	if (sm_update_lifetime(client.ctx->srv_obj_inst, &client.lifetime) ||
	    !sm_is_registered()) {
		snprintk(query_buffer, sizeof(query_buffer) - 1,
			 "lt=%d", client.lifetime);
		ret = coap_packet_append_option(
			&msg->cpkt, COAP_OPTION_URI_QUERY,
			query_buffer, strlen(query_buffer));
		if (ret < 0) {
			goto cleanup;
		}
	}

	lwm2m_engine_get_binding(binding);
	lwm2m_engine_get_queue_mode(queue);
	/* UDP is a default binding, no need to add option if UDP without queue is used. */
	if ((!sm_is_registered() && (strcmp(binding, "U") != 0 || strcmp(queue, "Q") == 0))) {
		snprintk(query_buffer, sizeof(query_buffer) - 1,
			 "b=%s", binding);

		ret = coap_packet_append_option(
			&msg->cpkt, COAP_OPTION_URI_QUERY,
			query_buffer, strlen(query_buffer));
		if (ret < 0) {
			goto cleanup;
		}

#if CONFIG_LWM2M_VERSION_1_1
		/* In LwM2M 1.1, queue mode is a separate parameter */
		uint16_t len = strlen(queue);

		if (len) {
			ret = coap_packet_append_option(
				&msg->cpkt, COAP_OPTION_URI_QUERY,
				queue, len);
			if (ret < 0) {
				goto cleanup;
			}
		}
#endif
	}

	if (send_obj_support_data) {
		ret = coap_packet_append_payload_marker(&msg->cpkt);
		if (ret < 0) {
			goto cleanup;
		}

		msg->out.out_cpkt = &msg->cpkt;
		msg->out.writer = &link_format_writer;

		ret = do_register_op_link_format(msg);
		if (ret < 0) {
			goto cleanup;
		}
	}

	lwm2m_send_message_async(msg);

	/* log the registration attempt */
	LOG_DBG("registration sent [%s]",
		lwm2m_sprint_ip_addr(&client.ctx->remote_addr));

	return 0;

cleanup:
	LOG_ERR("error %d when sending registration message", ret);
	lwm2m_reset_message(msg, true);
	return ret;
}

static void sm_handle_registration_update_failure(void)
{
	int ret;

	LOG_WRN("Registration Update fail -> trigger full registration");
	client.engine_state = ENGINE_DO_REGISTRATION;
	ret = sm_send_registration(true, do_registration_reply_cb, do_registration_timeout_cb);
	if (!ret) {
		set_sm_state(ENGINE_REGISTRATION_SENT);
	} else {
		LOG_ERR("Registration err: %d", ret);
		set_sm_state(ENGINE_NETWORK_ERROR);
	}
}

static int sm_do_registration(void)
{
	int ret = 0;

	/* clear out existing connection data */
	if (client.ctx->sock_fd > -1) {
		lwm2m_engine_context_close(client.ctx);
	}

	client.ctx->bootstrap_mode = false;
	ret = sm_select_security_inst(client.ctx->bootstrap_mode,
				      &client.ctx->sec_obj_inst);
	if (ret < 0) {
		LOG_ERR("Unable to find a valid security instance.");
		set_sm_state(ENGINE_INIT);
		return -EINVAL;
	}

	ret = sm_select_server_inst(client.ctx->sec_obj_inst,
				    &client.ctx->srv_obj_inst,
				    &client.lifetime);
	if (ret < 0) {
		LOG_ERR("Unable to find a valid server instance.");
		set_sm_state(ENGINE_INIT);
		return -EINVAL;
	}

	LOG_INF("RD Client started with endpoint '%s' with client lifetime %d",
		client.ep_name, client.lifetime);

	ret = lwm2m_engine_start(client.ctx);
	if (ret < 0) {
		LOG_ERR("Cannot init LWM2M engine (%d)", ret);
		set_sm_state(ENGINE_NETWORK_ERROR);
		return ret;
	}

	ret = sm_send_registration(true,
				   do_registration_reply_cb,
				   do_registration_timeout_cb);
	if (!ret) {
		set_sm_state(ENGINE_REGISTRATION_SENT);
	} else {
		LOG_ERR("Registration err: %d", ret);
		set_sm_state(ENGINE_NETWORK_ERROR);
	}

	return ret;
}

static int sm_registration_done(void)
{
	int ret = 0;
	bool update_objects;

	/*
	 * check for lifetime seconds - SECONDS_TO_UPDATE_EARLY
	 * so that we can update early and avoid lifetime timeout
	 */
	if (sm_is_registered() &&
	    (client.trigger_update ||
	     ((client.lifetime - SECONDS_TO_UPDATE_EARLY) <=
	      (k_uptime_get() - client.last_update) / 1000))) {
		update_objects = client.update_objects;
		client.trigger_update = false;
		client.update_objects = false;
#if defined(CONFIG_LWM2M_QUEUE_MODE_ENABLED)
		ret = lwm2m_engine_connection_resume(client.ctx);
		if (ret) {
			lwm2m_engine_context_close(client.ctx);
			/* perform full registration */
			set_sm_state(ENGINE_DO_REGISTRATION);
			return ret;
		}
#endif

		ret = sm_send_registration(update_objects,
					   do_update_reply_cb,
					   do_update_timeout_cb);
		if (!ret) {
			set_sm_state(ENGINE_UPDATE_SENT);
		} else {
			LOG_ERR("Registration update err: %d", ret);
			lwm2m_engine_context_close(client.ctx);
			/* perform full registration */
			set_sm_state(ENGINE_DO_REGISTRATION);
		}
	}

	if (IS_ENABLED(CONFIG_LWM2M_QUEUE_MODE_ENABLED) &&
	    (client.engine_state != ENGINE_REGISTRATION_DONE_RX_OFF) &&
	    (((k_uptime_get() - client.last_tx) / 1000) >=
	     CONFIG_LWM2M_QUEUE_MODE_UPTIME)) {
		set_sm_state(ENGINE_REGISTRATION_DONE_RX_OFF);
	}

	return ret;
}

static int sm_do_deregister(void)
{
	struct lwm2m_message *msg;
	int ret;

	msg = rd_get_message();
	if (!msg) {
		LOG_ERR("Unable to get a lwm2m message!");
		ret = -ENOMEM;
		goto close_ctx;
	}

	msg->type = COAP_TYPE_CON;
	msg->code = COAP_METHOD_DELETE;
	msg->mid = coap_next_id();
	msg->tkl = LWM2M_MSG_TOKEN_GENERATE_NEW;
	msg->reply_cb = do_deregister_reply_cb;
	msg->message_timeout_cb = do_deregister_timeout_cb;

	ret = lwm2m_init_message(msg);
	if (ret) {
		goto cleanup;
	}

	ret = coap_packet_append_option(&msg->cpkt, COAP_OPTION_URI_PATH,
					LWM2M_RD_CLIENT_URI,
					strlen(LWM2M_RD_CLIENT_URI));
	if (ret < 0) {
		LOG_ERR("Failed to encode URI path option (err:%d).", ret);
		goto cleanup;
	}

	/* include server endpoint in URI PATH */
	ret = coap_packet_append_option(&msg->cpkt, COAP_OPTION_URI_PATH,
					client.server_ep,
					strlen(client.server_ep));
	if (ret < 0) {
		LOG_ERR("Failed to encode URI path option (err:%d).", ret);
		goto cleanup;
	}

	LOG_INF("Deregister from '%s'", client.server_ep);

	lwm2m_send_message_async(msg);

	set_sm_state(ENGINE_DEREGISTER_SENT);
	return 0;

cleanup:
	lwm2m_reset_message(msg, true);
close_ctx:
	lwm2m_engine_context_close(client.ctx);
	set_sm_state(ENGINE_DEREGISTERED);
	return ret;
}

static void sm_do_network_error(void)
{
	if (--client.retry_delay > 0) {
		return;
	}

#if defined(CONFIG_LWM2M_RD_CLIENT_SUPPORT_BOOTSTRAP)
	if (client.ctx->bootstrap_mode) {
		set_sm_state(ENGINE_DO_BOOTSTRAP_REG);
		return;
	}
#endif

	set_sm_state(ENGINE_DO_REGISTRATION);
}

static void lwm2m_rd_client_service(struct k_work *work)
{
	k_mutex_lock(&client.mutex, K_FOREVER);

	if (client.ctx) {
		switch (get_sm_state()) {
		case ENGINE_IDLE:
			if (client.ctx->sock_fd > -1) {
				lwm2m_engine_context_close(client.ctx);
			}
			break;

		case ENGINE_INIT:
			sm_do_init();
			break;

#if defined(CONFIG_LWM2M_RD_CLIENT_SUPPORT_BOOTSTRAP)
		case ENGINE_DO_BOOTSTRAP_REG:
			sm_do_bootstrap_reg();
			break;

		case ENGINE_BOOTSTRAP_REG_SENT:
			/* wait for bootstrap registration done */
			break;

		case ENGINE_BOOTSTRAP_REG_DONE:
			/* wait for transfer done */
			break;

		case ENGINE_BOOTSTRAP_TRANS_DONE:
			sm_bootstrap_trans_done();
			break;
#endif

		case ENGINE_DO_REGISTRATION:
			sm_do_registration();
			break;

		case ENGINE_REGISTRATION_SENT:
			/* wait registration to be done or timeout */
			break;

		case ENGINE_REGISTRATION_DONE:
		case ENGINE_REGISTRATION_DONE_RX_OFF:
			sm_registration_done();
			break;

		case ENGINE_UPDATE_SENT:
			/* wait update to be done or abort */
			break;

		case ENGINE_DEREGISTER:
			sm_do_deregister();
			break;

		case ENGINE_DEREGISTER_SENT:
			/* wait for deregister to be done or reset */
			break;

		case ENGINE_DEREGISTERED:
			set_sm_state(ENGINE_IDLE);
			break;

		case ENGINE_NETWORK_ERROR:
			sm_do_network_error();
			break;

		default:
			LOG_ERR("Unhandled state: %d", get_sm_state());

		}
	}

	k_mutex_unlock(&client.mutex);
}

int lwm2m_rd_client_start(struct lwm2m_ctx *client_ctx, const char *ep_name,
			   uint32_t flags, lwm2m_ctx_event_cb_t event_cb,
			   lwm2m_observe_cb_t observe_cb)
{
	k_mutex_lock(&client.mutex, K_FOREVER);

	if (!IS_ENABLED(CONFIG_LWM2M_RD_CLIENT_SUPPORT_BOOTSTRAP) &&
	    (flags & LWM2M_RD_CLIENT_FLAG_BOOTSTRAP)) {
		LOG_ERR("Bootstrap support is disabled. Please enable "
			"CONFIG_LWM2M_RD_CLIENT_SUPPORT_BOOTSTRAP.");

		k_mutex_unlock(&client.mutex);
		return -ENOTSUP;
	}

	/* Check client idle state or socket is still active */

	if (client.ctx && (client.engine_state != ENGINE_IDLE || client.ctx->sock_fd != -1)) {
		LOG_WRN("Client is already running. state %d ", client.engine_state);
		k_mutex_unlock(&client.mutex);
		return -EINPROGRESS;
	}

	client.ctx = client_ctx;
	client.ctx->sock_fd = -1;
	client.ctx->fault_cb = socket_fault_cb;
	client.ctx->observe_cb = observe_cb;
	client.ctx->event_cb = event_cb;
	client.use_bootstrap = flags & LWM2M_RD_CLIENT_FLAG_BOOTSTRAP;

	set_sm_state(ENGINE_INIT);
	strncpy(client.ep_name, ep_name, CLIENT_EP_LEN - 1);
	client.ep_name[CLIENT_EP_LEN - 1] = '\0';
	LOG_INF("Start LWM2M Client: %s", client.ep_name);

	k_mutex_unlock(&client.mutex);
	return 0;
}

int lwm2m_rd_client_stop(struct lwm2m_ctx *client_ctx,
			   lwm2m_ctx_event_cb_t event_cb, bool deregister)
{
	k_mutex_lock(&client.mutex, K_FOREVER);

	if (client.ctx != client_ctx) {
		k_mutex_unlock(&client.mutex);
		LOG_WRN("Cannot stop. Wrong context");
		return -EPERM;
	}

	client.ctx->event_cb = event_cb;

	if (sm_is_registered() && deregister) {
		set_sm_state(ENGINE_DEREGISTER);
	} else {
		set_sm_state(ENGINE_DEREGISTERED);
	}

	LOG_INF("Stop LWM2M Client: %s", client.ep_name);

	k_mutex_unlock(&client.mutex);

	while (get_sm_state() != ENGINE_IDLE) {
		k_sleep(K_MSEC(STATE_MACHINE_UPDATE_INTERVAL_MS / 2));
	}
	return 0;
}

void lwm2m_rd_client_update(void)
{
	engine_trigger_update(false);
}

struct lwm2m_ctx *lwm2m_rd_client_ctx(void)
{
	return client.ctx;
}

#if defined(CONFIG_LWM2M_QUEUE_MODE_ENABLED)
int lwm2m_rd_client_connection_resume(struct lwm2m_ctx *client_ctx)
{
	if (client.ctx != client_ctx) {
		return -EPERM;
	}

	if (client.engine_state == ENGINE_REGISTRATION_DONE_RX_OFF) {
#ifdef CONFIG_LWM2M_DTLS_SUPPORT
		/*
		 * Switch state for triggering a proper registration message
		 * if CONFIG_LWM2M_TLS_SESSION_CACHING is false we force full
		 * registration after Fully DTLS handshake
		 */
		if (IS_ENABLED(CONFIG_LWM2M_TLS_SESSION_CACHING)) {
			client.engine_state = ENGINE_REGISTRATION_DONE;
		} else {
			client.engine_state = ENGINE_DO_REGISTRATION;
		}
#else
		client.engine_state = ENGINE_REGISTRATION_DONE;
#endif
		client.trigger_update = true;
	}

	return 0;
}
#endif

int lwm2m_rd_client_timeout(struct lwm2m_ctx *client_ctx)
{
	if (client.ctx != client_ctx) {
		return -EPERM;
	}

	if (!sm_is_registered()) {
		return 0;
	}

	LOG_WRN("Confirmable Timeout -> Re-connect and register");
	client.engine_state = ENGINE_DO_REGISTRATION;
	return 0;
}

bool lwm2m_rd_client_is_registred(struct lwm2m_ctx *client_ctx)
{
	if (client.ctx != client_ctx || !sm_is_registered()) {
		return false;
	}

	return true;
}

static int lwm2m_rd_client_init(const struct device *dev)
{
	client.ctx = NULL;
	client.rd_message.ctx = NULL;
	client.engine_state = ENGINE_IDLE;
	k_mutex_init(&client.mutex);

	return lwm2m_engine_add_service(lwm2m_rd_client_service,
					STATE_MACHINE_UPDATE_INTERVAL_MS);

}

SYS_INIT(lwm2m_rd_client_init, APPLICATION,
	 CONFIG_KERNEL_INIT_PRIORITY_DEFAULT);
