/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdint.h>
#include <zephyr.h>
#include <soc.h>
#include <sys/util.h>
#include <bluetooth/hci.h>

#include "hal/cpu.h"
#include "hal/ccm.h"

#include "util/util.h"
#include "util/mem.h"
#include "util/memq.h"
#include "util/mfifo.h"
#include "util/dbuf.h"

#include "pdu.h"

#include "lll.h"
#include "lll/lll_adv_types.h"
#include "lll_adv.h"
#include "lll/lll_adv_pdu.h"
#include "lll_scan.h"
#include "lll/lll_df_types.h"
#include "lll_sync.h"
#include "lll_conn.h"
#include "lll_df.h"
#include "lll/lll_df_internal.h"

#include "ull_scan_types.h"
#include "ull_sync_types.h"
#include "ull_sync_internal.h"
#include "ull_adv_types.h"
#include "ull_tx_queue.h"
#include "ull_conn_types.h"
#include "ull_conn_internal.h"
#include "ull_df_types.h"
#include "ull_df_internal.h"
#include "ull_llcp.h"

#include "ull_adv_internal.h"
#include "ull_internal.h"

#include "ll.h"

#define BT_DBG_ENABLED IS_ENABLED(CONFIG_BT_CTLR_DF_DEBUG_ENABLE)
#define LOG_MODULE_NAME bt_ctlr_ull_df
#include "common/log.h"
#include "hal/debug.h"

#if defined(CONFIG_BT_CTLR_DF_SCAN_CTE_RX) || defined(CONFIG_BT_CTLR_DF_CONN_CTE_RX)

#define CTE_LEN_MAX_US 160U

#define IQ_REPORT_HEADER_SIZE      (offsetof(struct node_rx_iq_report, pdu))
#define IQ_REPORT_STRUCT_OVERHEAD  (IQ_REPORT_HEADER_SIZE)
#define IQ_SAMPLE_SIZE (sizeof(struct iq_sample))

#define IQ_REPORT_RX_NODE_POOL_ELEMENT_SIZE              \
	MROUND(IQ_REPORT_STRUCT_OVERHEAD + (IQ_SAMPLE_TOTAL_CNT * IQ_SAMPLE_SIZE))
#define IQ_REPORT_POOL_SIZE (IQ_REPORT_RX_NODE_POOL_ELEMENT_SIZE * IQ_REPORT_CNT)

/* Memory pool to store IQ reports data */
static struct {
	void *free;
	uint8_t pool[IQ_REPORT_POOL_SIZE];
} mem_iq_report;

/* FIFO to store free IQ report norde_rx objects. */
static MFIFO_DEFINE(iq_report_free, sizeof(void *), IQ_REPORT_CNT);

/* Number of available instance of linked list to be used for node_rx_iq_reports. */
static uint8_t mem_link_iq_report_quota_pdu;
#endif /* CONFIG_BT_CTLR_DF_SCAN_CTE_RX || CONFIG_BT_CTLR_DF_CONN_CTE_RX*/

/* ToDo:
 * - Add release of df_adv_cfg when adv_sync is released.
 *   Open question, should df_adv_cfg be released when Adv. CTE is disabled?
 *   If yes that would mean, end user must always run ll_df_set_cl_cte_tx_params
 *   before consecutive Adv CTE enable.
 */

#if defined(CONFIG_BT_CTLR_DF_ADV_CTE_TX)
static struct lll_df_adv_cfg lll_df_adv_cfg_pool[CONFIG_BT_CTLR_ADV_AUX_SET];
static void *df_adv_cfg_free;
static uint8_t cte_info_clear(struct ll_adv_set *adv, struct lll_df_adv_cfg *df_cfg,
			      uint8_t *ter_idx, struct pdu_adv **first_pdu);
#endif /* CONFIG_BT_CTLR_DF_ADV_CTE_TX */

/* @brief Function performs common steps for initialization and reset
 * of Direction Finding ULL module.
 *
 * @return      Zero in case of success, other value in case of failure.
 */
static int init_reset(void);

#if defined(CONFIG_BT_CTLR_DF_ADV_CTE_TX)
/* @brief Function acquires memory for DF advertising configuration.
 *
 * The memory is acquired from private @ref lll_df_adv_cfg_pool memory store.
 *
 * @return Pointer to lll_df_adv_cfg or NULL if there is no more free memory.
 */
static struct lll_df_adv_cfg *df_adv_cfg_acquire(void);

static uint8_t cte_info_set(struct ll_adv_set *adv, struct lll_df_adv_cfg *df_cfg, uint8_t *ter_idx,
			    struct pdu_adv **first_pdu);
#endif /* CONFIG_BT_CTLR_DF_ADV_CTE_TX */

/* @brief Function performs ULL Direction Finding initialization
 *
 * @return      Zero in case of success, other value in case of failure.
 */
int ull_df_init(void)
{
	int err;

	err = init_reset();
	if (err) {
		return err;
	}

	return 0;
}

/* @brief Function performs ULL Direction Finding reset
 *
 * @return      Zero in case of success, other value in case of failure.
 */
int ull_df_reset(void)
{
	int err;

#if defined(CONFIG_BT_CTLR_DF_ADV_CTE_TX)
	struct ll_adv_set *adv;
	uint8_t handle;

	/* Get the advertising set instance */
	for (handle = 0U; handle < BT_CTLR_ADV_SET; handle++) {
		adv = ull_adv_is_created_get(handle);
		if (!adv) {
			continue;
		}

		adv->df_cfg = NULL;
	}
#endif /* CONFIG_BT_CTLR_DF_ADV_CTE_TX */

	err = init_reset();
	if (err) {
		return err;
	}

	return 0;
}

static int init_reset(void)
{
#if defined(CONFIG_BT_CTLR_DF_ADV_CTE_TX)
	/* Initialize advertising DF memory configuration pool. */
	mem_init(lll_df_adv_cfg_pool, sizeof(struct lll_df_adv_cfg),
		 sizeof(lll_df_adv_cfg_pool) / sizeof(struct lll_df_adv_cfg),
		 &df_adv_cfg_free);
#endif /* CONFIG_BT_CTLR_DF_ADV_CTE_TX */

#if defined(CONFIG_BT_CTLR_DF_SCAN_CTE_RX) || defined(CONFIG_BT_CTLR_DF_CONN_CTE_RX)
	/* Re-initialize the free IQ report mfifo */
	MFIFO_INIT(iq_report_free);

	/* Initialize IQ report memory pool. */
	mem_init(mem_iq_report.pool, (IQ_REPORT_RX_NODE_POOL_ELEMENT_SIZE),
		 sizeof(mem_iq_report.pool) / (IQ_REPORT_RX_NODE_POOL_ELEMENT_SIZE),
		 &mem_iq_report.free);

	/* Allocate free IQ report node rx */
	mem_link_iq_report_quota_pdu = IQ_REPORT_CNT;
	ull_df_rx_iq_report_alloc(UINT8_MAX);
#endif /* CONFIG_BT_CTLR_DF_SCAN_CTE_RX || CONFIG_BT_CTLR_DF_CONN_CTE_RX */
	return 0;
}

