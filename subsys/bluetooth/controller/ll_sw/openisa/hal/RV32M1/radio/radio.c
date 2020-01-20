/*
 * Copyright (c) 2016 - 2019 Nordic Semiconductor ASA
 * Copyright (c) 2016 Vinayak Kariappa Chettimada
 * Copyright 2019 NXP
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <sys/printk.h>
#include <sys/dlist.h>
#include <sys/mempool_base.h>
#include <toolchain.h>

#include "util/mem.h"
#include "hal/ccm.h"
#include "hal/radio.h"
#include "ll_sw/pdu.h"

#include "fsl_xcvr.h"
#include "irq.h"
#include "hal/cntr.h"
#include "hal/ticker.h"
#include "hal/swi.h"


static radio_isr_cb_t isr_cb;
static void *isr_cb_param;

#define RADIO_PDU_LEN_MAX (BIT(8) - 1)

/* us values */
#define MIN_CMD_TIME 10		/* Minimum interval for a delayed radio cmd */
#define RX_MARGIN 8
#define TX_MARGIN 0
#define RX_WTMRK 5		/* (AA + PDU header) - 1 */
#define AA_OVHD 27		/* AA playback overhead, depends on PHY type */
#define Rx_OVHD 32		/* Rx overhead, depends on PHY type */

#define PB_RX 544	/* half of PB (packet buffer) */

/* The PDU in packet buffer starts after the Access Address which is 4 octets */
#define PB_RX_PDU (PB_RX + 2)	/* Rx PDU offset (in halfwords) in PB */
#define PB_TX_PDU 2		/* Tx PDU offset (in halfwords) in packet
				 * buffer
				 */

static u32_t rtc_start;
static u32_t rtc_diff_start_us;

static u32_t tmr_aa;		/* AA (Access Address) timestamp saved value */
static u32_t tmr_aa_save;	/* save AA timestamp */
static u32_t tmr_ready;		/* radio ready for Tx/Rx timestamp */
static u32_t tmr_end;		/* Tx/Rx end timestamp saved value */
static u32_t tmr_end_save;	/* save Tx/Rx end timestamp */
static u32_t tmr_tifs;

static u32_t rx_wu;
static u32_t tx_wu;

static u32_t isr_tmr_aa;
static u32_t isr_tmr_end;
static u32_t isr_latency;
static u32_t next_wu;
static u32_t next_radio_cmd;

static u32_t radio_trx;
static u32_t force_bad_crc;
static u32_t skip_hcto;

static u8_t *rx_pkt_ptr;
static u32_t payload_max_size;

static u8_t MALIGN(4) _pkt_empty[PDU_EM_SIZE_MAX];
static u8_t MALIGN(4) _pkt_scratch[
			((RADIO_PDU_LEN_MAX + 3) > PDU_AC_SIZE_MAX) ?
			(RADIO_PDU_LEN_MAX + 3) : PDU_AC_SIZE_MAX];

static s8_t rssi;


static void tmp_cb(void *param)
{
	u32_t tmr = GENFSK->EVENT_TMR & GENFSK_EVENT_TMR_EVENT_TMR_MASK;
	u32_t t2 = GENFSK->T2_CMP & GENFSK_T2_CMP_T2_CMP_MASK;

	isr_latency = (tmr - t2) & GENFSK_EVENT_TMR_EVENT_TMR_MASK; /* 24bit */
	/* Mark as done */
	*(u32_t *)param = 1;
}

static void get_isr_latency(void)
{
	volatile u32_t tmp = 0;

	radio_isr_set(tmp_cb, (void *)&tmp);

	/* Reset TMR to zero */
	GENFSK->EVENT_TMR = 0x1000000;

	radio_disable();
	while (!tmp) {
	}
	irq_disable(LL_RADIO_IRQn_2nd_lvl);
}


static void pkt_rx(void)
{
	u32_t len, idx;
	u16_t *rxb = (u16_t *)rx_pkt_ptr, tmp;
	volatile u16_t *pb = &GENFSK->PACKET_BUFFER[PB_RX_PDU];
	volatile const u32_t *sts = &GENFSK->XCVR_STS;

	/* payload length */
	len = (GENFSK->XCVR_CTRL & GENFSK_XCVR_CTRL_LENGTH_EXT_MASK) >>
			GENFSK_XCVR_CTRL_LENGTH_EXT_SHIFT;

	if (len > payload_max_size) {
		/* Unexpected size */
		force_bad_crc = 1;
		next_radio_cmd = 0;
		while (*sts & GENFSK_XCVR_STS_RX_IN_PROGRESS_MASK) {
		}
		return;
	}

	/* For Data Physical Channel PDU,
	 * assume no CTEInfo field, CP=0 => 2 bytes header
	 */
	/* Add PDU header size */
	len += 2;

	/* Add to AA time, PDU + CRC time */
	isr_tmr_end = isr_tmr_aa + (len + 3) * 8;

	/* If not enough time for warmup after we copy the PDU from
	 * packet buffer, send delayed command now
	 */
	if (next_radio_cmd) {
		/* Start Rx/Tx in TIFS */
		idx = isr_tmr_end + tmr_tifs - next_wu;
		GENFSK->T1_CMP = GENFSK_T1_CMP_T1_CMP(idx) |
				 GENFSK_T1_CMP_T1_CMP_EN(1);
		GENFSK->XCVR_CTRL = next_radio_cmd;
		next_radio_cmd = 0;
	}

	/* Can't rely on data read from packet buffer while in Rx */
	/* Wait for Rx to finish */
	while (*sts & GENFSK_XCVR_STS_RX_IN_PROGRESS_MASK) {
	}

	/* Copy the PDU */
	for (idx = 0; idx < len / 2; idx++) {
		rxb[idx] = pb[idx];
	}

	/* Copy last byte */
	if (len & 0x1) {
		tmp = pb[len / 2];
		rx_pkt_ptr[len - 1] =  ((u8_t *)&tmp)[0];
	}

	force_bad_crc = 0;
}

