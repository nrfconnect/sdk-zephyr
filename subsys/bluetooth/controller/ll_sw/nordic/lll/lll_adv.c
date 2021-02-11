/*
 * Copyright (c) 2018-2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include <bluetooth/hci.h>
#include <sys/byteorder.h>
#include <soc.h>

#include "hal/cpu.h"
#include "hal/ccm.h"
#include "hal/radio.h"
#include "hal/ticker.h"

#include "util/util.h"
#include "util/mem.h"
#include "util/memq.h"
#include "util/mfifo.h"

#include "ticker/ticker.h"

#include "pdu.h"

#include "lll.h"
#include "lll_vendor.h"
#include "lll_clock.h"
#include "lll_adv.h"
#include "lll_adv_aux.h"
#include "lll_conn.h"
#include "lll_chan.h"
#include "lll_filter.h"

#include "lll_internal.h"
#include "lll_tim_internal.h"
#include "lll_adv_internal.h"
#include "lll_prof_internal.h"

#define BT_DBG_ENABLED IS_ENABLED(CONFIG_BT_DEBUG_HCI_DRIVER)
#define LOG_MODULE_NAME bt_ctlr_lll_adv
#include "common/log.h"
#include "hal/debug.h"

static int init_reset(void);
static int prepare_cb(struct lll_prepare_param *p);
static int is_abort_cb(void *next, int prio, void *curr,
		       lll_prepare_cb_t *resume_cb, int *resume_prio);
static void abort_cb(struct lll_prepare_param *prepare_param, void *param);
static void isr_tx(void *param);
static void isr_rx(void *param);
static void isr_done(void *param);
static void isr_abort(void *param);
static struct pdu_adv *chan_prepare(struct lll_adv *lll);

static inline int isr_rx_pdu(struct lll_adv *lll,
			     uint8_t devmatch_ok, uint8_t devmatch_id,
			     uint8_t irkmatch_ok, uint8_t irkmatch_id,
			     uint8_t rssi_ready);
static bool isr_rx_sr_adva_check(uint8_t tx_addr, uint8_t *addr,
				 struct pdu_adv *sr);


static inline bool isr_rx_ci_tgta_check(struct lll_adv *lll,
					uint8_t rx_addr, uint8_t *tgt_addr,
					struct pdu_adv *ci, uint8_t rl_idx);
static inline bool isr_rx_ci_adva_check(uint8_t tx_addr, uint8_t *addr,
					struct pdu_adv *ci);

#if defined(CONFIG_BT_CTLR_ADV_EXT)
#define PAYLOAD_FRAG_COUNT   ((CONFIG_BT_CTLR_ADV_DATA_LEN_MAX + \
			       PDU_AC_PAYLOAD_SIZE_MAX - 1) / \
			      PDU_AC_PAYLOAD_SIZE_MAX)
#define BT_CTLR_ADV_AUX_SET  CONFIG_BT_CTLR_ADV_AUX_SET
#if defined(CONFIG_BT_CTLR_ADV_PERIODIC)
#define BT_CTLR_ADV_SYNC_SET CONFIG_BT_CTLR_ADV_SYNC_SET
#else /* !CONFIG_BT_CTLR_ADV_PERIODIC */
#define BT_CTLR_ADV_SYNC_SET 0
#endif /* !CONFIG_BT_CTLR_ADV_PERIODIC */
#else
#define PAYLOAD_FRAG_COUNT   1
#define BT_CTLR_ADV_AUX_SET  0
#define BT_CTLR_ADV_SYNC_SET 0
#endif

#define PDU_MEM_SIZE       MROUND(PDU_AC_LL_HEADER_SIZE + \
				  PDU_AC_PAYLOAD_SIZE_MAX)
#define PDU_MEM_COUNT_MIN  (BT_CTLR_ADV_SET + \
			    (BT_CTLR_ADV_SET * PAYLOAD_FRAG_COUNT) + \
			    (BT_CTLR_ADV_AUX_SET * PAYLOAD_FRAG_COUNT) + \
			    (BT_CTLR_ADV_SYNC_SET * PAYLOAD_FRAG_COUNT))
#define PDU_MEM_FIFO_COUNT ((BT_CTLR_ADV_SET * PAYLOAD_FRAG_COUNT * 2) + \
			    (CONFIG_BT_CTLR_ADV_DATA_BUF_MAX * \
			     PAYLOAD_FRAG_COUNT))
#define PDU_MEM_COUNT      (PDU_MEM_COUNT_MIN + PDU_MEM_FIFO_COUNT)
#define PDU_POOL_SIZE      (PDU_MEM_SIZE * PDU_MEM_COUNT)

/* Free AD data PDU buffer pool */
static struct {
	void *free;
	uint8_t pool[PDU_POOL_SIZE];
} mem_pdu;

/* FIFO to return stale AD data PDU buffers from LLL to thread context */
static MFIFO_DEFINE(pdu_free, sizeof(void *), PDU_MEM_FIFO_COUNT);

/* Semaphore to wakeup thread waiting for free AD data PDU buffers */
static struct k_sem sem_pdu_free;

