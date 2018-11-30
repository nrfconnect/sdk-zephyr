/***************************************************************************//**
 * @file em_system.h
 * @brief System API
 * @version 5.6.0
 *******************************************************************************
 * # License
 * <b>Copyright 2016 Silicon Laboratories, Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 *
 * DISCLAIMER OF WARRANTY/LIMITATION OF REMEDIES: Silicon Labs has no
 * obligation to support this Software. Silicon Labs is providing the
 * Software "AS IS", with no express or implied warranties of any kind,
 * including, but not limited to, any implied warranties of merchantability
 * or fitness for any particular purpose or warranties against infringement
 * of any proprietary rights of a third party.
 *
 * Silicon Labs will not be liable for any consequential, incidental, or
 * special damages, or any other relief, or for any claim by any third party,
 * arising from your use of this Software.
 *
 ******************************************************************************/

#ifndef EM_SYSTEM_H
#define EM_SYSTEM_H

#include <stdbool.h>
#include "em_device.h"

#ifdef __cplusplus
extern "C" {
#endif

/***************************************************************************//**
 * @addtogroup emlib
 * @{
 ******************************************************************************/

/***************************************************************************//**
 * @addtogroup SYSTEM
 * @brief System API
 * @details
 *  This module contains functions to read information such as RAM and Flash size,
 *  device unique ID, chip revision, family, and part number from @ref DEVINFO and
 *  @ref SCB blocks. Functions to configure and read status from FPU are available for
 *  compatible devices.
 * @{
 ******************************************************************************/

/*******************************************************************************
 ********************************   ENUMS   ************************************
 ******************************************************************************/

/** Family identifiers. */
typedef enum {
/* New style family #defines */
#if defined(_DEVINFO_PART_DEVICE_FAMILY_EFM32G)
  systemPartFamilyEfm32Gecko   = _DEVINFO_PART_DEVICE_FAMILY_EFM32G,      /**< EFM32 Gecko Device Family. */
#endif
#if defined(_DEVINFO_PART_DEVICE_FAMILY_EFM32GG)
  systemPartFamilyEfm32Giant   = _DEVINFO_PART_DEVICE_FAMILY_EFM32GG,     /**< EFM32 Giant Gecko Series 0 Device Family. */
#endif
#if defined(_DEVINFO_PART_DEVICE_FAMILY_EFM32GG11B)
  systemPartFamilyEfm32Giant11B = _DEVINFO_PART_DEVICE_FAMILY_EFM32GG11B, /**< EFM32 Giant Gecko Series 1 Configuration 1 Basic Device Family. */
#endif
#if defined(_DEVINFO_PART_DEVICE_FAMILY_EFM32TG)
  systemPartFamilyEfm32Tiny    = _DEVINFO_PART_DEVICE_FAMILY_EFM32TG,     /**< EFM32 Tiny Gecko Device Family. */
#endif
#if defined(_DEVINFO_PART_DEVICE_FAMILY_EFM32TG11B)
  systemPartFamilyEfm32Tiny11B = _DEVINFO_PART_DEVICE_FAMILY_EFM32TG11B,  /**< EFM32 Tiny Gecko 11 Device Family. */
#endif
#if defined(_DEVINFO_PART_DEVICE_FAMILY_EFM32LG)
  systemPartFamilyEfm32Leopard = _DEVINFO_PART_DEVICE_FAMILY_EFM32LG,     /**< EFM32 Leopard Gecko Device Family. */
#endif
#if defined(_DEVINFO_PART_DEVICE_FAMILY_EFM32WG)
  systemPartFamilyEfm32Wonder  = _DEVINFO_PART_DEVICE_FAMILY_EFM32WG,     /**< EFM32 Wonder Gecko Device Family. */
#endif
#if defined(_DEVINFO_PART_DEVICE_FAMILY_EFM32ZG)
  systemPartFamilyEfm32Zero    = _DEVINFO_PART_DEVICE_FAMILY_EFM32ZG,     /**< EFM32 Zero Gecko Device Family. */
#endif
#if defined(_DEVINFO_PART_DEVICE_FAMILY_EFM32HG)
  systemPartFamilyEfm32Happy   = _DEVINFO_PART_DEVICE_FAMILY_EFM32HG,     /**< EFM32 Happy Gecko Device Family. */
#endif
#if defined(_DEVINFO_PART_DEVICE_FAMILY_EFM32PG1B)
  systemPartFamilyEfm32Pearl1B = _DEVINFO_PART_DEVICE_FAMILY_EFM32PG1B,   /**< EFM32 Pearl Gecko Series 1 Configuration 1 Basic Device Family. */
#endif
#if defined(_DEVINFO_PART_DEVICE_FAMILY_EFM32JG1B)
  systemPartFamilyEfm32Jade1B  = _DEVINFO_PART_DEVICE_FAMILY_EFM32JG1B,   /**< EFM32 Jade Gecko Series 1 Configuration 1 Basic Device Family. */
#endif
#if defined(_DEVINFO_PART_DEVICE_FAMILY_EFM32PG12B)
  systemPartFamilyEfm32Pearl12B = _DEVINFO_PART_DEVICE_FAMILY_EFM32PG12B, /**< EFM32 Pearl Gecko Series 1 Configuration 2 Basic Device Family. */
#endif
#if defined(_DEVINFO_PART_DEVICE_FAMILY_EFM32JG12B)
  systemPartFamilyEfm32Jade12B  = _DEVINFO_PART_DEVICE_FAMILY_EFM32JG12B, /**< EFM32 Jade Gecko Series 1 Configuration 2 Basic Device Family. */
#endif
#if defined(_DEVINFO_PART_DEVICE_FAMILY_EZR32WG)
  systemPartFamilyEzr32Wonder  = _DEVINFO_PART_DEVICE_FAMILY_EZR32WG,     /**< EZR32 Wonder Device Family. */
#endif
#if defined(_DEVINFO_PART_DEVICE_FAMILY_EZR32LG)
  systemPartFamilyEzr32Leopard = _DEVINFO_PART_DEVICE_FAMILY_EZR32LG,     /**< EZR32 Leopard Device Family. */
#endif
#if defined(_DEVINFO_PART_DEVICE_FAMILY_EZR32HG)
  systemPartFamilyEzr32Happy   = _DEVINFO_PART_DEVICE_FAMILY_EZR32HG,     /**< EZR32 Happy Device Family. */
#endif
#if defined(_DEVINFO_PART_DEVICE_FAMILY_EFR32MG1P)
  systemPartFamilyMighty1P = _DEVINFO_PART_DEVICE_FAMILY_EFR32MG1P,       /**< EFR32 Mighty Gecko Series 1 Configuration 1 Premium Device Family. */
#endif
#if defined(_DEVINFO_PART_DEVICE_FAMILY_EFR32MG1B)
  systemPartFamilyMighty1B = _DEVINFO_PART_DEVICE_FAMILY_EFR32MG1B,       /**< EFR32 Mighty Gecko Series 1 Configuration 1 Basic Device Family. */
#endif
#if defined(_DEVINFO_PART_DEVICE_FAMILY_EFR32MG1V)
  systemPartFamilyMighty1V = _DEVINFO_PART_DEVICE_FAMILY_EFR32MG1V,       /**< EFR32 Mighty Gecko Series 1 Configuration 1 Value Device Family. */
#endif
#if defined(_DEVINFO_PART_DEVICE_FAMILY_EFR32BG1P)
  systemPartFamilyBlue1P   = _DEVINFO_PART_DEVICE_FAMILY_EFR32BG1P,       /**< EFR32 Blue Gecko Series 1 Configuration 1 Premium Device Family. */
#endif
#if defined(_DEVINFO_PART_DEVICE_FAMILY_EFR32BG1B)
  systemPartFamilyBlue1B   = _DEVINFO_PART_DEVICE_FAMILY_EFR32BG1B,       /**< EFR32 Blue Gecko Series 1 Configuration 1 Basic Device Family. */
#endif
#if defined(_DEVINFO_PART_DEVICE_FAMILY_EFR32BG1V)
  systemPartFamilyBlue1V   = _DEVINFO_PART_DEVICE_FAMILY_EFR32BG1V,       /**< EFR32 Blue Gecko Series 1 Configuration 1 Value Device Family. */
#endif
#if defined(_DEVINFO_PART_DEVICE_FAMILY_EFR32FG1P)
  systemPartFamilyFlex1P   = _DEVINFO_PART_DEVICE_FAMILY_EFR32FG1P,       /**< EFR32 Flex Gecko Series 1 Configuration 1 Premium Device Family. */
#endif
#if defined(_DEVINFO_PART_DEVICE_FAMILY_EFR32FG1B)
  systemPartFamilyFlex1B   = _DEVINFO_PART_DEVICE_FAMILY_EFR32FG1B,       /**< EFR32 Flex Gecko Series 1 Configuration 1 Basic Device Family. */
#endif
#if defined(_DEVINFO_PART_DEVICE_FAMILY_EFR32FG1V)
  systemPartFamilyFlex1V   = _DEVINFO_PART_DEVICE_FAMILY_EFR32FG1V,       /**< EFR32 Flex Gecko Series 1 Configuration 1 Value Device Family. */
#endif
#if defined(_DEVINFO_PART_DEVICE_FAMILY_EFR32MG2P)
  systemPartFamilyMighty2P = _DEVINFO_PART_DEVICE_FAMILY_EFR32MG2P,       /**< EFR32 Mighty Gecko Series 1 Configuration 2 Premium Device Family. */
#endif
#if defined(_DEVINFO_PART_DEVICE_FAMILY_EFR32MG12P)
  systemPartFamilyMighty12P = _DEVINFO_PART_DEVICE_FAMILY_EFR32MG12P,     /**< EFR32 Mighty Gecko Series 1 Configuration 2 Premium Device Family. */
#endif
#if defined(_DEVINFO_PART_DEVICE_FAMILY_EFR32MG12B)
  systemPartFamilyMighty12B = _DEVINFO_PART_DEVICE_FAMILY_EFR32MG12B,     /**< EFR32 Mighty Gecko Series 1 Configuration 2 Basic Device Family. */
#endif
#if defined(_DEVINFO_PART_DEVICE_FAMILY_EFR32MG12V)
  systemPartFamilyMighty12V = _DEVINFO_PART_DEVICE_FAMILY_EFR32MG12V,     /**< EFR32 Mighty Gecko Series 1 Configuration 2 Value Device Family. */
#endif
#if defined(_DEVINFO_PART_DEVICE_FAMILY_EFR32BG12P)
  systemPartFamilyBlue12P   = _DEVINFO_PART_DEVICE_FAMILY_EFR32BG12P,     /**< EFR32 Blue Gecko Series 1 Configuration 2 Premium Device Family. */
#endif
#if defined(_DEVINFO_PART_DEVICE_FAMILY_EFR32BG12B)
  systemPartFamilyBlue12B   = _DEVINFO_PART_DEVICE_FAMILY_EFR32BG12B,     /**< EFR32 Blue Gecko Series 1 Configuration 2 Basic Device Family. */
#endif
#if defined(_DEVINFO_PART_DEVICE_FAMILY_EFR32BG12V)
  systemPartFamilyBlue12V   = _DEVINFO_PART_DEVICE_FAMILY_EFR32BG12V,     /**< EFR32 Blue Gecko Series 1 Configuration 2 Value Device Family. */
#endif
#if defined(_DEVINFO_PART_DEVICE_FAMILY_EFR32FG12P)
  systemPartFamilyFlex12P   = _DEVINFO_PART_DEVICE_FAMILY_EFR32FG12P,     /**< EFR32 Flex Gecko Series 1 Configuration 2 Premium Device Family. */
#endif
#if defined(_DEVINFO_PART_DEVICE_FAMILY_EFR32FG12B)
  systemPartFamilyFlex12B   = _DEVINFO_PART_DEVICE_FAMILY_EFR32FG12B,     /**< EFR32 Flex Gecko Series 1 Configuration 2 Basic Device Family. */
#endif
#if defined(_DEVINFO_PART_DEVICE_FAMILY_EFR32FG12V)
  systemPartFamilyFlex12V   = _DEVINFO_PART_DEVICE_FAMILY_EFR32FG12V,     /**< EFR32 Flex Gecko Series 1 Configuration 2 Value Device Family. */
#endif
#if defined(_DEVINFO_PART_DEVICE_FAMILY_EFR32MG13P)
  systemPartFamilyMighty13P = _DEVINFO_PART_DEVICE_FAMILY_EFR32MG13P,     /**< EFR32 Mighty Gecko Series 1 Configuration 3 Premium Device Family. */
#endif
#if defined(_DEVINFO_PART_DEVICE_FAMILY_EFR32MG13B)
  systemPartFamilyMighty13B = _DEVINFO_PART_DEVICE_FAMILY_EFR32MG13B,     /**< EFR32 Mighty Gecko Series 1 Configuration 3 Basic Device Family. */
#endif
#if defined(_DEVINFO_PART_DEVICE_FAMILY_EFR32MG13V)
  systemPartFamilyMighty13V = _DEVINFO_PART_DEVICE_FAMILY_EFR32MG13V,     /**< EFR32 Mighty Gecko Series 1 Configuration 3 Value Device Family. */
#endif
#if defined(_DEVINFO_PART_DEVICE_FAMILY_EFR32BG13P)
  systemPartFamilyBlue13P = _DEVINFO_PART_DEVICE_FAMILY_EFR32BG13P,       /**< EFR32 Blue Gecko Series 1 Configuration 3 Premium Device Family. */
#endif
#if defined(_DEVINFO_PART_DEVICE_FAMILY_EFR32BG13B)
  systemPartFamilyBlue13B = _DEVINFO_PART_DEVICE_FAMILY_EFR32BG13B,       /**< EFR32 Blue Gecko Series 1 Configuration 3 Basic Device Family. */
#endif
#if defined(_DEVINFO_PART_DEVICE_FAMILY_EFR32BG13V)
  systemPartFamilyBlue13V = _DEVINFO_PART_DEVICE_FAMILY_EFR32BG13V,       /**< EFR32 Blue Gecko Series 1 Configuration 3 Value Device Family. */
#endif
#if defined(_DEVINFO_PART_DEVICE_FAMILY_EFR32FG13P)
  systemPartFamilyFlex13P = _DEVINFO_PART_DEVICE_FAMILY_EFR32FG13P,       /**< EFR32 Flex Gecko Series 1 Configuration 3 Premium Device Family. */
#endif
#if defined(_DEVINFO_PART_DEVICE_FAMILY_EFR32FG13B)
  systemPartFamilyFlex13B = _DEVINFO_PART_DEVICE_FAMILY_EFR32FG13B,       /**< EFR32 Flex Gecko Series 1 Configuration 3 Basic Device Family. */
#endif
#if defined(_DEVINFO_PART_DEVICE_FAMILY_EFR32FG13V)
  systemPartFamilyFlex13V = _DEVINFO_PART_DEVICE_FAMILY_EFR32FG13V,       /**< EFR32 Flex Gecko Series 1 Configuration 3 Value Device Family. */
#endif
#if defined(_DEVINFO_PART_DEVICE_FAMILY_EFR32MG14P)
  systemPartFamilyMighty14P = _DEVINFO_PART_DEVICE_FAMILY_EFR32MG14P,     /**< EFR32 Mighty Gecko Series 1 Configuration 4 Premium Device Family. */
#endif
#if defined(_DEVINFO_PART_DEVICE_FAMILY_EFR32MG14B)
  systemPartFamilyMighty14B = _DEVINFO_PART_DEVICE_FAMILY_EFR32MG14B,     /**< EFR32 Mighty Gecko Series 1 Configuration 4 Basic Device Family. */
#endif
#if defined(_DEVINFO_PART_DEVICE_FAMILY_EFR32MG14V)
  systemPartFamilyMighty14V = _DEVINFO_PART_DEVICE_FAMILY_EFR32MG14V,     /**< EFR32 Mighty Gecko Series 1 Configuration 4 Value Device Family. */
#endif
#if defined(_DEVINFO_PART_DEVICE_FAMILY_EFR32BG14P)
  systemPartFamilyBlue14P = _DEVINFO_PART_DEVICE_FAMILY_EFR32BG14P,       /**< EFR32 Blue Gecko Series 1 Configuration 4 Premium Device Family. */
#endif
#if defined(_DEVINFO_PART_DEVICE_FAMILY_EFR32BG14B)
  systemPartFamilyBlue14B = _DEVINFO_PART_DEVICE_FAMILY_EFR32BG14B,       /**< EFR32 Blue Gecko Series 1 Configuration 4 Basic Device Family. */
#endif
#if defined(_DEVINFO_PART_DEVICE_FAMILY_EFR32BG14V)
  systemPartFamilyBlue14V = _DEVINFO_PART_DEVICE_FAMILY_EFR32BG14V,       /**< EFR32 Blue Gecko Series 1 Configuration 4 Value Device Family. */
#endif
#if defined(_DEVINFO_PART_DEVICE_FAMILY_EFR32FG14P)
  systemPartFamilyFlex14P = _DEVINFO_PART_DEVICE_FAMILY_EFR32FG14P,       /**< EFR32 Flex Gecko Series 1 Configuration 4 Premium Device Family. */
#endif
#if defined(_DEVINFO_PART_DEVICE_FAMILY_EFR32FG14B)
  systemPartFamilyFlex14B = _DEVINFO_PART_DEVICE_FAMILY_EFR32FG14B,       /**< EFR32 Flex Gecko Series 1 Configuration 4 Basic Device Family. */
#endif
#if defined(_DEVINFO_PART_DEVICE_FAMILY_EFR32FG14V)
  systemPartFamilyFlex14V = _DEVINFO_PART_DEVICE_FAMILY_EFR32FG14V,       /**< EFR32 Flex Gecko Series 1 Configuration 4 Value Device Family. */
#endif
#if defined(_DEVINFO_PART_DEVICE_FAMILY_EFR32ZG14P)
  systemPartFamilyZen14P = _DEVINFO_PART_DEVICE_FAMILY_EFR32ZG14P,        /**< EFR32 Zen Gecko Series 1 Configuration 4 Premium Device Family. */
#endif
#if defined(_SILICON_LABS_32B_SERIES_2_CONFIG_1)
  systemPartFamilyMighty21 = DEVINFO_PART_FAMILY_MG | (21 << _DEVINFO_PART_FAMILYNUM_SHIFT), /**< EFR32 Mighty Gecko Series 2 Config 1 Value Device Family */
  systemPartFamilyFlex21 = DEVINFO_PART_FAMILY_FG | (21 << _DEVINFO_PART_FAMILYNUM_SHIFT),   /**< EFR32 Flex Gecko Series 2 Config 1 Value Device Family */
  systemPartFamilyBlue21 = DEVINFO_PART_FAMILY_BG | (21 << _DEVINFO_PART_FAMILYNUM_SHIFT),   /**< EFR32 Blue Gecko Series 2 Config 1 Value Device Family */
#endif
/* Deprecated family #defines */
#if defined(_DEVINFO_PART_DEVICE_FAMILY_G)
  systemPartFamilyGecko   = _DEVINFO_PART_DEVICE_FAMILY_G,   /**< Gecko Device Family. */
#endif
#if defined(_DEVINFO_PART_DEVICE_FAMILY_GG)
  systemPartFamilyGiant   = _DEVINFO_PART_DEVICE_FAMILY_GG,  /**< Giant Gecko Device Family. */
#endif
#if defined(_DEVINFO_PART_DEVICE_FAMILY_TG)
  systemPartFamilyTiny    = _DEVINFO_PART_DEVICE_FAMILY_TG,  /**< Tiny Gecko Device Family. */
#endif
#if defined(_DEVINFO_PART_DEVICE_FAMILY_LG)
  systemPartFamilyLeopard = _DEVINFO_PART_DEVICE_FAMILY_LG,  /**< Leopard Gecko Device Family. */
#endif
#if defined(_DEVINFO_PART_DEVICE_FAMILY_WG)
  systemPartFamilyWonder  = _DEVINFO_PART_DEVICE_FAMILY_WG,  /**< Wonder Gecko Device Family. */
#endif
#if defined(_DEVINFO_PART_DEVICE_FAMILY_ZG)
  systemPartFamilyZero    = _DEVINFO_PART_DEVICE_FAMILY_ZG,  /**< Zero Gecko Device Family. */
#endif
#if defined(_DEVINFO_PART_DEVICE_FAMILY_HG)
  systemPartFamilyHappy   = _DEVINFO_PART_DEVICE_FAMILY_HG,  /**< Happy Gecko Device Family. */
#endif
  systemPartFamilyUnknown = 0xFF                             /**< Unknown Device Family.
                                                                  Family ID is missing
                                                                  on unprogrammed parts. */
} SYSTEM_PartFamily_TypeDef;

/*******************************************************************************
 *******************************   STRUCTS   ***********************************
 ******************************************************************************/

/** Chip revision details. */
typedef struct {
  uint8_t minor; /**< Minor revision number. */
  uint8_t major; /**< Major revision number. */
  uint8_t family;/**< Device family number.  */
} SYSTEM_ChipRevision_TypeDef;

#if defined(__FPU_PRESENT) && (__FPU_PRESENT == 1)
/** Floating point co-processor access modes. */
typedef enum {
  fpuAccessDenied         = (0x0 << 20),  /**< Access denied, any attempted access generates a NOCP UsageFault. */
  fpuAccessPrivilegedOnly = (0x5 << 20),  /**< Privileged access only, an unprivileged access generates a NOCP UsageFault. */
  fpuAccessReserved       = (0xA << 20),  /**< Reserved. */
  fpuAccessFull           = (0xF << 20)   /**< Full access. */
} SYSTEM_FpuAccess_TypeDef;
#endif

/** DEVINFO calibration address/value pair. */
typedef struct {
  uint32_t address;                       /**< Peripheral calibration register address. */
  uint32_t calValue;                      /**< Calibration value for register at address. */
}
SYSTEM_CalAddrVal_TypeDef;

/*******************************************************************************
 *****************************   PROTOTYPES   **********************************
 ******************************************************************************/

void SYSTEM_ChipRevisionGet(SYSTEM_ChipRevision_TypeDef *rev);
bool SYSTEM_GetCalibrationValue(volatile uint32_t *regAddress);

#if defined(_DEVINFO_DEVINFOREV_DEVINFOREV_MASK) || defined(_DEVINFO_INFO_DEVINFOREV_MASK)
/***************************************************************************//**
 * @brief
 *   Get DEVINFO revision.
 *
 * @return
 *   Revision of the DEVINFO contents.
 ******************************************************************************/
__STATIC_INLINE uint8_t SYSTEM_GetDevinfoRev(void)
{
#if defined(_DEVINFO_DEVINFOREV_DEVINFOREV_MASK)
  return (uint8_t)((DEVINFO->DEVINFOREV & _DEVINFO_DEVINFOREV_DEVINFOREV_MASK)
                   >> _DEVINFO_DEVINFOREV_DEVINFOREV_SHIFT);
#elif defined(_DEVINFO_INFO_DEVINFOREV_MASK)
  return (uint8_t)((DEVINFO->INFO & _DEVINFO_INFO_DEVINFOREV_MASK)
                   >> _DEVINFO_INFO_DEVINFOREV_SHIFT);
#endif
}
#endif

#if defined(__FPU_PRESENT) && (__FPU_PRESENT == 1)
/***************************************************************************//**
 * @brief
 *   Set floating point co-processor (FPU) access mode.
 *
 * @param[in] accessMode
 *   Floating point co-processor access mode. See @ref SYSTEM_FpuAccess_TypeDef
 *   for details.
 ******************************************************************************/
__STATIC_INLINE void SYSTEM_FpuAccessModeSet(SYSTEM_FpuAccess_TypeDef accessMode)
{
  SCB->CPACR = (SCB->CPACR & ~(0xFUL << 20)) | (uint32_t)accessMode;
}
#endif

/***************************************************************************//**
 * @brief
 *   Get unique number for this device.
 *
 * @return
 *   Unique number for this device.
 ******************************************************************************/
__STATIC_INLINE uint64_t SYSTEM_GetUnique(void)
{
#if defined (_DEVINFO_EUI64H_MASK)
  uint32_t tmp = DEVINFO->EUI64L;
  return (uint64_t)((uint64_t)DEVINFO->EUI64H << 32) | tmp;
#elif defined(_DEVINFO_UNIQUEH_MASK)
  uint32_t tmp = DEVINFO->UNIQUEL;
  return (uint64_t)((uint64_t)DEVINFO->UNIQUEH << 32) | tmp;
#else
#error Location of device unique number is not defined.
#endif
}

/***************************************************************************//**
 * @brief
 *   Get production revision for this part.
 *
 * @return
 *   Production revision for this part.
 ******************************************************************************/
__STATIC_INLINE uint8_t SYSTEM_GetProdRev(void)
{
#if defined (_DEVINFO_PART_PROD_REV_MASK)
  return (uint8_t)((DEVINFO->PART & _DEVINFO_PART_PROD_REV_MASK)
                   >> _DEVINFO_PART_PROD_REV_SHIFT);
#elif defined (_DEVINFO_INFO_PRODREV_MASK)
  return (uint8_t)((DEVINFO->INFO & _DEVINFO_INFO_PRODREV_MASK)
                   >> _DEVINFO_INFO_PRODREV_SHIFT);
#else
#error Location of production revision is not defined.
#endif
}

/***************************************************************************//**
 * @brief
 *   Get SRAM size (in KB).
 *
 * @note
 *   This function retrieves SRAM size by reading the chip device
 *   info structure. If your binary is made for one specific device only,
 *   use @ref SRAM_SIZE instead.
 *
 * @return
 *   Size of internal SRAM (in KB).
 ******************************************************************************/
__STATIC_INLINE uint16_t SYSTEM_GetSRAMSize(void)
{
  uint16_t sizekb;

#if defined(_EFM32_GECKO_FAMILY)
  /* Early Gecko devices had a bug where SRAM and Flash size were swapped. */
  if (SYSTEM_GetProdRev() < 5) {
    sizekb = (DEVINFO->MSIZE & _DEVINFO_MSIZE_FLASH_MASK)
             >> _DEVINFO_MSIZE_FLASH_SHIFT;
  }
#endif
  sizekb = (uint16_t)((DEVINFO->MSIZE & _DEVINFO_MSIZE_SRAM_MASK)
                      >> _DEVINFO_MSIZE_SRAM_SHIFT);

#if defined(_SILICON_LABS_GECKO_INTERNAL_SDID_80) && defined(_EFR_DEVICE)
  /* Do not include EFR32xG1 RAMH. */
  sizekb--;
#endif

  return sizekb;
}

/***************************************************************************//**
 * @brief
 *   Get the flash size (in KB).
 *
 * @note
 *   This function retrieves flash size by reading the chip device
 *   info structure. If your binary is made for one specific device only,
 *   use @ref FLASH_SIZE instead.
 *
 * @return
 *   Size of internal flash (in KB).
 ******************************************************************************/
__STATIC_INLINE uint16_t SYSTEM_GetFlashSize(void)
{
#if defined(_EFM32_GECKO_FAMILY)
  /* Early Gecko devices had a bug where SRAM and Flash size were swapped. */
  if (SYSTEM_GetProdRev() < 5) {
    return (DEVINFO->MSIZE & _DEVINFO_MSIZE_SRAM_MASK)
           >> _DEVINFO_MSIZE_SRAM_SHIFT;
  }
#endif
  return (uint16_t)((DEVINFO->MSIZE & _DEVINFO_MSIZE_FLASH_MASK)
                    >> _DEVINFO_MSIZE_FLASH_SHIFT);
}

/***************************************************************************//**
 * @brief
 *   Get the flash page size in bytes.
 *
 * @note
 *   This function retrieves flash page size by reading the chip device
 *   info structure. If your binary is made for one specific device only,
 *   use @ref FLASH_PAGE_SIZE instead.
 *
 * @return
 *   Page size of internal flash in bytes.
 ******************************************************************************/
__STATIC_INLINE uint32_t SYSTEM_GetFlashPageSize(void)
{
  uint32_t tmp;

#if defined(_SILICON_LABS_32B_SERIES_0)
#if defined(_EFM32_GIANT_FAMILY)
  if (SYSTEM_GetProdRev() < 18) {
    /* Early Giant/Leopard devices did not have MEMINFO in DEVINFO. */
    return FLASH_PAGE_SIZE;
  }
#elif defined(_EFM32_ZERO_FAMILY)
  if (SYSTEM_GetProdRev() < 24) {
    /* Early Zero devices have an incorrect DEVINFO flash page size */
    return FLASH_PAGE_SIZE;
  }
#endif
#endif

#if defined(_DEVINFO_MEMINFO_FLASHPAGESIZE_MASK)
  tmp = (DEVINFO->MEMINFO & _DEVINFO_MEMINFO_FLASHPAGESIZE_MASK)
        >> _DEVINFO_MEMINFO_FLASHPAGESIZE_SHIFT;
#elif defined(_DEVINFO_MEMINFO_FLASH_PAGE_SIZE_MASK)
  tmp = (DEVINFO->MEMINFO & _DEVINFO_MEMINFO_FLASH_PAGE_SIZE_MASK)
        >> _DEVINFO_MEMINFO_FLASH_PAGE_SIZE_SHIFT;
#else
#error Location of flash page size is not defined.
#endif

  return 1UL << ((tmp + 10UL) & 0x1FUL);
}

/***************************************************************************//**
 * @brief
 *   Get part number of MCU.
 *
 * @return
 *   The part number of MCU.
 ******************************************************************************/
__STATIC_INLINE uint16_t SYSTEM_GetPartNumber(void)
{
#if defined(_DEVINFO_PART_DEVICENUM_MASK)
  return (uint16_t)((DEVINFO->PART & _DEVINFO_PART_DEVICENUM_MASK)
                    >> _DEVINFO_PART_DEVICENUM_SHIFT);
#elif defined(_DEVINFO_PART_DEVICE_NUMBER_MASK)
  return (uint16_t)((DEVINFO->PART & _DEVINFO_PART_DEVICE_NUMBER_MASK)
                    >> _DEVINFO_PART_DEVICE_NUMBER_SHIFT);
#else
#error Location of device part number is not defined.
#endif
}

/***************************************************************************//**
 * @brief
 *   Get family identifier of MCU.
 *
 * @note
 *   This function retrieves family ID by reading the chip's device info
 *   structure in flash memory. Users can retrieve family ID directly
 *   by reading DEVINFO->PART item and decode with mask and shift
 *   \#defines defined in \<part_family\>_devinfo.h (refer to code
 *   below for details).
 *
 * @return
 *   Family identifier of MCU.
 ******************************************************************************/
__STATIC_INLINE SYSTEM_PartFamily_TypeDef SYSTEM_GetFamily(void)
{
#if defined(_SYSCFG_CHIPREV_FAMILY_MASK)
  return (SYSTEM_PartFamily_TypeDef)
         ((uint32_t)((DEVINFO->PART & (_DEVINFO_PART_FAMILY_MASK
                                       | _DEVINFO_PART_FAMILYNUM_MASK))));
#elif defined(_DEVINFO_PART_DEVICE_FAMILY_MASK)
  return (SYSTEM_PartFamily_TypeDef)
         ((uint32_t)((DEVINFO->PART & _DEVINFO_PART_DEVICE_FAMILY_MASK)
                     >> _DEVINFO_PART_DEVICE_FAMILY_SHIFT));
#else
#error Location of device family name is not defined.
#endif
}

/***************************************************************************//**
 * @brief
 *   Get calibration temperature (in degrees Celsius).
 *
 * @return
 *   Calibration temperature in Celsius.
 ******************************************************************************/
__STATIC_INLINE uint8_t SYSTEM_GetCalibrationTemperature(void)
{
#if defined(_DEVINFO_CAL_TEMP_MASK)
  return (uint8_t)((DEVINFO->CAL & _DEVINFO_CAL_TEMP_MASK)
                   >> _DEVINFO_CAL_TEMP_SHIFT);
#elif defined(_DEVINFO_CALTEMP_TEMP_MASK)
  return (uint8_t)((DEVINFO->CALTEMP & _DEVINFO_CALTEMP_TEMP_MASK)
                   >> _DEVINFO_CALTEMP_TEMP_SHIFT);
#else
#error Location of calibration temperature is not defined.
#endif
}

/** @} (end addtogroup SYSTEM) */
/** @} (end addtogroup emlib) */

#ifdef __cplusplus
}
#endif

#endif /* EM_SYSTEM_H */
