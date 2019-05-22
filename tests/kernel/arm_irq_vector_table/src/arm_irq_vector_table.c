/*
 * Copyright (c) 2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <ztest.h>
#include <arch/cpu.h>
#include <arch/arm/cortex_m/cmsis.h>
#include <linker/sections.h>


/*
 * Offset (starting from the beginning of the vector table)
 * of the location where the ISRs will be manually installed.
 */
#define _ISR_OFFSET 0

#if defined(CONFIG_SOC_SERIES_NRF52X)
/* The customized solution for nRF52X-based platforms
 * requires that the POWER_CLOCK_IRQn line equals 0.
 */
BUILD_ASSERT_MSG(POWER_CLOCK_IRQn == 0,
	"POWER_CLOCK_IRQn != 0. Consider rework manual vector table.");

/* The customized solution for nRF52X-based platforms
 * requires that the RTC1 IRQ line equals 17.
 */
BUILD_ASSERT_MSG(RTC1_IRQn == 17,
	 "RTC1_IRQn != 17. Consider rework manual vector table.");

#undef _ISR_OFFSET
/* Interrupt line 0 is used by POWER_CLOCK */
#define _ISR_OFFSET 1

#elif defined(CONFIG_SOC_SERIES_NRF91X)
/* The customized solution for nRF91X-based platforms
 * requires that the POWER_CLOCK_IRQn line equals 5.
 */
BUILD_ASSERT_MSG(CLOCK_POWER_IRQn == 5,
	"POWER_CLOCK_IRQn != 5."
	"Consider rework manual vector table.");

/* The customized solution for nRF91X-based platforms
 * requires that the RTC1 IRQ line equals 21.
 */
BUILD_ASSERT_MSG(RTC1_IRQn == 21,
	 "RTC1_IRQn != 21. Consider rework manual vector table.");

#undef _ISR_OFFSET
/* Interrupt lines 8-10 is the first set of consecutive interrupts implemented
 * in nRF9160 SOC.
 */
#define _ISR_OFFSET 8

#endif /* CONFIG_SOC_SERIES_NRF52X */


struct k_sem sem[3];

/**
 *
 * @brief ISR for IRQ0
 *
 * @return N/A
 */

void isr0(void)
{
	printk("%s ran!\n", __func__);
	k_sem_give(&sem[0]);
	_IntExit();
}

/**
 *
 * @brief ISR for IRQ1
 *
 * @return N/A
 */

void isr1(void)
{
	printk("%s ran!\n", __func__);
	k_sem_give(&sem[1]);
	_IntExit();
}

/**
 *
 * @brief ISR for IRQ2
 *
 * @return N/A
 */

void isr2(void)
{
	printk("%s ran!\n", __func__);
	k_sem_give(&sem[2]);
	_IntExit();
}

/**
 * @defgroup kernel_interrupt_tests Interrupts
 * @ingroup all_tests
 * @{
 */


/**
 * @brief Test installation of ISRs directly in the vector table
 *
 * @details Test validates the arm irq vector table. We create a
 * irq vector table with the address of the interrupt handler. We write
 * into the Software Trigger Interrupt Register(STIR) or calling
 * NVIC_SetPendingIRQ(), to trigger the pending interrupt. And we check
 * that the corresponding interrupt handler is getting called or not.
 *
 * @see irq_enable(), z_irq_priority_set(), NVIC_SetPendingIRQ()
 *
 */
void test_arm_irq_vector_table(void)
{
	printk("Test Cortex-M IRQs installed directly in the vector table\n");

	for (int ii = 0; ii < 3; ii++) {
		irq_enable(_ISR_OFFSET + ii);
		z_irq_priority_set(_ISR_OFFSET + ii, 0, 0);
		k_sem_init(&sem[ii], 0, UINT_MAX);
	}

	zassert_true((k_sem_take(&sem[0], K_NO_WAIT) ||
		      k_sem_take(&sem[1], K_NO_WAIT) ||
		      k_sem_take(&sem[2], K_NO_WAIT)), NULL);

	for (int ii = 0; ii < 3; ii++) {
#if defined(CONFIG_SOC_TI_LM3S6965_QEMU)
		/* the QEMU does not simulate the
		 * STIR register: this is a workaround
		 */
		NVIC_SetPendingIRQ(_ISR_OFFSET + ii);
#else
		NVIC->STIR = _ISR_OFFSET + ii;
#endif
	}

	zassert_false((k_sem_take(&sem[0], K_NO_WAIT) ||
		       k_sem_take(&sem[1], K_NO_WAIT) ||
		       k_sem_take(&sem[2], K_NO_WAIT)), NULL);

}

typedef void (*vth)(void); /* Vector Table Handler */

#if defined(CONFIG_SOC_SERIES_NRF52X) || defined(CONFIG_SOC_SERIES_NRF91X)
/* nRF52X- and nRF91X-based platforms employ a Hardware RTC peripheral
 * to implement the Kernel system timer, instead of the ARM Cortex-M
 * SysTick. Therefore, a pointer to the timer ISR needs to be added in
 * the custom vector table to handle the timer "tick" interrupts.
 *
 * The same applies to the CLOCK Control peripheral, which may trigger
 * IRQs that would need to be serviced.
 */
void rtc1_nrf_isr(void);
void nrf_power_clock_isr(void);
#if defined(CONFIG_SOC_SERIES_NRF52X)
vth __irq_vector_table _irq_vector_table[RTC1_IRQn + 1] = {
	nrf_power_clock_isr,
	isr0, isr1, isr2,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	rtc1_nrf_isr
};
#elif defined(CONFIG_SOC_SERIES_NRF91X)
vth __irq_vector_table _irq_vector_table[RTC1_IRQn + 1] = {
	0, 0, 0, 0, 0, nrf_power_clock_isr, 0, 0,
	isr0, isr1, isr2,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	rtc1_nrf_isr
};
#endif
#else
vth __irq_vector_table _irq_vector_table[CONFIG_NUM_IRQS] = {
	isr0, isr1, isr2
};
#endif /* CONFIG_SOC_SERIES_NRF52X || CONFIG_SOC_SERIES_NRF91X */

/**
 * @}
 */
