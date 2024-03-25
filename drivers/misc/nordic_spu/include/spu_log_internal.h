/*
 * Copyright (c) 2023 Nordic Semiconductor ASA.
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef NRF_INCLUDE_DRIVERS_SPU_LOG_INTERNAL_H__
#define NRF_INCLUDE_DRIVERS_SPU_LOG_INTERNAL_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <zephyr/logging/log.h>
#include <hal/nrf_spu.h>
#include <zephyr/drivers/misc/nordic_spu/spu.h>

/**
 * @brief    Logging wrapper for remapping peripheral IDs.
 *
 * @param[in]   address    Peripheral address.
 * @param[in]   index      Peripheral ID.
 */
void spu_log_internal_remapped_periph_id(uintptr_t address, unsigned int index);

/**
 * @brief    Logging wrapper for feature permission information, pre-configuration.
 *
 * @param[in]   spu    NRF SPU instance.
 * @param[in]   cfg    Feature configuration.
 */
void spu_log_internal_feature_perm_pre(NRF_SPU_Type *const spu, const struct spu_feature_cfg *cfg);

/**
 * @brief    Logging wrapper for feature permission information, post-configuration.
 *
 * @param[in]   spu    NRF SPU instance
 * @param[in]   cfg    Feature configuration.
 */
void spu_log_internal_feature_perm_post(NRF_SPU_Type *const spu, const struct spu_feature_cfg *cfg);

#if CONFIG_SPU_LOG_PERIPH_CONFIGS
#define SPU_LOG_PERIPH_PERM_PRE(_spu, _index, _cfg)                                                \
	do {                                                                                       \
		LOG_INF("Configuring SPU@0x%lx, PERIPH[%d].PERM", (uintptr_t)_spu, _index);        \
		LOG_INF("Periph@0x%lx - Owner ID: %d, Secure: %d, Lock: %d", _cfg->address,        \
			_cfg->perms.owner, _cfg->perms.secure, _cfg->perms.lock);                  \
	} while (0)

#define SPU_LOG_PERIPH_PERM_POST(_spu, _index)                                                     \
	LOG_DBG("PERIPH[%d].PERM - Owner ID: %d, SECATTR: %d, DMASEC: %d, Lock: %d", _index,       \
		nrf_spu_periph_perm_ownerid_get(_spu, _index),                                     \
		nrf_spu_periph_perm_secattr_get(_spu, _index),                                     \
		nrf_spu_periph_perm_dmasec_get(_spu, _index),                                      \
		nrf_spu_periph_perm_lock_get(_spu, _index))

#else
#define SPU_LOG_PERIPH_PERM_PRE(...)  (void)0
#define SPU_LOG_PERIPH_PERM_POST(...) (void)0
#endif /* CONFIG_SPU_LOG_PERIPH_CONFIGS */

#if CONFIG_SPU_LOG_REMAPPED_PERIPH_IDS
#define SPU_LOG_PERIPH_PERM_REMAP(_address, _index)                                                \
	spu_log_internal_remapped_periph_id(_address, _index)
#else
#define SPU_LOG_PERIPH_PERM_REMAP(...) (void)0
#endif /* CONFIG_SPU_LOG_REMAPPED_PERIPH_IDS */

#if CONFIG_SPU_LOG_FEATURE_CONFIGS
#define SPU_LOG_FEATURE_PERM_PRE(_spu, _cfg)  spu_log_internal_feature_perm_pre(_spu, _cfg)
#define SPU_LOG_FEATURE_PERM_POST(_spu, _cfg) spu_log_internal_feature_perm_post(_spu, _cfg)
#else
#define SPU_LOG_FEATURE_PERM_PRE(...)  (void)0
#define SPU_LOG_FEATURE_PERM_POST(...) (void)0
#endif /* CONFIG_SPU_LOG_FEATURE_CONFIGS */

#define SPU_LOG_PERIPHACCERR_CATCH(_spu_address, _info)                                            \
	do {                                                                                       \
		LOG_WRN("Caught PERIPHACCERR in SPU@0x%lx", (uintptr_t)_spu_address);              \
		LOG_WRN("Owner %d attempted to access 0x%.8lx", _info->transaction_owner,          \
			_info->accessed_address);                                                  \
	} while (0)

#ifdef __cplusplus
}
#endif

#endif /* NRF_INCLUDE_DRIVERS_SPU_LOG_INTERNAL_H__ */
