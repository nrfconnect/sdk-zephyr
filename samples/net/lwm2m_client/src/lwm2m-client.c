/*
 * Copyright (c) 2017 Linaro Limited
 * Copyright (c) 2017-2019 Foundries.io
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_MODULE_NAME net_lwm2m_client_app
#define LOG_LEVEL LOG_LEVEL_DBG

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(LOG_MODULE_NAME);

#include <zephyr/drivers/hwinfo.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/net/lwm2m.h>
#include "modules.h"

#define APP_BANNER "Run LWM2M client"

#if !defined(CONFIG_NET_CONFIG_PEER_IPV4_ADDR)
#define CONFIG_NET_CONFIG_PEER_IPV4_ADDR ""
#endif

#if !defined(CONFIG_NET_CONFIG_PEER_IPV6_ADDR)
#define CONFIG_NET_CONFIG_PEER_IPV6_ADDR ""
#endif

#if defined(CONFIG_NET_IPV6)
#define SERVER_ADDR CONFIG_NET_CONFIG_PEER_IPV6_ADDR
#elif defined(CONFIG_NET_IPV4)
#define SERVER_ADDR CONFIG_NET_CONFIG_PEER_IPV4_ADDR
#else
#error LwM2M requires either IPV6 or IPV4 support
#endif


#define WAIT_TIME	K_SECONDS(10)
#define CONNECT_TIME	K_SECONDS(10)

#define CLIENT_MANUFACTURER	"Zephyr"
#define CLIENT_MODEL_NUMBER	"OMA-LWM2M Sample Client"
#define CLIENT_SERIAL_NUMBER	"345000123"
#define CLIENT_FIRMWARE_VER	"1.0"
#define CLIENT_DEVICE_TYPE	"OMA-LWM2M Client"
#define CLIENT_HW_VER		"1.0.1"

#define ENDPOINT_LEN		32

static uint8_t bat_idx = LWM2M_DEVICE_PWR_SRC_TYPE_BAT_INT;
static int bat_mv = 3800;
static int bat_ma = 125;
static uint8_t usb_idx = LWM2M_DEVICE_PWR_SRC_TYPE_USB;
static int usb_mv = 5000;
static int usb_ma = 900;
static uint8_t bat_level = 95;
static uint8_t bat_status = LWM2M_DEVICE_BATTERY_STATUS_CHARGING;
static int mem_free = 15;
static int mem_total = 25;

static struct lwm2m_ctx client;

#if defined(CONFIG_LWM2M_DTLS_SUPPORT)
#define TLS_TAG			1

/* "000102030405060708090a0b0c0d0e0f" */
static unsigned char client_psk[] = {
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
	0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f
};

static const char client_psk_id[] = "Client_identity";
#endif /* CONFIG_LWM2M_DTLS_SUPPORT */

static struct k_sem quit_lock;

static int device_reboot_cb(uint16_t obj_inst_id,
			    uint8_t *args, uint16_t args_len)
{
	LOG_INF("DEVICE: REBOOT");
	/* Add an error for testing */
	lwm2m_device_add_err(LWM2M_DEVICE_ERROR_LOW_POWER);
	/* Change the battery voltage for testing */
	lwm2m_set_s32(&LWM2M_OBJ(3, 0, 7, 0), (bat_mv - 1));

	return 0;
}

static int device_factory_default_cb(uint16_t obj_inst_id,
				     uint8_t *args, uint16_t args_len)
{
	LOG_INF("DEVICE: FACTORY DEFAULT");
	/* Add an error for testing */
	lwm2m_device_add_err(LWM2M_DEVICE_ERROR_GPS_FAILURE);
	/* Change the USB current for testing */
	lwm2m_set_s32(&LWM2M_OBJ(3, 0, 8, 1), (usb_ma - 1));

	return 0;
}


