/*
 * Copyright (c) 2017-2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define ULL_ADV_RANDOM_DELAY HAL_TICKER_US_TO_TICKS(10000)

/* Bitmask value returned by ull_adv_is_enabled() */
#define ULL_ADV_ENABLED_BITMASK_ENABLED  BIT(0)

/* Helper functions to initialise and reset ull_adv module */
int ull_adv_init(void);
int ull_adv_reset(void);
int ull_adv_reset_finalize(void);

/* Return ll_adv_set context (unconditional) */
struct ll_adv_set *ull_adv_set_get(uint8_t handle);

/* Return the adv set handle given the adv set instance */
uint16_t ull_adv_handle_get(struct ll_adv_set *adv);

/* Return ll_adv_set context if enabled */
struct ll_adv_set *ull_adv_is_enabled_get(uint8_t handle);

/* Return enabled status of a set */
int ull_adv_is_enabled(uint8_t handle);

/* Return filter policy used */
uint32_t ull_adv_filter_pol_get(uint8_t handle);

/* Return ll_adv_set context if created */
struct ll_adv_set *ull_adv_is_created_get(uint8_t handle);

/* Helper function to construct AD data */
uint8_t ull_adv_data_set(struct ll_adv_set *adv, uint8_t len,
			 uint8_t const *const data);

/* Helper function to construct SR data */
uint8_t ull_scan_rsp_set(struct ll_adv_set *adv, uint8_t len,
			 uint8_t const *const data);

/* Update AdvA and TgtA (if application) in advertising PDU */
const uint8_t *ull_adv_pdu_update_addrs(struct ll_adv_set *adv,
					struct pdu_adv *pdu);

/* Helper to update primary channel advertising event time reservation */
uint8_t ull_adv_time_update(struct ll_adv_set *adv, struct pdu_adv *pdu,
			    struct pdu_adv *pdu_scan);

#if defined(CONFIG_BT_CTLR_ADV_EXT)

/* Enumeration provides flags for management of memory for extra_data
 * related with advertising PDUs.
 */
enum ull_adv_pdu_extra_data_flag {
	/* Allocate extra_data memory if it was available in former PDU */
	ULL_ADV_PDU_EXTRA_DATA_ALLOC_IF_EXIST,
	/* Allocate extra_data memory no matter if it was available */
	ULL_ADV_PDU_EXTRA_DATA_ALLOC_ALWAYS,
	/* Never allocate new memory for extra_data */
	ULL_ADV_PDU_EXTRA_DATA_ALLOC_NEVER
};

/* helper function to handle adv done events */
void ull_adv_done(struct node_rx_event_done *done);

/* Helper functions to initialise and reset ull_adv_aux module */
int ull_adv_aux_init(void);
int ull_adv_aux_reset_finalize(void);

/* Return the aux set handle given the aux set instance */
uint8_t ull_adv_aux_handle_get(struct ll_adv_aux_set *aux);

/* Helper function to apply Channel Map Update for auxiliary PDUs */
uint8_t ull_adv_aux_chm_update(void);

/* helper function to initialize event timings */
uint32_t ull_adv_aux_evt_init(struct ll_adv_aux_set *aux);

/* helper function to start auxiliary advertising */
uint32_t ull_adv_aux_start(struct ll_adv_aux_set *aux, uint32_t ticks_anchor,
			   uint32_t ticks_slot_overhead);

/* helper function to stop auxiliary advertising */
int ull_adv_aux_stop(struct ll_adv_aux_set *aux);

/* helper function to acquire and initialize auxiliary advertising instance */
struct ll_adv_aux_set *ull_adv_aux_acquire(struct lll_adv *lll);

/* helper function to release auxiliary advertising instance */
void ull_adv_aux_release(struct ll_adv_aux_set *aux);

/* helper function to schedule a mayfly to get aux offset */
void ull_adv_aux_offset_get(struct ll_adv_set *adv);

/* Below are BT Spec v5.2, Vol 6, Part B Section 2.3.4 Table 2.12 defined */
#define ULL_ADV_PDU_HDR_FIELD_NONE      0
#define ULL_ADV_PDU_HDR_FIELD_ADVA      BIT(0)
#define ULL_ADV_PDU_HDR_FIELD_TARGETA   BIT(1)
#define ULL_ADV_PDU_HDR_FIELD_CTE_INFO  BIT(2)
#define ULL_ADV_PDU_HDR_FIELD_ADI       BIT(3)
#define ULL_ADV_PDU_HDR_FIELD_AUX_PTR   BIT(4)
#define ULL_ADV_PDU_HDR_FIELD_SYNC_INFO BIT(5)
#define ULL_ADV_PDU_HDR_FIELD_TX_POWER  BIT(6)
#define ULL_ADV_PDU_HDR_FIELD_RFU       BIT(7)
/* Below are implementation defined bit fields */
#define ULL_ADV_PDU_HDR_FIELD_ACAD      BIT(8)
#define ULL_ADV_PDU_HDR_FIELD_AD_DATA   BIT(9)

/* helper defined for field offsets in the hdr_set_clear interfaces */
#define ULL_ADV_HDR_DATA_LEN_OFFSET      0
#define ULL_ADV_HDR_DATA_LEN_SIZE        1
#define ULL_ADV_HDR_DATA_ACAD_PTR_OFFSET 1
#define ULL_ADV_HDR_DATA_ACAD_PTR_SIZE   (sizeof(uint8_t *))
#define ULL_ADV_HDR_DATA_DATA_PTR_OFFSET 1
#define ULL_ADV_HDR_DATA_DATA_PTR_SIZE   (sizeof(uint8_t *))