#if defined(CONFIG_BT_CTLR_DF_ADV_CTE_TX)
/* @brief Function sets CTE transmission parameters for periodic advertising.
 *
 * @param[in]adv_handle                 Handle of advertising set.
 * @param[in]cte_len                    Length of CTE in 8us units.
 * @param[in]cte_type                   Type of CTE to be used for transmission.
 * @param[in]cte_count                  Number of CTE that should be transmitted
 *                                      during each periodic advertising
 *                                      interval.
 * @param[in]num_ant_ids                Number of antenna IDs in switching
 *                                      pattern. May be zero if CTE type is
 *                                      AoA.
 * @param[in]ant_ids                    Array of antenna IDs in a switching
 *                                      pattern. May be NULL if CTE type is AoA.
 *
 * @return Status of command completion.
 */
uint8_t ll_df_set_cl_cte_tx_params(uint8_t adv_handle, uint8_t cte_len,
				   uint8_t cte_type, uint8_t cte_count,
				   uint8_t num_ant_ids, uint8_t *ant_ids)
{
	struct lll_df_adv_cfg *cfg;
	struct ll_adv_set *adv;

	/* Get the advertising set instance */
	adv = ull_adv_is_created_get(adv_handle);
	if (!adv) {
		return BT_HCI_ERR_UNKNOWN_ADV_IDENTIFIER;
	}

	if (cte_len < BT_HCI_LE_CTE_LEN_MIN ||
	    cte_len > BT_HCI_LE_CTE_LEN_MAX) {
		return BT_HCI_ERR_UNSUPP_FEATURE_PARAM_VAL;
	}

	/* ToDo: Check if there is a limit of per. adv. pdu that may be
	 * sent. This affects number of CTE that may be requested.
	 */
	if (cte_count < BT_HCI_LE_CTE_COUNT_MIN ||
	    cte_count > BT_HCI_LE_CTE_COUNT_MAX) {
		return BT_HCI_ERR_UNSUPP_FEATURE_PARAM_VAL;
	}

	if (!(IS_ENABLED(CONFIG_BT_CTLR_DF_ADV_CTE_TX) &&
	      ((cte_type == BT_HCI_LE_AOA_CTE) ||
		(IS_ENABLED(CONFIG_BT_CTLR_DF_ANT_SWITCH_TX) &&
		 ((cte_type == BT_HCI_LE_AOD_CTE_2US) ||
		  (IS_ENABLED(CONFIG_BT_CTLR_DF_ANT_SWITCH_1US) &&
		   cte_type == BT_HCI_LE_AOD_CTE_1US)))))) {
		return BT_HCI_ERR_UNSUPP_FEATURE_PARAM_VAL;
	}

	if ((cte_type == BT_HCI_LE_AOD_CTE_1US || cte_type == BT_HCI_LE_AOD_CTE_2US) &&
	    (num_ant_ids < BT_HCI_LE_CTE_LEN_MIN ||
	     num_ant_ids > BT_CTLR_DF_MAX_ANT_SW_PATTERN_LEN || !ant_ids)) {
		return BT_HCI_ERR_UNSUPP_FEATURE_PARAM_VAL;
	}

	if (!adv->df_cfg) {
		adv->df_cfg = df_adv_cfg_acquire();
	}

	cfg = adv->df_cfg;

	if (cfg->is_enabled) {
		return BT_HCI_ERR_CMD_DISALLOWED;
	}

	cfg->cte_count = cte_count;
	cfg->cte_length = cte_len;
	cfg->cte_type = cte_type;

	if (cte_type == BT_HCI_LE_AOD_CTE_1US ||
	    cte_type == BT_HCI_LE_AOD_CTE_2US) {
		/* Note:
		 * Are we going to check antenna identifiers if they are valid?
		 * BT 5.2 Core spec. Vol. 4 Part E Section 7.8.80 says
		 * that not all controller may be able to do that.
		 */
		memcpy(cfg->ant_ids, ant_ids, num_ant_ids);
		cfg->ant_sw_len = num_ant_ids;
	} else {
		cfg->ant_sw_len = 0;
	}

	return BT_HCI_ERR_SUCCESS;
}

/* @brief Function enables or disables CTE TX for periodic advertising.
 *
 * @param[in] handle                    Advertising set handle.
 * @param[in] cte_enable                Enable or disable CTE TX
 *
 * @return Status of command completion.
 */
uint8_t ll_df_set_cl_cte_tx_enable(uint8_t adv_handle, uint8_t cte_enable)
{
	struct lll_adv_sync *lll_sync;
	struct lll_df_adv_cfg *df_cfg;
	struct ll_adv_sync_set *sync;
	struct ll_adv_set *adv;
	uint8_t err, ter_idx;
	struct pdu_adv *pdu;

	/* Get the advertising set instance */
	adv = ull_adv_is_created_get(adv_handle);
	if (!adv) {
		return BT_HCI_ERR_UNKNOWN_ADV_IDENTIFIER;
	}

	lll_sync = adv->lll.sync;
	/* If there is no sync in advertising set, then the HCI_LE_Set_-
	 * Periodic_Advertising_Parameters command was not issued before.
	 */
	if (!lll_sync) {
		return BT_HCI_ERR_CMD_DISALLOWED;
	}

	sync = HDR_LLL2ULL(lll_sync);

	/* If df_cfg is NULL, then the HCI_LE_Set_Connectionless_CTE_Transmit_-
	 * Parameters command was not issued before.
	 */
	df_cfg = adv->df_cfg;
	if (!df_cfg) {
		return BT_HCI_ERR_CMD_DISALLOWED;
	}

	if (adv->lll.phy_s == PHY_CODED) {
		return BT_HCI_ERR_CMD_DISALLOWED;
	}

	if (!cte_enable) {
		if (!df_cfg->is_enabled) {
			return BT_HCI_ERR_CMD_DISALLOWED;
		}

		err = cte_info_clear(adv, df_cfg, &ter_idx, &pdu);
		if (err) {
			return err;
		}

		df_cfg->is_enabled = 0U;
	} else {
		if (df_cfg->is_enabled) {
			return BT_HCI_ERR_CMD_DISALLOWED;
		}

		err = cte_info_set(adv, df_cfg, &ter_idx, &pdu);
		if (err) {
			return err;
		}

		df_cfg->is_enabled = 1U;
	}

	if (sync->is_started) {
		err = ull_adv_sync_time_update(sync, pdu);
		if (err) {
			return err;
		}
	}

	lll_adv_sync_data_enqueue(adv->lll.sync, ter_idx);

	return BT_HCI_ERR_SUCCESS;
}
#endif /* CONFIG_BT_CTLR_DF_ADV_CTE_TX */

