/*
 * Copyright (c) 2020 Demant
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/bluetooth/buf.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/bluetooth/iso.h>

#include "util/memq.h"
#include "util/mayfly.h"
#include "util/util.h"

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
#include "ull_tx_queue.h"
#include "ull_internal.h"

#include "ull_conn_types.h"
#include "ull_conn_internal.h"
#include "ull_conn_iso_types.h"
#include "ull_conn_iso_internal.h"
#include "ull_llcp.h"
#include "lll_central_iso.h"

#include "ll.h"

#include <zephyr/bluetooth/hci.h>

#include "hal/debug.h"

#define SDU_MAX_DRIFT_PPM 100

/* Setup cache for CIG commit transaction */
static struct {
	struct ll_conn_iso_group group;
	uint8_t c_ft;
	uint8_t p_ft;
	uint8_t cis_idx;
	struct ll_conn_iso_stream stream[CONFIG_BT_CTLR_CONN_ISO_STREAMS_PER_GROUP];
} ll_iso_setup;

uint8_t ll_cig_parameters_open(uint8_t cig_id,
			       uint32_t c_interval, uint32_t p_interval,
			       uint8_t sca, uint8_t packing, uint8_t framing,
			       uint16_t c_latency, uint16_t p_latency,
			       uint8_t num_cis)
{
	memset(&ll_iso_setup, 0, sizeof(ll_iso_setup));

	ll_iso_setup.group.cig_id = cig_id;
	ll_iso_setup.group.c_sdu_interval = c_interval;
	ll_iso_setup.group.p_sdu_interval = p_interval;
	ll_iso_setup.group.c_latency = c_latency * 1000;
	ll_iso_setup.group.p_latency = p_latency * 1000;
	ll_iso_setup.group.cis_count = num_cis;
	ll_iso_setup.group.central.sca = sca;
	ll_iso_setup.group.central.packing = packing;
	ll_iso_setup.group.central.framing = framing;

	return BT_HCI_ERR_SUCCESS;
}

uint8_t ll_cis_parameters_set(uint8_t cis_id,
			      uint16_t c_sdu, uint16_t p_sdu,
			      uint8_t c_phy, uint8_t p_phy,
			      uint8_t c_rtn, uint8_t p_rtn)
{
	uint8_t cis_idx = ll_iso_setup.cis_idx;

	if (cis_idx >= CONFIG_BT_CTLR_CONN_ISO_STREAMS_PER_GROUP) {
		return BT_HCI_ERR_INSUFFICIENT_RESOURCES;
	}

	memset(&ll_iso_setup.stream[cis_idx], 0, sizeof(struct ll_conn_iso_stream));

	ll_iso_setup.stream[cis_idx].cis_id = cis_id;
	ll_iso_setup.stream[cis_idx].c_max_sdu = c_sdu;
	ll_iso_setup.stream[cis_idx].p_max_sdu = p_sdu;
	ll_iso_setup.stream[cis_idx].lll.tx.phy = c_phy;
	ll_iso_setup.stream[cis_idx].lll.rx.phy = p_phy;
	ll_iso_setup.stream[cis_idx].central.c_rtn = c_rtn;
	ll_iso_setup.stream[cis_idx].central.p_rtn = p_rtn;
	ll_iso_setup.cis_idx++;

	return BT_HCI_ERR_SUCCESS;
}


void ll_cis_create(uint16_t cis_handle, uint16_t acl_handle)
{
	/* Handles have been verified prior to calling this function */
	(void)ull_cp_cis_create(
		ll_connected_get(acl_handle),
		ll_conn_iso_stream_get(cis_handle));
}

