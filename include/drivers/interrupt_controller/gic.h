/*
 * Copyright (c) 2019 Stephanos Ioannidis <root@stephanos.io>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_INCLUDE_DRIVERS_GIC_H_
#define ZEPHYR_INCLUDE_DRIVERS_GIC_H_

#include <arch/cpu.h>

/*
 * GIC Register Interface Base Addresses
 */

#define GIC_DIST_BASE	DT_INST_0_ARM_GIC_BASE_ADDRESS_0
#define GIC_CPU_BASE	DT_INST_0_ARM_GIC_BASE_ADDRESS_1

/*
 * GIC Distributor Interface
 */

/*
 * 0x000  Distributor Control Register
 * v1		ICDDCR
 * v2/v3	GICD_CTLR
 */
#define	GICD_CTLR		(GIC_DIST_BASE +   0x0)

/*
 * 0x004  Interrupt Controller Type Register
 * v1		ICDICTR
 * v2/v3	GICD_TYPER
 */
#define	GICD_TYPER		(GIC_DIST_BASE +   0x4)

/*
 * 0x008  Distributor Implementer Identification Register
 * v1		ICDIIDR
 * v2/v3	GICD_IIDR
 */
#define	GICD_IIDR		(GIC_DIST_BASE +   0x8)

/*
 * 0x080  Interrupt Group Registers
 * v1		ICDISRn
 * v2/v3	GICD_IGROUPRn
 */
#define	GICD_IGROUPRn		(GIC_DIST_BASE +  0x80)

/*
 * 0x100  Interrupt Set-Enable Reigsters
 * v1		ICDISERn
 * v2/v3	GICD_ISENABLERn
 */
#define	GICD_ISENABLERn		(GIC_DIST_BASE + 0x100)

/*
 * 0x180  Interrupt Clear-Enable Registers
 * v1		ICDICERn
 * v2/v3	GICD_ICENABLERn
 */
#define	GICD_ICENABLERn		(GIC_DIST_BASE + 0x180)

/*
 * 0x200  Interrupt Set-Pending Registers
 * v1		ICDISPRn
 * v2/v3	GICD_ISPENDRn
 */
#define	GICD_ISPENDRn		(GIC_DIST_BASE + 0x200)

/*
 * 0x280  Interrupt Clear-Pending Registers
 * v1		ICDICPRn
 * v2/v3	GICD_ICPENDRn
 */
#define	GICD_ICPENDRn		(GIC_DIST_BASE + 0x280)

/*
 * 0x300  Interrupt Set-Active Registers
 * v1		ICDABRn
 * v2/v3	GICD_ISACTIVERn
 */
#define	GICD_ISACTIVERn		(GIC_DIST_BASE + 0x300)

#if CONFIG_GIC_VER >= 2
/*
 * 0x380  Interrupt Clear-Active Registers
 * v2/v3	GICD_ICACTIVERn
 */
#define	GICD_ICACTIVERn		(GIC_DIST_BASE + 0x380)
#endif

/*
 * 0x400  Interrupt Priority Registers
 * v1		ICDIPRn
 * v2/v3	GICD_IPRIORITYRn
 */
#define	GICD_IPRIORITYRn	(GIC_DIST_BASE + 0x400)

/*
 * 0x800  Interrupt Processor Targets Registers
 * v1		ICDIPTRn
 * v2/v3	GICD_ITARGETSRn
 */
#define	GICD_ITARGETSRn		(GIC_DIST_BASE + 0x800)

/*
 * 0xC00  Interrupt Configuration Registers
 * v1		ICDICRn
 * v2/v3	GICD_ICFGRn
 */
#define	GICD_ICFGRn		(GIC_DIST_BASE + 0xc00)

/*
 * 0xF00  Software Generated Interrupt Register
 * v1		ICDSGIR
 * v2/v3	GICD_SGIR
 */
#define	GICD_SGIR		(GIC_DIST_BASE + 0xf00)

/*
 * GIC CPU Interface
 */

#if CONFIG_GIC_VER <= 2

/*
 * 0x0000  CPU Interface Control Register
 * v1		ICCICR
 * v2/v3	GICC_CTLR
 */
#define GICC_CTLR		(GIC_CPU_BASE +    0x0)

/*
 * 0x0004  Interrupt Priority Mask Register
 * v1		ICCPMR
 * v2/v3	GICC_PMR
 */
#define GICC_PMR		(GIC_CPU_BASE +    0x4)

/*
 * 0x0008  Binary Point Register
 * v1		ICCBPR
 * v2/v3	GICC_BPR
 */
#define GICC_BPR		(GIC_CPU_BASE +    0x8)

/*
 * 0x000C  Interrupt Acknowledge Register
 * v1		ICCIAR
 * v2/v3	GICC_IAR
 */
#define GICC_IAR		(GIC_CPU_BASE +    0xc)

/*
 * 0x0010  End of Interrupt Register
 * v1		ICCEOIR
 * v2/v3	GICC_EOIR
 */
#define GICC_EOIR		(GIC_CPU_BASE +   0x10)


/*
 * Helper Constants
 */

#define	GIC_SPI_INT_BASE	32

/* GICC_CTLR */
#define GICC_CTLR_ENABLEGRP0	BIT(0)
#define GICC_CTLR_ENABLEGRP1	BIT(1)

#define GICC_CTLR_ENABLE_MASK	(GICC_CTLR_ENABLEGRP0 | GICC_CTLR_ENABLEGRP1)

#if defined(CONFIG_GIC_V2)

#define GICC_CTLR_FIQBYPDISGRP0	BIT(5)
#define GICC_CTLR_IRQBYPDISGRP0	BIT(6)
#define GICC_CTLR_FIQBYPDISGRP1	BIT(7)
#define GICC_CTLR_IRQBYPDISGRP1	BIT(8)

#define GICC_CTLR_BYPASS_MASK	(GICC_CTLR_FIQBYPDISGRP0 | \
				 GICC_CTLR_IRQBYPDISGRP1 | \
				 GICC_CTLR_FIQBYPDISGRP1 | \
				 GICC_CTLR_IRQBYPDISGRP1)

#endif /* CONFIG_GIC_V2 */

/* GICC_IAR */
#define	GICC_IAR_SPURIOUS	1023

/* GICC_ICFGR */
#define GICC_ICFGR_MASK		BIT_MASK(2)
#define GICC_ICFGR_TYPE		BIT(1)

#endif /* CONFIG_GIC_VER <= 2 */

#endif /* ZEPHYR_INCLUDE_DRIVERS_GIC_H_ */
