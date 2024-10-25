/**
 * Copyright (c) 2023 Nordic Semiconductor ASA
 * Copyright 2024 NXP
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdarg.h>

#include <zephyr/logging/log.h>
#include <zephyr/kernel.h>
#include <zephyr/net/wifi_mgmt.h>

#include "includes.h"
#include "common.h"
#include "common/defs.h"
#include "wpa_supplicant/config.h"
#include "wpa_supplicant_i.h"
#include "driver_i.h"

#include "supp_main.h"
#include "supp_api.h"
#include "wpa_cli_zephyr.h"
#ifdef CONFIG_WIFI_NM_HOSTAPD_AP
#include "hostapd.h"
#include "hostapd_cli_zephyr.h"
#include "ap_drv_ops.h"
#endif
#include "supp_events.h"

extern struct k_sem wpa_supplicant_ready_sem;
extern struct wpa_global *global;

/* save the last wifi connection parameters */
static struct wifi_connect_req_params last_wifi_conn_params;

enum requested_ops {
	CONNECT = 0,
	DISCONNECT
};

enum status_thread_state {
	STATUS_THREAD_STOPPED = 0,
	STATUS_THREAD_RUNNING,
};

#define OP_STATUS_POLLING_INTERVAL 1

#define CONNECTION_SUCCESS 0
#define CONNECTION_FAILURE 1
#define CONNECTION_TERMINATED 2

#define DISCONNECT_TIMEOUT_MS 5000

#ifdef CONFIG_WIFI_NM_WPA_SUPPLICANT_CRYPTO_ENTERPRISE
static struct wifi_enterprise_creds_params enterprise_creds;
#endif

K_MUTEX_DEFINE(wpa_supplicant_mutex);

extern struct k_work_q *get_workq(void);

struct wpa_supp_api_ctrl {
	const struct device *dev;
	enum requested_ops requested_op;
	enum status_thread_state status_thread_state;
	int connection_timeout; /* in seconds */
	struct k_work_sync sync;
	bool terminate;
};

static struct wpa_supp_api_ctrl wpas_api_ctrl;

static void supp_shell_connect_status(struct k_work *work);

static K_WORK_DELAYABLE_DEFINE(wpa_supp_status_work,
		supp_shell_connect_status);

#define wpa_cli_cmd_v(cmd, ...)	({					\
	bool status;							\
									\
	if (zephyr_wpa_cli_cmd_v(cmd, ##__VA_ARGS__) < 0) {		\
		wpa_printf(MSG_ERROR,					\
			   "Failed to execute wpa_cli command: %s",	\
			   cmd);					\
		status = false;						\
	} else {							\
		status = true;						\
	}								\
									\
	status;								\
})

static struct wpa_supplicant *get_wpa_s_handle(const struct device *dev)
{
	struct net_if *iface = net_if_lookup_by_dev(dev);
	char if_name[CONFIG_NET_INTERFACE_NAME_LEN + 1];
	struct wpa_supplicant *wpa_s;
	int ret;

	if (!iface) {
		wpa_printf(MSG_ERROR, "Interface for device %s not found", dev->name);
		return NULL;
	}

	ret = net_if_get_name(iface, if_name, sizeof(if_name));
	if (!ret) {
		wpa_printf(MSG_ERROR, "Cannot get interface name (%d)", ret);
		return NULL;
	}

	wpa_s = zephyr_get_handle_by_ifname(if_name);
	if (!wpa_s) {
		wpa_printf(MSG_ERROR, "Interface %s not found", if_name);
		return NULL;
	}

	return wpa_s;
}

#ifdef CONFIG_WIFI_NM_HOSTAPD_AP
#define hostapd_cli_cmd_v(cmd, ...) ({					\
	bool status;							\
									\
	if (zephyr_hostapd_cli_cmd_v(cmd, ##__VA_ARGS__) < 0) {		\
		wpa_printf(MSG_ERROR,					\
			   "Failed to execute wpa_cli command: %s",	\
			   cmd);					\
		status = false;						\
	} else {							\
		status = true;						\
	}								\
									\
	status;								\
})

static inline struct hostapd_iface *get_hostapd_handle(const struct device *dev)
{
	struct net_if *iface = net_if_lookup_by_dev(dev);
	char if_name[CONFIG_NET_INTERFACE_NAME_LEN + 1];
	struct hostapd_iface *hapd;
	int ret;

	if (!iface) {
		wpa_printf(MSG_ERROR, "Interface for device %s not found", dev->name);
		return NULL;
	}

	ret = net_if_get_name(iface, if_name, sizeof(if_name));
	if (!ret) {
		wpa_printf(MSG_ERROR, "Cannot get interface name (%d)", ret);
		return NULL;
	}

	hapd = zephyr_get_hapd_handle_by_ifname(if_name);
	if (!hapd) {
		wpa_printf(MSG_ERROR, "Interface %s not found", if_name);
		return NULL;
	}

	return hapd;
}
#endif

#define WPA_SUPP_STATE_POLLING_MS 10
static int wait_for_disconnect_complete(const struct device *dev)
{
	int ret = 0;
	int attempts = 0;
	struct wpa_supplicant *wpa_s = get_wpa_s_handle(dev);
	unsigned int max_attempts = DISCONNECT_TIMEOUT_MS / WPA_SUPP_STATE_POLLING_MS;

	if (!wpa_s) {
		ret = -ENODEV;
		wpa_printf(MSG_ERROR, "Failed to get wpa_s handle");
		goto out;
	}

	while (wpa_s->wpa_state != WPA_DISCONNECTED) {
		if (attempts++ > max_attempts) {
			ret = -ETIMEDOUT;
			wpa_printf(MSG_WARNING, "Failed to disconnect from network");
			break;
		}

		k_sleep(K_MSEC(WPA_SUPP_STATE_POLLING_MS));
	}
out:
	return ret;
}

static void supp_shell_connect_status(struct k_work *work)
{
	static int seconds_counter;
	int status = CONNECTION_SUCCESS;
	int conn_result = CONNECTION_FAILURE;
	struct wpa_supplicant *wpa_s;
	struct wpa_supp_api_ctrl *ctrl = &wpas_api_ctrl;

	k_mutex_lock(&wpa_supplicant_mutex, K_FOREVER);

	if (ctrl->status_thread_state == STATUS_THREAD_RUNNING &&  ctrl->terminate) {
		status = CONNECTION_TERMINATED;
		goto out;
	}

	wpa_s = get_wpa_s_handle(ctrl->dev);
	if (!wpa_s) {
		status = CONNECTION_FAILURE;
		goto out;
	}

	if (ctrl->requested_op == CONNECT && wpa_s->wpa_state != WPA_COMPLETED) {
		if (ctrl->connection_timeout > 0 &&
		    seconds_counter++ > ctrl->connection_timeout) {
			if (!wpa_cli_cmd_v("disconnect")) {
				goto out;
			}

			conn_result = -ETIMEDOUT;
			supplicant_send_wifi_mgmt_event(wpa_s->ifname,
							NET_EVENT_WIFI_CMD_CONNECT_RESULT,
							(void *)&conn_result, sizeof(int));
			status = CONNECTION_FAILURE;
			goto out;
		}

		k_work_reschedule_for_queue(get_workq(), &wpa_supp_status_work,
					    K_SECONDS(OP_STATUS_POLLING_INTERVAL));
		ctrl->status_thread_state = STATUS_THREAD_RUNNING;
		k_mutex_unlock(&wpa_supplicant_mutex);
		return;
	}
out:
	seconds_counter = 0;

	ctrl->status_thread_state = STATUS_THREAD_STOPPED;
	k_mutex_unlock(&wpa_supplicant_mutex);
}

static struct hostapd_hw_modes *get_mode_by_band(struct wpa_supplicant *wpa_s, uint8_t band)
{
	enum hostapd_hw_mode hw_mode;
	bool is_6ghz = (band == WIFI_FREQ_BAND_6_GHZ) ? true : false;

	if (band == WIFI_FREQ_BAND_2_4_GHZ) {
		hw_mode = HOSTAPD_MODE_IEEE80211G;
	} else if ((band == WIFI_FREQ_BAND_5_GHZ) ||
		   (band == WIFI_FREQ_BAND_6_GHZ)) {
		hw_mode = HOSTAPD_MODE_IEEE80211A;
	} else {
		return NULL;
	}

	return get_mode(wpa_s->hw.modes, wpa_s->hw.num_modes, hw_mode, is_6ghz);
}