int lll_adv_init(void)
{
	int err;

#if defined(CONFIG_BT_CTLR_ADV_EXT)
#if (BT_CTLR_ADV_AUX_SET > 0)
	err = lll_adv_aux_init();
	if (err) {
		return err;
	}
#endif /* BT_CTLR_ADV_AUX_SET > 0 */
#endif /* CONFIG_BT_CTLR_ADV_EXT */

	err = init_reset();
	if (err) {
		return err;
	}

	return 0;
}

int lll_adv_reset(void)
{
	int err;

#if defined(CONFIG_BT_CTLR_ADV_EXT)
#if (BT_CTLR_ADV_AUX_SET > 0)
	err = lll_adv_aux_reset();
	if (err) {
		return err;
	}
#endif /* BT_CTLR_ADV_AUX_SET > 0 */
#endif /* CONFIG_BT_CTLR_ADV_EXT */

	err = init_reset();
	if (err) {
		return err;
	}

	return 0;
}

int lll_adv_data_init(struct lll_adv_pdu *pdu)
{
	struct pdu_adv *p;

	p = mem_acquire(&mem_pdu.free);
	if (!p) {
		return -ENOMEM;
	}

	p->len = 0U;
	pdu->pdu[0] = (void *)p;

	return 0;
}

int lll_adv_data_reset(struct lll_adv_pdu *pdu)
{
	/* NOTE: this function is used on HCI reset to mem-zero the structure
	 *       members that otherwise was zero-ed by the architecture
	 *       startup code that zero-ed the .bss section.
	 *       pdu[0] element in the array is not initialized as subsequent
	 *       call to lll_adv_data_init will allocate a PDU buffer and
	 *       assign that.
	 */

	pdu->first = 0U;
	pdu->last = 0U;
	pdu->pdu[1] = NULL;

	return 0;
}

int lll_adv_data_release(struct lll_adv_pdu *pdu)
{
	uint8_t last;
	void *p;

	last = pdu->last;
	p = pdu->pdu[last];
	pdu->pdu[last] = NULL;
	mem_release(p, &mem_pdu.free);

	last++;
	if (last == DOUBLE_BUFFER_SIZE) {
		last = 0U;
	}
	p = pdu->pdu[last];
	if (p) {
		pdu->pdu[last] = NULL;
		mem_release(p, &mem_pdu.free);
	}

	return 0;
}

struct pdu_adv *lll_adv_pdu_alloc(struct lll_adv_pdu *pdu, uint8_t *idx)
{
	uint8_t first, last;
	struct pdu_adv *p;
	int err;

	first = pdu->first;
	last = pdu->last;
	if (first == last) {
		last++;
		if (last == DOUBLE_BUFFER_SIZE) {
			last = 0U;
		}
	} else {
		uint8_t first_latest;

		pdu->last = first;
		cpu_dsb();
		first_latest = pdu->first;
		if (first_latest != first) {
			last++;
			if (last == DOUBLE_BUFFER_SIZE) {
				last = 0U;
			}
		}
	}

	*idx = last;

	p = (void *)pdu->pdu[last];
	if (p) {
		return p;
	}

	p = MFIFO_DEQUEUE_PEEK(pdu_free);
	if (p) {
		err = k_sem_take(&sem_pdu_free, K_NO_WAIT);
		LL_ASSERT(!err);

		MFIFO_DEQUEUE(pdu_free);
		pdu->pdu[last] = (void *)p;

		return p;
	}

	p = mem_acquire(&mem_pdu.free);
	if (p) {
		pdu->pdu[last] = (void *)p;

		return p;
	}

	err = k_sem_take(&sem_pdu_free, K_FOREVER);
	LL_ASSERT(!err);

	p = MFIFO_DEQUEUE(pdu_free);
	LL_ASSERT(p);

	pdu->pdu[last] = (void *)p;

	return p;
}

struct pdu_adv *lll_adv_pdu_latest_get(struct lll_adv_pdu *pdu,
				       uint8_t *is_modified)
{
	uint8_t first;

	first = pdu->first;
	if (first != pdu->last) {
		uint8_t free_idx;
		uint8_t pdu_idx;
		void *p;

		if (!MFIFO_ENQUEUE_IDX_GET(pdu_free, &free_idx)) {
			LL_ASSERT(false);

			return NULL;
		}

		pdu_idx = first;

		first += 1U;
		if (first == DOUBLE_BUFFER_SIZE) {
			first = 0U;
		}
		pdu->first = first;
		*is_modified = 1U;

		p = pdu->pdu[pdu_idx];
		pdu->pdu[pdu_idx] = NULL;

		MFIFO_BY_IDX_ENQUEUE(pdu_free, free_idx, p);
		k_sem_give(&sem_pdu_free);
	}

	return (void *)pdu->pdu[first];
}

void lll_adv_prepare(void *param)
{
	int err;

	err = lll_hfclock_on();
	LL_ASSERT(err >= 0);

	err = lll_prepare(is_abort_cb, abort_cb, prepare_cb, 0, param);
	LL_ASSERT(!err || err == -EINPROGRESS);
}

