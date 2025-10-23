/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef IRONSIDE_SE_PERIPHCONF_H_
#define IRONSIDE_SE_PERIPHCONF_H_

#include <stdint.h>

#include <nrfx.h>

#include <ironside/se/internal/mdk.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Entry in the PERIPHCONF table. */
struct periphconf_entry {
	/** Register pointer. */
	uint32_t regptr;
	/** Register value. */
	uint32_t value;
};

/** @brief Initialize a PERIPHCONF entry for a SPU PERIPH[n].PERM register value.
 *
 * @param _spu Global domain SPU instance address.
 * @param _index Peripheral slave index on the bus (PERIPH[n] register index).
 * @param _secattr If true, set SECATTR to secure, otherwise set it to non-secure.
 * @param _dmasec If true, set DMASEC to secure, otherwise set it to non-secure.
 * @param _ownerid OWNERID field value.
 */
#define PERIPHCONF_SPU_PERIPH_PERM(_spu, _index, _secattr, _dmasec, _ownerid)                      \
	(struct periphconf_entry)                                                                  \
	{                                                                                          \
		.regptr = (uint32_t)&((NRF_SPU_Type *)(_spu))->PERIPH[(_index)].PERM,              \
		.value = (uint32_t)((((_ownerid) << SPU_PERIPH_PERM_OWNERID_Pos) &                 \
				     SPU_PERIPH_PERM_OWNERID_Msk) |                                \
				    (((_secattr) ? SPU_PERIPH_PERM_SECATTR_Secure :                \
						   SPU_PERIPH_PERM_SECATTR_NonSecure)              \
				     << SPU_PERIPH_PERM_SECATTR_Pos) |                             \
				    (((_dmasec) ? SPU_PERIPH_PERM_DMASEC_Secure :                  \
						  SPU_PERIPH_PERM_DMASEC_NonSecure)                \
				     << SPU_PERIPH_PERM_DMASEC_Pos) |                              \
				    (SPU_PERIPH_PERM_LOCK_Locked << SPU_PERIPH_PERM_LOCK_Pos)),    \
	}

/** @brief Initialize a PERIPHCONF entry for a SPU FEATURE.IPCT.CH[n] register value.
 *
 * @param _spu Global domain SPU instance address.
 * @param _index Feature index.
 * @param _secattr If true, set SECATTR to secure, otherwise set it to non-secure.
 * @param _ownerid OWNERID field value.
 */
#define PERIPHCONF_SPU_FEATURE_IPCT_CH(_spu, _index, _secattr, _ownerid)                           \
	(struct periphconf_entry)                                                                  \
	{                                                                                          \
		.regptr = (uint32_t)&((NRF_SPU_Type *)(_spu))->FEATURE.IPCT.CH[_index],            \
		.value = _PERIPHCONF_SPU_FEATURE_VAL(_secattr, _ownerid),                          \
	}

/** @brief Initialize a PERIPHCONF entry for a SPU FEATURE.IPCT.INTERRUPT[n] register value.
 *
 * @param _spu Global domain SPU instance address.
 * @param _index Feature index.
 * @param _secattr If true, set SECATTR to secure, otherwise set it to non-secure.
 * @param _ownerid OWNERID field value.
 */
#define PERIPHCONF_SPU_FEATURE_IPCT_INTERRUPT(_spu, _index, _secattr, _ownerid)                    \
	(struct periphconf_entry)                                                                  \
	{                                                                                          \
		.regptr = (uint32_t)&((NRF_SPU_Type *)(_spu))->FEATURE.IPCT.INTERRUPT[_index],     \
		.value = _PERIPHCONF_SPU_FEATURE_VAL(_secattr, _ownerid),                          \
	}

/** @brief Initialize a PERIPHCONF entry for a SPU FEATURE.DPPIC.CH[n] register value.
 *
 * @param _spu Global domain SPU instance address.
 * @param _index Feature index.
 * @param _secattr If true, set SECATTR to secure, otherwise set it to non-secure.
 * @param _ownerid OWNERID field value.
 */
#define PERIPHCONF_SPU_FEATURE_DPPIC_CH(_spu, _index, _secattr, _ownerid)                          \
	(struct periphconf_entry)                                                                  \
	{                                                                                          \
		.regptr = (uint32_t)&((NRF_SPU_Type *)(_spu))->FEATURE.DPPIC.CH[_index],           \
		.value = _PERIPHCONF_SPU_FEATURE_VAL(_secattr, _ownerid),                          \
	}