#if defined(CONFIG_BT_CTLR_DF_SCAN_CTE_RX)
/* @brief Function sets IQ sampling enabled or disabled.
 *
 * Set IQ sampling enable for received PDUs that has attached CTE.
 *
 * @param[in]handle                     Connection handle.
 * @param[in]sampling_enable            Enable or disable CTE RX
 * @param[in]slot_durations             Switching and samplig slot durations for
 *                                      AoA mode.
 * @param[in]max_cte_count              Maximum number of sampled CTEs in single
 *                                      periodic advertising event.
 * @param[in]switch_pattern_len         Number of antenna ids in switch pattern.
 * @param[in]ant_ids                    Array of antenna identifiers.
 *
 * @return Status of command completion.
 *
 * @Note This function may put TX thread into wait state. This may lead to a
 *       situation that ll_sync_set instnace is relased (RX thread has higher
 *       priority than TX thread). l_sync_set instance may not be accessed after
 *       call to ull_sync_slot_update.
 *       This is related with possible race condition with RX thread handling
 *       periodic sync lost event.
 */
uint8_t ll_df_set_cl_iq_sampling_enable(uint16_t handle,
					uint8_t sampling_enable,
					uint8_t slot_durations,
					uint8_t max_cte_count,
					uint8_t switch_pattern_len,
					uint8_t *ant_ids)
{
	struct lll_df_sync_cfg *cfg, *cfg_prev;
	uint32_t slot_minus_us = 0;
	uint32_t slot_plus_us = 0;
	struct ll_sync_set *sync;
	struct lll_sync *lll;
	uint8_t cfg_idx;

	/* After this call and before ull_sync_slot_update the function may not
	 * call any kernel API that may put the thread into wait state. It may
	 * cause race condition with RX thread and lead to use of released memory.
	 */
	sync = ull_sync_is_enabled_get(handle);
	if (!sync) {
		return BT_HCI_ERR_UNKNOWN_ADV_IDENTIFIER;
	}

	lll = &sync->lll;

	/* CTE is not supported for CODED Phy */
	if (lll->phy == PHY_CODED) {
		return BT_HCI_ERR_CMD_DISALLOWED;
	}

	cfg_prev = lll_df_sync_cfg_curr_get(&lll->df_cfg);
	cfg = lll_df_sync_cfg_alloc(&lll->df_cfg, &cfg_idx);

	if (!sampling_enable) {
		if (!cfg_prev->is_enabled) {
			/* Disable already disabled CTE Rx */
			return BT_HCI_ERR_SUCCESS;
		}
		slot_minus_us = CTE_LEN_MAX_US;
		cfg->is_enabled = 0U;
	} else {
		/* Enable of already enabled CTE updates AoA configuration */
		if (!((IS_ENABLED(CONFIG_BT_CTLR_DF_ANT_SWITCH_1US) &&
		       slot_durations == BT_HCI_LE_ANTENNA_SWITCHING_SLOT_1US) ||
		      slot_durations == BT_HCI_LE_ANTENNA_SWITCHING_SLOT_2US)) {
			return BT_HCI_ERR_UNSUPP_FEATURE_PARAM_VAL;
		}

		/* max_cte_count equal to 0x0 has special meaning - sample and
		 * report continuously until there are CTEs received.
		 */
		if (max_cte_count > BT_HCI_LE_SAMPLE_CTE_COUNT_MAX) {
			return BT_HCI_ERR_UNSUPP_FEATURE_PARAM_VAL;
		}

		if (switch_pattern_len < BT_HCI_LE_SWITCH_PATTERN_LEN_MIN ||
		    switch_pattern_len > BT_CTLR_DF_MAX_ANT_SW_PATTERN_LEN ||
		    !ant_ids) {
			return BT_HCI_ERR_UNSUPP_FEATURE_PARAM_VAL;
		}

		cfg->slot_durations = slot_durations;
		cfg->max_cte_count = max_cte_count;
		memcpy(cfg->ant_ids, ant_ids, switch_pattern_len);
		cfg->ant_sw_len = switch_pattern_len;

		cfg->is_enabled = 1U;

		if (!cfg_prev->is_enabled) {
			/* Extend sync event by maximum CTE duration.
			 * CTE duration denepnds on transmitter configuration
			 * so it is unknown for receiver upfront.
			 */
			slot_plus_us = BT_HCI_LE_CTE_LEN_MAX;
		}
	}

	lll_df_sync_cfg_enqueue(&lll->df_cfg, cfg_idx);

	if (slot_plus_us || slot_minus_us) {
		int err;
		/* Update of sync slot may fail due to race condition.
		 * If periodic sync is lost, the ticker event will be stopped.
		 * The stop operation may preempt call to this functions.
		 * So update may be called after that. Accept this failure
		 * (-ENOENT) gracefully.
		 * Periodic sync lost event also disables the CTE sampling.
		 */
		err = ull_sync_slot_update(sync, 0, CTE_LEN_MAX_US);
		LL_ASSERT(err == 0 || err == -ENOENT);
	}

	return 0;
}

void ull_df_sync_cfg_init(struct lll_df_sync *df_cfg)
{
	(void)memset(&df_cfg->cfg, 0, sizeof(df_cfg->cfg));
	df_cfg->first = 0U;
	df_cfg->last = 0U;
}

bool ull_df_sync_cfg_is_not_enabled(struct lll_df_sync *df_cfg)
{
	struct lll_df_sync_cfg *cfg;

	/* If new CTE sampling configuration was enqueued, get reference to
	 * latest congiruation without swapping buffers. Buffer should be
	 * swapped only at the beginning of the radio event.
	 *
	 * We may not get here if CTE sampling is not enabled in current
	 * configuration.
	 */
	if (lll_df_sync_cfg_is_modified(df_cfg)) {
		cfg = lll_df_sync_cfg_peek(df_cfg);
	} else {
		cfg = lll_df_sync_cfg_curr_get(df_cfg);
	}

	return !cfg->is_enabled;
}
#endif /* CONFIG_BT_CTLR_DF_SCAN_CTE_RX */

#if defined(CONFIG_BT_CTLR_DF_SCAN_CTE_RX) || defined(CONFIG_BT_CTLR_DF_CONN_CTE_RX)
void *ull_df_iq_report_alloc_peek(uint8_t count)
{
	if (count > MFIFO_AVAIL_COUNT_GET(iq_report_free)) {
		return NULL;
	}

	return MFIFO_DEQUEUE_PEEK(iq_report_free);
}

void *ull_df_iq_report_alloc_peek_iter(uint8_t *idx)
{
	return *(void **)MFIFO_DEQUEUE_ITER_GET(iq_report_free, idx);
}

void *ull_df_iq_report_alloc(void)
{
	return MFIFO_DEQUEUE(iq_report_free);
}

void ull_df_iq_report_mem_release(struct node_rx_hdr *rx)
{
	mem_release(rx, &mem_iq_report.free);
}

void ull_iq_report_link_inc_quota(int8_t delta)
{
	LL_ASSERT(delta <= 0 || mem_link_iq_report_quota_pdu < IQ_REPORT_CNT);
	mem_link_iq_report_quota_pdu += delta;
}

