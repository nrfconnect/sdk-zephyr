/** @file
 *  @brief Bluetooth Generic Access Profile defines and Assigned Numbers.
 */

/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_INCLUDE_BLUETOOTH_GAP_H_
#define ZEPHYR_INCLUDE_BLUETOOTH_GAP_H_

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Bluetooth Generic Access Profile defines and Assigned Numbers.
 * @defgroup bt_gap_defines Defines and Assigned Numbers
 * @ingroup bluetooth
 * @{
 */

/** Company Identifiers (see Bluetooth Assigned Numbers) */
#define BT_COMP_ID_LF           0x05f1 /* The Linux Foundation */

/** EIR/AD data type definitions */
#define BT_DATA_FLAGS                   0x01 /* AD flags */
#define BT_DATA_UUID16_SOME             0x02 /* 16-bit UUID, more available */
#define BT_DATA_UUID16_ALL              0x03 /* 16-bit UUID, all listed */
#define BT_DATA_UUID32_SOME             0x04 /* 32-bit UUID, more available */
#define BT_DATA_UUID32_ALL              0x05 /* 32-bit UUID, all listed */
#define BT_DATA_UUID128_SOME            0x06 /* 128-bit UUID, more available */
#define BT_DATA_UUID128_ALL             0x07 /* 128-bit UUID, all listed */
#define BT_DATA_NAME_SHORTENED          0x08 /* Shortened name */
#define BT_DATA_NAME_COMPLETE           0x09 /* Complete name */
#define BT_DATA_TX_POWER                0x0a /* Tx Power */
#define BT_DATA_SM_TK_VALUE             0x10 /* Security Manager TK Value */
#define BT_DATA_SM_OOB_FLAGS            0x11 /* Security Manager OOB Flags */
#define BT_DATA_SOLICIT16               0x14 /* Solicit UUIDs, 16-bit */
#define BT_DATA_SOLICIT128              0x15 /* Solicit UUIDs, 128-bit */
#define BT_DATA_SVC_DATA16              0x16 /* Service data, 16-bit UUID */
#define BT_DATA_GAP_APPEARANCE          0x19 /* GAP appearance */
#define BT_DATA_LE_BT_DEVICE_ADDRESS    0x1b /* LE Bluetooth Device Address */
#define BT_DATA_LE_ROLE                 0x1c /* LE Role */
#define BT_DATA_SOLICIT32               0x1f /* Solicit UUIDs, 32-bit */
#define BT_DATA_SVC_DATA32              0x20 /* Service data, 32-bit UUID */
#define BT_DATA_SVC_DATA128             0x21 /* Service data, 128-bit UUID */
#define BT_DATA_LE_SC_CONFIRM_VALUE     0x22 /* LE SC Confirmation Value */
#define BT_DATA_LE_SC_RANDOM_VALUE      0x23 /* LE SC Random Value */
#define BT_DATA_URI                     0x24 /* URI */
#define BT_DATA_LE_SUPPORTED_FEATURES   0x27 /* LE Supported Features */
#define BT_DATA_CHANNEL_MAP_UPDATE_IND  0x28 /* Channel Map Update Indication */
#define BT_DATA_MESH_PROV               0x29 /* Mesh Provisioning PDU */
#define BT_DATA_MESH_MESSAGE            0x2a /* Mesh Networking PDU */
#define BT_DATA_MESH_BEACON             0x2b /* Mesh Beacon */
#define BT_DATA_BIG_INFO                0x2c /* BIGInfo */
#define BT_DATA_BROADCAST_CODE          0x2d /* Broadcast Code */
#define BT_DATA_CSIS_RSI                0x2e /* CSIS Random Set ID type */

#define BT_DATA_MANUFACTURER_DATA       0xff /* Manufacturer Specific Data */

#define BT_LE_AD_LIMITED                0x01 /* Limited Discoverable */
#define BT_LE_AD_GENERAL                0x02 /* General Discoverable */
#define BT_LE_AD_NO_BREDR               0x04 /* BR/EDR not supported */

/* Defined GAP timers */
#define BT_GAP_SCAN_FAST_INTERVAL               0x0060  /* 60 ms    */
#define BT_GAP_SCAN_FAST_WINDOW                 0x0030  /* 30 ms    */
#define BT_GAP_SCAN_SLOW_INTERVAL_1             0x0800  /* 1.28 s   */
#define BT_GAP_SCAN_SLOW_WINDOW_1               0x0012  /* 11.25 ms */
#define BT_GAP_SCAN_SLOW_INTERVAL_2             0x1000  /* 2.56 s   */
#define BT_GAP_SCAN_SLOW_WINDOW_2               0x0012  /* 11.25 ms */
#define BT_GAP_ADV_FAST_INT_MIN_1               0x0030  /* 30 ms    */
#define BT_GAP_ADV_FAST_INT_MAX_1               0x0060  /* 60 ms    */
#define BT_GAP_ADV_FAST_INT_MIN_2               0x00a0  /* 100 ms   */
#define BT_GAP_ADV_FAST_INT_MAX_2               0x00f0  /* 150 ms   */
#define BT_GAP_ADV_SLOW_INT_MIN                 0x0640  /* 1 s      */
#define BT_GAP_ADV_SLOW_INT_MAX                 0x0780  /* 1.2 s    */
#define BT_GAP_PER_ADV_FAST_INT_MIN_1           0x0018  /* 30 ms    */
#define BT_GAP_PER_ADV_FAST_INT_MAX_1           0x0030  /* 60 ms    */
#define BT_GAP_PER_ADV_FAST_INT_MIN_2           0x0050  /* 100 ms   */
#define BT_GAP_PER_ADV_FAST_INT_MAX_2           0x0078  /* 150 ms   */
#define BT_GAP_PER_ADV_SLOW_INT_MIN             0x0320  /* 1 s      */
#define BT_GAP_PER_ADV_SLOW_INT_MAX             0x03C0  /* 1.2 s    */
#define BT_GAP_INIT_CONN_INT_MIN                0x0018  /* 30 ms    */
#define BT_GAP_INIT_CONN_INT_MAX                0x0028  /* 50 ms    */

/** LE PHY types */
enum {
	/** Convenience macro for when no PHY is set. */
	BT_GAP_LE_PHY_NONE                    = 0,
	/** LE 1M PHY */
	BT_GAP_LE_PHY_1M                      = BIT(0),
	 /** LE 2M PHY */
	BT_GAP_LE_PHY_2M                      = BIT(1),
	/** LE Coded PHY */
	BT_GAP_LE_PHY_CODED                   = BIT(2),
};

/** Advertising PDU types */
enum {
	/** Scannable and connectable advertising. */
	BT_GAP_ADV_TYPE_ADV_IND               = 0x00,
	/** Directed connectable advertising. */
	BT_GAP_ADV_TYPE_ADV_DIRECT_IND        = 0x01,
	/** Non-connectable and scannable advertising. */
	BT_GAP_ADV_TYPE_ADV_SCAN_IND          = 0x02,
	/** Non-connectable and non-scannable advertising. */
	BT_GAP_ADV_TYPE_ADV_NONCONN_IND       = 0x03,
	/** Additional advertising data requested by an active scanner. */
	BT_GAP_ADV_TYPE_SCAN_RSP              = 0x04,
	/** Extended advertising, see advertising properties. */
	BT_GAP_ADV_TYPE_EXT_ADV               = 0x05,
};

/** Advertising PDU properties */
enum {
	/** Connectable advertising. */
	BT_GAP_ADV_PROP_CONNECTABLE           = BIT(0),
	/** Scannable advertising. */
	BT_GAP_ADV_PROP_SCANNABLE             = BIT(1),
	/** Directed advertising. */
	BT_GAP_ADV_PROP_DIRECTED              = BIT(2),
	/** Additional advertising data requested by an active scanner. */
	BT_GAP_ADV_PROP_SCAN_RESPONSE         = BIT(3),
	/** Extended advertising. */
	BT_GAP_ADV_PROP_EXT_ADV               = BIT(4),
};

/** Maximum advertising data length. */
#define BT_GAP_ADV_MAX_ADV_DATA_LEN             31
/** Maximum extended advertising data length.
 *
 *  @note The maximum advertising data length that can be sent by an extended
 *        advertiser is defined by the controller.
 */
#define BT_GAP_ADV_MAX_EXT_ADV_DATA_LEN         1650

#define BT_GAP_TX_POWER_INVALID                 0x7f
#define BT_GAP_RSSI_INVALID                     0x7f
#define BT_GAP_SID_INVALID                      0xff
#define BT_GAP_NO_TIMEOUT                       0x0000

/* The maximum allowed high duty cycle directed advertising timeout, 1.28
 * seconds in 10 ms unit.
 */
#define BT_GAP_ADV_HIGH_DUTY_CYCLE_MAX_TIMEOUT  128

#define BT_GAP_DATA_LEN_DEFAULT                 0x001b /* 27 bytes */
#define BT_GAP_DATA_LEN_MAX                     0x00fb /* 251 bytes */

#define BT_GAP_DATA_TIME_DEFAULT                0x0148 /* 328 us */
#define BT_GAP_DATA_TIME_MAX                    0x4290 /* 17040 us */

#define BT_GAP_SID_MAX                          0x0F
#define BT_GAP_PER_ADV_MAX_SKIP                 0x01F3
#define BT_GAP_PER_ADV_MIN_TIMEOUT              0x000A
#define BT_GAP_PER_ADV_MAX_TIMEOUT              0x4000
/** Minimum Periodic Advertising Interval (N * 1.25 ms) */
#define BT_GAP_PER_ADV_MIN_INTERVAL             0x0006
/** Maximum Periodic Advertising Interval (N * 1.25 ms) */
#define BT_GAP_PER_ADV_MAX_INTERVAL             0xFFFF

/**
 * @brief Convert periodic advertising interval (N * 1.25 ms) to milliseconds
 *
 * 5 / 4 represents 1.25 ms unit.
 */
#define BT_GAP_PER_ADV_INTERVAL_TO_MS(interval) ((interval) * 5 / 4)

/** Constant Tone Extension (CTE) types */
enum {
	/** Angle of Arrival */
	BT_GAP_CTE_AOA = 0x00,
	/** Angle of Departure with 1 us slots */
	BT_GAP_CTE_AOD_1US = 0x01,
	/** Angle of Departure with 2 us slots */
	BT_GAP_CTE_AOD_2US = 0x02,
	/** No extensions */
	BT_GAP_CTE_NONE = 0xFF,
};

/** @brief Peripheral sleep clock accuracy (SCA) in ppm (parts per million) */
enum {
	BT_GAP_SCA_UNKNOWN = 0,
	BT_GAP_SCA_251_500 = 0,
	BT_GAP_SCA_151_250 = 1,
	BT_GAP_SCA_101_150 = 2,
	BT_GAP_SCA_76_100 = 3,
	BT_GAP_SCA_51_75 = 4,
	BT_GAP_SCA_31_50 = 5,
	BT_GAP_SCA_21_30 = 6,
	BT_GAP_SCA_0_20 = 7,
};

/**
 * @brief Encode 40 least significant bits of 64-bit LE Supported Features into array values
 *        in little-endian format.
 *
 * Helper macro to encode 40 least significant bits of 64-bit LE Supported Features value into
 * advertising data. The number of bits that are encoded is a number of LE Supported Features
 * defined by BT 5.3 Core specification.
 *
 * Example of how to encode the `0x000000DFF00DF00D` into advertising data.
 *
 * @code
 * BT_DATA_BYTES(BT_DATA_LE_SUPPORTED_FEATURES, BT_LE_SUPP_FEAT_40_ENCODE(0x000000DFF00DF00D))
 * @endcode
 *
 * @param w64 LE Supported Features value (64-bits)
 *
 * @return The comma separated values for LE Supported Features value that
 *         may be used directly as an argument for @ref BT_DATA_BYTES.
 */
#define BT_LE_SUPP_FEAT_40_ENCODE(w64) \
	(((w64) >> 0) & 0xFF),  \
	(((w64) >> 8) & 0xFF),  \
	(((w64) >> 16) & 0xFF), \
	(((w64) >> 24) & 0xFF), \
	(((w64) >> 32) & 0xFF)

/** @brief Encode 4 least significant bytes of 64-bit LE Supported Features into
 *         4 bytes long array of values in little-endian format.
 *
 *  Helper macro to encode 64-bit LE Supported Features value into advertising
 *  data. The macro encodes 4 least significant bytes into advertising data.
 *  Other 4 bytes are not encoded.
 *
 *  Example of how to encode the `0x000000DFF00DF00D` into advertising data.
 *
 *  @code
 *  BT_DATA_BYTES(BT_DATA_LE_SUPPORTED_FEATURES, BT_LE_SUPP_FEAT_32_ENCODE(0x000000DFF00DF00D))
 *  @endcode
 *
 * @param w64 LE Supported Features value (64-bits)
 *
 * @return The comma separated values for LE Supported Features value that
 *         may be used directly as an argument for @ref BT_DATA_BYTES.
 */
#define BT_LE_SUPP_FEAT_32_ENCODE(w64) \
	(((w64) >> 0) & 0xFF),  \
	(((w64) >> 8) & 0xFF),  \
	(((w64) >> 16) & 0xFF), \
	(((w64) >> 24) & 0xFF)

/**
 * @brief Encode 3 least significant bytes of 64-bit LE Supported Features into
 *        3 bytes long array of values in little-endian format.
 *
 * Helper macro to encode 64-bit LE Supported Features value into advertising
 * data. The macro encodes 3 least significant bytes into advertising data.
 * Other 5 bytes are not encoded.
 *
 * Example of how to encode the `0x000000DFF00DF00D` into advertising data.
 *
 * @code
 * BT_DATA_BYTES(BT_DATA_LE_SUPPORTED_FEATURES, BT_LE_SUPP_FEAT_24_ENCODE(0x000000DFF00DF00D))
 * @endcode
 *
 * @param w64 LE Supported Features value (64-bits)
 *
 * @return The comma separated values for LE Supported Features value that
 *         may be used directly as an argument for @ref BT_DATA_BYTES.
 */
#define BT_LE_SUPP_FEAT_24_ENCODE(w64) \
	(((w64) >> 0) & 0xFF),  \
	(((w64) >> 8) & 0xFF),  \
	(((w64) >> 16) & 0xFF),

/**
 * @brief Encode 2 least significant bytes of 64-bit LE Supported Features into
 *        2 bytes long array of values in little-endian format.
 *
 * Helper macro to encode 64-bit LE Supported Features value into advertising
 * data. The macro encodes 3 least significant bytes into advertising data.
 * Other 6 bytes are not encoded.
 *
 * Example of how to encode the `0x000000DFF00DF00D` into advertising data.
 *
 * @code
 * BT_DATA_BYTES(BT_DATA_LE_SUPPORTED_FEATURES, BT_LE_SUPP_FEAT_16_ENCODE(0x000000DFF00DF00D))
 * @endcode
 *
 * @param w64 LE Supported Features value (64-bits)
 *
 * @return The comma separated values for LE Supported Features value that
 *         may be used directly as an argument for @ref BT_DATA_BYTES.
 */
#define BT_LE_SUPP_FEAT_16_ENCODE(w64) \
	(((w64) >> 0) & 0xFF),  \
	(((w64) >> 8) & 0xFF),

/**
 * @brief Encode the least significant byte of 64-bit LE Supported Features into
 *        single byte long array.
 *
 * Helper macro to encode 64-bit LE Supported Features value into advertising
 * data. The macro encodes the least significant byte into advertising data.
 * Other 7 bytes are not encoded.
 *
 * Example of how to encode the `0x000000DFF00DF00D` into advertising data.
 *
 * @code
 * BT_DATA_BYTES(BT_DATA_LE_SUPPORTED_FEATURES, BT_LE_SUPP_FEAT_8_ENCODE(0x000000DFF00DF00D))
 * @endcode
 *
 * @param w64 LE Supported Features value (64-bits)
 *
 * @return The value of least significant byte of LE Supported Features value
 *         that may be used directly as an argument for @ref BT_DATA_BYTES.
 */
#define BT_LE_SUPP_FEAT_8_ENCODE(w64) \
	(((w64) >> 0) & 0xFF)

/**
 * @brief Validate whether LE Supported Features value does not use bits that are reserved for
 *        future use.
 *
 * Helper macro to check if @p w64 has zeros as bits 40-63. The macro is compliant with BT 5.3
 * Core Specification where bits 0-40 has assigned values. In case of invalid value, build time
 * error is reported.
 */
#define BT_LE_SUPP_FEAT_VALIDATE(w64) \
	BUILD_ASSERT(!((w64) & (~BIT64_MASK(40))), \
		     "RFU bit in LE Supported Features are not zeros.")

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif /* ZEPHYR_INCLUDE_BLUETOOTH_GAP_H_ */
