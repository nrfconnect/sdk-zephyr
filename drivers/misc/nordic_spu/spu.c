/*
 * Copyright (c) 2023 Nordic Semiconductor ASA.
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include "spu_internal.h"
#include "spu_log_internal.h"

#include <errno.h>
#include <string.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/sys/util.h>

#include <zephyr/drivers/misc/nordic_spu/spu.h>

LOG_MODULE_REGISTER(spu, CONFIG_SPU_LOG_LEVEL);

#define DT_DRV_COMPAT nordic_nrf_spu_v2

/* Size of APB used to mask address bits that don't need to be taken into account. */
#define ADDRESS_BUS_SIZE 1UL

/**
 * @brief       SPU instance information.
 */
struct spu {
	union {
		const uintptr_t address;
		NRF_SPU_Type *const ptr;
	} instance; /**< Convenience union of SPU instance/address. */
	const struct spu_remapped_periph_id *const
		remapped_ids; /**< Pointer to the instance's array of remapped peripheral IDs. */
	const size_t num_remapped_ids; /**< Length of the remapped peripheral index array. */
};

struct spu_data {
	spu_periphaccerr_callback_t periphaccerr_cb;
};

/**
 * @brief       Simple helper for getting the NRF instance of an SPU from its device.
 */
static NRF_SPU_Type *get_nrf_instance(const struct device *dev)
{
	const struct spu *dev_conf = dev->config;

	return (NRF_SPU_Type *)dev_conf->instance.ptr;
}

/**
 * @brief       Return the index of a peripheral on its bus.
 *
 *              This function accounts for remapped peripheral indexes when applicable for the
 *              given peripheral.
 *
 * @param[in]   spu    	Peripheral bus SPU information.
 * @param[in]   address Peripheral address
 *
 * @return      Bus index of the peripheral.
 */
static inline uint32_t get_peripheral_id(const struct spu *spu, uintptr_t address)
{
	const uintptr_t periph_id = nrf_address_slave_get(address);

	for (int i = 0; i < spu->num_remapped_ids; i++) {
		const struct spu_remapped_periph_id *ids = &spu->remapped_ids[i];

		if (periph_id == ids->periph_id) {
			return ids->remapped_id;
		}
	}

	return periph_id;
}

/**
 * @brief       Validate a peripheral address against an SPU address.
 *
 *              SPUs cannot configure peripherals that exist outside of their own bus and memory
 * 		region. Since a bus may exist in multiple regions and SPUs do exist in multiple
 *              regions (such as TDD), we validate against these to see if an peripheral can be
 *              configured by the SPU.
 *
 * @param[in]   spu_address       SPU address
 * @param[in]   periph_address    Peripheral address
 *
 * @return      True if the peripheral is valid for the SPU, false otherwise.
 */
static inline bool is_valid_peripheral_address_for_spu(uintptr_t spu_address,
						       uintptr_t periph_address)
{
	return (nrf_address_region_get(periph_address) == nrf_address_region_get(spu_address)) &&
	       (nrf_address_bus_get(periph_address, ADDRESS_BUS_SIZE) ==
		nrf_address_bus_get(spu_address, ADDRESS_BUS_SIZE));
}

static int get_peripheral_settings(const struct device *dev, uintptr_t address,
				   struct spu_periph_settings *settings)
{
	if (settings == NULL) {
		return -EINVAL;
	}

	const struct spu *dev_conf = dev->config;

	if (nrf_address_region_get(address) != nrf_address_region_get(dev_conf->instance.address)) {
		return -EFAULT;
	}

	NRF_SPU_Type *const spu = (NRF_SPU_Type *const)dev_conf->instance.ptr;
	const uint32_t index = get_peripheral_id(dev_conf, address);

