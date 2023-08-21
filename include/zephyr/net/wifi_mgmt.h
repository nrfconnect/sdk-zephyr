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
#include <zephyr/net/offloaded_netdev.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @addtogroup wifi_mgmt
 * @{
 */

/* Management part definitions */

#define _NET_WIFI_LAYER	NET_MGMT_LAYER_L2
#define _NET_WIFI_CODE	0x156
#define _NET_WIFI_BASE	(NET_MGMT_IFACE_BIT |			\
			 NET_MGMT_LAYER(_NET_WIFI_LAYER) |	\
			 NET_MGMT_LAYER_CODE(_NET_WIFI_CODE))
#define _NET_WIFI_EVENT	(_NET_WIFI_BASE | NET_MGMT_EVENT_BIT)

/** Wi-Fi management commands */
enum net_request_wifi_cmd {
	/** Scan for Wi-Fi networks */
	NET_REQUEST_WIFI_CMD_SCAN = 1,
	/** Connect to a Wi-Fi network */
	NET_REQUEST_WIFI_CMD_CONNECT,
	/** Disconnect from a Wi-Fi network */
	NET_REQUEST_WIFI_CMD_DISCONNECT,
	/** Enable AP mode */
	NET_REQUEST_WIFI_CMD_AP_ENABLE,
	/** Disable AP mode */
	NET_REQUEST_WIFI_CMD_AP_DISABLE,
	/** Get interface status */
	NET_REQUEST_WIFI_CMD_IFACE_STATUS,
	/** Set power save status */
	NET_REQUEST_WIFI_CMD_PS,
	/** Set power save mode */
	NET_REQUEST_WIFI_CMD_PS_MODE,
	/** Setup or teardown TWT flow */
	NET_REQUEST_WIFI_CMD_TWT,
	/** Get power save config */
	NET_REQUEST_WIFI_CMD_PS_CONFIG,
	/** Set or get regulatory domain */
	NET_REQUEST_WIFI_CMD_REG_DOMAIN,
	/** Set power save timeout */
	NET_REQUEST_WIFI_CMD_PS_TIMEOUT,
	NET_REQUEST_WIFI_CMD_MAX
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

#define NET_REQUEST_WIFI_PS				\
	(_NET_WIFI_BASE | NET_REQUEST_WIFI_CMD_PS)

NET_MGMT_DEFINE_REQUEST_HANDLER(NET_REQUEST_WIFI_PS);

#define NET_REQUEST_WIFI_PS_MODE			\
	(_NET_WIFI_BASE | NET_REQUEST_WIFI_CMD_PS_MODE)

NET_MGMT_DEFINE_REQUEST_HANDLER(NET_REQUEST_WIFI_PS_MODE);

#define NET_REQUEST_WIFI_TWT			\
	(_NET_WIFI_BASE | NET_REQUEST_WIFI_CMD_TWT)

NET_MGMT_DEFINE_REQUEST_HANDLER(NET_REQUEST_WIFI_TWT);

#define NET_REQUEST_WIFI_PS_CONFIG				\
	(_NET_WIFI_BASE | NET_REQUEST_WIFI_CMD_PS_CONFIG)

NET_MGMT_DEFINE_REQUEST_HANDLER(NET_REQUEST_WIFI_PS_CONFIG);
#define NET_REQUEST_WIFI_REG_DOMAIN				\
	(_NET_WIFI_BASE | NET_REQUEST_WIFI_CMD_REG_DOMAIN)

NET_MGMT_DEFINE_REQUEST_HANDLER(NET_REQUEST_WIFI_REG_DOMAIN);

#define NET_REQUEST_WIFI_PS_TIMEOUT			\
	(_NET_WIFI_BASE | NET_REQUEST_WIFI_CMD_PS_TIMEOUT)

NET_MGMT_DEFINE_REQUEST_HANDLER(NET_REQUEST_WIFI_PS_TIMEOUT);

/** Wi-Fi management events */
enum net_event_wifi_cmd {
	/** Scan results available */
	NET_EVENT_WIFI_CMD_SCAN_RESULT = 1,
	/** Scan done */
	NET_EVENT_WIFI_CMD_SCAN_DONE,
	/** Connect result */
	NET_EVENT_WIFI_CMD_CONNECT_RESULT,
	/** Disconnect result */
	NET_EVENT_WIFI_CMD_DISCONNECT_RESULT,
	/** Interface status */
	NET_EVENT_WIFI_CMD_IFACE_STATUS,
	/** TWT events */
	NET_EVENT_WIFI_CMD_TWT,
	/** TWT sleep status: awake or sleeping, can be used by application
	 * to determine if it can send data or not.
	 */
	NET_EVENT_WIFI_CMD_TWT_SLEEP_STATE,
	/** Raw scan results available */
	NET_EVENT_WIFI_CMD_RAW_SCAN_RESULT,
	/** Disconnect complete */
	NET_EVENT_WIFI_CMD_DISCONNECT_COMPLETE,
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

#define NET_EVENT_WIFI_TWT					\
	(_NET_WIFI_EVENT | NET_EVENT_WIFI_CMD_TWT)

#define NET_EVENT_WIFI_TWT_SLEEP_STATE				\
	(_NET_WIFI_EVENT | NET_EVENT_WIFI_CMD_TWT_SLEEP_STATE)

#define NET_EVENT_WIFI_RAW_SCAN_RESULT                          \
	(_NET_WIFI_EVENT | NET_EVENT_WIFI_CMD_RAW_SCAN_RESULT)

#define NET_EVENT_WIFI_DISCONNECT_COMPLETE			\
	(_NET_WIFI_EVENT | NET_EVENT_WIFI_CMD_DISCONNECT_COMPLETE)

/** Wi-Fi scan parameters */
struct wifi_scan_params {
	/** Scan type, see enum wifi_scan_type.
	 *
	 * The scan_type is only a hint to the underlying Wi-Fi chip for the
	 * preferred mode of scan. The actual mode of scan can depend on factors
	 * such as the Wi-Fi chip implementation support, regulatory domain
	 * restrictions etc.
	 */
	enum wifi_scan_type scan_type;
};

/** Wi-Fi scan result, each result is provided to the net_mgmt_event_callback
 * via its info attribute (see net_mgmt.h)
 */
struct wifi_scan_result {
	/** SSID */
	uint8_t ssid[WIFI_SSID_MAX_LEN];
	/** SSID length */
	uint8_t ssid_length;
	/** Frequency band */
	uint8_t band;
	/** Channel */
	uint8_t channel;
	/** Security type */
	enum wifi_security_type security;
	/** MFP options */
	enum wifi_mfp_options mfp;
	/** RSSI */
	int8_t rssi;
	/** BSSID */
	uint8_t mac[WIFI_MAC_ADDR_LEN];
	/** BSSID length */
	uint8_t mac_length;
};

/** Wi-Fi connect request parameters */
struct wifi_connect_req_params {
	/** SSID */
	const uint8_t *ssid;
	/** SSID length */
	uint8_t ssid_length; /* Max 32 */
	/** Pre-shared key */
	uint8_t *psk;
	/** Pre-shared key length */
	uint8_t psk_length; /* Min 8 - Max 64 */
	/** SAE password (same as PSK but with no length restrictions), optional */
	uint8_t *sae_password;
	/** SAE password length */
	uint8_t sae_password_length; /* No length restrictions */
	/** Frequency band */
	uint8_t band;
	/** Channel */
	uint8_t channel;
	/** Security type */
	enum wifi_security_type security;
	/** MFP options */
	enum wifi_mfp_options mfp;
	/** Connect timeout in seconds, SYS_FOREVER_MS for no timeout */
	int timeout;
};

/** Generic Wi-Fi status for commands and events */
struct wifi_status {
	int status;
};

/** Wi-Fi interface status */
struct wifi_iface_status {
	/** Interface state, see enum wifi_iface_state */
	int state;
	/** SSID length */
	unsigned int ssid_len;
	/** SSID */
	char ssid[WIFI_SSID_MAX_LEN];
	/** BSSID */
	char bssid[WIFI_MAC_ADDR_LEN];
	/** Frequency band */
	enum wifi_frequency_bands band;
	/** Channel */
	unsigned int channel;
	/** Interface mode, see enum wifi_iface_mode */
	enum wifi_iface_mode iface_mode;
	/** Link mode, see enum wifi_link_mode */
	enum wifi_link_mode link_mode;
	/** Security type, see enum wifi_security_type */
	enum wifi_security_type security;
	/** MFP options, see enum wifi_mfp_options */
	enum wifi_mfp_options mfp;
	/** RSSI */
	int rssi;
	/** DTIM period */
	unsigned char dtim_period;
	/** Beacon interval */
	unsigned short beacon_interval;
	/** is TWT capable? */
	bool twt_capable;
};

/** Wi-Fi power save parameters */
struct wifi_ps_params {
	/* Power save state */
	enum wifi_ps enabled;
	/* Listen interval */
	unsigned short listen_interval;
	/** Wi-Fi power save wakeup mode */
	enum wifi_ps_wakeup_mode wakeup_mode;
	/** Wi-Fi power save mode */
	enum wifi_ps_mode mode;
	/** Wi-Fi power save timeout
	 *
	 * This is the time out to wait after sending a TX packet
	 * before going back to power save (in ms) to receive any replies
	 * from the AP. Zero means this feature is disabled.
	 *
	 * It's a tradeoff between power consumption and latency.
	 */
	unsigned int timeout_ms;
	/** Wi-Fi power save type */
	enum ps_param_type type;
	/** Wi-Fi power save fail reason */
	enum wifi_config_ps_param_fail_reason fail_reason;
};

/** Wi-Fi TWT parameters */
struct wifi_twt_params {
	/** TWT operation, see enum wifi_twt_operation */
	enum wifi_twt_operation operation;
	/** TWT negotiation type, see enum wifi_twt_negotiation_type */
	enum wifi_twt_negotiation_type negotiation_type;
	/** TWT setup command, see enum wifi_twt_setup_cmd */
	enum wifi_twt_setup_cmd setup_cmd;
	/** TWT setup response status, see enum wifi_twt_setup_resp_status */
	enum wifi_twt_setup_resp_status resp_status;
	/** Dialog token, used to map requests to responses */
	uint8_t dialog_token;
	/** Flow ID, used to map setup with teardown */
	uint8_t flow_id;
	union {
		/** Setup specific parameters */
		struct {
			/**Interval = Wake up time + Sleeping time */
			uint64_t twt_interval;
			/** Requestor or responder */
			bool responder;
			/** Trigger enabled or disabled */
			bool trigger;
			/** Implicit or explicit */
			bool implicit;
			/** Announced or unannounced */
			bool announce;
			/** Wake up time */
			uint32_t twt_wake_interval;
		} setup;
		/** Teardown specific parameters */
		struct {
			/** Teardown all flows */
			bool teardown_all;
		} teardown;
	};
	/** TWT fail reason, see enum wifi_twt_fail_reason */
	enum wifi_twt_fail_reason fail_reason;
};

/* Flow ID is only 3 bits */
#define WIFI_MAX_TWT_FLOWS 8
#define WIFI_MAX_TWT_INTERVAL_US (LONG_MAX - 1)
/* 256 (u8) * 1TU */
#define WIFI_MAX_TWT_WAKE_INTERVAL_US 262144

/** Wi-Fi TWT flow information */
struct wifi_twt_flow_info {
	/** Interval = Wake up time + Sleeping time */
	uint64_t  twt_interval;
	/** Dialog token, used to map requests to responses */
	uint8_t dialog_token;
	/** Flow ID, used to map setup with teardown */
	uint8_t flow_id;
	/** TWT negotiation type, see enum wifi_twt_negotiation_type */
	enum wifi_twt_negotiation_type negotiation_type;
	/** Requestor or responder */
	bool responder;
	/** Trigger enabled or disabled */
	bool trigger;
	/** Implicit or explicit */
	bool implicit;
	/** Announced or unannounced */
	bool announce;
	/** Wake up time */
	uint32_t twt_wake_interval;
};

/** Wi-Fi power save configuration */
struct wifi_ps_config {
	/** Number of TWT flows */
	char num_twt_flows;
	/** TWT flow details */
	struct wifi_twt_flow_info twt_flows[WIFI_MAX_TWT_FLOWS];
	/** Power save configuration */
	struct wifi_ps_params ps_params;
};

/** Generic get/set operation for any command*/
enum wifi_mgmt_op {
	/** Get operation */
	WIFI_MGMT_GET = 0,
	/** Set operation */
	WIFI_MGMT_SET = 1,
};

/** Regulatory domain information or configuration */
struct wifi_reg_domain {
	/* Regulatory domain operation */
	enum wifi_mgmt_op oper;
	/** Ignore all other regulatory hints over this one */
	bool force;
	/** Country code: ISO/IEC 3166-1 alpha-2 */
	uint8_t country_code[WIFI_COUNTRY_CODE_LEN];
};

/** Wi-Fi TWT sleep states */
enum wifi_twt_sleep_state {
	/** TWT sleep state: sleeping */
	WIFI_TWT_STATE_SLEEP = 0,
	/** TWT sleep state: awake */
	WIFI_TWT_STATE_AWAKE = 1,
};

#if defined(CONFIG_WIFI_MGMT_RAW_SCAN_RESULTS) || defined(__DOXYGEN__)
/** Wi-Fi raw scan result */
struct wifi_raw_scan_result {
	/** RSSI */
	int8_t rssi;
	/** Frame length */
	int frame_length;
	/** Frequency */
	unsigned short frequency;
	/** Raw scan data */
	uint8_t data[CONFIG_WIFI_MGMT_RAW_SCAN_RESULT_LENGTH];
};
#endif /* CONFIG_WIFI_MGMT_RAW_SCAN_RESULTS */
#include <zephyr/net/net_if.h>

/** Scan result callback
 *
 * @param iface Network interface
 * @param status Scan result status
 * @param entry Scan result entry
 */
typedef void (*scan_result_cb_t)(struct net_if *iface, int status,
				 struct wifi_scan_result *entry);

#ifdef CONFIG_WIFI_MGMT_RAW_SCAN_RESULTS
/** Raw scan result callback
 *
 * @param iface Network interface
 * @param status Raw scan result status
 * @param entry Raw scan result entry
 */
typedef void (*raw_scan_result_cb_t)(struct net_if *iface, int status,
				     struct wifi_raw_scan_result *entry);
#endif /* CONFIG_WIFI_MGMT_RAW_SCAN_RESULTS */

/** Wi-Fi management API */
struct wifi_mgmt_ops {
	/** Scan for Wi-Fi networks
	 *
	 * @param dev Pointer to the device structure for the driver instance.
	 * @param params Scan parameters
	 * @param cb Callback to be called for each result
	 *           cb parameter is the cb that should be called for each
	 *           result by the driver. The wifi mgmt part will take care of
	 *           raising the necessary event etc.
	 *
	 * @return 0 if ok, < 0 if error
	 */
	int (*scan)(const struct device *dev,
		    struct wifi_scan_params *params,
		    scan_result_cb_t cb);
	/** Connect to a Wi-Fi network
	 *
	 * @param dev Pointer to the device structure for the driver instance.
	 * @param params Connect parameters
	 *
	 * @return 0 if ok, < 0 if error
	 */
	int (*connect)(const struct device *dev,
		       struct wifi_connect_req_params *params);
	/** Disconnect from a Wi-Fi network
	 *
	 * @param dev Pointer to the device structure for the driver instance.
	 *
	 * @return 0 if ok, < 0 if error
	 */
	int (*disconnect)(const struct device *dev);
	/** Enable AP mode
	 *
	 * @param dev Pointer to the device structure for the driver instance.
	 * @param params AP mode parameters
	 *
	 * @return 0 if ok, < 0 if error
	 */
	int (*ap_enable)(const struct device *dev,
			 struct wifi_connect_req_params *params);
	/** Disable AP mode
	 *
	 * @param dev Pointer to the device structure for the driver instance.
	 *
	 * @return 0 if ok, < 0 if error
	 */
	int (*ap_disable)(const struct device *dev);
	/** Get interface status
	 *
	 * @param dev Pointer to the device structure for the driver instance.
	 * @param status Interface status
	 *
	 * @return 0 if ok, < 0 if error
	 */
	int (*iface_status)(const struct device *dev, struct wifi_iface_status *status);
#if defined(CONFIG_NET_STATISTICS_WIFI) || defined(__DOXYGEN__)
	/** Get Wi-Fi statistics
	 *
	 * @param dev Pointer to the device structure for the driver instance.
	 * @param stats Wi-Fi statistics
	 *
	 * @return 0 if ok, < 0 if error
	 */
	int (*get_stats)(const struct device *dev, struct net_stats_wifi *stats);
#endif /* CONFIG_NET_STATISTICS_WIFI */
	/** Set power save status
	 *
	 * @param dev Pointer to the device structure for the driver instance.
	 * @param params Power save parameters
	 *
	 * @return 0 if ok, < 0 if error
	 */
	int (*set_power_save)(const struct device *dev, struct wifi_ps_params *params);
	/** Setup or teardown TWT flow
	 *
	 * @param dev Pointer to the device structure for the driver instance.
	 * @param params TWT parameters
	 *
	 * @return 0 if ok, < 0 if error
	 */
	int (*set_twt)(const struct device *dev, struct wifi_twt_params *params);
	/** Get power save config
	 *
	 * @param dev Pointer to the device structure for the driver instance.
	 * @param config Power save config
	 *
	 * @return 0 if ok, < 0 if error
	 */
	int (*get_power_save_config)(const struct device *dev, struct wifi_ps_config *config);
	/** Set or get regulatory domain
	 *
	 * @param dev Pointer to the device structure for the driver instance.
	 * @param reg_domain Regulatory domain
	 *
	 * @return 0 if ok, < 0 if error
	 */
	int (*reg_domain)(const struct device *dev, struct wifi_reg_domain *reg_domain);
};

/** Wi-Fi management offload API */
struct net_wifi_mgmt_offload {
	/**
	 * Mandatory to get in first position.
	 * A network device should indeed provide a pointer on such
	 * net_if_api structure. So we make current structure pointer
	 * that can be casted to a net_if_api structure pointer.
	 */
#if defined(CONFIG_WIFI_USE_NATIVE_NETWORKING) || defined(__DOXYGEN__)
	/** Ethernet API */
	struct ethernet_api wifi_iface;
#else
	/** Offloaded network device API */
	struct offloaded_if_api wifi_iface;
#endif
	/** Wi-Fi management API */
	const struct wifi_mgmt_ops *const wifi_mgmt_api;
};

/* Make sure that the network interface API is properly setup inside
 * Wifi mgmt offload API struct (it is the first one).
 */
BUILD_ASSERT(offsetof(struct net_wifi_mgmt_offload, wifi_iface) == 0);

/** Wi-Fi management connect result event
 *
 * @param iface Network interface
 * @param status Connect result status
 */
void wifi_mgmt_raise_connect_result_event(struct net_if *iface, int status);

/** Wi-Fi management disconnect result event
 *
 * @param iface Network interface
 * @param status Disconnect result status
 */
void wifi_mgmt_raise_disconnect_result_event(struct net_if *iface, int status);

/** Wi-Fi management interface status event
 *
 * @param iface Network interface
 * @param iface_status Interface status
 */
void wifi_mgmt_raise_iface_status_event(struct net_if *iface,
		struct wifi_iface_status *iface_status);

/** Wi-Fi management TWT event
 *
 * @param iface Network interface
 * @param twt_params TWT parameters
 */
void wifi_mgmt_raise_twt_event(struct net_if *iface,
		struct wifi_twt_params *twt_params);

/** Wi-Fi management TWT sleep state event
 *
 * @param iface Network interface
 * @param twt_sleep_state TWT sleep state
 */
void wifi_mgmt_raise_twt_sleep_state(struct net_if *iface, int twt_sleep_state);

#if defined(CONFIG_WIFI_MGMT_RAW_SCAN_RESULTS) || defined(__DOXYGEN__)
/** Wi-Fi management raw scan result event
 *
 * @param iface Network interface
 * @param raw_scan_info Raw scan result
 */
void wifi_mgmt_raise_raw_scan_result_event(struct net_if *iface,
		struct wifi_raw_scan_result *raw_scan_info);
#endif /* CONFIG_WIFI_MGMT_RAW_SCAN_RESULTS */

/** Wi-Fi management disconnect complete event
 *
 * @param iface Network interface
 * @param status Disconnect complete status
 */
void wifi_mgmt_raise_disconnect_complete_event(struct net_if *iface, int status);

/**
 * @}
 */
#ifdef __cplusplus
}
#endif

#endif /* ZEPHYR_INCLUDE_NET_WIFI_MGMT_H_ */
