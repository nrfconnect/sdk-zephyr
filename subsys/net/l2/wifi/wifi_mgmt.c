/*
 * Copyright (c) 2016 Intel Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(net_wifi_mgmt, CONFIG_NET_L2_WIFI_MGMT_LOG_LEVEL);

#include <errno.h>

#include <zephyr/net/net_core.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/wifi_mgmt.h>
#ifdef CONFIG_WIFI_NM
#include <zephyr/net/wifi_nm.h>
#endif /* CONFIG_WIFI_NM */

static const struct wifi_mgmt_ops *const get_wifi_api(struct net_if *iface)
{
	const struct device *dev = net_if_get_device(iface);
	struct net_wifi_mgmt_offload *off_api =
			(struct net_wifi_mgmt_offload *) dev->api;
#ifdef CONFIG_WIFI_NM
	struct wifi_nm_instance *nm = wifi_nm_get_instance_iface(iface);

	if (nm) {
		return nm->ops;
	}
#endif /* CONFIG_WIFI_NM */
	return off_api ? off_api->wifi_mgmt_api : NULL;
}

static int wifi_connect(uint32_t mgmt_request, struct net_if *iface,
			void *data, size_t len)
{
	struct wifi_connect_req_params *params =
		(struct wifi_connect_req_params *)data;
	const struct device *dev = net_if_get_device(iface);

	const struct wifi_mgmt_ops *const wifi_mgmt_api = get_wifi_api(iface);

	if (wifi_mgmt_api == NULL || wifi_mgmt_api->connect == NULL) {
		return -ENOTSUP;
	}

	LOG_HEXDUMP_DBG(params->ssid, params->ssid_length, "ssid");
	LOG_HEXDUMP_DBG(params->psk, params->psk_length, "psk");
	if (params->sae_password) {
		LOG_HEXDUMP_DBG(params->sae_password, params->sae_password_length, "sae");
	}
	NET_DBG("ch %u sec %u", params->channel, params->security);

	if ((params->security > WIFI_SECURITY_TYPE_MAX) ||
	    (params->ssid_length > WIFI_SSID_MAX_LEN) ||
	    (params->ssid_length == 0U) ||
	    ((params->security == WIFI_SECURITY_TYPE_PSK ||
		  params->security == WIFI_SECURITY_TYPE_PSK_SHA256) &&
	     ((params->psk_length < 8) || (params->psk_length > 64) ||
	      (params->psk_length == 0U) || !params->psk)) ||
	    ((params->security == WIFI_SECURITY_TYPE_SAE) &&
	      ((params->psk_length == 0U) || !params->psk) &&
		  ((params->sae_password_length == 0U) || !params->sae_password)) ||
	    ((params->channel != WIFI_CHANNEL_ANY) &&
	     (params->channel > WIFI_CHANNEL_MAX)) ||
	    !params->ssid) {
		return -EINVAL;
	}

	return wifi_mgmt_api->connect(dev, params);
}

NET_MGMT_REGISTER_REQUEST_HANDLER(NET_REQUEST_WIFI_CONNECT, wifi_connect);

static void scan_result_cb(struct net_if *iface, int status,
			    struct wifi_scan_result *entry)
{
	if (!iface) {
		return;
	}

	if (!entry) {
		struct wifi_status scan_status = {
			.status = status,
		};

		net_mgmt_event_notify_with_info(NET_EVENT_WIFI_SCAN_DONE,
						iface, &scan_status,
						sizeof(struct wifi_status));
		return;
	}

#ifndef CONFIG_WIFI_MGMT_RAW_SCAN_RESULTS_ONLY
	net_mgmt_event_notify_with_info(NET_EVENT_WIFI_SCAN_RESULT, iface,
					entry, sizeof(struct wifi_scan_result));
#endif /* CONFIG_WIFI_MGMT_RAW_SCAN_RESULTS_ONLY */
}

static int wifi_scan(uint32_t mgmt_request, struct net_if *iface,
		     void *data, size_t len)
{
	const struct device *dev = net_if_get_device(iface);
	struct wifi_scan_params *params = data;
	const struct wifi_mgmt_ops *const wifi_mgmt_api = get_wifi_api(iface);

	if (wifi_mgmt_api == NULL || wifi_mgmt_api->scan == NULL) {
		return -ENOTSUP;
	}

	if (data && (len == sizeof(*params))) {
#ifdef CONFIG_WIFI_MGMT_FORCED_PASSIVE_SCAN
		params->scan_type = WIFI_SCAN_TYPE_PASSIVE;
#endif
	}

	return wifi_mgmt_api->scan(dev, params, scan_result_cb);
}