/** @brief Initialize a PERIPHCONF entry for a SPU FEATURE.DPPIC.CHG[n] register value.
 *
 * @param _spu Global domain SPU instance address.
 * @param _index Register index.
 * @param _secattr If true, set SECATTR to secure, otherwise set it to non-secure.
 * @param _ownerid OWNERID field value.
 */
#define PERIPHCONF_SPU_FEATURE_DPPIC_CHG(_spu, _index, _secattr, _ownerid)                         \
	(struct periphconf_entry)                                                                  \
	{                                                                                          \
		.regptr = (uint32_t)&((NRF_SPU_Type *)(_spu))->FEATURE.DPPIC.CHG[_index],          \
		.value = _PERIPHCONF_SPU_FEATURE_VAL(_secattr, _ownerid),                          \
	}

/** @brief Initialize a PERIPHCONF entry for a SPU FEATURE.GPIOTE[n].CH[m] register value.
 *
 * @param _spu Global domain SPU instance address.
 * @param _index Feature index (GPIOTE[n] register index).
 * @param _subindex Feature subindex (CH[m] register index).
 * @param _secattr If true, set the SECATTR to secure, otherwise set it to non-secure.
 * @param _ownerid OWNERID field value.
 */
#define PERIPHCONF_SPU_FEATURE_GPIOTE_CH(_spu, _index, _subindex, _secattr, _ownerid)              \
	(struct periphconf_entry)                                                                  \
	{                                                                                          \
		.regptr =                                                                          \
			(uint32_t)&((NRF_SPU_Type *)(_spu))->FEATURE.GPIOTE[_index].CH[_subindex], \
		.value = _PERIPHCONF_SPU_FEATURE_VAL(_secattr, _ownerid),                          \
	}

/** @brief Initialize a PERIPHCONF entry for a SPU FEATURE.GPIOTE.INTERRUPT[n] register value.
 *
 * @param _spu Global domain SPU instance address.
 * @param _index Feature index.
 * @param _subindex Feature subindex.
 * @param _secattr If true, set SECATTR to secure, otherwise set it to non-secure.
 * @param _ownerid OWNERID field value.
 */
#define PERIPHCONF_SPU_FEATURE_GPIOTE_INTERRUPT(_spu, _index, _subindex, _secattr, _ownerid)       \
	(struct periphconf_entry)                                                                  \
	{                                                                                          \
		.regptr = (uint32_t)&((NRF_SPU_Type *)(_spu))                                      \
				  ->FEATURE.GPIOTE[_index]                                         \
				  .INTERRUPT[_subindex],                                           \
		.value = _PERIPHCONF_SPU_FEATURE_VAL(_secattr, _ownerid),                          \
	}

/** @brief Initialize a PERIPHCONF entry for a SPU FEATURE.GPIO[n].PIN[m] register value.
 *
 * @param _spu Global domain SPU instance address.
 * @param _index Feature index.
 * @param _subindex Feature subindex.
 * @param _secattr If true, set SECATTR to secure, otherwise set it to non-secure.
 * @param _ownerid OWNERID field value.
 */
#define PERIPHCONF_SPU_FEATURE_GPIO_PIN(_spu, _index, _subindex, _secattr, _ownerid)               \
	(struct periphconf_entry)                                                                  \
	{                                                                                          \
		.regptr =                                                                          \
			(uint32_t)&((NRF_SPU_Type *)(_spu))->FEATURE.GPIO[_index].PIN[_subindex],  \
		.value = _PERIPHCONF_SPU_FEATURE_VAL(_secattr, _ownerid),                          \
	}

/** @brief Initialize a PERIPHCONF entry for a SPU FEATURE.GRTC.CC[n] register value.
 *
 * @param _spu Global domain SPU instance address.
 * @param _index Feature index.
 * @param _secattr If true, set SECATTR to secure, otherwise set it to non-secure.
 * @param _ownerid OWNERID field value.
 */
#define PERIPHCONF_SPU_FEATURE_GRTC_CC(_spu, _index, _secattr, _ownerid)                           \
	(struct periphconf_entry)                                                                  \
	{                                                                                          \
		.regptr = (uint32_t)&((NRF_SPU_Type *)(_spu))->FEATURE.GRTC.CC[_index],            \
		.value = _PERIPHCONF_SPU_FEATURE_VAL(_secattr, _ownerid),                          \
	}