bool lll_adv_scan_req_check(struct lll_adv *lll, struct pdu_adv *sr,
			    uint8_t tx_addr, uint8_t *addr,
			    uint8_t devmatch_ok, uint8_t *rl_idx)
{
#if defined(CONFIG_BT_CTLR_PRIVACY)
	return ((((lll->filter_policy & 0x01) == 0) &&
		 ull_filter_lll_rl_addr_allowed(sr->tx_addr,
						sr->scan_req.scan_addr,
						rl_idx)) ||
		(((lll->filter_policy & 0x01) != 0) &&
		 (devmatch_ok || ull_filter_lll_irk_whitelisted(*rl_idx)))) &&
		isr_rx_sr_adva_check(tx_addr, addr, sr);
#else
	return (((lll->filter_policy & 0x01) == 0U) || devmatch_ok) &&
		isr_rx_sr_adva_check(tx_addr, addr, sr);
#endif /* CONFIG_BT_CTLR_PRIVACY */
}

#if defined(CONFIG_BT_CTLR_SCAN_REQ_NOTIFY)
int lll_adv_scan_req_report(struct lll_adv *lll, struct pdu_adv *pdu_adv_rx,
			    uint8_t rl_idx, uint8_t rssi_ready)
{
	struct node_rx_pdu *node_rx;
	struct pdu_adv *pdu_adv;
	uint8_t pdu_len;

	node_rx = ull_pdu_rx_alloc_peek(3);
	if (!node_rx) {
		return -ENOBUFS;
	}
	ull_pdu_rx_alloc();

	/* Prepare the report (scan req) */
	node_rx->hdr.type = NODE_RX_TYPE_SCAN_REQ;
	node_rx->hdr.handle = ull_adv_lll_handle_get(lll);

	/* Make a copy of PDU into Rx node (as the received PDU is in the
	 * scratch buffer), and save the RSSI value.
	 */
	pdu_adv = (void *)node_rx->pdu;
	pdu_len = offsetof(struct pdu_adv, payload) + pdu_adv_rx->len;
	memcpy(pdu_adv, pdu_adv_rx, pdu_len);

	node_rx->hdr.rx_ftr.rssi = (rssi_ready) ? radio_rssi_get() :
						  BT_HCI_LE_RSSI_NOT_AVAILABLE;
#if defined(CONFIG_BT_CTLR_PRIVACY)
	node_rx->hdr.rx_ftr.rl_idx = rl_idx;
#endif

	ull_rx_put(node_rx->hdr.link, node_rx);
	ull_rx_sched();

	return 0;
}
#endif /* CONFIG_BT_CTLR_SCAN_REQ_NOTIFY */

bool lll_adv_connect_ind_check(struct lll_adv *lll, struct pdu_adv *ci,
			       uint8_t tx_addr, uint8_t *addr,
			       uint8_t rx_addr, uint8_t *tgt_addr,
			       uint8_t devmatch_ok, uint8_t *rl_idx)
{
	/* LL 4.3.2: filter policy shall be ignored for directed adv */
	if (tgt_addr) {
#if defined(CONFIG_BT_CTLR_PRIVACY)
		return ull_filter_lll_rl_addr_allowed(ci->tx_addr,
						      ci->connect_ind.init_addr,
						      rl_idx) &&
#else
		return (1) &&
#endif
		       isr_rx_ci_adva_check(tx_addr, addr, ci) &&
		       isr_rx_ci_tgta_check(lll, rx_addr, tgt_addr, ci,
					    *rl_idx);
	}

#if defined(CONFIG_BT_CTLR_PRIVACY)
	return ((((lll->filter_policy & 0x02) == 0) &&
		 ull_filter_lll_rl_addr_allowed(ci->tx_addr,
						ci->connect_ind.init_addr,
						rl_idx)) ||
		(((lll->filter_policy & 0x02) != 0) &&
		 (devmatch_ok || ull_filter_lll_irk_whitelisted(*rl_idx)))) &&
	       isr_rx_ci_adva_check(tx_addr, addr, ci);
#else
	return (((lll->filter_policy & 0x02) == 0) ||
		(devmatch_ok)) &&
	       isr_rx_ci_adva_check(tx_addr, addr, ci);
#endif /* CONFIG_BT_CTLR_PRIVACY */
}

/* Helper function to initialize data variable both at power up and on
 * HCI reset.
 */
static int init_reset(void)
{
	/* Initialize AC PDU pool */
	mem_init(mem_pdu.pool, PDU_MEM_SIZE,
		 (sizeof(mem_pdu.pool) / PDU_MEM_SIZE), &mem_pdu.free);

	/* Initialize AC PDU free buffer return queue */
	MFIFO_INIT(pdu_free);

	/* Initialize semaphore for ticker API blocking wait */
	k_sem_init(&sem_pdu_free, 0, PDU_MEM_FIFO_COUNT);

	return 0;
}