void ull_df_rx_iq_report_alloc(uint8_t max)
{
	uint8_t idx;

	if (max > mem_link_iq_report_quota_pdu) {
		max = mem_link_iq_report_quota_pdu;
	}

	while ((max--) && MFIFO_ENQUEUE_IDX_GET(iq_report_free, &idx)) {
		memq_link_t *link;
		struct node_rx_hdr *rx;

		link = ll_rx_link_alloc();
		if (!link) {
			return;
		}

		rx = mem_acquire(&mem_iq_report.free);
		if (!rx) {
			ll_rx_link_release(link);
			return;
		}

		rx->link = link;

		MFIFO_BY_IDX_ENQUEUE(iq_report_free, idx, rx);

		ull_iq_report_link_inc_quota(-1);
	}
}
#endif /* CONFIG_BT_CTLR_DF_SCAN_CTE_RX || CONFIG_BT_CTLR_DF_CONN_CTE_RX */

#if defined(CONFIG_BT_CTLR_DF_CONN_CTE_RX)
bool ull_df_conn_cfg_is_not_enabled(struct lll_df_conn_rx_cfg *rx_cfg)
{
	struct lll_df_conn_rx_params *rx_params;

	/* If new CTE sampling configuration was enqueued, get reference to
	 * latest congiruation without swapping buffers. Buffer should be
	 * swapped only at the beginning of the radio event.
	 *
	 * We may not get here if CTE sampling is not enabled in current
	 * configuration.
	 */
	if (dbuf_is_modified(&rx_cfg->hdr)) {
		rx_params = dbuf_peek(&rx_cfg->hdr);
	} else {
		rx_params = dbuf_curr_get(&rx_cfg->hdr);
	}

	return !rx_params->is_enabled;
}
#endif /* CONFIG_BT_CTLR_DF_CONN_CTE_RX */

#if defined(CONFIG_BT_CTLR_DF_ADV_CTE_TX)
/* @brief Function releases unused memory for DF advertising configuration.
 *
 * The memory is released to private @ref lll_df_adv_cfg_pool memory store.
 *
 * @param[in] df_adv_cfg        Pointer to lll_df_adv_cfg memory to be released.
 */
void ull_df_adv_cfg_release(struct lll_df_adv_cfg *df_adv_cfg)
{
	mem_release(df_adv_cfg, &df_adv_cfg_free);
}

static struct lll_df_adv_cfg *df_adv_cfg_acquire(void)
{
	struct lll_df_adv_cfg *df_adv_cfg;

	df_adv_cfg = mem_acquire(&df_adv_cfg_free);
	if (!df_adv_cfg) {
		return NULL;
	}

	df_adv_cfg->is_enabled = 0U;

	return df_adv_cfg;
}

#if (CONFIG_BT_CTLR_DF_PER_ADV_CTE_NUM_MAX > 1)
/*
 * @brief Function sets content of cte_info field in periodic advertising chain.
 *
 * The function allocates new PDU (or chain of PDUs) for periodic advertising to
 * fill it with information about CTE, that is going to be transmitted with the PDU.
 * If there is already allocated PDU, it will be updated to hold information about
 * CTE.
 *
 * @param lll_sync       Pointer to periodic advertising sync object.
 * @param pdu_prev       Pointer to a PDU that is already in use by LLL or was updated with new PDU
 *                       payload.
 * @param pdu            Pointer to a new head of periodic advertising chain. The pointer may have
 *                       the same value as @p pdu_prev, if payload of PDU pointerd by @p pdu_prev
 *                       was already updated.
 * @param cte_count      Number of CTEs that should be transmitted in periodic advertising chain.
 * @param cte_into       Pointer to instence of cte_info stuctuct that is added to PDUs extended
 *                       advertising header.
 *
 * @return Zero in case of success, other value in case of failure.
 */
static uint8_t per_adv_chain_cte_info_set(struct lll_adv_sync *lll_sync, struct pdu_adv *pdu_prev,
					  struct pdu_adv *pdu, uint8_t cte_count,
					  struct pdu_cte_info *cte_info)
{
	uint8_t pdu_add_field_flags;
	struct pdu_adv *pdu_next;
	uint8_t cte_index = 1;
	bool adi_in_sync_ind;
	bool new_chain;
	uint8_t err;

	new_chain = (pdu_prev == pdu ? false : true);

	pdu_add_field_flags = ULL_ADV_PDU_HDR_FIELD_CTE_INFO;

	if (IS_ENABLED(CONFIG_BT_CTLR_ADV_PERIODIC_ADI_SUPPORT)) {
		adi_in_sync_ind = ull_adv_sync_pdu_had_adi(pdu_prev);
	}

	pdu_prev = lll_adv_pdu_linked_next_get(pdu_prev);

	/* Update PDUs in existing chain. Add cte_info to extended advertising header. */
	while (pdu_prev) {
		if (new_chain) {
			pdu_next = lll_adv_pdu_alloc_pdu_adv();
			lll_adv_pdu_linked_append(pdu_next, pdu);
			pdu = pdu_next;
		} else {
			pdu = lll_adv_pdu_linked_next_get(pdu);
		}

		pdu_next = lll_adv_pdu_linked_next_get(pdu_prev);
		/* If all CTEs were added to chain, remove CTE from flags */
		if (cte_index >= cte_count) {
			pdu_add_field_flags = 0;
		} else {
			++cte_index;
			/* If it is last PDU in existing chain and there are CTE to be included
			 * add aux_ptr to flags.
			 */
			if (!pdu_next && cte_index < cte_count) {
				pdu_add_field_flags |= ULL_ADV_PDU_HDR_FIELD_AUX_PTR;
			}
		}

		if (IS_ENABLED(CONFIG_BT_CTLR_ADV_PERIODIC_ADI_SUPPORT) && adi_in_sync_ind) {
			pdu_add_field_flags |= ULL_ADV_PDU_HDR_FIELD_ADI;
		}

		err = ull_adv_sync_pdu_set_clear(lll_sync, pdu_prev, pdu, pdu_add_field_flags, 0,
						 cte_info);
		if (err != BT_HCI_ERR_SUCCESS) {
			/* TODO: implement gracefull error handling, cleanup of
			 * changed PDUs and notify host abut issue during start of CTE
			 * transmission.
			 */
			return err;
		}
		pdu_prev = pdu_next;
	}

	/* If there is missing only one CTE do not add aux_ptr to PDU */
	if (cte_count - cte_index >= 2) {
		pdu_add_field_flags |= ULL_ADV_PDU_HDR_FIELD_AUX_PTR;
	} else {
		pdu_add_field_flags = ULL_ADV_PDU_HDR_FIELD_CTE_INFO;
	}

	if (IS_ENABLED(CONFIG_BT_CTLR_ADV_PERIODIC_ADI_SUPPORT) && adi_in_sync_ind) {
		pdu_add_field_flags |= ULL_ADV_PDU_HDR_FIELD_ADI;
	}