NET_MGMT_REGISTER_REQUEST_HANDLER(NET_REQUEST_WIFI_SCAN, wifi_scan);

static int wifi_disconnect(uint32_t mgmt_request, struct net_if *iface,
			   void *data, size_t len)
{
	const struct device *dev = net_if_get_device(iface);
	const struct wifi_mgmt_ops *const wifi_mgmt_api = get_wifi_api(iface);

	if (wifi_mgmt_api == NULL || wifi_mgmt_api->disconnect == NULL) {
		return -ENOTSUP;
	}

	return wifi_mgmt_api->disconnect(dev);
}

NET_MGMT_REGISTER_REQUEST_HANDLER(NET_REQUEST_WIFI_DISCONNECT, wifi_disconnect);

void wifi_mgmt_raise_connect_result_event(struct net_if *iface, int status)
{
	struct wifi_status cnx_status = {
		.status = status,
	};

	net_mgmt_event_notify_with_info(NET_EVENT_WIFI_CONNECT_RESULT,
					iface, &cnx_status,
					sizeof(struct wifi_status));
}

void wifi_mgmt_raise_disconnect_result_event(struct net_if *iface, int status)
{
	struct wifi_status cnx_status = {
		.status = status,
	};

	net_mgmt_event_notify_with_info(NET_EVENT_WIFI_DISCONNECT_RESULT,
					iface, &cnx_status,
					sizeof(struct wifi_status));
}

static int wifi_ap_enable(uint32_t mgmt_request, struct net_if *iface,
			  void *data, size_t len)
{
	struct wifi_connect_req_params *params =
		(struct wifi_connect_req_params *)data;
	const struct device *dev = net_if_get_device(iface);
	const struct wifi_mgmt_ops *const wifi_mgmt_api = get_wifi_api(iface);

	if (wifi_mgmt_api == NULL || wifi_mgmt_api->ap_enable == NULL) {
		return -ENOTSUP;
	}

	return wifi_mgmt_api->ap_enable(dev, params);
}

NET_MGMT_REGISTER_REQUEST_HANDLER(NET_REQUEST_WIFI_AP_ENABLE, wifi_ap_enable);

static int wifi_ap_disable(uint32_t mgmt_request, struct net_if *iface,
			  void *data, size_t len)
{
	const struct device *dev = net_if_get_device(iface);
	const struct wifi_mgmt_ops *const wifi_mgmt_api = get_wifi_api(iface);

	if (wifi_mgmt_api == NULL || wifi_mgmt_api->ap_enable == NULL) {
		return -ENOTSUP;
	}

	return wifi_mgmt_api->ap_disable(dev);
}

NET_MGMT_REGISTER_REQUEST_HANDLER(NET_REQUEST_WIFI_AP_DISABLE, wifi_ap_disable);

static int wifi_iface_status(uint32_t mgmt_request, struct net_if *iface,
			  void *data, size_t len)
{
	const struct device *dev = net_if_get_device(iface);
	const struct wifi_mgmt_ops *const wifi_mgmt_api = get_wifi_api(iface);
	struct wifi_iface_status *status = data;

	if (wifi_mgmt_api == NULL || wifi_mgmt_api->iface_status == NULL) {
		return -ENOTSUP;
	}

	if (!data || len != sizeof(*status)) {
		return -EINVAL;
	}

	return wifi_mgmt_api->iface_status(dev, status);
}
NET_MGMT_REGISTER_REQUEST_HANDLER(NET_REQUEST_WIFI_IFACE_STATUS, wifi_iface_status);

void wifi_mgmt_raise_iface_status_event(struct net_if *iface,
		struct wifi_iface_status *iface_status)
{
	net_mgmt_event_notify_with_info(NET_EVENT_WIFI_IFACE_STATUS,
					iface, iface_status,
					sizeof(struct wifi_iface_status));
}

