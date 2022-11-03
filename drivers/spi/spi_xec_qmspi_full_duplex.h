/*
 * Copyright (c) 2022 Microchip Technology Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _SPI_XEC_QMSPI_V2_H
#define _SPI_XEC_QMSPI_V2_H

#define MEC152X_QSPI_SRC_CLOCK_HZ	48000000u
#define MEC172X_QSPI_SRC_CLOCK_HZ	48000000u
#define MEC172X_QSPI_TURBO_SRC_CLOCK_HZ	96000000u

#define XEC_QSPI_TX_FIFO_SIZE		8
#define XEC_QSPI_RX_FIFO_SIZE		8

#define XEC_QSPI_DESCR_MAX		16

/* mode register */
#define XEC_QSPI_M_ACTV_POS		0
#define XEC_QSPI_M_SRST_POS		1
#define XEC_QSPI_M_RX_LDMA_EN_POS	3
#define XEC_QSPI_M_TX_LDMA_EN_POS	4
#define XEC_QSPI_M_CPOL_POS		8
#define XEC_QSPI_M_CPHA_MOSI_POS	9
#define XEC_QSPI_M_CPHA_MISO_POS	10
#define XEC_QSPI_M_CP_MSK		(0x7u << XEC_QSPI_M_CPOL_POS)
#define XEC_QSPI_M_CS_SEL_POS		12
#define XEC_QSPI_M_CS_SEL_MSK		(0x3u << XEC_QSPI_M_CS_SEL_POS)
#define XEC_QSPI_M_CS0_SEL		0
#define XEC_QSPI_M_CS1_SEL		(1u << XEC_QSPI_M_CS_SEL_POS)
#define XEC_QSPI_M_CLK_DIV_POS		16
#ifdef CONFIG_SOC_SERIES_MEC172X
#define XEC_QSPI_M_CLK_DIV_MASK		0xffff0000u
#else
#define XEC_QSPI_M_CLK_DIV_MASK		0xff000000u
#endif

/* control register */
#define XEC_QSPI_C_IFC_POS		0
#define XEC_QSPI_C_IFC_MSK		0x3u
#define XEC_QSPI_C_IFC_1X		0
#define XEC_QSPI_C_IFC_2X		0x1u
#define XEC_QSPI_C_IFC_4X		0x2u
#define XEC_QSPI_C_TX_EN_POS		2
#define XEC_QSPI_C_TX_EN_MSK		(0x3u << XEC_QSPI_C_TX_EN_POS)
#define XEC_QSPI_C_TX_EN_DATA		(0x1u << XEC_QSPI_C_TX_EN_POS)
#define XEC_QSPI_C_TX_EN_ZEROS		(0x2u << XEC_QSPI_C_TX_EN_POS)
#define XEC_QSPI_C_TX_EN_ONES		(0x3u << XEC_QSPI_C_TX_EN_POS)
#define XEC_QSPI_C_TX_DMA_EN_POS	4
#define XEC_QSPI_C_TX_DMA_EN_MSK	(0x3u << XEC_QSPI_C_TX_DMA_EN_POS)
#define XEC_QSPI_C_TX_DMA_EN_1B		(0x1u << XEC_QSPI_C_TX_DMA_EN_POS)
#define XEC_QSPI_C_TX_DMA_EN_2B		(0x2u << XEC_QSPI_C_TX_DMA_EN_POS)
#define XEC_QSPI_C_TX_DMA_EN_4B		(0x3u << XEC_QSPI_C_TX_DMA_EN_POS)
#ifdef CONFIG_SOC_SERIES_MEC172X
#define XEC_QSPI_C_TX_DMA_EN_LDCH0	(0x1u << XEC_QSPI_C_TX_DMA_EN_POS)
#define XEC_QSPI_C_TX_DMA_EN_LDCH1	(0x2u << XEC_QSPI_C_TX_DMA_EN_POS)
#define XEC_QSPI_C_TX_DMA_EN_LDCH2	(0x3u << XEC_QSPI_C_TX_DMA_EN_POS)
#endif
#define XEC_QSPI_C_RX_EN_POS		6
#define XEC_QSPI_C_RX_DMA_EN_POS	7
#define XEC_QSPI_C_RX_DMA_EN_MSK	(0x3u << XEC_QSPI_C_RX_DMA_EN_POS)
#define XEC_QSPI_C_RX_DMA_EN_1B		(0x1u << XEC_QSPI_C_RX_DMA_EN_POS)
#define XEC_QSPI_C_RX_DMA_EN_2B		(0x2u << XEC_QSPI_C_RX_DMA_EN_POS)
#define XEC_QSPI_C_RX_DMA_EN_4B		(0x3u << XEC_QSPI_C_RX_DMA_EN_POS)
#ifdef CONFIG_SOC_SERIES_MEC172X
#define XEC_QSPI_C_RX_DMA_EN_LDCH0	(0x1u << XEC_QSPI_C_RX_DMA_EN_POS)
#define XEC_QSPI_C_RX_DMA_EN_LDCH1	(0x2u << XEC_QSPI_C_RX_DMA_EN_POS)
#define XEC_QSPI_C_RX_DMA_EN_LDCH2	(0x3u << XEC_QSPI_C_RX_DMA_EN_POS)
#endif
#define XEC_QSPI_C_CLOSE_POS		9
#define XEC_QSPI_C_Q_XFR_UNITS_POS	10
#define XEC_QSPI_C_Q_XFR_UNITS_MSK	(0x3u << XEC_QSPI_C_Q_XFR_UNITS_POS)
#define XEC_QSPI_C_Q_XFR_UNITS_BITS	0
#define XEC_QSPI_C_Q_XFR_UNITS_1B	(0x1u << XEC_QSPI_C_Q_XFR_UNITS_POS)
#define XEC_QSPI_C_Q_XFR_UNITS_4B	(0x2u << XEC_QSPI_C_Q_XFR_UNITS_POS)
#define XEC_QSPI_C_Q_XFR_UNITS_16B	(0x3u << XEC_QSPI_C_Q_XFR_UNITS_POS)
#define XEC_QSPI_C_FN_DESCR_POS	12
#define XEC_QSPI_C_FN_DESCR_MSK		(0xfu << XEC_QSPI_C_FN_DESCR_POS)
#define XEC_QSPI_C_FN_DESCR(n)		\
	(((uint32_t)(n) & 0xfu) << XEC_QSPI_C_FN_DESCR_POS)