static int wpa_supp_supported_channels(struct wpa_supplicant *wpa_s, uint8_t band, char **chan_list)
{
	struct hostapd_hw_modes *mode = NULL;
	int i;
	int offset, retval;
	int size;
	char *_chan_list;

	mode = get_mode_by_band(wpa_s, band);
	if (!mode) {
		wpa_printf(MSG_ERROR, "Unsupported or invalid band: %d", band);
		return -EINVAL;
	}

	size = ((mode->num_channels) * CHAN_NUM_LEN) + 1;
	_chan_list = os_malloc(size);
	if (!_chan_list) {
		wpa_printf(MSG_ERROR, "Mem alloc failed for channel list");
		return -ENOMEM;
	}

	retval = 0;
	offset = 0;
	for (i = 0; i < mode->num_channels; i++) {
		retval = snprintf(_chan_list + offset, CHAN_NUM_LEN, " %d",
				  mode->channels[i].freq);
		offset += retval;
	}
	*chan_list = _chan_list;

	return 0;
}

static int wpa_supp_band_chan_compat(struct wpa_supplicant *wpa_s, uint8_t band, uint8_t channel)
{
	struct hostapd_hw_modes *mode = NULL;
	int i;

	mode = get_mode_by_band(wpa_s, band);
	if (!mode) {
		wpa_printf(MSG_ERROR, "Unsupported or invalid band: %d", band);
		return -EINVAL;
	}

	for (i = 0; i < mode->num_channels; i++) {
		if (mode->channels[i].chan == channel) {
			return mode->channels[i].freq;
		}
	}

	wpa_printf(MSG_ERROR, "Channel %d not supported for band %d", channel, band);

	return -EINVAL;
}

static inline void wpa_supp_restart_status_work(void)
{
	/* Terminate synchronously */
	wpas_api_ctrl.terminate = 1;
	k_work_flush_delayable(&wpa_supp_status_work, &wpas_api_ctrl.sync);
	wpas_api_ctrl.terminate = 0;

	/* Start afresh */
	k_work_reschedule_for_queue(get_workq(), &wpa_supp_status_work, K_MSEC(10));
}

static inline int chan_to_freq(int chan)
{
	/* We use global channel list here and also use the widest
	 * op_class for 5GHz channels as there is no user input
	 * for these (yet).
	 */
	int freq = -1;
	int op_classes[] = {81, 82, 128};
	int op_classes_size = ARRAY_SIZE(op_classes);

	for (int i = 0; i < op_classes_size; i++) {
		freq = ieee80211_chan_to_freq(NULL, op_classes[i], chan);
		if (freq > 0) {
			break;
		}
	}

	if (freq <= 0) {
		wpa_printf(MSG_ERROR, "Invalid channel %d", chan);
		return -1;
	}

	return freq;
}

static inline enum wifi_frequency_bands wpas_band_to_zephyr(enum wpa_radio_work_band band)
{
	switch (band) {
	case BAND_2_4_GHZ:
		return WIFI_FREQ_BAND_2_4_GHZ;
	case BAND_5_GHZ:
		return WIFI_FREQ_BAND_5_GHZ;
	default:
		return WIFI_FREQ_BAND_UNKNOWN;
	}
}

static inline enum wifi_security_type wpas_key_mgmt_to_zephyr(int key_mgmt, int proto)
{
	switch (key_mgmt) {
	case WPA_KEY_MGMT_IEEE8021X:
	case WPA_KEY_MGMT_IEEE8021X_SUITE_B:
	case WPA_KEY_MGMT_IEEE8021X_SUITE_B_192:
		return WIFI_SECURITY_TYPE_EAP_TLS;
	case WPA_KEY_MGMT_NONE:
		return WIFI_SECURITY_TYPE_NONE;
	case WPA_KEY_MGMT_PSK:
		if (proto == WPA_PROTO_RSN) {
			return WIFI_SECURITY_TYPE_PSK;
		} else {
			return WIFI_SECURITY_TYPE_WPA_PSK;
		}
	case WPA_KEY_MGMT_PSK_SHA256:
		return WIFI_SECURITY_TYPE_PSK_SHA256;
	case WPA_KEY_MGMT_SAE:
		return WIFI_SECURITY_TYPE_SAE;
	default:
		return WIFI_SECURITY_TYPE_UNKNOWN;
	}
}

#ifdef CONFIG_WIFI_NM_WPA_SUPPLICANT_CRYPTO_ENTERPRISE
int supplicant_add_enterprise_creds(const struct device *dev,
			struct wifi_enterprise_creds_params *creds)
{
	int ret = 0;

	if (!creds) {
		ret = -1;
		wpa_printf(MSG_ERROR, "enterprise creds is NULL");
		goto out;
	}

	memcpy((void *)&enterprise_creds, (void *)creds,
			sizeof(struct wifi_enterprise_creds_params));

out:
	return ret;
}

static int wpas_config_process_blob(struct wpa_config *config, char *name, uint8_t *data,
				uint32_t data_len)
{
	struct wpa_config_blob *blob;

	if (!data || !data_len) {
		return -1;
	}

	blob = os_zalloc(sizeof(*blob));
	if (blob == NULL) {
		return -1;
	}

	blob->data = os_zalloc(data_len);
	if (blob->data == NULL) {
		os_free(blob);
		return -1;
	}

	blob->name = os_strdup(name);

	if (blob->name == NULL) {
		wpa_config_free_blob(blob);
		return -1;
	}

	os_memcpy(blob->data, data, data_len);
	blob->len = data_len;

	wpa_config_set_blob(config, blob);

	return 0;
}
#endif

