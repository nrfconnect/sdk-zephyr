/*
 * Copyright (c) 2016 Linaro Limited.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _STM32F4_PINMUX_H_
#define _STM32F4_PINMUX_H_

/**
 * @file Header for STM32F4 pin multiplexing helper
 */

/* Port A */
#define STM32F4_PINMUX_FUNC_PA0_PWM2_CH1    \
	(STM32_PINMUX_ALT_FUNC_1 | STM32_PUSHPULL_PULLUP)
#define STM32F4_PINMUX_FUNC_PA0_UART4_TX    \
	(STM32_PINMUX_ALT_FUNC_8 | STM32_PUSHPULL_PULLUP)

#define STM32F4_PINMUX_FUNC_PA1_UART4_RX    \
	(STM32_PINMUX_ALT_FUNC_8 | STM32_PUSHPULL_PULLUP)
#define STM32F4_PINMUX_FUNC_PA1_ETH         \
	(STM32_PINMUX_ALT_FUNC_11 | STM32_PUSHPULL_NOPULL | \
	 STM32_OSPEEDR_VERY_HIGH_SPEED)

#define STM32F4_PINMUX_FUNC_PA2_USART2_TX   \
	(STM32_PINMUX_ALT_FUNC_7 | STM32_PUSHPULL_PULLUP)
#define STM32F4_PINMUX_FUNC_PA2_ETH         \
	(STM32_PINMUX_ALT_FUNC_11 | STM32_PUSHPULL_NOPULL | \
	 STM32_OSPEEDR_VERY_HIGH_SPEED)

#define STM32F4_PINMUX_FUNC_PA3_USART2_RX   \
	(STM32_PINMUX_ALT_FUNC_7 | STM32_PUSHPULL_PULLUP)
#define STM32F4_PINMUX_FUNC_PA3_ETH         \
	(STM32_PINMUX_ALT_FUNC_11 | STM32_PUSHPULL_NOPULL | \
	 STM32_OSPEEDR_VERY_HIGH_SPEED)

#define STM32F4_PINMUX_FUNC_PA4_SPI1_NSS    \
	(STM32_PINMUX_ALT_FUNC_5 | STM32_PUSHPULL_NOPULL)

#define STM32F4_PINMUX_FUNC_PA5_SPI1_SCK    \
	(STM32_PINMUX_ALT_FUNC_5 | STM32_PUSHPULL_NOPULL)

#define STM32F4_PINMUX_FUNC_PA6_SPI1_MISO   \
	(STM32_PINMUX_ALT_FUNC_5 | STM32_PUSHPULL_NOPULL)

#define STM32F4_PINMUX_FUNC_PA7_SPI1_MOSI   \
	(STM32_PINMUX_ALT_FUNC_5 | STM32_PUSHPULL_NOPULL)
#define STM32F4_PINMUX_FUNC_PA7_ETH         \
	(STM32_PINMUX_ALT_FUNC_11 | STM32_PUSHPULL_NOPULL | \
	 STM32_OSPEEDR_VERY_HIGH_SPEED)

#define STM32F4_PINMUX_FUNC_PA8_I2C3_SCL    \
	(STM32_PINMUX_ALT_FUNC_4 | STM32_OPENDRAIN_PULLUP)
#define STM32F4_PINMUX_FUNC_PA8_UART7_RX    \
	(STM32_PINMUX_ALT_FUNC_8 | STM32_PUSHPULL_PULLUP)

#define STM32F4_PINMUX_FUNC_PA9_USART1_TX   \
	(STM32_PINMUX_ALT_FUNC_7 | STM32_PUSHPULL_PULLUP)

#define STM32F4_PINMUX_FUNC_PA10_USART1_RX  \
	(STM32_PINMUX_ALT_FUNC_7 | STM32_PUSHPULL_PULLUP)

#define STM32F4_PINMUX_FUNC_PA11_USART6_TX  \
	(STM32_PINMUX_ALT_FUNC_8 | STM32_PUSHPULL_PULLUP)
#define STM32F4_PINMUX_FUNC_PA11_UART4_RX   \
	(STM32_PINMUX_ALT_FUNC_11 | STM32_PUSHPULL_PULLUP)
#define STM32F4_PINMUX_FUNC_PA11_OTG_FS_DM  \
	(STM32_PINMUX_ALT_FUNC_10 | STM32_PUSHPULL_NOPULL)

