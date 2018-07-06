/**
  ******************************************************************************
  * @file    stm32f1xx_hal_conf.h
  * @author  MCD Application Team
  * @brief   HAL configuration template file.
  *          This file should be copied to the application folder and renamed
  *          to stm32f1xx_hal_conf.h.
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; COPYRIGHT(c) 2017 STMicroelectronics</center></h2>
  *
  * Redistribution and use in source and binary forms, with or without modification,
  * are permitted provided that the following conditions are met:
  *   1. Redistributions of source code must retain the above copyright notice,
  *      this list of conditions and the following disclaimer.
  *   2. Redistributions in binary form must reproduce the above copyright notice,
  *      this list of conditions and the following disclaimer in the documentation
  *      and/or other materials provided with the distribution.
  *   3. Neither the name of STMicroelectronics nor the names of its contributors
  *      may be used to endorse or promote products derived from this software
  *      without specific prior written permission.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
  * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
  * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
  * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
  * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
  * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
  * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
  *
  ******************************************************************************
  */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __STM32F1xx_HAL_CONF_H
#define __STM32F1xx_HAL_CONF_H

#ifdef __cplusplus
extern "C" {
#endif

/* Exported types ------------------------------------------------------------*/
/* Exported constants --------------------------------------------------------*/

/* ########################## Module Selection ############################## */
/**
  * @brief This is the list of modules to be used in the HAL driver
  *
  *     Note:
  *     	HAL_I2C_MODULE_ENABLED had to be disabled due to the HAL
  *     	redefining I2C_SPEED_STANDARD and I2C_SPEED_FAST when the
  *     	I2C LL driver was enabled.
  *
  */
#define HAL_MODULE_ENABLED
#define HAL_ADC_MODULE_ENABLED
#define HAL_CAN_MODULE_ENABLED
#define HAL_CEC_MODULE_ENABLED
#define HAL_CORTEX_MODULE_ENABLED
#define HAL_CRC_MODULE_ENABLED
#define HAL_DAC_MODULE_ENABLED
#define HAL_DMA_MODULE_ENABLED
#define HAL_ETH_MODULE_ENABLED
#define HAL_FLASH_MODULE_ENABLED
#define HAL_GPIO_MODULE_ENABLED
#define HAL_HCD_MODULE_ENABLED
#define HAL_I2S_MODULE_ENABLED
#define HAL_IRDA_MODULE_ENABLED
#define HAL_IWDG_MODULE_ENABLED
#define HAL_NAND_MODULE_ENABLED
#define HAL_NOR_MODULE_ENABLED
#define HAL_PCCARD_MODULE_ENABLED
#define HAL_PCD_MODULE_ENABLED
#define HAL_PWR_MODULE_ENABLED
#define HAL_RCC_MODULE_ENABLED
#define HAL_RTC_MODULE_ENABLED
#define HAL_SD_MODULE_ENABLED
#define HAL_SMARTCARD_MODULE_ENABLED
#define HAL_SPI_MODULE_ENABLED
#define HAL_SRAM_MODULE_ENABLED
#define HAL_TIM_MODULE_ENABLED
#define HAL_UART_MODULE_ENABLED
#define HAL_USART_MODULE_ENABLED
#define HAL_WWDG_MODULE_ENABLED
#define HAL_MMC_MODULE_ENABLED

/* ########################## Oscillator Values adaptation ####################*/
/**
  * @brief Adjust the value of External High Speed oscillator (HSE) used in your application.
  *        This value is used by the RCC HAL module to compute the system frequency
  *        (when HSE is used as system clock source, directly or through the PLL).
  */
#if !defined  (HSE_VALUE)
#if defined(USE_STM3210C_EVAL)
#define HSE_VALUE    25000000U /*!< Value of the External oscillator in Hz */
#else
#define HSE_VALUE    8000000U /*!< Value of the External oscillator in Hz */
#endif
#endif /* HSE_VALUE */

#if !defined  (HSE_STARTUP_TIMEOUT)
#define HSE_STARTUP_TIMEOUT    100U      /*!< Time out for HSE start up, in ms */
#endif /* HSE_STARTUP_TIMEOUT */

/**
  * @brief Internal High Speed oscillator (HSI) value.
  *        This value is used by the RCC HAL module to compute the system frequency
  *        (when HSI is used as system clock source, directly or through the PLL).
  */
#if !defined  (HSI_VALUE)
#define HSI_VALUE              8000000U  /*!< Value of the Internal oscillator in Hz */
#endif /* HSI_VALUE */

/**
  * @brief Internal Low Speed oscillator (LSI) value.
  */
#if !defined  (LSI_VALUE)
#define LSI_VALUE               32000U     /*!< LSI Typical Value in Hz */
#endif /* LSI_VALUE */                     /*!< Value of the Internal Low Speed oscillator in Hz
                                                The real value may vary depending on the variations
                                                in voltage and temperature. */
/**
  * @brief External Low Speed oscillator (LSE) value.
  *        This value is used by the UART, RTC HAL module to compute the system frequency
  */
#if !defined  (LSE_VALUE)
#define LSE_VALUE               32768U    /*!< Value of the External Low Speed oscillator in Hz */
#endif /* LSE_VALUE */

#if !defined  (LSE_STARTUP_TIMEOUT)
#define LSE_STARTUP_TIMEOUT    5000U     /*!< Time out for LSE start up, in ms */
#endif /* LSE_STARTUP_TIMEOUT */

/* Tip: To avoid modifying this file each time you need to use different HSE,
   ===  you can define the HSE value in your toolchain compiler preprocessor. */

/* ########################### System Configuration ######################### */
/**
  * @brief This is the HAL system configuration section
  */
#define  VDD_VALUE                    3300U /*!< Value of VDD in mv */
#define  TICK_INT_PRIORITY            0x0FU /*!< tick interrupt priority */
#define  USE_RTOS                     0U
#define  PREFETCH_ENABLE              1U

/* ########################## Assert Selection ############################## */
/**
  * @brief Uncomment the line below to expanse the "assert_param" macro in the
  *        HAL drivers code
  */
/* #define USE_FULL_ASSERT    1U */

/* ################## Ethernet peripheral configuration ##################### */

/* Section 1 : Ethernet peripheral configuration */

/* MAC ADDRESS: MAC_ADDR0:MAC_ADDR1:MAC_ADDR2:MAC_ADDR3:MAC_ADDR4:MAC_ADDR5 */
#define MAC_ADDR0   2U
#define MAC_ADDR1   0U
#define MAC_ADDR2   0U
#define MAC_ADDR3   0U
#define MAC_ADDR4   0U
#define MAC_ADDR5   0U

/* Definition of the Ethernet driver buffers size and count */
#define ETH_RX_BUF_SIZE                ETH_MAX_PACKET_SIZE /* buffer size for receive               */
#define ETH_TX_BUF_SIZE                ETH_MAX_PACKET_SIZE /* buffer size for transmit              */
#define ETH_RXBUFNB                    8U                  /* 8 Rx buffers of size ETH_RX_BUF_SIZE  */
#define ETH_TXBUFNB                    4U                  /* 4 Tx buffers of size ETH_TX_BUF_SIZE  */

/* Section 2: PHY configuration section */

/* DP83848 PHY Address*/
#define DP83848_PHY_ADDRESS             0x01U
/* PHY Reset delay these values are based on a 1 ms Systick interrupt*/
#define PHY_RESET_DELAY                 0x000000FFU
/* PHY Configuration delay */
#define PHY_CONFIG_DELAY                0x00000FFFU

#define PHY_READ_TO                     0x0000FFFFU
#define PHY_WRITE_TO                    0x0000FFFFU

/* Section 3: Common PHY Registers */

#define PHY_BCR                         ((uint16_t)0x0000)  /*!< Transceiver Basic Control Register   */
#define PHY_BSR                         ((uint16_t)0x0001)  /*!< Transceiver Basic Status Register    */

#define PHY_RESET                       ((uint16_t)0x8000)  /*!< PHY Reset */
#define PHY_LOOPBACK                    ((uint16_t)0x4000)  /*!< Select loop-back mode */
#define PHY_FULLDUPLEX_100M             ((uint16_t)0x2100)  /*!< Set the full-duplex mode at 100 Mb/s */
#define PHY_HALFDUPLEX_100M             ((uint16_t)0x2000)  /*!< Set the half-duplex mode at 100 Mb/s */
#define PHY_FULLDUPLEX_10M              ((uint16_t)0x0100)  /*!< Set the full-duplex mode at 10 Mb/s  */
#define PHY_HALFDUPLEX_10M              ((uint16_t)0x0000)  /*!< Set the half-duplex mode at 10 Mb/s  */
#define PHY_AUTONEGOTIATION             ((uint16_t)0x1000)  /*!< Enable auto-negotiation function     */
#define PHY_RESTART_AUTONEGOTIATION     ((uint16_t)0x0200)  /*!< Restart auto-negotiation function    */
#define PHY_POWERDOWN                   ((uint16_t)0x0800)  /*!< Select the power down mode           */
#define PHY_ISOLATE                     ((uint16_t)0x0400)  /*!< Isolate PHY from MII                 */

#define PHY_AUTONEGO_COMPLETE           ((uint16_t)0x0020)  /*!< Auto-Negotiation process completed   */
#define PHY_LINKED_STATUS               ((uint16_t)0x0004)  /*!< Valid link established               */
#define PHY_JABBER_DETECTION            ((uint16_t)0x0002)  /*!< Jabber condition detected            */

/* Section 4: Extended PHY Registers */

#define PHY_SR                          ((uint16_t)0x0010)  /*!< PHY status register Offset                      */
#define PHY_MICR                        ((uint16_t)0x0011)  /*!< MII Interrupt Control Register                  */
#define PHY_MISR                        ((uint16_t)0x0012)  /*!< MII Interrupt Status and Misc. Control Register */

#define PHY_LINK_STATUS                 ((uint16_t)0x0001)  /*!< PHY Link mask                                   */
#define PHY_SPEED_STATUS                ((uint16_t)0x0002)  /*!< PHY Speed mask                                  */
#define PHY_DUPLEX_STATUS               ((uint16_t)0x0004)  /*!< PHY Duplex mask                                 */

#define PHY_MICR_INT_EN                 ((uint16_t)0x0002)  /*!< PHY Enable interrupts                           */
#define PHY_MICR_INT_OE                 ((uint16_t)0x0001)  /*!< PHY Enable output interrupt events              */

#define PHY_MISR_LINK_INT_EN            ((uint16_t)0x0020)  /*!< Enable Interrupt on change of link status       */
#define PHY_LINK_INTERRUPT              ((uint16_t)0x2000)  /*!< PHY link status interrupt mask                  */

/* ################## SPI peripheral configuration ########################## */

/* CRC FEATURE: Use to activate CRC feature inside HAL SPI Driver
* Activated: CRC code is present inside driver
* Deactivated: CRC code cleaned from driver
*/

#define USE_SPI_CRC                     1U

/* Includes ------------------------------------------------------------------*/
/**
  * @brief Include module's header file
  */

#ifdef HAL_RCC_MODULE_ENABLED
#include "stm32f1xx_hal_rcc.h"
#endif /* HAL_RCC_MODULE_ENABLED */

#ifdef HAL_GPIO_MODULE_ENABLED
#include "stm32f1xx_hal_gpio.h"
#endif /* HAL_GPIO_MODULE_ENABLED */

#ifdef HAL_DMA_MODULE_ENABLED
#include "stm32f1xx_hal_dma.h"
#endif /* HAL_DMA_MODULE_ENABLED */

#ifdef HAL_ETH_MODULE_ENABLED
#include "stm32f1xx_hal_eth.h"
#endif /* HAL_ETH_MODULE_ENABLED */

#ifdef HAL_CAN_MODULE_ENABLED
#include "stm32f1xx_hal_can.h"
#endif /* HAL_CAN_MODULE_ENABLED */

#ifdef HAL_CEC_MODULE_ENABLED
#include "stm32f1xx_hal_cec.h"
#endif /* HAL_CEC_MODULE_ENABLED */

#ifdef HAL_CORTEX_MODULE_ENABLED
#include "stm32f1xx_hal_cortex.h"
#endif /* HAL_CORTEX_MODULE_ENABLED */

#ifdef HAL_ADC_MODULE_ENABLED
#include "stm32f1xx_hal_adc.h"
#endif /* HAL_ADC_MODULE_ENABLED */

#ifdef HAL_CRC_MODULE_ENABLED
#include "stm32f1xx_hal_crc.h"
#endif /* HAL_CRC_MODULE_ENABLED */

#ifdef HAL_DAC_MODULE_ENABLED
#include "stm32f1xx_hal_dac.h"
#endif /* HAL_DAC_MODULE_ENABLED */

#ifdef HAL_FLASH_MODULE_ENABLED
#include "stm32f1xx_hal_flash.h"
#endif /* HAL_FLASH_MODULE_ENABLED */

#ifdef HAL_SRAM_MODULE_ENABLED
#include "stm32f1xx_hal_sram.h"
#endif /* HAL_SRAM_MODULE_ENABLED */

#ifdef HAL_NOR_MODULE_ENABLED
#include "stm32f1xx_hal_nor.h"
#endif /* HAL_NOR_MODULE_ENABLED */

#ifdef HAL_I2C_MODULE_ENABLED
#include "stm32f1xx_hal_i2c.h"
#endif /* HAL_I2C_MODULE_ENABLED */

#ifdef HAL_I2S_MODULE_ENABLED
#include "stm32f1xx_hal_i2s.h"
#endif /* HAL_I2S_MODULE_ENABLED */

#ifdef HAL_IWDG_MODULE_ENABLED
#include "stm32f1xx_hal_iwdg.h"
#endif /* HAL_IWDG_MODULE_ENABLED */

#ifdef HAL_PWR_MODULE_ENABLED
#include "stm32f1xx_hal_pwr.h"
#endif /* HAL_PWR_MODULE_ENABLED */

#ifdef HAL_RTC_MODULE_ENABLED
#include "stm32f1xx_hal_rtc.h"
#endif /* HAL_RTC_MODULE_ENABLED */

#ifdef HAL_PCCARD_MODULE_ENABLED
#include "stm32f1xx_hal_pccard.h"
#endif /* HAL_PCCARD_MODULE_ENABLED */

#ifdef HAL_SD_MODULE_ENABLED
#include "stm32f1xx_hal_sd.h"
#endif /* HAL_SD_MODULE_ENABLED */

#ifdef HAL_NAND_MODULE_ENABLED
#include "stm32f1xx_hal_nand.h"
#endif /* HAL_NAND_MODULE_ENABLED */

#ifdef HAL_SPI_MODULE_ENABLED
#include "stm32f1xx_hal_spi.h"
#endif /* HAL_SPI_MODULE_ENABLED */

#ifdef HAL_TIM_MODULE_ENABLED
#include "stm32f1xx_hal_tim.h"
#endif /* HAL_TIM_MODULE_ENABLED */

#ifdef HAL_UART_MODULE_ENABLED
#include "stm32f1xx_hal_uart.h"
#endif /* HAL_UART_MODULE_ENABLED */

#ifdef HAL_USART_MODULE_ENABLED
#include "stm32f1xx_hal_usart.h"
#endif /* HAL_USART_MODULE_ENABLED */

#ifdef HAL_IRDA_MODULE_ENABLED
#include "stm32f1xx_hal_irda.h"
#endif /* HAL_IRDA_MODULE_ENABLED */

#ifdef HAL_SMARTCARD_MODULE_ENABLED
#include "stm32f1xx_hal_smartcard.h"
#endif /* HAL_SMARTCARD_MODULE_ENABLED */

#ifdef HAL_WWDG_MODULE_ENABLED
#include "stm32f1xx_hal_wwdg.h"
#endif /* HAL_WWDG_MODULE_ENABLED */

#ifdef HAL_PCD_MODULE_ENABLED
#include "stm32f1xx_hal_pcd.h"
#endif /* HAL_PCD_MODULE_ENABLED */

#ifdef HAL_HCD_MODULE_ENABLED
#include "stm32f1xx_hal_hcd.h"
#endif /* HAL_HCD_MODULE_ENABLED */

#ifdef HAL_MMC_MODULE_ENABLED
#include "stm32f1xx_hal_mmc.h"
#endif /* HAL_MMC_MODULE_ENABLED */

/* Exported macro ------------------------------------------------------------*/
#ifdef  USE_FULL_ASSERT
/**
  * @brief  The assert_param macro is used for function's parameters check.
  * @param  expr: If expr is false, it calls assert_failed function
  *         which reports the name of the source file and the source
  *         line number of the call that failed.
  *         If expr is true, it returns no value.
  * @retval None
  */
#define assert_param(expr) ((expr) ? (void)0U : assert_failed((uint8_t *)__FILE__, __LINE__))
/* Exported functions ------------------------------------------------------- */
void assert_failed(uint8_t *file, uint32_t line);
#else
#define assert_param(expr) ((void)0U)
#endif /* USE_FULL_ASSERT */


#ifdef __cplusplus
}
#endif

#endif /* __STM32F1xx_HAL_CONF_H */


/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