/* control register enable descriptor mode */
#define XEC_QSPI_C_DESCR_MODE_EN_POS	16
/* descriptor specifies last descriptor to be processed */
#define XEC_QSPI_D_DESCR_LAST_POS	16
#define XEC_QSPI_C_Q_NUNITS_POS		17
#define XEC_QSPI_C_Q_NUNITS_MAX		0x7fffu
#define XEC_QSPI_C_Q_NUNITS_MSK		(0x7fffu << XEC_QSPI_C_Q_NUNITS_POS)
#define XEC_QSPI_C_NUNITS(n)		\
	(((uint32_t)(n) & 0x7fffu) << XEC_QSPI_C_Q_NUNITS_POS)

/* execute register (WO). Set one bit at a time! */
#define XEC_QSPI_EXE_START_POS		0
#define XEC_QSPI_EXE_STOP_POS		1
#define XEC_QSPI_EXE_CLR_FIFOS_POS	2

/* status register */
#define XEC_QSPI_STS_MSK		0x0f01ff7fu
#define XEC_QSPI_STS_MSK_RW1C		0x0000cc1fu
#define XEC_QSPI_STS_XFR_DONE_POS	0
#define XEC_QSPI_STS_DMA_DONE_POS	1
#define XEC_QSPI_STS_TXB_ERR_POS	2
#define XEC_QSPI_STS_RXB_ERR_POS	3
#define XEC_QSPI_STS_PROG_ERR_POS	4
#ifdef CONFIG_SOC_SERIES_MEC172X
#define XEC_QSPI_STS_LDMA_RX_ERR_POS	5
#define XEC_QSPI_STS_LDMA_TX_ERR_POS	6
#endif
#define XEC_QSPI_STS_TXB_FULL_POS	8
#define XEC_QSPI_STS_TXB_EMPTY_POS	9
#define XEC_QSPI_STS_TXB_REQ_POS	10
#define XEC_QSPI_STS_TXB_STALL_POS	11
#define XEC_QSPI_STS_RXB_FULL_POS	12
#define XEC_QSPI_STS_RXB_EMPTY_POS	13
#define XEC_QSPI_STS_RXB_REQ_POS	14
#define XEC_QSPI_STS_RXB_STALL_POS	15
#define XEC_QSPI_STS_XFR_ACTIVE_POS	16
#define XEC_QSPI_STS_CURR_DESCR_POS	24
#define XEC_QSPI_STS_CURR_DESCR_MSK	(0xfu << XEC_QSPI_STS_CURR_DESCR_POS)