static int wpas_add_and_config_network(struct wpa_supplicant *wpa_s,
				       struct wifi_connect_req_params *params,
				       bool mode_ap)
{
	struct add_network_resp resp = {0};
	char *chan_list = NULL;
	struct net_eth_addr mac = {0};
	int ret = 0;
	uint8_t ssid_null_terminated[WIFI_SSID_MAX_LEN + 1];
	uint8_t psk_null_terminated[WIFI_PSK_MAX_LEN + 1];
	uint8_t sae_null_terminated[WIFI_SAE_PSWD_MAX_LEN + 1];

	if (!wpa_cli_cmd_v("remove_network all")) {
		goto out;
	}

	ret = z_wpa_ctrl_add_network(&resp);
	if (ret) {
		wpa_printf(MSG_ERROR, "Failed to add network");
		goto out;
	}

	wpa_printf(MSG_DEBUG, "NET added: %d", resp.network_id);

	if (mode_ap) {
		if (!wpa_cli_cmd_v("set_network %d mode 2", resp.network_id)) {
			goto out;
		}
	}

	if (params->ssid_length > WIFI_SSID_MAX_LEN) {
		wpa_printf(MSG_ERROR, "SSID too long (max %d characters)", WIFI_SSID_MAX_LEN);
		goto out;
	}

	strncpy(ssid_null_terminated, params->ssid, WIFI_SSID_MAX_LEN);
	ssid_null_terminated[params->ssid_length] = '\0';

	if (!wpa_cli_cmd_v("set_network %d ssid \"%s\"",
			   resp.network_id, ssid_null_terminated)) {
		goto out;
	}

	if (!wpa_cli_cmd_v("set_network %d scan_ssid 1", resp.network_id)) {
		goto out;
	}

	if (!wpa_cli_cmd_v("set_network %d key_mgmt NONE", resp.network_id)) {
		goto out;
	}

	if (!wpa_cli_cmd_v("set_network %d ieee80211w 0", resp.network_id)) {
		goto out;
	}

	if (params->band != WIFI_FREQ_BAND_UNKNOWN) {
		ret = wpa_supp_supported_channels(wpa_s, params->band, &chan_list);
		if (ret < 0) {
			goto rem_net;
		}

		if (chan_list) {
			if (!wpa_cli_cmd_v("set_network %d scan_freq%s", resp.network_id,
					   chan_list)) {
				os_free(chan_list);
				goto out;
			}

			os_free(chan_list);
		}
	}

	if (params->security != WIFI_SECURITY_TYPE_NONE) {
		if (params->sae_password) {
			if ((params->sae_password_length < WIFI_PSK_MIN_LEN) ||
			    (params->sae_password_length > WIFI_SAE_PSWD_MAX_LEN)) {
				wpa_printf(MSG_ERROR,
					   "Passphrase should be in range (%d-%d) characters",
					   WIFI_PSK_MIN_LEN, WIFI_SAE_PSWD_MAX_LEN);
				goto out;
			}
			strncpy(sae_null_terminated, params->sae_password, WIFI_SAE_PSWD_MAX_LEN);
			sae_null_terminated[params->sae_password_length] = '\0';
		} else {
			if ((params->psk_length < WIFI_PSK_MIN_LEN) ||
			    (params->psk_length > WIFI_PSK_MAX_LEN)) {
				wpa_printf(MSG_ERROR,
					   "Passphrase should be in range (%d-%d) characters",
					   WIFI_PSK_MIN_LEN, WIFI_PSK_MAX_LEN);
				goto out;
			}
			strncpy(psk_null_terminated, params->psk, WIFI_PSK_MAX_LEN);
			psk_null_terminated[params->psk_length] = '\0';
		}

		/* SAP - only open and WPA2-PSK are supported for now */
		if (mode_ap && params->security != WIFI_SECURITY_TYPE_PSK) {
			ret = -1;
			wpa_printf(MSG_ERROR, "Unsupported security type: %d",
				params->security);
			goto rem_net;
		}

		/* Except for WPA-PSK, rest all are under WPA2 */
		if (params->security != WIFI_SECURITY_TYPE_WPA_PSK) {
			if (!wpa_cli_cmd_v("set_network %d proto RSN",
					   resp.network_id)) {
				goto out;
			}
		}

		if (params->security == WIFI_SECURITY_TYPE_SAE_HNP ||
		    params->security == WIFI_SECURITY_TYPE_SAE_H2E ||
		    params->security == WIFI_SECURITY_TYPE_SAE_AUTO) {
			if (params->sae_password) {
				if (!wpa_cli_cmd_v("set_network %d sae_password \"%s\"",
						   resp.network_id, sae_null_terminated)) {
					goto out;
				}
			} else {
				if (!wpa_cli_cmd_v("set_network %d sae_password \"%s\"",
						   resp.network_id, psk_null_terminated)) {
					goto out;
				}
			}

			if (params->security == WIFI_SECURITY_TYPE_SAE_H2E ||
			    params->security == WIFI_SECURITY_TYPE_SAE_AUTO) {
				if (!wpa_cli_cmd_v("set sae_pwe %d",
						   (params->security == WIFI_SECURITY_TYPE_SAE_H2E)
							   ? 1
							   : 2)) {
					goto out;
				}
			}

			if (!wpa_cli_cmd_v("set_network %d key_mgmt SAE", resp.network_id)) {
				goto out;
			}
		} else if (params->security == WIFI_SECURITY_TYPE_PSK_SHA256) {
			if (!wpa_cli_cmd_v("set_network %d psk \"%s\"",
					   resp.network_id, psk_null_terminated)) {
				goto out;
			}

			if (!wpa_cli_cmd_v("set_network %d key_mgmt WPA-PSK-SHA256",
					   resp.network_id)) {
				goto out;
			}
		} else if (params->security == WIFI_SECURITY_TYPE_PSK ||
			   params->security == WIFI_SECURITY_TYPE_WPA_PSK) {
			if (!wpa_cli_cmd_v("set_network %d psk \"%s\"",
					   resp.network_id, psk_null_terminated)) {
				goto out;
			}

			if (!wpa_cli_cmd_v("set_network %d key_mgmt WPA-PSK",
					   resp.network_id)) {
				goto out;
			}

			if (params->security == WIFI_SECURITY_TYPE_WPA_PSK) {
				if (!wpa_cli_cmd_v("set_network %d proto WPA",
						   resp.network_id)) {
					goto out;
				}
			}
#ifdef CONFIG_WIFI_NM_WPA_SUPPLICANT_CRYPTO_ENTERPRISE
		} else if (params->security == WIFI_SECURITY_TYPE_EAP_TLS) {
			if (!wpa_cli_cmd_v("set_network %d key_mgmt WPA-EAP",
					   resp.network_id)) {
				goto out;
			}

			if (!wpa_cli_cmd_v("set_network %d proto RSN",
					   resp.network_id)) {
				goto out;
			}

			if (!wpa_cli_cmd_v("set_network %d eap TLS",
					   resp.network_id)) {
				goto out;
			}

			if (!wpa_cli_cmd_v("set_network %d anonymous_identity \"%s\"",
					   resp.network_id, params->anon_id)) {
				goto out;
			}

			if (wpas_config_process_blob(wpa_s->conf, "ca_cert",
					   enterprise_creds.ca_cert,
					   enterprise_creds.ca_cert_len)) {
				goto out;
			}

			if (!wpa_cli_cmd_v("set_network %d ca_cert \"blob://ca_cert\"",
					   resp.network_id)) {
				goto out;
			}

			if (wpas_config_process_blob(wpa_s->conf, "client_cert",
					   enterprise_creds.client_cert,
					   enterprise_creds.client_cert_len)) {
				goto out;
			}

			if (!wpa_cli_cmd_v("set_network %d client_cert \"blob://client_cert\"",
					   resp.network_id)) {
				goto out;
			}

			if (wpas_config_process_blob(wpa_s->conf, "private_key",
					   enterprise_creds.client_key,
					   enterprise_creds.client_key_len)) {
				goto out;
			}

			if (!wpa_cli_cmd_v("set_network %d private_key \"blob://private_key\"",
					   resp.network_id)) {
				goto out;
			}

			if (!wpa_cli_cmd_v("set_network %d private_key_passwd \"%s\"",
					   resp.network_id, params->key_passwd)) {
				goto out;
			}
#endif
		} else {
			ret = -1;
			wpa_printf(MSG_ERROR, "Unsupported security type: %d",
				params->security);
			goto rem_net;
		}

		if (params->mfp) {
			if (!wpa_cli_cmd_v("set_network %d ieee80211w %d",
					   resp.network_id, params->mfp)) {
				goto out;
			}
		}
	}

	if (params->channel != WIFI_CHANNEL_ANY) {
		int freq;

		if (params->band != WIFI_FREQ_BAND_UNKNOWN) {
			freq = wpa_supp_band_chan_compat(wpa_s, params->band, params->channel);
			if (freq < 0) {
				goto rem_net;
			}
		} else {
			freq = chan_to_freq(params->channel);
			if (freq < 0) {
				ret = -1;
				wpa_printf(MSG_ERROR, "Invalid channel %d",
					params->channel);
				goto rem_net;
			}
		}

		if (mode_ap) {
			if (!wpa_cli_cmd_v("set_network %d frequency %d",
					   resp.network_id, freq)) {
				goto out;
			}
		} else {
			if (!wpa_cli_cmd_v("set_network %d scan_freq %d",
					   resp.network_id, freq)) {
				goto out;
			}
		}
	}

	memcpy((void *)&mac, params->bssid, WIFI_MAC_ADDR_LEN);
	if (net_eth_is_addr_broadcast(&mac) ||
	    net_eth_is_addr_multicast(&mac)) {
		wpa_printf(MSG_ERROR, "Invalid BSSID. Configuration "
			   "of multicast or broadcast MAC is not allowed.");
		ret =  -EINVAL;
		goto rem_net;
	}

	if (!net_eth_is_addr_unspecified(&mac)) {
		char bssid_str[MAC_STR_LEN] = {0};

		snprintf(bssid_str, MAC_STR_LEN, "%02x:%02x:%02x:%02x:%02x:%02x",
			params->bssid[0], params->bssid[1], params->bssid[2],
			params->bssid[3], params->bssid[4], params->bssid[5]);

		if (!wpa_cli_cmd_v("set_network %d bssid %s",
				   resp.network_id, bssid_str)) {
			goto out;
		}
	}

	/* enable and select network */
	if (!wpa_cli_cmd_v("enable_network %d", resp.network_id)) {
		goto out;
	}

	if (!wpa_cli_cmd_v("select_network %d", resp.network_id)) {
		goto out;
	}

	memset(&last_wifi_conn_params, 0, sizeof(struct wifi_connect_req_params));
	memcpy((void *)&last_wifi_conn_params, params, sizeof(struct wifi_connect_req_params));
	return 0;

rem_net:
	if (!wpa_cli_cmd_v("remove_network %d", resp.network_id)) {
		goto out;
	}

out:
	return ret;
}

