/*
 * Copyright (c) 2019 Oticon A/S
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef EDDT_APP_COMMANDS_H
#define EDDT_APP_COMMANDS_H

enum commands_t {
	CMD_NOTHING = 0,
	CMD_ECHO_REQ,
	CMD_ECHO_RSP,
	CMD_INQUIRE_REQ,
	CMD_INQUIRE_RSP,
	CMD_DISCONNECT_REQ,
	CMD_DISCONNECT_RSP,
	CMD_READ_REMOTE_VERSION_INFORMATION_REQ,
	CMD_READ_REMOTE_VERSION_INFORMATION_RSP,
	CMD_SET_EVENT_MASK_REQ,
	CMD_SET_EVENT_MASK_RSP,
	CMD_RESET_REQ,
	CMD_RESET_RSP,
	CMD_READ_TRANSMIT_POWER_LEVEL_REQ,
	CMD_READ_TRANSMIT_POWER_LEVEL_RSP,
	CMD_SET_CONTROLLER_TO_HOST_FLOW_CONTROL_REQ,
	CMD_SET_CONTROLLER_TO_HOST_FLOW_CONTROL_RSP,
	CMD_HOST_BUFFER_SIZE_REQ,
	CMD_HOST_BUFFER_SIZE_RSP,
	CMD_HOST_NUMBER_OF_COMPLETED_PACKETS_REQ,
	CMD_HOST_NUMBER_OF_COMPLETED_PACKETS_RSP,
	CMD_SET_EVENT_MASK_PAGE_2_REQ,
	CMD_SET_EVENT_MASK_PAGE_2_RSP,
	CMD_WRITE_LE_HOST_SUPPORT_REQ,
	CMD_WRITE_LE_HOST_SUPPORT_RSP,
	CMD_READ_AUTHENTICATED_PAYLOAD_TIMEOUT_REQ,
	CMD_READ_AUTHENTICATED_PAYLOAD_TIMEOUT_RSP,
	CMD_WRITE_AUTHENTICATED_PAYLOAD_TIMEOUT_REQ,
	CMD_WRITE_AUTHENTICATED_PAYLOAD_TIMEOUT_RSP,
	CMD_READ_LOCAL_VERSION_INFORMATION_REQ,
	CMD_READ_LOCAL_VERSION_INFORMATION_RSP,
	CMD_READ_LOCAL_SUPPORTED_COMMANDS_REQ,
	CMD_READ_LOCAL_SUPPORTED_COMMANDS_RSP,
	CMD_READ_LOCAL_SUPPORTED_FEATURES_REQ,
	CMD_READ_LOCAL_SUPPORTED_FEATURES_RSP,
	CMD_READ_BUFFER_SIZE_REQ,
	CMD_READ_BUFFER_SIZE_RSP,
	CMD_READ_BD_ADDR_REQ,
	CMD_READ_BD_ADDR_RSP,
	CMD_READ_RSSI_REQ,
	CMD_READ_RSSI_RSP,
	CMD_LE_SET_EVENT_MASK_REQ,
	CMD_LE_SET_EVENT_MASK_RSP,
	CMD_LE_READ_BUFFER_SIZE_REQ,
	CMD_LE_READ_BUFFER_SIZE_RSP,
	CMD_LE_READ_LOCAL_SUPPORTED_FEATURES_REQ,
	CMD_LE_READ_LOCAL_SUPPORTED_FEATURES_RSP,
	CMD_LE_SET_RANDOM_ADDRESS_REQ,
	CMD_LE_SET_RANDOM_ADDRESS_RSP,
	CMD_LE_SET_ADVERTISING_PARAMETERS_REQ,
	CMD_LE_SET_ADVERTISING_PARAMETERS_RSP,
	CMD_LE_READ_ADVERTISING_CHANNEL_TX_POWER_REQ,
	CMD_LE_READ_ADVERTISING_CHANNEL_TX_POWER_RSP,
	CMD_LE_SET_ADVERTISING_DATA_REQ,
	CMD_LE_SET_ADVERTISING_DATA_RSP,
	CMD_LE_SET_SCAN_RESPONSE_DATA_REQ,
	CMD_LE_SET_SCAN_RESPONSE_DATA_RSP,
	CMD_LE_SET_ADVERTISING_ENABLE_REQ,
	CMD_LE_SET_ADVERTISING_ENABLE_RSP,
	CMD_LE_SET_SCAN_PARAMETERS_REQ,
	CMD_LE_SET_SCAN_PARAMETERS_RSP,
	CMD_LE_SET_SCAN_ENABLE_REQ,
	CMD_LE_SET_SCAN_ENABLE_RSP,
	CMD_LE_CREATE_CONNECTION_REQ,
	CMD_LE_CREATE_CONNECTION_RSP,
	CMD_LE_CREATE_CONNECTION_CANCEL_REQ,
	CMD_LE_CREATE_CONNECTION_CANCEL_RSP,
	CMD_LE_READ_FILTER_ACCEPT_LIST_SIZE_REQ,
	CMD_LE_READ_FILTER_ACCEPT_LIST_SIZE_RSP,
	CMD_LE_CLEAR_FILTER_ACCEPT_LIST_REQ,
	CMD_LE_CLEAR_FILTER_ACCEPT_LIST_RSP,
	CMD_LE_ADD_DEVICE_TO_FILTER_ACCEPT_LIST_REQ,
	CMD_LE_ADD_DEVICE_TO_FILTER_ACCEPT_LIST_RSP,
	CMD_LE_REMOVE_DEVICE_FROM_FILTER_ACCEPT_LIST_REQ,
	CMD_LE_REMOVE_DEVICE_FROM_FILTER_ACCEPT_LIST_RSP,
	CMD_LE_CONNECTION_UPDATE_REQ,
	CMD_LE_CONNECTION_UPDATE_RSP,
	CMD_LE_SET_HOST_CHANNEL_CLASSIFICATION_REQ,
	CMD_LE_SET_HOST_CHANNEL_CLASSIFICATION_RSP,
	CMD_LE_READ_CHANNEL_MAP_REQ,
	CMD_LE_READ_CHANNEL_MAP_RSP,
	CMD_LE_READ_REMOTE_FEATURES_REQ,
	CMD_LE_READ_REMOTE_FEATURES_RSP,
	CMD_LE_ENCRYPT_REQ,
	CMD_LE_ENCRYPT_RSP,
	CMD_LE_RAND_REQ,
	CMD_LE_RAND_RSP,
	CMD_LE_START_ENCRYPTION_REQ,
	CMD_LE_START_ENCRYPTION_RSP,
	CMD_LE_LONG_TERM_KEY_REQUEST_REPLY_REQ,
	CMD_LE_LONG_TERM_KEY_REQUEST_REPLY_RSP,
	CMD_LE_LONG_TERM_KEY_REQUEST_NEGATIVE_REPLY_REQ,
	CMD_LE_LONG_TERM_KEY_REQUEST_NEGATIVE_REPLY_RSP,
	CMD_LE_READ_SUPPORTED_STATES_REQ,
	CMD_LE_READ_SUPPORTED_STATES_RSP,
	CMD_LE_RECEIVER_TEST_REQ,
	CMD_LE_RECEIVER_TEST_RSP,
	CMD_LE_TRANSMITTER_TEST_REQ,
	CMD_LE_TRANSMITTER_TEST_RSP,
	CMD_LE_TEST_END_REQ,
	CMD_LE_TEST_END_RSP,
	CMD_LE_REMOTE_CONNECTION_PARAMETER_REQUEST_REPLY_REQ,
	CMD_LE_REMOTE_CONNECTION_PARAMETER_REQUEST_REPLY_RSP,
	CMD_LE_REMOTE_CONNECTION_PARAMETER_REQUEST_NEGATIVE_REPLY_REQ,
	CMD_LE_REMOTE_CONNECTION_PARAMETER_REQUEST_NEGATIVE_REPLY_RSP,
	CMD_LE_SET_DATA_LENGTH_REQ,
	CMD_LE_SET_DATA_LENGTH_RSP,
	CMD_LE_READ_SUGGESTED_DEFAULT_DATA_LENGTH_REQ,
	CMD_LE_READ_SUGGESTED_DEFAULT_DATA_LENGTH_RSP,
	CMD_LE_WRITE_SUGGESTED_DEFAULT_DATA_LENGTH_REQ,
	CMD_LE_WRITE_SUGGESTED_DEFAULT_DATA_LENGTH_RSP,
	CMD_LE_READ_LOCAL_P_256_PUBLIC_KEY_COMMAND_REQ,
	CMD_LE_READ_LOCAL_P_256_PUBLIC_KEY_COMMAND_RSP,
	CMD_LE_GENERATE_DHKEY_COMMAND_REQ,
	CMD_LE_GENERATE_DHKEY_COMMAND_RSP,
	CMD_LE_ADD_DEVICE_TO_RESOLVING_LIST_REQ,
	CMD_LE_ADD_DEVICE_TO_RESOLVING_LIST_RSP,
	CMD_LE_REMOVE_DEVICE_FROM_RESOLVING_LIST_REQ,
	CMD_LE_REMOVE_DEVICE_FROM_RESOLVING_LIST_RSP,
	CMD_LE_CLEAR_RESOLVING_LIST_REQ,
	CMD_LE_CLEAR_RESOLVING_LIST_RSP,
	CMD_LE_READ_RESOLVING_LIST_SIZE_REQ,
	CMD_LE_READ_RESOLVING_LIST_SIZE_RSP,
	CMD_LE_READ_PEER_RESOLVABLE_ADDRESS_REQ,
	CMD_LE_READ_PEER_RESOLVABLE_ADDRESS_RSP,
	CMD_LE_READ_LOCAL_RESOLVABLE_ADDRESS_REQ,
	CMD_LE_READ_LOCAL_RESOLVABLE_ADDRESS_RSP,
	CMD_LE_SET_ADDRESS_RESOLUTION_ENABLE_REQ,
	CMD_LE_SET_ADDRESS_RESOLUTION_ENABLE_RSP,
	CMD_LE_SET_RESOLVABLE_PRIVATE_ADDRESS_TIMEOUT_REQ,
	CMD_LE_SET_RESOLVABLE_PRIVATE_ADDRESS_TIMEOUT_RSP,
	CMD_LE_READ_MAXIMUM_DATA_LENGTH_REQ,
	CMD_LE_READ_MAXIMUM_DATA_LENGTH_RSP,
	CMD_LE_READ_PHY_REQ,
	CMD_LE_READ_PHY_RSP,
	CMD_LE_SET_DEFAULT_PHY_REQ,
	CMD_LE_SET_DEFAULT_PHY_RSP,
	CMD_LE_SET_PHY_REQ,
	CMD_LE_SET_PHY_RSP,
	CMD_LE_ENHANCED_RECEIVER_TEST_REQ,
	CMD_LE_ENHANCED_RECEIVER_TEST_RSP,
	CMD_LE_ENHANCED_TRANSMITTER_TEST_REQ,
	CMD_LE_ENHANCED_TRANSMITTER_TEST_RSP,
	CMD_LE_SET_EXTENDED_ADVERTISING_PARAMETERS_REQ,
	CMD_LE_SET_EXTENDED_ADVERTISING_PARAMETERS_RSP,
	CMD_LE_SET_EXTENDED_ADVERTISING_DATA_REQ,
	CMD_LE_SET_EXTENDED_ADVERTISING_DATA_RSP,
	CMD_LE_SET_EXTENDED_SCAN_RESPONSE_DATA_REQ,
	CMD_LE_SET_EXTENDED_SCAN_RESPONSE_DATA_RSP,
	CMD_LE_SET_EXTENDED_ADVERTISING_ENABLE_REQ,
	CMD_LE_SET_EXTENDED_ADVERTISING_ENABLE_RSP,
	CMD_LE_READ_MAXIMUM_ADVERTISING_DATA_LENGTH_REQ,
	CMD_LE_READ_MAXIMUM_ADVERTISING_DATA_LENGTH_RSP,
	CMD_LE_READ_NUMBER_OF_SUPPORTED_ADVERTISING_SETS_REQ,
	CMD_LE_READ_NUMBER_OF_SUPPORTED_ADVERTISING_SETS_RSP,
	CMD_LE_REMOVE_ADVERTISING_SET_REQ,
	CMD_LE_REMOVE_ADVERTISING_SET_RSP,
	CMD_LE_CLEAR_ADVERTISING_SETS_REQ,
	CMD_LE_CLEAR_ADVERTISING_SETS_RSP,
	CMD_LE_SET_PERIODIC_ADVERTISING_PARAMETERS_REQ,
	CMD_LE_SET_PERIODIC_ADVERTISING_PARAMETERS_RSP,
	CMD_LE_SET_PERIODIC_ADVERTISING_DATA_REQ,
	CMD_LE_SET_PERIODIC_ADVERTISING_DATA_RSP,
	CMD_LE_SET_PERIODIC_ADVERTISING_ENABLE_REQ,
	CMD_LE_SET_PERIODIC_ADVERTISING_ENABLE_RSP,
	CMD_LE_SET_EXTENDED_SCAN_PARAMETERS_REQ,
	CMD_LE_SET_EXTENDED_SCAN_PARAMETERS_RSP,
	CMD_LE_SET_EXTENDED_SCAN_ENABLE_REQ,
	CMD_LE_SET_EXTENDED_SCAN_ENABLE_RSP,
	CMD_LE_EXTENDED_CREATE_CONNECTION_REQ,
	CMD_LE_EXTENDED_CREATE_CONNECTION_RSP,
	CMD_LE_PERIODIC_ADVERTISING_CREATE_SYNC_REQ,
	CMD_LE_PERIODIC_ADVERTISING_CREATE_SYNC_RSP,
	CMD_LE_PERIODIC_ADVERTISING_CREATE_SYNC_CANCEL_REQ,
	CMD_LE_PERIODIC_ADVERTISING_CREATE_SYNC_CANCEL_RSP,
	CMD_LE_PERIODIC_ADVERTISING_TERMINATE_SYNC_REQ,
	CMD_LE_PERIODIC_ADVERTISING_TERMINATE_SYNC_RSP,
	CMD_LE_ADD_DEVICE_TO_PERIODIC_ADVERTISER_LIST_REQ,
	CMD_LE_ADD_DEVICE_TO_PERIODIC_ADVERTISER_LIST_RSP,
	CMD_LE_REMOVE_DEVICE_FROM_PERIODIC_ADVERTISER_LIST_REQ,
	CMD_LE_REMOVE_DEVICE_FROM_PERIODIC_ADVERTISER_LIST_RSP,
	CMD_LE_CLEAR_PERIODIC_ADVERTISER_LIST_REQ,
	CMD_LE_CLEAR_PERIODIC_ADVERTISER_LIST_RSP,
	CMD_LE_READ_PERIODIC_ADVERTISER_LIST_SIZE_REQ,
	CMD_LE_READ_PERIODIC_ADVERTISER_LIST_SIZE_RSP,
	CMD_LE_READ_TRANSMIT_POWER_REQ,
	CMD_LE_READ_TRANSMIT_POWER_RSP,
	CMD_LE_READ_RF_PATH_COMPENSATION_REQ,
	CMD_LE_READ_RF_PATH_COMPENSATION_RSP,
	CMD_LE_WRITE_RF_PATH_COMPENSATION_REQ,
	CMD_LE_WRITE_RF_PATH_COMPENSATION_RSP,
	CMD_LE_SET_PRIVACY_MODE_REQ,
	CMD_LE_SET_PRIVACY_MODE_RSP,
	CMD_WRITE_BD_ADDR_REQ,
	CMD_WRITE_BD_ADDR_RSP,
	CMD_FLUSH_EVENTS_REQ,
	CMD_FLUSH_EVENTS_RSP,
	CMD_HAS_EVENT_REQ,
	CMD_HAS_EVENT_RSP,
	CMD_GET_EVENT_REQ,
	CMD_GET_EVENT_RSP,
	CMD_LE_FLUSH_DATA_REQ,
	CMD_LE_FLUSH_DATA_RSP,
	CMD_LE_DATA_READY_REQ,
	CMD_LE_DATA_READY_RSP,
	CMD_LE_DATA_WRITE_REQ,
	CMD_LE_DATA_WRITE_RSP,
	CMD_LE_DATA_READ_REQ,
	CMD_LE_DATA_READ_RSP,
	CMD_GATT_SERVICE_SET_REQ,
	CMD_GATT_SERVICE_SET_RSP,
	CMD_GATT_SERVICE_NOTIFY_REQ,
	CMD_GATT_SERVICE_NOTIFY_RSP,
	CMD_GATT_SERVICE_INDICATE_REQ,
	CMD_GATT_SERVICE_INDICATE_RSP,
	CMD_GAP_ADVERTISING_MODE_REQ,
	CMD_GAP_ADVERTISING_MODE_RSP,
	CMD_GAP_ADVERTISING_DATA_REQ,
	CMD_GAP_ADVERTISING_DATA_RSP,
	CMD_GAP_SCANNING_MODE_REQ,
	CMD_GAP_SCANNING_MODE_RSP,
	CMD_READ_STATIC_ADDRESSES_REQ,
	CMD_READ_STATIC_ADDRESSES_RSP,
	CMD_READ_KEY_HIERARCHY_ROOTS_REQ,
	CMD_READ_KEY_HIERARCHY_ROOTS_RSP,
	CMD_GAP_READ_IRK_REQ,
	CMD_GAP_READ_IRK_RSP,
	CMD_GAP_ROLE_REQ,
	CMD_GAP_ROLE_RSP,
	CMD_LE_FLUSH_ISO_DATA_REQ,
	CMD_LE_FLUSH_ISO_DATA_RSP,
	CMD_LE_ISO_DATA_READY_REQ,
	CMD_LE_ISO_DATA_READY_RSP,
	CMD_LE_ISO_DATA_WRITE_REQ,
	CMD_LE_ISO_DATA_WRITE_RSP,
	CMD_LE_ISO_DATA_READ_REQ,
	CMD_LE_ISO_DATA_READ_RSP,
	CMD_LE_SET_CIG_PARAMETERS_REQ,
	CMD_LE_SET_CIG_PARAMETERS_RSP,
	CMD_LE_SET_CIG_PARAMETERS_TEST_REQ,
	CMD_LE_SET_CIG_PARAMETERS_TEST_RSP,
	CMD_LE_CREATE_CIS_REQ,
	CMD_LE_CREATE_CIS_RSP,
	CMD_LE_REMOVE_CIG_REQ,
	CMD_LE_REMOVE_CIG_RSP,
	CMD_LE_ACCEPT_CIS_REQUEST_REQ,
	CMD_LE_ACCEPT_CIS_REQUEST_RSP,
	CMD_LE_REJECT_CIS_REQUEST_REQ,
	CMD_LE_REJECT_CIS_REQUEST_RSP,
	CMD_LE_SETUP_ISO_DATA_PATH_REQ,
	CMD_LE_SETUP_ISO_DATA_PATH_RSP,
	CMD_LE_REMOVE_ISO_DATA_PATH_REQ,
	CMD_LE_REMOVE_ISO_DATA_PATH_RSP,
	CMD_LE_SET_HOST_FEATURE_REQ,
	CMD_LE_SET_HOST_FEATURE_RSP,
	CMD_GET_IXIT_VALUE_REQ,
	CMD_GET_IXIT_VALUE_RSP,
	CMD_HCI_LE_ISO_TRANSMIT_TEST_REQ,
	CMD_HCI_LE_ISO_TRANSMIT_TEST_RSP,
	CMD_HCI_LE_ISO_RECEIVE_TEST_REQ,
	CMD_HCI_LE_ISO_RECEIVE_TEST_RSP,
	CMD_HCI_LE_ISO_READ_TEST_COUNTERS_REQ,
	CMD_HCI_LE_ISO_READ_TEST_COUNTERS_RSP,
	CMD_HCI_LE_ISO_TEST_END_REQ,
	CMD_HCI_LE_ISO_TEST_END_RSP
};

enum profile_t {
	PROFILE_ID_GAP,
	PROFILE_ID_GATT,
	PROFILE_ID_HCI,
	PROFILE_ID_L2CAP,
	PROFILE_ID_LL,
	PROFILE_ID_SM,
};

#endif /* EDDT_APP_COMMANDS_H */
