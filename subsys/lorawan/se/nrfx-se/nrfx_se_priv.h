/*
 * Copyright (c) 2021 Intellinium <giuliano.franchetto@intellinium.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef NRFX_SE_PRIV_H
#define NRFX_SE_PRIV_H

#include <zephyr/types.h>
#include <LoRaMacTypes.h>

struct nrfx_se_key {
	/*!
	 * Key value
	 */
	uint8_t value[16];

	/*!
	 * Random used for derivation
	 */
	uint8_t random[32];
};

int nrfx_se_keys_load(KeyIdentifier_t id, struct nrfx_se_key *key);

int nrfx_se_keys_save(KeyIdentifier_t id, struct nrfx_se_key *key);

#endif /* NRFX_SE_PRIV_H */
