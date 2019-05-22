/*
 * Copyright (c) 2017 Linaro Limited
 * Copyright (c) 2017-2019 Foundries.io
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/** @file lwm2m.h
 *
 * @defgroup lwm2m_api LwM2M high-level API
 * @ingroup networking
 * @{
 * @brief LwM2M high-level API
 *
 * @details
 * LwM2M high-level interface is defined in this header.
 *
 * @note The implementation assumes UDP module is enabled.
 *
 * @note LwM2M 1.0.x is currently the only supported version.
 */

#ifndef ZEPHYR_INCLUDE_NET_LWM2M_H_
#define ZEPHYR_INCLUDE_NET_LWM2M_H_

#include <kernel.h>
#include <net/coap.h>

/**
 * @brief LwM2M Objects managed by OMA for LwM2M tech specification.  Objects
 * in this range have IDs from 0 to 1023.
 * For more information refer to Technical Specification
 * OMA-TS-LightweightM2M-V1_0_2-20180209-A
 */

#define LWM2M_OBJECT_SECURITY_ID			0
#define LWM2M_OBJECT_SERVER_ID				1
#define LWM2M_OBJECT_ACCESS_CONTROL_ID			2
#define LWM2M_OBJECT_DEVICE_ID				3
#define LWM2M_OBJECT_CONNECTIVITY_MONITORING_ID		4
#define LWM2M_OBJECT_FIRMWARE_ID			5
#define LWM2M_OBJECT_LOCATION_ID			6
#define LWM2M_OBJECT_CONNECTIVITY_STATISTICS_ID		7

/**
 * @brief LwM2M Objects produced by 3rd party Standards Development
 * Organizations.  Objects in this range have IDs from 2048 to 10240
 * Refer to the OMA LightweightM2M (LwM2M) Object and Resource Registry:
 * http://www.openmobilealliance.org/wp/OMNA/LwM2M/LwM2MRegistry.html
 */

#define IPSO_OBJECT_TEMP_SENSOR_ID			3303
#define IPSO_OBJECT_LIGHT_CONTROL_ID			3311
#define IPSO_OBJECT_TIMER_ID				3340

/**
 * @brief LwM2M context structure to maintain information for a single
 * LwM2M connection.
 */
struct lwm2m_ctx {
	/** Destination address storage */
	struct sockaddr remote_addr;

	/** Private CoAP and networking structures */
	struct coap_pending pendings[CONFIG_LWM2M_ENGINE_MAX_PENDING];
	struct coap_reply replies[CONFIG_LWM2M_ENGINE_MAX_REPLIES];
	struct k_delayed_work retransmit_work;

#if defined(CONFIG_LWM2M_DTLS_SUPPORT)
	/** TLS tag is set by client as a reference used when the
	 *  LwM2M engine calls tls_credential_(add|delete)
	 */
	int tls_tag;
#endif
	/** Flag to indicate if context should use DTLS.
	 *  Enabled via the use of coaps:// protocol prefix in connection
	 *  information.
	 *  NOTE: requires CONFIG_LWM2M_DTLS_SUPPORT=y
	 */
	bool use_dtls;

	/** Current index of Security Object used for server credentials */
	int sec_obj_inst;

	/** Flag to enable BOOTSTRAP interface.  See Section 5.2
	 *  "Bootstrap Interface" of LwM2M Technical Specification 1.0.2
	 *  for more information.
	 */
	bool bootstrap_mode;

	/** This flag enables the context to handle an initial ACK after
	 *  requesting a block of data, but a follow-up packet will contain
	 *  actual data block.
	 *  NOTE: This is required for CoAP proxy use-case.
	 */
	bool handle_separate_response;

	/** Socket File Descriptor */
	int sock_fd;
};


/**
 * @brief Asynchronous callback to get a resource buffer and length.
 *
 * Prior to accessing the data buffer of a resource, the engine can
 * use this callback to get the buffer pointer and length instead
 * of using the resource's data buffer.
 *
 * The client or LwM2M objects can register a function of this type via:
 * lwm2m_engine_register_read_callback()
 * lwm2m_engine_register_pre_write_callback()
 *
 * @param[in] obj_inst_id Object instance ID generating the callback.
 * @param[out] data_len Length of the data buffer.
 *
 * @return Callback returns a pointer to the data buffer or NULL for failure.
 */