	/* Add new PDUs if the number of PDUs in existing chain is lower than requested number
	 * of CTEs.
	 */
	while (cte_index < cte_count) {
		pdu_prev = pdu;
		pdu = lll_adv_pdu_alloc_pdu_adv();
		if (!pdu) {
			/* TODO: implement graceful error handling, cleanup of
			 * changed PDUs.
			 */
			return BT_HCI_ERR_MEM_CAPACITY_EXCEEDED;
		}
		ull_adv_sync_pdu_init(pdu, pdu_add_field_flags);
		ull_adv_sync_pdu_cte_info_set(pdu, cte_info);

		/* Link PDU into a chain */
		lll_adv_pdu_linked_append(pdu, pdu_prev);

		++cte_index;
		/* If next PDU in a chain is last PDU, then remove aux_ptr field flag from
		 * extended advertising header.
		 */
		if (cte_index == cte_count - 1) {
			pdu_add_field_flags &= (~ULL_ADV_PDU_HDR_FIELD_AUX_PTR);
		}
	}

	return BT_HCI_ERR_SUCCESS;
}
#endif /* CONFIG_BT_CTLR_DF_PER_ADV_CTE_NUM_MAX > 1 */

/*
 * @brief Function sets content of cte_info field for periodic advertising
 *
 * @param adv            Pointer to periodic advertising set.
 * @param df_cfg         Pointer to direction finding configuration
 * @param[out] ter_idx   Pointer used to return index of allocated or updated PDU.
 *                       Index is required for scheduling the PDU for transmission in LLL.
 * @param[out] first_pdu Pointer to return address of first PDU in a periodic advertising chain
 *
 * @return Zero in case of success, other value in case of failure.
 */
static uint8_t cte_info_set(struct ll_adv_set *adv, struct lll_df_adv_cfg *df_cfg, uint8_t *ter_idx,
			    struct pdu_adv **first_pdu)
{
	struct pdu_adv *pdu_prev, *pdu;
	struct lll_adv_sync *lll_sync;
	struct pdu_cte_info cte_info;
	uint8_t pdu_add_field_flags;
	void *extra_data;
	uint8_t err;

	lll_sync = adv->lll.sync;

	cte_info.type = df_cfg->cte_type;
	cte_info.time = df_cfg->cte_length;

	/* Note: ULL_ADV_PDU_EXTRA_DATA_ALLOC_ALWAYS is just information that extra_data
	 * is required in case of this ull_adv_sync_pdu_alloc.
	 */
	err = ull_adv_sync_pdu_alloc(adv, ULL_ADV_PDU_EXTRA_DATA_ALLOC_ALWAYS, &pdu_prev, &pdu,
				     NULL, &extra_data, ter_idx);
	if (err != BT_HCI_ERR_SUCCESS) {
		return err;
	}

	ull_adv_sync_extra_data_set_clear(NULL, extra_data, ULL_ADV_PDU_HDR_FIELD_CTE_INFO, 0,
					  df_cfg);

#if (CONFIG_BT_CTLR_DF_PER_ADV_CTE_NUM_MAX > 1)
	if (df_cfg->cte_count > 1) {
		pdu_add_field_flags =
			ULL_ADV_PDU_HDR_FIELD_CTE_INFO | ULL_ADV_PDU_HDR_FIELD_AUX_PTR;
	} else
#endif /* CONFIG_BT_CTLR_DF_PER_ADV_CTE_NUM_MAX > 1 */
	{
		pdu_add_field_flags = ULL_ADV_PDU_HDR_FIELD_CTE_INFO;
	}

	err = ull_adv_sync_pdu_set_clear(lll_sync, pdu_prev, pdu, pdu_add_field_flags, 0,
					 &cte_info);
	if (err != BT_HCI_ERR_SUCCESS) {
		return err;
	}

	*first_pdu = pdu;
#if (CONFIG_BT_CTLR_DF_PER_ADV_CTE_NUM_MAX > 1)
	err = per_adv_chain_cte_info_set(lll_sync, pdu_prev, pdu, df_cfg->cte_count, &cte_info);
	if (err != BT_HCI_ERR_SUCCESS) {
		return err;
	}
#endif /* CONFIG_BT_CTLR_DF_PER_ADV_CTE_NUM_MAX > 1 */

	return BT_HCI_ERR_SUCCESS;
}

#if (CONFIG_BT_CTLR_DF_PER_ADV_CTE_NUM_MAX > 1)
static bool pdu_ext_adv_is_empty_without_cte(const struct pdu_adv *pdu)
{
	if (pdu->len != PDU_AC_PAYLOAD_SIZE_MIN) {
		const struct pdu_adv_ext_hdr *ext_hdr;
		uint8_t size_rem = 0;

		if ((pdu->adv_ext_ind.ext_hdr_len + PDU_AC_EXT_HEADER_SIZE_MIN) != pdu->len) {
			/* There are adv. data in PDU */
			return false;
		}

		/* Check size of the ext. header without cte_info and aux_ptr. If that is minimum
		 * extended PDU size then the PDU was allocated to transport CTE only.
		 */
		ext_hdr = &pdu->adv_ext_ind.ext_hdr;

		if (ext_hdr->cte_info) {
			size_rem += sizeof(struct pdu_cte_info);
		}
		if (ext_hdr->aux_ptr) {
			size_rem += sizeof(struct pdu_adv_aux_ptr);
		}
		if (IS_ENABLED(CONFIG_BT_CTLR_ADV_PERIODIC_ADI_SUPPORT) && ext_hdr->adi) {
			size_rem += sizeof(struct pdu_adv_adi);
		}

		if ((pdu->adv_ext_ind.ext_hdr_len - size_rem) != PDU_AC_EXT_HEADER_SIZE_MIN) {
			return false;
		}
	}

	return true;
}

/*
 * @brief Function removes content of cte_info field in periodic advertising chain.
 *
 * The function removes cte_info from extended advertising header in all PDUs in periodic
 * advertising chain. If particular PDU is empty (holds cte_info only) it will be removed from
 * chain.
 *
 * @param[in] lll_sync     Pointer to periodic advertising sync object.
 * @param[in-out] pdu_prev Pointer to a PDU that is already in use by LLL or was updated with new
 *                         PDU payload. Points to last PDU in a previous chain after return.
 * @param[in-out] pdu      Pointer to a new head of periodic advertising chain. The pointer may have
 *                         the same value as @p pdu_prev, if payload of PDU pointerd by @p pdu_prev
 *                         was already updated. Points to last PDU in a new chain after return.
 *
 * @return Zero in case of success, other value in case of failure.
 */
static uint8_t rem_cte_info_from_per_adv_chain(struct lll_adv_sync *lll_sync,
					       struct pdu_adv **pdu_prev, struct pdu_adv **pdu)
{
	struct pdu_adv *pdu_new, *pdu_chained;
	uint8_t pdu_rem_field_flags;
	bool new_chain;
	uint8_t err;

