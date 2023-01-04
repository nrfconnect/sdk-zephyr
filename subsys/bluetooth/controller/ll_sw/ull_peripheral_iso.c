/*
 * Copyright (c) 2020 Demant
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/bluetooth/buf.h>
#include <zephyr/sys/byteorder.h>

#include "util/memq.h"
#include "util/mayfly.h"

#include "hal/ccm.h"
#include "hal/ticker.h"

#include "ticker/ticker.h"

#include "pdu.h"

#include "lll.h"
#include "lll/lll_vendor.h"
#include "lll_conn.h"
#include "lll_conn_iso.h"
#include "lll_clock.h"

#include "isoal.h"
#include "ull_iso_types.h"

#if !defined(CONFIG_BT_LL_SW_LLCP_LEGACY)
#include "ull_tx_queue.h"
#endif

#include "ull_conn_types.h"
#include "ull_conn_iso_types.h"
#include "ull_internal.h"
#include "ull_llcp.h"
#include "ull_llcp_internal.h"

#include "ull_conn_internal.h"
#include "ull_conn_iso_internal.h"
#include "lll_peripheral_iso.h"

#include <zephyr/bluetooth/hci.h>

#include "hal/debug.h"

#define LOG_LEVEL CONFIG_BT_HCI_DRIVER_LOG_LEVEL
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(bt_ctlr_ull_peripheral_iso);

static struct ll_conn *ll_cis_get_acl_awaiting_reply(uint16_t handle, uint8_t *error)
{
	struct ll_conn *acl_conn = NULL;

	if (!IS_CIS_HANDLE(handle) || ll_conn_iso_stream_get(handle)->group == NULL) {
		LOG_ERR("Unknown CIS handle %u", handle);
		*error = BT_HCI_ERR_UNKNOWN_CONN_ID;
		return NULL;
	}

	for (int h = 0; h < CONFIG_BT_MAX_CONN; h++) {
		struct ll_conn *conn = ll_conn_get(h);
#if defined(CONFIG_BT_LL_SW_LLCP_LEGACY)
		uint16_t cis_handle = conn->llcp_cis.cis_handle;
#else
		uint16_t cis_handle = ull_cp_cc_ongoing_handle(conn);
#endif

		if (handle == cis_handle) {
			/* ACL connection found */
			acl_conn = conn;
			break;
		}
	}

	if (!acl_conn) {
		LOG_ERR("No connection found for handle %u", handle);
		*error = BT_HCI_ERR_CMD_DISALLOWED;
		return NULL;
	}

	if (acl_conn->lll.role == BT_CONN_ROLE_CENTRAL) {
		LOG_ERR("Not allowed for central");
		*error = BT_HCI_ERR_CMD_DISALLOWED;
		return NULL;
	}

#if defined(CONFIG_BT_LL_SW_LLCP_LEGACY)
	if (acl_conn->llcp_cis.state != LLCP_CIS_STATE_RSP_WAIT) {
#else
	if (!ull_cp_cc_awaiting_reply(acl_conn)) {
#endif
		LOG_ERR("Not allowed in current procedure state");
		*error = BT_HCI_ERR_CMD_DISALLOWED;
		return NULL;
	}

	return acl_conn;
}

uint8_t ll_cis_accept(uint16_t handle)
{
	uint8_t status = BT_HCI_ERR_SUCCESS;
	struct ll_conn *acl_conn = ll_cis_get_acl_awaiting_reply(handle, &status);

	if (acl_conn) {
		/* Accept request */
#if defined(CONFIG_BT_LL_SW_LLCP_LEGACY)
		acl_conn->llcp_cis.req++;
#else
		ull_cp_cc_accept(acl_conn);
#endif
	}

	return status;
}

uint8_t ll_cis_reject(uint16_t handle, uint8_t reason)
{
	uint8_t status = BT_HCI_ERR_SUCCESS;

#if defined(CONFIG_BT_LL_SW_LLCP_LEGACY)
	status = BT_HCI_ERR_CMD_DISALLOWED;
#else
	struct ll_conn *acl_conn = ll_cis_get_acl_awaiting_reply(handle, &status);

	if (acl_conn) {
		/* Reject request */
		ull_cp_cc_reject(acl_conn, reason);
	}
#endif

	return status;
}

int ull_peripheral_iso_init(void)
{
	return 0;
}

int ull_peripheral_iso_reset(void)
{
	return 0;
}

/* Use this function to release CIS/CIG resources on an aborted CIS setup
 * ie if CIS setup is 'cancelled' after call to ull_peripheral_iso_acquire()
 * because of a rejection of the CIS request
 */
