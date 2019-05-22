/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <logging/log.h>
LOG_MODULE_REGISTER(net_mqtt_rx, CONFIG_MQTT_LOG_LEVEL);

#include "mqtt_internal.h"
#include "mqtt_transport.h"
#include "mqtt_os.h"

/** @file mqtt_rx.c
 *
 * @brief MQTT Received data handling.
 */

static int mqtt_handle_packet(struct mqtt_client *client,
			      u8_t type_and_flags,
			      u32_t var_length,
			      struct buf_ctx *buf)
{
	int err_code = 0;
	bool notify_event = true;
	struct mqtt_evt evt;

	/* Success by default, overwritten in special cases. */
	evt.result = 0;

	switch (type_and_flags & 0xF0) {
	case MQTT_PKT_TYPE_CONNACK:
		MQTT_TRC("[CID %p]: Received MQTT_PKT_TYPE_CONNACK!", client);

		evt.type = MQTT_EVT_CONNACK;
		err_code = connect_ack_decode(client, buf, &evt.param.connack);
		if (err_code == 0) {
			MQTT_TRC("[CID %p]: return_code: %d", client,
				 evt.param.connack.return_code);

			if (evt.param.connack.return_code ==
						MQTT_CONNECTION_ACCEPTED) {
				/* Set state. */
				MQTT_SET_STATE(client, MQTT_STATE_CONNECTED);
			}

			evt.result = evt.param.connack.return_code;
		} else {
			evt.result = err_code;
		}

		break;

	case MQTT_PKT_TYPE_PUBLISH:
		MQTT_TRC("[CID %p]: Received MQTT_PKT_TYPE_PUBLISH", client);

		evt.type = MQTT_EVT_PUBLISH;
		err_code = publish_decode(type_and_flags, var_length, buf,
					  &evt.param.publish);
		evt.result = err_code;

		client->internal.remaining_payload =
					evt.param.publish.message.payload.len;

		MQTT_TRC("PUB QoS:%02x, message len %08x, topic len %08x",
			 evt.param.publish.message.topic.qos,
			 evt.param.publish.message.payload.len,
			 evt.param.publish.message.topic.topic.size);

		break;

	case MQTT_PKT_TYPE_PUBACK:
		MQTT_TRC("[CID %p]: Received MQTT_PKT_TYPE_PUBACK!", client);

		evt.type = MQTT_EVT_PUBACK;
		err_code = publish_ack_decode(buf, &evt.param.puback);
		evt.result = err_code;
		break;

	case MQTT_PKT_TYPE_PUBREC:
		MQTT_TRC("[CID %p]: Received MQTT_PKT_TYPE_PUBREC!", client);

		evt.type = MQTT_EVT_PUBREC;
		err_code = publish_receive_decode(buf, &evt.param.pubrec);
		evt.result = err_code;
		break;

	case MQTT_PKT_TYPE_PUBREL:
		MQTT_TRC("[CID %p]: Received MQTT_PKT_TYPE_PUBREL!", client);

		evt.type = MQTT_EVT_PUBREL;
		err_code = publish_release_decode(buf, &evt.param.pubrel);
		evt.result = err_code;
		break;

	case MQTT_PKT_TYPE_PUBCOMP:
		MQTT_TRC("[CID %p]: Received MQTT_PKT_TYPE_PUBCOMP!", client);

		evt.type = MQTT_EVT_PUBCOMP;
		err_code = publish_complete_decode(buf, &evt.param.pubcomp);
		evt.result = err_code;
		break;

	case MQTT_PKT_TYPE_SUBACK:
		MQTT_TRC("[CID %p]: Received MQTT_PKT_TYPE_SUBACK!", client);

		evt.type = MQTT_EVT_SUBACK;
		err_code = subscribe_ack_decode(buf, &evt.param.suback);
		evt.result = err_code;
		break;

	case MQTT_PKT_TYPE_UNSUBACK:
		MQTT_TRC("[CID %p]: Received MQTT_PKT_TYPE_UNSUBACK!", client);

		evt.type = MQTT_EVT_UNSUBACK;
		err_code = unsubscribe_ack_decode(buf, &evt.param.unsuback);
		evt.result = err_code;
		break;

	case MQTT_PKT_TYPE_PINGRSP:
		MQTT_TRC("[CID %p]: Received MQTT_PKT_TYPE_PINGRSP!", client);

		/* No notification of Ping response to application. */
		notify_event = false;
		break;

	default:
		/* Nothing to notify. */
		notify_event = false;
		break;
	}