	pdu_rem_field_flags = ULL_ADV_PDU_HDR_FIELD_CTE_INFO;

	/* It is possible that the function is called after e.g. advertising data were updated.
	 * In such situation the function will run on already allocated chain. Do not allocate
	 * new chain then. Reuse already allocated PDUs and allocate new ones only if the chain
	 * was not updated yet.
	 */
	new_chain = (*pdu_prev == *pdu ? false : true);

	/* Get next PDU in a chain. Alway use pdu_prev because it points to actual
	 * former chain.
	 */
	pdu_chained = lll_adv_pdu_linked_next_get(*pdu_prev);

	/* Go through existing chain and remove CTE info. */
	while (pdu_chained) {
		if (pdu_ext_adv_is_empty_without_cte(pdu_chained)) {
			/* If there is an empty PDU then all remaining PDUs shoudl be released. */
			if (!new_chain) {
				lll_adv_pdu_linked_release_all(pdu_chained);

				/* Set new end of chain in PDUs linked list. If pdu differs from
				 * prev_pdu then it is already end of a chain. If it doesn't differ,
				 * then chain end is changed in right place by use of pdu_prev.
				 * That makes sure there is no PDU released twice (here and when LLL
				 * swaps PDU buffers).
				 */
				lll_adv_pdu_linked_append(NULL, *pdu_prev);
			}
			pdu_chained = NULL;
		} else {
			/* Update one before pdu_chained */
			err = ull_adv_sync_pdu_set_clear(lll_sync, *pdu_prev, *pdu, 0,
							 pdu_rem_field_flags, NULL);
			if (err != BT_HCI_ERR_SUCCESS) {
				/* TODO: return here leaves periodic advertising chain in
				 * an inconsisten state. Add gracefull return or assert.
				 */
				return err;
			}

			/* Prepare for next iteration. Allocate new PDU or move to next one in
			 * a chain.
			 */
			if (new_chain) {
				pdu_new = lll_adv_pdu_alloc_pdu_adv();
				lll_adv_pdu_linked_append(pdu_new, *pdu);
				*pdu = pdu_new;
			} else {
				*pdu = lll_adv_pdu_linked_next_get(*pdu);
			}

			/* Move to next chained PDU (it moves through chain that is in use
			 * by LLL or is new one with updated advertising payload).
			 */
			*pdu_prev = pdu_chained;
			pdu_chained = lll_adv_pdu_linked_next_get(*pdu_prev);
		}
	}

	return BT_HCI_ERR_SUCCESS;
}
#endif /* (CONFIG_BT_CTLR_DF_PER_ADV_CTE_NUM_MAX > 1) */

/*
 * @brief Function removes content of cte_info field from periodic advertising PDUs.
 *
 * @param adv            Pointer to periodic advertising set.
 * @param df_cfg         Pointer to direction finding configuration
 * @param[out] ter_idx   Pointer used to return index of allocated or updated PDU.
 *                       Index is required for scheduling the PDU for transmission in LLL.
 * @param[out] first_pdu Pointer to return address of first PDU in a chain
 *
 * @return Zero in case of success, other value in case of failure.
 */
static uint8_t cte_info_clear(struct ll_adv_set *adv, struct lll_df_adv_cfg *df_cfg,
			      uint8_t *ter_idx, struct pdu_adv **first_pdu)
{
	void *extra_data_prev, *extra_data;
	struct pdu_adv *pdu_prev, *pdu;
	struct lll_adv_sync *lll_sync;
	uint8_t pdu_rem_field_flags;
	uint8_t err;

	lll_sync = adv->lll.sync;

	/* NOTE: ULL_ADV_PDU_EXTRA_DATA_ALLOC_NEVER is just information that extra_data
	 * should be removed in case of this call ull_adv_sync_pdu_alloc.
	 */
	err = ull_adv_sync_pdu_alloc(adv, ULL_ADV_PDU_EXTRA_DATA_ALLOC_NEVER, &pdu_prev, &pdu,
				     &extra_data_prev, &extra_data, ter_idx);
	if (err != BT_HCI_ERR_SUCCESS) {
		return err;
	}

	if (extra_data) {
		ull_adv_sync_extra_data_set_clear(extra_data_prev, extra_data, 0,
						  ULL_ADV_PDU_HDR_FIELD_CTE_INFO, NULL);
	}

	*first_pdu = pdu;

	pdu_rem_field_flags = ULL_ADV_PDU_HDR_FIELD_CTE_INFO;

#if (CONFIG_BT_CTLR_DF_PER_ADV_CTE_NUM_MAX > 1)
	err = rem_cte_info_from_per_adv_chain(lll_sync, &pdu_prev, &pdu);
	if (err != BT_HCI_ERR_SUCCESS) {
		return err;
	}

	/* Update last PDU in a chain. It may not have aux_ptr.
	 * NOTE: If there is no AuxPtr flag in the PDU, attempt to remove it does not harm.
	 */
	pdu_rem_field_flags |= ULL_ADV_PDU_HDR_FIELD_AUX_PTR;
#endif /* CONFIG_BT_CTLR_DF_PER_ADV_CTE_NUM_MAX > 1 */

	err = ull_adv_sync_pdu_set_clear(lll_sync, pdu_prev, pdu, 0, pdu_rem_field_flags, NULL);
	if (err != BT_HCI_ERR_SUCCESS) {
		/* TODO: return here leaves periodic advertising chain in an inconsisten state.
		 * Add gracefull return or assert.
		 */
		return err;
	}

	return BT_HCI_ERR_SUCCESS;
}
#endif /* CONFIG_BT_CTLR_DF_ADV_CTE_TX */

#if defined(CONFIG_BT_CTLR_DF_CONN_CTE_TX)
/* @brief Function sets CTE transmission parameters for a connection.
 *
 * @param handle             Connection handle.
 * @param cte_types          Bitfield holding information about
 *                           allowed CTE types.
 * @param switch_pattern_len Number of antenna ids in switch pattern.
 * @param ant_id             Array of antenna identifiers.
 *
 * @return Status of command completion.
 */
