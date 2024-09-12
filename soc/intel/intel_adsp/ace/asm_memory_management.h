/* Copyright (c) 2022 Intel Corporation
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @brief Macros for power gating memory banks specific for ACE 1.0
 */

#ifndef __ZEPHYR_ACE_LIB_ASM_MEMORY_MANAGEMENT_H__
#define __ZEPHYR_ACE_LIB_ASM_MEMORY_MANAGEMENT_H__

/* These definitions should be placed elsewhere, but I can't find a good place for them. */
#define LSPGCTL				0x71D80
#define LSPGCTL_HIGH			((LSPGCTL >> 4) & 0xff00)
#define LSPGCTL_LOW			((LSPGCTL >> 4) & 0xff)

#include <zephyr/devicetree.h>

#ifdef _ASMLANGUAGE

.macro m_ace_lpsram_power_down_entire ax, ay, az, au
	movi \au, 8 /* LPSRAM_EBB_QUANTITY */
	movi \az, LSPGCTL_LOW
	addmi \az, \az, LSPGCTL_HIGH
	slli \az, \az, 4

	movi \ay, 1
2 :
	s8i \ay, \az, 0
	memw

1 :
	l8ui \ax, \az, 4
	bne \ax, \ay, 1b

	addi \az, \az, 8
	addi \au, \au, -1
	bnez \au, 2b
.endm

.macro m_ace_hpsram_power_down_entire ax, ay, az, au
	/* Read the HPSRAM bank count from ACE_L2MCAP register */
	movi \au, DT_REG_ADDR(DT_NODELABEL(hsbcap))
	l32i \au, \au, 0
	extui \au, \au, 0, 8 /* Bank count is in the lower 8 bits */

	movi \ay, 1             /* Power down command */

	/* Calculate the address of the HSxPGCTL register */
	movi \az, DT_REG_ADDR(DT_NODELABEL(hsbpm))
2 :
	s8i \ay, \az, 0         /* HSxPGCTL.l2lmpge = 1 (power down) */
	memw
1 :
	l8ui \ax, \az, 4        /* ax = HSxPGISTS.l2lmpgis */
	bne \ax, \ay, 1b        /* wait till status == request */

	addi \az, \az, DT_REG_SIZE(DT_NODELABEL(hsbpm)) /* Move to next bank control register */
	addi \au, \au, -1       /* Decrement bank count */
	bnez \au, 2b            /* If banks are left, continue loop */
.endm
#endif /* _ASMLANGUAGE */
#endif /* __Z_ACE_LIB_ASM_MEMORY_MANAGEMENT_H__ */