static int lwm2m_setup(void)
{
	int ret;
	char *server_url;
	uint16_t server_url_len;

	/* setup SECURITY object */

	/* Server URL */
	ret = lwm2m_get_res_buf(&LWM2M_OBJ(0, 0, 0), (void **)&server_url, &server_url_len, NULL,
				NULL);
	if (ret < 0) {
		return ret;
	}

	server_url_len = snprintk(server_url, server_url_len, "coap%s//%s%s%s",
				  IS_ENABLED(CONFIG_LWM2M_DTLS_SUPPORT) ? "s:" : ":",
				  strchr(SERVER_ADDR, ':') ? "[" : "", SERVER_ADDR,
				  strchr(SERVER_ADDR, ':') ? "]" : "");

	lwm2m_set_res_data_len(&LWM2M_OBJ(0, 0, 0), server_url_len + 1);

	/* Security Mode */
	lwm2m_set_u8(&LWM2M_OBJ(0, 0, 2), IS_ENABLED(CONFIG_LWM2M_DTLS_SUPPORT) ? 0 : 3);
#if defined(CONFIG_LWM2M_DTLS_SUPPORT)
	lwm2m_set_string(&LWM2M_OBJ(0, 0, 3), (char *)client_psk_id);
	lwm2m_set_opaque(&LWM2M_OBJ(0, 0, 5),
				(void *)client_psk, sizeof(client_psk));
#endif /* CONFIG_LWM2M_DTLS_SUPPORT */

#if defined(CONFIG_LWM2M_RD_CLIENT_SUPPORT_BOOTSTRAP)
	/* Mark 1st instance of security object as a bootstrap server */
	lwm2m_set_u8(&LWM2M_OBJ(0, 0, 1), 1);

	/* Create 2nd instance of security object needed for bootstrap */
	lwm2m_create_object_inst(&LWM2M_OBJ(0, 1));
#else
	/* Match Security object instance with a Server object instance with
	 * Short Server ID.
	 */
	lwm2m_set_u16(&LWM2M_OBJ(0, 0, 10), CONFIG_LWM2M_SERVER_DEFAULT_SSID);
	lwm2m_set_u16(&LWM2M_OBJ(1, 0, 0), CONFIG_LWM2M_SERVER_DEFAULT_SSID);
#endif

	/* setup SERVER object */

	/* setup DEVICE object */

	lwm2m_set_res_buf(&LWM2M_OBJ(3, 0, 0), CLIENT_MANUFACTURER, sizeof(CLIENT_MANUFACTURER),
			  sizeof(CLIENT_MANUFACTURER), LWM2M_RES_DATA_FLAG_RO);
	lwm2m_set_res_buf(&LWM2M_OBJ(3, 0, 1), CLIENT_MODEL_NUMBER, sizeof(CLIENT_MODEL_NUMBER),
			  sizeof(CLIENT_MODEL_NUMBER), LWM2M_RES_DATA_FLAG_RO);
	lwm2m_set_res_buf(&LWM2M_OBJ(3, 0, 2), CLIENT_SERIAL_NUMBER, sizeof(CLIENT_SERIAL_NUMBER),
			  sizeof(CLIENT_SERIAL_NUMBER), LWM2M_RES_DATA_FLAG_RO);
	lwm2m_set_res_buf(&LWM2M_OBJ(3, 0, 3), CLIENT_FIRMWARE_VER, sizeof(CLIENT_FIRMWARE_VER),
			  sizeof(CLIENT_FIRMWARE_VER), LWM2M_RES_DATA_FLAG_RO);
	lwm2m_register_exec_callback(&LWM2M_OBJ(3, 0, 4), device_reboot_cb);
	lwm2m_register_exec_callback(&LWM2M_OBJ(3, 0, 5), device_factory_default_cb);
	lwm2m_set_res_buf(&LWM2M_OBJ(3, 0, 9), &bat_level, sizeof(bat_level), sizeof(bat_level), 0);
	lwm2m_set_res_buf(&LWM2M_OBJ(3, 0, 10), &mem_free, sizeof(mem_free), sizeof(mem_free), 0);
	lwm2m_set_res_buf(&LWM2M_OBJ(3, 0, 17), CLIENT_DEVICE_TYPE, sizeof(CLIENT_DEVICE_TYPE),
			  sizeof(CLIENT_DEVICE_TYPE), LWM2M_RES_DATA_FLAG_RO);
	lwm2m_set_res_buf(&LWM2M_OBJ(3, 0, 18), CLIENT_HW_VER, sizeof(CLIENT_HW_VER),
			  sizeof(CLIENT_HW_VER), LWM2M_RES_DATA_FLAG_RO);
	lwm2m_set_res_buf(&LWM2M_OBJ(3, 0, 20), &bat_status, sizeof(bat_status),
			  sizeof(bat_status), 0);
	lwm2m_set_res_buf(&LWM2M_OBJ(3, 0, 21), &mem_total, sizeof(mem_total),
			  sizeof(mem_total), 0);

	/* add power source resource instances */
	lwm2m_create_res_inst(&LWM2M_OBJ(3, 0, 6, 0));
	lwm2m_set_res_buf(&LWM2M_OBJ(3, 0, 6, 0), &bat_idx, sizeof(bat_idx), sizeof(bat_idx), 0);
	lwm2m_create_res_inst(&LWM2M_OBJ(3, 0, 7, 0));
	lwm2m_set_res_buf(&LWM2M_OBJ(3, 0, 7, 0), &bat_mv, sizeof(bat_mv), sizeof(bat_mv), 0);
	lwm2m_create_res_inst(&LWM2M_OBJ(3, 0, 8, 0));
	lwm2m_set_res_buf(&LWM2M_OBJ(3, 0, 8, 0), &bat_ma, sizeof(bat_ma), sizeof(bat_ma), 0);
	lwm2m_create_res_inst(&LWM2M_OBJ(3, 0, 6, 1));
	lwm2m_set_res_buf(&LWM2M_OBJ(3, 0, 6, 1), &usb_idx, sizeof(usb_idx), sizeof(usb_idx), 0);
	lwm2m_create_res_inst(&LWM2M_OBJ(3, 0, 7, 1));
	lwm2m_set_res_buf(&LWM2M_OBJ(3, 0, 7, 1), &usb_mv, sizeof(usb_mv), sizeof(usb_mv), 0);
	lwm2m_create_res_inst(&LWM2M_OBJ(3, 0, 8, 1));
	lwm2m_set_res_buf(&LWM2M_OBJ(3, 0, 8, 1), &usb_ma, sizeof(usb_ma), sizeof(usb_ma), 0);

	/* setup FIRMWARE object */
	if (IS_ENABLED(CONFIG_LWM2M_FIRMWARE_UPDATE_OBJ_SUPPORT)) {
		init_firmware_update();
	}

	/* setup TEMP SENSOR object */
	init_temp_sensor();

	/* IPSO: Light Control object */
	init_led_device();

	/* IPSO: Timer object */
	init_timer_object();

	return 0;
}