#define IRQ_MASK ~(GENFSK_IRQ_CTRL_T2_IRQ_MASK | \
		   GENFSK_IRQ_CTRL_RX_WATERMARK_IRQ_MASK | \
		   GENFSK_IRQ_CTRL_TX_IRQ_MASK)
void isr_radio(void *arg)
{
	ARG_UNUSED(arg);

	u32_t tmr = GENFSK->EVENT_TMR & GENFSK_EVENT_TMR_EVENT_TMR_MASK;
	u32_t irq = GENFSK->IRQ_CTRL;

	if (irq & GENFSK_IRQ_CTRL_TX_IRQ_MASK) {
		GENFSK->IRQ_CTRL &= (IRQ_MASK | GENFSK_IRQ_CTRL_TX_IRQ_MASK);
		GENFSK->T1_CMP &= ~GENFSK_T1_CMP_T1_CMP_EN_MASK;

		isr_tmr_end = tmr - isr_latency;
		if (tmr_end_save) {
			tmr_end = isr_tmr_end;
		}
		radio_trx = 1;
	}

	if (irq & GENFSK_IRQ_CTRL_RX_WATERMARK_IRQ_MASK) {
		/* Disable Rx timeout */
		/* 0b1010..RX Cancel -- Cancels pending RX events but do not
		 *			abort a RX-in-progress
		 */
		GENFSK->XCVR_CTRL = GENFSK_XCVR_CTRL_SEQCMD(0xa);
		GENFSK->T2_CMP &= ~GENFSK_T2_CMP_T2_CMP_EN_MASK;

		GENFSK->IRQ_CTRL &= (IRQ_MASK |
				     GENFSK_IRQ_CTRL_RX_WATERMARK_IRQ_MASK);
		GENFSK->T1_CMP &= ~GENFSK_T1_CMP_T1_CMP_EN_MASK;

		/* Fix reported AA time */
		isr_tmr_aa = GENFSK->TIMESTAMP - AA_OVHD;
		if (tmr_aa_save) {
			tmr_aa = isr_tmr_aa;
		}
		/* Copy the PDU as it arrives, calculates Rx end */
		pkt_rx();
		if (tmr_end_save) {
			tmr_end = isr_tmr_end;	/* from pkt_rx() */
		}
		radio_trx = 1;
		rssi = (GENFSK->XCVR_STS & GENFSK_XCVR_STS_RSSI_MASK) >>
					GENFSK_XCVR_STS_RSSI_SHIFT;
	}

	if (irq & GENFSK_IRQ_CTRL_T2_IRQ_MASK) {
		GENFSK->IRQ_CTRL &= (IRQ_MASK | GENFSK_IRQ_CTRL_T2_IRQ_MASK);
		/* Disable both comparators */
		GENFSK->T1_CMP &= ~GENFSK_T1_CMP_T1_CMP_EN_MASK;
		GENFSK->T2_CMP &= ~GENFSK_T2_CMP_T2_CMP_EN_MASK;
	}

	if (radio_trx && next_radio_cmd) {
		/* Start Rx/Tx in TIFS */
		tmr = isr_tmr_end + tmr_tifs - next_wu;
		GENFSK->T1_CMP = GENFSK_T1_CMP_T1_CMP(tmr) |
				 GENFSK_T1_CMP_T1_CMP_EN(1);
		GENFSK->XCVR_CTRL = next_radio_cmd;
		next_radio_cmd = 0;
	}

	isr_cb(isr_cb_param);
}

void radio_isr_set(radio_isr_cb_t cb, void *param)
{
	irq_disable(LL_RADIO_IRQn_2nd_lvl);

	isr_cb_param = param;
	isr_cb = cb;

	/* Clear pending interrupts */
	GENFSK->IRQ_CTRL &= 0xffffffff;
	irq_enable(LL_RADIO_IRQn_2nd_lvl);
}

#define DISABLE_HPMCAL

#ifdef DISABLE_HPMCAL
#define WU_OPTIM            26  /* 34: quite ok, 36 few ok */
#define USE_FIXED_HPMCAL    563

