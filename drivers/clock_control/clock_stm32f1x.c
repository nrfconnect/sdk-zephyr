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

/*
 * Select PLL source for STM32F1 Connectivity line devices (STM32F105xx and
 * STM32F107xx).
 * Both flags are defined in STM32Cube LL API. Keep only the selected one.
 */
#ifdef CONFIG_CLOCK_STM32_PLL_SRC_PLL2
#undef RCC_PREDIV1_SOURCE_HSE
#else
#undef RCC_PREDIV1_SOURCE_PLL2
#endif /* CONFIG_CLOCK_STM32_PLL_SRC_PLL2 */


/**
 * @brief fill in pll configuration structure
 */
void config_pll_init(LL_UTILS_PLLInitTypeDef *pllinit)
{
	/*
	 * PLLMUL on SOC_STM32F10X_DENSITY_DEVICE
	 * 2  -> LL_RCC_PLL_MUL_2  -> 0x00000000
	 * 3  -> LL_RCC_PLL_MUL_3  -> 0x00040000
	 * 4  -> LL_RCC_PLL_MUL_4  -> 0x00080000
	 * ...
	 * 16 -> LL_RCC_PLL_MUL_16 -> 0x00380000
	 *
	 * PLLMUL on SOC_STM32F10X_CONNECTIVITY_LINE_DEVICE
	 * 4  -> LL_RCC_PLL_MUL_4   -> 0x00080000
	 * ...
	 * 9  -> LL_RCC_PLL_MUL_9   -> 0x001C0000
	 * 13 -> LL_RCC_PLL_MUL_6_5 -> 0x00340000
	 */
	pllinit->PLLMul = ((CONFIG_CLOCK_STM32_PLL_MULTIPLIER - 2)
					<< RCC_CFGR_PLLMULL_Pos);

#ifdef CONFIG_SOC_STM32F10X_DENSITY_DEVICE
	/* PLL prediv */
#ifdef CONFIG_CLOCK_STM32_PLL_XTPRE
	/*
	 * SOC_STM32F10X_DENSITY_DEVICE:
	 * PLLXPTRE (depends on PLL source HSE)
	 * HSE/2 used as PLL source
	 */
	pllinit->Prediv = LL_RCC_PREDIV_DIV_2;
#else
	/*
	 * SOC_STM32F10X_DENSITY_DEVICE:
	 * PLLXPTRE (depends on PLL source HSE)
	 * HSE used as direct PLL source
	 */
	pllinit->Prediv = LL_RCC_PREDIV_DIV_1;
#endif /* CONFIG_CLOCK_STM32_PLL_XTPRE */
#else
	/*
	 * SOC_STM32F10X_CONNECTIVITY_LINE_DEVICE
	 * 1  -> LL_RCC_PREDIV_DIV_1  -> 0x00000000
	 * 2  -> LL_RCC_PREDIV_DIV_2  -> 0x00000001
	 * 3  -> LL_RCC_PREDIV_DIV_3  -> 0x00000002
	 * ...
	 * 16 -> LL_RCC_PREDIV_DIV_16 -> 0x0000000F
	 */
	pllinit->Prediv = CONFIG_CLOCK_STM32_PLL_PREDIV1 - 1;
#endif /* CONFIG_SOC_STM32F10X_DENSITY_DEVICE */
}

#endif /* CONFIG_CLOCK_STM32_SYSCLK_SRC_PLL */

/**
 * @brief Activate default clocks
 */
void config_enable_default_clocks(void)
{
	/* Nothing for now */
}

/**
 * @brief Function kept for driver genericity
 */
void LL_RCC_MSI_Disable(void)
{
	/* Do nothing */
}