/* helper function to set/clear common extended header format fields */
uint8_t ull_adv_aux_hdr_set_clear(struct ll_adv_set *adv,
				  uint16_t sec_hdr_add_fields,
				  uint16_t sec_hdr_rem_fields,
				  void *value,
				  struct pdu_adv_adi *adi,
				  uint8_t *pri_idx);

/* helper to initialize extended advertising PDU */
void ull_adv_sync_pdu_init(struct pdu_adv *pdu, uint8_t ext_hdr_flags);

/* helper to add cte_info field to extended advertising header */
uint8_t ull_adv_sync_pdu_cte_info_set(struct pdu_adv *pdu, const struct pdu_cte_info *cte_info);

/* helper to get information whether ADI field is avaialbe in extended advertising PDU */
static inline bool ull_adv_sync_pdu_had_adi(const struct pdu_adv *pdu)
{
	return pdu->adv_ext_ind.ext_hdr.adi;
}
/* helper function to calculate common ext adv payload header length and
 * adjust the data pointer.
 * NOTE: This function reverts the header data pointer if there is no
 *       header fields flags set, and hence no header fields have been
 *       populated.
 */
static inline uint8_t
ull_adv_aux_hdr_len_calc(struct pdu_adv_com_ext_adv *com_hdr, uint8_t **dptr)
{
	uint8_t len;

	len = *dptr - (uint8_t *)com_hdr;
	if (len <= (PDU_AC_EXT_HEADER_SIZE_MIN +
		    sizeof(struct pdu_adv_ext_hdr))) {
		len = PDU_AC_EXT_HEADER_SIZE_MIN;
		*dptr = (uint8_t *)com_hdr + len;
	}

	return len;
}

/* helper function to fill common ext adv payload header length */
static inline void
ull_adv_aux_hdr_len_fill(struct pdu_adv_com_ext_adv *com_hdr, uint8_t len)
{
	com_hdr->ext_hdr_len = len - PDU_AC_EXT_HEADER_SIZE_MIN;

}

/* helper function to get next unique DID value */
uint16_t ull_adv_aux_did_next_unique_get(uint8_t sid);

/* helper function to fill the aux ptr structure in common ext adv payload */
void ull_adv_aux_ptr_fill(struct pdu_adv_aux_ptr *aux_ptr, uint32_t offs_us,
			  uint8_t phy_s);

/* helper function to handle adv aux done events */
void ull_adv_aux_done(struct node_rx_event_done *done);

int ull_adv_sync_init(void);
int ull_adv_sync_reset(void);
int ull_adv_sync_reset_finalize(void);

/* helper function to release periodic advertising instance */
void ull_adv_sync_release(struct ll_adv_sync_set *sync);

/* helper function to start periodic advertising */
uint32_t ull_adv_sync_start(struct ll_adv_set *adv,
			    struct ll_adv_sync_set *sync,
			    uint32_t ticks_anchor);

/* helper function to update periodic advertising event time reservation */
uint8_t ull_adv_sync_time_update(struct ll_adv_sync_set *sync,
				 struct pdu_adv *pdu);

/* helper function to initial channel map update indications */
uint8_t ull_adv_sync_chm_update(void);

/* helper function to cleanup after channel map update indications complete */
void ull_adv_sync_chm_complete(struct node_rx_hdr *rx);

/* helper function to fill initial value of sync_info structure */
void ull_adv_sync_info_fill(struct ll_adv_sync_set *sync,
			    struct pdu_adv_sync_info *si);

/* helper function to allocate new PDU data for AUX_SYNC_IND and return
 * previous and new PDU for further processing.
 */
uint8_t ull_adv_sync_pdu_alloc(struct ll_adv_set *adv,
			       enum ull_adv_pdu_extra_data_flag extra_data_flags,
			       struct pdu_adv **ter_pdu_prev, struct pdu_adv **ter_pdu_new,
			       void **extra_data_prev, void **extra_data_new, uint8_t *ter_idx);

/* helper function to set/clear common extended header format fields
 * for AUX_SYNC_IND PDU.
 */
uint8_t ull_adv_sync_pdu_set_clear(struct lll_adv_sync *lll_sync, struct pdu_adv *ter_pdu_prev,
				   struct pdu_adv *ter_pdu, uint16_t hdr_add_fields,
				   uint16_t hdr_rem_fields, void *hdr_data);

/* helper function to update extra_data field */
void ull_adv_sync_extra_data_set_clear(void *extra_data_prev,
				       void *extra_data_new,
				       uint16_t hdr_add_fields,
				       uint16_t hdr_rem_fields,
				       void *data);

/* helper function to schedule a mayfly to get sync offset */
void ull_adv_sync_offset_get(struct ll_adv_set *adv);

int ull_adv_iso_init(void);
int ull_adv_iso_reset(void);

/* helper function to schedule a mayfly to get BIG offset */
void ull_adv_iso_offset_get(struct ll_adv_sync_set *sync);

/* helper function to handle adv ISO done BIG complete events */
void ull_adv_iso_done_complete(struct node_rx_event_done *done);

/* helper function to handle adv ISO done BIG terminate events */
void ull_adv_iso_done_terminate(struct node_rx_event_done *done);

/* helper function to return adv_iso instance */
struct ll_adv_iso_set *ull_adv_iso_by_stream_get(uint16_t handle);

/* helper function to release stream instances */
void ull_adv_iso_stream_release(struct ll_adv_iso_set *adv_iso);

#if defined(CONFIG_BT_CTLR_DF_ADV_CTE_TX)
/* helper function to release unused DF configuration memory */
void ull_df_adv_cfg_release(struct lll_df_adv_cfg *df_adv_cfg);
#endif /* CONFIG_BT_CTLR_DF_ADV_CTE_TX */

#endif /* CONFIG_BT_CTLR_ADV_EXT */