static int prepare_cb(struct lll_prepare_param *p)
{
	uint32_t ticks_at_event;
	uint32_t ticks_at_start;
	struct pdu_adv *pdu;
	struct evt_hdr *evt;
	struct lll_adv *lll;
	uint32_t remainder;
	uint32_t start_us;
	uint32_t aa;

	DEBUG_RADIO_START_A(1);

	lll = p->param;

	/* Check if stopped (on connection establishment race between LLL and
	 * ULL.
	 */
	if (unlikely(lll_is_stop(lll))) {
		int err;

		err = lll_hfclock_off();
		LL_ASSERT(err >= 0);

		lll_done(NULL);

		DEBUG_RADIO_CLOSE_A(0);
		return 0;
	}

	radio_reset();

#if defined(CONFIG_BT_CTLR_TX_PWR_DYNAMIC_CONTROL)
	radio_tx_power_set(lll->tx_pwr_lvl);
#else
	radio_tx_power_set(RADIO_TXP_DEFAULT);
#endif /* CONFIG_BT_CTLR_TX_PWR_DYNAMIC_CONTROL */

#if defined(CONFIG_BT_CTLR_ADV_EXT)
	/* TODO: if coded we use S8? */
	radio_phy_set(lll->phy_p, 1);
	radio_pkt_configure(8, PDU_AC_LEG_PAYLOAD_SIZE_MAX, (lll->phy_p << 1));
#else /* !CONFIG_BT_CTLR_ADV_EXT */
	radio_phy_set(0, 0);
	radio_pkt_configure(8, PDU_AC_LEG_PAYLOAD_SIZE_MAX, 0);
#endif /* !CONFIG_BT_CTLR_ADV_EXT */

	aa = sys_cpu_to_le32(PDU_AC_ACCESS_ADDR);
	radio_aa_set((uint8_t *)&aa);
	radio_crc_configure(((0x5bUL) | ((0x06UL) << 8) | ((0x00UL) << 16)),
			    0x555555);

	lll->chan_map_curr = lll->chan_map;

	pdu = chan_prepare(lll);

#if defined(CONFIG_BT_HCI_MESH_EXT)
	_radio.mesh_adv_end_us = 0;
#endif /* CONFIG_BT_HCI_MESH_EXT */


#if defined(CONFIG_BT_CTLR_PRIVACY)
	if (ull_filter_lll_rl_enabled()) {
		struct lll_filter *filter =
			ull_filter_lll_get(!!(lll->filter_policy));

		radio_filter_configure(filter->enable_bitmask,
				       filter->addr_type_bitmask,
				       (uint8_t *)filter->bdaddr);
	} else
#endif /* CONFIG_BT_CTLR_PRIVACY */

	if (IS_ENABLED(CONFIG_BT_CTLR_FILTER) && lll->filter_policy) {
		/* Setup Radio Filter */
		struct lll_filter *wl = ull_filter_lll_get(true);


		radio_filter_configure(wl->enable_bitmask,
				       wl->addr_type_bitmask,
				       (uint8_t *)wl->bdaddr);
	}

	ticks_at_event = p->ticks_at_expire;
	evt = HDR_LLL2EVT(lll);
	ticks_at_event += lll_evt_offset_get(evt);

	ticks_at_start = ticks_at_event;
	ticks_at_start += HAL_TICKER_US_TO_TICKS(EVENT_OVERHEAD_START_US);

	remainder = p->remainder;
	start_us = radio_tmr_start(1, ticks_at_start, remainder);

	/* capture end of Tx-ed PDU, used to calculate HCTO. */
	radio_tmr_end_capture();

#if defined(CONFIG_BT_CTLR_GPIO_PA_PIN)
	radio_gpio_pa_setup();
	radio_gpio_pa_lna_enable(start_us + radio_tx_ready_delay_get(0, 0) -
				 CONFIG_BT_CTLR_GPIO_PA_OFFSET);
#else /* !CONFIG_BT_CTLR_GPIO_PA_PIN */
	ARG_UNUSED(start_us);
#endif /* !CONFIG_BT_CTLR_GPIO_PA_PIN */

#if defined(CONFIG_BT_CTLR_XTAL_ADVANCED) && \
	(EVENT_OVERHEAD_PREEMPT_US <= EVENT_OVERHEAD_PREEMPT_MIN_US)
	/* check if preempt to start has changed */
	if (lll_preempt_calc(evt, (TICKER_ID_ADV_BASE +
				   ull_adv_lll_handle_get(lll)),
			     ticks_at_event)) {
		radio_isr_set(isr_abort, lll);
		radio_disable();
	} else
#endif /* CONFIG_BT_CTLR_XTAL_ADVANCED */
	{
		uint32_t ret;

		ret = lll_prepare_done(lll);
		LL_ASSERT(!ret);
	}

	DEBUG_RADIO_START_A(1);

	return 0;
}

#if defined(CONFIG_BT_PERIPHERAL)
static int resume_prepare_cb(struct lll_prepare_param *p)
{
	struct evt_hdr *evt;

	evt = HDR_LLL2EVT(p->param);
	p->ticks_at_expire = ticker_ticks_now_get() - lll_evt_offset_get(evt);
	p->remainder = 0;
	p->lazy = 0;

	return prepare_cb(p);
}
#endif /* CONFIG_BT_PERIPHERAL */