#ifdef CONFIG_NET_STATISTICS_WIFI
static int wifi_iface_stats(uint32_t mgmt_request, struct net_if *iface,
			  void *data, size_t len)
{
	const struct device *dev = net_if_get_device(iface);
	const struct wifi_mgmt_ops *const wifi_mgmt_api = get_wifi_api(iface);
	struct net_stats_wifi *stats = data;

	if (wifi_mgmt_api == NULL || wifi_mgmt_api->get_stats == NULL) {
		return -ENOTSUP;
	}

	if (!data || len != sizeof(*stats)) {
		return -EINVAL;
	}

	return wifi_mgmt_api->get_stats(dev, stats);
}
NET_MGMT_REGISTER_REQUEST_HANDLER(NET_REQUEST_STATS_GET_WIFI, wifi_iface_stats);
#endif /* CONFIG_NET_STATISTICS_WIFI */

static int wifi_set_power_save(uint32_t mgmt_request, struct net_if *iface,
			  void *data, size_t len)
{
	const struct device *dev = net_if_get_device(iface);
	const struct wifi_mgmt_ops *const wifi_mgmt_api = get_wifi_api(iface);
	struct wifi_ps_params *ps_params = data;
	struct wifi_iface_status info = { 0 };

	if (wifi_mgmt_api == NULL || wifi_mgmt_api->set_power_save == NULL) {
		return -ENOTSUP;
	}

	switch (ps_params->type) {
	case WIFI_PS_PARAM_LISTEN_INTERVAL:
	case WIFI_PS_PARAM_MODE:
		if (net_mgmt(NET_REQUEST_WIFI_IFACE_STATUS, iface, &info,
			     sizeof(struct wifi_iface_status))) {
			ps_params->fail_reason =
				WIFI_PS_PARAM_FAIL_UNABLE_TO_GET_IFACE_STATUS;
			return -EIO;
		}

		if (info.state == WIFI_STATE_COMPLETED) {
			ps_params->fail_reason =
				WIFI_PS_PARAM_FAIL_DEVICE_CONNECTED;
			return -ENOTSUP;
		}
		break;
	case WIFI_PS_PARAM_STATE:
	case WIFI_PS_PARAM_WAKEUP_MODE:
	case WIFI_PS_PARAM_TIMEOUT:
		break;
	default:
		ps_params->fail_reason =
			WIFI_PS_PARAM_FAIL_OPERATION_NOT_SUPPORTED;
		return -ENOTSUP;
	}

	return wifi_mgmt_api->set_power_save(dev, ps_params);
}

NET_MGMT_REGISTER_REQUEST_HANDLER(NET_REQUEST_WIFI_PS, wifi_set_power_save);

static int wifi_get_power_save_config(uint32_t mgmt_request, struct net_if *iface,
			  void *data, size_t len)
{
	const struct device *dev = net_if_get_device(iface);
	const struct wifi_mgmt_ops *const wifi_mgmt_api = get_wifi_api(iface);
	struct wifi_ps_config *ps_config = data;

	if (wifi_mgmt_api == NULL || wifi_mgmt_api->get_power_save_config == NULL) {
		return -ENOTSUP;
	}

	if (!data || len != sizeof(*ps_config)) {
		return -EINVAL;
	}

	return wifi_mgmt_api->get_power_save_config(dev, ps_config);
}

NET_MGMT_REGISTER_REQUEST_HANDLER(NET_REQUEST_WIFI_PS_CONFIG, wifi_get_power_save_config);