typedef void *(*lwm2m_engine_get_data_cb_t)(u16_t obj_inst_id,
				       size_t *data_len);

/**
 * @brief Asynchronous callback when data has been set to a resource buffer.
 *
 * After changing the data of a resource buffer, the LwM2M engine can
 * make use of this callback to pass the data back to the client or LwM2M
 * objects.
 *
 * A function of this type can be registered via:
 * lwm2m_engine_register_post_write_callback()
 *
 * @param[in] obj_inst_id Object instance ID generating the callback.
 * @param[in] data Pointer to data.
 * @param[in] data_len Length of the data.
 * @param[in] last_block Flag used during block transfer to indicate the last
 *                       block of data. For non-block transfers this is always
 *                       false.
 * @param[in] total_size Expected total size of data for a block transfer.
 *                       For non-block transfers this is 0.
 *
 * @return Callback returns a negative error code (errno.h) indicating
 *         reason of failure or 0 for success.
 */
typedef int (*lwm2m_engine_set_data_cb_t)(u16_t obj_inst_id,
				       u8_t *data, u16_t data_len,
				       bool last_block, size_t total_size);

/**
 * @brief Asynchronous event notification callback.
 *
 * Various object instance and resource-based events in the LwM2M engine
 * can trigger a callback of this function type: object instance create,
 * object instance delete and resource execute.
 *
 * Register a function of this type via:
 * lwm2m_engine_register_exec_callback()
 * lwm2m_engine_register_create_callback()
 * lwm2m_engine_register_delete_callback()
 *
 * @param[in] obj_inst_id Object instance ID generating the callback.
 *
 * @return Callback returns a negative error code (errno.h) indicating
 *         reason of failure or 0 for success.
 */
typedef int (*lwm2m_engine_user_cb_t)(u16_t obj_inst_id);

/**
 * @brief Power source types used for the "Available Power Sources" resource of
 * the LwM2M Device object.  An LwM2M client can use one of the following
 * codes to register a power source using lwm2m_device_add_pwrsrc().
 */
#define LWM2M_DEVICE_PWR_SRC_TYPE_DC_POWER	0
#define LWM2M_DEVICE_PWR_SRC_TYPE_BAT_INT	1
#define LWM2M_DEVICE_PWR_SRC_TYPE_BAT_EXT	2
#define LWM2M_DEVICE_PWR_SRC_TYPE_UNUSED	3
#define LWM2M_DEVICE_PWR_SRC_TYPE_PWR_OVER_ETH	4
#define LWM2M_DEVICE_PWR_SRC_TYPE_USB		5
#define LWM2M_DEVICE_PWR_SRC_TYPE_AC_POWER	6
#define LWM2M_DEVICE_PWR_SRC_TYPE_SOLAR		7
#define LWM2M_DEVICE_PWR_SRC_TYPE_MAX		8

/**
 * @brief Error codes used for the "Error Code" resource of the LwM2M Device
 * object.  An LwM2M client can register one of the following error codes via
 * the lwm2m_device_add_err() function.
 */
#define LWM2M_DEVICE_ERROR_NONE			0
#define LWM2M_DEVICE_ERROR_LOW_POWER		1
#define LWM2M_DEVICE_ERROR_EXT_POWER_SUPPLY_OFF	2
#define LWM2M_DEVICE_ERROR_GPS_FAILURE		3
#define LWM2M_DEVICE_ERROR_LOW_SIGNAL_STRENGTH	4
#define LWM2M_DEVICE_ERROR_OUT_OF_MEMORY	5
#define LWM2M_DEVICE_ERROR_SMS_FAILURE		6
#define LWM2M_DEVICE_ERROR_NETWORK_FAILURE	7
#define LWM2M_DEVICE_ERROR_PERIPHERAL_FAILURE	8

/**
 * @brief Battery status codes used for the "Battery Status" resource (3/0/20)
 *        of the LwM2M Device object.  As the battery status changes, an LwM2M
 *        client can set one of the following codes via:
 *        lwm2m_engine_set_u8("3/0/20", [battery status])
 */
