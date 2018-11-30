/***************************************************************************//**
 * @file em_acmp.h
 * @brief Analog Comparator (ACMP) peripheral API
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

#ifndef EM_ACMP_H
#define EM_ACMP_H

#include "em_device.h"
#include "em_gpio.h"

#if defined(ACMP_COUNT) && (ACMP_COUNT > 0)

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/***************************************************************************//**
 * @addtogroup emlib
 * @{
 ******************************************************************************/

/***************************************************************************//**
 * @addtogroup ACMP
 * @brief Analog comparator (ACMP) Peripheral API
 *
 * @details
 *  The Analog Comparator is used to compare voltage of two analog inputs
 *  with a digital output indicating which input voltage is higher. Inputs can
 *  either be one of the selectable internal references or from external pins.
 *  Response time and current consumption can be configured by
 *  altering the current supply to the comparator.
 *
 *  ACMP is available to EM3 and is able to wake up the system when
 *  input signals pass a certain threshold. Use @ref ACMP_IntEnable to enable
 *  an edge interrupt to use this functionality.
 *
 *  This example shows how to use the em_acmp.h API for comparing an input
 *  pin to an internal 2.5 V reference voltage.
 *
 *  @if DOXYDOC_P1_DEVICE
 *  @include em_acmp_compare_p1.c
 *  @endif
 *
 *  @if DOXYDOC_P2_DEVICE
 *  @include em_acmp_compare_p2.c
 *  @endif
 *
 * @note
 *  ACMP can also be used to compare two separate input pins.
 *
 * @details
 *  ACMP also contains specialized hardware for capacitive sensing. This
 *  module contains the @ref ACMP_CapsenseInit function to initialize
 *  ACMP for capacitive sensing and the @ref ACMP_CapsenseChannelSet function
 *  to select the current capsense channel.
 *
 *  For applications that require capacitive sensing it is recommended to use a
 *  library, such as cslib, which is provided by Silicon Labs.
 *
 * @{
 ******************************************************************************/

/*******************************************************************************
 ********************************   ENUMS   ************************************
 ******************************************************************************/

/** Resistor values used for the internal capacative sense resistor. See
 *  data sheet for your device for details on each resistor value. */
typedef enum {
#if defined(_ACMP_INPUTCTRL_CSRESSEL_MASK)
  acmpResistor0 = _ACMP_INPUTCTRL_CSRESSEL_RES0,   /**< Resistor value 0 */
  acmpResistor1 = _ACMP_INPUTCTRL_CSRESSEL_RES1,   /**< Resistor value 1 */
  acmpResistor2 = _ACMP_INPUTCTRL_CSRESSEL_RES2,   /**< Resistor value 2 */
  acmpResistor3 = _ACMP_INPUTCTRL_CSRESSEL_RES3,   /**< Resistor value 3 */
  acmpResistor4 = _ACMP_INPUTCTRL_CSRESSEL_RES4,   /**< Resistor value 4 */
  acmpResistor5 = _ACMP_INPUTCTRL_CSRESSEL_RES5,   /**< Resistor value 5 */
  acmpResistor6 = _ACMP_INPUTCTRL_CSRESSEL_RES6,   /**< Resistor value 6 */
#else
  acmpResistor0 = _ACMP_INPUTSEL_CSRESSEL_RES0,   /**< Resistor value 0 */
  acmpResistor1 = _ACMP_INPUTSEL_CSRESSEL_RES1,   /**< Resistor value 1 */
  acmpResistor2 = _ACMP_INPUTSEL_CSRESSEL_RES2,   /**< Resistor value 2 */
  acmpResistor3 = _ACMP_INPUTSEL_CSRESSEL_RES3,   /**< Resistor value 3 */
#if defined(_ACMP_INPUTSEL_CSRESSEL_RES4)
  acmpResistor4 = _ACMP_INPUTSEL_CSRESSEL_RES4,   /**< Resistor value 4 */
  acmpResistor5 = _ACMP_INPUTSEL_CSRESSEL_RES5,   /**< Resistor value 5 */
  acmpResistor6 = _ACMP_INPUTSEL_CSRESSEL_RES6,   /**< Resistor value 6 */
  acmpResistor7 = _ACMP_INPUTSEL_CSRESSEL_RES7,   /**< Resistor value 7 */
#endif
#endif
} ACMP_CapsenseResistor_TypeDef;

/** Hysteresis level. See data sheet for your device for details on each
 *  level. */
typedef enum {
#if defined(_ACMP_CTRL_HYSTSEL_MASK)
  acmpHysteresisLevel0 = _ACMP_CTRL_HYSTSEL_HYST0,       /**< Hysteresis level 0 */
  acmpHysteresisLevel1 = _ACMP_CTRL_HYSTSEL_HYST1,       /**< Hysteresis level 1 */
  acmpHysteresisLevel2 = _ACMP_CTRL_HYSTSEL_HYST2,       /**< Hysteresis level 2 */
  acmpHysteresisLevel3 = _ACMP_CTRL_HYSTSEL_HYST3,       /**< Hysteresis level 3 */
  acmpHysteresisLevel4 = _ACMP_CTRL_HYSTSEL_HYST4,       /**< Hysteresis level 4 */
  acmpHysteresisLevel5 = _ACMP_CTRL_HYSTSEL_HYST5,       /**< Hysteresis level 5 */
  acmpHysteresisLevel6 = _ACMP_CTRL_HYSTSEL_HYST6,       /**< Hysteresis level 6 */
  acmpHysteresisLevel7 = _ACMP_CTRL_HYSTSEL_HYST7        /**< Hysteresis level 7 */
#endif
#if defined(_ACMP_HYSTERESIS0_HYST_MASK)
  acmpHysteresisLevel0 = _ACMP_HYSTERESIS0_HYST_HYST0,   /**< Hysteresis level 0 */
  acmpHysteresisLevel1 = _ACMP_HYSTERESIS0_HYST_HYST1,   /**< Hysteresis level 1 */
  acmpHysteresisLevel2 = _ACMP_HYSTERESIS0_HYST_HYST2,   /**< Hysteresis level 2 */
  acmpHysteresisLevel3 = _ACMP_HYSTERESIS0_HYST_HYST3,   /**< Hysteresis level 3 */
  acmpHysteresisLevel4 = _ACMP_HYSTERESIS0_HYST_HYST4,   /**< Hysteresis level 4 */
  acmpHysteresisLevel5 = _ACMP_HYSTERESIS0_HYST_HYST5,   /**< Hysteresis level 5 */
  acmpHysteresisLevel6 = _ACMP_HYSTERESIS0_HYST_HYST6,   /**< Hysteresis level 6 */
  acmpHysteresisLevel7 = _ACMP_HYSTERESIS0_HYST_HYST7,   /**< Hysteresis level 7 */
  acmpHysteresisLevel8 = _ACMP_HYSTERESIS0_HYST_HYST8,   /**< Hysteresis level 8 */
  acmpHysteresisLevel9 = _ACMP_HYSTERESIS0_HYST_HYST9,   /**< Hysteresis level 9 */
  acmpHysteresisLevel10 = _ACMP_HYSTERESIS0_HYST_HYST10, /**< Hysteresis level 10 */
  acmpHysteresisLevel11 = _ACMP_HYSTERESIS0_HYST_HYST11, /**< Hysteresis level 11 */
  acmpHysteresisLevel12 = _ACMP_HYSTERESIS0_HYST_HYST12, /**< Hysteresis level 12 */
  acmpHysteresisLevel13 = _ACMP_HYSTERESIS0_HYST_HYST13, /**< Hysteresis level 13 */
  acmpHysteresisLevel14 = _ACMP_HYSTERESIS0_HYST_HYST14, /**< Hysteresis level 14 */
  acmpHysteresisLevel15 = _ACMP_HYSTERESIS0_HYST_HYST15, /**< Hysteresis level 15 */
#endif
#if defined(_ACMP_CFG_HYST_MASK)
  acmpHysteresisDisabled = _ACMP_CFG_HYST_DISABLED,   /**< Mode DISABLED for ACMP_CFG */
  acmpHysteresis10Sym = _ACMP_CFG_HYST_HYST10SYM,     /**< Mode HYST10SYM for ACMP_CFG */
  acmpHysteresis20Sym = _ACMP_CFG_HYST_HYST20SYM,     /**< Mode HYST20SYM for ACMP_CFG */
  acmpHysteresis30Sym = _ACMP_CFG_HYST_HYST30SYM,     /**< Mode HYST30SYM for ACMP_CFG */
  acmpHysteresis10Pos = _ACMP_CFG_HYST_HYST10POS,     /**< Mode HYST10POS for ACMP_CFG */
  acmpHysteresis20Pos = _ACMP_CFG_HYST_HYST20POS,     /**< Mode HYST20POS for ACMP_CFG */
  acmpHysteresis30Pos = _ACMP_CFG_HYST_HYST30POS,     /**< Mode HYST30POS for ACMP_CFG */
  acmpHysteresis10Neg = _ACMP_CFG_HYST_HYST10NEG,     /**< Mode HYST10NEG for ACMP_CFG */
  acmpHysteresis20Neg = _ACMP_CFG_HYST_HYST20NEG,     /**< Mode HYST20NEG for ACMP_CFG */
  acmpHysteresis30Neg = _ACMP_CFG_HYST_HYST30NEG,     /**< Mode HYST30NEG for ACMP_CFG */
#endif
} ACMP_HysteresisLevel_TypeDef;

