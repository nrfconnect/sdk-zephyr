/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <soc.h>

#define BT_DBG_ENABLED IS_ENABLED(CONFIG_BT_DEBUG_HCI_DRIVER)
#include "common/log.h"

extern void soc_rand_prio_low_vector_get_blocking(u8_t *p_buff, u8_t length);
extern void ll_util_revcpy(u8_t *dest, const u8_t *src, u8_t size);
extern void ll_util_block_encrypt(const u8_t key[16], const u8_t plaintext[16],
				  bool is_result_le, u8_t ciphertext[16]);

int bt_rand(void *buf, size_t len)
{
	soc_rand_prio_low_vector_get_blocking(buf, len);
	return 0;
}

int bt_encrypt_le(const u8_t key[16], const u8_t plaintext[16],
		  u8_t enc_data[16])
{
	BT_DBG("key %s plaintext %s", bt_hex(key, 16), bt_hex(plaintext, 16));

	ll_util_block_encrypt(key, plaintext, true, enc_data);

	BT_DBG("enc_data %s", bt_hex(enc_data, 16));

	return 0;
}

int bt_encrypt_be(const u8_t key[16], const u8_t plaintext[16],
		  u8_t enc_data[16])
{
	BT_DBG("key %s plaintext %s", bt_hex(key, 16), bt_hex(plaintext, 16));

	u8_t key_be[16], plaintext_be[16];

	ll_util_revcpy(key_be, key, 16);
	ll_util_revcpy(plaintext_be, plaintext, 16);
	ll_util_block_encrypt(key_be, plaintext_be, false, enc_data);

	BT_DBG("enc_data %s", bt_hex(enc_data, 16));

	return 0;
}