static void hpmcal_disable(void)
{
#ifdef USE_FIXED_HPMCAL
	u32_t hpmcal = USE_FIXED_HPMCAL;
#else
	u32_t hpmcal_vals[40];
	u32_t hpmcal;
	int i;

	GENFSK->TX_POWER = GENFSK_TX_POWER_TX_POWER(1);

	/* TX warm-up at Channel Frequency = 2.44GHz */
	for (i = 0; i < ARRAY_SIZE(hpmcal_vals); i++) {
		GENFSK->CHANNEL_NUM = 2402 - 2360 + 2 * i;

		/* Reset TMR to zero */
		GENFSK->EVENT_TMR = 0x1000000;

		/* TX Start Now */
		GENFSK->XCVR_CTRL = GENFSK_XCVR_CTRL_SEQCMD(0x1);

		while ((GENFSK->EVENT_TMR & 0xffffff) < 1000)
			;

		/* Abort All */
		GENFSK->XCVR_CTRL = GENFSK_XCVR_CTRL_SEQCMD(0xb);

		/* Wait for XCVR to become idle. */
		while (GENFSK->XCVR_CTRL & GENFSK_XCVR_CTRL_XCVR_BUSY_MASK)
			;

		hpmcal_vals[i] = (XCVR_PLL_DIG->HPMCAL_CTRL &
				XCVR_PLL_DIG_HPMCAL_CTRL_HPM_CAL_FACTOR_MASK) >>
				XCVR_PLL_DIG_HPMCAL_CTRL_HPM_CAL_FACTOR_SHIFT;
	}

	hpmcal = hpmcal_vals[20];
#endif

	XCVR_PLL_DIG->HPMCAL_CTRL = (XCVR_PLL_DIG->HPMCAL_CTRL &
			~XCVR_PLL_DIG_HPMCAL_CTRL_HPM_CAL_FACTOR_MANUAL_MASK) +
			XCVR_PLL_DIG_HPMCAL_CTRL_HPM_CAL_FACTOR_MANUAL(hpmcal) +
			XCVR_PLL_DIG_HPMCAL_CTRL_HP_CAL_DISABLE_MASK +
			0x00000000;

	/* Move the sigma_delta_en signal to be 1us after pll_dig_en */
	int pll_dig_en  = (XCVR_TSM->TIMING34 &
			XCVR_TSM_TIMING34_PLL_DIG_EN_TX_HI_MASK) >>
			XCVR_TSM_TIMING34_PLL_DIG_EN_TX_HI_SHIFT;

	XCVR_TSM->TIMING38 = (XCVR_TSM->TIMING38 &
			~XCVR_TSM_TIMING38_SIGMA_DELTA_EN_TX_HI_MASK) +
			XCVR_TSM_TIMING38_SIGMA_DELTA_EN_TX_HI(pll_dig_en+1);
			/* sigma_delta_en */

	XCVR_TSM->TIMING19   -= B1(WU_OPTIM); /* sy_pd_filter_charge_en */
	XCVR_TSM->TIMING24   -= B1(WU_OPTIM); /* sy_divn_cal_en */
	XCVR_TSM->TIMING13   -= B1(WU_OPTIM); /* sy_vco_autotune_en */
	XCVR_TSM->TIMING17   -= B0(WU_OPTIM); /* sy_lo_tx_buf_en */
	XCVR_TSM->TIMING26   -= B0(WU_OPTIM); /* tx_pa_en */
	XCVR_TSM->TIMING35   -= B0(WU_OPTIM); /* tx_dig_en */
	XCVR_TSM->TIMING14   -= B0(WU_OPTIM); /* sy_pd_cycle_slip_ld_ft_en */

	XCVR_TSM->END_OF_SEQ -= B1(WU_OPTIM) + B0(WU_OPTIM);
}
#endif

