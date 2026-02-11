/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef IRONSIDE_SE_KEY_IDS_H_
#define IRONSIDE_SE_KEY_IDS_H_

/* IronSide key IDs with implementation-defined properties */
#define IRONSIDE_KEY_ID_REVOCABLE_MIN (0x40002000UL)
#define IRONSIDE_KEY_ID_REVOCABLE_MAX (0x4fffffffUL)

/* CRACEN built-in key IDs */
#define CRACEN_KEY_ID_IAK  (0x7fffc001UL)
#define CRACEN_KEY_ID_MKEK (0x7fffc002UL)
#define CRACEN_KEY_ID_MEXT (0x7fffc003UL)

/* Location value for CRACEN built-in keys */
#define CRACEN_KEY_LOCATION ((psa_key_location_t)0x804e00)

#endif /* IRONSIDE_SE_KEY_IDS_H_ */