	const struct spu_periph_settings current_settings = {
		.present = nrf_spu_periph_perm_present_get(spu, index),
		.owner_programmable = nrf_spu_periph_perm_ownerprog_get(spu, index),
		.securemapping = nrf_spu_periph_perm_securemapping_get(spu, index),
		.dma = nrf_spu_periph_perm_dma_get(spu, index),
		.owner = nrf_spu_periph_perm_ownerid_get(spu, index),
		.secattr = nrf_spu_periph_perm_secattr_get(spu, index),
		.dmasec = nrf_spu_periph_perm_dmasec_get(spu, index),
		.lock = nrf_spu_periph_perm_lock_get(spu, index),
	};

	memcpy(settings, &current_settings, sizeof(struct spu_periph_settings));

	return 0;
}

static int set_periph_configuration(const struct device *dev, const struct spu_periph_cfg *cfg)
{
	if (cfg == NULL) {
		return -EINVAL;
	}

	const struct spu *dev_conf = dev->config;

	if (!is_valid_peripheral_address_for_spu(dev_conf->instance.address, cfg->address)) {
		return -EFAULT;
	}

	int err;
	NRF_SPU_Type *const spu = (NRF_SPU_Type *const)dev_conf->instance.ptr;
	const uint32_t index = get_peripheral_id(dev_conf, cfg->address);
	struct spu_periph_settings settings;

	err = get_peripheral_settings(dev, cfg->address, &settings);
	if (err) {
		LOG_ERR("Error in SPU@0x%lx PERIPH[%d].PERM (Periph@0x%lx): %d",
			dev_conf->instance.address, index, cfg->address, err);
		return err;
	}

	if (!settings.present) {
		return -ENOTSUP;
	}

	if (settings.lock) {
		return -EACCES;
	}

	SPU_LOG_PERIPH_PERM_REMAP(cfg->address, index);
	SPU_LOG_PERIPH_PERM_PRE(spu, index, cfg);

	if (settings.owner_programmable && (settings.owner != cfg->perms.owner)) {
		nrf_spu_periph_perm_ownerid_set(spu, index, cfg->perms.owner);
	}

	if ((settings.securemapping == NRF_SPU_SECUREMAPPING_USERSELECTABLE) &&
	    (settings.secattr != cfg->perms.secure)) {
		nrf_spu_periph_perm_secattr_set(spu, index, cfg->perms.secure);
	}

	/* Current use cases have DMA security aligning with peripheral security. */
	if ((settings.dma == NRF_SPU_DMA_SEPARATEATTRIBUTE) &&
	    (settings.dmasec != cfg->perms.secure)) {
		nrf_spu_periph_perm_dmasec_set(spu, index, cfg->perms.secure);
	}

	if (cfg->perms.lock) {
		nrf_spu_periph_perm_lock_enable(spu, index);
	}

	SPU_LOG_PERIPH_PERM_POST(spu, index);

	return 0;
}

static int set_feature_configuration(const struct device *dev, const struct spu_feature_cfg *cfg)
{
	if (cfg == NULL) {
		return -EINVAL;
	}

	NRF_SPU_Type *const spu = get_nrf_instance(dev);

	const bool is_locked =
		nrf_spu_feature_lock_get(spu, cfg->feature, cfg->index, cfg->subindex);
	if (is_locked) {
		return -EACCES;
	}

	SPU_LOG_FEATURE_PERM_PRE(spu, cfg);

	nrf_spu_feature_secattr_set(spu, cfg->feature, cfg->index, cfg->subindex,
				    cfg->perms.secure);
	nrf_spu_feature_ownerid_set(spu, cfg->feature, cfg->index, cfg->subindex, cfg->perms.owner);

	if (cfg->perms.lock) {
		nrf_spu_feature_lock_enable(spu, cfg->feature, cfg->index, cfg->subindex);
	}

	SPU_LOG_FEATURE_PERM_POST(spu, cfg);

	return 0;
}

static int register_periphaccerr_callback(const struct device *dev, spu_periphaccerr_callback_t cb)
{
	struct spu_data *data = dev->data;

	if (cb == NULL) {
		return -EINVAL;
	}

	data->periphaccerr_cb = cb;

	return 0;
}

static struct spu_driver_api nrf_spu_driver_api = {
	.get_periph_settings = get_peripheral_settings,
	.configure_periph = set_periph_configuration,
	.configure_feature = set_feature_configuration,
	.register_periphaccerr_callback = register_periphaccerr_callback,
};

