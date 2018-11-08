/*

Copyright (c) 2009-2018 ARM Limited. All rights reserved.

    SPDX-License-Identifier: Apache-2.0

Licensed under the Apache License, Version 2.0 (the License); you may
not use this file except in compliance with the License.
You may obtain a copy of the License at

    www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an AS IS BASIS, WITHOUT
WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

NOTICE: This file has been modified by Nordic Semiconductor ASA.

*/

/* NOTE: Template files (including this one) are application specific and therefore expected to 
   be copied into the application project folder prior to its use! */

#include <stdint.h>
#include <stdbool.h>
#include "nrf.h"
#include "system_nrf9160.h"

/*lint ++flb "Enter library region" */


#define __SYSTEM_CLOCK      (64000000UL)     /*!< nRF9160 Application core uses a fixed System Clock Frequency of 64MHz */


#if defined ( __CC_ARM )
    uint32_t SystemCoreClock __attribute__((used)) = __SYSTEM_CLOCK;  
#elif defined ( __ICCARM__ )
    __root uint32_t SystemCoreClock = __SYSTEM_CLOCK;
#elif defined ( __GNUC__ )
    uint32_t SystemCoreClock __attribute__((used)) = __SYSTEM_CLOCK;
#endif


void SystemCoreClockUpdate(void)
{
    SystemCoreClock = __SYSTEM_CLOCK;
}

void SystemInit(void)
{
    /* Set all ARM SAU regions to NonSecure if TrustZone extensions are enabled.
     * Nordic SPU should handle Secure Attribution tasks */
    #if defined (__ARM_FEATURE_CMSE) && (__ARM_FEATURE_CMSE == 3U)
        SAU->CTRL |= (1 << SAU_CTRL_ALLNS_Pos);
    #endif
    
    #if !defined(NRF_TRUSTZONE_NONSECURE)
      /* Make sure UICR->HFXOSRC is set */
      if ((NRF_UICR_S->HFXOSRC & UICR_HFXOSRC_HFXOSRC_Msk) != UICR_HFXOSRC_HFXOSRC_TCXO) {
          /* Wait for pending NVMC operations to finish */
          while (NRF_NVMC_S->READY != NVMC_READY_READY_Ready);
          
          /* Enable write mode in NVMC */
          NRF_NVMC_S->CONFIG = NVMC_CONFIG_WEN_Wen;
          while (NRF_NVMC_S->READY != NVMC_READY_READY_Ready);
          
          /* Write new value to UICR->HFXOSRC */
          NRF_UICR_S->HFXOSRC = (NRF_UICR_S->HFXOSRC & ~UICR_HFXOSRC_HFXOSRC_Msk) | UICR_HFXOSRC_HFXOSRC_TCXO;
          while (NRF_NVMC_S->READY != NVMC_READY_READY_Ready);
                
          /* Enable read mode in NVMC */
          NRF_NVMC_S->CONFIG = NVMC_CONFIG_WEN_Ren;
          while (NRF_NVMC_S->READY != NVMC_READY_READY_Ready);
          
          /* Reset to apply clock select update */
          NVIC_SystemReset();
      }
    #endif
    
    /* Enable the FPU if the compiler used floating point unit instructions. __FPU_USED is a MACRO defined by the
     * compiler. Since the FPU consumes energy, remember to disable FPU use in the compiler if floating point unit
     * operations are not used in your code. */
    #if (__FPU_USED == 1)
        SCB->CPACR |= (3UL << 20) | (3UL << 22);
        __DSB();
        __ISB();
    #endif
    
    SystemCoreClockUpdate();
}

/*lint --flb "Leave library region" */
