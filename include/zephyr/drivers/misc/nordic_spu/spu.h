/*
 * Copyright (c) 2023 Nordic Semiconductor ASA.
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef NRF_INCLUDE_DRIVERS_SPU_H__
#define NRF_INCLUDE_DRIVERS_SPU_H__

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#include <nrfx.h>
#include <hal/nrf_spu.h>
#include <zephyr/device.h>

/** Value used for features that have no subindex */
#define SPU_FEATURE_NO_SUBINDEX (0)

/**
 * @brief    Information for remapped peripheral IDs.
 */
struct spu_remapped_periph_id {
	unsigned int periph_id;	  /**< Peripheral ID on its bus. */
	unsigned int remapped_id; /**< Peripheral ID index used for its configuration in the SPU. */
};

/**
 * @brief    Initialization helper for spu_remapped_periph_id structs.
 */
#define SPU_REMAPPED_PERIPH_ID_INIT(_periph_id, _remapped_id, ...)                                 \
	{                                                                                          \
		.periph_id = _periph_id, .remapped_id = _remapped_id,                              \
	}

/**
 * @brief       SPU permissions.
 */
struct spu_perms {
	nrf_owner_t owner; /**< Owner ID. */
	bool secure;	   /**< Security state. */
	bool lock;	   /**< Lock related configuration until next reset. */
};

/**
 * @brief   Configuration information for SPU peripherals.
 */
struct spu_periph_cfg {
	uintptr_t address;	/**< Peripheral address. */
	struct spu_perms perms; /**< Peripheral permissions. */
};

/**
 * @brief   Initialize spu_periph_perm structs with default values.
 */
#define SPU_PERIPH_CFG_INIT_DEFAULT(_address)                                                      \
	{                                                                                          \
		.address = _address,                                                               \
		.perms = {                                                                         \
			.owner = NRF_OWNER,                                                        \
			.secure = true,                                                            \
			.lock = IS_ENABLED(CONFIG_SPU_PERIPHERAL_DEFAULT_LOCK_STATE),              \
		},                                                                                 \
	}

/**
 * @brief       Summary of a peripheral's settings information kept by the SPU.
 */
struct spu_periph_settings {
	bool present;			       /**< Peripheral exists on bus. */
	bool owner_programmable;	       /**< Peripheral owner is programmable. */
	bool secattr;			       /**< Peripheral security attribute. */
	bool dmasec;			       /**< DMA security attribute. */
	bool lock;			       /**< Peripheral configuration is locked. */
	nrf_spu_securemapping_t securemapping; /**< Trustzone security capabilities. */
	nrf_spu_dma_t dma;		       /**< DMA capabilities. */
	nrf_owner_t owner;		       /**< Current peripheral owner. */
};

/**
 * @brief       Relevant info for an SPU feature.
 *
 * @note        Not all SPU features have a subindex (e.g. IPCT.CH[index]
 *              versus GPIO[index].PIN[subindex]), which will result in it
 *              being ignored.
 */
struct spu_feature_cfg {
	nrf_spu_feature_t feature; /**< ID of SPU feature. */
	size_t index;		   /**< Feature array Index, e.g SPU.FEATURE.x[index]. */
	size_t subindex; /**< Feature sub-array Index, e.g SPU.FEATURE.x[index].y[subindex] */
	struct spu_perms perms; /**< Feature permissions. */
};

/**
 * @brief   Initialize spu_feature_perm structs with default values.
 */
#define SPU_FEATURE_CFG_INIT_DEFAULT(_feature)                                                     \
	{                                                                                          \
		.feature = _feature, .index = 0, .subindex = 0,                                    \
		.perms = {                                                                         \
			.owner = NRF_OWNER,                                                        \
			.secure = true,                                                            \
			.lock = IS_ENABLED(CONFIG_SPU_FEATURE_DEFAULT_LOCK_STATE),                 \
		},                                                                                 \
	}

/**
 * @brief    SPU Peripheral Access Error information.
 */