uint8_t ll_cig_parameters_test_open(uint8_t cig_id,
				    uint32_t c_interval,
				    uint32_t p_interval,
				    uint8_t  c_ft,
				    uint8_t  p_ft,
				    uint16_t iso_interval,
				    uint8_t  sca,
				    uint8_t  packing,
				    uint8_t  framing,
				    uint8_t  num_cis)
{
	memset(&ll_iso_setup, 0, sizeof(ll_iso_setup));

	ll_iso_setup.group.cig_id = cig_id;
	ll_iso_setup.group.c_sdu_interval = c_interval;
	ll_iso_setup.group.p_sdu_interval = p_interval;
	ll_iso_setup.group.iso_interval = iso_interval;
	ll_iso_setup.group.cis_count = num_cis;
	ll_iso_setup.group.central.sca = sca;
	ll_iso_setup.group.central.packing = packing;
	ll_iso_setup.group.central.framing = framing;
	ll_iso_setup.group.central.test = 1U;

	/* TODO: Perhaps move FT to LLL CIG */
	ll_iso_setup.c_ft = c_ft;
	ll_iso_setup.p_ft = p_ft;

	return BT_HCI_ERR_SUCCESS;
}

uint8_t ll_cis_parameters_test_set(uint8_t cis_id, uint8_t nse,
				   uint16_t c_sdu, uint16_t p_sdu,
				   uint16_t c_pdu, uint16_t p_pdu,
				   uint8_t c_phy, uint8_t p_phy,
				   uint8_t c_bn, uint8_t p_bn)
{
	uint8_t cis_idx = ll_iso_setup.cis_idx;

	if (cis_idx >= CONFIG_BT_CTLR_CONN_ISO_STREAMS_PER_GROUP) {
		return BT_HCI_ERR_INSUFFICIENT_RESOURCES;
	}

	memset(&ll_iso_setup.stream[cis_idx], 0, sizeof(struct ll_conn_iso_stream));

	ll_iso_setup.stream[cis_idx].cis_id = cis_id;
	ll_iso_setup.stream[cis_idx].c_max_sdu = c_sdu;
	ll_iso_setup.stream[cis_idx].p_max_sdu = p_sdu;
	ll_iso_setup.stream[cis_idx].lll.num_subevents = nse;
	ll_iso_setup.stream[cis_idx].lll.tx.max_octets = c_bn ? c_pdu : 0;
	ll_iso_setup.stream[cis_idx].lll.rx.max_octets = p_bn ? p_pdu : 0;
	ll_iso_setup.stream[cis_idx].lll.tx.phy = c_phy;
	ll_iso_setup.stream[cis_idx].lll.rx.phy = p_phy;
	ll_iso_setup.stream[cis_idx].lll.tx.burst_number = c_bn;
	ll_iso_setup.stream[cis_idx].lll.rx.burst_number = p_bn;
	ll_iso_setup.cis_idx++;

	return BT_HCI_ERR_SUCCESS;
}

static void set_bn_max_pdu(bool framed, uint32_t iso_interval, uint32_t sdu_interval,
			   uint16_t max_sdu, uint8_t *bn, uint8_t *max_pdu)
{
	uint32_t ceil_f_x_max_sdu;
	uint16_t max_pdu_bn1;
	uint32_t max_drift;
	uint32_t ceil_f;

	if (framed) {
		/* Framed (From ES-18002):
		 *   Max_PDU >= ((ceil(F) x 5 + ceil(F x Max_SDU)) / BN) + 2
		 *   F = (1 + MaxDrift) x ISO_Interval / SDU_Interval
		 *   SegmentationHeader + TimeOffset = 5 bytes
		 *   Continuation header = 2 bytes
		 *   MaxDrift (Max. allowed SDU delivery timing drift) = 100 ppm
		 */
		max_drift = ceiling_fraction(SDU_MAX_DRIFT_PPM * sdu_interval, 1000000U);
		ceil_f = ceiling_fraction(iso_interval + max_drift, sdu_interval);
		ceil_f_x_max_sdu = ceiling_fraction(max_sdu * (iso_interval + max_drift),
						    sdu_interval);

		/* Strategy: Keep lowest possible BN.
		 * TODO: Implement other strategies, possibly as policies.
		 */
		max_pdu_bn1 = ceil_f * 5 + ceil_f_x_max_sdu;
		*bn = ceiling_fraction(max_pdu_bn1, CONFIG_BT_CTLR_ISO_TX_BUFFER_SIZE);
		*max_pdu = ceiling_fraction(max_pdu_bn1, *bn) + 2;
	} else {
		/* For unframed, ISO_Interval must be N x SDU_Interval */
		LL_ASSERT(iso_interval % sdu_interval == 0);

		/* Core 5.3 Vol 6, Part G section 2.1:
		 * BN >= ceil(Max_SDU/Max_PDU * ISO_Interval/SDU_Interval)
		 */
		*bn = ceiling_fraction(max_sdu * iso_interval, (*max_pdu) * sdu_interval);
	}
}