static int is_abort_cb(void *next, int prio, void *curr,
		       lll_prepare_cb_t *resume_cb, int *resume_prio)
{
#if defined(CONFIG_BT_PERIPHERAL)
	struct lll_adv *lll = curr;
	struct pdu_adv *pdu;
#endif /* CONFIG_BT_PERIPHERAL */

	/* TODO: prio check */
	if (next != curr) {
		if (0) {
#if defined(CONFIG_BT_PERIPHERAL)
		} else if (lll->is_hdcd) {
			int err;

			/* wrap back after the pre-empter */
			*resume_cb = resume_prepare_cb;
			*resume_prio = 0; /* TODO: */

			/* Retain HF clk */
			err = lll_hfclock_on();
			LL_ASSERT(err >= 0);

			return -EAGAIN;
#endif /* CONFIG_BT_PERIPHERAL */
		} else {
			return -ECANCELED;
		}
	}

#if defined(CONFIG_BT_PERIPHERAL)
	pdu = lll_adv_data_curr_get(lll);
	if (pdu->type == PDU_ADV_TYPE_DIRECT_IND) {
		return 0;
	}
#endif /* CONFIG_BT_PERIPHERAL */

	return -ECANCELED;
}

static void abort_cb(struct lll_prepare_param *prepare_param, void *param)
{
	int err;

	/* NOTE: This is not a prepare being cancelled */
	if (!prepare_param) {
		/* Perform event abort here.
		 * After event has been cleanly aborted, clean up resources
		 * and dispatch event done.
		 */
		radio_isr_set(isr_abort, param);
		radio_disable();
		return;
	}

	/* NOTE: Else clean the top half preparations of the aborted event
	 * currently in preparation pipeline.
	 */
	err = lll_hfclock_off();
	LL_ASSERT(err >= 0);

	lll_done(param);
}

static void isr_tx(void *param)
{
	uint32_t hcto;
#if defined(CONFIG_BT_CTLR_ADV_EXT)
	struct lll_adv *lll = param;
	uint8_t phy_p = lll->phy_p;
#else
	uint8_t phy_p = 0;
#endif

	if (IS_ENABLED(CONFIG_BT_CTLR_PROFILE_ISR)) {
		lll_prof_latency_capture();
	}

	/* Clear radio tx status and events */
	lll_isr_tx_status_reset();

	/* setup tIFS switching */
	radio_tmr_tifs_set(EVENT_IFS_US);
	radio_switch_complete_and_tx(phy_p, 0, phy_p, 0);

	radio_pkt_rx_set(radio_pkt_scratch_get());
	/* assert if radio packet ptr is not set and radio started rx */
	LL_ASSERT(!radio_is_ready());

	if (IS_ENABLED(CONFIG_BT_CTLR_PROFILE_ISR)) {
		lll_prof_cputime_capture();
	}

	radio_isr_set(isr_rx, param);

#if defined(CONFIG_BT_CTLR_PRIVACY)
	if (ull_filter_lll_rl_enabled()) {
		uint8_t count, *irks = ull_filter_lll_irks_get(&count);

		radio_ar_configure(count, irks, 0);
	}
#endif /* CONFIG_BT_CTLR_PRIVACY */

	/* +/- 2us active clock jitter, +1 us hcto compensation */
	hcto = radio_tmr_tifs_base_get() + EVENT_IFS_US + 4 + 1;
	hcto += radio_rx_chain_delay_get(phy_p, 0);
	hcto += addr_us_get(phy_p);
	hcto -= radio_tx_chain_delay_get(phy_p, 0);
	radio_tmr_hcto_configure(hcto);

	/* capture end of CONNECT_IND PDU, used for calculating first
	 * slave event.
	 */
	radio_tmr_end_capture();

	if (IS_ENABLED(CONFIG_BT_CTLR_SCAN_REQ_RSSI) ||
	    IS_ENABLED(CONFIG_BT_CTLR_CONN_RSSI)) {
		radio_rssi_measure();
	}

#if defined(CONFIG_BT_CTLR_GPIO_LNA_PIN)
	if (IS_ENABLED(CONFIG_BT_CTLR_PROFILE_ISR)) {
		/* PA/LNA enable is overwriting packet end used in ISR
		 * profiling, hence back it up for later use.
		 */
		lll_prof_radio_end_backup();
	}

	radio_gpio_lna_setup();
	radio_gpio_pa_lna_enable(radio_tmr_tifs_base_get() + EVENT_IFS_US - 4 -
				 radio_tx_chain_delay_get(phy_p, 0) -
				 CONFIG_BT_CTLR_GPIO_LNA_OFFSET);
#endif /* CONFIG_BT_CTLR_GPIO_LNA_PIN */

	if (IS_ENABLED(CONFIG_BT_CTLR_PROFILE_ISR)) {
		/* NOTE: as scratch packet is used to receive, it is safe to
		 * generate profile event using rx nodes.
		 */
		lll_prof_send();
	}
}

