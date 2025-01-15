/**
 * Common functions and helpers for BSIM GATT tests
 *
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define CHRC_SIZE 10

#define TEST_SERVICE_UUID \
	BT_UUID_DECLARE_128(0x01, 0x23, 0x45, 0x67, 0x89, 0x01, 0x02, 0x03, \
			    0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x00, 0x00)

#define TEST_UNHANDLED_CHRC_UUID \
	BT_UUID_DECLARE_128(0x01, 0x23, 0x45, 0x67, 0x89, 0x01, 0x02, 0x03, \
			    0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0xFD, 0x00)

#define TEST_UNAUTHORIZED_CHRC_UUID \
	BT_UUID_DECLARE_128(0x01, 0x23, 0x45, 0x67, 0x89, 0x01, 0x02, 0x03, \
			    0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0xFE, 0x00)

#define TEST_AUTHORIZED_CHRC_UUID \
	BT_UUID_DECLARE_128(0x01, 0x23, 0x45, 0x67, 0x89, 0x01, 0x02, 0x03, \
			    0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0xFF, 0x00)

#define TEST_CP_CHRC_UUID \
	BT_UUID_DECLARE_128(0x01, 0x23, 0x45, 0x67, 0x89, 0x01, 0x02, 0x03, \
			    0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0xF0, 0x00)
