/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __TRUSTED_STORAGE_AUTH_CRYPT_NONCE_H_
#define __TRUSTED_STORAGE_AUTH_CRYPT_NONCE_H_

#include <psa/error.h>

psa_status_t trusted_storage_get_nonce(uint8_t *nonce, size_t nonce_len);

#endif /* __TRUSTED_STORAGE_AUTH_CRYPT_NONCE_H_ */