#if defined(_ACMP_CTRL_WARMTIME_MASK)
/** ACMP warmup time. The delay is measured in HFPERCLK cycles and should
 *  be at least 10 us. */
typedef enum {
  /** 4 HFPERCLK cycles warmup */
  acmpWarmTime4   = _ACMP_CTRL_WARMTIME_4CYCLES,
  /** 8 HFPERCLK cycles warmup */
  acmpWarmTime8   = _ACMP_CTRL_WARMTIME_8CYCLES,
  /** 16 HFPERCLK cycles warmup */
  acmpWarmTime16  = _ACMP_CTRL_WARMTIME_16CYCLES,
  /** 32 HFPERCLK cycles warmup */
  acmpWarmTime32  = _ACMP_CTRL_WARMTIME_32CYCLES,
  /** 64 HFPERCLK cycles warmup */
  acmpWarmTime64  = _ACMP_CTRL_WARMTIME_64CYCLES,
  /** 128 HFPERCLK cycles warmup */
  acmpWarmTime128 = _ACMP_CTRL_WARMTIME_128CYCLES,
  /** 256 HFPERCLK cycles warmup */
  acmpWarmTime256 = _ACMP_CTRL_WARMTIME_256CYCLES,
  /** 512 HFPERCLK cycles warmup */
  acmpWarmTime512 = _ACMP_CTRL_WARMTIME_512CYCLES
} ACMP_WarmTime_TypeDef;
#endif

#if defined(_ACMP_CTRL_INPUTRANGE_MASK) \
  || defined(_ACMP_CFG_INPUTRANGE_MASK)
/**
 * Adjust ACMP performance for a given input voltage range.
 */
typedef enum {
#if defined(_ACMP_CTRL_INPUTRANGE_MASK)
  acmpInputRangeFull = _ACMP_CTRL_INPUTRANGE_FULL,      /**< Input can be from 0 to VDD. */
  acmpInputRangeHigh = _ACMP_CTRL_INPUTRANGE_GTVDDDIV2, /**< Input will always be greater than VDD/2. */
  acmpInputRangeLow  = _ACMP_CTRL_INPUTRANGE_LTVDDDIV2  /**< Input will always be less than VDD/2. */
#elif defined(_ACMP_CFG_INPUTRANGE_MASK)
  acmpInputRangeFull    = _ACMP_CFG_INPUTRANGE_FULL,    /**< Input can be from 0 to VDD. */
  acmpInputRangeReduced = _ACMP_CFG_INPUTRANGE_REDUCED, /**< Input can be from 0 to VDD-0.7 V. */
#endif
} ACMP_InputRange_TypeDef;
#endif

#if defined(_ACMP_CTRL_PWRSEL_MASK)
/**
 * ACMP Power source.
 */
typedef enum {
  acmpPowerSourceAvdd      = _ACMP_CTRL_PWRSEL_AVDD,    /**< Power ACMP using the AVDD supply. */
#if defined(_ACMP_CTRL_PWRSEL_DVDD)
  acmpPowerSourceDvdd      = _ACMP_CTRL_PWRSEL_DVDD,    /**< Power ACMP using the DVDD supply. */
#endif
  acmpPowerSourceIOVdd0    = _ACMP_CTRL_PWRSEL_IOVDD0,  /**< Power ACMP using the IOVDD/IOVDD0 supply. */
  acmpPowerSourceIOVdd1    = _ACMP_CTRL_PWRSEL_IOVDD1,  /**< Power ACMP using the IOVDD1 supply (if the part has two I/O voltages). */
} ACMP_PowerSource_TypeDef;
#endif

#if defined(_ACMP_CTRL_ACCURACY_MASK) \
  || defined(_ACMP_CFG_ACCURACY_MASK)
/**
 * ACMP accuracy mode.
 */
typedef enum {
#if defined(_ACMP_CTRL_ACCURACY_MASK)
  acmpAccuracyLow = _ACMP_CTRL_ACCURACY_LOW,   /**< Low-accuracy mode which consumes less current. */
  acmpAccuracyHigh = _ACMP_CTRL_ACCURACY_HIGH  /**< High-accuracy mode which consumes more current. */
#elif defined(_ACMP_CFG_ACCURACY_MASK)
  acmpAccuracyLow = _ACMP_CFG_ACCURACY_LOW,   /**< Low-accuracy mode which consumes less current. */
  acmpAccuracyHigh = _ACMP_CFG_ACCURACY_HIGH  /**< High-accuracy mode which consumes more current. */
#endif
} ACMP_Accuracy_TypeDef;
#endif

#if defined(_ACMP_INPUTSEL_VASEL_MASK)
/** ACMP input to the VA divider. This enumeration is used to select the input for
 *  the VA Divider. */
typedef enum {
  acmpVAInputVDD       = _ACMP_INPUTSEL_VASEL_VDD,
  acmpVAInputAPORT2YCH0  = _ACMP_INPUTSEL_VASEL_APORT2YCH0,
  acmpVAInputAPORT2YCH2  = _ACMP_INPUTSEL_VASEL_APORT2YCH2,
  acmpVAInputAPORT2YCH4  = _ACMP_INPUTSEL_VASEL_APORT2YCH4,
  acmpVAInputAPORT2YCH6  = _ACMP_INPUTSEL_VASEL_APORT2YCH6,
  acmpVAInputAPORT2YCH8  = _ACMP_INPUTSEL_VASEL_APORT2YCH8,
  acmpVAInputAPORT2YCH10 = _ACMP_INPUTSEL_VASEL_APORT2YCH10,
  acmpVAInputAPORT2YCH12 = _ACMP_INPUTSEL_VASEL_APORT2YCH12,
  acmpVAInputAPORT2YCH14 = _ACMP_INPUTSEL_VASEL_APORT2YCH14,
  acmpVAInputAPORT2YCH16 = _ACMP_INPUTSEL_VASEL_APORT2YCH16,
  acmpVAInputAPORT2YCH18 = _ACMP_INPUTSEL_VASEL_APORT2YCH18,
  acmpVAInputAPORT2YCH20 = _ACMP_INPUTSEL_VASEL_APORT2YCH20,
  acmpVAInputAPORT2YCH22 = _ACMP_INPUTSEL_VASEL_APORT2YCH22,
  acmpVAInputAPORT2YCH24 = _ACMP_INPUTSEL_VASEL_APORT2YCH24,
  acmpVAInputAPORT2YCH26 = _ACMP_INPUTSEL_VASEL_APORT2YCH26,
  acmpVAInputAPORT2YCH28 = _ACMP_INPUTSEL_VASEL_APORT2YCH28,
  acmpVAInputAPORT2YCH30 = _ACMP_INPUTSEL_VASEL_APORT2YCH30,
  acmpVAInputAPORT1XCH0  = _ACMP_INPUTSEL_VASEL_APORT1XCH0,
  acmpVAInputAPORT1YCH1  = _ACMP_INPUTSEL_VASEL_APORT1YCH1,
  acmpVAInputAPORT1XCH2  = _ACMP_INPUTSEL_VASEL_APORT1XCH2,
  acmpVAInputAPORT1YCH3  = _ACMP_INPUTSEL_VASEL_APORT1YCH3,
  acmpVAInputAPORT1XCH4  = _ACMP_INPUTSEL_VASEL_APORT1XCH4,
  acmpVAInputAPORT1YCH5  = _ACMP_INPUTSEL_VASEL_APORT1YCH5,
  acmpVAInputAPORT1XCH6  = _ACMP_INPUTSEL_VASEL_APORT1XCH6,
  acmpVAInputAPORT1YCH7  = _ACMP_INPUTSEL_VASEL_APORT1YCH7,
  acmpVAInputAPORT1XCH8  = _ACMP_INPUTSEL_VASEL_APORT1XCH8,
  acmpVAInputAPORT1YCH9  = _ACMP_INPUTSEL_VASEL_APORT1YCH9,
  acmpVAInputAPORT1XCH10 = _ACMP_INPUTSEL_VASEL_APORT1XCH10,
  acmpVAInputAPORT1YCH11 = _ACMP_INPUTSEL_VASEL_APORT1YCH11,
  acmpVAInputAPORT1XCH12 = _ACMP_INPUTSEL_VASEL_APORT1XCH12,
  acmpVAInputAPORT1YCH13 = _ACMP_INPUTSEL_VASEL_APORT1YCH13,
  acmpVAInputAPORT1XCH14 = _ACMP_INPUTSEL_VASEL_APORT1XCH14,
  acmpVAInputAPORT1YCH15 = _ACMP_INPUTSEL_VASEL_APORT1YCH15,
  acmpVAInputAPORT1XCH16 = _ACMP_INPUTSEL_VASEL_APORT1XCH16,
  acmpVAInputAPORT1YCH17 = _ACMP_INPUTSEL_VASEL_APORT1YCH17,
  acmpVAInputAPORT1XCH18 = _ACMP_INPUTSEL_VASEL_APORT1XCH18,
  acmpVAInputAPORT1YCH19 = _ACMP_INPUTSEL_VASEL_APORT1YCH19,
  acmpVAInputAPORT1XCH20 = _ACMP_INPUTSEL_VASEL_APORT1XCH20,
  acmpVAInputAPORT1YCH21 = _ACMP_INPUTSEL_VASEL_APORT1YCH21,
  acmpVAInputAPORT1XCH22 = _ACMP_INPUTSEL_VASEL_APORT1XCH22,
  acmpVAInputAPORT1YCH23 = _ACMP_INPUTSEL_VASEL_APORT1YCH23,
  acmpVAInputAPORT1XCH24 = _ACMP_INPUTSEL_VASEL_APORT1XCH24,
  acmpVAInputAPORT1YCH25 = _ACMP_INPUTSEL_VASEL_APORT1YCH25,
  acmpVAInputAPORT1XCH26 = _ACMP_INPUTSEL_VASEL_APORT1XCH26,
  acmpVAInputAPORT1YCH27 = _ACMP_INPUTSEL_VASEL_APORT1YCH27,
  acmpVAInputAPORT1XCH28 = _ACMP_INPUTSEL_VASEL_APORT1XCH28,
  acmpVAInputAPORT1YCH29 = _ACMP_INPUTSEL_VASEL_APORT1YCH29,
  acmpVAInputAPORT1XCH30 = _ACMP_INPUTSEL_VASEL_APORT1XCH30,
  acmpVAInputAPORT1YCH31 = _ACMP_INPUTSEL_VASEL_APORT1YCH31
} ACMP_VAInput_TypeDef;
#endif

