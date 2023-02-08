/*
 * Copyright (c) 2017 Intel Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief WiFi L2 stack public header
 */

#ifndef ZEPHYR_INCLUDE_NET_WIFI_MGMT_H_
#define ZEPHYR_INCLUDE_NET_WIFI_MGMT_H_

#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/wifi.h>
#include <zephyr/net/ethernet.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Management part definitions */

#define _NET_WIFI_LAYER	NET_MGMT_LAYER_L2
#define _NET_WIFI_CODE	0x156
#define _NET_WIFI_BASE	(NET_MGMT_IFACE_BIT |			\
			 NET_MGMT_LAYER(_NET_WIFI_LAYER) |	\
			 NET_MGMT_LAYER_CODE(_NET_WIFI_CODE))
#define _NET_WIFI_EVENT	(_NET_WIFI_BASE | NET_MGMT_EVENT_BIT)

enum net_request_wifi_cmd {
	NET_REQUEST_WIFI_CMD_SCAN = 1,
	NET_REQUEST_WIFI_CMD_CONNECT,
	NET_REQUEST_WIFI_CMD_DISCONNECT,
	NET_REQUEST_WIFI_CMD_AP_ENABLE,
	NET_REQUEST_WIFI_CMD_AP_DISABLE,
	NET_REQUEST_WIFI_CMD_IFACE_STATUS,
};

#define NET_REQUEST_WIFI_SCAN					\
	(_NET_WIFI_BASE | NET_REQUEST_WIFI_CMD_SCAN)

NET_MGMT_DEFINE_REQUEST_HANDLER(NET_REQUEST_WIFI_SCAN);

#define NET_REQUEST_WIFI_CONNECT				\
	(_NET_WIFI_BASE | NET_REQUEST_WIFI_CMD_CONNECT)

NET_MGMT_DEFINE_REQUEST_HANDLER(NET_REQUEST_WIFI_CONNECT);

#define NET_REQUEST_WIFI_DISCONNECT				\
	(_NET_WIFI_BASE | NET_REQUEST_WIFI_CMD_DISCONNECT)

NET_MGMT_DEFINE_REQUEST_HANDLER(NET_REQUEST_WIFI_DISCONNECT);

#define NET_REQUEST_WIFI_AP_ENABLE				\
	(_NET_WIFI_BASE | NET_REQUEST_WIFI_CMD_AP_ENABLE)

NET_MGMT_DEFINE_REQUEST_HANDLER(NET_REQUEST_WIFI_AP_ENABLE);

#define NET_REQUEST_WIFI_AP_DISABLE				\
	(_NET_WIFI_BASE | NET_REQUEST_WIFI_CMD_AP_DISABLE)

NET_MGMT_DEFINE_REQUEST_HANDLER(NET_REQUEST_WIFI_AP_DISABLE);

#define NET_REQUEST_WIFI_IFACE_STATUS				\
	(_NET_WIFI_BASE | NET_REQUEST_WIFI_CMD_IFACE_STATUS)

NET_MGMT_DEFINE_REQUEST_HANDLER(NET_REQUEST_WIFI_IFACE_STATUS);

enum net_event_wifi_cmd {
	NET_EVENT_WIFI_CMD_SCAN_RESULT = 1,
	NET_EVENT_WIFI_CMD_SCAN_DONE,
	NET_EVENT_WIFI_CMD_CONNECT_RESULT,
	NET_EVENT_WIFI_CMD_DISCONNECT_RESULT,
	NET_EVENT_WIFI_CMD_IFACE_STATUS,
};

#define NET_EVENT_WIFI_SCAN_RESULT				\
	(_NET_WIFI_EVENT | NET_EVENT_WIFI_CMD_SCAN_RESULT)

#define NET_EVENT_WIFI_SCAN_DONE				\
	(_NET_WIFI_EVENT | NET_EVENT_WIFI_CMD_SCAN_DONE)

#define NET_EVENT_WIFI_CONNECT_RESULT				\
	(_NET_WIFI_EVENT | NET_EVENT_WIFI_CMD_CONNECT_RESULT)