static void isr_rx(void *param)
{
	uint8_t devmatch_ok;
	uint8_t devmatch_id;
	uint8_t irkmatch_ok;
	uint8_t irkmatch_id;
	uint8_t rssi_ready;
	uint8_t trx_done;
	uint8_t crc_ok;

	if (IS_ENABLED(CONFIG_BT_CTLR_PROFILE_ISR)) {
		lll_prof_latency_capture();
	}

	/* Read radio status and events */
	trx_done = radio_is_done();
	if (trx_done) {
		crc_ok = radio_crc_is_valid();
		devmatch_ok = radio_filter_has_match();
		devmatch_id = radio_filter_match_get();
		irkmatch_ok = radio_ar_has_match();
		irkmatch_id = radio_ar_match_get();
		rssi_ready = radio_rssi_is_ready();
	} else {
		crc_ok = devmatch_ok = irkmatch_ok = rssi_ready = 0U;
		devmatch_id = irkmatch_id = 0xFF;
	}

	/* Clear radio status and events */
	lll_isr_status_reset();

	/* No Rx */
	if (!trx_done) {
		goto isr_rx_do_close;
	}

	if (crc_ok) {
		int err;

		err = isr_rx_pdu(param, devmatch_ok, devmatch_id, irkmatch_ok,
				 irkmatch_id, rssi_ready);
		if (!err) {
			if (IS_ENABLED(CONFIG_BT_CTLR_PROFILE_ISR)) {
				lll_prof_send();
			}

			return;
		}
	}

isr_rx_do_close:
	radio_isr_set(isr_done, param);
	radio_disable();
}

static void isr_done(void *param)
{
	struct lll_adv *lll;

	/* Clear radio status and events */
	lll_isr_status_reset();

#if defined(CONFIG_BT_HCI_MESH_EXT)
	if (_radio.advertiser.is_mesh &&
	    !_radio.mesh_adv_end_us) {
		_radio.mesh_adv_end_us = radio_tmr_end_get();
	}
#endif /* CONFIG_BT_HCI_MESH_EXT */

	lll = param;

#if defined(CONFIG_BT_PERIPHERAL)
	if (!IS_ENABLED(CONFIG_BT_CTLR_LOW_LAT) && lll->is_hdcd &&
	    !lll->chan_map_curr) {
		lll->chan_map_curr = lll->chan_map;
	}
#endif /* CONFIG_BT_PERIPHERAL */

	if (lll->chan_map_curr) {
		struct pdu_adv *pdu;
		uint32_t start_us;

		pdu = chan_prepare(lll);

#if defined(CONFIG_BT_CTLR_GPIO_PA_PIN) || defined(CONFIG_BT_CTLR_ADV_EXT)
		start_us = radio_tmr_start_now(1);

#if defined(CONFIG_BT_CTLR_ADV_EXT)
		if (lll->aux) {
			ull_adv_aux_lll_offset_fill(lll->aux->ticks_offset,
						    start_us, pdu);
		}
#else /* !CONFIG_BT_CTLR_ADV_EXT */
		ARG_UNUSED(pdu);
#endif /* !CONFIG_BT_CTLR_ADV_EXT */

#if defined(CONFIG_BT_CTLR_GPIO_PA_PIN)
		radio_gpio_pa_setup();
		radio_gpio_pa_lna_enable(start_us +
					 radio_tx_ready_delay_get(0, 0) -
					 CONFIG_BT_CTLR_GPIO_PA_OFFSET);
#endif /* CONFIG_BT_CTLR_GPIO_PA_PIN */
#else /* !(CONFIG_BT_CTLR_GPIO_PA_PIN || defined(CONFIG_BT_CTLR_ADV_EXT)) */
		ARG_UNUSED(start_us);

		radio_tx_enable();
#endif /* !(CONFIG_BT_CTLR_GPIO_PA_PIN || defined(CONFIG_BT_CTLR_ADV_EXT)) */

		/* capture end of Tx-ed PDU, used to calculate HCTO. */
		radio_tmr_end_capture();

		return;

#if defined(CONFIG_BT_CTLR_ADV_EXT) && defined(BT_CTLR_ADV_EXT_PBACK)
	} else {
		struct pdu_adv_com_ext_adv *p;
		struct pdu_adv_ext_hdr *h;
		struct pdu_adv *pdu;

		pdu = lll_adv_data_curr_get(lll);
		p = (void *)&pdu->adv_ext_ind;
		h = (void *)p->ext_hdr_adv_data;

		if ((pdu->type == PDU_ADV_TYPE_EXT_IND) && h->aux_ptr) {
			radio_filter_disable();

			lll_adv_aux_pback_prepare(lll);

			return;
		}
#endif /* CONFIG_BT_CTLR_ADV_EXT */
	}

	radio_filter_disable();

#if defined(CONFIG_BT_PERIPHERAL)
	if (!lll->is_hdcd)
#endif /* CONFIG_BT_PERIPHERAL */
	{
#if defined(CONFIG_BT_HCI_MESH_EXT)
		if (_radio.advertiser.is_mesh) {
			uint32_t err;

			err = isr_close_adv_mesh();
			if (err) {
				return 0;
			}
		}
#endif /* CONFIG_BT_HCI_MESH_EXT */
	}

#if defined(CONFIG_BT_CTLR_ADV_INDICATION)
	struct node_rx_hdr *node_rx = ull_pdu_rx_alloc_peek(3);

	if (node_rx) {
		ull_pdu_rx_alloc();

		/* TODO: add other info by defining a payload struct */
		node_rx->type = NODE_RX_TYPE_ADV_INDICATION;

		ull_rx_put(node_rx->link, node_rx);
		ull_rx_sched();
	}
#endif /* CONFIG_BT_CTLR_ADV_INDICATION */

#if defined(CONFIG_BT_CTLR_ADV_EXT)
	struct event_done_extra *extra;

	extra = ull_event_done_extra_get();
	LL_ASSERT(extra);

	extra->type = EVENT_DONE_EXTRA_TYPE_ADV;
#endif  /* CONFIG_BT_CTLR_ADV_EXT */

	lll_isr_cleanup(param);
}