uint8_t ll_df_set_conn_cte_tx_params(uint16_t handle, uint8_t cte_types, uint8_t switch_pattern_len,
				     const uint8_t *ant_ids)
{
	struct lll_df_conn_tx_cfg *df_tx_cfg;
	struct ll_conn *conn;

	conn = ll_connected_get(handle);
	if (!conn) {
		return BT_HCI_ERR_UNKNOWN_CONN_ID;
	}

	df_tx_cfg = &conn->lll.df_tx_cfg;

	if (df_tx_cfg->cte_rsp_en) {
		return BT_HCI_ERR_CMD_DISALLOWED;
	}

	/* Bits other than representing AoA, AoD 1us, AoD 2us are RFU */
	if (cte_types == 0U ||
	    ((cte_types & (~(uint8_t)(BT_HCI_LE_AOA_CTE_RSP | BT_HCI_LE_AOD_CTE_RSP_1US |
				      BT_HCI_LE_AOD_CTE_RSP_2US))) != 0U)) {
		return BT_HCI_ERR_UNSUPP_FEATURE_PARAM_VAL;
	}

	if (!IS_ENABLED(CONFIG_BT_CTLR_DF_ANT_SWITCH_TX)) {
		if (cte_types & BT_HCI_LE_AOD_CTE_RSP_2US) {
			return BT_HCI_ERR_UNSUPP_FEATURE_PARAM_VAL;
		}

		if ((cte_types & BT_HCI_LE_AOD_CTE_RSP_1US) &&
		    !IS_ENABLED(CONFIG_BT_CTLR_DF_ANT_SWITCH_1US)) {
			return BT_HCI_ERR_UNSUPP_FEATURE_PARAM_VAL;
		}
	}

	/* Check antenna switching pattern only whether CTE TX in AoD mode is allowed */
	if (((cte_types & BT_HCI_LE_AOD_CTE_RSP_1US) || (cte_types & BT_HCI_LE_AOD_CTE_RSP_2US)) &&
	    (switch_pattern_len < BT_HCI_LE_SWITCH_PATTERN_LEN_MIN ||
	     switch_pattern_len > BT_CTLR_DF_MAX_ANT_SW_PATTERN_LEN || !ant_ids)) {
		return BT_HCI_ERR_UNSUPP_FEATURE_PARAM_VAL;
	}

	(void)memcpy(df_tx_cfg->ant_ids, ant_ids, switch_pattern_len);
	df_tx_cfg->ant_sw_len = switch_pattern_len;

	df_tx_cfg->cte_types_allowed = cte_types;
	df_tx_cfg->is_initialized = 1U;

	return BT_HCI_ERR_SUCCESS;
}
#endif /* CONFIG_BT_CTLR_DF_CONN_CTE_TX */

#if defined(CONFIG_BT_CTLR_DF_CONN_CTE_RX)
/**
 * @brief Function sets CTE reception parameters for a connection.
 *
 * @note: The CTE may not be send/received with PHY CODED. The BT Core 5.3 specification does not
 *        mention special handling of CTE receive and sampling while the functionality is enabled
 *        for a connection that currently uses PHY CODED. Enable of CTE receive for a PHY CODED
 *        will introduce coplications for TISF maintenance by software switch. To avoid that
 *        the lower link layer will enable the functionality when connection uses PHY UNCODED only.
 *
 * @param handle             Connection handle.
 * @param sampling_enable    Enable or disable CTE RX. When the parameter is set to false,
 *                           @p slot_durations, @p switch_pattern_len and @ant_ids are ignored.
 * @param slot_durations     Switching and samplig slot durations for AoA mode.
 * @param switch_pattern_len Number of antenna ids in switch pattern.
 * @param ant_ids            Array of antenna identifiers.
 *
 * @return HCI status of command completion.
 */
uint8_t ll_df_set_conn_cte_rx_params(uint16_t handle, uint8_t sampling_enable,
				     uint8_t slot_durations, uint8_t switch_pattern_len,
				     const uint8_t *ant_ids)
{
	struct lll_df_conn_rx_params *params_rx;
	struct dbuf_hdr *params_buf_hdr;
	struct lll_df_conn_rx_cfg *cfg_rx;
	struct ll_conn *conn;
	uint8_t params_idx;

	conn = ll_connected_get(handle);
	if (!conn) {
		return BT_HCI_ERR_UNKNOWN_CONN_ID;
	}

	cfg_rx = &conn->lll.df_rx_cfg;
	/* This is an information for HCI_LE_Connection_CTE_Request_Enable that
	 * HCI_LE_Set_Connection_CTE_Receive_Parameters was called at least once.
	 */
	cfg_rx->is_initialized = 1U;
	params_buf_hdr = &cfg_rx->hdr;

	params_rx = dbuf_alloc(params_buf_hdr, &params_idx);

	if (!sampling_enable) {
		params_rx->is_enabled = false;
	} else {
		if (IS_ENABLED(CONFIG_BT_CTLR_DF_ANT_SWITCH_RX)) {
			if (!((IS_ENABLED(CONFIG_BT_CTLR_DF_ANT_SWITCH_1US) &&
			       slot_durations == BT_HCI_LE_ANTENNA_SWITCHING_SLOT_1US) ||
			      slot_durations == BT_HCI_LE_ANTENNA_SWITCHING_SLOT_2US)) {
				return BT_HCI_ERR_UNSUPP_FEATURE_PARAM_VAL;
			}

			if (switch_pattern_len < BT_HCI_LE_SWITCH_PATTERN_LEN_MIN ||
			    switch_pattern_len > BT_CTLR_DF_MAX_ANT_SW_PATTERN_LEN || !ant_ids) {
				return BT_HCI_ERR_UNSUPP_FEATURE_PARAM_VAL;
			}
		}

		params_rx->is_enabled = true;
		params_rx->slot_durations = slot_durations;
		(void)memcpy(params_rx->ant_ids, ant_ids, switch_pattern_len);
		params_rx->ant_sw_len = switch_pattern_len;
	}

	dbuf_enqueue(params_buf_hdr, params_idx);

	return BT_HCI_ERR_SUCCESS;
}
#endif /* CONFIG_BT_CTLR_DF_CONN_CTE_RX */

#if defined(CONFIG_BT_CTLR_DF_CONN_CTE_REQ) || defined(CONFIG_BT_CTLR_DF_CONN_CTE_RSP)
static void df_conn_cte_req_disable(void *param)
{
	k_sem_give(param);
}
#endif /* CONFIG_BT_CTLR_DF_CONN_CTE_REQ || CONFIG_BT_CTLR_DF_CONN_CTE_RSP */

#if defined(CONFIG_BT_CTLR_DF_CONN_CTE_REQ)
/* @brief Function enables or disables CTE request control procedure for a connection.
 *
 * The procedure may be enabled in two modes:
 * - single-shot, it is autmatically disabled when the occurrence finishes.
 * - periodic, it is executed periodically until disabled, connection is lost or PHY is changed
 *   to the one that does not support CTE.
 *
 * @param handle               Connection handle.
 * @param enable               Enable or disable CTE request. When the parameter is set to false
 *                             @p cte_request_interval, @requested_cte_length and
 *                             @p requested_cte_type are ignored.
 * @param cte_request_interval Value zero enables single-shot mode. Other values enable periodic
 *                             mode. In periodic mode, the value is a number of connection envets
 *                             the procedure is executed. The value may not be lower than
 *                             connection peer latency.
 * @param requested_cte_length Minimum value of CTE length requested from peer.
 * @param requested_cte_type   Type of CTE requested from peer.
 *
 * @return HCI Status of command completion.
 */