#define LWM2M_DEVICE_BATTERY_STATUS_NORMAL	0
#define LWM2M_DEVICE_BATTERY_STATUS_CHARGING	1
#define LWM2M_DEVICE_BATTERY_STATUS_CHARGE_COMP	2
#define LWM2M_DEVICE_BATTERY_STATUS_DAMAGED	3
#define LWM2M_DEVICE_BATTERY_STATUS_LOW		4
#define LWM2M_DEVICE_BATTERY_STATUS_NOT_INST	5
#define LWM2M_DEVICE_BATTERY_STATUS_UNKNOWN	6

/**
 * @brief Register a power source with the LwM2M Device object.
 *
 * @param[in] pwr_src_type Power source type code.
 *
 * @return The newly added index of the power source.  The index is used
 *         for removing the power source, setting voltage or setting current.
 */
int lwm2m_device_add_pwrsrc(u8_t pwr_src_type); /* returns index */

/**
 * @brief Remove power source previously registered in the LwM2M Device object.
 *
 * @param[in] index Index of the power source returned by
 *                  lwm2m_device_add_pwrsrc().
 *
 * @return 0 for success or negative in case of error.
 */
int lwm2m_device_remove_pwrsrc(int index);

/**
 * @brief Set power source voltage (in millivolts).
 *
 * @param[in] index Index of the power source returned by
 *                  lwm2m_device_add_pwrsrc().
 * @param[in] voltage_mv New voltage in millivolts.
 *
 * @return 0 for success or negative in case of error.
 */
int lwm2m_device_set_pwrsrc_voltage_mv(int index, int voltage_mv);

/**
 * @brief Set power source current (in milliamps).
 *
 * @param[in] index Index of the power source returned by
 *                  lwm2m_device_add_pwrsrc().
 * @param[in] current_ma New current value milliamps.
 *
 * @return 0 for success or negative in case of error.
 */
int lwm2m_device_set_pwrsrc_current_ma(int index, int current_ma);

/**
 * @brief Register a new error code with LwM2M Device object.
 *
 * @param[in] error_code New error code.
 *
 * @return 0 for success or negative in case of error.
 */
int lwm2m_device_add_err(u8_t error_code);


/**
 * @brief LWM2M Firmware Update object states
 *
 * An LwM2M client or the LwM2M Firmware Update object use the following codes
 * to represent the LwM2M Firmware Update state (5/0/3).
 */
#define STATE_IDLE		0
#define STATE_DOWNLOADING	1
#define STATE_DOWNLOADED	2
#define STATE_UPDATING		3

/**
 * @brief LWM2M Firmware Update object result codes
 *
 * After processing a firmware update, the client sets the result via one of
 * the following codes via lwm2m_engine_set_u8("5/0/5", [result code])
 */
#define RESULT_DEFAULT		0
#define RESULT_SUCCESS		1
#define RESULT_NO_STORAGE	2
#define RESULT_OUT_OF_MEM	3
#define RESULT_CONNECTION_LOST	4
#define RESULT_INTEGRITY_FAILED	5
#define RESULT_UNSUP_FW		6
#define RESULT_INVALID_URI	7
#define RESULT_UPDATE_FAILED	8
#define RESULT_UNSUP_PROTO	9

#if defined(CONFIG_LWM2M_FIRMWARE_UPDATE_OBJ_SUPPORT)
/**
 * @brief Set data callback for firmware block transfer.
 *
 * LwM2M clients use this function to register a callback for receiving the
 * block transfer data when performing a firmware update.
 *
 * @param[in] cb A callback function to receive the block transfer data
 */
void lwm2m_firmware_set_write_cb(lwm2m_engine_set_data_cb_t cb);

/**
 * @brief Get the data callback for firmware block transfer writes.
 *
 * @return A registered callback function to receive the block transfer data
 */
lwm2m_engine_set_data_cb_t lwm2m_firmware_get_write_cb(void);

#if defined(CONFIG_LWM2M_FIRMWARE_UPDATE_PULL_SUPPORT)
/**
 * @brief Set data callback to handle firmware update execute events.
 *
 * LwM2M clients use this function to register a callback for receiving the
 * update resource "execute" operation on the LwM2M Firmware Update object.
 *
 * @param[in] cb A callback function to receive the execute event.
 */