void radio_setup(void)
{
	XCVR_Init(GFSK_BT_0p5_h_0p5, DR_1MBPS);
	XCVR_SetXtalTrim(41);

#ifdef DISABLE_HPMCAL
	hpmcal_disable();
#endif

	/* enable the CRC as it is disabled by default after reset */
	XCVR_MISC->CRCW_CFG |= XCVR_CTRL_CRCW_CFG_CRCW_EN(1);

	/* Assign Radio #0 Interrupt to GENERIC_FSK */
	XCVR_MISC->XCVR_CTRL |= XCVR_CTRL_XCVR_CTRL_RADIO0_IRQ_SEL(3);

	GENFSK->BITRATE = DR_1MBPS;

	/*
	 * Split the buffer in equal parts: first half for Tx,
	 * second half for Rx
	 */
	GENFSK->PB_PARTITION = GENFSK_PB_PARTITION_PB_PARTITION(PB_RX);

	/* Get warmpup times. They are used in TIFS calculations */
	rx_wu = (XCVR_TSM->END_OF_SEQ & XCVR_TSM_END_OF_SEQ_END_OF_RX_WU_MASK)
				>> XCVR_TSM_END_OF_SEQ_END_OF_RX_WU_SHIFT;
	tx_wu = (XCVR_TSM->END_OF_SEQ & XCVR_TSM_END_OF_SEQ_END_OF_TX_WU_MASK)
				>> XCVR_TSM_END_OF_SEQ_END_OF_TX_WU_SHIFT;

	/* IRQ config, clear pending interrupts */
	irq_disable(LL_RADIO_IRQn_2nd_lvl);
	GENFSK->IRQ_CTRL = 0xffffffff;
	GENFSK->IRQ_CTRL = GENFSK_IRQ_CTRL_GENERIC_FSK_IRQ_EN(1) |
			   GENFSK_IRQ_CTRL_RX_WATERMARK_IRQ_EN(1) |
			   GENFSK_IRQ_CTRL_TX_IRQ_EN(1) |
			   GENFSK_IRQ_CTRL_T2_IRQ_EN(1);

	/* Disable Rx recycle */
	GENFSK->IRQ_CTRL |= GENFSK_IRQ_CTRL_CRC_IGNORE(1);
	GENFSK->WHITEN_SZ_THR |= GENFSK_WHITEN_SZ_THR_REC_BAD_PKT(1);

	get_isr_latency();
}

void radio_reset(void)
{
	irq_disable(LL_RADIO_IRQn_2nd_lvl);
	/* Vega radio is never disabled therefore doesn't require resetting */
}

void radio_phy_set(u8_t phy, u8_t flags)
{
	ARG_UNUSED(phy);
	ARG_UNUSED(flags);

	/* This function should set one of three modes:
	 * - BLE 1 Mbps
	 * - BLE 2 Mbps
	 * - Coded BLE
	 * We set this on radio_setup() function. There radio is
	 * setup for DataRate of 1 Mbps.
	 * For now this function does nothing. In the future it
	 * may have to reset the radio
	 * to the 2 Mbps (the only other mode supported by Vega radio).
	 */
}

void radio_tx_power_set(u32_t power)
{
	ARG_UNUSED(power);

	/* tx_power_level should only have the values 1, 2, 4, 6, ... , 62.
	 * Any value odd value (1, 3, ...) is not permitted and will result in
	 * undefined behavior.
	 * Those value have an associated db level that was not found in the
	 * documentation.
	 * Because of these inconsistencies for the moment this
	 * function sets the power level to a known value.
	 */
	u32_t tx_power_level = 62;

	GENFSK->TX_POWER = GENFSK_TX_POWER_TX_POWER(tx_power_level);
}

void radio_tx_power_max_set(void)
{
	printk("%s\n", __func__);
}

void radio_freq_chan_set(u32_t chan)
{
	/*
	 * The channel number for vega radio is computed
	 * as 2360 + ch_num [in MHz]
	 * The LLL expects the channel number to be computed as 2400 + ch_num.
	 * Therefore a compensation of 40 MHz has been provided.
	 */
	GENFSK->CHANNEL_NUM = GENFSK_CHANNEL_NUM_CHANNEL_NUM(40 + chan);
}

#define GENFSK_BLE_WHITEN_START			1	/* after H0 */
#define GENFSK_BLE_WHITEN_END			1	/* at the end of CRC */
#define GENFSK_BLE_WHITEN_POLY_TYPE		0	/* Galois poly type */
#define GENFSK_BLE_WHITEN_SIZE			7	/* poly order */
#define GENFSK_BLE_WHITEN_POLY			0x04

void radio_whiten_iv_set(u32_t iv)
{
	GENFSK->WHITEN_CFG &= ~(GENFSK_WHITEN_CFG_WHITEN_START_MASK |
				GENFSK_WHITEN_CFG_WHITEN_END_MASK |
				GENFSK_WHITEN_CFG_WHITEN_B4_CRC_MASK |
				GENFSK_WHITEN_CFG_WHITEN_POLY_TYPE_MASK |
				GENFSK_WHITEN_CFG_WHITEN_REF_IN_MASK |
				GENFSK_WHITEN_CFG_WHITEN_PAYLOAD_REINIT_MASK |
				GENFSK_WHITEN_CFG_WHITEN_SIZE_MASK |
				GENFSK_WHITEN_CFG_MANCHESTER_EN_MASK |
				GENFSK_WHITEN_CFG_MANCHESTER_INV_MASK |
				GENFSK_WHITEN_CFG_MANCHESTER_START_MASK |
				GENFSK_WHITEN_CFG_WHITEN_INIT_MASK);

	GENFSK->WHITEN_CFG |=
	    GENFSK_WHITEN_CFG_WHITEN_START(GENFSK_BLE_WHITEN_START) |
	    GENFSK_WHITEN_CFG_WHITEN_END(GENFSK_BLE_WHITEN_END) |
	    GENFSK_WHITEN_CFG_WHITEN_B4_CRC(0) |
	    GENFSK_WHITEN_CFG_WHITEN_POLY_TYPE(GENFSK_BLE_WHITEN_POLY_TYPE) |
	    GENFSK_WHITEN_CFG_WHITEN_REF_IN(0) |
	    GENFSK_WHITEN_CFG_WHITEN_PAYLOAD_REINIT(0) |
	    GENFSK_WHITEN_CFG_WHITEN_SIZE(GENFSK_BLE_WHITEN_SIZE) |
	    GENFSK_WHITEN_CFG_MANCHESTER_EN(0) |
	    GENFSK_WHITEN_CFG_MANCHESTER_INV(0) |
	    GENFSK_WHITEN_CFG_MANCHESTER_START(0) |
	    GENFSK_WHITEN_CFG_WHITEN_INIT(iv | 0x40);

	GENFSK->WHITEN_POLY =
		GENFSK_WHITEN_POLY_WHITEN_POLY(GENFSK_BLE_WHITEN_POLY);

	GENFSK->WHITEN_SZ_THR &= ~GENFSK_WHITEN_SZ_THR_WHITEN_SZ_THR_MASK;
	GENFSK->WHITEN_SZ_THR |= GENFSK_WHITEN_SZ_THR_WHITEN_SZ_THR(0);
}

