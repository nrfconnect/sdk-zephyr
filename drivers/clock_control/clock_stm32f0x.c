/*
 *
 * Copyright (c) 2017 Linaro Limited.
 *
 * SPDX-License-Identifier: Apache-2.0
 */


#include <soc.h>
#include <soc_registers.h>
#include <clock_control.h>
#include <misc/util.h>
#include <clock_control/stm32_clock_control.h>
#include "clock_stm32_ll_common.h"


#ifdef CONFIG_CLOCK_STM32_SYSCLK_SRC_PLL

/**
 * @brief fill in pll configuration structure
 */
void config_pll_init(LL_UTILS_PLLInitTypeDef *pllinit)
{
	/*
	 * PLL MUL
	 * 2  -> LL_RCC_PLL_MUL_2  -> 0x00000000
	 * 3  -> LL_RCC_PLL_MUL_3  -> 0x00040000
	 * 4  -> LL_RCC_PLL_MUL_4  -> 0x00080000
	 * ...
	 * 16 -> LL_RCC_PLL_MUL_16 -> 0x00380000
	 */
	pllinit->PLLMul = ((CONFIG_CLOCK_STM32_PLL_MULTIPLIER - 2)
					<< RCC_CFGR_PLLMUL_Pos);
#if defined(RCC_PLLSRC_PREDIV1_SUPPORT)
	/* PREDIV support is a specific RCC configuration present on */
	/* following SoCs: STM32F070x6, STM32F070xB and STM32F030xC */
	/* cf Reference manual for more details */
#if defined(CONFIG_CLOCK_STM32_PLL_SRC_HSI)
	pllinit->PLLDiv = LL_RCC_PLLSOURCE_HSI;
#else
	/*
	 * PLL DIV
	 * 1  -> LL_RCC_PLLSOURCE_HSE_DIV_1  -> 0x00010000
	 * 2  -> LL_RCC_PLLSOURCE_HSE_DIV_2  -> 0x00010001
	 * 3  -> LL_RCC_PLLSOURCE_HSE_DIV_3  -> 0x00010002
	 * ...
	 * 16 -> LL_RCC_PLLSOURCE_HSE_DIV_16 -> 0x0001000F
	 */
	pllinit->PLLDiv = (RCC_CFGR_PLLSRC_HSE_PREDIV |
					(CONFIG_CLOCK_STM32_PLL_PREDIV1 - 1));
#endif /* CONFIG_CLOCK_STM32_PLL_SRC_HSI */
#else
	/*
	 * PLL Prediv
	 * 1  -> LL_RCC_PREDIV_DIV_1  -> 0x00000000
	 * 2  -> LL_RCC_PREDIV_DIV_2  -> 0x00000001
	 * 3  -> LL_RCC_PREDIV_DIV_3  -> 0x00000002
	 * ...
	 * 16 -> LL_RCC_PREDIV_DIV_16 -> 0x0000000F
	 */
	pllinit->Prediv = CONFIG_CLOCK_STM32_PLL_PREDIV - 1;
#endif /* RCC_PLLSRC_PREDIV1_SUPPORT */
}

#endif /* CONFIG_CLOCK_STM32_SYSCLK_SRC_PLL */

/**
 * @brief Activate default clocks
 */
void config_enable_default_clocks(void)
{
#if defined(CONFIG_EXTI_STM32) || defined(CONFIG_USB_DC_STM32)
	/* Enable System Configuration Controller clock. */
	LL_APB1_GRP2_EnableClock(LL_APB1_GRP2_PERIPH_SYSCFG);
#endif
}

/**
 * @brief Function kept for driver genericity
 */
void LL_RCC_MSI_Disable(void)
{
	/* Do nothing */
}