	if (notify_event == true) {
		event_notify(client, &evt);
	}

	return err_code;
}

static int mqtt_read_message_chunk(struct mqtt_client *client,
				   struct buf_ctx *buf, u32_t length)
{
	int remaining;
	int len;

	/* Calculate how much data we need to read from the transport,
	 * given the already buffered data.
	 */
	remaining = length - (buf->end - buf->cur);
	if (remaining <= 0) {
		return 0;
	}

	/* Check if read does not exceed the buffer. */
	if (buf->end + remaining > client->rx_buf + client->rx_buf_size) {
		MQTT_ERR("[CID %p]: Buffer too small to receive the message",
			 client);
		return -ENOMEM;
	}

	len = mqtt_transport_read(client, buf->end, remaining, false);
	if (len < 0) {
		MQTT_TRC("[CID %p]: Transport read error: %d", client, len);
		return len;
	}

	if (len == 0) {
		MQTT_TRC("[CID %p]: Connection closed.", client);
		return -ENOTCONN;
	}

	client->internal.rx_buf_datalen += len;
	buf->end += len;

	if (len < remaining) {
		MQTT_TRC("[CID %p]: Message partially received.", client);
		return -EAGAIN;
	}

	return 0;
}

static int mqtt_read_publish_var_header(struct mqtt_client *client,
					u8_t type_and_flags,
					struct buf_ctx *buf)
{
	u8_t qos = (type_and_flags & MQTT_HEADER_QOS_MASK) >> 1;
	int err_code;
	u32_t variable_header_length;

	/* Read topic length field. */
	err_code = mqtt_read_message_chunk(client, buf, sizeof(u16_t));
	if (err_code < 0) {
		return err_code;
	}

	variable_header_length = *buf->cur << 8; /* MSB */
	variable_header_length |= *(buf->cur + 1); /* LSB */

	/* Add two bytes for topic length field. */
	variable_header_length += sizeof(u16_t);

	/* Add two bytes for message_id, if needed. */
	if (qos > MQTT_QOS_0_AT_MOST_ONCE) {
		variable_header_length += sizeof(u16_t);
	}

	/* Now we can read the whole header. */
	err_code = mqtt_read_message_chunk(client, buf,
					   variable_header_length);
	if (err_code < 0) {
		return err_code;
	}

	return 0;
}

static int mqtt_read_and_parse_fixed_header(struct mqtt_client *client,
					    u8_t *type_and_flags,
					    u32_t *var_length,
					    struct buf_ctx *buf)
{
	/* Read the mandatory part of the fixed header in first iteration. */
	u8_t chunk_size = MQTT_FIXED_HEADER_MIN_SIZE;
	int err_code;

	do {
		err_code = mqtt_read_message_chunk(client, buf, chunk_size);
		if (err_code < 0) {
			return err_code;
		}

		/* Reset to pointer to the beginning of the frame. */
		buf->cur = client->rx_buf;
		chunk_size = 1U;

		err_code = fixed_header_decode(buf, type_and_flags, var_length);
	} while (err_code == -EAGAIN);

	return err_code;
}

int mqtt_handle_rx(struct mqtt_client *client)
{
	int err_code;
	u8_t type_and_flags;
	u32_t var_length;
	struct buf_ctx buf;

	buf.cur = client->rx_buf;
	buf.end = client->rx_buf + client->internal.rx_buf_datalen;

	err_code = mqtt_read_and_parse_fixed_header(client, &type_and_flags,
						    &var_length, &buf);
	if (err_code < 0) {
		return (err_code == -EAGAIN) ? 0 : err_code;
	}

	if ((type_and_flags & 0xF0) == MQTT_PKT_TYPE_PUBLISH) {
		err_code = mqtt_read_publish_var_header(client, type_and_flags,
							&buf);
	} else {
		err_code = mqtt_read_message_chunk(client, &buf, var_length);
	}

	if (err_code < 0) {
		return (err_code == -EAGAIN) ? 0 : err_code;
	}

	/* At this point, packet is ready to be passed to the application. */
	err_code = mqtt_handle_packet(client, type_and_flags, var_length, &buf);
	if (err_code < 0) {
		return err_code;
	}

	client->internal.rx_buf_datalen = 0U;

	return 0;
}