static void isr_abort(void *param)
{
	/* Clear radio status and events */
	lll_isr_status_reset();

	radio_filter_disable();

	lll_isr_cleanup(param);
}

static struct pdu_adv *chan_prepare(struct lll_adv *lll)
{
	struct pdu_adv *pdu;
	uint8_t chan;
	uint8_t upd;

	chan = find_lsb_set(lll->chan_map_curr);
	LL_ASSERT(chan);

	lll->chan_map_curr &= (lll->chan_map_curr - 1);

	lll_chan_set(36 + chan);

	/* FIXME: get latest only when primary PDU without Aux PDUs */
	upd = 0U;
	pdu = lll_adv_data_latest_get(lll, &upd);

	radio_pkt_tx_set(pdu);

	if ((pdu->type != PDU_ADV_TYPE_NONCONN_IND) &&
	    (!IS_ENABLED(CONFIG_BT_CTLR_ADV_EXT) ||
	     (pdu->type != PDU_ADV_TYPE_EXT_IND))) {
		struct pdu_adv *scan_pdu;

		scan_pdu = lll_adv_scan_rsp_latest_get(lll, &upd);

#if defined(CONFIG_BT_CTLR_PRIVACY)
		if (upd) {
			/* Copy the address from the adv packet we will send
			 * into the scan response.
			 */
			memcpy(&scan_pdu->scan_rsp.addr[0],
			       &pdu->adv_ind.addr[0], BDADDR_SIZE);
		}
#else
		ARG_UNUSED(scan_pdu);
		ARG_UNUSED(upd);
#endif /* !CONFIG_BT_CTLR_PRIVACY */

		radio_isr_set(isr_tx, lll);
		radio_tmr_tifs_set(EVENT_IFS_US);
		radio_switch_complete_and_rx(0);
	} else {
		radio_isr_set(isr_done, lll);
		radio_switch_complete_and_disable();
	}

	return pdu;
}

