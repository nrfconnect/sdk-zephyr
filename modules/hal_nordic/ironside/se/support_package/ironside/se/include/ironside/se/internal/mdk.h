/*
 * Copyright (c) 2025 Nordic Semiconductor ASA.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef IRONSIDE_INTERNAL_MDK_H_
#define IRONSIDE_INTERNAL_MDK_H_

#include <nrfx.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Only add these definitions for Haltium architecture based products. */
#if defined(HALTIUM_XXAA)

/* The definitions below are not currently available in the MDK but are needed for certain
 * macros in this package. When they are, this can be deleted.
 */
#ifndef IPCMAP_CHANNEL_SOURCE_SOURCE_Msk

typedef struct {
	__IOM uint32_t SOURCE;
	__IOM uint32_t SINK;
} NRF_IPCMAP_CHANNEL_Type;

#define IPCMAP_CHANNEL_SOURCE_SOURCE_Pos      (0UL)
#define IPCMAP_CHANNEL_SOURCE_SOURCE_Msk      (0xFUL << IPCMAP_CHANNEL_SOURCE_SOURCE_Pos)
#define IPCMAP_CHANNEL_SOURCE_DOMAIN_Pos      (8UL)
#define IPCMAP_CHANNEL_SOURCE_DOMAIN_Msk      (0xFUL << IPCMAP_CHANNEL_SOURCE_DOMAIN_Pos)
#define IPCMAP_CHANNEL_SOURCE_ENABLE_Pos      (31UL)
#define IPCMAP_CHANNEL_SOURCE_ENABLE_Disabled (0x0UL)
#define IPCMAP_CHANNEL_SOURCE_ENABLE_Enabled  (0x1UL)
#define IPCMAP_CHANNEL_SINK_SINK_Pos          (0UL)
#define IPCMAP_CHANNEL_SINK_SINK_Msk          (0xFUL << IPCMAP_CHANNEL_SINK_SINK_Pos)
#define IPCMAP_CHANNEL_SINK_DOMAIN_Pos        (8UL)
#define IPCMAP_CHANNEL_SINK_DOMAIN_Msk        (0xFUL << IPCMAP_CHANNEL_SINK_DOMAIN_Pos)

typedef struct {
	__IM uint32_t RESERVED[256];
	__IOM NRF_IPCMAP_CHANNEL_Type CHANNEL[16];
} NRF_IPCMAP_Type;

#endif /* IPCMAP_CHANNEL_SOURCE_SOURCE_Msk */

#ifndef NRF_IPCMAP
#define NRF_IPCMAP ((NRF_IPCMAP_Type *)0x5F923000UL)
#endif

#ifndef IRQMAP_IRQ_SINK_PROCESSORID_Msk

typedef struct {
	__IOM uint32_t SINK;
} NRF_IRQMAP_IRQ_Type;

#define IRQMAP_IRQ_SINK_PROCESSORID_Pos (8UL)
#define IRQMAP_IRQ_SINK_PROCESSORID_Msk (0xFUL << IRQMAP_IRQ_SINK_PROCESSORID_Pos)

typedef struct {
	__IM uint32_t RESERVED[256];
	__IOM NRF_IRQMAP_IRQ_Type IRQ[480];
} NRF_IRQMAP_Type;

#endif /* IRQMAP_IRQ_SINK_PROCESSORID_Msk */

#ifndef NRF_IRQMAP
#define NRF_IRQMAP ((NRF_IRQMAP_Type *)0x5F924000UL)
#endif /* NRF_IRQMAP */

#ifndef GPIO_PIN_CNF_CTRLSEL_Pos

#define GPIO_PIN_CNF_CTRLSEL_Pos          (28UL)
#define GPIO_PIN_CNF_CTRLSEL_Msk          (0x7UL << GPIO_PIN_CNF_CTRLSEL_Pos)
#define GPIO_PIN_CNF_CTRLSEL_Min          (0x0UL)
#define GPIO_PIN_CNF_CTRLSEL_Max          (0x7UL)
#define GPIO_PIN_CNF_CTRLSEL_GPIO         (0x0UL)
#define GPIO_PIN_CNF_CTRLSEL_VPR          (0x1UL)
#define GPIO_PIN_CNF_CTRLSEL_GRC          (0x1UL)
#define GPIO_PIN_CNF_CTRLSEL_SecureDomain (0x2UL)
#define GPIO_PIN_CNF_CTRLSEL_PWM          (0x2UL)
#define GPIO_PIN_CNF_CTRLSEL_I3C          (0x2UL)
#define GPIO_PIN_CNF_CTRLSEL_Serial       (0x3UL)
#define GPIO_PIN_CNF_CTRLSEL_HSSPI        (0x3UL)
#define GPIO_PIN_CNF_CTRLSEL_RadioCore    (0x4UL)
#define GPIO_PIN_CNF_CTRLSEL_EXMIF        (0x4UL)
#define GPIO_PIN_CNF_CTRLSEL_CELL         (0x4UL)
#define GPIO_PIN_CNF_CTRLSEL_DTB          (0x6UL)
#define GPIO_PIN_CNF_CTRLSEL_TND          (0x7UL)