/* TODO:
 * - Drop retransmissions to stay within Max_Transmission_Latency instead of asserting
 * - Calculate ISO_Interval to allow SDU_Interval < ISO_Interval
 */
uint8_t ll_cig_parameters_commit(uint8_t cig_id)
{
	struct ll_conn_iso_stream *cis;
	struct ll_conn_iso_group *cig;
	uint32_t iso_interval_us;
	uint32_t cig_sync_delay;
	uint32_t max_se_length;
	uint32_t c_max_latency;
	uint32_t p_max_latency;
	uint16_t handle_iter;
	uint8_t  cis_count;

	/* Intermediate subevent data */
	struct {
		uint32_t length;
		uint8_t  total_count;
	} se[CONFIG_BT_CTLR_CONN_ISO_STREAMS_PER_GROUP];

	/* If CIG already exists, controller and host are not in sync */
	cig = ll_conn_iso_group_get_by_id(cig_id);
	LL_ASSERT(!cig);

	/* CIG does not exist - create it */
	cig = ll_conn_iso_group_acquire();
	if (!cig) {
		/* No space for new CIG */
		return BT_HCI_ERR_INSUFFICIENT_RESOURCES;
	}

	/* Transfer parameters from update cache and clear LLL fields */
	memcpy(cig, &ll_iso_setup.group, sizeof(struct ll_conn_iso_group));

	/* Setup LLL parameters */
	cig->lll.handle = ll_conn_iso_group_handle_get(cig);
	cig->lll.role = BT_HCI_ROLE_CENTRAL;
	cig->lll.resume_cis = LLL_HANDLE_INVALID;

	if (!cig->central.test) {
		/* TODO: Calculate ISO_Interval based on SDU_Interval and Max_SDU vs Max_PDU,
		 * taking the policy into consideration. It may also be interesting to select an
		 * ISO_Interval which is less likely to collide with other connections.
		 * For instance:
		 *
		 *  SDU_Interval   ISO_Interval   Max_SDU   Max_SDU   Collision risk (10 ms)
		 *  ------------------------------------------------------------------------
		 *  10 ms          10 ms          40        40        100%
		 *  10 ms          12.5 ms        40        50         25%
		 */
		iso_interval_us = cig->c_sdu_interval;
		cig->iso_interval = ceiling_fraction(iso_interval_us, ISO_INT_UNIT_US);
	} else {
		iso_interval_us = cig->iso_interval * ISO_INT_UNIT_US;
	}

	ull_hdr_init(&cig->ull);
	lll_hdr_init(&cig->lll, cig);

	max_se_length = 0;
	cis_count = cig->cis_count;

	/* 1) Acquire CIS instances and initialize instance data.
	 * 2) Calculate SE_Length for each CIS and store the largest
	 * 3) Calculate BN
	 * 4) Calculate total number of subevents needed to transfer payloads
	 *
	 *                 Sequential                Interleaved
	 * CIS0            ___█_█_█_____________█_   ___█___█___█_________█_
	 * CIS1            _________█_█_█_________   _____█___█___█_________
	 * CIS_Sub_Interval  |.|                       |...|
	 * CIG_Sync_Delay    |............|            |............|
	 * CIS_Sync_Delay 0  |............|            |............|
	 * CIS_Sync_Delay 1        |......|              |..........|
	 * ISO_Interval      |.................|..     |.................|..
	 */
	for (uint8_t i = 0; i < cis_count; i++) {
		uint32_t mpt_c;
		uint32_t mpt_p;
		bool tx;
		bool rx;

		/* Acquire new CIS */
		cis = ll_conn_iso_stream_acquire();
		if (cis == NULL) {
			/* No space for new CIS */
			return BT_HCI_ERR_INSUFFICIENT_RESOURCES;
		}

		/* Transfer parameters from update cache */
		memcpy(cis, &ll_iso_setup.stream[i], sizeof(struct ll_conn_iso_stream));
		cis->group  = cig;
		cis->framed = cig->central.framing;

		cis->lll.handle = ll_conn_iso_stream_handle_get(cis);

		if (cig->central.test) {
			cis->lll.tx.flush_timeout = ll_iso_setup.c_ft;
			cis->lll.rx.flush_timeout = ll_iso_setup.p_ft;

			tx = cis->lll.tx.burst_number && cis->lll.tx.max_octets;
			rx = cis->lll.rx.burst_number && cis->lll.rx.max_octets;
		} else {
			LL_ASSERT(iso_interval_us >= cig->c_sdu_interval);

			tx = cig->c_sdu_interval && cis->c_max_sdu;
			rx = cig->p_sdu_interval && cis->p_max_sdu;

			/* Use Max_PDU = MIN(<buffer_size>, Max_SDU) as default. May be changed by
			 * set_bn_max_pdu.
			 */
			cis->lll.tx.max_octets = MIN(CONFIG_BT_CTLR_ISO_TX_BUFFER_SIZE,
						     cis->c_max_sdu);
			cis->lll.rx.max_octets = MIN(251, cis->p_max_sdu);

			/* Calculate BN and Max_PDU (framed) for both directions */
			if (tx) {
				set_bn_max_pdu(cis->framed, iso_interval_us, cig->c_sdu_interval,
					       cis->c_max_sdu, &cis->lll.tx.burst_number,
					       &cis->lll.tx.max_octets);
			} else {
				cis->lll.tx.burst_number = 0;
			}

			if (rx) {
				set_bn_max_pdu(cis->framed, iso_interval_us, cig->p_sdu_interval,
					       cis->p_max_sdu, &cis->lll.rx.burst_number,
					       &cis->lll.rx.max_octets);
			} else {
				cis->lll.rx.burst_number = 0;
			}
		}

		/* Calculate SE_Length */
		mpt_c = PDU_CIS_MAX_US(cis->lll.tx.max_octets, tx ? 1 : 0, cis->lll.tx.phy);
		mpt_p = PDU_CIS_MAX_US(cis->lll.rx.max_octets, rx ? 1 : 0, cis->lll.rx.phy);

		se[i].length = mpt_c + EVENT_IFS_US + mpt_p + EVENT_MSS_US;
		max_se_length = MAX(max_se_length, se[i].length);

		/* Total number of subevents needed */
		se[i].total_count = MAX((cis->central.c_rtn + 1) * cis->lll.tx.burst_number,
					(cis->central.p_rtn + 1) * cis->lll.rx.burst_number);

		/* Initialize TX link */
		cis->lll.link_tx_free = &cis->lll.link_tx;

		memq_init(cis->lll.link_tx_free, &cis->lll.memq_tx.head, &cis->lll.memq_tx.tail);
		cis->lll.link_tx_free = NULL;
	}

	handle_iter = UINT16_MAX;
	uint32_t total_time = 0;

	/* 1) Prepare calculation of the flush timeout by adding up the total time needed to
	 *    transfer all payloads, including retransmissions.
	 */
	for (uint8_t i = 0; i < cis_count; i++) {
		cis = ll_conn_iso_stream_get_by_group(cig, &handle_iter);

		if (cig->central.packing == BT_ISO_PACKING_SEQUENTIAL) {
			/* Sequential CISes - add up the duration and set individual
			 * subinterval.
			 */
			total_time += se[i].total_count * se[i].length;
		} else {
			/* Interleaved CISes - find the largest total duration and
			 * set all subintervals to the largest SE_Length of any CIS x
			 * the number of interleaved CISes.
			 */
			total_time = MAX(total_time, se[i].total_count * cis->lll.sub_interval +
					 (i * cis->lll.sub_interval / cis_count));
		}
	}

	handle_iter = UINT16_MAX;
	cig_sync_delay = 0;

	/* 1) Calculate the flush timeout either by dividing the total time needed to transfer all,
	 *    payloads including retransmissions, and divide by the ISO_Interval (low latency
	 *    policy), or by dividing the Max_Transmission_Latency by the ISO_Interval (reliability
	 *    policy).
	 * 2) Calculate the number of subevents (NSE) by distributing total number of subevents into
	 *    FT ISO_intervals.
	 * 3) Calculate subinterval as either individual CIS subinterval (sequential), or the
	 *    largest SE_Length times number of CISes (interleaved). Min. subinterval is 400 us.
	 * 4) Calculate CIG_Sync_Delay
	 */
	for (uint8_t i = 0; i < cis_count; i++) {
		cis = ll_conn_iso_stream_get_by_group(cig, &handle_iter);

		if (!cig->central.test) {
#if defined(CONFIG_BT_CTLR_CONN_ISO_LOW_LATENCY_POLICY)
			/* Use symmetric flush timeout */
			cis->lll.tx.flush_timeout = ceiling_fraction(total_time, iso_interval_us);
			cis->lll.rx.flush_timeout = cis->lll.tx.flush_timeout;

#elif defined(CONFIG_BT_CTLR_CONN_ISO_RELIABILITY_POLICY)
			/* Utilize Max_Transmission_latency */
			if (cis->framed) {
				/* TL = CIG_Sync_Delay + FT x ISO_Interval + SDU_Interval.
				 * SDU_Interval <= CIG_Sync_Delay
				 */
				cis->lll.tx.flush_timeout =
					ceiling_fraction(cig->c_latency - cig->c_sdu_interval -
							iso_interval_us, iso_interval_us);
				cis->lll.rx.flush_timeout =
					ceiling_fraction(cig->p_latency - cig->p_sdu_interval -
							iso_interval_us, iso_interval_us);
			} else {
				/* TL = CIG_Sync_Delay + FT x ISO_Interval - SDU_Interval.
				 * SDU_Interval <= CIG_Sync_Delay
				 */
				cis->lll.tx.flush_timeout =
					ceiling_fraction(cig->c_latency + cig->c_sdu_interval -
							iso_interval_us, iso_interval_us);
				cis->lll.rx.flush_timeout =
					ceiling_fraction(cig->p_latency + cig->p_sdu_interval -
							iso_interval_us, iso_interval_us);
			}
#else
			LL_ASSERT(0);
#endif
			cis->lll.num_subevents = ceiling_fraction(se[i].total_count,
								  cis->lll.tx.flush_timeout);
		}

		if (cig->central.packing == BT_ISO_PACKING_SEQUENTIAL) {
			/* Accumulate CIG sync delay for sequential CISes */
			cis->lll.sub_interval = MAX(400, se[i].length);
			cig_sync_delay += cis->lll.num_subevents * cis->lll.sub_interval;
		} else {
			/* For interleaved CISes, offset each CIS by a fraction of a subinterval,
			 * positioning them evenly within the subinterval.
			 */
			cis->lll.sub_interval = MAX(400, cis_count * max_se_length);
			cig_sync_delay = MAX(cig_sync_delay,
					     (cis->lll.num_subevents * cis->lll.sub_interval) +
					     (i * cis->lll.sub_interval / cis_count));
		}
	}

	cig->sync_delay = cig_sync_delay;

	handle_iter = UINT16_MAX;
	c_max_latency = 0;
	p_max_latency = 0;

	/* 1) Calculate transport latencies for each CIS and validate against Max_Transport_Latency.
	 * 2) Lay out CISes by updating CIS_Sync_Delay, distributing according to the packing.
	 */
	for (uint8_t i = 0; i < cis_count; i++) {
		uint32_t c_latency;
		uint32_t p_latency;

		cis = ll_conn_iso_stream_get_by_group(cig, &handle_iter);

		if (cis->framed) {
			/* Transport_Latency = CIG_Sync_Delay + FT x ISO_Interval + SDU_Interval */
			c_latency = cig->sync_delay +
				    (cis->lll.tx.flush_timeout * iso_interval_us) +
				    cig->c_sdu_interval;
			p_latency = cig->sync_delay +
				    (cis->lll.rx.flush_timeout * iso_interval_us) +
				    cig->p_sdu_interval;

		} else {
			/* Transport_Latency = CIG_Sync_Delay + FT x ISO_Interval - SDU_Interval */
			c_latency = cig->sync_delay +
				    (cis->lll.tx.flush_timeout * iso_interval_us) -
				    cig->c_sdu_interval;
			p_latency = cig->sync_delay +
				    (cis->lll.rx.flush_timeout * iso_interval_us) -
				    cig->p_sdu_interval;
		}

		if (!cig->central.test) {
			/* Make sure specified Max_Transport_Latency is not exceeded */
			LL_ASSERT(c_latency <= cig->c_latency);
			LL_ASSERT(p_latency <= cig->p_latency);
		}

		c_max_latency = MAX(c_max_latency, c_latency);
		p_max_latency = MAX(p_max_latency, p_latency);

		if (cig->central.packing == BT_ISO_PACKING_SEQUENTIAL) {
			/* Distribute CISes sequentially */
			cis->sync_delay = cig_sync_delay;
			cig_sync_delay -= cis->lll.num_subevents * cis->lll.sub_interval;
		} else {
			/* Distribute CISes interleaved */
			cis->sync_delay = cig_sync_delay;
			cig_sync_delay -= (cis->lll.sub_interval / cis_count);
		}

		if (cis->lll.num_subevents <= 1) {
			cis->lll.sub_interval = 0;
		}
	}

	/* Update actual latency */
	cig->c_latency = c_max_latency;
	cig->p_latency = p_max_latency;

	cig->lll.num_cis = cis_count;

	return BT_HCI_ERR_SUCCESS;
}