void ull_peripheral_iso_release(uint16_t cis_handle)
{
	struct ll_conn_iso_stream *cis;
	struct ll_conn_iso_group *cig;

	cis = ll_conn_iso_stream_get(cis_handle);
	LL_ASSERT(cis);

	cig = cis->group;

	ll_conn_iso_stream_release(cis);
	cig->lll.num_cis--;

	if (!cig->lll.num_cis) {
		ll_conn_iso_group_release(cig);
	}
}

uint8_t ull_peripheral_iso_acquire(struct ll_conn *acl,
				   struct pdu_data_llctrl_cis_req *req,
				   uint16_t *cis_handle)
{
	struct ll_conn_iso_group *cig;
	struct ll_conn_iso_stream *cis;
	uint32_t iso_interval_us;
	uint16_t handle;

	/* Get CIG by id */
	cig = ll_conn_iso_group_get_by_id(req->cig_id);
	if (!cig) {
		/* CIG does not exist - create it */
		cig = ll_conn_iso_group_acquire();
		if (!cig) {
			/* No space for new CIG */
			return BT_HCI_ERR_INSUFFICIENT_RESOURCES;
		}

		memset(&cig->lll, 0, sizeof(cig->lll));

		cig->iso_interval = sys_le16_to_cpu(req->iso_interval);
		iso_interval_us = cig->iso_interval * CONN_INT_UNIT_US;

		cig->cig_id = req->cig_id;
		cig->lll.handle = LLL_HANDLE_INVALID;
		cig->lll.role = acl->lll.role;
		cig->lll.resume_cis = LLL_HANDLE_INVALID;

		/* Calculate CIG default maximum window widening. NOTE: This calculation
		 * does not take into account that leading CIS with NSE>=3 must reduce
		 * the maximum window widening to one sub-interval. This must be applied
		 * in LLL (BT Core 5.3, Vol 6, Part B, section 4.2.4).
		 */
		cig->lll.window_widening_max_us = (iso_interval_us >> 1) -
						  EVENT_IFS_US;
		cig->lll.window_widening_periodic_us_frac =
			ceiling_fraction(((lll_clock_ppm_local_get() +
					 lll_clock_ppm_get(acl->periph.sca)) *
					 EVENT_US_TO_US_FRAC(iso_interval_us)), USEC_PER_SEC);

		ull_hdr_init(&cig->ull);
		lll_hdr_init(&cig->lll, cig);
	}

	if (cig->lll.num_cis == CONFIG_BT_CTLR_CONN_ISO_STREAMS_PER_GROUP) {
		/* No space in CIG for new CIS */
		return BT_HCI_ERR_INSUFFICIENT_RESOURCES;
	}

	for (handle = LL_CIS_HANDLE_BASE; handle <= LAST_VALID_CIS_HANDLE; handle++) {
		cis = ll_iso_stream_connected_get(handle);
		if (cis && cis->group && cis->cis_id == req->cis_id) {
			/* CIS ID already in use */
			return BT_HCI_ERR_INVALID_LL_PARAM;
		}
	}

	/* Acquire new CIS */
	cis = ll_conn_iso_stream_acquire();
	if (cis == NULL) {
		if (!cig->lll.num_cis) {
			/* No CIS's in CIG, so this was just allocated
			 * so release as we can't use it
			 */
			ll_conn_iso_group_release(cig);
		}
		/* No space for new CIS */
		return BT_HCI_ERR_INSUFFICIENT_RESOURCES;
	}

	/* Read 20-bit SDU intervals (mask away RFU bits) */
	cig->c_sdu_interval = sys_get_le24(req->c_sdu_interval) & 0x0FFFFF;
	cig->p_sdu_interval = sys_get_le24(req->p_sdu_interval) & 0x0FFFFF;

	cis->cis_id = req->cis_id;
	cis->framed = (req->c_max_sdu_packed[1] & BIT(7)) >> 7;
	cis->established = 0;
	cis->group = cig;
	cis->teardown = 0;
	cis->released_cb = NULL;
	cis->c_max_sdu = (uint16_t)(req->c_max_sdu_packed[1] & 0x0F) << 8 |
				    req->c_max_sdu_packed[0];
	cis->p_max_sdu = (uint16_t)(req->p_max_sdu[1] & 0x0F) << 8 |
				    req->p_max_sdu[0];

	cis->lll.handle = 0xFFFF;
	cis->lll.acl_handle = acl->lll.handle;
	cis->lll.sub_interval = sys_get_le24(req->sub_interval);
	cis->lll.num_subevents = req->nse;
	cis->lll.next_subevent = 0;
	cis->lll.sn = 0;
	cis->lll.nesn = 0;
	cis->lll.cie = 0;
	cis->lll.flushed = 0;
	cis->lll.active = 0;
	cis->lll.datapath_ready_rx = 0;

	cis->lll.rx.phy = req->c_phy;
	cis->lll.rx.burst_number = req->c_bn;
	cis->lll.rx.flush_timeout = req->c_ft;
	cis->lll.rx.max_octets = sys_le16_to_cpu(req->c_max_pdu);
	cis->lll.rx.payload_number = 0;

	cis->lll.tx.phy = req->p_phy;
	cis->lll.tx.burst_number = req->p_bn;
	cis->lll.tx.flush_timeout = req->p_ft;
	cis->lll.tx.max_octets = sys_le16_to_cpu(req->p_max_pdu);
	cis->lll.tx.payload_number = 0;

	if (!cis->lll.link_tx_free) {
		cis->lll.link_tx_free = &cis->lll.link_tx;
	}

	memq_init(cis->lll.link_tx_free, &cis->lll.memq_tx.head,
		  &cis->lll.memq_tx.tail);
	cis->lll.link_tx_free = NULL;

	*cis_handle = ll_conn_iso_stream_handle_get(cis);
	cig->lll.num_cis++;

	return 0;
}

