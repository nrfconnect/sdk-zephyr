/*
 * Copyright (c) 2018-2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdbool.h>

#include <toolchain.h>
#include <zephyr/types.h>
#include <sys/util.h>

#include "hal/ccm.h"
#include "hal/radio.h"
#include "hal/ticker.h"

#include "util/memq.h"

#include "pdu.h"

#include "lll.h"
#include "lll_vendor.h"
#include "lll_conn.h"
#include "lll_master.h"
#include "lll_chan.h"

#include "lll_internal.h"
#include "lll_tim_internal.h"

#define BT_DBG_ENABLED IS_ENABLED(CONFIG_BT_DEBUG_HCI_DRIVER)
#define LOG_MODULE_NAME bt_ctlr_lll_master
#include "common/log.h"
#include <soc.h>
#include "hal/debug.h"

static int init_reset(void);
static int prepare_cb(struct lll_prepare_param *prepare_param);

int lll_master_init(void)
{
	int err;

	err = init_reset();
	if (err) {
		return err;
	}

	return 0;
}

int lll_master_reset(void)
{
	int err;

	err = init_reset();
	if (err) {
		return err;
	}

	return 0;
}

void lll_master_prepare(void *param)
{
	struct lll_prepare_param *p = param;
	int err;

	err = lll_clk_on();
	LL_ASSERT(!err || err == -EINPROGRESS);

	err = lll_prepare(lll_conn_is_abort_cb, lll_conn_abort_cb, prepare_cb,
			  0, p);
	LL_ASSERT(!err || err == -EINPROGRESS);
}

static int init_reset(void)
{
	return 0;
}

static int prepare_cb(struct lll_prepare_param *prepare_param)
{
	struct lll_conn *lll = prepare_param->param;
	u32_t ticks_at_event, ticks_at_start;
	struct pdu_data *pdu_data_tx;
	struct evt_hdr *evt;
	u16_t event_counter;
	u32_t remainder_us;
	u8_t data_chan_use;
	u32_t remainder;
	u16_t lazy;

	DEBUG_RADIO_START_M(1);

	/* TODO: Do the below in ULL ?  */

	lazy = prepare_param->lazy;

	/* save the latency for use in event */
	lll->latency_prepare += lazy;

	/* calc current event counter value */
	event_counter = lll->event_counter + lll->latency_prepare;

	/* store the next event counter value */
	lll->event_counter = event_counter + 1;

	/* TODO: Do the above in ULL ?  */

	/* Reset connection event global variables */
	lll_conn_prepare_reset();

	/* TODO: can we do something in ULL? */
	lll->latency_event = lll->latency_prepare;
	lll->latency_prepare = 0;

	if (lll->data_chan_sel) {
#if defined(CONFIG_BT_CTLR_CHAN_SEL_2)
		data_chan_use = lll_chan_sel_2(lll->event_counter - 1,
					       lll->data_chan_id,
					       &lll->data_chan_map[0],
					       lll->data_chan_count);
#else /* !CONFIG_BT_CTLR_CHAN_SEL_2 */
		data_chan_use = 0;
		LL_ASSERT(0);
#endif /* !CONFIG_BT_CTLR_CHAN_SEL_2 */
	} else {
		data_chan_use = lll_chan_sel_1(&lll->data_chan_use,
					       lll->data_chan_hop,
					       lll->latency_event,
					       &lll->data_chan_map[0],
					       lll->data_chan_count);
	}

	/* Prepare the Tx PDU */
	lll_conn_pdu_tx_prep(lll, &pdu_data_tx);
	pdu_data_tx->sn = lll->sn;
	pdu_data_tx->nesn = lll->nesn;

	/* Start setting up of Radio h/w */
	radio_reset();
#if defined(CONFIG_BT_CTLR_TX_PWR_DYNAMIC_CONTROL)
	radio_tx_power_set(lll->tx_pwr_lvl);
#else
	radio_tx_power_set(RADIO_TXP_DEFAULT);
#endif

	radio_aa_set(lll->access_addr);
	radio_crc_configure(((0x5bUL) | ((0x06UL) << 8) | ((0x00UL) << 16)),
			    (((u32_t)lll->crc_init[2] << 16) |
			     ((u32_t)lll->crc_init[1] << 8) |
			     ((u32_t)lll->crc_init[0])));
	lll_chan_set(data_chan_use);

	/* setup the radio tx packet buffer */
	lll_conn_tx_pkt_set(lll, pdu_data_tx);

	radio_isr_set(lll_conn_isr_tx, lll);

	radio_tmr_tifs_set(EVENT_IFS_US);

#if defined(CONFIG_BT_CTLR_PHY)
	radio_switch_complete_and_rx(lll->phy_rx);
#else /* !CONFIG_BT_CTLR_PHY */
	radio_switch_complete_and_rx(0);
#endif /* !CONFIG_BT_CTLR_PHY */

	ticks_at_event = prepare_param->ticks_at_expire;
	evt = HDR_LLL2EVT(lll);
	ticks_at_event += lll_evt_offset_get(evt);

	ticks_at_start = ticks_at_event;
	ticks_at_start += HAL_TICKER_US_TO_TICKS(EVENT_OVERHEAD_START_US);

	remainder = prepare_param->remainder;
	remainder_us = radio_tmr_start(1, ticks_at_start, remainder);

	/* capture end of Tx-ed PDU, used to calculate HCTO. */
	radio_tmr_end_capture();

#if defined(CONFIG_BT_CTLR_GPIO_PA_PIN)
	radio_gpio_pa_setup();

#if defined(CONFIG_BT_CTLR_PHY)
	radio_gpio_pa_lna_enable(remainder_us +
				 radio_tx_ready_delay_get(lll->phy_tx,
							  lll->phy_flags) -
				 CONFIG_BT_CTLR_GPIO_PA_OFFSET);
#else /* !CONFIG_BT_CTLR_PHY */
	radio_gpio_pa_lna_enable(remainder_us +
				 radio_tx_ready_delay_get(0, 0) -
				 CONFIG_BT_CTLR_GPIO_PA_OFFSET);
#endif /* !CONFIG_BT_CTLR_PHY */
#else /* !CONFIG_BT_CTLR_GPIO_PA_PIN */
	ARG_UNUSED(remainder_us);
#endif /* !CONFIG_BT_CTLR_GPIO_PA_PIN */

#if defined(CONFIG_BT_CTLR_XTAL_ADVANCED) && \
	(EVENT_OVERHEAD_PREEMPT_US <= EVENT_OVERHEAD_PREEMPT_MIN_US)
	/* check if preempt to start has changed */
	if (lll_preempt_calc(evt, (TICKER_ID_CONN_BASE + lll->handle),
			     ticks_at_event)) {
		radio_isr_set(lll_conn_isr_abort, lll);
		radio_disable();
	} else
#endif /* CONFIG_BT_CTLR_XTAL_ADVANCED */
	{
		u32_t ret;

		ret = lll_prepare_done(lll);
		LL_ASSERT(!ret);
	}

	DEBUG_RADIO_START_M(1);

	return 0;
}