#define NET_EVENT_WIFI_DISCONNECT_RESULT			\
	(_NET_WIFI_EVENT | NET_EVENT_WIFI_CMD_DISCONNECT_RESULT)

#define NET_EVENT_WIFI_IFACE_STATUS						\
	(_NET_WIFI_EVENT | NET_EVENT_WIFI_CMD_IFACE_STATUS)


/* Each result is provided to the net_mgmt_event_callback
 * via its info attribute (see net_mgmt.h)
 */
struct wifi_scan_result {
	uint8_t ssid[WIFI_SSID_MAX_LEN];
	uint8_t ssid_length;

	uint8_t band;
	uint8_t channel;
	enum wifi_security_type security;
	enum wifi_mfp_options mfp;
	int8_t rssi;

	uint8_t mac[WIFI_MAC_ADDR_LEN];
	uint8_t mac_length;
};

struct wifi_connect_req_params {
	const uint8_t *ssid;
	uint8_t ssid_length; /* Max 32 */

	uint8_t *psk;
	uint8_t psk_length; /* Min 8 - Max 64 */

	uint8_t *sae_password; /* Optional with fallback to psk */
	uint8_t sae_password_length; /* No length restrictions */

	uint8_t band;
	uint8_t channel;
	enum wifi_security_type security;
	enum wifi_mfp_options mfp;
	int timeout; /* SYS_FOREVER_MS for no timeout */
};

struct wifi_status {
	int status;
};

struct wifi_iface_status {
	int state; /* enum wifi_iface_state */
	unsigned int ssid_len;
	char ssid[WIFI_SSID_MAX_LEN];
	char bssid[WIFI_MAC_ADDR_LEN];
	enum wifi_frequency_bands band;
	unsigned int channel;
	enum wifi_iface_mode iface_mode;
	enum wifi_link_mode link_mode;
	enum wifi_security_type security;
	enum wifi_mfp_options mfp;
	int rssi;
};

#include <zephyr/net/net_if.h>

typedef void (*scan_result_cb_t)(struct net_if *iface, int status,
				 struct wifi_scan_result *entry);

struct net_wifi_mgmt_offload {
	/**
	 * Mandatory to get in first position.
	 * A network device should indeed provide a pointer on such
	 * net_if_api structure. So we make current structure pointer
	 * that can be casted to a net_if_api structure pointer.
	 */
#ifdef CONFIG_WIFI_USE_NATIVE_NETWORKING
	struct ethernet_api wifi_iface;
#else
	struct net_if_api wifi_iface;
#endif

	/* cb parameter is the cb that should be called for each
	 * result by the driver. The wifi mgmt part will take care of
	 * raising the necessary event etc...
	 */
	int (*scan)(const struct device *dev, scan_result_cb_t cb);
	int (*connect)(const struct device *dev,
		       struct wifi_connect_req_params *params);
	int (*disconnect)(const struct device *dev);
	int (*ap_enable)(const struct device *dev,
			 struct wifi_connect_req_params *params);
	int (*ap_disable)(const struct device *dev);
	int (*iface_status)(const struct device *dev, struct wifi_iface_status *status);
#ifdef CONFIG_NET_STATISTICS_WIFI
	int (*get_stats)(const struct device *dev, struct net_stats_wifi *stats);
#endif /* CONFIG_NET_STATISTICS_WIFI */
};

/* Make sure that the network interface API is properly setup inside
 * Wifi mgmt offload API struct (it is the first one).
 */
BUILD_ASSERT(offsetof(struct net_wifi_mgmt_offload, wifi_iface) == 0);

void wifi_mgmt_raise_connect_result_event(struct net_if *iface, int status);
void wifi_mgmt_raise_disconnect_result_event(struct net_if *iface, int status);
void wifi_mgmt_raise_iface_status_event(struct net_if *iface,
		struct wifi_iface_status *iface_status);
#ifdef __cplusplus
}
#endif

#endif /* ZEPHYR_INCLUDE_NET_WIFI_MGMT_H_ */