uint8_t ull_peripheral_iso_setup(struct pdu_data_llctrl_cis_ind *ind,
				 uint8_t cig_id, uint16_t cis_handle)
{
	struct ll_conn_iso_stream *cis = NULL;
	struct ll_conn_iso_group *cig;

	/* Get CIG by id */
	cig = ll_conn_iso_group_get_by_id(cig_id);
	if (!cig) {
		return BT_HCI_ERR_UNSPECIFIED;
	}

	cig->lll.handle = ll_conn_iso_group_handle_get(cig);
	cig->sync_delay = sys_get_le24(ind->cig_sync_delay);

	cis = ll_conn_iso_stream_get(cis_handle);
	if (!cis) {
		return BT_HCI_ERR_UNSPECIFIED;
	}

	cis->sync_delay = sys_get_le24(ind->cis_sync_delay);
	cis->offset = sys_get_le24(ind->cis_offset);
	cis->lll.event_count = -1;
	memcpy(cis->lll.access_addr, ind->aa, sizeof(ind->aa));

	return 0;
}

static void ticker_op_cb(uint32_t status, void *param)
{
	ARG_UNUSED(param);

	LL_ASSERT(status == TICKER_STATUS_SUCCESS);
}

void ull_peripheral_iso_update_ticker(struct ll_conn_iso_group *cig,
				      uint32_t ticks_at_expire,
				      uint32_t iso_interval_us_frac)
{

	/* stop/start with new updated timings */
	uint8_t ticker_id_cig = TICKER_ID_CONN_ISO_BASE + ll_conn_iso_group_handle_get(cig);
	uint32_t ticker_status = ticker_stop(TICKER_INSTANCE_ID_CTLR, TICKER_USER_ID_ULL_HIGH,
				    ticker_id_cig, ticker_op_cb, NULL);
	LL_ASSERT((ticker_status == TICKER_STATUS_SUCCESS) ||
		  (ticker_status == TICKER_STATUS_BUSY));

	ticker_status = ticker_start(TICKER_INSTANCE_ID_CTLR,
				     TICKER_USER_ID_ULL_HIGH,
				     ticker_id_cig,
				     ticks_at_expire,
				     EVENT_US_FRAC_TO_TICKS(iso_interval_us_frac),
				     EVENT_US_FRAC_TO_TICKS(iso_interval_us_frac),
				     EVENT_US_FRAC_TO_REMAINDER(iso_interval_us_frac),
				     TICKER_NULL_LAZY,
				     0,
				     ull_conn_iso_ticker_cb, cig,
				     ticker_op_cb, NULL);

	LL_ASSERT((ticker_status == TICKER_STATUS_SUCCESS) ||
		  (ticker_status == TICKER_STATUS_BUSY));

}

void ull_peripheral_iso_update_peer_sca(struct ll_conn *acl)
{
	uint8_t cig_handle;

	/* Find CIG associated with ACL conn */
	for (cig_handle = 0; cig_handle < CONFIG_BT_CTLR_CONN_ISO_GROUPS; cig_handle++) {
		/* Go through all ACL affiliated CIGs and update peer SCA */
		struct ll_conn_iso_stream *cis;
		struct ll_conn_iso_group *cig;

		cig = ll_conn_iso_group_get(cig_handle);
		if (!cig || !cig->lll.num_cis) {
			continue;
		}
		cis = ll_conn_iso_stream_get_by_group(cig, NULL);
		LL_ASSERT(cis);

		uint16_t cis_handle = cis->lll.handle;

		cis = ll_iso_stream_connected_get(cis_handle);
		if (!cis) {
			continue;
		}

		if (cis->lll.acl_handle == acl->lll.handle) {
			cig->sca_update = acl->periph.sca + 1;
		}
	}
}
