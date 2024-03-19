/*
 * Copyright (c) 2023 Nordic Semiconductor ASA.
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include "spu_log_internal.h"
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(spu, CONFIG_SPU_LOG_LEVEL);

void spu_log_internal_remapped_periph_id(uintptr_t address, unsigned int index)
{
	const uint32_t address_id = nrf_address_slave_get(address);
	if (index != address_id) {
		LOG_DBG("Periph@0x%lx (ID %u) configures on index %d", address, address_id, index);
	}
}

static inline bool is_feature_enabled(nrf_spu_feature_t feature)
{
	switch (feature) {
	case NRF_SPU_FEATURE_DPPI_CHANNEL:
	case NRF_SPU_FEATURE_DPPI_CHANNEL_GROUP:
		return IS_ENABLED(CONFIG_SPU_LOG_FEATURE_DPPI);
	case NRF_SPU_FEATURE_GPIO_PIN:
		return IS_ENABLED(CONFIG_SPU_LOG_FEATURE_GPIO);
	case NRF_SPU_FEATURE_GPIOTE_CHANNEL:
	case NRF_SPU_FEATURE_GPIOTE_INTERRUPT:
		return IS_ENABLED(CONFIG_SPU_LOG_FEATURE_GPIOTE);
	case NRF_SPU_FEATURE_IPCT_CHANNEL:
	case NRF_SPU_FEATURE_IPCT_INTERRUPT:
		return IS_ENABLED(CONFIG_SPU_LOG_FEATURE_IPCT);
	case NRF_SPU_FEATURE_GRTC_CC:
	case NRF_SPU_FEATURE_GRTC_SYSCOUNTER:
	case NRF_SPU_FEATURE_GRTC_INTERRUPT:
		return IS_ENABLED(CONFIG_SPU_LOG_FEATURE_GRTC);
	/*
	case NRF_SPU_FEATURE_BELLS_TASKS:
	case NRF_SPU_FEATURE_BELLS_EVENTS:
	case NRF_SPU_FEATURE_BELLS_INTERRUPT:
		return IS_ENABLED(CONFIG_SPU_LOG_FEATURE_BELLBOARD);
	*/
	default:
		return false;
	}
}

void spu_log_internal_feature_perm_pre(NRF_SPU_Type *const spu, const struct spu_feature_cfg *cfg)
{
	if (!is_feature_enabled(cfg->feature)) {
		return;
	}

	LOG_INF("Configuring SPU@0x%lx, Feature %d - Index: [%d][%d]", (uintptr_t)spu, cfg->feature,
		cfg->index, cfg->subindex);
	LOG_DBG("Existing Perms - Owner ID: %d, Secure: %d, Lock: %d",
		nrf_spu_feature_ownerid_get(spu, cfg->feature, cfg->index, cfg->subindex),
		nrf_spu_feature_secattr_get(spu, cfg->feature, cfg->index, cfg->subindex),
		nrf_spu_feature_lock_get(spu, cfg->feature, cfg->index, cfg->subindex));
	LOG_DBG("Perms to Set - Owner: %d, Secure: %d, Lock: %d", cfg->perms.owner,
		cfg->perms.secure, cfg->perms.lock);
}

void spu_log_internal_feature_perm_post(NRF_SPU_Type *const spu, const struct spu_feature_cfg *cfg)
{
	if (!is_feature_enabled(cfg->feature)) {
		return;
	}

	LOG_INF("Owner ID: %d, Secure: %d, Lock: %d",
		nrf_spu_feature_ownerid_get(spu, cfg->feature, cfg->index, cfg->subindex),
		nrf_spu_feature_secattr_get(spu, cfg->feature, cfg->index, cfg->subindex),
		nrf_spu_feature_lock_get(spu, cfg->feature, cfg->index, cfg->subindex));
}