#if defined(_ACMP_INPUTSEL_VBSEL_MASK)
/**
 * ACMP input to the VB divider. This enumeration is used to select the input for
 * the VB divider.
 */
typedef enum {
  acmpVBInput1V25 = _ACMP_INPUTSEL_VBSEL_1V25,
  acmpVBInput2V5  = _ACMP_INPUTSEL_VBSEL_2V5
} ACMP_VBInput_TypeDef;
#endif

#if defined(_ACMP_INPUTSEL_VLPSEL_MASK)
/**
 * ACMP Low-Power Input Selection.
 */
typedef enum {
  acmpVLPInputVADIV = _ACMP_INPUTSEL_VLPSEL_VADIV,
  acmpVLPInputVBDIV = _ACMP_INPUTSEL_VLPSEL_VBDIV
} ACMP_VLPInput_Typedef;
#endif

#if defined(_ACMP_INPUTCTRL_MASK)
/** ACMP Input Selection. */
typedef enum {
  acmpInputVSS            = _ACMP_INPUTCTRL_POSSEL_VSS,
  acmpInputVREFDIVAVDD    = _ACMP_INPUTCTRL_POSSEL_VREFDIVAVDD,
  acmpInputVREFDIVAVDDLP  = _ACMP_INPUTCTRL_POSSEL_VREFDIVAVDDLP,
  acmpInputVREFDIV1V25    = _ACMP_INPUTCTRL_POSSEL_VREFDIV1V25,
  acmpInputVREFDIV1V25LP  = _ACMP_INPUTCTRL_POSSEL_VREFDIV1V25LP,
  acmpInputVREFDIV2V5     = _ACMP_INPUTCTRL_POSSEL_VREFDIV2V5,
  acmpInputVREFDIV2V5LP   = _ACMP_INPUTCTRL_POSSEL_VREFDIV2V5LP,
  acmpInputVSENSE01DIV4   = _ACMP_INPUTCTRL_POSSEL_VSENSE01DIV4,
  acmpInputVSENSE01DIV4LP = _ACMP_INPUTCTRL_POSSEL_VSENSE01DIV4LP,
  acmpInputVSENSE11DIV4   = _ACMP_INPUTCTRL_POSSEL_VSENSE11DIV4,
  acmpInputVSENSE11DIV4LP = _ACMP_INPUTCTRL_POSSEL_VSENSE11DIV4LP,
  acmpInputCAPSENSE       = _ACMP_INPUTCTRL_NEGSEL_CAPSENSE,
  acmpInputPA0            = _ACMP_INPUTCTRL_POSSEL_PA0,
  acmpInputPA1            = _ACMP_INPUTCTRL_POSSEL_PA1,
  acmpInputPA2            = _ACMP_INPUTCTRL_POSSEL_PA2,
  acmpInputPA3            = _ACMP_INPUTCTRL_POSSEL_PA3,
  acmpInputPA4            = _ACMP_INPUTCTRL_POSSEL_PA4,
  acmpInputPA5            = _ACMP_INPUTCTRL_POSSEL_PA5,
  acmpInputPA6            = _ACMP_INPUTCTRL_POSSEL_PA6,
  acmpInputPA7            = _ACMP_INPUTCTRL_POSSEL_PA7,
  acmpInputPA8            = _ACMP_INPUTCTRL_POSSEL_PA8,
  acmpInputPA9            = _ACMP_INPUTCTRL_POSSEL_PA9,
  acmpInputPA10           = _ACMP_INPUTCTRL_POSSEL_PA10,
  acmpInputPA11           = _ACMP_INPUTCTRL_POSSEL_PA11,
  acmpInputPA12           = _ACMP_INPUTCTRL_POSSEL_PA12,
  acmpInputPA13           = _ACMP_INPUTCTRL_POSSEL_PA13,
  acmpInputPA14           = _ACMP_INPUTCTRL_POSSEL_PA14,
  acmpInputPA15           = _ACMP_INPUTCTRL_POSSEL_PA15,
  acmpInputPB0            = _ACMP_INPUTCTRL_POSSEL_PB0,
  acmpInputPB1            = _ACMP_INPUTCTRL_POSSEL_PB1,
  acmpInputPB2            = _ACMP_INPUTCTRL_POSSEL_PB2,
  acmpInputPB3            = _ACMP_INPUTCTRL_POSSEL_PB3,
  acmpInputPB4            = _ACMP_INPUTCTRL_POSSEL_PB4,
  acmpInputPB5            = _ACMP_INPUTCTRL_POSSEL_PB5,
  acmpInputPB6            = _ACMP_INPUTCTRL_POSSEL_PB6,
  acmpInputPB7            = _ACMP_INPUTCTRL_POSSEL_PB7,
  acmpInputPB8            = _ACMP_INPUTCTRL_POSSEL_PB8,
  acmpInputPB9            = _ACMP_INPUTCTRL_POSSEL_PB9,
  acmpInputPB10           = _ACMP_INPUTCTRL_POSSEL_PB10,
  acmpInputPB11           = _ACMP_INPUTCTRL_POSSEL_PB11,
  acmpInputPB12           = _ACMP_INPUTCTRL_POSSEL_PB12,
  acmpInputPB13           = _ACMP_INPUTCTRL_POSSEL_PB13,
  acmpInputPB14           = _ACMP_INPUTCTRL_POSSEL_PB14,
  acmpInputPB15           = _ACMP_INPUTCTRL_POSSEL_PB15,
  acmpInputPC0            = _ACMP_INPUTCTRL_POSSEL_PC0,
  acmpInputPC1            = _ACMP_INPUTCTRL_POSSEL_PC1,
  acmpInputPC2            = _ACMP_INPUTCTRL_POSSEL_PC2,
  acmpInputPC3            = _ACMP_INPUTCTRL_POSSEL_PC3,
  acmpInputPC4            = _ACMP_INPUTCTRL_POSSEL_PC4,
  acmpInputPC5            = _ACMP_INPUTCTRL_POSSEL_PC5,
  acmpInputPC6            = _ACMP_INPUTCTRL_POSSEL_PC6,
  acmpInputPC7            = _ACMP_INPUTCTRL_POSSEL_PC7,
  acmpInputPC8            = _ACMP_INPUTCTRL_POSSEL_PC8,
  acmpInputPC9            = _ACMP_INPUTCTRL_POSSEL_PC9,
  acmpInputPC10           = _ACMP_INPUTCTRL_POSSEL_PC10,
  acmpInputPC11           = _ACMP_INPUTCTRL_POSSEL_PC11,
  acmpInputPC12           = _ACMP_INPUTCTRL_POSSEL_PC12,
  acmpInputPC13           = _ACMP_INPUTCTRL_POSSEL_PC13,
  acmpInputPC14           = _ACMP_INPUTCTRL_POSSEL_PC14,
  acmpInputPC15           = _ACMP_INPUTCTRL_POSSEL_PC15,
  acmpInputPD0            = _ACMP_INPUTCTRL_POSSEL_PD0,
  acmpInputPD1            = _ACMP_INPUTCTRL_POSSEL_PD1,
  acmpInputPD2            = _ACMP_INPUTCTRL_POSSEL_PD2,
  acmpInputPD3            = _ACMP_INPUTCTRL_POSSEL_PD3,
  acmpInputPD4            = _ACMP_INPUTCTRL_POSSEL_PD4,
  acmpInputPD5            = _ACMP_INPUTCTRL_POSSEL_PD5,
  acmpInputPD6            = _ACMP_INPUTCTRL_POSSEL_PD6,
  acmpInputPD7            = _ACMP_INPUTCTRL_POSSEL_PD7,
  acmpInputPD8            = _ACMP_INPUTCTRL_POSSEL_PD8,
  acmpInputPD9            = _ACMP_INPUTCTRL_POSSEL_PD9,
  acmpInputPD10           = _ACMP_INPUTCTRL_POSSEL_PD10,
  acmpInputPD11           = _ACMP_INPUTCTRL_POSSEL_PD11,
  acmpInputPD12           = _ACMP_INPUTCTRL_POSSEL_PD12,
  acmpInputPD13           = _ACMP_INPUTCTRL_POSSEL_PD13,
  acmpInputPD14           = _ACMP_INPUTCTRL_POSSEL_PD14,
  acmpInputPD15           = _ACMP_INPUTCTRL_POSSEL_PD15,
} ACMP_Channel_TypeDef;
#elif defined(_ACMP_INPUTSEL_POSSEL_APORT0XCH0)
/** ACMP Input Selection. */
typedef enum {
  acmpInputAPORT0XCH0  = _ACMP_INPUTSEL_POSSEL_APORT0XCH0,
  acmpInputAPORT0XCH1  = _ACMP_INPUTSEL_POSSEL_APORT0XCH1,
  acmpInputAPORT0XCH2  = _ACMP_INPUTSEL_POSSEL_APORT0XCH2,
  acmpInputAPORT0XCH3  = _ACMP_INPUTSEL_POSSEL_APORT0XCH3,
  acmpInputAPORT0XCH4  = _ACMP_INPUTSEL_POSSEL_APORT0XCH4,
  acmpInputAPORT0XCH5  = _ACMP_INPUTSEL_POSSEL_APORT0XCH5,
  acmpInputAPORT0XCH6  = _ACMP_INPUTSEL_POSSEL_APORT0XCH6,
  acmpInputAPORT0XCH7  = _ACMP_INPUTSEL_POSSEL_APORT0XCH7,
  acmpInputAPORT0XCH8  = _ACMP_INPUTSEL_POSSEL_APORT0XCH8,
  acmpInputAPORT0XCH9  = _ACMP_INPUTSEL_POSSEL_APORT0XCH9,
  acmpInputAPORT0XCH10 = _ACMP_INPUTSEL_POSSEL_APORT0XCH10,
  acmpInputAPORT0XCH11 = _ACMP_INPUTSEL_POSSEL_APORT0XCH11,
  acmpInputAPORT0XCH12 = _ACMP_INPUTSEL_POSSEL_APORT0XCH12,
  acmpInputAPORT0XCH13 = _ACMP_INPUTSEL_POSSEL_APORT0XCH13,
  acmpInputAPORT0XCH14 = _ACMP_INPUTSEL_POSSEL_APORT0XCH14,
  acmpInputAPORT0XCH15 = _ACMP_INPUTSEL_POSSEL_APORT0XCH15,
  acmpInputAPORT0YCH0  = _ACMP_INPUTSEL_POSSEL_APORT0YCH0,
  acmpInputAPORT0YCH1  = _ACMP_INPUTSEL_POSSEL_APORT0YCH1,
  acmpInputAPORT0YCH2  = _ACMP_INPUTSEL_POSSEL_APORT0YCH2,
  acmpInputAPORT0YCH3  = _ACMP_INPUTSEL_POSSEL_APORT0YCH3,
  acmpInputAPORT0YCH4  = _ACMP_INPUTSEL_POSSEL_APORT0YCH4,
  acmpInputAPORT0YCH5  = _ACMP_INPUTSEL_POSSEL_APORT0YCH5,
  acmpInputAPORT0YCH6  = _ACMP_INPUTSEL_POSSEL_APORT0YCH6,
  acmpInputAPORT0YCH7  = _ACMP_INPUTSEL_POSSEL_APORT0YCH7,
  acmpInputAPORT0YCH8  = _ACMP_INPUTSEL_POSSEL_APORT0YCH8,
  acmpInputAPORT0YCH9  = _ACMP_INPUTSEL_POSSEL_APORT0YCH9,
  acmpInputAPORT0YCH10 = _ACMP_INPUTSEL_POSSEL_APORT0YCH10,
  acmpInputAPORT0YCH11 = _ACMP_INPUTSEL_POSSEL_APORT0YCH11,
  acmpInputAPORT0YCH12 = _ACMP_INPUTSEL_POSSEL_APORT0YCH12,
  acmpInputAPORT0YCH13 = _ACMP_INPUTSEL_POSSEL_APORT0YCH13,
  acmpInputAPORT0YCH14 = _ACMP_INPUTSEL_POSSEL_APORT0YCH14,
  acmpInputAPORT0YCH15 = _ACMP_INPUTSEL_POSSEL_APORT0YCH15,
  acmpInputAPORT1XCH0  = _ACMP_INPUTSEL_POSSEL_APORT1XCH0,
  acmpInputAPORT1YCH1  = _ACMP_INPUTSEL_POSSEL_APORT1YCH1,
  acmpInputAPORT1XCH2  = _ACMP_INPUTSEL_POSSEL_APORT1XCH2,
  acmpInputAPORT1YCH3  = _ACMP_INPUTSEL_POSSEL_APORT1YCH3,
  acmpInputAPORT1XCH4  = _ACMP_INPUTSEL_POSSEL_APORT1XCH4,
  acmpInputAPORT1YCH5  = _ACMP_INPUTSEL_POSSEL_APORT1YCH5,
  acmpInputAPORT1XCH6  = _ACMP_INPUTSEL_POSSEL_APORT1XCH6,
  acmpInputAPORT1YCH7  = _ACMP_INPUTSEL_POSSEL_APORT1YCH7,
  acmpInputAPORT1XCH8  = _ACMP_INPUTSEL_POSSEL_APORT1XCH8,
  acmpInputAPORT1YCH9  = _ACMP_INPUTSEL_POSSEL_APORT1YCH9,
  acmpInputAPORT1XCH10 = _ACMP_INPUTSEL_POSSEL_APORT1XCH10,
  acmpInputAPORT1YCH11 = _ACMP_INPUTSEL_POSSEL_APORT1YCH11,
  acmpInputAPORT1XCH12 = _ACMP_INPUTSEL_POSSEL_APORT1XCH12,
  acmpInputAPORT1YCH13 = _ACMP_INPUTSEL_POSSEL_APORT1YCH13,
  acmpInputAPORT1XCH14 = _ACMP_INPUTSEL_POSSEL_APORT1XCH14,
  acmpInputAPORT1YCH15 = _ACMP_INPUTSEL_POSSEL_APORT1YCH15,
  acmpInputAPORT1XCH16 = _ACMP_INPUTSEL_POSSEL_APORT1XCH16,
  acmpInputAPORT1YCH17 = _ACMP_INPUTSEL_POSSEL_APORT1YCH17,
  acmpInputAPORT1XCH18 = _ACMP_INPUTSEL_POSSEL_APORT1XCH18,
  acmpInputAPORT1YCH19 = _ACMP_INPUTSEL_POSSEL_APORT1YCH19,
  acmpInputAPORT1XCH20 = _ACMP_INPUTSEL_POSSEL_APORT1XCH20,
  acmpInputAPORT1YCH21 = _ACMP_INPUTSEL_POSSEL_APORT1YCH21,
  acmpInputAPORT1XCH22 = _ACMP_INPUTSEL_POSSEL_APORT1XCH22,
  acmpInputAPORT1YCH23 = _ACMP_INPUTSEL_POSSEL_APORT1YCH23,
  acmpInputAPORT1XCH24 = _ACMP_INPUTSEL_POSSEL_APORT1XCH24,
  acmpInputAPORT1YCH25 = _ACMP_INPUTSEL_POSSEL_APORT1YCH25,
  acmpInputAPORT1XCH26 = _ACMP_INPUTSEL_POSSEL_APORT1XCH26,
  acmpInputAPORT1YCH27 = _ACMP_INPUTSEL_POSSEL_APORT1YCH27,
  acmpInputAPORT1XCH28 = _ACMP_INPUTSEL_POSSEL_APORT1XCH28,
  acmpInputAPORT1YCH29 = _ACMP_INPUTSEL_POSSEL_APORT1YCH29,
  acmpInputAPORT1XCH30 = _ACMP_INPUTSEL_POSSEL_APORT1XCH30,
  acmpInputAPORT1YCH31 = _ACMP_INPUTSEL_POSSEL_APORT1YCH31,
  acmpInputAPORT2YCH0  = _ACMP_INPUTSEL_POSSEL_APORT2YCH0,
  acmpInputAPORT2XCH1  = _ACMP_INPUTSEL_POSSEL_APORT2XCH1,
  acmpInputAPORT2YCH2  = _ACMP_INPUTSEL_POSSEL_APORT2YCH2,
  acmpInputAPORT2XCH3  = _ACMP_INPUTSEL_POSSEL_APORT2XCH3,
  acmpInputAPORT2YCH4  = _ACMP_INPUTSEL_POSSEL_APORT2YCH4,
  acmpInputAPORT2XCH5  = _ACMP_INPUTSEL_POSSEL_APORT2XCH5,
  acmpInputAPORT2YCH6  = _ACMP_INPUTSEL_POSSEL_APORT2YCH6,
  acmpInputAPORT2XCH7  = _ACMP_INPUTSEL_POSSEL_APORT2XCH7,
  acmpInputAPORT2YCH8  = _ACMP_INPUTSEL_POSSEL_APORT2YCH8,
  acmpInputAPORT2XCH9  = _ACMP_INPUTSEL_POSSEL_APORT2XCH9,
  acmpInputAPORT2YCH10 = _ACMP_INPUTSEL_POSSEL_APORT2YCH10,
  acmpInputAPORT2XCH11 = _ACMP_INPUTSEL_POSSEL_APORT2XCH11,
  acmpInputAPORT2YCH12 = _ACMP_INPUTSEL_POSSEL_APORT2YCH12,
  acmpInputAPORT2XCH13 = _ACMP_INPUTSEL_POSSEL_APORT2XCH13,
  acmpInputAPORT2YCH14 = _ACMP_INPUTSEL_POSSEL_APORT2YCH14,
  acmpInputAPORT2XCH15 = _ACMP_INPUTSEL_POSSEL_APORT2XCH15,
  acmpInputAPORT2YCH16 = _ACMP_INPUTSEL_POSSEL_APORT2YCH16,
  acmpInputAPORT2XCH17 = _ACMP_INPUTSEL_POSSEL_APORT2XCH17,
  acmpInputAPORT2YCH18 = _ACMP_INPUTSEL_POSSEL_APORT2YCH18,
  acmpInputAPORT2XCH19 = _ACMP_INPUTSEL_POSSEL_APORT2XCH19,
  acmpInputAPORT2YCH20 = _ACMP_INPUTSEL_POSSEL_APORT2YCH20,
  acmpInputAPORT2XCH21 = _ACMP_INPUTSEL_POSSEL_APORT2XCH21,
  acmpInputAPORT2YCH22 = _ACMP_INPUTSEL_POSSEL_APORT2YCH22,
  acmpInputAPORT2XCH23 = _ACMP_INPUTSEL_POSSEL_APORT2XCH23,
  acmpInputAPORT2YCH24 = _ACMP_INPUTSEL_POSSEL_APORT2YCH24,
  acmpInputAPORT2XCH25 = _ACMP_INPUTSEL_POSSEL_APORT2XCH25,
  acmpInputAPORT2YCH26 = _ACMP_INPUTSEL_POSSEL_APORT2YCH26,
  acmpInputAPORT2XCH27 = _ACMP_INPUTSEL_POSSEL_APORT2XCH27,
  acmpInputAPORT2YCH28 = _ACMP_INPUTSEL_POSSEL_APORT2YCH28,
  acmpInputAPORT2XCH29 = _ACMP_INPUTSEL_POSSEL_APORT2XCH29,
  acmpInputAPORT2YCH30 = _ACMP_INPUTSEL_POSSEL_APORT2YCH30,
  acmpInputAPORT2XCH31 = _ACMP_INPUTSEL_POSSEL_APORT2XCH31,
  acmpInputAPORT3XCH0  = _ACMP_INPUTSEL_POSSEL_APORT3XCH0,
  acmpInputAPORT3YCH1  = _ACMP_INPUTSEL_POSSEL_APORT3YCH1,
  acmpInputAPORT3XCH2  = _ACMP_INPUTSEL_POSSEL_APORT3XCH2,
  acmpInputAPORT3YCH3  = _ACMP_INPUTSEL_POSSEL_APORT3YCH3,
  acmpInputAPORT3XCH4  = _ACMP_INPUTSEL_POSSEL_APORT3XCH4,
  acmpInputAPORT3YCH5  = _ACMP_INPUTSEL_POSSEL_APORT3YCH5,
  acmpInputAPORT3XCH6  = _ACMP_INPUTSEL_POSSEL_APORT3XCH6,
  acmpInputAPORT3YCH7  = _ACMP_INPUTSEL_POSSEL_APORT3YCH7,
  acmpInputAPORT3XCH8  = _ACMP_INPUTSEL_POSSEL_APORT3XCH8,
  acmpInputAPORT3YCH9  = _ACMP_INPUTSEL_POSSEL_APORT3YCH9,
  acmpInputAPORT3XCH10 = _ACMP_INPUTSEL_POSSEL_APORT3XCH10,
  acmpInputAPORT3YCH11 = _ACMP_INPUTSEL_POSSEL_APORT3YCH11,
  acmpInputAPORT3XCH12 = _ACMP_INPUTSEL_POSSEL_APORT3XCH12,
  acmpInputAPORT3YCH13 = _ACMP_INPUTSEL_POSSEL_APORT3YCH13,
  acmpInputAPORT3XCH14 = _ACMP_INPUTSEL_POSSEL_APORT3XCH14,
  acmpInputAPORT3YCH15 = _ACMP_INPUTSEL_POSSEL_APORT3YCH15,
  acmpInputAPORT3XCH16 = _ACMP_INPUTSEL_POSSEL_APORT3XCH16,
  acmpInputAPORT3YCH17 = _ACMP_INPUTSEL_POSSEL_APORT3YCH17,
  acmpInputAPORT3XCH18 = _ACMP_INPUTSEL_POSSEL_APORT3XCH18,
  acmpInputAPORT3YCH19 = _ACMP_INPUTSEL_POSSEL_APORT3YCH19,
  acmpInputAPORT3XCH20 = _ACMP_INPUTSEL_POSSEL_APORT3XCH20,
  acmpInputAPORT3YCH21 = _ACMP_INPUTSEL_POSSEL_APORT3YCH21,
  acmpInputAPORT3XCH22 = _ACMP_INPUTSEL_POSSEL_APORT3XCH22,
  acmpInputAPORT3YCH23 = _ACMP_INPUTSEL_POSSEL_APORT3YCH23,
  acmpInputAPORT3XCH24 = _ACMP_INPUTSEL_POSSEL_APORT3XCH24,
  acmpInputAPORT3YCH25 = _ACMP_INPUTSEL_POSSEL_APORT3YCH25,
  acmpInputAPORT3XCH26 = _ACMP_INPUTSEL_POSSEL_APORT3XCH26,
  acmpInputAPORT3YCH27 = _ACMP_INPUTSEL_POSSEL_APORT3YCH27,
  acmpInputAPORT3XCH28 = _ACMP_INPUTSEL_POSSEL_APORT3XCH28,
  acmpInputAPORT3YCH29 = _ACMP_INPUTSEL_POSSEL_APORT3YCH29,
  acmpInputAPORT3XCH30 = _ACMP_INPUTSEL_POSSEL_APORT3XCH30,
  acmpInputAPORT3YCH31 = _ACMP_INPUTSEL_POSSEL_APORT3YCH31,
  acmpInputAPORT4YCH0  = _ACMP_INPUTSEL_POSSEL_APORT4YCH0,
  acmpInputAPORT4XCH1  = _ACMP_INPUTSEL_POSSEL_APORT4XCH1,
  acmpInputAPORT4YCH2  = _ACMP_INPUTSEL_POSSEL_APORT4YCH2,
  acmpInputAPORT4XCH3  = _ACMP_INPUTSEL_POSSEL_APORT4XCH3,
  acmpInputAPORT4YCH4  = _ACMP_INPUTSEL_POSSEL_APORT4YCH4,
  acmpInputAPORT4XCH5  = _ACMP_INPUTSEL_POSSEL_APORT4XCH5,
  acmpInputAPORT4YCH6  = _ACMP_INPUTSEL_POSSEL_APORT4YCH6,
  acmpInputAPORT4XCH7  = _ACMP_INPUTSEL_POSSEL_APORT4XCH7,
  acmpInputAPORT4YCH8  = _ACMP_INPUTSEL_POSSEL_APORT4YCH8,
  acmpInputAPORT4XCH9  = _ACMP_INPUTSEL_POSSEL_APORT4XCH9,
  acmpInputAPORT4YCH10 = _ACMP_INPUTSEL_POSSEL_APORT4YCH10,
  acmpInputAPORT4XCH11 = _ACMP_INPUTSEL_POSSEL_APORT4XCH11,
  acmpInputAPORT4YCH12 = _ACMP_INPUTSEL_POSSEL_APORT4YCH12,
  acmpInputAPORT4XCH13 = _ACMP_INPUTSEL_POSSEL_APORT4XCH13,
  acmpInputAPORT4YCH16 = _ACMP_INPUTSEL_POSSEL_APORT4YCH16,
  acmpInputAPORT4XCH17 = _ACMP_INPUTSEL_POSSEL_APORT4XCH17,
  acmpInputAPORT4YCH18 = _ACMP_INPUTSEL_POSSEL_APORT4YCH18,
  acmpInputAPORT4XCH19 = _ACMP_INPUTSEL_POSSEL_APORT4XCH19,
  acmpInputAPORT4YCH20 = _ACMP_INPUTSEL_POSSEL_APORT4YCH20,
  acmpInputAPORT4XCH21 = _ACMP_INPUTSEL_POSSEL_APORT4XCH21,
  acmpInputAPORT4YCH22 = _ACMP_INPUTSEL_POSSEL_APORT4YCH22,
  acmpInputAPORT4XCH23 = _ACMP_INPUTSEL_POSSEL_APORT4XCH23,
  acmpInputAPORT4YCH24 = _ACMP_INPUTSEL_POSSEL_APORT4YCH24,
  acmpInputAPORT4XCH25 = _ACMP_INPUTSEL_POSSEL_APORT4XCH25,
  acmpInputAPORT4YCH26 = _ACMP_INPUTSEL_POSSEL_APORT4YCH26,
  acmpInputAPORT4XCH27 = _ACMP_INPUTSEL_POSSEL_APORT4XCH27,
  acmpInputAPORT4YCH28 = _ACMP_INPUTSEL_POSSEL_APORT4YCH28,
  acmpInputAPORT4XCH29 = _ACMP_INPUTSEL_POSSEL_APORT4XCH29,
  acmpInputAPORT4YCH30 = _ACMP_INPUTSEL_POSSEL_APORT4YCH30,
  acmpInputAPORT4YCH14 = _ACMP_INPUTSEL_POSSEL_APORT4YCH14,
  acmpInputAPORT4XCH15 = _ACMP_INPUTSEL_POSSEL_APORT4XCH15,
  acmpInputAPORT4XCH31 = _ACMP_INPUTSEL_POSSEL_APORT4XCH31,
#if defined(_ACMP_INPUTSEL_POSSEL_DACOUT0)
  acmpInputDACOUT0   = _ACMP_INPUTSEL_POSSEL_DACOUT0,
#endif
#if defined(_ACMP_INPUTSEL_POSSEL_DACOUT1)
  acmpInputDACOUT1   = _ACMP_INPUTSEL_POSSEL_DACOUT1,
#endif
  acmpInputVLP       = _ACMP_INPUTSEL_POSSEL_VLP,
  acmpInputVBDIV     = _ACMP_INPUTSEL_POSSEL_VBDIV,
  acmpInputVADIV     = _ACMP_INPUTSEL_POSSEL_VADIV,
  acmpInputVDD       = _ACMP_INPUTSEL_POSSEL_VDD,
  acmpInputVSS       = _ACMP_INPUTSEL_POSSEL_VSS,
} ACMP_Channel_TypeDef;
#else
/** ACMP inputs. Note that scaled VDD and bandgap references can only be used
 *  as negative inputs. */