static void rd_client_event(struct lwm2m_ctx *client,
			    enum lwm2m_rd_client_event client_event)
{
	switch (client_event) {

	case LWM2M_RD_CLIENT_EVENT_NONE:
		/* do nothing */
		break;

	case LWM2M_RD_CLIENT_EVENT_BOOTSTRAP_REG_FAILURE:
		LOG_DBG("Bootstrap registration failure!");
		break;

	case LWM2M_RD_CLIENT_EVENT_BOOTSTRAP_REG_COMPLETE:
		LOG_DBG("Bootstrap registration complete");
		break;

	case LWM2M_RD_CLIENT_EVENT_BOOTSTRAP_TRANSFER_COMPLETE:
		LOG_DBG("Bootstrap transfer complete");
		break;

	case LWM2M_RD_CLIENT_EVENT_REGISTRATION_FAILURE:
		LOG_DBG("Registration failure!");
		break;

	case LWM2M_RD_CLIENT_EVENT_REGISTRATION_COMPLETE:
		LOG_DBG("Registration complete");
		break;

	case LWM2M_RD_CLIENT_EVENT_REG_TIMEOUT:
		LOG_DBG("Registration timeout!");
		break;

	case LWM2M_RD_CLIENT_EVENT_REG_UPDATE_COMPLETE:
		LOG_DBG("Registration update complete");
		break;

	case LWM2M_RD_CLIENT_EVENT_DEREGISTER_FAILURE:
		LOG_DBG("Deregister failure!");
		break;

	case LWM2M_RD_CLIENT_EVENT_DISCONNECT:
		LOG_DBG("Disconnected");
		break;

	case LWM2M_RD_CLIENT_EVENT_QUEUE_MODE_RX_OFF:
		LOG_DBG("Queue mode RX window closed");
		break;

	case LWM2M_RD_CLIENT_EVENT_ENGINE_SUSPENDED:
		LOG_DBG("LwM2M engine suspended");
		break;

	case LWM2M_RD_CLIENT_EVENT_NETWORK_ERROR:
		LOG_ERR("LwM2M engine reported a network error.");
		lwm2m_rd_client_stop(client, rd_client_event, true);
		break;

	case LWM2M_RD_CLIENT_EVENT_REG_UPDATE:
		LOG_DBG("Registration update");
		break;
	}
}