void radio_aa_set(u8_t *aa)
{
	/* Configure Access Address detection using NETWORK ADDRESS 0 */
	GENFSK->NTW_ADR_0 = *((u32_t *)aa);
	GENFSK->NTW_ADR_CTRL &= ~(GENFSK_NTW_ADR_CTRL_NTW_ADR0_SZ_MASK |
				  GENFSK_NTW_ADR_CTRL_NTW_ADR_THR0_MASK);
	GENFSK->NTW_ADR_CTRL |= GENFSK_NTW_ADR_CTRL_NTW_ADR0_SZ(3) |
				GENFSK_NTW_ADR_CTRL_NTW_ADR_THR0(0);

	GENFSK->NTW_ADR_CTRL |= (uint32_t) ((1 << 0) <<
			GENFSK_NTW_ADR_CTRL_NTW_ADR_EN_SHIFT);

	/*
	 * The Access Address must be written in the packet buffer
	 * in front of the PDU
	 */
	GENFSK->PACKET_BUFFER[0] = (aa[1] << 8) + aa[0];
	GENFSK->PACKET_BUFFER[1] = (aa[3] << 8) + aa[2];
}

#define GENFSK_BLE_CRC_SZ	3 /* 3 bytes */
#define GENFSK_BLE_PREAMBLE_SZ	0 /* 1 byte of preamble, depends on PHY type */
#define GENFSK_BLE_LEN_BIT_ORD	0 /* LSB */
#define GENFSK_BLE_SYNC_ADDR_SZ	3 /* 4 bytes, Access Address */
#define GENFSK_BLE_LEN_ADJ_SZ	GENFSK_BLE_CRC_SZ /* adjust length with CRC
						   * size
						   */
#define GENFSK_BLE_H0_SZ	8 /* 8 bits */

void radio_pkt_configure(u8_t bits_len, u8_t max_len, u8_t flags)
{
	ARG_UNUSED(flags);

	payload_max_size = max_len;

	GENFSK->XCVR_CFG &= ~GENFSK_XCVR_CFG_PREAMBLE_SZ_MASK;
	GENFSK->XCVR_CFG |=
		GENFSK_XCVR_CFG_PREAMBLE_SZ(GENFSK_BLE_PREAMBLE_SZ);

	GENFSK->PACKET_CFG &= ~(GENFSK_PACKET_CFG_LENGTH_SZ_MASK |
			GENFSK_PACKET_CFG_LENGTH_BIT_ORD_MASK |
			GENFSK_PACKET_CFG_SYNC_ADDR_SZ_MASK |
			GENFSK_PACKET_CFG_LENGTH_ADJ_MASK |
			GENFSK_PACKET_CFG_H0_SZ_MASK |
			GENFSK_PACKET_CFG_H1_SZ_MASK);

	GENFSK->PACKET_CFG |= GENFSK_PACKET_CFG_LENGTH_SZ(bits_len) |
		GENFSK_PACKET_CFG_LENGTH_BIT_ORD(GENFSK_BLE_LEN_BIT_ORD) |
		GENFSK_PACKET_CFG_SYNC_ADDR_SZ(GENFSK_BLE_SYNC_ADDR_SZ) |
		GENFSK_PACKET_CFG_LENGTH_ADJ(GENFSK_BLE_LEN_ADJ_SZ) |
		GENFSK_PACKET_CFG_H0_SZ(GENFSK_BLE_H0_SZ) |
		GENFSK_PACKET_CFG_H1_SZ((8 - bits_len));

	GENFSK->H0_CFG &= ~(GENFSK_H0_CFG_H0_MASK_MASK |
			    GENFSK_H0_CFG_H0_MATCH_MASK);
	GENFSK->H0_CFG |= GENFSK_H0_CFG_H0_MASK(0) |
			  GENFSK_H0_CFG_H0_MATCH(0);

	GENFSK->H1_CFG &= ~(GENFSK_H1_CFG_H1_MASK_MASK |
			    GENFSK_H1_CFG_H1_MATCH_MASK);
	GENFSK->H1_CFG |= GENFSK_H1_CFG_H1_MASK(0) |
			  GENFSK_H1_CFG_H1_MATCH(0);

	/* set Rx watermak to AA + PDU header */
	GENFSK->RX_WATERMARK = GENFSK_RX_WATERMARK_RX_WATERMARK(RX_WTMRK);
}