static void common_irq_handler(const struct device *dev)
{
	const struct spu *conf = dev->config;
	const struct spu_data *data = dev->data;
	NRF_SPU_Type *const spu = (NRF_SPU_Type *const)conf->instance.ptr;

	if (nrf_spu_event_check(spu, NRF_SPU_EVENT_PERIPHACCERR)) {
		const struct spu_periphaccerr_info info = {
			.accessed_address =
				conf->instance.address | nrf_spu_periphaccerr_address_get(spu),
			.transaction_owner = nrf_spu_periphaccerr_ownerid_get(spu),
		};

		/* This also clears PERIPHACCERR.ADDRESS/INFO */
		nrf_spu_event_clear(spu, NRF_SPU_EVENT_PERIPHACCERR);

		if (data->periphaccerr_cb) {
			data->periphaccerr_cb(dev, &info);
		}
	}
}

static void default_periphaccerr_handler(const struct device *dev,
					 const struct spu_periphaccerr_info *info)
{
	const struct spu *conf = dev->config;

	SPU_LOG_PERIPHACCERR_CATCH(conf->instance.address, info);
}

static int common_init(const struct device *dev, int irqn)
{
	NRF_SPU_Type *const spu = get_nrf_instance(dev);

	nrf_spu_event_clear(spu, NRF_SPU_EVENT_PERIPHACCERR);
	nrf_spu_int_enable(spu, NRF_SPU_INT_PERIPHACCERR_MASK);
	irq_enable(irqn);

	return 0;
}

#define SPU_HAS_REMAPPED_IDS(inst) DT_INST_NODE_HAS_PROP(inst, remapped_periph_ids)
#define SPU_REMAPPED_IDS_PTR(inst)                                                                 \
	COND_CODE_1(SPU_HAS_REMAPPED_IDS(inst), (spu_remapped_ids_##inst), (NULL))
#define SPU_REMAPPED_IDS_LEN(inst)                                                                 \
	COND_CODE_1(SPU_HAS_REMAPPED_IDS(inst), ARRAY_SIZE(spu_remapped_ids_##inst), (0))

#define SPU_DEVICE_INIT(inst)                                                                      \
	IF_ENABLED(SPU_HAS_REMAPPED_IDS(inst),                                                     \
		   (static const struct spu_remapped_periph_id spu_remapped_ids_##inst[] = {       \
			    SPU_GENERATE_REMAPPED_ID_ARRAY(inst)};))                               \
	static const struct spu spu_config_##inst = {                                              \
		.instance.address = DT_INST_REG_ADDR(inst),                                        \
		.remapped_ids = SPU_REMAPPED_IDS_PTR(inst),                                        \
		.num_remapped_ids = SPU_REMAPPED_IDS_LEN(inst),                                    \
	};                                                                                         \
	static struct spu_data spu_data_##inst = {                                                 \
		.periphaccerr_cb = default_periphaccerr_handler,                                   \
	};                                                                                         \
	static void spu_##inst##_irq_handler(void)                                                 \
	{                                                                                          \
		common_irq_handler(DEVICE_DT_INST_GET(inst));                                      \
	}                                                                                          \
	static int spu_init_##inst(const struct device *dev)                                       \
	{                                                                                          \
		IRQ_CONNECT(DT_INST_IRQN(inst), DT_INST_IRQ(inst, priority), nrfx_isr,             \
			    spu_##inst##_irq_handler, 0);                                          \
		return common_init(dev, DT_INST_IRQN(inst));                                       \
	}                                                                                          \
                                                                                                   \
	DEVICE_DT_INST_DEFINE(inst, spu_init_##inst, NULL, &spu_data_##inst, &spu_config_##inst,   \
			      PRE_KERNEL_1, CONFIG_SPU_DEVICE_INIT_PRIORITY, &nrf_spu_driver_api);

/* Call the device creation macro for each instance: */
DT_INST_FOREACH_STATUS_OKAY(SPU_DEVICE_INIT)
