/** @file
 *  @brief Header for Bluetooth GMAP LC3 presets.
 *
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_INCLUDE_BLUETOOTH_AUDIO_GMAP_LC3_PRESET_
#define ZEPHYR_INCLUDE_BLUETOOTH_AUDIO_GMAP_LC3_PRESET_

#include <zephyr/bluetooth/audio/bap_lc3_preset.h>

/* GMAP LC3 unicast presets defined by table 3.16 in the GMAP v1.0 specification */

/**
 *  @brief Helper to declare LC3 32_1_gr codec configuration
 *
 *  @param _loc             Audio channel location bitfield (@ref bt_audio_location)
 *  @param _stream_context  Stream context (``BT_AUDIO_CONTEXT_*``)
 */
#define BT_GMAP_LC3_PRESET_32_1_GR(_loc, _stream_context)                                          \
	BT_BAP_LC3_PRESET(BT_AUDIO_CODEC_LC3_CONFIG_32_1(_loc, _stream_context),                   \
			  BT_AUDIO_CODEC_LC3_QOS_7_5_UNFRAMED(60U, 1U, 15U, 10000U))

/**
 *  @brief Helper to declare LC3 32_2_gr codec configuration
 *
 *  @param _loc             Audio channel location bitfield (@ref bt_audio_location)
 *  @param _stream_context  Stream context (``BT_AUDIO_CONTEXT_*``)
 */
#define BT_GMAP_LC3_PRESET_32_2_GR(_loc, _stream_context)                                          \
	BT_BAP_LC3_PRESET(BT_AUDIO_CODEC_LC3_CONFIG_32_2(_loc, _stream_context),                   \
			  BT_AUDIO_CODEC_LC3_QOS_10_UNFRAMED(80U, 1U, 20U, 10000U))

/**
 *  @brief Helper to declare LC3 48_1_gr codec configuration
 *
 *  @param _loc             Audio channel location bitfield (@ref bt_audio_location)
 *  @param _stream_context  Stream context (``BT_AUDIO_CONTEXT_*``)
 */
#define BT_GMAP_LC3_PRESET_48_1_GR(_loc, _stream_context)                                          \
	BT_BAP_LC3_PRESET(BT_AUDIO_CODEC_LC3_CONFIG_48_1(_loc, _stream_context),                   \
			  BT_AUDIO_CODEC_LC3_QOS_7_5_UNFRAMED(75U, 1U, 15U, 10000U))

/**
 *  @brief Helper to declare LC3 48_2_gr codec configuration
 *
 *  Mandatory to support as both unicast client and server
 *
 *  @param _loc             Audio channel location bitfield (@ref bt_audio_location)
 *  @param _stream_context  Stream context (``BT_AUDIO_CONTEXT_*``)
 */
#define BT_GMAP_LC3_PRESET_48_2_GR(_loc, _stream_context)                                          \
	BT_BAP_LC3_PRESET(BT_AUDIO_CODEC_LC3_CONFIG_48_2(_loc, _stream_context),                   \
			  BT_AUDIO_CODEC_LC3_QOS_10_UNFRAMED(100U, 1U, 20U, 10000U))

/**
 *  @brief Helper to declare LC3 48_3_gr codec configuration
 *
 *  @param _loc             Audio channel location bitfield (@ref bt_audio_location)
 *  @param _stream_context  Stream context (``BT_AUDIO_CONTEXT_*``)
 */
#define BT_GMAP_LC3_PRESET_48_3_GR(_loc, _stream_context)                                          \
	BT_BAP_LC3_PRESET(BT_AUDIO_CODEC_LC3_CONFIG_48_3(_loc, _stream_context),                   \
			  BT_AUDIO_CODEC_LC3_QOS_7_5_UNFRAMED(90U, 1U, 15U, 10000U))

/**
 *  @brief Helper to declare LC3 48_4_gr codec configuration
 *
 *  Mandatory to support as unicast server
 *
 *  @param _loc             Audio channel location bitfield (@ref bt_audio_location)
 *  @param _stream_context  Stream context (``BT_AUDIO_CONTEXT_*``)
 */
#define BT_GMAP_LC3_PRESET_48_4_GR(_loc, _stream_context)                                          \
	BT_BAP_LC3_PRESET(BT_AUDIO_CODEC_LC3_CONFIG_48_4(_loc, _stream_context),                   \
			  BT_AUDIO_CODEC_LC3_QOS_10_UNFRAMED(120U, 1U, 20U, 10000U))

/**
 *  @brief Helper to declare LC3 16_1_gs codec configuration
 *
 *  @param _loc             Audio channel location bitfield (@ref bt_audio_location)
 *  @param _stream_context  Stream context (``BT_AUDIO_CONTEXT_*``)
 */
#define BT_GMAP_LC3_PRESET_16_1_GS(_loc, _stream_context)                                          \
	BT_BAP_LC3_PRESET(BT_AUDIO_CODEC_LC3_CONFIG_16_1(_loc, _stream_context),                   \
			  BT_AUDIO_CODEC_LC3_QOS_7_5_UNFRAMED(30U, 1U, 15U, 60000U))

/**
 *  @brief Helper to declare LC3 16_2_gs codec configuration
 *
 *  @param _loc             Audio channel location bitfield (@ref bt_audio_location)
 *  @param _stream_context  Stream context (``BT_AUDIO_CONTEXT_*``)
 */
#define BT_GMAP_LC3_PRESET_16_2_GS(_loc, _stream_context)                                          \
	BT_BAP_LC3_PRESET(BT_AUDIO_CODEC_LC3_CONFIG_16_2(_loc, _stream_context),                   \
			  BT_AUDIO_CODEC_LC3_QOS_10_UNFRAMED(40U, 1U, 20U, 60000U))