static int wpas_disconnect_network(const struct device *dev, int cur_mode)
{
	struct net_if *iface = net_if_lookup_by_dev(dev);
	struct wpa_supplicant *wpa_s;
	bool is_ap = false;
	int ret = 0;

	if (!iface) {
		ret = -ENOENT;
		wpa_printf(MSG_ERROR, "Interface for device %s not found", dev->name);
		return ret;
	}

	wpa_s = get_wpa_s_handle(dev);
	if (!wpa_s) {
		ret = -1;
		wpa_printf(MSG_ERROR, "Interface %s not found", dev->name);
		goto out;
	}

	k_mutex_lock(&wpa_supplicant_mutex, K_FOREVER);

	if (wpa_s->current_ssid && wpa_s->current_ssid->mode != cur_mode) {
		ret = -EBUSY;
		wpa_printf(MSG_ERROR, "Interface %s is not in %s mode", dev->name,
			   cur_mode == WPAS_MODE_INFRA ? "STA" : "AP");
		goto out;
	}

	is_ap = (cur_mode == WPAS_MODE_AP);

	wpas_api_ctrl.dev = dev;
	wpas_api_ctrl.requested_op = DISCONNECT;

	if (!wpa_cli_cmd_v("disconnect")) {
		goto out;
	}

out:
	k_mutex_unlock(&wpa_supplicant_mutex);

	if (ret) {
		wpa_printf(MSG_ERROR, "Disconnect failed: %s", strerror(-ret));
		return ret;
	}

	wpa_supp_restart_status_work();

	ret = wait_for_disconnect_complete(dev);
#ifdef CONFIG_AP
	if (is_ap) {
		supplicant_send_wifi_mgmt_ap_status(wpa_s,
			NET_EVENT_WIFI_CMD_AP_DISABLE_RESULT,
			ret == 0 ? WIFI_STATUS_AP_SUCCESS : WIFI_STATUS_AP_FAIL);
	} else {
#else
	{
#endif /* CONFIG_AP */
		wifi_mgmt_raise_disconnect_complete_event(iface, ret);
	}

	return ret;
}

/* Public API */
int supplicant_connect(const struct device *dev, struct wifi_connect_req_params *params)
{
	struct wpa_supplicant *wpa_s;
	int ret = 0;

	if (!net_if_is_admin_up(net_if_lookup_by_dev(dev))) {
		wpa_printf(MSG_ERROR,
			   "Interface %s is down, dropping connect",
			   dev->name);
		return -1;
	}

	k_mutex_lock(&wpa_supplicant_mutex, K_FOREVER);

	wpa_s = get_wpa_s_handle(dev);
	if (!wpa_s) {
		ret = -1;
		wpa_printf(MSG_ERROR, "Device %s not found", dev->name);
		goto out;
	}

	/* Allow connect in STA mode only even if we are connected already */
	if  (wpa_s->current_ssid && wpa_s->current_ssid->mode != WPAS_MODE_INFRA) {
		ret = -EBUSY;
		wpa_printf(MSG_ERROR, "Interface %s is not in STA mode", dev->name);
		goto out;
	}

	ret = wpas_add_and_config_network(wpa_s, params, false);
	if (ret) {
		wpa_printf(MSG_ERROR, "Failed to add and configure network for STA mode: %d", ret);
		goto out;
	}

	wpas_api_ctrl.dev = dev;
	wpas_api_ctrl.requested_op = CONNECT;
	wpas_api_ctrl.connection_timeout = params->timeout;

out:
	k_mutex_unlock(&wpa_supplicant_mutex);

	if (!ret) {
		wpa_supp_restart_status_work();
	}

	return ret;
}

int supplicant_disconnect(const struct device *dev)
{
	return wpas_disconnect_network(dev, WPAS_MODE_INFRA);
}

int supplicant_status(const struct device *dev, struct wifi_iface_status *status)
{
	struct net_if *iface = net_if_lookup_by_dev(dev);
	struct wpa_supplicant *wpa_s;
	int ret = -1;
	struct wpa_signal_info *si = NULL;
	struct wpa_conn_info *conn_info = NULL;

	if (!iface) {
		ret = -ENOENT;
		wpa_printf(MSG_ERROR, "Interface for device %s not found", dev->name);
		return ret;
	}

	k_mutex_lock(&wpa_supplicant_mutex, K_FOREVER);

	wpa_s = get_wpa_s_handle(dev);
	if (!wpa_s) {
		wpa_printf(MSG_ERROR, "Device %s not found", dev->name);
		goto out;
	}

	si = os_zalloc(sizeof(struct wpa_signal_info));
	if (!si) {
		wpa_printf(MSG_ERROR, "Failed to allocate memory for signal info");
		goto out;
	}

	status->state = wpa_s->wpa_state; /* 1-1 Mapping */

	if (wpa_s->wpa_state >= WPA_ASSOCIATED) {
		struct wpa_ssid *ssid = wpa_s->current_ssid;
		u8 channel;
		struct signal_poll_resp signal_poll;
		u8 *_ssid = ssid->ssid;
		size_t ssid_len = ssid->ssid_len;
		struct status_resp cli_status;
		bool is_ap;
		int proto;
		int key_mgmt;

		if (!ssid) {
			wpa_printf(MSG_ERROR, "Failed to get current ssid");
			goto out;
		}

		is_ap = ssid->mode == WPAS_MODE_AP;
		/* For AP its always the configured one */
		proto = is_ap ? ssid->proto : wpa_s->wpa_proto;
		key_mgmt = is_ap ? ssid->key_mgmt : wpa_s->key_mgmt;
		os_memcpy(status->bssid, wpa_s->bssid, WIFI_MAC_ADDR_LEN);
		status->band = wpas_band_to_zephyr(wpas_freq_to_band(wpa_s->assoc_freq));
		status->security = wpas_key_mgmt_to_zephyr(key_mgmt, proto);
		status->mfp = ssid->ieee80211w; /* Same mapping */
		ieee80211_freq_to_chan(wpa_s->assoc_freq, &channel);
		status->channel = channel;

		if (ssid_len == 0) {
			int _res = z_wpa_ctrl_status(&cli_status);

			if (_res < 0) {
				ssid_len = 0;
			} else {
				ssid_len = cli_status.ssid_len;
			}

			_ssid = cli_status.ssid;
		}

		os_memcpy(status->ssid, _ssid, ssid_len);
		status->ssid_len = ssid_len;
		status->iface_mode = ssid->mode;

		if (wpa_s->connection_set == 1) {
			status->link_mode = wpa_s->connection_he ? WIFI_6 :
					wpa_s->connection_vht ? WIFI_5 :
					wpa_s->connection_ht ? WIFI_4 :
					wpa_s->connection_g ? WIFI_3 :
					wpa_s->connection_a ? WIFI_2 :
					wpa_s->connection_b ? WIFI_1 :
					WIFI_0;
		} else {
			status->link_mode = WIFI_LINK_MODE_UNKNOWN;
		}

		status->rssi = -WPA_INVALID_NOISE;
		if (status->iface_mode == WIFI_MODE_INFRA) {
			ret = z_wpa_ctrl_signal_poll(&signal_poll);
			if (!ret) {
				status->rssi = signal_poll.rssi;
			} else {
				wpa_printf(MSG_WARNING, "%s:Failed to read RSSI", __func__);
			}
		}

		conn_info = os_zalloc(sizeof(struct wpa_conn_info));
		if (!conn_info) {
			wpa_printf(MSG_ERROR, "%s:Failed to allocate memory\n",
				__func__);
			ret = -ENOMEM;
			goto out;
		}

		ret = wpa_drv_get_conn_info(wpa_s, conn_info);
		if (!ret) {
			status->beacon_interval = conn_info->beacon_interval;
			status->dtim_period = conn_info->dtim_period;
			status->twt_capable = conn_info->twt_capable;
		} else {
			wpa_printf(MSG_WARNING, "%s: Failed to get connection info\n",
				__func__);

			status->beacon_interval = 0;
			status->dtim_period = 0;
			status->twt_capable = false;
			ret = 0;
		}

		os_free(conn_info);

		ret = wpa_drv_signal_poll(wpa_s, si);
		if (!ret) {
			status->current_phy_rate = si->current_txrate;
		} else {
			wpa_printf(MSG_WARNING, "%s: Failed to get signal info\n", __func__);
			status->current_phy_rate = 0;
			ret = 0;
		}
	} else {
		ret = 0;
	}

out:
	os_free(si);
	k_mutex_unlock(&wpa_supplicant_mutex);
	return ret;
}

/* Below APIs are not natively supported by WPA supplicant, so,
 * these are just wrappers around driver offload APIs. But it is
 * transparent to the user.
 *
 * In the future these might be implemented natively by the WPA
 * supplicant.
 */

static const struct wifi_mgmt_ops *const get_wifi_mgmt_api(const struct device *dev)
{
	struct net_wifi_mgmt_offload *api = (struct net_wifi_mgmt_offload *)dev->api;

	return api ? api->wifi_mgmt_api : NULL;
}

int supplicant_get_version(const struct device *dev, struct wifi_version *params)
{
	const struct wifi_mgmt_ops *const wifi_mgmt_api = get_wifi_mgmt_api(dev);

	if (!wifi_mgmt_api || !wifi_mgmt_api->get_version) {
		wpa_printf(MSG_ERROR, "get_version not supported");
		return -ENOTSUP;
	}

	return wifi_mgmt_api->get_version(dev, params);
}

int supplicant_scan(const struct device *dev, struct wifi_scan_params *params,
		    scan_result_cb_t cb)
{
	const struct wifi_mgmt_ops *const wifi_mgmt_api = get_wifi_mgmt_api(dev);

	if (!wifi_mgmt_api || !wifi_mgmt_api->scan) {
		wpa_printf(MSG_ERROR, "Scan not supported");
		return -ENOTSUP;
	}

	return wifi_mgmt_api->scan(dev, params, cb);
}

#ifdef CONFIG_NET_STATISTICS_WIFI
int supplicant_get_stats(const struct device *dev, struct net_stats_wifi *stats)
{
	const struct wifi_mgmt_ops *const wifi_mgmt_api = get_wifi_mgmt_api(dev);

	if (!wifi_mgmt_api || !wifi_mgmt_api->get_stats) {
		wpa_printf(MSG_ERROR, "Get stats not supported");
		return -ENOTSUP;
	}

	return wifi_mgmt_api->get_stats(dev, stats);
}

int supplicant_reset_stats(const struct device *dev)
{
	const struct wifi_mgmt_ops *const wifi_mgmt_api = get_wifi_mgmt_api(dev);

	if (!wifi_mgmt_api || !wifi_mgmt_api->reset_stats) {
		wpa_printf(MSG_WARNING, "Reset stats not supported");
		return -ENOTSUP;
	}

	return wifi_mgmt_api->reset_stats(dev);
}
#endif /* CONFIG_NET_STATISTICS_WIFI */

int supplicant_pmksa_flush(const struct device *dev)
{
	struct wpa_supplicant *wpa_s;
	int ret = 0;

	k_mutex_lock(&wpa_supplicant_mutex, K_FOREVER);

	wpa_s = get_wpa_s_handle(dev);
	if (!wpa_s) {
		ret = -1;
		wpa_printf(MSG_ERROR, "Device %s not found", dev->name);
		goto out;
	}

	if (!wpa_cli_cmd_v("pmksa_flush")) {
		ret = -1;
		wpa_printf(MSG_ERROR, "pmksa_flush failed");
		goto out;
	}

out:
	k_mutex_unlock(&wpa_supplicant_mutex);
	return ret;
}

int supplicant_set_power_save(const struct device *dev, struct wifi_ps_params *params)
{
	const struct wifi_mgmt_ops *const wifi_mgmt_api = get_wifi_mgmt_api(dev);

	if (!wifi_mgmt_api || !wifi_mgmt_api->set_power_save) {
		wpa_printf(MSG_ERROR, "Set power save not supported");
		return -ENOTSUP;
	}

	return wifi_mgmt_api->set_power_save(dev, params);
}

int supplicant_set_twt(const struct device *dev, struct wifi_twt_params *params)
{
	const struct wifi_mgmt_ops *const wifi_mgmt_api = get_wifi_mgmt_api(dev);

	if (!wifi_mgmt_api || !wifi_mgmt_api->set_twt) {
		wpa_printf(MSG_ERROR, "Set TWT not supported");
		return -ENOTSUP;
	}

	return wifi_mgmt_api->set_twt(dev, params);
}

int supplicant_get_power_save_config(const struct device *dev,
				     struct wifi_ps_config *config)
{
	const struct wifi_mgmt_ops *const wifi_mgmt_api = get_wifi_mgmt_api(dev);

	if (!wifi_mgmt_api || !wifi_mgmt_api->get_power_save_config) {
		wpa_printf(MSG_ERROR, "Get power save config not supported");
		return -ENOTSUP;
	}

	return wifi_mgmt_api->get_power_save_config(dev, config);
}

int supplicant_reg_domain(const struct device *dev,
			  struct wifi_reg_domain *reg_domain)
{
	const struct wifi_mgmt_ops *const wifi_mgmt_api = get_wifi_mgmt_api(dev);
	struct wpa_supplicant *wpa_s;
	int ret = -1;

	if (!wifi_mgmt_api || !wifi_mgmt_api->reg_domain) {
		wpa_printf(MSG_ERROR, "Regulatory domain not supported");
		return -ENOTSUP;
	}

	if (reg_domain->oper == WIFI_MGMT_GET) {
		return wifi_mgmt_api->reg_domain(dev, reg_domain);
	}

	if (reg_domain->oper == WIFI_MGMT_SET) {
		k_mutex_lock(&wpa_supplicant_mutex, K_FOREVER);

		wpa_s = get_wpa_s_handle(dev);
		if (!wpa_s) {
			wpa_printf(MSG_ERROR, "Interface %s not found", dev->name);
			goto out;
		}

		if (!wpa_cli_cmd_v("set country %s", reg_domain->country_code)) {
			goto out;
		}

#ifdef CONFIG_WIFI_NM_HOSTAPD_AP
		if (!hostapd_cli_cmd_v("set country_code %s", reg_domain->country_code)) {
			goto out;
		}
#endif

		ret = 0;

out:
		k_mutex_unlock(&wpa_supplicant_mutex);
	}

	return ret;
}

int supplicant_mode(const struct device *dev, struct wifi_mode_info *mode)
{
	const struct wifi_mgmt_ops *const wifi_mgmt_api = get_wifi_mgmt_api(dev);

	if (!wifi_mgmt_api || !wifi_mgmt_api->mode) {
		wpa_printf(MSG_ERROR, "Setting mode not supported");
		return -ENOTSUP;
	}

	return wifi_mgmt_api->mode(dev, mode);
}

int supplicant_filter(const struct device *dev, struct wifi_filter_info *filter)
{
	const struct wifi_mgmt_ops *const wifi_mgmt_api = get_wifi_mgmt_api(dev);

	if (!wifi_mgmt_api || !wifi_mgmt_api->filter) {
		wpa_printf(MSG_ERROR, "Setting filter not supported");
		return -ENOTSUP;
	}

	return wifi_mgmt_api->filter(dev, filter);
}

int supplicant_channel(const struct device *dev, struct wifi_channel_info *channel)
{
	const struct wifi_mgmt_ops *const wifi_mgmt_api = get_wifi_mgmt_api(dev);

	if (!wifi_mgmt_api || !wifi_mgmt_api->channel) {
		wpa_printf(MSG_ERROR, "Setting channel not supported");
		return -ENOTSUP;
	}

	return wifi_mgmt_api->channel(dev, channel);
}

int supplicant_set_rts_threshold(const struct device *dev, unsigned int rts_threshold)
{
	const struct wifi_mgmt_ops *const wifi_mgmt_api = get_wifi_mgmt_api(dev);

	if (!wifi_mgmt_api || !wifi_mgmt_api->set_rts_threshold) {
		wpa_printf(MSG_ERROR, "Set RTS not supported");
		return -ENOTSUP;
	}

	return wifi_mgmt_api->set_rts_threshold(dev, rts_threshold);
}

int supplicant_get_rts_threshold(const struct device *dev, unsigned int *rts_threshold)
{
	const struct wifi_mgmt_ops *const wifi_mgmt_api = get_wifi_mgmt_api(dev);

	if (!wifi_mgmt_api || !wifi_mgmt_api->get_rts_threshold) {
		wpa_printf(MSG_ERROR, "Get RTS not supported");
		return -ENOTSUP;
	}

	return wifi_mgmt_api->get_rts_threshold(dev, rts_threshold);
}

#ifdef CONFIG_WIFI_NM_WPA_SUPPLICANT_WNM
int supplicant_btm_query(const struct device *dev, uint8_t reason)
{
	struct wpa_supplicant *wpa_s;
	int ret = -1;

	k_mutex_lock(&wpa_supplicant_mutex, K_FOREVER);

	wpa_s = get_wpa_s_handle(dev);
	if (!wpa_s) {
		ret = -1;
		wpa_printf(MSG_ERROR, "Interface %s not found", dev->name);
		goto out;
	}

	if (!wpa_cli_cmd_v("wnm_bss_query %d", reason)) {
		goto out;
	}

	ret = 0;

out:
	k_mutex_unlock(&wpa_supplicant_mutex);

	return ret;
}
#endif

int supplicant_get_wifi_conn_params(const struct device *dev,
			struct wifi_connect_req_params *params)
{
	struct wpa_supplicant *wpa_s;
	int ret = 0;

	k_mutex_lock(&wpa_supplicant_mutex, K_FOREVER);

	wpa_s = get_wpa_s_handle(dev);
	if (!wpa_s) {
		ret = -1;
		wpa_printf(MSG_ERROR, "Device %s not found", dev->name);
		goto out;
	}

	memcpy(params, &last_wifi_conn_params, sizeof(struct wifi_connect_req_params));
out:
	k_mutex_unlock(&wpa_supplicant_mutex);
	return ret;
}

#ifdef CONFIG_AP
#ifdef CONFIG_WIFI_NM_HOSTAPD_AP
int hapd_state(const struct device *dev, int *state)
{
	struct hostapd_iface *iface;
	int ret = 0;

	k_mutex_lock(&wpa_supplicant_mutex, K_FOREVER);

	iface = get_hostapd_handle(dev);
	if (!iface) {
		wpa_printf(MSG_ERROR, "Device %s not found", dev->name);
		ret = -ENOENT;
		goto out;
	}

	*state = iface->state;

out:
	k_mutex_unlock(&wpa_supplicant_mutex);
	return ret;
}

int hapd_config_network(struct hostapd_iface *iface,
			struct wifi_connect_req_params *params)
{
	int ret = 0;

	if (!hostapd_cli_cmd_v("set ssid %s", params->ssid)) {
		goto out;
	}

	if (params->channel == 0) {
		if (params->band == 0) {
			if (!hostapd_cli_cmd_v("set hw_mode g")) {
				goto out;
			}
		} else if (params->band == 1) {
			if (!hostapd_cli_cmd_v("set hw_mode a")) {
				goto out;
			}
		} else {
			wpa_printf(MSG_ERROR, "Invalid band %d", params->band);
			goto out;
		}
	} else if (params->channel > 14) {
		if (!hostapd_cli_cmd_v("set hw_mode a")) {
			goto out;
		}
	} else {
		if (!hostapd_cli_cmd_v("set hw_mode g")) {
			goto out;
		}
	}

	if (!hostapd_cli_cmd_v("set channel %d", params->channel)) {
		goto out;
	}

	if (params->security != WIFI_SECURITY_TYPE_NONE) {
		if (params->security == WIFI_SECURITY_TYPE_WPA_PSK) {
			if (!hostapd_cli_cmd_v("set wpa 1")) {
				goto out;
			}
			if (!hostapd_cli_cmd_v("set wpa_key_mgmt WPA-PSK")) {
				goto out;
			}
			if (!hostapd_cli_cmd_v("set wpa_passphrase \"%s\"", params->psk)) {
				goto out;
			}
			if (!hostapd_cli_cmd_v("set wpa_pairwise CCMP")) {
				goto out;
			}
		} else if (params->security == WIFI_SECURITY_TYPE_PSK) {
			if (!hostapd_cli_cmd_v("set wpa 2")) {
				goto out;
			}
			if (!hostapd_cli_cmd_v("set wpa_key_mgmt WPA-PSK")) {
				goto out;
			}
			if (!hostapd_cli_cmd_v("set wpa_passphrase \"%s\"", params->psk)) {
				goto out;
			}
			if (!hostapd_cli_cmd_v("set rsn_pairwise CCMP")) {
				goto out;
			}
		} else if (params->security == WIFI_SECURITY_TYPE_PSK_SHA256) {
			if (!hostapd_cli_cmd_v("set wpa 2")) {
				goto out;
			}
			if (!hostapd_cli_cmd_v("set wpa_key_mgmt WPA-PSK-SHA256")) {
				goto out;
			}
			if (!hostapd_cli_cmd_v("set wpa_passphrase \"%s\"", params->psk)) {
				goto out;
			}
			if (!hostapd_cli_cmd_v("set rsn_pairwise CCMP")) {
				goto out;
			}
		} else if (params->security == WIFI_SECURITY_TYPE_SAE) {
			if (!hostapd_cli_cmd_v("set wpa 2")) {
				goto out;
			}
			if (!hostapd_cli_cmd_v("set wpa_key_mgmt SAE")) {
				goto out;
			}
			if (!hostapd_cli_cmd_v("set sae_password \"%s\"",
					       params->sae_password ? params->sae_password :
					       params->psk)) {
				goto out;
			}
			if (!hostapd_cli_cmd_v("set rsn_pairwise CCMP")) {
				goto out;
			}
			if (!hostapd_cli_cmd_v("set sae_pwe 2")) {
				goto out;
			}
			iface->bss[0]->conf->sae_pwe = 2;
		} else if (params->security == WIFI_SECURITY_TYPE_DPP) {
			if (!hostapd_cli_cmd_v("set wpa 2")) {
				goto out;
			}
			if (!hostapd_cli_cmd_v("set wpa_key_mgmt WPA-PSK DPP")) {
				goto out;
			}
			if (!hostapd_cli_cmd_v("set wpa_passphrase %s", params->psk)) {
				goto out;
			}
			if (!hostapd_cli_cmd_v("set wpa_pairwise CCMP")) {
				goto out;
			}
			if (!hostapd_cli_cmd_v("set rsn_pairwise CCMP")) {
				goto out;
			}
			if (!hostapd_cli_cmd_v("set dpp_configurator_connectivity 1")) {
				goto out;
			}
		}
	} else {
		if (!hostapd_cli_cmd_v("set wpa 0")) {
			goto out;
		}
		iface->bss[0]->conf->wpa_key_mgmt = 0;
	}

	if (!hostapd_cli_cmd_v("set ieee80211w %d", params->mfp)) {
		goto out;
	}
out:
	return ret;
}

int supplicant_ap_config_params(const struct device *dev, struct wifi_ap_config_params *params)
{
	struct hostapd_iface *iface;
	const struct wifi_mgmt_ops *const wifi_mgmt_api = get_wifi_mgmt_api(dev);
	int ret = 0;

	if (params->type & WIFI_AP_CONFIG_PARAM_MAX_INACTIVITY) {
		if (!wifi_mgmt_api || !wifi_mgmt_api->ap_config_params) {
			wpa_printf(MSG_ERROR, "ap_config_params not supported");
			return -ENOTSUP;
		}

		ret = wifi_mgmt_api->ap_config_params(dev, params);
		if (ret) {
			wpa_printf(MSG_ERROR,
				   "Failed to set maximum inactivity duration for stations");
		} else {
			wpa_printf(MSG_INFO, "Set maximum inactivity duration for stations: %d (s)",
				   params->max_inactivity);
		}
	}
	if (params->type & WIFI_AP_CONFIG_PARAM_MAX_NUM_STA) {
		k_mutex_lock(&wpa_supplicant_mutex, K_FOREVER);

		iface = get_hostapd_handle(dev);
		if (!iface) {
			ret = -ENOENT;
			wpa_printf(MSG_ERROR, "Interface %s not found", dev->name);
			goto out;
		}

		if (iface->state > HAPD_IFACE_DISABLED) {
			ret = -EBUSY;
			wpa_printf(MSG_ERROR, "Interface %s is not in disable state", dev->name);
			goto out;
		}

		if (!hostapd_cli_cmd_v("set max_num_sta %d", params->max_num_sta)) {
			ret = -EINVAL;
			wpa_printf(MSG_ERROR, "Failed to set maximum number of stations");
			goto out;
		}
		wpa_printf(MSG_INFO, "Set maximum number of stations: %d", params->max_num_sta);

out:
		k_mutex_unlock(&wpa_supplicant_mutex);
	}

	return ret;
}
#endif

int supplicant_ap_enable(const struct device *dev,
			 struct wifi_connect_req_params *params)
{
#ifdef CONFIG_WIFI_NM_HOSTAPD_AP
	struct hostapd_iface *iface;
	struct hostapd_data *hapd;
	struct wpa_driver_capa capa;
#else
	struct wpa_supplicant *wpa_s;
#endif
	int ret;

	if (!net_if_is_admin_up(net_if_lookup_by_dev(dev))) {
		wpa_printf(MSG_ERROR,
			   "Interface %s is down, dropping connect",
			   dev->name);
		return -1;
	}

	k_mutex_lock(&wpa_supplicant_mutex, K_FOREVER);

#ifdef CONFIG_WIFI_NM_HOSTAPD_AP
	iface = get_hostapd_handle(dev);
	if (!iface) {
		ret = -1;
		wpa_printf(MSG_ERROR, "Interface %s not found", dev->name);
		goto out;
	}

	if (iface->state == HAPD_IFACE_ENABLED) {
		ret = -EBUSY;
		wpa_printf(MSG_ERROR, "Interface %s is not in disable state", dev->name);
		goto out;
	}

	ret = hapd_config_network(iface, params);
	if (ret) {
		wpa_printf(MSG_ERROR, "Failed to configure network for AP: %d", ret);
		goto out;
	}

	hapd = iface->bss[0];
	if (!iface->extended_capa || !iface->extended_capa_mask) {
		if (hapd->driver->get_capa && hapd->driver->get_capa(hapd->drv_priv, &capa) == 0) {
			iface->extended_capa         = capa.extended_capa;
			iface->extended_capa_mask    = capa.extended_capa_mask;
			iface->extended_capa_len     = capa.extended_capa_len;
			iface->drv_max_acl_mac_addrs = capa.max_acl_mac_addrs;

			/*
			 * Override extended capa with per-interface type (AP), if
			 * available from the driver.
			 */
			hostapd_get_ext_capa(iface);
		} else {
			ret = -1;
			wpa_printf(MSG_ERROR, "Failed to get capability for AP: %d", ret);
			goto out;
		}
	}

	if (!hostapd_cli_cmd_v("enable")) {
		goto out;
	}

#else
	wpa_s = get_wpa_s_handle(dev);
	if (!wpa_s) {
		ret = -1;
		wpa_printf(MSG_ERROR, "Interface %s not found", dev->name);
		goto out;
	}

	if (wpa_s->wpa_state != WPA_DISCONNECTED) {
		ret = -EBUSY;
		wpa_printf(MSG_ERROR, "Interface %s is not in disconnected state", dev->name);
		goto out;
	}

	/* No need to check for existing network to join for SoftAP*/
	wpa_s->conf->ap_scan = 2;
	/* Set BSS parameter max_num_sta to default configured value */
	wpa_s->conf->max_num_sta = CONFIG_WIFI_MGMT_AP_MAX_NUM_STA;

	ret = wpas_add_and_config_network(wpa_s, params, true);
	if (ret) {
		wpa_printf(MSG_ERROR, "Failed to add and configure network for AP mode: %d", ret);
		goto out;
	}
#endif

out:
	k_mutex_unlock(&wpa_supplicant_mutex);

	return ret;
}

int supplicant_ap_disable(const struct device *dev)
{
#ifdef CONFIG_WIFI_NM_HOSTAPD_AP
	struct hostapd_iface *iface;
	int ret = 0;
#else
	struct wpa_supplicant *wpa_s;
	int ret = -1;
#endif

	k_mutex_lock(&wpa_supplicant_mutex, K_FOREVER);

#ifdef CONFIG_WIFI_NM_HOSTAPD_AP
	iface = get_hostapd_handle(dev);
	if (!iface) {
		ret = -ENOENT;
		wpa_printf(MSG_ERROR, "Interface %s not found", dev->name);
		goto out;
	}

	if (iface->state != HAPD_IFACE_ENABLED) {
		ret = -EBUSY;
		wpa_printf(MSG_ERROR, "Interface %s is not in enable state", dev->name);
		goto out;
	}

	if (!hostapd_cli_cmd_v("disable")) {
		goto out;
	}

	iface->freq = 0;
#else
	wpa_s = get_wpa_s_handle(dev);
	if (!wpa_s) {
		ret = -1;
		wpa_printf(MSG_ERROR, "Interface %s not found", dev->name);
		goto out;
	}

	ret = wpas_disconnect_network(dev, WPAS_MODE_AP);
	if (ret) {
		wpa_printf(MSG_ERROR, "Failed to disconnect from network");
		goto out;
	}

	/* Restore ap_scan to default value */
	wpa_s->conf->ap_scan = 1;
#endif

out:
	k_mutex_unlock(&wpa_supplicant_mutex);
	return ret;
}

int supplicant_ap_sta_disconnect(const struct device *dev,
				 const uint8_t *mac_addr)
{
#ifdef CONFIG_WIFI_NM_HOSTAPD_AP
	struct hostapd_iface *iface;
	int ret  = 0;
#else
	struct wpa_supplicant *wpa_s;
	int ret = -1;
#endif

	k_mutex_lock(&wpa_supplicant_mutex, K_FOREVER);

#ifdef CONFIG_WIFI_NM_HOSTAPD_AP
	iface = get_hostapd_handle(dev);
	if (!iface) {
		ret = -1;
		wpa_printf(MSG_ERROR, "Interface %s not found", dev->name);
		goto out;
	}

	if (iface->state != HAPD_IFACE_ENABLED) {
		ret = -EBUSY;
		wpa_printf(MSG_ERROR, "Interface %s is not in enable state", dev->name);
		goto out;
	}

	if (!mac_addr) {
		ret = -EINVAL;
		wpa_printf(MSG_ERROR, "Invalid MAC address");
		goto out;
	}

	if (!hostapd_cli_cmd_v("deauthenticate %02x:%02x:%02x:%02x:%02x:%02x",
				mac_addr[0], mac_addr[1], mac_addr[2],
				mac_addr[3], mac_addr[4], mac_addr[5])) {
		goto out;
	}

#else
	wpa_s = get_wpa_s_handle(dev);
	if (!wpa_s) {
		ret = -1;
		wpa_printf(MSG_ERROR, "Interface %s not found", dev->name);
		goto out;
	}

	if (!mac_addr) {
		ret = -EINVAL;
		wpa_printf(MSG_ERROR, "Invalid MAC address");
		goto out;
	}

	if (!wpa_cli_cmd_v("disassociate %02x:%02x:%02x:%02x:%02x:%02x",
			   mac_addr[0], mac_addr[1], mac_addr[2],
			   mac_addr[3], mac_addr[4], mac_addr[5])) {
		goto out;
	}

	ret = 0;
#endif

out:
	k_mutex_unlock(&wpa_supplicant_mutex);

	return ret;
}
#endif /* CONFIG_AP */

#ifdef CONFIG_WIFI_NM_WPA_SUPPLICANT_DPP
static const char *dpp_params_to_args_curve(int curve)
{
	switch (curve) {
	case WIFI_DPP_CURVES_P_256:
		return "P-256";
	case WIFI_DPP_CURVES_P_384:
		return "P-384";
	case WIFI_DPP_CURVES_P_512:
		return "P-521";
	case WIFI_DPP_CURVES_BP_256:
		return "BP-256";
	case WIFI_DPP_CURVES_BP_384:
		return "BP-384";
	case WIFI_DPP_CURVES_BP_512:
		return "BP-512";
	default:
		return "P-256";
	}
}

static const char *dpp_params_to_args_conf(int conf)
{
	switch (conf) {
	case WIFI_DPP_CONF_STA:
		return "sta-dpp";
	case WIFI_DPP_CONF_AP:
		return "ap-dpp";
	case WIFI_DPP_CONF_QUERY:
		return "query";
	default:
		return "sta-dpp";
	}
}

static const char *dpp_params_to_args_role(int role)
{
	switch (role) {
	case WIFI_DPP_ROLE_CONFIGURATOR:
		return "configurator";
	case WIFI_DPP_ROLE_ENROLLEE:
		return "enrollee";
	case WIFI_DPP_ROLE_EITHER:
		return "either";
	default:
		return "either";
	}
}

static void dpp_ssid_bin2str(char *dst, uint8_t *src, int max_len)
{
	uint8_t *end = src + strlen(src);

	/* do 4 bytes convert first */
	for (; (src + 4) < end; src += 4) {
		snprintf(dst, max_len, "%02x%02x%02x%02x",
			 src[0], src[1], src[2], src[3]);
		dst += 8;
	}

	/* then do 1 byte convert */
	for (; src < end; src++) {
		snprintf(dst, max_len, "%02x", src[0]);
		dst += 2;
	}
}

#define SUPPLICANT_DPP_CMD_BUF_SIZE 384
#define STR_CUR_TO_END(cur) (cur) = (&(cur)[0] + strlen((cur)))

static int dpp_params_to_cmd(struct wifi_dpp_params *params, char *cmd, size_t max_len)
{
	char *pos = cmd;
	char *end = cmd + max_len;

	switch (params->action) {
	case WIFI_DPP_CONFIGURATOR_ADD:
		strncpy(pos, "DPP_CONFIGURATOR_ADD", end - pos);
		STR_CUR_TO_END(pos);

		if (params->configurator_add.curve) {
			snprintf(pos, end - pos, " curve=%s",
				 dpp_params_to_args_curve(params->configurator_add.curve));
			STR_CUR_TO_END(pos);
		}

		if (params->configurator_add.net_access_key_curve) {
			snprintf(pos, end - pos, " net_access_key_curve=%s",
				 dpp_params_to_args_curve(
				 params->configurator_add.net_access_key_curve));
		}
		break;
	case WIFI_DPP_AUTH_INIT:
		strncpy(pos, "DPP_AUTH_INIT", end - pos);
		STR_CUR_TO_END(pos);

		if (params->auth_init.peer) {
			snprintf(pos, end - pos, " peer=%d", params->auth_init.peer);
			STR_CUR_TO_END(pos);
		}

		if (params->auth_init.conf) {
			snprintf(pos, end - pos, " conf=%s",
				 dpp_params_to_args_conf(
				 params->auth_init.conf));
			STR_CUR_TO_END(pos);
		}

		if (params->auth_init.ssid[0]) {
			strncpy(pos, " ssid=", end - pos);
			STR_CUR_TO_END(pos);
			dpp_ssid_bin2str(pos, params->auth_init.ssid,
					 WIFI_SSID_MAX_LEN * 2);
			STR_CUR_TO_END(pos);
		}

		if (params->auth_init.configurator) {
			snprintf(pos, end - pos, " configurator=%d",
				 params->auth_init.configurator);
			STR_CUR_TO_END(pos);
		}

		if (params->auth_init.role) {
			snprintf(pos, end - pos, " role=%s",
				 dpp_params_to_args_role(
				 params->auth_init.role));
		}
		break;
	case WIFI_DPP_QR_CODE:
		strncpy(pos, "DPP_QR_CODE", end - pos);
		STR_CUR_TO_END(pos);

		if (params->dpp_qr_code[0]) {
			snprintf(pos, end - pos, " %s", params->dpp_qr_code);
		}
		break;
	case WIFI_DPP_CHIRP:
		strncpy(pos, "DPP_CHIRP", end - pos);
		STR_CUR_TO_END(pos);

		if (params->chirp.id) {
			snprintf(pos, end - pos, " own=%d", params->chirp.id);
			STR_CUR_TO_END(pos);
		}

		if (params->chirp.freq) {
			snprintf(pos, end - pos, " listen=%d", params->chirp.freq);
		}
		break;
	case WIFI_DPP_LISTEN:
		strncpy(pos, "DPP_LISTEN", end - pos);
		STR_CUR_TO_END(pos);

		if (params->listen.freq) {
			snprintf(pos, end - pos, " %d", params->listen.freq);
			STR_CUR_TO_END(pos);
		}

		if (params->listen.role) {
			snprintf(pos, end - pos, " role=%s",
				 dpp_params_to_args_role(
				 params->listen.role));
		}
		break;
	case WIFI_DPP_BOOTSTRAP_GEN:
		strncpy(pos, "DPP_BOOTSTRAP_GEN", end - pos);
		STR_CUR_TO_END(pos);

		if (params->bootstrap_gen.type) {
			strncpy(pos, " type=qrcode", end - pos);
			STR_CUR_TO_END(pos);
		}

		if (params->bootstrap_gen.op_class &&
		    params->bootstrap_gen.chan) {
			snprintf(pos, end - pos, " chan=%d/%d",
				 params->bootstrap_gen.op_class,
				 params->bootstrap_gen.chan);
			STR_CUR_TO_END(pos);
		}

		/* mac is mandatory, even if it is zero mac address */
		snprintf(pos, end - pos, " mac=%02x:%02x:%02x:%02x:%02x:%02x",
			 params->bootstrap_gen.mac[0], params->bootstrap_gen.mac[1],
			 params->bootstrap_gen.mac[2], params->bootstrap_gen.mac[3],
			 params->bootstrap_gen.mac[4], params->bootstrap_gen.mac[5]);
		STR_CUR_TO_END(pos);

		if (params->bootstrap_gen.curve) {
			snprintf(pos, end - pos, " curve=%s",
				 dpp_params_to_args_curve(params->bootstrap_gen.curve));
		}
		break;
	case WIFI_DPP_BOOTSTRAP_GET_URI:
		snprintf(pos, end - pos, "DPP_BOOTSTRAP_GET_URI %d", params->id);
		break;
	case WIFI_DPP_SET_CONF_PARAM:
		strncpy(pos, "SET dpp_configurator_params", end - pos);
		STR_CUR_TO_END(pos);

		if (params->configurator_set.peer) {
			snprintf(pos, end - pos, " peer=%d", params->configurator_set.peer);
			STR_CUR_TO_END(pos);
		}

		if (params->configurator_set.conf) {
			snprintf(pos, end - pos, " conf=%s",
				 dpp_params_to_args_conf(
				 params->configurator_set.conf));
			STR_CUR_TO_END(pos);
		}

		if (params->configurator_set.ssid[0]) {
			strncpy(pos, " ssid=", end - pos);
			STR_CUR_TO_END(pos);
			dpp_ssid_bin2str(pos, params->configurator_set.ssid,
					 WIFI_SSID_MAX_LEN * 2);
			STR_CUR_TO_END(pos);
		}

		if (params->configurator_set.configurator) {
			snprintf(pos, end - pos, " configurator=%d",
				 params->configurator_set.configurator);
			STR_CUR_TO_END(pos);
		}

		if (params->configurator_set.role) {
			snprintf(pos, end - pos, " role=%s",
				 dpp_params_to_args_role(
				 params->configurator_set.role));
			STR_CUR_TO_END(pos);
		}

		if (params->configurator_set.curve) {
			snprintf(pos, end - pos, " curve=%s",
				 dpp_params_to_args_curve(params->configurator_set.curve));
			STR_CUR_TO_END(pos);
		}

		if (params->configurator_set.net_access_key_curve) {
			snprintf(pos, end - pos, " net_access_key_curve=%s",
				 dpp_params_to_args_curve(
				 params->configurator_set.net_access_key_curve));
		}
		break;
	case WIFI_DPP_SET_WAIT_RESP_TIME:
		snprintf(pos, end - pos, "SET dpp_resp_wait_time %d",
			 params->dpp_resp_wait_time);
		break;
	case WIFI_DPP_RECONFIG:
		snprintf(pos, end - pos, "DPP_RECONFIG %d", params->network_id);
		break;
	default:
		wpa_printf(MSG_ERROR, "Unknown DPP action");
		return -EINVAL;
	}

	return 0;
}

int supplicant_dpp_dispatch(const struct device *dev, struct wifi_dpp_params *params)
{
	int ret;
	char *cmd = NULL;

	if (params == NULL) {
		return -EINVAL;
	}

	cmd = os_zalloc(SUPPLICANT_DPP_CMD_BUF_SIZE);
	if (cmd == NULL) {
		return -ENOMEM;
	}

	/* leave one byte always be 0 */
	ret = dpp_params_to_cmd(params, cmd, SUPPLICANT_DPP_CMD_BUF_SIZE - 2);
	if (ret) {
		os_free(cmd);
		return ret;
	}

	wpa_printf(MSG_DEBUG, "wpa_cli %s", cmd);
	if (zephyr_wpa_cli_cmd_resp(cmd, params->resp)) {
		os_free(cmd);
		return -ENOEXEC;
	}

	os_free(cmd);
	return 0;
}
#endif /* CONFIG_WIFI_NM_WPA_SUPPLICANT_DPP */

#ifdef CONFIG_WIFI_NM_HOSTAPD_AP
int hapd_dpp_dispatch(const struct device *dev, struct wifi_dpp_params *params)
{
	int ret;
	char *cmd = NULL;

	if (params == NULL) {
		return -EINVAL;
	}

	cmd = os_zalloc(SUPPLICANT_DPP_CMD_BUF_SIZE);
	if (cmd == NULL) {
		return -ENOMEM;
	}

	/* leave one byte always be 0 */
	ret = dpp_params_to_cmd(params, cmd, SUPPLICANT_DPP_CMD_BUF_SIZE - 2);
	if (ret) {
		os_free(cmd);
		return ret;
	}

	wpa_printf(MSG_DEBUG, "hostapd_cli %s", cmd);
	if (zephyr_hostapd_cli_cmd_resp(cmd, params->resp)) {
		os_free(cmd);
		return -ENOEXEC;
	}

	os_free(cmd);
	return 0;
}
#endif /* CONFIG_WIFI_NM_HOSTAPD_AP */