typedef enum {
  /** Channel 0 */
  acmpChannel0    = _ACMP_INPUTSEL_NEGSEL_CH0,
  /** Channel 1 */
  acmpChannel1    = _ACMP_INPUTSEL_NEGSEL_CH1,
  /** Channel 2 */
  acmpChannel2    = _ACMP_INPUTSEL_NEGSEL_CH2,
  /** Channel 3 */
  acmpChannel3    = _ACMP_INPUTSEL_NEGSEL_CH3,
  /** Channel 4 */
  acmpChannel4    = _ACMP_INPUTSEL_NEGSEL_CH4,
  /** Channel 5 */
  acmpChannel5    = _ACMP_INPUTSEL_NEGSEL_CH5,
  /** Channel 6 */
  acmpChannel6    = _ACMP_INPUTSEL_NEGSEL_CH6,
  /** Channel 7 */
  acmpChannel7    = _ACMP_INPUTSEL_NEGSEL_CH7,
  /** 1.25 V internal reference */
  acmpChannel1V25 = _ACMP_INPUTSEL_NEGSEL_1V25,
  /** 2.5 V internal reference */
  acmpChannel2V5  = _ACMP_INPUTSEL_NEGSEL_2V5,
  /** Scaled VDD reference */
  acmpChannelVDD  = _ACMP_INPUTSEL_NEGSEL_VDD,

#if defined(_ACMP_INPUTSEL_NEGSEL_DAC0CH0)
  /** DAC0 channel 0 */
  acmpChannelDAC0Ch0 = _ACMP_INPUTSEL_NEGSEL_DAC0CH0,
#endif

#if defined(_ACMP_INPUTSEL_NEGSEL_DAC0CH1)
  /** DAC0 channel 1 */
  acmpChannelDAC0Ch1 = _ACMP_INPUTSEL_NEGSEL_DAC0CH1,
#endif

#if defined(_ACMP_INPUTSEL_NEGSEL_CAPSENSE)
  /** Capacitive sense mode */
  acmpChannelCapSense = _ACMP_INPUTSEL_NEGSEL_CAPSENSE,
#endif
} ACMP_Channel_TypeDef;
#endif