void lwm2m_firmware_set_update_cb(lwm2m_engine_user_cb_t cb);

/**
 * @brief Get the event callback for firmware update execute events.
 *
 * @return A registered callback function to receive the execute event.
 */
lwm2m_engine_user_cb_t lwm2m_firmware_get_update_cb(void);
#endif
#endif

/**
 * @brief Data structure used to represent the LwM2M float type:
 * val1 is the whole number portion of the decimal
 * val2 is the decimal portion *1000000 for 32bit, *1000000000 for 64bit
 * Example: 123.456 == val1: 123, val2:456000
 * Example: 123.000456 = val1: 123, val2:456
 */

/**
 * @brief Maximum precision value for 32-bit LwM2M float val2
 */
#define LWM2M_FLOAT32_DEC_MAX 1000000

/**
 * @brief 32-bit variant of the LwM2M float structure
 */
typedef struct float32_value {
	s32_t val1;
	s32_t val2;
} float32_value_t;

/**
 * @brief Maximum precision value for 64-bit LwM2M float val2
 */
#define LWM2M_FLOAT64_DEC_MAX 1000000000LL

/**
 * @brief 32-bit variant of the LwM2M float structure
 */
typedef struct float64_value {
	s64_t val1;
	s64_t val2;
} float64_value_t;

/**
 * @brief Create an LwM2M object instance.
 *
 * LwM2M clients use this function to create non-default LwM2M objects:
 * Example to create first temperature sensor object:
 * lwm2m_engine_create_obj_inst("3303/0");
 *
 * @param[in] pathstr LwM2M object instance path string (obj/obj-instance)
 *
 * @return 0 for success or negative in case of error.
 */
int lwm2m_engine_create_obj_inst(char *pathstr);

/**
 * @brief Set resource value (opaque buffer)
 *
 * @param[in] pathstr LwM2M resource path string
 *                    (obj/obj-instance/resource)
 * @param[in] data_ptr Data buffer
 * @param[in] data_len Length of buffer
 *
 * @return 0 for success or negative in case of error.
 */
int lwm2m_engine_set_opaque(char *pathstr, char *data_ptr, u16_t data_len);

/**
 * @brief Set resource value (string)
 *
 * @param[in] path LwM2M resource path string (obj/obj-instance/resource)
 * @param[in] data_ptr NULL terminated char buffer
 *
 * @return 0 for success or negative in case of error.
 */
int lwm2m_engine_set_string(char *path, char *data_ptr);

/**
 * @brief Set resource value (u8)
 *
 * @param[in] path LwM2M resource path string (obj/obj-instance/resource)
 * @param[in] value u8 value
 *
 * @return 0 for success or negative in case of error.
 */
int lwm2m_engine_set_u8(char *path, u8_t value);

/**
 * @brief Set resource value (u16)
 *
 * @param[in] path LwM2M resource path string (obj/obj-instance/resource)
 * @param[in] value u16 value
 *
 * @return 0 for success or negative in case of error.
 */
int lwm2m_engine_set_u16(char *path, u16_t value);

/**
 * @brief Set resource value (u32)
 *
 * @param[in] path LwM2M resource path string (obj/obj-instance/resource)
 * @param[in] value u32 value
 *
 * @return 0 for success or negative in case of error.
 */
int lwm2m_engine_set_u32(char *path, u32_t value);

/**
 * @brief Set resource value (u64)
 *
 * @param[in] path LwM2M resource path string (obj/obj-instance/resource)
 * @param[in] value u64 value
 *
 * @return 0 for success or negative in case of error.
 */
int lwm2m_engine_set_u64(char *path, u64_t value);

/**
 * @brief Set resource value (s8)
 *
 * @param[in] path LwM2M resource path string (obj/obj-instance/resource)
 * @param[in] value s8 value
 *
 * @return 0 for success or negative in case of error.
 */
int lwm2m_engine_set_s8(char *path, s8_t value);

/**
 * @brief Set resource value (s16)
 *
 * @param[in] path LwM2M resource path string (obj/obj-instance/resource)
 * @param[in] value s16 value
 *
 * @return 0 for success or negative in case of error.
 */
int lwm2m_engine_set_s16(char *path, s16_t value);