uint8_t ll_df_set_conn_cte_req_enable(uint16_t handle, uint8_t enable,
				      uint16_t cte_request_interval, uint8_t requested_cte_length,
				      uint8_t requested_cte_type)
{
	struct ll_conn *conn;

	conn = ll_connected_get(handle);
	if (!conn) {
		return BT_HCI_ERR_UNKNOWN_CONN_ID;
	}

	if (!enable) {
		ull_cp_cte_req_set_disable(conn);

		if (conn->llcp.cte_req.is_active) {
			struct k_sem sem;

			k_sem_init(&sem, 0U, 1U);
			conn->llcp.cte_req.disable_param = &sem;
			conn->llcp.cte_req.disable_cb = df_conn_cte_req_disable;

			if (!conn->llcp.cte_req.is_active) {
				k_sem_take(&sem, K_FOREVER);
			}
		}

		return BT_HCI_ERR_SUCCESS;
	} else {
		if (!conn->lll.df_rx_cfg.is_initialized) {
			return BT_HCI_ERR_CMD_DISALLOWED;
		}

		if (conn->llcp.cte_req.is_enabled) {
			return BT_HCI_ERR_CMD_DISALLOWED;
		}

#if defined(CONFIG_BT_CTLR_PHY)
		/* CTE request may be enabled only in case the receiver PHY is not CODED */
		if (conn->lll.phy_rx == PHY_CODED) {
			return BT_HCI_ERR_CMD_DISALLOWED;
		}
#endif /* CONFIG_BT_CTLR_PHY */

		if (cte_request_interval != 0 && cte_request_interval < conn->lll.latency) {
			return BT_HCI_ERR_CMD_DISALLOWED;
		}

		if (requested_cte_length < BT_HCI_LE_CTE_LEN_MIN ||
		    requested_cte_length > BT_HCI_LE_CTE_LEN_MAX) {
			return BT_HCI_ERR_UNSUPP_FEATURE_PARAM_VAL;
		}

		if (requested_cte_type != BT_HCI_LE_AOA_CTE &&
		    requested_cte_type != BT_HCI_LE_AOD_CTE_1US &&
		    requested_cte_type != BT_HCI_LE_AOD_CTE_2US) {
			return BT_HCI_ERR_UNSUPP_FEATURE_PARAM_VAL;
		}

		/* If controller is aware of features supported by peer device then check
		 * whether required features are enabled.
		 */
		if (conn->llcp.fex.valid &&
		    (!(conn->llcp.fex.features_peer & BIT64(BT_LE_FEAT_BIT_CONN_CTE_RESP)) ||
		     ((requested_cte_type == BT_HCI_LE_AOD_CTE_1US ||
		       requested_cte_type == BT_HCI_LE_AOD_CTE_2US) &&
		      !(conn->llcp.fex.features_peer &
			BIT64(BT_LE_FEAT_BIT_ANT_SWITCH_TX_AOD))))) {
			return BT_HCI_ERR_UNSUPP_REMOTE_FEATURE;
		}

		conn->llcp.cte_req.is_enabled = 1U;
		conn->llcp.cte_req.req_interval = cte_request_interval;
		conn->llcp.cte_req.cte_type = requested_cte_type;
		conn->llcp.cte_req.min_cte_len = requested_cte_length;
	}

	return ull_cp_cte_req(conn, requested_cte_length, requested_cte_type);
}
#endif /* CONFIG_BT_CTLR_DF_CONN_CTE_REQ */

#if defined(CONFIG_BT_CTLR_DF_CONN_CTE_RSP)
/**
 * @brief Function enables or disables CTE response control procedure for a connection.
 *
 * @param handle Connection handle.
 * @param enable Enable or disable CTE response.
 *
 * @return HCI Status of command completion.
 */
uint8_t ll_df_set_conn_cte_rsp_enable(uint16_t handle, uint8_t enable)
{
	struct ll_conn *conn;

	conn = ll_connected_get(handle);
	if (!conn) {
		return BT_HCI_ERR_UNKNOWN_CONN_ID;
	}

	if (enable) {
		if (!conn->lll.df_tx_cfg.is_initialized) {
			return BT_HCI_ERR_CMD_DISALLOWED;
		}

#if defined(CONFIG_BT_CTLR_PHY)
		/* CTE may not be send over CODED PHY */
		if (conn->lll.phy_tx == PHY_CODED) {
			return BT_HCI_ERR_CMD_DISALLOWED;
		}
#endif /* CONFIG_BT_CTLR_PHY */
		conn->lll.df_tx_cfg.cte_rsp_en = 1U;

		ull_cp_cte_rsp_enable(conn, enable, LLL_DF_MAX_CTE_LEN,
				conn->lll.df_tx_cfg.cte_types_allowed);
	} else {
		conn->lll.df_tx_cfg.cte_rsp_en = false;

		if (conn->llcp.cte_rsp.is_active) {
			struct k_sem sem;

			k_sem_init(&sem, 0U, 1U);
			conn->llcp.cte_rsp.disable_param = &sem;
			conn->llcp.cte_rsp.disable_cb = df_conn_cte_req_disable;

			if (!conn->llcp.cte_rsp.is_active) {
				k_sem_take(&sem, K_FOREVER);
			}
		}
	}

	return BT_HCI_ERR_SUCCESS;
}
#endif /* CONFIG_BT_CTLR_DF_CONN_CTE_RSP */

/* @brief Function provides information about Direction Finding
 *        antennas switching and sampling related settings.
 *
 * @param[out]switch_sample_rates       Pointer to store available antennas
 *                                      switch-sampling configurations.
 * @param[out]num_ant                   Pointer to store number of available
 *                                      antennas.
 * @param[out]max_switch_pattern_len    Pointer to store maximum number of
 *                                      antennas ids in switch pattern.
 * @param[out]max_cte_len               Pointer to store maximum length of CTE
 *                                      in [8us] units.
 */
void ll_df_read_ant_inf(uint8_t *switch_sample_rates, uint8_t *num_ant,
			uint8_t *max_switch_pattern_len, uint8_t *max_cte_len)
{
	*switch_sample_rates = 0;
	if (IS_ENABLED(CONFIG_BT_CTLR_DF_ANT_SWITCH_TX) &&
	    IS_ENABLED(CONFIG_BT_CTLR_DF_ANT_SWITCH_1US)) {
		*switch_sample_rates |= DF_AOD_1US_TX;
	}

	if (IS_ENABLED(CONFIG_BT_CTLR_DF_CTE_RX) &&
	    IS_ENABLED(CONFIG_BT_CTLR_DF_CTE_RX_SAMPLE_1US)) {
		*switch_sample_rates |= DF_AOD_1US_RX;
	}

	if (IS_ENABLED(CONFIG_BT_CTLR_DF_ANT_SWITCH_RX) &&
	    IS_ENABLED(CONFIG_BT_CTLR_DF_CTE_RX_SAMPLE_1US)) {
		*switch_sample_rates |= DF_AOA_1US;
	}

	*max_switch_pattern_len = BT_CTLR_DF_MAX_ANT_SW_PATTERN_LEN;
	*num_ant = lll_df_ant_num_get();
	*max_cte_len = LLL_DF_MAX_CTE_LEN;
}