#if defined(_ACMP_EXTIFCTRL_MASK)
/**
 * ACMP external input select. This type is used to select which APORT is
 * used by an external module, such as LESENSE, when it's taking control over
 * the ACMP input.
 */
typedef enum {
  acmpExternalInputAPORT0X  = _ACMP_EXTIFCTRL_APORTSEL_APORT0X,
  acmpExternalInputAPORT0Y  = _ACMP_EXTIFCTRL_APORTSEL_APORT0Y,
  acmpExternalInputAPORT1X  = _ACMP_EXTIFCTRL_APORTSEL_APORT1X,
  acmpExternalInputAPORT1Y  = _ACMP_EXTIFCTRL_APORTSEL_APORT1Y,
  acmpExternalInputAPORT1XY = _ACMP_EXTIFCTRL_APORTSEL_APORT1XY,
  acmpExternalInputAPORT2X  = _ACMP_EXTIFCTRL_APORTSEL_APORT2X,
  acmpExternalInputAPORT2Y  = _ACMP_EXTIFCTRL_APORTSEL_APORT2Y,
  acmpExternalInputAPORT2YX = _ACMP_EXTIFCTRL_APORTSEL_APORT2YX,
  acmpExternalInputAPORT3X  = _ACMP_EXTIFCTRL_APORTSEL_APORT3X,
  acmpExternalInputAPORT3Y  = _ACMP_EXTIFCTRL_APORTSEL_APORT3Y,
  acmpExternalInputAPORT3XY = _ACMP_EXTIFCTRL_APORTSEL_APORT3XY,
  acmpExternalInputAPORT4X  = _ACMP_EXTIFCTRL_APORTSEL_APORT4X,
  acmpExternalInputAPORT4Y  = _ACMP_EXTIFCTRL_APORTSEL_APORT4Y,
  acmpExternalInputAPORT4YX = _ACMP_EXTIFCTRL_APORTSEL_APORT4YX,
} ACMP_ExternalInput_Typedef;
#endif