/**
 *  @brief Helper to declare LC3 32_1_gs codec configuration
 *
 *  @param _loc             Audio channel location bitfield (@ref bt_audio_location)
 *  @param _stream_context  Stream context (``BT_AUDIO_CONTEXT_*``)
 */
#define BT_GMAP_LC3_PRESET_32_1_GS(_loc, _stream_context)                                          \
	BT_BAP_LC3_PRESET(BT_AUDIO_CODEC_LC3_CONFIG_32_1(_loc, _stream_context),                   \
			  BT_AUDIO_CODEC_LC3_QOS_7_5_UNFRAMED(60U, 1U, 15U, 60000U))

/**
 *  @brief Helper to declare LC3 32_2_gs codec configuration
 *
 *  @param _loc             Audio channel location bitfield (@ref bt_audio_location)
 *  @param _stream_context  Stream context (``BT_AUDIO_CONTEXT_*``)
 */
#define BT_GMAP_LC3_PRESET_32_2_GS(_loc, _stream_context)                                          \
	BT_BAP_LC3_PRESET(BT_AUDIO_CODEC_LC3_CONFIG_32_2(_loc, _stream_context),                   \
			  BT_AUDIO_CODEC_LC3_QOS_10_UNFRAMED(80U, 1U, 20U, 60000U))

/**
 *  @brief Helper to declare LC3 48_1_gs codec configuration
 *
 *  @param _loc             Audio channel location bitfield (@ref bt_audio_location)
 *  @param _stream_context  Stream context (``BT_AUDIO_CONTEXT_*``)
 */
#define BT_GMAP_LC3_PRESET_48_1_GS(_loc, _stream_context)                                          \
	BT_BAP_LC3_PRESET(BT_AUDIO_CODEC_LC3_CONFIG_48_1(_loc, _stream_context),                   \
			  BT_AUDIO_CODEC_LC3_QOS_7_5_UNFRAMED(75U, 1U, 15U, 60000U))

/**
 *  @brief Helper to declare LC3 48_2_gs codec configuration
 *
 *  @param _loc             Audio channel location bitfield (@ref bt_audio_location)
 *  @param _stream_context  Stream context (``BT_AUDIO_CONTEXT_*``)
 */
#define BT_GMAP_LC3_PRESET_48_2_GS(_loc, _stream_context)                                          \
	BT_BAP_LC3_PRESET(BT_AUDIO_CODEC_LC3_CONFIG_48_2(_loc, _stream_context),                   \
			  BT_AUDIO_CODEC_LC3_QOS_10_UNFRAMED(100U, 1U, 20U, 60000U))

/* GMAP LC3 broadcast presets defined by table 3.22 in the GMAP v1.0 specification */

/**
 *  @brief Helper to declare LC3 48_1_g codec configuration
 *
 *  @param _loc             Audio channel location bitfield (@ref bt_audio_location)
 *  @param _stream_context  Stream context (``BT_AUDIO_CONTEXT_*``)
 */
#define BT_GMAP_LC3_PRESET_48_1_G(_loc, _stream_context)                                           \
	BT_BAP_LC3_PRESET(BT_AUDIO_CODEC_LC3_CONFIG_48_1(_loc, _stream_context),                   \
			  BT_AUDIO_CODEC_LC3_QOS_7_5_UNFRAMED(75U, 1U, 8U, 10000U))

/**
 *  @brief Helper to declare LC3 48_2_g codec configuration
 *
 *  @param _loc             Audio channel location bitfield (@ref bt_audio_location)
 *  @param _stream_context  Stream context (``BT_AUDIO_CONTEXT_*``)
 */
#define BT_GMAP_LC3_PRESET_48_2_G(_loc, _stream_context)                                           \
	BT_BAP_LC3_PRESET(BT_AUDIO_CODEC_LC3_CONFIG_48_2(_loc, _stream_context),                   \
			  BT_AUDIO_CODEC_LC3_QOS_10_UNFRAMED(100U, 1U, 10U, 10000U))

/**
 *  @brief Helper to declare LC3 48_3_g codec configuration
 *
 *  @param _loc             Audio channel location bitfield (@ref bt_audio_location)
 *  @param _stream_context  Stream context (``BT_AUDIO_CONTEXT_*``)
 */
#define BT_GMAP_LC3_PRESET_48_3_G(_loc, _stream_context)                                           \
	BT_BAP_LC3_PRESET(BT_AUDIO_CODEC_LC3_CONFIG_48_3(_loc, _stream_context),                   \
			  BT_AUDIO_CODEC_LC3_QOS_7_5_UNFRAMED(90U, 1U, 8U, 10000U))

/**
 *  @brief Helper to declare LC3 48_4_g codec configuration
 *
 *  @param _loc             Audio channel location bitfield (@ref bt_audio_location)
 *  @param _stream_context  Stream context (``BT_AUDIO_CONTEXT_*``)
 */
#define BT_GMAP_LC3_PRESET_48_4_G(_loc, _stream_context)                                           \
	BT_BAP_LC3_PRESET(BT_AUDIO_CODEC_LC3_CONFIG_48_4(_loc, _stream_context),                   \
			  BT_AUDIO_CODEC_LC3_QOS_10_UNFRAMED(120U, 1U, 10U, 10000U))

#endif /* ZEPHYR_INCLUDE_BLUETOOTH_AUDIO_GMAP_LC3_PRESET_ */
