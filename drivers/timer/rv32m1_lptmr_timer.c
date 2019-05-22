/*
 * Copyright (c) 2018 Foundries.io Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <misc/util.h>
#include <system_timer.h>
#include <soc.h>

/*
 * This is just a getting started point.
 *
 * Assumptions and limitations:
 *
 * - system clock based on an LPTMR instance, clocked by SIRC output
 *   SIRCDIV3, prescaler divide-by-1, SIRC at 8MHz
 * - no tickless
 */

#define CYCLES_PER_SEC  CONFIG_SYS_CLOCK_HW_CYCLES_PER_SEC
#define CYCLES_PER_TICK (CYCLES_PER_SEC / CONFIG_SYS_CLOCK_TICKS_PER_SEC)

/*
 * As a simplifying assumption, we only support a clock ticking at the
 * SIRC reset rate of 8MHz.
 */
#if MHZ(8) != CYCLES_PER_SEC
#error "system timer misconfiguration; unsupported clock rate"
#endif

#define SYSTEM_TIMER_INSTANCE \
	((LPTMR_Type *)(DT_OPENISA_RV32M1_LPTMR_SYSTEM_LPTMR_BASE_ADDRESS))
#define SYSTEM_TIMER_IRQ_PRIO 0

#define SIRC_RANGE_8MHZ      SCG_SIRCCFG_RANGE(1)
#define SIRCDIV3_DIVIDE_BY_1 1
#define PCS_SOURCE_SIRCDIV3  0

struct device;	       /* forward declaration; type is not used. */

static volatile u32_t cycle_count;

static void lptmr_irq_handler(struct device *unused)
{
	ARG_UNUSED(unused);

	SYSTEM_TIMER_INSTANCE->CSR |= LPTMR_CSR_TCF(1); /* Rearm timer. */
	cycle_count += CYCLES_PER_TICK;          /* Track cycles. */
	z_clock_announce(1);                     /* Poke the scheduler. */
}

int z_clock_driver_init(struct device *unused)
{
	u32_t csr, psr, sircdiv; /* LPTMR registers */

	ARG_UNUSED(unused);
	IRQ_CONNECT(DT_OPENISA_RV32M1_LPTMR_SYSTEM_LPTMR_IRQ,
		    SYSTEM_TIMER_IRQ_PRIO, lptmr_irq_handler, NULL, 0);

	if ((SCG->SIRCCSR & SCG_SIRCCSR_SIRCEN_MASK) == SCG_SIRCCSR_SIRCEN(0)) {
		/*
		 * SIRC is on by default, so something else turned it off.
		 *
		 * This is incompatible with this driver, which is SIRC-based.
		 */
		return -ENODEV;
	}

	/* Disable the timer and clear any pending IRQ. */
	csr = SYSTEM_TIMER_INSTANCE->CSR;
	csr &= ~LPTMR_CSR_TEN(0);
	csr |= LPTMR_CSR_TFC(1);
	SYSTEM_TIMER_INSTANCE->CSR = csr;

	/*
	 * Set up the timer clock source and configure the timer.
	 */

	/*
	 * SIRCDIV3 is the SIRC divider for LPTMR (SoC dependent).
	 * Pass it directly through without any divider.
	 */
	sircdiv = SCG->SIRCDIV;
	sircdiv &= ~SCG_SIRCDIV_SIRCDIV3_MASK;
	sircdiv |= SCG_SIRCDIV_SIRCDIV3(SIRCDIV3_DIVIDE_BY_1);
	SCG->SIRCDIV = sircdiv;
	/*
	 * TMS = 0: time counter mode, not pulse counter
	 * TCF = 0: reset counter register on reaching compare value
	 * TDRE = 0: disable DMA request
	 */
	csr &= ~(LPTMR_CSR_TMS(1) | LPTMR_CSR_TFC(1) | LPTMR_CSR_TDRE(1));
	/*
	 * TIE = 1: enable interrupt
	 */
	csr |= LPTMR_CSR_TIE(1);
	SYSTEM_TIMER_INSTANCE->CSR = csr;
	/*
	 * PCS = 0: clock source is SIRCDIV3 (SoC dependent)
	 * PBYP = 1: bypass the prescaler
	 */
	psr = SYSTEM_TIMER_INSTANCE->PSR;
	psr &= ~LPTMR_PSR_PCS_MASK;
	psr |= (LPTMR_PSR_PBYP(1) | LPTMR_PSR_PCS(PCS_SOURCE_SIRCDIV3));
	SYSTEM_TIMER_INSTANCE->PSR = psr;

	/*
	 * Set compare register to the proper tick count. The check
	 * here makes sure SIRC is left at its default reset value to
	 * make the defconfig setting work properly.
	 *
	 * TODO: be smarter to meet arbitrary Kconfig settings.
	 */
	if ((SCG->SIRCCFG & SCG_SIRCCFG_RANGE_MASK) != SIRC_RANGE_8MHZ) {
		return -EINVAL;
	}
	SYSTEM_TIMER_INSTANCE->CMR = CYCLES_PER_TICK;

	/*
	 * Enable interrupts and the timer. There's no need to clear the
	 * TFC bit in the csr variable, as it's already clear.
	 */
	irq_enable(DT_OPENISA_RV32M1_LPTMR_SYSTEM_LPTMR_IRQ);
	csr = SYSTEM_TIMER_INSTANCE->CSR;
	csr |= LPTMR_CSR_TEN(1);
	SYSTEM_TIMER_INSTANCE->CSR = csr;
	return 0;
}

u32_t z_timer_cycle_get_32(void)
{
	return cycle_count + SYSTEM_TIMER_INSTANCE->CNR;
}

/*
 * Since we're not tickless, this is identically zero.
 */
u32_t z_clock_elapsed(void)
{
	return 0;
}