static int wifi_set_twt(uint32_t mgmt_request, struct net_if *iface,
			  void *data, size_t len)
{
	const struct device *dev = net_if_get_device(iface);
	const struct wifi_mgmt_ops *const wifi_mgmt_api = get_wifi_api(iface);
	struct wifi_twt_params *twt_params = data;
	struct wifi_iface_status info = { 0 };

	if (wifi_mgmt_api == NULL || wifi_mgmt_api->set_twt == NULL) {
		twt_params->fail_reason =
			WIFI_TWT_FAIL_OPERATION_NOT_SUPPORTED;
		return -ENOTSUP;
	}

	if (twt_params->operation == WIFI_TWT_TEARDOWN) {
		return wifi_mgmt_api->set_twt(dev, twt_params);
	}

	if (net_mgmt(NET_REQUEST_WIFI_IFACE_STATUS, iface, &info,
			sizeof(struct wifi_iface_status))) {
		twt_params->fail_reason =
			WIFI_TWT_FAIL_UNABLE_TO_GET_IFACE_STATUS;
		goto fail;
	}

	if (info.state != WIFI_STATE_COMPLETED) {
		twt_params->fail_reason =
			WIFI_TWT_FAIL_DEVICE_NOT_CONNECTED;
		goto fail;
	}

#ifdef CONFIG_WIFI_MGMT_TWT_CHECK_IP
	if ((!net_if_ipv4_get_global_addr(iface, NET_ADDR_PREFERRED)) &&
	    (!net_if_ipv6_get_global_addr(NET_ADDR_PREFERRED, &iface))) {
		twt_params->fail_reason =
			WIFI_TWT_FAIL_IP_NOT_ASSIGNED;
		goto fail;
	}
#else
	NET_WARN("Check for valid IP address been disabled. "
		 "Device might be unreachable or might not receive traffic.\n");
#endif /* CONFIG_WIFI_MGMT_TWT_CHECK_IP */

	if (info.link_mode < WIFI_6) {
		twt_params->fail_reason =
			WIFI_TWT_FAIL_PEER_NOT_HE_CAPAB;
		goto fail;
	}

	if (!info.twt_capable) {
		twt_params->fail_reason =
			WIFI_TWT_FAIL_PEER_NOT_TWT_CAPAB;
		goto fail;
	}

	return wifi_mgmt_api->set_twt(dev, twt_params);
fail:
	return -ENOEXEC;

}

NET_MGMT_REGISTER_REQUEST_HANDLER(NET_REQUEST_WIFI_TWT, wifi_set_twt);

void wifi_mgmt_raise_twt_event(struct net_if *iface, struct wifi_twt_params *twt_params)
{
	net_mgmt_event_notify_with_info(NET_EVENT_WIFI_TWT,
					iface, twt_params,
					sizeof(struct wifi_twt_params));
}

static int wifi_reg_domain(uint32_t mgmt_request, struct net_if *iface,
			   void *data, size_t len)
{
	const struct device *dev = net_if_get_device(iface);
	const struct wifi_mgmt_ops *const wifi_mgmt_api = get_wifi_api(iface);
	struct wifi_reg_domain *reg_domain = data;

	if (wifi_mgmt_api == NULL || wifi_mgmt_api->reg_domain == NULL) {
		return -ENOTSUP;
	}

	if (!data || len != sizeof(*reg_domain)) {
		return -EINVAL;
	}

	return wifi_mgmt_api->reg_domain(dev, reg_domain);
}

NET_MGMT_REGISTER_REQUEST_HANDLER(NET_REQUEST_WIFI_REG_DOMAIN, wifi_reg_domain);

void wifi_mgmt_raise_twt_sleep_state(struct net_if *iface,
				     int twt_sleep_state)
{
	net_mgmt_event_notify_with_info(NET_EVENT_WIFI_TWT_SLEEP_STATE,
					iface, &twt_sleep_state,
					sizeof(twt_sleep_state));
}

#ifdef CONFIG_WIFI_MGMT_RAW_SCAN_RESULTS
void wifi_mgmt_raise_raw_scan_result_event(struct net_if *iface,
					   struct wifi_raw_scan_result *raw_scan_result)
{
	if (raw_scan_result->frame_length > CONFIG_WIFI_MGMT_RAW_SCAN_RESULT_LENGTH) {
		LOG_INF("raw scan result frame length = %d too big,"
			 "saving upto max raw scan length = %d",
			 raw_scan_result->frame_length,
			 CONFIG_WIFI_MGMT_RAW_SCAN_RESULT_LENGTH);
	}

	net_mgmt_event_notify_with_info(NET_EVENT_WIFI_RAW_SCAN_RESULT,
					iface, raw_scan_result,
					sizeof(*raw_scan_result));
}
#endif /* CONFIG_WIFI_MGMT_RAW_SCAN_RESULTS */

void wifi_mgmt_raise_disconnect_complete_event(struct net_if *iface,
					       int status)
{
	struct wifi_status cnx_status = {
		.status = status,
	};

	net_mgmt_event_notify_with_info(NET_EVENT_WIFI_DISCONNECT_COMPLETE,
					iface, &cnx_status,
					sizeof(struct wifi_status));
}
