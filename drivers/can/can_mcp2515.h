/*
 * Copyright (c) 2018 Karsten Koenig
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#ifndef _MCP2515_H_
#define _MCP2515_H_

#include <can.h>

#define MCP2515_TX_CNT                   3
#define MCP2515_FRAME_LEN               13

#define DEV_CFG(dev) \
	((const struct mcp2515_config *const)(dev)->config->config_info)
#define DEV_DATA(dev) ((struct mcp2515_data *const)(dev)->driver_data)

struct mcp2515_tx_cb {
	struct k_sem sem;
	can_tx_callback_t cb;
	void *cb_arg;
};

struct mcp2515_data {
	/* spi device data */
	struct device *spi;
	struct spi_config spi_cfg;
#ifdef DT_MICROCHIP_MCP2515_0_CS_GPIO_PIN
	struct spi_cs_control spi_cs_ctrl;
#endif /* DT_MICROCHIP_MCP2515_0_CS_GPIO_PIN */

	/* interrupt data */
	struct device *int_gpio;
	struct gpio_callback int_gpio_cb;
	struct k_thread int_thread;
	k_thread_stack_t *int_thread_stack;
	struct k_sem int_sem;

	/* tx data */
	struct k_sem tx_sem;
	struct k_mutex tx_mutex;
	struct mcp2515_tx_cb tx_cb[MCP2515_TX_CNT];
	u8_t tx_busy_map;

	/* filter data */
	struct k_mutex filter_mutex;
	u32_t filter_usage;
	can_rx_callback_t rx_cb[CONFIG_CAN_MCP2515_MAX_FILTER];
	void *cb_arg[CONFIG_CAN_MCP2515_MAX_FILTER];
	struct zcan_filter filter[CONFIG_CAN_MCP2515_MAX_FILTER];
};

struct mcp2515_config {
	/* spi configuration */
	const char *spi_port;
	u8_t spi_cs_pin;
	const char *spi_cs_port;
	u32_t spi_freq;
	u8_t spi_slave;

	/* interrupt configuration */
	u8_t int_pin;
	const char *int_port;
	size_t int_thread_stack_size;
	int int_thread_priority;

	/* CAN timing */
	u8_t tq_sjw;
	u8_t tq_prop;
	u8_t tq_bs1;
	u8_t tq_bs2;
	u32_t bus_speed;
};

/* MCP2515 Opcodes */
#define MCP2515_OPCODE_WRITE            0x02
#define MCP2515_OPCODE_READ             0x03
#define MCP2515_OPCODE_BIT_MODIFY       0x05
#define MCP2515_OPCODE_READ_STATUS      0xA0
#define MCP2515_OPCODE_RESET            0xC0

/* MCP2515 Registers */
#define MCP2515_ADDR_CANSTAT            0x0E
#define MCP2515_ADDR_CANCTRL            0x0F
#define MCP2515_ADDR_CNF3               0x28
#define MCP2515_ADDR_CNF2               0x29
#define MCP2515_ADDR_CNF1               0x2A
#define MCP2515_ADDR_CANINTE            0x2B
#define MCP2515_ADDR_CANINTF            0x2C
#define MCP2515_ADDR_TXB0CTRL           0x30
#define MCP2515_ADDR_TXB1CTRL           0x40
#define MCP2515_ADDR_TXB2CTRL           0x50
#define MCP2515_ADDR_RXB0CTRL           0x60
#define MCP2515_ADDR_RXB1CTRL           0x70

#define MCP2515_ADDR_OFFSET_FRAME2FRAME	0x10
#define MCP2515_ADDR_OFFSET_CTRL2FRAME	0x01

/* MCP2515 Operation Modes */
#define MCP2515_MODE_NORMAL             0x00
#define MCP2515_MODE_LOOPBACK           0x02
#define MCP2515_MODE_SILENT             0x03
#define MCP2515_MODE_CONFIGURATION      0x04

/* MCP2515_FRAME_OFFSET */
#define MCP2515_FRAME_OFFSET_SIDH       0
#define MCP2515_FRAME_OFFSET_SIDL       1
#define MCP2515_FRAME_OFFSET_EID8       2
#define MCP2515_FRAME_OFFSET_EID0       3
#define MCP2515_FRAME_OFFSET_DLC        4
#define MCP2515_FRAME_OFFSET_D0         5

/* MCP2515_CANINTF */
#define MCP2515_CANINTF_RX0IF           BIT(0)
#define MCP2515_CANINTF_RX1IF           BIT(1)
#define MCP2515_CANINTF_TX0IF           BIT(2)
#define MCP2515_CANINTF_TX1IF           BIT(3)
#define MCP2515_CANINTF_TX2IF           BIT(4)
#define MCP2515_CANINTF_ERRIF           BIT(5)
#define MCP2515_CANINTF_WAKIF           BIT(6)
#define MCP2515_CANINTF_MERRF           BIT(7)


#define MCP2515_TXCTRL_TXREQ			BIT(3)

#define MCP2515_CANSTAT_MODE_POS		5
#define MCP2515_CANSTAT_MODE_MASK		(0x07 << MCP2515_CANSTAT_MODE_POS)
#define MCP2515_CANCTRL_MODE_POS		5
#define MCP2515_CANCTRL_MODE_MASK		(0x07 << MCP2515_CANCTRL_MODE_POS)

#endif /*_MCP2515_H_*/