/* Core 5.3 Vol 6, Part B section 7.8.100:
 * The HCI_LE_Remove_CIG command is used by the Central’s Host to remove the CIG
 * identified by CIG_ID.
 * This command shall delete the CIG_ID and also delete the Connection_Handles
 * of the CIS configurations stored in the CIG.
 * This command shall also remove the isochronous data paths that are associated
 * with the Connection_Handles of the CIS configurations.
 */
uint8_t ll_cig_remove(uint8_t cig_id)
{
	struct ll_conn_iso_stream *cis;
	struct ll_conn_iso_group *cig;
	uint16_t handle_iter;
	bool has_cis;

	cig = ll_conn_iso_group_get_by_id(cig_id);
	if (!cig) {
		/* Unknown CIG id */
		return BT_HCI_ERR_UNKNOWN_CONN_ID;
	}

	if (cig->started) {
		/* CIG is in active state */
		return BT_HCI_ERR_CMD_DISALLOWED;
	}

	handle_iter = UINT16_MAX;
	for (int i = 0; i < cig->cis_count; i++)  {
		struct ll_conn *conn;

		cis = ll_conn_iso_stream_get_by_group(cig, &handle_iter);
		if (!cis) {
			break;
		}

		conn = ll_connected_get(cis->lll.acl_handle);

		if (conn) {
			if (ull_lp_cc_is_active(conn)) {
				/* CIG creation is ongoing */
				return BT_HCI_ERR_CMD_DISALLOWED;
			}
		}
	}

	/* CIG exists and is not active */
	handle_iter = UINT16_MAX;
	has_cis = false;

	for (int i = 0; i < cig->cis_count; i++)  {
		cis = ll_conn_iso_stream_get_by_group(cig, &handle_iter);
		if (!cis) {
			break;
		}

		/* Remove data path and ISOAL sink/source associated with this CIS
		 * for both directions.
		 */
		ll_remove_iso_path(cis->lll.handle, BT_HCI_DATAPATH_DIR_CTLR_TO_HOST);
		ll_remove_iso_path(cis->lll.handle, BT_HCI_DATAPATH_DIR_HOST_TO_CTLR);

		has_cis = true;
	}

	if (has_cis) {
		/* Clear configuration only - let CIS disconnection release instance */
		cig->cis_count = 0;
	} else {
		/* No CISes associated with the CIG - release the instance */
		ll_conn_iso_group_release(cig);
	}

	return BT_HCI_ERR_SUCCESS;
}