static void observe_cb(enum lwm2m_observe_event event,
		       struct lwm2m_obj_path *path, void *user_data)
{
	char buf[LWM2M_MAX_PATH_STR_SIZE];

	switch (event) {

	case LWM2M_OBSERVE_EVENT_OBSERVER_ADDED:
		LOG_INF("Observer added for %s", lwm2m_path_log_buf(buf, path));
		break;

	case LWM2M_OBSERVE_EVENT_OBSERVER_REMOVED:
		LOG_INF("Observer removed for %s", lwm2m_path_log_buf(buf, path));
		break;

	case LWM2M_OBSERVE_EVENT_NOTIFY_ACK:
		LOG_INF("Notify acknowledged for %s", lwm2m_path_log_buf(buf, path));
		break;

	case LWM2M_OBSERVE_EVENT_NOTIFY_TIMEOUT:
		LOG_INF("Notify timeout for %s, trying registration update",
			lwm2m_path_log_buf(buf, path));

		lwm2m_rd_client_update();
		break;
	}
}

void main(void)
{
	uint32_t flags = IS_ENABLED(CONFIG_LWM2M_RD_CLIENT_SUPPORT_BOOTSTRAP) ?
				LWM2M_RD_CLIENT_FLAG_BOOTSTRAP : 0;
	int ret;

	LOG_INF(APP_BANNER);

	k_sem_init(&quit_lock, 0, K_SEM_MAX_LIMIT);

	ret = lwm2m_setup();
	if (ret < 0) {
		LOG_ERR("Cannot setup LWM2M fields (%d)", ret);
		return;
	}

	(void)memset(&client, 0x0, sizeof(client));
#if defined(CONFIG_LWM2M_DTLS_SUPPORT)
	client.tls_tag = TLS_TAG;
#endif

#if defined(CONFIG_HWINFO)
	uint8_t dev_id[16];
	char dev_str[33];
	ssize_t length;
	int i;

	(void)memset(dev_id, 0x0, sizeof(dev_id));

	/* Obtain the device id */
	length = hwinfo_get_device_id(dev_id, sizeof(dev_id));

	/* If this fails for some reason, use all zeros instead */
	if (length <= 0) {
		length = sizeof(dev_id);
	}

	/* Render the obtained serial number in hexadecimal representation */
	for (i = 0 ; i < length ; i++) {
		sprintf(&dev_str[i*2], "%02x", dev_id[i]);
	}

	lwm2m_rd_client_start(&client, dev_str, flags, rd_client_event, observe_cb);
#else
	/* client.sec_obj_inst is 0 as a starting point */
	lwm2m_rd_client_start(&client, CONFIG_BOARD, flags, rd_client_event, observe_cb);
#endif

	k_sem_take(&quit_lock, K_FOREVER);
}