/*******************************************************************************
 ******************************   STRUCTS   ************************************
 ******************************************************************************/

/** Capsense initialization structure. */
typedef struct {
#if defined(_ACMP_CTRL_FULLBIAS_MASK)
  /** Full-bias current. See the ACMP chapter about bias and response time in
   *  the reference manual for details. */
  bool                          fullBias;
#endif

#if defined(_ACMP_CTRL_HALFBIAS_MASK)
  /** Half-bias current. See the ACMP chapter about bias and response time in
   *  the reference manual for details. */
  bool                          halfBias;
#endif

  /** Bias current. See the ACMP chapter about bias and response time in the
   *  reference manual for details. */
  uint32_t                      biasProg;

#if defined(_ACMP_CTRL_WARMTIME_MASK)
  /** Warmup time, which is measured in HFPERCLK cycles and should be
   *  about 10 us in wall clock time. */
  ACMP_WarmTime_TypeDef         warmTime;
#endif

#if defined(_ACMP_CTRL_HYSTSEL_MASK) \
  ||  defined(_ACMP_CFG_HYST_MASK)
  /** Hysteresis level. */
  ACMP_HysteresisLevel_TypeDef  hysteresisLevel;
#else
  /** Hysteresis level when ACMP output is 0. */
  ACMP_HysteresisLevel_TypeDef  hysteresisLevel_0;

  /** Hysteresis level when ACMP output is 1. */
  ACMP_HysteresisLevel_TypeDef  hysteresisLevel_1;
#endif

  /** A resistor used in the capacative sensing circuit. For values see
   *  the device data sheet. */
  ACMP_CapsenseResistor_TypeDef resistor;

#if defined(_ACMP_INPUTSEL_LPREF_MASK)
  /** Low-power reference enabled. This setting, if enabled, reduces the
   *  power used by VDD and bandgap references. */
  bool                          lowPowerReferenceEnabled;
#endif

#if defined(_ACMP_INPUTCTRL_VREFDIV_MASK)
  /** VDD division factor. VREFOUT = VREFIN * (VREFDIV / 63).
   *  Valid values are in the 0-63 range. */
  uint32_t                      vrefDiv;
#elif defined(_ACMP_INPUTSEL_VDDLEVEL_MASK)
  /** VDD reference value. VDD_SCALED = (VDD * VDDLEVEL) / 63.
   *  Valid values are in the 0-63 range. */
  uint32_t                      vddLevel;
#else
  /**
   * This value configures the upper voltage threshold of the capsense
   * oscillation rail.
   *
   * The voltage threshold is calculated as follows:
   *   VDD * (vddLevelHigh + 1) / 64
   */
  uint32_t                      vddLevelHigh;

  /**
   * This value configures the lower voltage threshold of the capsense
   * oscillation rail.
   *
   * The voltage threshold is calculated as follows:
   *   VDD * (vddLevelLow + 1) / 64
   */
  uint32_t                      vddLevelLow;
#endif

  /** If true, ACMP is enabled after configuration. */
  bool                          enable;
} ACMP_CapsenseInit_TypeDef;