#endif /* GPIO_PIN_CNF_CTRLSEL_Pos */

#ifndef NRF_SPU110
#define NRF_SPU110 ((NRF_SPU_Type *)0x5F080000UL)
#endif
#ifndef NRF_SPU111
#define NRF_SPU111 ((NRF_SPU_Type *)0x5F090000UL)
#endif
#ifndef NRF_SPU120
#define NRF_SPU120 ((NRF_SPU_Type *)0x5F8C0000UL)
#endif
#ifndef NRF_SPU121
#define NRF_SPU121 ((NRF_SPU_Type *)0x5F8D0000UL)
#endif
#ifndef NRF_SPU122
#define NRF_SPU122 ((NRF_SPU_Type *)0x5F8E0000UL)
#endif
#ifndef NRF_SPU130
#define NRF_SPU130 ((NRF_SPU_Type *)0x5F900000UL)
#endif
#ifndef NRF_SPU131
#define NRF_SPU131 ((NRF_SPU_Type *)0x5F920000UL)
#endif
#ifndef NRF_SPU132
#define NRF_SPU132 ((NRF_SPU_Type *)0x5F980000UL)
#endif
#ifndef NRF_SPU133
#define NRF_SPU133 ((NRF_SPU_Type *)0x5F990000UL)
#endif
#ifndef NRF_SPU134
#define NRF_SPU134 ((NRF_SPU_Type *)0x5F9A0000UL)
#endif
#ifndef NRF_SPU135
#define NRF_SPU135 ((NRF_SPU_Type *)0x5F9B0000UL)
#endif
#ifndef NRF_SPU136
#define NRF_SPU136 ((NRF_SPU_Type *)0x5F9C0000UL)
#endif
#ifndef NRF_SPU137
#define NRF_SPU137 ((NRF_SPU_Type *)0x5F9D0000UL)
#endif

#ifndef NRF_PPIB110
#define NRF_PPIB110 ((NRF_PPIB_Type *)0x5F098000UL)
#endif
#ifndef NRF_PPIB120
#define NRF_PPIB120 ((NRF_PPIB_Type *)0x5F8EE000UL)
#endif
#ifndef NRF_PPIB121
#define NRF_PPIB121 ((NRF_PPIB_Type *)0x5F8EF000UL)
#endif
#ifndef NRF_PPIB130
#define NRF_PPIB130 ((NRF_PPIB_Type *)0x5F925000UL)
#endif
#ifndef NRF_PPIB131
#define NRF_PPIB131 ((NRF_PPIB_Type *)0x5F926000UL)
#endif
#ifndef NRF_PPIB132
#define NRF_PPIB132 ((NRF_PPIB_Type *)0x5F98D000UL)
#endif
#ifndef NRF_PPIB133
#define NRF_PPIB133 ((NRF_PPIB_Type *)0x5F99D000UL)
#endif
#ifndef NRF_PPIB134
#define NRF_PPIB134 ((NRF_PPIB_Type *)0x5F9AD000UL)
#endif
#ifndef NRF_PPIB135
#define NRF_PPIB135 ((NRF_PPIB_Type *)0x5F9BD000UL)
#endif
#ifndef NRF_PPIB136
#define NRF_PPIB136 ((NRF_PPIB_Type *)0x5F9CD000UL)
#endif
#ifndef NRF_PPIB137
#define NRF_PPIB137 ((NRF_PPIB_Type *)0x5F9DD000UL)
#endif

#ifndef NRF_MEMCONF120
#define NRF_MEMCONF120 ((NRF_MEMCONF_Type *)0x5F8C7000UL)
#endif
#ifndef NRF_MEMCONF130
#define NRF_MEMCONF130 ((NRF_MEMCONF_Type *)0x5F905000UL)
#endif

#ifndef MRAMC110_NMRAMWORDSIZE
#define MRAMC110_NMRAMWORDSIZE 128
#endif

#endif /* NRF_HALTIUM_XXAA */

#ifdef __cplusplus
}
#endif
#endif /* IRONSIDE_INTERNAL_MDK_H_ */