static inline int isr_rx_pdu(struct lll_adv *lll,
			     uint8_t devmatch_ok, uint8_t devmatch_id,
			     uint8_t irkmatch_ok, uint8_t irkmatch_id,
			     uint8_t rssi_ready)
{
	struct pdu_adv *pdu_adv;
	struct pdu_adv *pdu_rx;
	uint8_t tx_addr;
	uint8_t *addr;
	uint8_t rx_addr;
	uint8_t *tgt_addr;

#if defined(CONFIG_BT_CTLR_PRIVACY)
	/* An IRK match implies address resolution enabled */
	uint8_t rl_idx = irkmatch_ok ? ull_filter_lll_rl_irk_idx(irkmatch_id) :
				    FILTER_IDX_NONE;
#else
	uint8_t rl_idx = FILTER_IDX_NONE;
#endif /* CONFIG_BT_CTLR_PRIVACY */

	pdu_rx = (void *)radio_pkt_scratch_get();
	pdu_adv = lll_adv_data_curr_get(lll);

	addr = pdu_adv->adv_ind.addr;
	tx_addr = pdu_adv->tx_addr;

	if (pdu_adv->type == PDU_ADV_TYPE_DIRECT_IND) {
		tgt_addr = pdu_adv->direct_ind.tgt_addr;
	} else {
		tgt_addr = NULL;
	}
	rx_addr = pdu_adv->rx_addr;

	if ((pdu_rx->type == PDU_ADV_TYPE_SCAN_REQ) &&
	    (pdu_rx->len == sizeof(struct pdu_adv_scan_req)) &&
	    (tgt_addr == NULL) &&
	    lll_adv_scan_req_check(lll, pdu_rx, tx_addr, addr, devmatch_ok,
				    &rl_idx)) {
		radio_isr_set(isr_done, lll);
		radio_switch_complete_and_disable();
		radio_pkt_tx_set(lll_adv_scan_rsp_curr_get(lll));

		/* assert if radio packet ptr is not set and radio started tx */
		LL_ASSERT(!radio_is_ready());

		if (IS_ENABLED(CONFIG_BT_CTLR_PROFILE_ISR)) {
			lll_prof_cputime_capture();
		}

#if defined(CONFIG_BT_CTLR_SCAN_REQ_NOTIFY)
		if (!IS_ENABLED(CONFIG_BT_CTLR_ADV_EXT) ||
		    lll->scan_req_notify) {
			uint32_t err;

			/* Generate the scan request event */
			err = lll_adv_scan_req_report(lll, pdu_rx, rl_idx,
						      rssi_ready);
			if (err) {
				/* Scan Response will not be transmitted */
				return err;
			}
		}
#endif /* CONFIG_BT_CTLR_SCAN_REQ_NOTIFY */

#if defined(CONFIG_BT_CTLR_GPIO_PA_PIN)
		if (IS_ENABLED(CONFIG_BT_CTLR_PROFILE_ISR)) {
			/* PA/LNA enable is overwriting packet end used in ISR
			 * profiling, hence back it up for later use.
			 */
			lll_prof_radio_end_backup();
		}

		radio_gpio_pa_setup();
		radio_gpio_pa_lna_enable(radio_tmr_tifs_base_get() +
					 EVENT_IFS_US -
					 radio_rx_chain_delay_get(0, 0) -
					 CONFIG_BT_CTLR_GPIO_PA_OFFSET);
#endif /* CONFIG_BT_CTLR_GPIO_PA_PIN */
		return 0;

#if defined(CONFIG_BT_PERIPHERAL)
	} else if ((pdu_rx->type == PDU_ADV_TYPE_CONNECT_IND) &&
		   (pdu_rx->len == sizeof(struct pdu_adv_connect_ind)) &&
		   lll_adv_connect_ind_check(lll, pdu_rx, tx_addr, addr,
					     rx_addr, tgt_addr,
					     devmatch_ok, &rl_idx) &&
		   lll->conn) {
		struct node_rx_ftr *ftr;
		struct node_rx_pdu *rx;
		int ret;

		if (IS_ENABLED(CONFIG_BT_CTLR_CHAN_SEL_2)) {
			rx = ull_pdu_rx_alloc_peek(4);
		} else {
			rx = ull_pdu_rx_alloc_peek(3);
		}

		if (!rx) {
			return -ENOBUFS;
		}

		radio_isr_set(isr_abort, lll);
		radio_disable();

		/* assert if radio started tx */
		LL_ASSERT(!radio_is_ready());

		if (IS_ENABLED(CONFIG_BT_CTLR_PROFILE_ISR)) {
			lll_prof_cputime_capture();
		}

#if defined(CONFIG_BT_CTLR_CONN_RSSI)
		if (rssi_ready) {
			lll->conn->rssi_latest =  radio_rssi_get();
		}
#endif /* CONFIG_BT_CTLR_CONN_RSSI */

		/* Stop further LLL radio events */
		ret = lll_stop(lll);
		LL_ASSERT(!ret);

		rx = ull_pdu_rx_alloc();

		rx->hdr.type = NODE_RX_TYPE_CONNECTION;
		rx->hdr.handle = 0xffff;

		memcpy(rx->pdu, pdu_rx, (offsetof(struct pdu_adv, connect_ind) +
					 sizeof(struct pdu_adv_connect_ind)));

		ftr = &(rx->hdr.rx_ftr);
		ftr->param = lll;
		ftr->ticks_anchor = radio_tmr_start_get();
		ftr->radio_end_us = radio_tmr_end_get() -
				    radio_tx_chain_delay_get(0, 0);

#if defined(CONFIG_BT_CTLR_PRIVACY)
		ftr->rl_idx = irkmatch_ok ? rl_idx : FILTER_IDX_NONE;
#endif /* CONFIG_BT_CTLR_PRIVACY */

		if (IS_ENABLED(CONFIG_BT_CTLR_CHAN_SEL_2)) {
			ftr->extra = ull_pdu_rx_alloc();
		}

		ull_rx_put(rx->hdr.link, rx);
		ull_rx_sched();

		return 0;
#endif /* CONFIG_BT_PERIPHERAL */
	}

	return -EINVAL;
}

static bool isr_rx_sr_adva_check(uint8_t tx_addr, uint8_t *addr,
				 struct pdu_adv *sr)
{
	return (tx_addr == sr->rx_addr) &&
		!memcmp(addr, sr->scan_req.adv_addr, BDADDR_SIZE);
}

static inline bool isr_rx_ci_tgta_check(struct lll_adv *lll,
					uint8_t rx_addr, uint8_t *tgt_addr,
					struct pdu_adv *ci, uint8_t rl_idx)
{
#if defined(CONFIG_BT_CTLR_PRIVACY)
	if (rl_idx != FILTER_IDX_NONE && lll->rl_idx != FILTER_IDX_NONE) {
		return rl_idx == lll->rl_idx;
	}
#endif /* CONFIG_BT_CTLR_PRIVACY */
	return (rx_addr == ci->tx_addr) &&
	       !memcmp(tgt_addr, ci->connect_ind.init_addr, BDADDR_SIZE);
}

static inline bool isr_rx_ci_adva_check(uint8_t tx_addr, uint8_t *addr,
					struct pdu_adv *ci)
{
	return (tx_addr == ci->rx_addr) &&
		!memcmp(addr, ci->connect_ind.adv_addr, BDADDR_SIZE);
}