/* Common macro for encoding a SPU FEATURE.* register value.
 * Note that the MDK SPU_FEATURE_IPCT_CH_ macros are used for all since all the FEATURE registers
 * have the same layout with different naming.
 */
#define _PERIPHCONF_SPU_FEATURE_VAL(_secattr, _ownerid)                                            \
	(uint32_t)((((_ownerid) << SPU_FEATURE_IPCT_CH_OWNERID_Pos) &                              \
		    SPU_FEATURE_IPCT_CH_OWNERID_Msk) |                                             \
		   (((_secattr) ? SPU_FEATURE_IPCT_CH_SECATTR_Secure :                             \
				  SPU_FEATURE_IPCT_CH_SECATTR_NonSecure)                           \
		    << SPU_FEATURE_IPCT_CH_SECATTR_Pos) |                                          \
		   (SPU_FEATURE_IPCT_CH_LOCK_Locked << SPU_FEATURE_IPCT_CH_LOCK_Pos))

/** @brief Initialize a PERIPHCONF entry for configuring IPCMAP CHANNEL.SOURCE[n].
 *
 * @param _index CHANNEL.SOURCE[n] register index.
 * @param _source_domain DOMAIN field value in CHANNEL[n].SOURCE.
 * @param _source_ch SOURCE field value in CHANNEL[n].SOURCE.
 */
#define PERIPHCONF_IPCMAP_CHANNEL_SOURCE(_index, _source_domain, _source_ch)                       \
	(struct periphconf_entry)                                                                  \
	{                                                                                          \
		.regptr = (uint32_t)&NRF_IPCMAP->CHANNEL[(_index)].SOURCE,                         \
		.value = (uint32_t)((((_source_domain) << IPCMAP_CHANNEL_SOURCE_DOMAIN_Pos) &      \
				     IPCMAP_CHANNEL_SOURCE_DOMAIN_Msk) |                           \
				    (((_source_ch) << IPCMAP_CHANNEL_SOURCE_SOURCE_Pos) &          \
				     IPCMAP_CHANNEL_SOURCE_SOURCE_Msk) |                           \
				    (IPCMAP_CHANNEL_SOURCE_ENABLE_Enabled                          \
				     << IPCMAP_CHANNEL_SOURCE_ENABLE_Pos)),                        \
	}

/** @brief Initialize a PERIPHCONF entry for configuring IPCMAP CHANNEL.SINK[n].
 *
 * @param _index CHANNEL.SOURCE[n]/CHANNEL.SINK[n] register index.
 * @param _sink_domain DOMAIN field value in CHANNEL[n].SINK.
 * @param _sink_ch SINK field value in CHANNEL[n].SINK.
 */
#define PERIPHCONF_IPCMAP_CHANNEL_SINK(_index, _sink_domain, _sink_ch)                             \
	(struct periphconf_entry)                                                                  \
	{                                                                                          \
		.regptr = (uint32_t)&NRF_IPCMAP->CHANNEL[(_index)].SINK,                           \
		.value = (uint32_t)((((_sink_domain) << IPCMAP_CHANNEL_SINK_DOMAIN_Pos) &          \
				     IPCMAP_CHANNEL_SINK_DOMAIN_Msk) |                             \
				    (((_sink_ch) << IPCMAP_CHANNEL_SINK_SINK_Pos) &                \
				     IPCMAP_CHANNEL_SINK_SINK_Msk)),                               \
	}

/** @brief Initialize a PERIPHCONF entry for an IRQMAP IRQ[n].SINK register value.
 *
 * @param _irqnum IRQ number (IRQ[n] register index).
 * @param _processor Processor to route the interrupt to (PROCESSORID field value).
 */
#define PERIPHCONF_IRQMAP_IRQ_SINK(_irqnum, _processor)                                            \
	(struct periphconf_entry)                                                                  \
	{                                                                                          \
		.regptr = (uint32_t)&NRF_IRQMAP->IRQ[(_irqnum)].SINK,                              \
		.value = (uint32_t)(((_processor) << IRQMAP_IRQ_SINK_PROCESSORID_Pos) &            \
				    IRQMAP_IRQ_SINK_PROCESSORID_Msk),                              \
	}