struct spu_periphaccerr_info {
	uintptr_t accessed_address;    /**< Target address of access violation. */
	nrf_owner_t transaction_owner; /**< ID of the owner causing the access violation. */
};

/**
 * @brief    Callback API for an SPU peripheral access error.
 *
 * These callbacks execute in an interrupt context. Therefore, use only
 * interrupt-safe APIs. Registration of callbacks is done via @a
 * spu_register_peripheraccerr_callback().
 *
 * @param[in] dev     Driver instance.
 * @param[in] info    Pointer to error information.
 */
typedef void (*spu_periphaccerr_callback_t)(const struct device *dev,
					    const struct spu_periphaccerr_info *info);

/**
 * @cond INTERNAL_HIDDEN
 *
 * For internal driver use only, skip these in public documentation.
 */
/** @brief Driver API structure. */
__subsystem struct spu_driver_api {
	int (*get_periph_settings)(const struct device *dev, uintptr_t address,
				   struct spu_periph_settings *settings);
	int (*configure_periph)(const struct device *dev, const struct spu_periph_cfg *cfg);
	int (*configure_feature)(const struct device *dev, const struct spu_feature_cfg *cfg);
	int (*register_periphaccerr_callback)(const struct device *dev,
					      spu_periphaccerr_callback_t cb);
};
/** @endcond */

/**
 * @brief       Retrieve the settings of a peripheral on the bus of the related SPU.
 *
 * @param[in]    dev         SPU device.
 * @param[in]    address     Peripheral address.
 * @param[out]   settings    Peripheral settings.
 *
 * @retval    0 on success.
 *            -EINVAL for invalid parameters.
 *            -EFAULT if peripheral is not mapped on the SPU device's bus.
 */
static inline int spu_get_peripheral_settings(const struct device *dev, uintptr_t address,
					      struct spu_periph_settings *settings)
{
	const struct spu_driver_api *api = (const struct spu_driver_api *)dev->api;

	return api->get_periph_settings(dev, address, settings);
}

/**
 * @brief       Configure the permission and settings of a peripheral on the bus of the related SPU.
 *
 * @param[in]   dev     SPU device.
 * @param[in]   cfg     Peripheral configuration.
 *
 * @retval    0 on success.
 *            -EINVAL for invalid parameters.
 *            -EFAULT if peripheral and SPU addresses are on separate buses.
 *            -ENOTSUP if peripheral is not supported by the SPU.
 *            -EACCES if peripheral is unconfigurable (locked).
 */
static inline int spu_configure_peripheral(const struct device *dev,
					   const struct spu_periph_cfg *cfg)
{
	const struct spu_driver_api *api = (const struct spu_driver_api *)dev->api;

	return api->configure_periph(dev, cfg);
}

/**
 * @brief       Configure the permission and settings of an SPU feature.
 *
 * @param[in]   dev     SPU device.
 * @param[in]   cfg     Feature configuration.
 *
 * @retval    0 on success.
 *            -EINVAL for invalid parameters.
 *            -EACCES if feature is unconfigurable (locked).
 */
static inline int spu_configure_feature(const struct device *dev, const struct spu_feature_cfg *cfg)
{
	const struct spu_driver_api *api = (const struct spu_driver_api *)dev->api;

	return api->configure_feature(dev, cfg);
}

/**
 * @brief       Register a callback for addtional handling of peripheral access errors.
 *
 * @param[in]   dev    SPU device.
 * @param[in]   cb     Function to execute when handling an error.
 *
 * @retval    0 on success.
 */
static inline int spu_register_periphaccerr_callback(const struct device *dev,
						     spu_periphaccerr_callback_t cb)
{
	const struct spu_driver_api *api = (const struct spu_driver_api *)dev->api;

	return api->register_periphaccerr_callback(dev, cb);
}

#ifdef __cplusplus
}
#endif

#endif /* NRF_INCLUDE_DRIVERS_SPU_H__ */