void radio_pkt_rx_set(void *rx_packet)
{
	rx_pkt_ptr = rx_packet;
}

void radio_pkt_tx_set(void *tx_packet)
{
	/*
	 * The GENERIC_FSK software must program the TX buffer
	 * before commanding a TX operation, and must not access the RAM during
	 * the transmission.
	 */
	u16_t *pkt = tx_packet;
	u32_t cnt = 0, pkt_len = (((u8_t *)tx_packet)[1] + 1) / 2 + 1;
	volatile u16_t *pkt_buffer = &GENFSK->PACKET_BUFFER[PB_TX_PDU];

	for (; cnt < pkt_len; cnt++) {
		pkt_buffer[cnt] = pkt[cnt];
	}
}

u32_t radio_tx_ready_delay_get(u8_t phy, u8_t flags)
{
	return tx_wu;
}

u32_t radio_tx_chain_delay_get(u8_t phy, u8_t flags)
{
	return 0;
}

u32_t radio_rx_ready_delay_get(u8_t phy, u8_t flags)
{
	return rx_wu;
}

u32_t radio_rx_chain_delay_get(u8_t phy, u8_t flags)
{
	/* RX_WTMRK = AA + PDU header, but AA time is already accounted for */
	/* PDU header (assume 2 bytes) => 16us, depends on PHY type */
	/* 2 * Rx_OVHD = RX_WATERMARK_IRQ time - TIMESTAMP - isr_latency */
	/* The rest is Rx margin that for now isn't well defined */
	return 16 + 2 * Rx_OVHD + RX_MARGIN + isr_latency + Rx_OVHD;
}

void radio_rx_enable(void)
{
	/* Wait for idle state */
	while (GENFSK->XCVR_CTRL & GENFSK_XCVR_CTRL_XCVR_BUSY_MASK) {
	}

	/* 0b0101..RX Start Now */
	GENFSK->XCVR_CTRL = GENFSK_XCVR_CTRL_SEQCMD(0x5);
}

void radio_tx_enable(void)
{
	/* Wait for idle state */
	while (GENFSK->XCVR_CTRL & GENFSK_XCVR_CTRL_XCVR_BUSY_MASK) {
	}

	/* 0b0001..TX Start Now */
	GENFSK->XCVR_CTRL = GENFSK_XCVR_CTRL_SEQCMD(0x1);
}

void radio_disable(void)
{
	/*
	 * 0b1011..Abort All - Cancels all pending events and abort any
	 * sequence-in-progress
	 */
	GENFSK->XCVR_CTRL = GENFSK_XCVR_CTRL_SEQCMD(0xb);

	/* generate T2 interrupt to get into isr_radio() */
	u32_t tmr = GENFSK->EVENT_TMR + 8;

	GENFSK->T2_CMP = GENFSK_T2_CMP_T2_CMP(tmr) | GENFSK_T2_CMP_T2_CMP_EN(1);
}

void radio_status_reset(void)
{
	radio_trx = 0;
}

u32_t radio_is_ready(void)
{
	/* Always false. LLL expects the radio not to be in idle/Tx/Rx state */
	return 0;
}

u32_t radio_is_done(void)
{
	return radio_trx;
}

u32_t radio_has_disabled(void)
{
	/* Not used */
	return 0;
}

u32_t radio_is_idle(void)
{
	/* Vega radio is never disabled */
	return 1;
}

#define GENFSK_BLE_CRC_START_BYTE	4 /* After Access Address */
#define GENFSK_BLE_CRC_BYTE_ORD		0 /* LSB */

void radio_crc_configure(u32_t polynomial, u32_t iv)
{
	/* printk("%s poly: %08x, iv: %08x\n", __func__, polynomial, iv); */

	GENFSK->CRC_CFG &= ~(GENFSK_CRC_CFG_CRC_SZ_MASK |
			     GENFSK_CRC_CFG_CRC_START_BYTE_MASK |
			     GENFSK_CRC_CFG_CRC_REF_IN_MASK |
			     GENFSK_CRC_CFG_CRC_REF_OUT_MASK |
			     GENFSK_CRC_CFG_CRC_BYTE_ORD_MASK);
	GENFSK->CRC_CFG |= GENFSK_CRC_CFG_CRC_SZ(GENFSK_BLE_CRC_SZ) |
		GENFSK_CRC_CFG_CRC_START_BYTE(GENFSK_BLE_CRC_START_BYTE) |
		GENFSK_CRC_CFG_CRC_REF_IN(0) |
		GENFSK_CRC_CFG_CRC_REF_OUT(0) |
		GENFSK_CRC_CFG_CRC_BYTE_ORD(GENFSK_BLE_CRC_BYTE_ORD);

	GENFSK->CRC_INIT = (iv << ((4U - GENFSK_BLE_CRC_SZ) << 3));
	GENFSK->CRC_POLY = (polynomial << ((4U - GENFSK_BLE_CRC_SZ) << 3));
	GENFSK->CRC_XOR_OUT = 0;

	/*
	 * Enable CRC in hardware; this is already done at startup but we do it
	 * here anyways just to be sure.
	 */
	GENFSK->XCVR_CFG &= ~GENFSK_XCVR_CFG_SW_CRC_EN_MASK;
}