/**
 * @brief Set resource value (s32)
 *
 * @param[in] path LwM2M resource path string (obj/obj-instance/resource)
 * @param[in] value s32 value
 *
 * @return 0 for success or negative in case of error.
 */
int lwm2m_engine_set_s32(char *path, s32_t value);

/**
 * @brief Set resource value (s64)
 *
 * @param[in] path LwM2M resource path string (obj/obj-instance/resource)
 * @param[in] value s64 value
 *
 * @return 0 for success or negative in case of error.
 */
int lwm2m_engine_set_s64(char *path, s64_t value);

/**
 * @brief Set resource value (bool)
 *
 * @param[in] path LwM2M resource path string (obj/obj-instance/resource)
 * @param[in] value bool value
 *
 * @return 0 for success or negative in case of error.
 */
int lwm2m_engine_set_bool(char *path, bool value);

/**
 * @brief Set resource value (32-bit float structure)
 *
 * @param[in] pathstr LwM2M resource path string (obj/obj-instance/resource)
 * @param[in] value 32-bit float value
 *
 * @return 0 for success or negative in case of error.
 */
int lwm2m_engine_set_float32(char *pathstr, float32_value_t *value);

/**
 * @brief Set resource value (64-bit float structure)
 *
 * @param[in] pathstr LwM2M resource path string (obj/obj-instance/resource)
 * @param[in] value 64-bit float value
 *
 * @return 0 for success or negative in case of error.
 */
int lwm2m_engine_set_float64(char *pathstr, float64_value_t *value);

/**
 * @brief Get resource value (opaque buffer)
 *
 * @param[in] pathstr LwM2M resource path string (obj/obj-instance/resource)
 * @param[out] buf Data buffer to copy data into
 * @param[in] buflen Length of buffer
 *
 * @return 0 for success or negative in case of error.
 */
int lwm2m_engine_get_opaque(char *pathstr, void *buf, u16_t buflen);

/**
 * @brief Get resource value (string)
 *
 * @param[in] path LwM2M resource path string (obj/obj-instance/resource)
 * @param[out] str String buffer to copy data into
 * @param[in] strlen Length of buffer
 *
 * @return 0 for success or negative in case of error.
 */
int lwm2m_engine_get_string(char *path, void *str, u16_t strlen);

/**
 * @brief Get resource value (u8)
 *
 * @param[in] path LwM2M resource path string (obj/obj-instance/resource)
 * @param[out] value u8 buffer to copy data into
 *
 * @return 0 for success or negative in case of error.
 */
int lwm2m_engine_get_u8(char *path, u8_t *value);

/**
 * @brief Get resource value (u16)
 *
 * @param[in] path LwM2M resource path string (obj/obj-instance/resource)
 * @param[out] value u16 buffer to copy data into
 *
 * @return 0 for success or negative in case of error.
 */
int lwm2m_engine_get_u16(char *path, u16_t *value);

/**
 * @brief Get resource value (u32)
 *
 * @param[in] path LwM2M resource path string (obj/obj-instance/resource)
 * @param[out] value u32 buffer to copy data into
 *
 * @return 0 for success or negative in case of error.
 */
int lwm2m_engine_get_u32(char *path, u32_t *value);

/**
 * @brief Get resource value (u64)
 *
 * @param[in] path LwM2M resource path string (obj/obj-instance/resource)
 * @param[out] value u64 buffer to copy data into
 *
 * @return 0 for success or negative in case of error.
 */
int lwm2m_engine_get_u64(char *path, u64_t *value);

/**
 * @brief Get resource value (s8)
 *
 * @param[in] path LwM2M resource path string (obj/obj-instance/resource)
 * @param[out] value s8 buffer to copy data into
 *
 * @return 0 for success or negative in case of error.
 */
int lwm2m_engine_get_s8(char *path, s8_t *value);

/**
 * @brief Get resource value (s16)
 *
 * @param[in] path LwM2M resource path string (obj/obj-instance/resource)
 * @param[out] value s16 buffer to copy data into
 *
 * @return 0 for success or negative in case of error.
 */
int lwm2m_engine_get_s16(char *path, s16_t *value);

/**
 * @brief Get resource value (s32)
 *
 * @param[in] path LwM2M resource path string (obj/obj-instance/resource)
 * @param[out] value s32 buffer to copy data into
 *
 * @return 0 for success or negative in case of error.
 */