/** @brief Initialize a PERIPHCONF entry for configuring a GPIO PIN_CNF[n] CTRLSEL field value.
 *
 * @param _gpio GPIO instance address.
 * @param _pin Pin number (PIN_CNF[n] register index).
 * @param _ctrlsel CTRLSEL field value.
 */
#define PERIPHCONF_GPIO_PIN_CNF_CTRLSEL(_gpio, _pin, _ctrlsel)                                     \
	(struct periphconf_entry)                                                                  \
	{                                                                                          \
		.regptr = (uint32_t)&((NRF_GPIO_Type *)(_gpio))->PIN_CNF[(_pin)],                  \
		.value = ((GPIO_PIN_CNF_ResetValue) |                                              \
			  (uint32_t)(((_ctrlsel) << GPIO_PIN_CNF_CTRLSEL_Pos) &                    \
				     GPIO_PIN_CNF_CTRLSEL_Msk)),                                   \
	}

/** @brief Initialize a PERIPHCONF entry for a PPIB SUBSCRIBE_SEND[n] register.
 *
 * @param _ppib Global domain PPIB instance address.
 * @param _ppib_ch PPIB channel number.
 */
#define PERIPHCONF_PPIB_SUBSCRIBE_SEND(_ppib, _ppib_ch)                                            \
	(struct periphconf_entry)                                                                  \
	{                                                                                          \
		.regptr = (uint32_t)&((NRF_PPIB_Type *)(_ppib))->SUBSCRIBE_SEND[(_ppib_ch)],       \
		.value = (uint32_t)PPIB_SUBSCRIBE_SEND_EN_Msk,                                     \
	}

/** @brief Initialize a PERIPHCONF entry for a PPIB PUBLISH_RECEIVE[n] register.
 *
 * @param _ppib Global domain PPIB instance address.
 * @param _ppib_ch PPIB channel number.
 */
#define PERIPHCONF_PPIB_PUBLISH_RECEIVE(_ppib, _ppib_ch)                                           \
	(struct periphconf_entry)                                                                  \
	{                                                                                          \
		.regptr = (uint32_t)&((NRF_PPIB_Type *)(_ppib))->PUBLISH_RECEIVE[(_ppib_ch)],      \
		.value = (uint32_t)PPIB_PUBLISH_RECEIVE_EN_Msk,                                    \
	}

/** @brief Initialize a PERIPHCONF entry for a MEMCONF POWER[n].CONTROL register.
 *
 * @param _memconf Global domain MEMCONF instance address.
 * @param _index Register index.
 * @param _value Register value (memory region bitfield).
 */
#define PERIPHCONF_MEMCONF_POWER_CONTROL(_memconf, _index, _value)                                 \
	(struct periphconf_entry)                                                                  \
	{                                                                                          \
		.regptr = (uint32_t)&((NRF_MEMCONF_Type *)(_memconf))->POWER[(_index)].CONTROL,    \
		.value = (uint32_t)(_value),                                                       \
	}

/** @brief Initialize a PERIPHCONF entry for a MEMCONF POWER[n].RET register.
 *
 * @param _memconf Global domain MEMCONF instance address.
 * @param _index Register index.
 * @param _value Register value (memory region bitfield).
 */
#define PERIPHCONF_MEMCONF_POWER_RET(_memconf, _index, _value)                                     \
	(struct periphconf_entry)                                                                  \
	{                                                                                          \
		.regptr = (uint32_t)&((NRF_MEMCONF_Type *)(_memconf))->POWER[(_index)].RET,        \
		.value = (uint32_t)(_value),                                                       \
	}

/** @brief Initialize a PERIPHCONF entry for a MEMCONF POWER[n].RET2 register.
 *
 * @param _memconf Global domain MEMCONF instance address.
 * @param _index Register index.
 * @param _value Register value (memory region bitfield).
 */
#define PERIPHCONF_MEMCONF_POWER_RET2(_memconf, _index, _value)                                    \
	(struct periphconf_entry)                                                                  \
	{                                                                                          \
		.regptr = (uint32_t)&((NRF_MEMCONF_Type *)(_memconf))->POWER[(_index)].RET2,       \
		.value = (uint32_t)(_value),                                                       \
	}

#ifdef __cplusplus
}
#endif
#endif /* IRONSIDE_SE_PERIPHCONF_H_ */
