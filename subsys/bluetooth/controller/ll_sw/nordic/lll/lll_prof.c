/*
 * Copyright (c) 2018-2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdint.h>
#include <stddef.h>
#include <errno.h>

#include <zephyr/toolchain.h>

#include "hal/ccm.h"
#include "hal/radio.h"

#include "util/memq.h"

#include "pdu.h"

#include "lll.h"

static int send(struct node_rx_pdu *rx);
static inline void sample(uint32_t *timestamp);
static inline void delta(uint32_t timestamp, uint8_t *cputime);

static uint32_t timestamp_radio;
static uint32_t timestamp_lll;
static uint32_t timestamp_ull_high;
static uint32_t timestamp_ull_low;
static uint8_t cputime_radio;
static uint8_t cputime_lll;
static uint8_t cputime_ull_high;
static uint8_t cputime_ull_low;
static uint8_t latency_min = (uint8_t) -1;
static uint8_t latency_max;
static uint8_t latency_prev;
static uint8_t cputime_min = (uint8_t) -1;
static uint8_t cputime_max;
static uint8_t cputime_prev;
static uint32_t timestamp_latency;

void lll_prof_enter_radio(void)
{
	sample(&timestamp_radio);
}

void lll_prof_exit_radio(void)
{
	delta(timestamp_radio, &cputime_radio);
}

void lll_prof_enter_lll(void)
{
	sample(&timestamp_lll);
}

void lll_prof_exit_lll(void)
{
	delta(timestamp_lll, &cputime_lll);
}

void lll_prof_enter_ull_high(void)
{
	sample(&timestamp_ull_high);
}

void lll_prof_exit_ull_high(void)
{
	delta(timestamp_ull_high, &cputime_ull_high);
}

void lll_prof_enter_ull_low(void)
{
	sample(&timestamp_ull_low);
}

void lll_prof_exit_ull_low(void)
{
	delta(timestamp_ull_low, &cputime_ull_low);
}

void lll_prof_latency_capture(void)
{
	/* sample the packet timer, use it to calculate ISR latency
	 * and generate the profiling event at the end of the ISR.
	 */
	radio_tmr_sample();
}

#if defined(HAL_RADIO_GPIO_HAVE_PA_PIN)
static uint32_t timestamp_radio_end;

uint32_t lll_prof_radio_end_backup(void)
{
	/* PA enable is overwriting packet end used in ISR profiling, hence
	 * back it up for later use.
	 */
	timestamp_radio_end = radio_tmr_end_get();

	return timestamp_radio_end;
}
#endif /* !HAL_RADIO_GPIO_HAVE_PA_PIN */

void lll_prof_cputime_capture(void)
{
	/* get the ISR latency sample */
	timestamp_latency = radio_tmr_sample_get();

	/* sample the packet timer again, use it to calculate ISR execution time
	 * and use it in profiling event
	 */
	radio_tmr_sample();
}

void lll_prof_send(void)
{
	struct node_rx_pdu *rx;

	/* Generate only if spare node rx is available */
	rx = ull_pdu_rx_alloc_peek(3);
	if (rx) {
		(void)send(NULL);
	}
}

struct node_rx_pdu *lll_prof_reserve(void)
{
	struct node_rx_pdu *rx;

	rx = ull_pdu_rx_alloc_peek(3);
	if (!rx) {
		return NULL;
	}

	ull_pdu_rx_alloc();

	return rx;
}

void lll_prof_reserve_send(struct node_rx_pdu *rx)
{
	if (rx) {
		int err;

		err = send(rx);
		if (err) {
			rx->hdr.type = NODE_RX_TYPE_PROFILE;

			ull_rx_put_sched(rx->hdr.link, rx);
		}
	}
}

static int send(struct node_rx_pdu *rx)
{
	uint8_t latency, cputime, prev;
	struct pdu_data *pdu;
	struct profile *p;
	uint8_t chg = 0U;

	/* calculate the elapsed time in us since on-air radio packet end
	 * to ISR entry
	 */
#if defined(HAL_RADIO_GPIO_HAVE_PA_PIN)
	latency = timestamp_latency - timestamp_radio_end;
#else /* !HAL_RADIO_GPIO_HAVE_PA_PIN */
	latency = timestamp_latency - radio_tmr_end_get();
#endif /* !HAL_RADIO_GPIO_HAVE_PA_PIN */

	/* check changes in min, avg and max of latency */
	if (latency > latency_max) {
		latency_max = latency;
		chg = 1U;
	}
	if (latency < latency_min) {
		latency_min = latency;
		chg = 1U;
	}

	/* check for +/- 1us change */
	prev = ((uint16_t)latency_prev + latency) >> 1;
	if (prev != latency_prev) {
		latency_prev = latency;
		chg = 1U;
	}

	/* calculate the elapsed time in us since ISR entry */
	cputime = radio_tmr_sample_get() - timestamp_latency;

	/* check changes in min, avg and max */
	if (cputime > cputime_max) {
		cputime_max = cputime;
		chg = 1U;
	}

	if (cputime < cputime_min) {
		cputime_min = cputime;
		chg = 1U;
	}

	/* check for +/- 1us change */
	prev = ((uint16_t)cputime_prev + cputime) >> 1;
	if (prev != cputime_prev) {
		cputime_prev = cputime;
		chg = 1U;
	}

	/* generate event if any change */
	if (!chg) {
		return -ENODATA;
	}

	/* Allocate if not already allocated */
	if (!rx) {
		rx = ull_pdu_rx_alloc();
		if (!rx) {
			return -ENOMEM;
		}
	}

	/* Generate event with the allocated node rx */
	rx->hdr.type = NODE_RX_TYPE_PROFILE;
	rx->hdr.handle = NODE_RX_HANDLE_INVALID;

	pdu = (void *)rx->pdu;
	p = &pdu->profile;
	p->lcur = latency;
	p->lmin = latency_min;
	p->lmax = latency_max;
	p->cur = cputime;
	p->min = cputime_min;
	p->max = cputime_max;
	p->radio = cputime_radio;
	p->lll = cputime_lll;
	p->ull_high = cputime_ull_high;
	p->ull_low = cputime_ull_low;

	ull_rx_put_sched(rx->hdr.link, rx);

	return 0;
}

static inline void sample(uint32_t *timestamp)
{
	radio_tmr_sample();
	*timestamp = radio_tmr_sample_get();
}

static inline void delta(uint32_t timestamp, uint8_t *cputime)
{
	uint32_t delta;

	radio_tmr_sample();
	delta = radio_tmr_sample_get() - timestamp;
	if (delta < UINT8_MAX && delta > *cputime) {
		*cputime = delta;
	}
}