#define STM32F4_PINMUX_FUNC_PA12_USART6_RX  \
	(STM32_PINMUX_ALT_FUNC_8 | STM32_PUSHPULL_PULLUP)
#define STM32F4_PINMUX_FUNC_PA12_UART4_TX   \
	(STM32_PINMUX_ALT_FUNC_11 | STM32_PUSHPULL_PULLUP)
#define STM32F4_PINMUX_FUNC_PA12_OTG_FS_DP  \
	(STM32_PINMUX_ALT_FUNC_10 | STM32_PUSHPULL_NOPULL)

#define STM32F4_PINMUX_FUNC_PA15_USART1_TX  \
	(STM32_PINMUX_ALT_FUNC_7 | STM32_PUSHPULL_PULLUP)
#define STM32F4_PINMUX_FUNC_PA15_UART7_TX   \
	(STM32_PINMUX_ALT_FUNC_8 | STM32_PUSHPULL_PULLUP)

/* Port B */
#define STM32F4_PINMUX_FUNC_PB3_USART1_RX   \
	(STM32_PINMUX_ALT_FUNC_7 | STM32_PUSHPULL_PULLUP)
#define STM32F4_PINMUX_FUNC_PB3_UART7_RX    \
	(STM32_PINMUX_ALT_FUNC_8 | STM32_PUSHPULL_PULLUP)
#define STM32F4_PINMUX_FUNC_PB3_I2C2_SDA    \
	(STM32_PINMUX_ALT_FUNC_9 | STM32_OPENDRAIN_PULLUP)

#define STM32F4_PINMUX_FUNC_PB4_UART7_TX    \
	(STM32_PINMUX_ALT_FUNC_8 | STM32_PUSHPULL_PULLUP)
#define STM32F4_PINMUX_FUNC_PB4_I2C3_SDA    \
	(STM32_PINMUX_ALT_FUNC_9 | STM32_OPENDRAIN_PULLUP)

#define STM32F4_PINMUX_FUNC_PB5_UART5_RX    \
	(STM32_PINMUX_ALT_FUNC_11 | STM32_PUSHPULL_PULLUP)

#define STM32F4_PINMUX_FUNC_PB6_PWM4_CH1    \
	(STM32_PINMUX_ALT_FUNC_2 | STM32_PUSHPULL_NOPULL)
#define STM32F4_PINMUX_FUNC_PB6_I2C1_SCL    \
	(STM32_PINMUX_ALT_FUNC_4 | STM32_OPENDRAIN_PULLUP)
#define STM32F4_PINMUX_FUNC_PB6_USART1_TX   \
	(STM32_PINMUX_ALT_FUNC_7 | STM32_PUSHPULL_PULLUP)
#define STM32F4_PINMUX_FUNC_PB6_UART5_TX    \
	(STM32_PINMUX_ALT_FUNC_11 | STM32_PUSHPULL_PULLUP)

#define STM32F4_PINMUX_FUNC_PB7_PWM4_CH2    \
	(STM32_PINMUX_ALT_FUNC_2 | STM32_PUSHPULL_NOPULL)
#define STM32F4_PINMUX_FUNC_PB7_I2C1_SDA    \
	(STM32_PINMUX_ALT_FUNC_4 | STM32_OPENDRAIN_PULLUP)
#define STM32F4_PINMUX_FUNC_PB7_USART1_RX   \
	(STM32_PINMUX_ALT_FUNC_7 | STM32_PUSHPULL_PULLUP)

#define STM32F4_PINMUX_FUNC_PB8_PWM4_CH3    \
	(STM32_PINMUX_ALT_FUNC_2 | STM32_PUSHPULL_NOPULL)
#define STM32F4_PINMUX_FUNC_PB8_I2C1_SCL    \
	(STM32_PINMUX_ALT_FUNC_4 | STM32_OPENDRAIN_PULLUP)
#define STM32F4_PINMUX_FUNC_PB8_UART5_RX    \
	(STM32_PINMUX_ALT_FUNC_11 | STM32_PUSHPULL_PULLUP)

#define STM32F4_PINMUX_FUNC_PB9_PWM4_CH4    \
	(STM32_PINMUX_ALT_FUNC_2 | STM32_PUSHPULL_NOPULL)
#define STM32F4_PINMUX_FUNC_PB9_I2C1_SDA    \
	(STM32_PINMUX_ALT_FUNC_4 | STM32_OPENDRAIN_PULLUP)
#define STM32F4_PINMUX_FUNC_PB9_I2C2_SDA    \
	(STM32_PINMUX_ALT_FUNC_9 | STM32_OPENDRAIN_PULLUP)
#define STM32F4_PINMUX_FUNC_PB9_UART5_TX    \
	(STM32_PINMUX_ALT_FUNC_11 | STM32_PUSHPULL_PULLUP)

#define STM32F4_PINMUX_FUNC_PB10_I2C2_SCL   \
	(STM32_PINMUX_ALT_FUNC_4 | STM32_OPENDRAIN_PULLUP)
#define STM32F4_PINMUX_FUNC_PB10_USART3_TX  \
	(STM32_PINMUX_ALT_FUNC_7 | STM32_PUSHPULL_PULLUP)

#define STM32F4_PINMUX_FUNC_PB11_I2C2_SDA   \
	(STM32_PINMUX_ALT_FUNC_4 | STM32_OPENDRAIN_PULLUP)
#define STM32F4_PINMUX_FUNC_PB11_USART3_RX  \
	(STM32_PINMUX_ALT_FUNC_7 | STM32_PUSHPULL_PULLUP)
#define STM32F4_PINMUX_FUNC_PB11_ETH        \
	(STM32_PINMUX_ALT_FUNC_11 | STM32_PUSHPULL_NOPULL | \
	 STM32_OSPEEDR_VERY_HIGH_SPEED)

#define STM32F4_PINMUX_FUNC_PB12_SPI2_NSS   \
	(STM32_PINMUX_ALT_FUNC_5 | STM32_PUSHPULL_NOPULL)
#define STM32F4_PINMUX_FUNC_PB12_UART5_RX   \
	(STM32_PINMUX_ALT_FUNC_11 | STM32_PUSHPULL_PULLUP)
#define STM32F4_PINMUX_FUNC_PB12_ETH        \
	(STM32_PINMUX_ALT_FUNC_11 | STM32_PUSHPULL_NOPULL | \
	 STM32_OSPEEDR_VERY_HIGH_SPEED)

#define STM32F4_PINMUX_FUNC_PB13_SPI2_SCK   \
	(STM32_PINMUX_ALT_FUNC_5 | STM32_PUSHPULL_NOPULL)
#define STM32F4_PINMUX_FUNC_PB13_UART5_TX   \
	(STM32_PINMUX_ALT_FUNC_11 | STM32_PUSHPULL_NOPULL)
#define STM32F4_PINMUX_FUNC_PB13_ETH        \
	(STM32_PINMUX_ALT_FUNC_11 | STM32_PUSHPULL_NOPULL | \
	 STM32_OSPEEDR_VERY_HIGH_SPEED)

#define STM32F4_PINMUX_FUNC_PB14_SPI2_MISO  \
	(STM32_PINMUX_ALT_FUNC_5 | STM32_PUSHPULL_NOPULL)

#define STM32F4_PINMUX_FUNC_PB15_SPI2_MOSI  \
	(STM32_PINMUX_ALT_FUNC_5 | STM32_PUSHPULL_NOPULL)

/* Port C */
#define STM32F4_PINMUX_FUNC_PC1_ETH         \
	(STM32_PINMUX_ALT_FUNC_11 | STM32_PUSHPULL_NOPULL | \
	 STM32_OSPEEDR_VERY_HIGH_SPEED)

#define STM32F4_PINMUX_FUNC_PC4_ETH         \
	(STM32_PINMUX_ALT_FUNC_11 | STM32_PUSHPULL_NOPULL | \
	 STM32_OSPEEDR_VERY_HIGH_SPEED)

#define STM32F4_PINMUX_FUNC_PC5_USART3_RX   \
	(STM32_PINMUX_ALT_FUNC_7 | STM32_PUSHPULL_PULLUP)
#define STM32F4_PINMUX_FUNC_PC5_ETH         \
	(STM32_PINMUX_ALT_FUNC_11 | STM32_PUSHPULL_NOPULL | \
	 STM32_OSPEEDR_VERY_HIGH_SPEED)

#define STM32F4_PINMUX_FUNC_PC6_PWM3_CH1    \
	(STM32_PINMUX_ALT_FUNC_2 | STM32_PUSHPULL_NOPULL)
#define STM32F4_PINMUX_FUNC_PC6_USART6_TX   \
	(STM32_PINMUX_ALT_FUNC_8 | STM32_PUSHPULL_PULLUP)

#define STM32F4_PINMUX_FUNC_PC7_PWM3_CH2    \
	(STM32_PINMUX_ALT_FUNC_2 | STM32_PUSHPULL_NOPULL)
#define STM32F4_PINMUX_FUNC_PC7_USART6_RX   \
	(STM32_PINMUX_ALT_FUNC_8 | STM32_PUSHPULL_PULLUP)

#define STM32F4_PINMUX_FUNC_PC8_PWM3_CH3    \
	(STM32_PINMUX_ALT_FUNC_2 | STM32_PUSHPULL_NOPULL)

#define STM32F4_PINMUX_FUNC_PC9_PWM3_CH4    \
	(STM32_PINMUX_ALT_FUNC_2 | STM32_PUSHPULL_NOPULL)
#define STM32F4_PINMUX_FUNC_PC9_I2C3_SDA    \
	(STM32_PINMUX_ALT_FUNC_4 | STM32_OPENDRAIN_PULLUP)

#define STM32F4_PINMUX_FUNC_PC10_USART3_TX  \
	(STM32_PINMUX_ALT_FUNC_7 | STM32_PUSHPULL_NOPULL)

#define STM32F4_PINMUX_FUNC_PC11_USART3_RX  \
	(STM32_PINMUX_ALT_FUNC_7 | STM32_PUSHPULL_PULLUP)
#define STM32F4_PINMUX_FUNC_PC11_UART4_RX   \
	(STM32_PINMUX_ALT_FUNC_8 | STM32_PUSHPULL_PULLUP)

#define STM32F4_PINMUX_FUNC_PC12_UART5_TX   \
	(STM32_PINMUX_ALT_FUNC_8 | STM32_PUSHPULL_PULLUP)

/* Port D */
#define STM32F4_PINMUX_FUNC_PD0_UART4_RX    \
	(STM32_PINMUX_ALT_FUNC_11 | STM32_PUSHPULL_PULLUP)

#define STM32F4_PINMUX_FUNC_PD2_UART5_RX    \
	(STM32_PINMUX_ALT_FUNC_8 | STM32_PUSHPULL_PULLUP)

#define STM32F4_PINMUX_FUNC_PD5_USART2_TX   \
	(STM32_PINMUX_ALT_FUNC_7 | STM32_PUSHPULL_NOPULL)

#define STM32F4_PINMUX_FUNC_PD6_USART2_RX   \
	(STM32_PINMUX_ALT_FUNC_7 | STM32_PUSHPULL_PULLUP)

#define STM32F4_PINMUX_FUNC_PD8_USART3_TX   \
	(STM32_PINMUX_ALT_FUNC_7 | STM32_PUSHPULL_NOPULL)

#define STM32F4_PINMUX_FUNC_PD9_USART3_RX   \
	(STM32_PINMUX_ALT_FUNC_7 | STM32_PUSHPULL_PULLUP)

#define STM32F4_PINMUX_FUNC_PD10_UART4_TX   \
	(STM32_PINMUX_ALT_FUNC_8 | STM32_PUSHPULL_PULLUP)

#define STM32F4_PINMUX_FUNC_PD14_UART9_RX   \
	(STM32_PINMUX_ALT_FUNC_11 | STM32_PUSHPULL_PULLUP)

#define STM32F4_PINMUX_FUNC_PD15_UART9_TX   \
	(STM32_PINMUX_ALT_FUNC_11 | STM32_PUSHPULL_NOPULL)

/* Port E */
#define STM32F4_PINMUX_FUNC_PE0_UART8_RX    \
	(STM32_PINMUX_ALT_FUNC_8 | STM32_PUSHPULL_PULLUP)

#define STM32F4_PINMUX_FUNC_PE1_UART8_TX    \
	(STM32_PINMUX_ALT_FUNC_8 | STM32_PUSHPULL_PULLUP)

#define STM32F4_PINMUX_FUNC_PE2_UART10_RX   \
	(STM32_PINMUX_ALT_FUNC_11 | STM32_PUSHPULL_PULLUP)

#define STM32F4_PINMUX_FUNC_PE3_UART10_TX   \
	(STM32_PINMUX_ALT_FUNC_11 | STM32_PUSHPULL_NOPULL)

#define STM32F4_PINMUX_FUNC_PE7_UART7_RX    \
	(STM32_PINMUX_ALT_FUNC_8 | STM32_PUSHPULL_PULLUP)

#define STM32F4_PINMUX_FUNC_PE8_UART7_TX    \
	(STM32_PINMUX_ALT_FUNC_8 | STM32_PUSHPULL_PULLUP)

/* Port F */
#define STM32F4_PINMUX_FUNC_PF0_I2C2_SDA    \
	(STM32_PINMUX_ALT_FUNC_4 | STM32_OPENDRAIN_PULLUP)

#define STM32F4_PINMUX_FUNC_PF1_I2C2_SCL    \
	(STM32_PINMUX_ALT_FUNC_4 | STM32_OPENDRAIN_PULLUP)

#define STM32F4_PINMUX_FUNC_PF6_UART7_RX    \
	(STM32_PINMUX_ALT_FUNC_8 | STM32_PUSHPULL_PULLUP)

#define STM32F4_PINMUX_FUNC_PF7_UART7_TX    \
	(STM32_PINMUX_ALT_FUNC_8 | STM32_PUSHPULL_PULLUP)

#define STM32F4_PINMUX_FUNC_PF8_UART8_RX    \
	(STM32_PINMUX_ALT_FUNC_8 | STM32_PUSHPULL_PULLUP)

#define STM32F4_PINMUX_FUNC_PF9_UART8_TX    \
	(STM32_PINMUX_ALT_FUNC_8 | STM32_PUSHPULL_PULLUP)

/* Port G */
#define STM32F4_PINMUX_FUNC_PG0_UART9_RX    \
	(STM32_PINMUX_ALT_FUNC_11 | STM32_PUSHPULL_PULLUP)

#define STM32F4_PINMUX_FUNC_PG1_UART9_TX    \
	(STM32_PINMUX_ALT_FUNC_11 | STM32_PUSHPULL_NOPULL)

#define STM32F4_PINMUX_FUNC_PG9_USART6_RX   \
	(STM32_PINMUX_ALT_FUNC_8 | STM32_PUSHPULL_PULLUP)

#define STM32F4_PINMUX_FUNC_PG11_UART10_RX  \
	(STM32_PINMUX_ALT_FUNC_11 | STM32_PUSHPULL_PULLUP)
#define STM32F4_PINMUX_FUNC_PG11_ETH        \
	(STM32_PINMUX_ALT_FUNC_11 | STM32_PUSHPULL_NOPULL | \
	 STM32_OSPEEDR_VERY_HIGH_SPEED)

#define STM32F4_PINMUX_FUNC_PG12_UART10_TX  \
	(STM32_PINMUX_ALT_FUNC_11 | STM32_PUSHPULL_NOPULL)

#define STM32F4_PINMUX_FUNC_PG13_ETH        \
	(STM32_PINMUX_ALT_FUNC_11 | STM32_PUSHPULL_NOPULL | \
	 STM32_OSPEEDR_VERY_HIGH_SPEED)

#define STM32F4_PINMUX_FUNC_PG14_USART6_TX  \
	(STM32_PINMUX_ALT_FUNC_8 | STM32_PUSHPULL_PULLUP)
#define STM32F4_PINMUX_FUNC_PG14_ETH        \
	(STM32_PINMUX_ALT_FUNC_11 | STM32_PUSHPULL_NOPULL | \
	 STM32_OSPEEDR_VERY_HIGH_SPEED)

/* Port H */
#define STM32F4_PINMUX_FUNC_PH4_I2C2_SCL    \
	(STM32_PINMUX_ALT_FUNC_4 | STM32_OPENDRAIN_PULLUP)

#define STM32F4_PINMUX_FUNC_PH5_I2C2_SDA    \
	(STM32_PINMUX_ALT_FUNC_4 | STM32_OPENDRAIN_PULLUP)

#define STM32F4_PINMUX_FUNC_PH7_I2C3_SCL    \
	(STM32_PINMUX_ALT_FUNC_4 | STM32_OPENDRAIN_PULLUP)

#define STM32F4_PINMUX_FUNC_PH8_I2C3_SDA    \
	(STM32_PINMUX_ALT_FUNC_4 | STM32_OPENDRAIN_PULLUP)

#endif /* _STM32F4_PINMUX_H_ */