/** A default configuration for capacitive sense mode initialization. */
#if defined(_ACMP_CFG_MASK)
#define ACMP_CAPSENSE_INIT_DEFAULT                                          \
  {                                                                         \
    0x2,                    /* Using biasProg value of 0x2. */              \
    acmpHysteresisDisabled, /* Disable hysteresis. */                       \
    acmpResistor5,          /* Use internal resistor value 5. */            \
    0x3F,                   /* Set VREFDIV to maximum to disable divide. */ \
    true                    /* Enable after init. */                        \
  }
#elif defined(_ACMP_HYSTERESIS0_HYST_MASK)
#define ACMP_CAPSENSE_INIT_DEFAULT                                            \
  {                                                                           \
    false,              /* Don't use fullBias to lower power consumption. */  \
    0x20,               /* Using biasProg value of 0x20 (32). */              \
    acmpHysteresisLevel8, /* Use hysteresis level 8 when ACMP output is 0. */ \
    acmpHysteresisLevel8, /* Use hysteresis level 8 when ACMP output is 1. */ \
    acmpResistor5,      /* Use internal resistor value 5. */                  \
    0x30,               /* VDD level high. */                                 \
    0x10,               /* VDD level low. */                                  \
    true                /* Enable after initialization. */                    \
  }
#elif defined(_ACMP_CTRL_WARMTIME_MASK)
#define ACMP_CAPSENSE_INIT_DEFAULT                      \
  {                                                     \
    false,            /* fullBias */                    \
    false,            /* halfBias */                    \
    0x7,              /* biasProg */                    \
    acmpWarmTime512,  /* 512 cycle warmup to be safe */ \
    acmpHysteresisLevel5,                               \
    acmpResistor3,                                      \
    false,            /* low power reference */         \
    0x3D,             /* VDD level */                   \
    true              /* Enable after init. */          \
  }
#else
#define ACMP_CAPSENSE_INIT_DEFAULT              \
  {                                             \
    false,            /* fullBias */            \
    false,            /* halfBias */            \
    0x7,              /* biasProg */            \
    acmpHysteresisLevel5,                       \
    acmpResistor3,                              \
    false,            /* low power reference */ \
    0x3D,             /* VDD level */           \
    true              /* Enable after init. */  \
  }
#endif

/** ACMP initialization structure. */
typedef struct {
#if defined(_ACMP_CTRL_FULLBIAS_MASK)
  /** Full-bias current. See the ACMP chapter about bias and response time in
   *  the reference manual for details. */
  bool                         fullBias;
#endif

#if defined(_ACMP_CTRL_HALFBIAS_MASK)
  /** Half-bias current. See the ACMP chapter about bias and response time in
   *  the reference manual for details. */
  bool                         halfBias;
#endif

  /** Bias current. See the ACMP chapter about bias and response time in the
   *  reference manual for details. Valid values are in the range 0-7. */
  uint32_t                     biasProg;

#if defined(_ACMP_CTRL_IFALL_SHIFT)
  /** Enable setting the interrupt flag on the falling edge. */
  bool                         interruptOnFallingEdge;
#endif
#if defined(_ACMP_CTRL_IRISE_SHIFT)
  /** Enable setting the interrupt flag on the rising edge. */
  bool                         interruptOnRisingEdge;
#endif

#if defined(_ACMP_CTRL_INPUTRANGE_MASK) \
  || defined(_ACMP_CFG_INPUTRANGE_MASK)
  /** Input range. Adjust this setting to optimize the performance for a
   *  given input voltage range.  */
  ACMP_InputRange_TypeDef      inputRange;
#endif

#if defined(_ACMP_CTRL_ACCURACY_MASK) \
  || defined(_ACMP_CFG_ACCURACY_MASK)
  /** ACMP accuracy mode. Select the accuracy mode that matches the
   *  required current usage and accuracy requirement. Low accuracy
   *  consumes less current while high accuracy consumes more current. */
  ACMP_Accuracy_TypeDef        accuracy;
#endif

#if defined(_ACMP_CTRL_PWRSEL_MASK)
  /** Select the power source for the ACMP. */
  ACMP_PowerSource_TypeDef     powerSource;
#endif

#if defined(_ACMP_CTRL_WARMTIME_MASK)
  /** Warmup time, which is measured in HFPERCLK cycles and should be
   *  about 10 us in wall clock time. */
  ACMP_WarmTime_TypeDef        warmTime;
#endif

#if defined(_ACMP_CTRL_HYSTSEL_MASK) \
  ||  defined(_ACMP_CFG_HYST_MASK)
  /** Hysteresis level. */
  ACMP_HysteresisLevel_TypeDef hysteresisLevel;
#else
  /** Hysteresis when ACMP output is 0. */
  ACMP_HysteresisLevel_TypeDef hysteresisLevel_0;

  /** Hysteresis when ACMP output is 1. */
  ACMP_HysteresisLevel_TypeDef hysteresisLevel_1;
#endif

#if defined(_ACMP_INPUTSEL_VLPSEL_MASK)
  /** VLP Input source. Select between using VADIV or VBDIV as the VLP
   *  source. */
  ACMP_VLPInput_Typedef        vlpInput;
#endif

  /** Inactive value emitted by ACMP during warmup. */
  bool                         inactiveValue;

#if defined(_ACMP_INPUTSEL_LPREF_MASK)
  /** Low power reference enabled. This setting, if enabled, reduces the
   *  power used by the VDD and bandgap references. */
  bool                         lowPowerReferenceEnabled;
#endif

#if defined(_ACMP_INPUTCTRL_VREFDIV_MASK)
  /** VDD division factor. VREFOUT = VREFIN * (VREFDIV / 63).
   *  Valid values are in the 0-63 range. */
  uint32_t                     vrefDiv;
#elif defined(_ACMP_INPUTSEL_VDDLEVEL_MASK)
  /** VDD reference value. VDD_SCALED = VDD * VDDLEVEL * 50 mV/3.8 V.
   *  Valid values are in the 0-63 range. */
  uint32_t                     vddLevel;
#endif

  /** If true, ACMP is enabled after configuration. */
  bool                         enable;
} ACMP_Init_TypeDef;

/** Default configuration for ACMP regular initialization. */
#if defined(_ACMP_CFG_MASK)
#define ACMP_INIT_DEFAULT                                                     \
  {                                                                           \
    0x2,                      /* Using biasProg value of 0x2. */              \
    acmpInputRangeFull,       /* Input range from 0 to Vdd. */                \
    acmpAccuracyLow,          /* Low accuracy, less current usage. */         \
    acmpHysteresisDisabled,   /* Disable hysteresis. */                       \
    false,                    /* Output 0 when ACMP is inactive. */           \
    0x3F,                     /* Set VREFDIV to maximum to disable divide. */ \
    true                      /* Enable after init. */                        \
  }
#elif defined(_ACMP_HYSTERESIS0_HYST_MASK)
#define ACMP_INIT_DEFAULT                                                   \
  {                                                                         \
    false,                    /* fullBias */                                \
    0x7,                      /* biasProg */                                \
    false,                    /* No interrupt on falling edge. */           \
    false,                    /* No interrupt on rising edge. */            \
    acmpInputRangeFull,       /* Input range from 0 to VDD. */              \
    acmpAccuracyLow,          /* Low accuracy, less current usage. */       \
    acmpPowerSourceAvdd,      /* Use the AVDD supply. */                    \
    acmpHysteresisLevel5,     /* Use hysteresis level 5 when output is 0 */ \
    acmpHysteresisLevel5,     /* Use hysteresis level 5 when output is 1 */ \
    acmpVLPInputVADIV,        /* Use VADIV as the VLP input source. */      \
    false,                    /* Output 0 when ACMP is inactive. */         \
    true                      /* Enable after init. */                      \
  }
#else
#define ACMP_INIT_DEFAULT                                                   \
  {                                                                         \
    false,            /* fullBias */                                        \
    false,            /* halfBias */                                        \
    0x7,              /* biasProg */                                        \
    false,            /* No interrupt on falling edge. */                   \
    false,            /* No interrupt on rising edge. */                    \
    acmpWarmTime512,  /* 512 cycle warmup to be safe */                     \
    acmpHysteresisLevel5,                                                   \
    false,            /* Disabled emitting inactive value during warmup. */ \
    false,            /* low power reference */                             \
    0x3D,             /* VDD level */                                       \
    true              /* Enable after init. */                              \
  }
#endif

#if defined(_ACMP_INPUTSEL_VASEL_MASK)
/** VA Configuration structure. This structure is used to configure the
 *  VA voltage input source and its dividers. */
typedef struct {
  ACMP_VAInput_TypeDef input; /**< VA voltage input source */

  /**
   * A divider for VA voltage input source when ACMP output is 0. This value is
   * used to divide the VA voltage input source by a specific value. The valid
   * range is between 0 and 63.
   *
   *  VA divided = VA input * (div0 + 1) / 64
   */
  uint32_t             div0;

  /**
   * A divider for VA voltage input source when ACMP output is 1. This value is
   * used to divide the VA voltage input source by a specific value. The valid
   * range is between 0 and 63.
   *
   *  VA divided = VA input * (div1 + 1) / 64
   */
  uint32_t             div1;
} ACMP_VAConfig_TypeDef;

