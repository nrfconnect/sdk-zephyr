/*
 * Copyright (c) 2018 Synopsys, Inc. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @brief Board configuration macros for EM Starter kit board
 *
 * This header file is used to specify and describe board-level
 * aspects for the target.
 */

#ifndef _SOC__H_
#define _SOC__H_

#include <misc/util.h>

/* ARC EM Core IRQs */
#define IRQ_TIMER0				16
#define IRQ_TIMER1				17

#define IRQ_SEC_TIMER0			20

#ifndef _ASMLANGUAGE

#include <misc/util.h>
#include <random/rand32.h>

#define ARCV2_TIMER0_INT_LVL			IRQ_TIMER0
#define ARCV2_TIMER0_INT_PRI			0

#define ARCV2_TIMER1_INT_LVL			IRQ_TIMER1
#define ARCV2_TIMER1_INT_PRI			1

#define INT_ENABLE_ARC				~(0x00000001 << 8)
#define INT_ENABLE_ARC_BIT_POS			(8)

#endif /* !_ASMLANGUAGE */

#endif /* _SOC__H_ */