int lwm2m_engine_get_s32(char *path, s32_t *value);

/**
 * @brief Get resource value (s64)
 *
 * @param[in] path LwM2M resource path string (obj/obj-instance/resource)
 * @param[out] value s64 buffer to copy data into
 *
 * @return 0 for success or negative in case of error.
 */
int lwm2m_engine_get_s64(char *path, s64_t *value);

/**
 * @brief Get resource value (bool)
 *
 * @param[in] path LwM2M resource path string (obj/obj-instance/resource)
 * @param[out] value bool buffer to copy data into
 *
 * @return 0 for success or negative in case of error.
 */
int lwm2m_engine_get_bool(char *path, bool *value);

/**
 * @brief Get resource value (32-bit float structure)
 *
 * @param[in] pathstr LwM2M resource path string (obj/obj-instance/resource)
 * @param[out] buf 32-bit float buffer to copy data into
 *
 * @return 0 for success or negative in case of error.
 */
int lwm2m_engine_get_float32(char *pathstr, float32_value_t *buf);

/**
 * @brief Get resource value (64-bit float structure)
 *
 * @param[in] pathstr LwM2M resource path string (obj/obj-instance/resource)
 * @param[out] buf 64-bit float buffer to copy data into
 *
 * @return 0 for success or negative in case of error.
 */
int lwm2m_engine_get_float64(char *pathstr, float64_value_t *buf);

/**
 * @brief Set resource read callback
 *
 * LwM2M clients can use this to set the callback function for resource reads.
 *
 * @param[in] path LwM2M resource path string (obj/obj-instance/resource)
 * @param[in] cb Read resource callback
 *
 * @return 0 for success or negative in case of error.
 */
int lwm2m_engine_register_read_callback(char *path,
					lwm2m_engine_get_data_cb_t cb);

/**
 * @brief Set resource pre-write callback
 *
 * This callback is triggered before setting the value of a resource.  It
 * can pass a special data buffer to the engine so that the actual resource
 * value can be calculated later, etc.
 *
 * @param[in] path LwM2M resource path string (obj/obj-instance/resource)
 * @param[in] cb Pre-write resource callback
 *
 * @return 0 for success or negative in case of error.
 */
int lwm2m_engine_register_pre_write_callback(char *path,
					     lwm2m_engine_get_data_cb_t cb);

/**
 * @brief Set resource post-write callback
 *
 * This callback is triggered after setting the value of a resource to the
 * resource data buffer.
 *
 * It allows an LwM2M client or object to post-process the value of a resource
 * or trigger other related resource calculations.
 *
 * @param[in] path LwM2M resource path string (obj/obj-instance/resource)
 * @param[in] cb Post-write resource callback
 *
 * @return 0 for success or negative in case of error.
 */
int lwm2m_engine_register_post_write_callback(char *path,
					      lwm2m_engine_set_data_cb_t cb);

/**
 * @brief Set resource execute event callback
 *
 * This event is triggered when the execute method of a resource is enabled.
 *
 * @param[in] path LwM2M resource path string (obj/obj-instance/resource)
 * @param[in] cb Execute resource callback
 *
 * @return 0 for success or negative in case of error.
 */
int lwm2m_engine_register_exec_callback(char *path,
					lwm2m_engine_user_cb_t cb);

/**
 * @brief Set object instance create event callback
 *
 * This event is triggered when an object instance is created.
 *
 * @param[in] obj_id LwM2M object id
 * @param[in] cb Create object instance callback
 *
 * @return 0 for success or negative in case of error.
 */
int lwm2m_engine_register_create_callback(u16_t obj_id,
					  lwm2m_engine_user_cb_t cb);

/**
 * @brief Set object instance delete event callback
 *
 * This event is triggered when an object instance is deleted.
 *
 * @param[in] obj_id LwM2M object id
 * @param[in] cb Delete object instance callback
 *
 * @return 0 for success or negative in case of error.
 */
int lwm2m_engine_register_delete_callback(u16_t obj_id,
					  lwm2m_engine_user_cb_t cb);

/**
 * @brief Resource read-only value bit
 */
#define LWM2M_RES_DATA_READ_ONLY	0

