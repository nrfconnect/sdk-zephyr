/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT zephyr_psa_crypto_rng

#include <drivers/entropy.h>
#include <psa/crypto.h>

/* API implementation: PSA Crypto initialization */
static int entropy_psa_crypto_rng_init(const struct device *dev)
{
	psa_status_t status = PSA_ERROR_CORRUPTION_DETECTED;

	ARG_UNUSED(dev);

	status = psa_crypto_init();
	if (status != PSA_SUCCESS) {
		return -EIO;
	}

	return 0;
}

/* API implementation: get_entropy */
static int entropy_psa_crypto_rng_get_entropy(const struct device *dev,
					      uint8_t *buffer, uint16_t length)
{
	psa_status_t status = PSA_ERROR_CORRUPTION_DETECTED;

	ARG_UNUSED(dev);

	status = psa_generate_random(buffer, length);
	if (status != PSA_SUCCESS) {
		return -EIO;
	}

	return 0;
}

/* API implementation: get_entropy_isr */
static int entropy_psa_crypto_rng_get_entropy_isr(const struct device *dev,
						  uint8_t *buffer,
						  uint16_t length,
						  uint32_t flags)
{
	ARG_UNUSED(flags);

	/* No specific handling in case of running from ISR, just call standard API */
	return entropy_psa_crypto_rng_get_entropy(dev, buffer, length);
}

/* Entropy driver APIs structure */
static const struct entropy_driver_api entropy_psa_crypto_rng_api = {
	.get_entropy = entropy_psa_crypto_rng_get_entropy,
	.get_entropy_isr = entropy_psa_crypto_rng_get_entropy_isr
};

/* Entropy driver registration */
DEVICE_DT_INST_DEFINE(0, entropy_psa_crypto_rng_init, NULL, NULL, NULL,
		      PRE_KERNEL_1, CONFIG_ENTROPY_INIT_PRIORITY,
		      &entropy_psa_crypto_rng_api);