uint8_t ll_cis_create_check(uint16_t cis_handle, uint16_t acl_handle)
{
	struct ll_conn *conn;

	conn = ll_connected_get(acl_handle);
	if (conn) {
		struct ll_conn_iso_stream *cis;

		/* Verify handle validity and association */
		cis = ll_conn_iso_stream_get(cis_handle);
		if (cis->lll.handle == cis_handle && cis->lll.acl_handle == acl_handle) {
			return BT_HCI_ERR_SUCCESS;
		}
	}

	return BT_HCI_ERR_CMD_DISALLOWED;
}

int ull_central_iso_init(void)
{
	return 0;
}

int ull_central_iso_reset(void)
{
	return 0;
}

uint8_t ull_central_iso_setup(uint16_t cis_handle,
			      uint32_t *cig_sync_delay,
			      uint32_t *cis_sync_delay,
			      uint32_t *cis_offset_min,
			      uint32_t *cis_offset_max,
			      uint16_t *conn_event_count,
			      uint8_t  *access_addr)
{
	struct ll_conn_iso_stream *cis;
	struct ll_conn_iso_group *cig;
	struct ll_conn *conn;
	uint16_t handle_iter;
	uint32_t cis_offset;
	uint16_t instant;
	int err;

	cis = ll_conn_iso_stream_get(cis_handle);
	if (!cis) {
		return BT_HCI_ERR_UNSPECIFIED;
	}

	cig = cis->group;
	if (!cig) {
		return BT_HCI_ERR_UNSPECIFIED;
	}

	conn = ll_conn_get(cis->lll.acl_handle);
	instant = MAX(*conn_event_count, ull_conn_event_counter(conn) + 1);

	handle_iter = UINT16_MAX;
	cis_offset = *cis_offset_min;

	/* Calculate offset for CIS */
	for (uint8_t i = 0; i < cig->cis_count; i++) {
		struct ll_conn_iso_stream *c;
		int16_t conn_events_since_ref;
		uint32_t iso_interval_us;
		uint32_t time_since_ref;

		c = ll_conn_iso_stream_get_by_group(cig, &handle_iter);
		if (c->cis_id != cis->cis_id && c->lll.active) {
			conn_events_since_ref = (int16_t)(instant - c->central.instant);
			LL_ASSERT(conn_events_since_ref > 0);

			time_since_ref = c->offset + conn_events_since_ref * conn->lll.interval *
					 CONN_INT_UNIT_US;
			iso_interval_us = cig->iso_interval * ISO_INT_UNIT_US;
			cis_offset = time_since_ref % iso_interval_us;
			break;
		}
	}

	cis->offset = cis_offset;
	cis->central.instant = instant;
	cis->lll.event_count = -1;

	/* Create access address */
	err = util_aa_le32(cis->lll.access_addr);
	LL_ASSERT(!err);

	/* Transfer to caller */
	*cig_sync_delay = cig->sync_delay;
	*cis_sync_delay = cis->sync_delay;
	*cis_offset_min = cis->offset;
	memcpy(access_addr, cis->lll.access_addr, sizeof(cis->lll.access_addr));

	*conn_event_count = instant;

	return 0;
}

uint16_t ull_central_iso_cis_offset_get(uint16_t cis_handle, uint32_t *cis_offset_min,
					uint32_t *cis_offset_max)
{
	struct ll_conn_iso_stream *cis;
	struct ll_conn *conn;

	cis = ll_conn_iso_stream_get(cis_handle);
	LL_ASSERT(cis);

	conn = ll_conn_get(cis->lll.acl_handle);

	if (cis_offset_min && cis_offset_max) {
		struct ll_conn_iso_group *cig;

		cig  = cis->group;

		/* Provide CIS offset range
		 * CIS_Offset_Max < (connInterval - (CIG_Sync_Delay + T_MSS))
		 */
		*cis_offset_max = (conn->lll.interval * CONN_INT_UNIT_US) - cig->sync_delay;
		*cis_offset_min = MAX(400, EVENT_OVERHEAD_CIS_SETUP_US);
	}

	cis->central.instant = ull_conn_event_counter(conn) + 3;
	return cis->central.instant;
}