/**
 * @brief Resource read-only flag
 */
#define LWM2M_RES_DATA_FLAG_RO		BIT(LWM2M_RES_DATA_READ_ONLY)

/**
 * @brief Read resource flags helper macro
 */
#define LWM2M_HAS_RES_FLAG(res, f)	((res->data_flags & f) == f)

/**
 * @brief Set data buffer for a resource
 *
 * Use this function to set the data buffer and flags for the specified LwM2M
 * resource.
 *
 * @param[in] pathstr LwM2M resource path string
 *            (obj/obj-instance/resource)
 * @param[in] data_ptr Data buffer pointer
 * @param[in] data_len Length of buffer
 * @param[in] data_flags Data buffer flags (such as read-only, etc)
 *
 * @return 0 for success or negative in case of error.
 */
int lwm2m_engine_set_res_data(char *pathstr, void *data_ptr, u16_t data_len,
			      u8_t data_flags);

/**
 * @brief Get data buffer for a resource
 *
 * Use this function to get the data buffer information for the specified LwM2M
 * resource.
 *
 * @param[in] pathstr LwM2M resource path string
 *            (obj/obj-instance/resource)
 * @param[out] data_ptr Data buffer pointer
 * @param[out] data_len Length of buffer
 * @param[out] data_flags Data buffer flags (such as read-only, etc)
 *
 * @return 0 for success or negative in case of error.
 */
int lwm2m_engine_get_res_data(char *pathstr, void **data_ptr, u16_t *data_len,
			      u8_t *data_flags);

/**
 * @brief Start the LwM2M engine
 *
 * LwM2M clients normally do not need to call this function as it is called
 * by lwm2m_rd_client_start().  However, if the client does not use the RD
 * client implementation, it will need to be called manually.
 *
 * @param[in] client_ctx LwM2M context
 *
 * @return 0 for success or negative in case of error.
 */
int lwm2m_engine_start(struct lwm2m_ctx *client_ctx);

/**
 * @brief LwM2M RD client events
 *
 * LwM2M client events are passed back to the event_cb function in
 * lwm2m_rd_client_start()
 */
enum lwm2m_rd_client_event {
	LWM2M_RD_CLIENT_EVENT_NONE,
	LWM2M_RD_CLIENT_EVENT_BOOTSTRAP_REG_FAILURE,
	LWM2M_RD_CLIENT_EVENT_BOOTSTRAP_REG_COMPLETE,
	LWM2M_RD_CLIENT_EVENT_BOOTSTRAP_TRANSFER_COMPLETE,
	LWM2M_RD_CLIENT_EVENT_REGISTRATION_FAILURE,
	LWM2M_RD_CLIENT_EVENT_REGISTRATION_COMPLETE,
	LWM2M_RD_CLIENT_EVENT_REG_UPDATE_FAILURE,
	LWM2M_RD_CLIENT_EVENT_REG_UPDATE_COMPLETE,
	LWM2M_RD_CLIENT_EVENT_DEREGISTER_FAILURE,
	LWM2M_RD_CLIENT_EVENT_DISCONNECT
};

/**
 * @brief Asynchronous RD client event callback
 *
 * @param[in] ctx LwM2M context generating the event
 * @param[in] event LwM2M RD client event code
 */
typedef void (*lwm2m_ctx_event_cb_t)(struct lwm2m_ctx *ctx,
				     enum lwm2m_rd_client_event event);

/**
 * @brief Start the LwM2M RD (Registration / Discovery) Client
 *
 * The RD client sits just above the LwM2M engine and performs the necessary
 * actions to implement the "Registration interface".
 * For more information see Section 5.3 "Client Registration Interface" of the
 * LwM2M Technical Specification.
 *
 * NOTE: lwm2m_engine_start() is called automatically by this function.
 *
 * @param[in] client_ctx LwM2M context
 * @param[in] ep_name Registered endpoint name
 * @param[in] event_cb Client event callback function
 *
 * @return 0 for success or negative in case of error.
 */
void lwm2m_rd_client_start(struct lwm2m_ctx *client_ctx, const char *ep_name,
			   lwm2m_ctx_event_cb_t event_cb);

#endif	/* ZEPHYR_INCLUDE_NET_LWM2M_H_ */

/**@}  */
