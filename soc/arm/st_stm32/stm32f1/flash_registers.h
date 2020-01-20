/*
 * Copyright (c) 2016 Open-RnD Sp. z o.o.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _STM32F10X_FLASH_REGISTERS_H_
#define _STM32F10X_FLASH_REGISTERS_H_

/**
 * @brief
 *
 * Based on reference manual:
 *   STM32F101xx, STM32F102xx, STM32F103xx, STM32F105xx and STM32F107xx
 *   advanced ARM(r)-based 32-bit MCUs
 *
 * Chapter 3.3.3: Embedded Flash Memory
 */

enum {
	STM32F10X_FLASH_LATENCY_0 = 0x0,
	STM32F10X_FLASH_LATENCY_1 = 0x1,
	STM32F10X_FLASH_LATENCY_2 = 0x2,
};

/* 3.3.3 FLASH_ACR */
union __ef_acr {
	u32_t val;
	struct {
		u32_t latency :3 __packed;
		u32_t hlfcya :1 __packed;
		u32_t prftbe :1 __packed;
		u32_t prftbs :1 __packed;
		u32_t rsvd__6_31 :26 __packed;
	} bit;
};

/* 3.3.3 Embedded flash registers */
struct stm32f1x_flash {
	volatile union __ef_acr acr;
	volatile u32_t keyr;
	volatile u32_t optkeyr;
	volatile u32_t sr;
	volatile u32_t cr;
	volatile u32_t ar;
	volatile u32_t rsvd;
	volatile u32_t obr;
	volatile u32_t wrpr;
};

#endif	/* _STM32F10X_FLASHREGISTERS_H_ */
