/*
 * Copyright (c) 2018 Workaround GmbH.
 * Copyright (c) 2017 Intel Corporation.
 * Copyright (c) 2017 Nordic Semiconductor ASA
 * Copyright (c) 2015 Runtime Inc
 * Copyright (c) 2018 Google LLC.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
/** @file
 * @brief CRC computation function
 */

#ifndef ZEPHYR_INCLUDE_CRC_H_
#define ZEPHYR_INCLUDE_CRC_H_

#include <zephyr/types.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Initial value expected to be used at the beginning of the crc8_ccitt
 * computation.
 */
#define CRC8_CCITT_INITIAL_VALUE 0xFF

/**
 * @defgroup checksum Checksum
 */

/**
 * @defgroup crc CRC
 * @ingroup checksum
 * @{
 */

/**
 * @brief Generic function for computing CRC 16
 *
 * Compute CRC 16 by passing in the address of the input, the input length
 * and polynomial used in addition to the initial value.
 *
 * @param src Input bytes for the computation
 * @param len Length of the input in bytes
 * @param polynomial The polynomial to use omitting the leading x^16
 *        coefficient
 * @param initial_value Initial value for the CRC computation
 * @param pad Adds padding with zeros at the end of input bytes
 *
 * @return The computed CRC16 value
 */
u16_t crc16(const u8_t *src, size_t len, u16_t polynomial,
	    u16_t initial_value, bool pad);

/**
 * @brief Compute the CRC-16/CCITT checksum of a buffer.
 *
 * See ITU-T Recommendation V.41 (November 1988).  Uses 0x1021 as the
 * polynomial, reflects the input, and reflects the output.
 *
 * To calculate the CRC across non-contiguous blocks use the return
 * value from block N-1 as the seed for block N.
 *
 * For CRC-16/CCITT, use 0 as the initial seed.  Other checksums in
 * the same family can be calculated by changing the seed and/or
 * XORing the final value.  Examples include:
 *
 * - X-25 (used in PPP): seed=0xffff, xor=0xffff, residual=0xf0b8
 *
 * @note API changed in Zephyr 1.11.
 *
 * @param seed Value to seed the CRC with
 * @param src Input bytes for the computation
 * @param len Length of the input in bytes
 *
 * @return The computed CRC16 value
 */
u16_t crc16_ccitt(u16_t seed, const u8_t *src, size_t len);

/**
 * @brief Compute the CRC-16/XMODEM checksum of a buffer.
 *
 * The MSB first version of ITU-T Recommendation V.41 (November 1988).
 * Uses 0x1021 as the polynomial with no reflection.
 *
 * To calculate the CRC across non-contiguous blocks use the return
 * value from block N-1 as the seed for block N.
 *
 * For CRC-16/XMODEM, use 0 as the initial seed.  Other checksums in
 * the same family can be calculated by changing the seed and/or
 * XORing the final value.  Examples include:
 *
 * - CCIITT-FALSE: seed=0xffff
 * - GSM: seed=0, xorout=0xffff, residue=0x1d0f
 *
 * @param seed Value to seed the CRC with
 * @param src Input bytes for the computation
 * @param len Length of the input in bytes
 *
 * @return The computed CRC16 value
 */
u16_t crc16_itu_t(u16_t seed, const u8_t *src, size_t len);

/**
 * @brief Compute ANSI variant of CRC 16
 *
 * ANSI variant of CRC 16 is using 0x8005 as its polynomial with the initial
 * value set to 0xffff.
 *
 * @param src Input bytes for the computation
 * @param len Length of the input in bytes
 *
 * @return The computed CRC16 value
 */
static inline u16_t crc16_ansi(const u8_t *src, size_t len)
{
	return crc16(src, len, 0x8005, 0xffff, true);
}

/**
 * @brief Generate IEEE conform CRC32 checksum.
 *
 * @param  *data        Pointer to data on which the CRC should be calculated.
 * @param  len          Data length.
 *
 * @return CRC32 value.
 *
 */
u32_t crc32_ieee(const u8_t *data, size_t len);

/**
 * @brief Update an IEEE conforming CRC32 checksum.
 *
 * @param crc   CRC32 checksum that needs to be updated.
 * @param *data Pointer to data on which the CRC should be calculated.
 * @param len   Data length.
 *
 * @return CRC32 value.
 *
 */
u32_t crc32_ieee_update(u32_t crc, const u8_t *data, size_t len);

/**
 * @brief Compute CCITT variant of CRC 8
 *
 * Normal CCITT variant of CRC 8 is using 0x07.
 *
 * @param initial_value Initial value for the CRC computation
 * @param buf Input bytes for the computation
 * @param len Length of the input in bytes
 *
 * @return The computed CRC8 value
 */
u8_t crc8_ccitt(u8_t initial_value, const void *buf, size_t len);

/**
 * @brief Compute the CRC-7 checksum of a buffer.
 *
 * See JESD84-A441.  Used by the MMC protocol.  Uses 0x09 as the
 * polynomial with no reflection.  The CRC is left
 * justified, so bit 7 of the result is bit 6 of the CRC.
 *
 * @param seed Value to seed the CRC with
 * @param src Input bytes for the computation
 * @param len Length of the input in bytes
 *
 * @return The computed CRC7 value
 */
u8_t crc7_be(u8_t seed, const u8_t *src, size_t len);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif
