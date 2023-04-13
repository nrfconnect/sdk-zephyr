/*
 * Copyright (c) 2023 TOKITA Hiroshi <tokita.hiroshi@fujitsu.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_INCLUDE_DT_BINDINGS_RPI_PICO_DMA_H_
#define ZEPHYR_INCLUDE_DT_BINDINGS_RPI_PICO_DMA_H_

/*
 * Use lower 6-bit of inverted DREQ value for `slot` cell.
 * Need to be able to work for memory-to-memory transfer
 * with zero, which is the default value.
 */
#define RPI_PICO_DMA_SLOT_TO_DREQ(s) (~(s)&0x3F)
#define RPI_PICO_DMA_DREQ_TO_SLOT    RPI_PICO_DMA_SLOT_TO_DREQ

#define RPI_PICO_DMA_SLOT_PIO0_TX0   RPI_PICO_DMA_DREQ_TO_SLOT(0x00)
#define RPI_PICO_DMA_SLOT_PIO0_TX1   RPI_PICO_DMA_DREQ_TO_SLOT(0x01)
#define RPI_PICO_DMA_SLOT_PIO0_TX2   RPI_PICO_DMA_DREQ_TO_SLOT(0x02)
#define RPI_PICO_DMA_SLOT_PIO0_TX3   RPI_PICO_DMA_DREQ_TO_SLOT(0x03)
#define RPI_PICO_DMA_SLOT_PIO0_RX0   RPI_PICO_DMA_DREQ_TO_SLOT(0x04)
#define RPI_PICO_DMA_SLOT_PIO0_RX1   RPI_PICO_DMA_DREQ_TO_SLOT(0x05)
#define RPI_PICO_DMA_SLOT_PIO0_RX2   RPI_PICO_DMA_DREQ_TO_SLOT(0x06)
#define RPI_PICO_DMA_SLOT_PIO0_RX3   RPI_PICO_DMA_DREQ_TO_SLOT(0x07)
#define RPI_PICO_DMA_SLOT_PIO1_TX0   RPI_PICO_DMA_DREQ_TO_SLOT(0x08)
#define RPI_PICO_DMA_SLOT_PIO1_TX1   RPI_PICO_DMA_DREQ_TO_SLOT(0x09)
#define RPI_PICO_DMA_SLOT_PIO1_TX2   RPI_PICO_DMA_DREQ_TO_SLOT(0x0A)
#define RPI_PICO_DMA_SLOT_PIO1_TX3   RPI_PICO_DMA_DREQ_TO_SLOT(0x0B)
#define RPI_PICO_DMA_SLOT_PIO1_RX0   RPI_PICO_DMA_DREQ_TO_SLOT(0x0C)
#define RPI_PICO_DMA_SLOT_PIO1_RX1   RPI_PICO_DMA_DREQ_TO_SLOT(0x0D)
#define RPI_PICO_DMA_SLOT_PIO1_RX2   RPI_PICO_DMA_DREQ_TO_SLOT(0x0E)
#define RPI_PICO_DMA_SLOT_PIO1_RX3   RPI_PICO_DMA_DREQ_TO_SLOT(0x0F)
#define RPI_PICO_DMA_SLOT_SPI0_TX    RPI_PICO_DMA_DREQ_TO_SLOT(0x10)
#define RPI_PICO_DMA_SLOT_SPI0_RX    RPI_PICO_DMA_DREQ_TO_SLOT(0x11)
#define RPI_PICO_DMA_SLOT_SPI1_TX    RPI_PICO_DMA_DREQ_TO_SLOT(0x12)
#define RPI_PICO_DMA_SLOT_SPI1_RX    RPI_PICO_DMA_DREQ_TO_SLOT(0x13)
#define RPI_PICO_DMA_SLOT_UART0_TX   RPI_PICO_DMA_DREQ_TO_SLOT(0x14)
#define RPI_PICO_DMA_SLOT_UART0_RX   RPI_PICO_DMA_DREQ_TO_SLOT(0x15)
#define RPI_PICO_DMA_SLOT_UART1_TX   RPI_PICO_DMA_DREQ_TO_SLOT(0x16)
#define RPI_PICO_DMA_SLOT_UART1_RX   RPI_PICO_DMA_DREQ_TO_SLOT(0x17)
#define RPI_PICO_DMA_SLOT_PWM_WRAP0  RPI_PICO_DMA_DREQ_TO_SLOT(0x18)
#define RPI_PICO_DMA_SLOT_PWM_WRAP1  RPI_PICO_DMA_DREQ_TO_SLOT(0x19)
#define RPI_PICO_DMA_SLOT_PWM_WRAP2  RPI_PICO_DMA_DREQ_TO_SLOT(0x1A)
#define RPI_PICO_DMA_SLOT_PWM_WRAP3  RPI_PICO_DMA_DREQ_TO_SLOT(0x1B)
#define RPI_PICO_DMA_SLOT_PWM_WRAP4  RPI_PICO_DMA_DREQ_TO_SLOT(0x1C)
#define RPI_PICO_DMA_SLOT_PWM_WRAP5  RPI_PICO_DMA_DREQ_TO_SLOT(0x1D)
#define RPI_PICO_DMA_SLOT_PWM_WRAP6  RPI_PICO_DMA_DREQ_TO_SLOT(0x1E)
#define RPI_PICO_DMA_SLOT_PWM_WRAP7  RPI_PICO_DMA_DREQ_TO_SLOT(0x1F)
#define RPI_PICO_DMA_SLOT_I2C0_TX    RPI_PICO_DMA_DREQ_TO_SLOT(0x30)
#define RPI_PICO_DMA_SLOT_I2C0_RX    RPI_PICO_DMA_DREQ_TO_SLOT(0x31)
#define RPI_PICO_DMA_SLOT_I2C1_TX    RPI_PICO_DMA_DREQ_TO_SLOT(0x32)
#define RPI_PICO_DMA_SLOT_I2C1_RX    RPI_PICO_DMA_DREQ_TO_SLOT(0x33)
#define RPI_PICO_DMA_SLOT_ADC	     RPI_PICO_DMA_DREQ_TO_SLOT(0x34)
#define RPI_PICO_DMA_SLOT_XIP_STREAM RPI_PICO_DMA_DREQ_TO_SLOT(0x35)
#define RPI_PICO_DMA_SLOT_XIP_SSITX  RPI_PICO_DMA_DREQ_TO_SLOT(0x36)
#define RPI_PICO_DMA_SLOT_XIP_SSIRX  RPI_PICO_DMA_DREQ_TO_SLOT(0x37)
#define RPI_PICO_DMA_SLOT_DMA_TIMER0 RPI_PICO_DMA_DREQ_TO_SLOT(0x3B)
#define RPI_PICO_DMA_SLOT_DMA_TIMER1 RPI_PICO_DMA_DREQ_TO_SLOT(0x3C)
#define RPI_PICO_DMA_SLOT_DMA_TIMER2 RPI_PICO_DMA_DREQ_TO_SLOT(0x3D)
#define RPI_PICO_DMA_SLOT_DMA_TIMER3 RPI_PICO_DMA_DREQ_TO_SLOT(0x3E)
#define RPI_PICO_DMA_SLOT_FORCE	     RPI_PICO_DMA_DREQ_TO_SLOT(0x3F)

#endif /* ZEPHYR_INCLUDE_DT_BINDINGS_RPI_PICO_DMA_H_ */