u32_t radio_crc_is_valid(void)
{
	if (force_bad_crc)
		return 0;

	u32_t radio_crc = (GENFSK->XCVR_STS & GENFSK_XCVR_STS_CRC_VALID_MASK) >>
						GENFSK_XCVR_STS_CRC_VALID_SHIFT;
	return radio_crc;
}

void *radio_pkt_empty_get(void)
{
	return _pkt_empty;
}

void *radio_pkt_scratch_get(void)
{
	return _pkt_scratch;
}

void radio_switch_complete_and_rx(u8_t phy_rx)
{
	/*  0b0110..RX Start @ T1 Timer Compare Match (EVENT_TMR = T1_CMP) */
	next_radio_cmd = GENFSK_XCVR_CTRL_SEQCMD(0x6);

	/* the margin is used to account for any overhead in radio switching */
	next_wu = rx_wu + RX_MARGIN;
}

void radio_switch_complete_and_tx(u8_t phy_rx, u8_t flags_rx, u8_t phy_tx,
				  u8_t flags_tx)
{
	/*  0b0010..TX Start @ T1 Timer Compare Match (EVENT_TMR = T1_CMP) */
	next_radio_cmd = GENFSK_XCVR_CTRL_SEQCMD(0x2);

	/* the margin is used to account for any overhead in radio switching */
	next_wu = tx_wu + TX_MARGIN;
}

void radio_switch_complete_and_disable(void)
{
	next_radio_cmd = 0;
}

void radio_rssi_measure(void)
{
	rssi = 0;
}

u32_t radio_rssi_get(void)
{
	return (u32_t)-rssi;
}

void radio_rssi_status_reset(void)
{
}

u32_t radio_rssi_is_ready(void)
{
	return (rssi != 0);
}

void radio_filter_configure(u8_t bitmask_enable, u8_t bitmask_addr_type,
			    u8_t *bdaddr)
{
	printk("%s\n", __func__);
}

void radio_filter_disable(void)
{
	/* Nothing to do here */
}

void radio_filter_status_reset(void)
{
	/* printk("%s\n", __func__); */
}

u32_t radio_filter_has_match(void)
{
	/* printk("%s\n", __func__); */
	return 0;
}

u32_t radio_filter_match_get(void)
{
	/* printk("%s\n", __func__); */
	return 0;
}

void radio_bc_configure(u32_t n)
{
	printk("%s\n", __func__);
}

void radio_bc_status_reset(void)
{
	printk("%s\n", __func__);
}

u32_t radio_bc_has_match(void)
{
	printk("%s\n", __func__);
	return 0;
}

void radio_tmr_status_reset(void)
{
	tmr_aa_save = 0;
	tmr_end_save = 0;
}

void radio_tmr_tifs_set(u32_t tifs)
{
	tmr_tifs = tifs;
}

/* Start the radio after ticks_start (ticks) + remainder (us) time */
static u32_t radio_tmr_start_hlp(u8_t trx, u32_t ticks_start, u32_t remainder)
{
	u32_t radio_start_now_cmd = 0;

	/* Save it for later */
	rtc_start = ticks_start;

	/* Convert ticks to us and use just EVENT_TMR */
	rtc_diff_start_us = HAL_TICKER_TICKS_TO_US(rtc_start - cntr_cnt_get());

	skip_hcto = 0;
	if (rtc_diff_start_us > GENFSK_T1_CMP_T1_CMP_MASK) {
		/* ticks_start already passed. Don't start the radio */
		rtc_diff_start_us = 0;

		/* Ignore time out as well */
		skip_hcto = 1;
		return remainder;
	}
	remainder += rtc_diff_start_us;

	if (trx) {
		if (remainder <= MIN_CMD_TIME) {
			/* 0b0001..TX Start Now */
			radio_start_now_cmd = GENFSK_XCVR_CTRL_SEQCMD(0x1);
			remainder = 0;
		} else {
			/*
			 * 0b0010..TX Start @ T1 Timer Compare Match
			 *         (EVENT_TMR = T1_CMP)
			 */
			GENFSK->XCVR_CTRL = GENFSK_XCVR_CTRL_SEQCMD(0x2);
			GENFSK->T1_CMP = GENFSK_T1_CMP_T1_CMP(remainder);
		}
		tmr_ready = remainder + tx_wu;
	} else {
		if (remainder <= MIN_CMD_TIME) {
			/* 0b0101..RX Start Now */
			radio_start_now_cmd = GENFSK_XCVR_CTRL_SEQCMD(0x5);
			remainder = 0;
		} else {
			/*
			 * 0b0110..RX Start @ T1 Timer Compare Match
			 *         (EVENT_TMR = T1_CMP)
			 */
			GENFSK->XCVR_CTRL = GENFSK_XCVR_CTRL_SEQCMD(0x6);
			GENFSK->T1_CMP = GENFSK_T1_CMP_T1_CMP(remainder);
		}
		tmr_ready = remainder + rx_wu;
	}

	/*
	 * reset EVENT_TMR should be done after ticks_start.
	 * We converted ticks to us, so we reset it now.
	 * All tmr_* times are relative to EVENT_TMR reset.
	 * rtc_diff_start_us is used to adjust them
	 */
	GENFSK->EVENT_TMR = GENFSK_EVENT_TMR_EVENT_TMR_LD(1);

	if (radio_start_now_cmd) {
		/* trigger Rx/Tx Start Now */
		GENFSK->XCVR_CTRL = radio_start_now_cmd;
	} else {
		/* enable T1_CMP to trigger the SEQCMD */
		GENFSK->T1_CMP |= GENFSK_T1_CMP_T1_CMP_EN(1);
	}

	return remainder;
}