#define XEC_QSPI_STS_ALL_ERR		(BIT(XEC_QSPI_STS_TXB_ERR_POS) | \
					 BIT(XEC_QSPI_STS_RXB_ERR_POS) | \
					 BIT(XEC_QSPI_STS_PROG_ERR_POS))

/* buffer count status (RO) */
#define XEC_QSPI_BCNT_STS_TX_POS	0
#define XEC_QSPI_BCNT_STS_TX_MSK	0xffffu
#define XEC_QSPI_BCNT_STS_RX_POS	16
#define XEC_QSPI_BCNT_STS_RX_MSK	(0xffffu << XEC_QSPI_BCNT_STS_RX_POS)

/* interrupt enable */
#define XEC_QSPI_IEN_XFR_DONE_POS	0
#define XEC_QSPI_IEN_DMA_DONE_POS	1
#define XEC_QSPI_IEN_TXB_ERR_POS	2
#define XEC_QSPI_IEN_RXB_ERR_POS	3
#define XEC_QSPI_IEN_PROG_ERR_POS	4
#ifdef CONFIG_SOC_SERIES_MEC172X
#define XEC_QSPI_IEN_LDMA_RX_ERR_POS	5
#define XEC_QSPI_IEN_LDMA_TX_ERR_POS	6
#endif
#define XEC_QSPI_IEN_TXB_FULL_POS	8
#define XEC_QSPI_IEN_TXB_EMPTY_POS	9
#define XEC_QSPI_IEN_TXB_REQ_POS	10
#define XEC_QSPI_IEN_RXB_FULL_POS	12
#define XEC_QSPI_IEN_RXB_EMPTY_POS	13
#define XEC_QSPI_IEN_RXB_REQ_POS	14

/* chip select timing */
#define XEC_QSPI_CSTM_DLY_CS_TO_START_POS	0
#define XEC_QSPI_CSTM_DLY_CS_TO_START_MSK	0xfu
#define XEC_QSPI_CSTM_DLY_CLK_OFF_TO_CS_OFF_POS	8
#define XEC_QSPI_CSTM_DLY_CLK_OFF_TO_CS_OFF_MSK	0xf00u
#define XEC_QSPI_CSTM_DLY_LAST_DATA_HOLD_POS	16
#define XEC_QSPI_CSTM_DLY_LAST_DATA_HOLD_MSK	0xf0000u
#define XEC_QSPI_CSTM_DLY_CS_OFF_TO_CS_ON_POS	24
#define XEC_QSPI_CSTM_DLY_CS_OFF_TO_CS_ON_MSK	0xff000000u

#ifdef CONFIG_SOC_SERIES_MEC172X
#define XEC_QSPI_MALT1_EN_POS		0
#define XEC_QSPI_MALT1_CLK_DIV_POS	16
#define XEC_QSPI_MALT1_CLK_DIV_MSK	0xffff0000u

#define XEC_QSPI_LDCH_CTRL_EN_POS		0
#define XEC_QSPI_LDCH_CTRL_RESTART_EN_POS	1
#define XEC_QSPI_LDCH_CTRL_RESTART_ADDR_EN_POS	2
#define XEC_QSPI_LDCH_CTRL_OVRLEN_POS		3
#define XEC_QSPI_LDCH_CTRL_ACCSZ_POS		4
#define XEC_QSPI_LDCH_CTRL_ACCSZ_MSK		0x30u
#define XEC_QSPI_LDCH_CTRL_ACCSZ_1B		0u
#define XEC_QSPI_LDCH_CTRL_ACCSZ_2B		1u
#define XEC_QSPI_LDCH_CTRL_ACCSZ_4B		2u
#define XEC_QSPI_LDCH_CTRL_INCR_ADDR_POS	6

struct qspi_ldma_chan {
	volatile uint32_t ldctrl;
	volatile uint32_t mstart;
	volatile uint32_t nbytes;
	uint32_t rsvd[1];
};
#endif /* CONFIG_SOC_SERIES_MEC172X */

#endif /* _SPI_XEC_QMSPI_V2_H */