#define ACMP_VACONFIG_DEFAULT                                               \
  {                                                                         \
    acmpVAInputVDD, /* Use VDD as VA voltage input source. */               \
    63,           /* No division of the VA source when ACMP output is 0. */ \
    63,           /* No division of the VA source when ACMP output is 1. */ \
  }
#endif

#if defined(_ACMP_INPUTSEL_VBSEL_MASK)
/** VB Configuration structure. This structure is used to configure the
 *  VB voltage input source and its dividers. */
typedef struct {
  ACMP_VBInput_TypeDef input; /**< VB Voltage input source */

  /**
   * A divider for VB voltage input source when ACMP output is 0. This value is
   * used to divide the VB voltage input source by a specific value. The valid
   * range is between 0 and 63.
   *
   *  VB divided = VB input * (div0 + 1) / 64
   */
  uint32_t             div0;

  /**
   * A divider for VB voltage input source when ACMP output is 1. This value is
   * used to divide the VB voltage input source by a specific value. The valid
   * range is between 0 and 63.
   *
   *  VB divided = VB input * (div1 + 1) / 64
   */
  uint32_t             div1;
} ACMP_VBConfig_TypeDef;

#define ACMP_VBCONFIG_DEFAULT                                                \
  {                                                                          \
    acmpVBInput1V25, /* Use 1.25 V as VB voltage input source. */            \
    63,            /* No division of the VB source when ACMP output is 0. */ \
    63,            /* No division of the VB source when ACMP output is 1. */ \
  }
#endif

/*******************************************************************************
 *****************************   PROTOTYPES   **********************************
 ******************************************************************************/

void ACMP_CapsenseInit(ACMP_TypeDef *acmp, const ACMP_CapsenseInit_TypeDef *init);
void ACMP_CapsenseChannelSet(ACMP_TypeDef *acmp, ACMP_Channel_TypeDef channel);
void ACMP_ChannelSet(ACMP_TypeDef *acmp, ACMP_Channel_TypeDef negSel, ACMP_Channel_TypeDef posSel);
void ACMP_Disable(ACMP_TypeDef *acmp);
void ACMP_Enable(ACMP_TypeDef *acmp);
#if defined(_ACMP_EXTIFCTRL_MASK)
void ACMP_ExternalInputSelect(ACMP_TypeDef *acmp, ACMP_ExternalInput_Typedef aport);
#endif
#if defined(_GPIO_ACMP_ROUTEEN_MASK)
void ACMP_GPIOSetup(ACMP_TypeDef *acmp, GPIO_Port_TypeDef port, unsigned int pin, bool enable, bool invert);
#else
void ACMP_GPIOSetup(ACMP_TypeDef *acmp, uint32_t location, bool enable, bool invert);
#endif
void ACMP_Init(ACMP_TypeDef *acmp, const ACMP_Init_TypeDef *init);
void ACMP_Reset(ACMP_TypeDef *acmp);
#if defined(_ACMP_INPUTSEL_VASEL_MASK)
void ACMP_VASetup(ACMP_TypeDef *acmp, const ACMP_VAConfig_TypeDef *vaconfig);
#endif
#if defined(_ACMP_INPUTSEL_VBSEL_MASK)
void ACMP_VBSetup(ACMP_TypeDef *acmp, const ACMP_VBConfig_TypeDef *vbconfig);
#endif

/***************************************************************************//**
 * @brief
 *   Clear one or more pending ACMP interrupts.
 *
 * @param[in] acmp
 *   A pointer to the ACMP peripheral register block.
 *
 * @param[in] flags
 *   Pending ACMP interrupt source to clear. Use a bitwise logic OR combination
 *   of valid interrupt flags for the ACMP module. The flags can be, for instance,
 *   @ref ACMP_IFC_EDGE or @ref ACMP_IFC_WARMUP.
 ******************************************************************************/
__STATIC_INLINE void ACMP_IntClear(ACMP_TypeDef *acmp, uint32_t flags)
{
#if defined(ACMP_HAS_SET_CLEAR)
  acmp->IF_CLR = flags;
#else
  acmp->IFC = flags;
#endif
}

/***************************************************************************//**
 * @brief
 *   Disable one or more ACMP interrupts.
 *
 * @param[in] acmp
 *   A pointer to the ACMP peripheral register block.
 *
 * @param[in] flags
 *   ACMP interrupt sources to disable. Use a bitwise logic OR combination of
 *   valid interrupt flags for the ACMP module. The flags can be, for instance,
 *   @ref ACMP_IEN_EDGE or @ref ACMP_IEN_WARMUP.
 ******************************************************************************/
__STATIC_INLINE void ACMP_IntDisable(ACMP_TypeDef *acmp, uint32_t flags)
{
  BUS_RegMaskedClear(&(acmp->IEN), flags);
}

/***************************************************************************//**
 * @brief
 *   Enable one or more ACMP interrupts.
 *
 * @note
 *   Depending on the use, a pending interrupt may already be set prior to
 *   enabling the interrupt. Consider using ACMP_IntClear() prior to enabling
 *   if a pending interrupt should be ignored.
 *
 * @param[in] acmp
 *   A pointer to the ACMP peripheral register block.
 *
 * @param[in] flags
 *   ACMP interrupt sources to enable. Use a bitwise logic OR combination of
 *   valid interrupt flags for the ACMP module. The flags can be, for instance,
 *   @ref ACMP_IEN_EDGE or @ref ACMP_IEN_WARMUP.
 ******************************************************************************/
__STATIC_INLINE void ACMP_IntEnable(ACMP_TypeDef *acmp, uint32_t flags)
{
#if defined(ACMP_HAS_SET_CLEAR)
  acmp->IEN_SET = flags;
#else
  acmp->IEN |= flags;
#endif
}

/***************************************************************************//**
 * @brief
 *   Get pending ACMP interrupt flags.
 *
 * @note
 *   This function does not clear event bits.
 *
 * @param[in] acmp
 *   A pointer to the ACMP peripheral register block.
 *
 * @return
 *   Pending ACMP interrupt sources. A bitwise logic OR combination of valid
 *   interrupt flags for the ACMP module. The pending interrupt sources can be,
 *   for instance, @ref ACMP_IF_EDGE or @ref ACMP_IF_WARMUP.
 ******************************************************************************/
__STATIC_INLINE uint32_t ACMP_IntGet(ACMP_TypeDef *acmp)
{
  return acmp->IF;
}

/***************************************************************************//**
 * @brief
 *   Get enabled and pending ACMP interrupt flags.
 *   Useful for handling more interrupt sources in the same interrupt handler.
 *
 * @param[in] acmp
 *   A pointer to the ACMP peripheral register block.
 *
 * @note
 *   This function does not clear interrupt flags.
 *
 * @return
 *   Pending and enabled ACMP interrupt sources.
 *   The return value is the bitwise AND combination of
 *   - the OR combination of enabled interrupt sources in ACMPx_IEN_nnn
 *     register (ACMPx_IEN_nnn) and
 *   - the OR combination of valid interrupt flags of the ACMP module
 *     (ACMPx_IF_nnn).
 ******************************************************************************/
__STATIC_INLINE uint32_t ACMP_IntGetEnabled(ACMP_TypeDef *acmp)
{
  uint32_t tmp;

  /* Store ACMPx->IEN in a temporary variable to define the explicit order
   * of volatile accesses. */
  tmp = acmp->IEN;

  /* Bitwise AND of pending and enabled interrupts. */
  return acmp->IF & tmp;
}

/***************************************************************************//**
 * @brief
 *   Set one or more pending ACMP interrupts from software.
 *
 * @param[in] acmp
 *   A pointer to the ACMP peripheral register block.
 *
 * @param[in] flags
 *   ACMP interrupt sources to set as pending. Use a bitwise logic OR
 *   combination of valid interrupt flags for the ACMP module. The flags can be,
 *   for instance, @ref ACMP_IFS_EDGE or @ref ACMP_IFS_WARMUP.
 ******************************************************************************/
__STATIC_INLINE void ACMP_IntSet(ACMP_TypeDef *acmp, uint32_t flags)
{
#if defined(ACMP_HAS_SET_CLEAR)
  acmp->IF_SET = flags;
#else
  acmp->IFS = flags;
#endif
}

#if defined(_ACMP_INPUTCTRL_MASK)
/***************************************************************************//**
 * @brief
 *   Convert GPIO port/pin to ACMP input selection.
 *
 * @param[in] port
 *   GPIO port
 *
 * @param[in] pin
 *   GPIO pin
 *
 * @return
 *   ACMP input selection
 ******************************************************************************/
__STATIC_INLINE ACMP_Channel_TypeDef ACMP_PortPinToInput(GPIO_Port_TypeDef port, uint8_t pin)
{
  uint32_t input = (((uint32_t) port + (_ACMP_INPUTCTRL_POSSEL_PA0 >> 4)) << 4) | pin;

  return (ACMP_Channel_TypeDef) input;
}
#endif

/** @} (end addtogroup ACMP) */
/** @} (end addtogroup emlib) */

#ifdef __cplusplus
}
#endif

#endif /* defined(ACMP_COUNT) && (ACMP_COUNT > 0) */
#endif /* EM_ACMP_H */