u32_t radio_tmr_start(u8_t trx, u32_t ticks_start, u32_t remainder)
{
	if ((!(remainder / 1000000UL)) || (remainder & 0x80000000)) {
		ticks_start--;
		remainder += 30517578UL;
	}
	remainder /= 1000000UL;

	return radio_tmr_start_hlp(trx, ticks_start, remainder);
}

u32_t radio_tmr_start_tick(u8_t trx, u32_t tick)
{
	/* Setup compare event with min. 1 us offset */
	u32_t remainder_us = 1;

	return radio_tmr_start_hlp(trx, tick, remainder_us);
}

void radio_tmr_start_us(u8_t trx, u32_t us)
{
	printk("%s\n", __func__);
}

u32_t radio_tmr_start_now(u8_t trx)
{
	printk("%s\n", __func__);
	return 0;
}

u32_t radio_tmr_start_get(void)
{
	return rtc_start;
}

void radio_tmr_stop(void)
{
	/* Deep Sleep Mode (DSM)? */
}

void radio_tmr_hcto_configure(u32_t hcto)
{
	if (skip_hcto) {
		skip_hcto = 0;
		return;
	}

	GENFSK->T2_CMP = GENFSK_T2_CMP_T2_CMP(hcto) |
			 GENFSK_T2_CMP_T2_CMP_EN(1);

	/* 0b1001..RX Stop @ T2 Timer Compare Match (EVENT_TMR = T2_CMP) */
	GENFSK->XCVR_CTRL = GENFSK_XCVR_CTRL_SEQCMD(0x9);
}

void radio_tmr_aa_capture(void)
{
	tmr_aa_save = 1;
}

u32_t radio_tmr_aa_get(void)
{
	return tmr_aa - rtc_diff_start_us;
}

static u32_t radio_tmr_aa;

void radio_tmr_aa_save(u32_t aa)
{
	radio_tmr_aa = aa;
}

u32_t radio_tmr_aa_restore(void)
{
	return radio_tmr_aa;
}

u32_t radio_tmr_ready_get(void)
{
	return tmr_ready - rtc_diff_start_us;
}

void radio_tmr_end_capture(void)
{
	tmr_end_save = 1;
}

u32_t radio_tmr_end_get(void)
{
	return tmr_end - rtc_diff_start_us;
}

u32_t radio_tmr_tifs_base_get(void)
{
	return radio_tmr_end_get() + rtc_diff_start_us;
}

void radio_tmr_sample(void)
{
	printk("%s\n", __func__);
}

u32_t radio_tmr_sample_get(void)
{
	printk("%s\n", __func__);
	return 0;
}

void *radio_ccm_rx_pkt_set(struct ccm *ccm, u8_t phy, void *pkt)
{
	printk("%s\n", __func__);
	return NULL;
}

void *radio_ccm_tx_pkt_set(struct ccm *ccm, void *pkt)
{
	printk("%s\n", __func__);
	return NULL;
}

u32_t radio_ccm_is_done(void)
{
	printk("%s\n", __func__);
	return 0;
}

u32_t radio_ccm_mic_is_valid(void)
{
	printk("%s\n", __func__);
	return 0;
}

void radio_ar_configure(u32_t nirk, void *irk)
{
	printk("%s\n", __func__);
}

u32_t radio_ar_match_get(void)
{
	/* printk("%s\n", __func__); */
	return 0;
}

void radio_ar_status_reset(void)
{
	/* printk("%s\n", __func__); */
}

u32_t radio_ar_has_match(void)
{
	/* printk("%s\n", __func__); */
	return 0;
}
