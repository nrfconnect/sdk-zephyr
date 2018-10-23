/*
 * Copyright (c) 2016-2018 Nordic Semiconductor ASA
 * Copyright (c) 2016 Vinayak Kariappa Chettimada
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdbool.h>
#include <misc/dlist.h>
#include <misc/mempool_base.h>

#include "hal/cntr.h"

#include "util/memq.h"
#include "util/mayfly.h"

#include "ticker/ticker.h"

#define LOG_MODULE_NAME bt_ctlr_nrf5_ticker
#include "common/log.h"
#include "hal/debug.h"

#if defined(CONFIG_BT_LL_SW)
#define TICKER_MAYFLY_CALL_ID_TRIGGER MAYFLY_CALL_ID_0
#define TICKER_MAYFLY_CALL_ID_WORKER  MAYFLY_CALL_ID_0
#define TICKER_MAYFLY_CALL_ID_JOB     MAYFLY_CALL_ID_1
#define TICKER_MAYFLY_CALL_ID_PROGRAM MAYFLY_CALL_ID_PROGRAM
static u8_t const caller_id_lut[] = {
	TICKER_CALL_ID_WORKER,
	TICKER_CALL_ID_JOB,
	TICKER_CALL_ID_NONE,
	TICKER_CALL_ID_PROGRAM
};
#else
#error Unknown LL variant.
#endif

u8_t hal_ticker_instance0_caller_id_get(u8_t user_id)
{
	u8_t caller_id;

	LL_ASSERT(user_id < sizeof(caller_id_lut));

	caller_id = caller_id_lut[user_id];
	LL_ASSERT(caller_id != TICKER_CALL_ID_NONE);

	return caller_id;
}

void hal_ticker_instance0_sched(u8_t caller_id, u8_t callee_id, u8_t chain,
				void *instance)
{
	/* return value not checked as we allow multiple calls to schedule
	 * before being actually needing the work to complete before new
	 * schedule.
	 */
	switch (caller_id) {
	case TICKER_CALL_ID_TRIGGER:
		switch (callee_id) {
		case TICKER_CALL_ID_WORKER:
		{
			static memq_link_t link;
			static struct mayfly m = {0, 0, &link, NULL,
						  ticker_worker};

			m.param = instance;

			mayfly_enqueue(TICKER_MAYFLY_CALL_ID_TRIGGER,
				       TICKER_MAYFLY_CALL_ID_WORKER,
				       chain,
				       &m);
		}
		break;

		default:
			LL_ASSERT(0);
			break;
		}
		break;

	case TICKER_CALL_ID_WORKER:
		switch (callee_id) {
		case TICKER_CALL_ID_JOB:
		{
			static memq_link_t link;
			static struct mayfly m = {0, 0, &link, NULL,
						  ticker_job};

			m.param = instance;

			mayfly_enqueue(TICKER_MAYFLY_CALL_ID_WORKER,
				       TICKER_MAYFLY_CALL_ID_JOB,
				       chain,
				       &m);
		}
		break;

		default:
			LL_ASSERT(0);
			break;
		}
		break;

	case TICKER_CALL_ID_JOB:
		switch (callee_id) {
		case TICKER_CALL_ID_WORKER:
		{
			static memq_link_t link;
			static struct mayfly m = {0, 0, &link, NULL,
						  ticker_worker};

			m.param = instance;

			mayfly_enqueue(TICKER_MAYFLY_CALL_ID_JOB,
				       TICKER_MAYFLY_CALL_ID_WORKER,
				       chain,
				       &m);
		}
		break;

		case TICKER_CALL_ID_JOB:
		{
			static memq_link_t link;
			static struct mayfly m = {0, 0, &link, NULL,
						  ticker_job};

			m.param = instance;

			mayfly_enqueue(TICKER_MAYFLY_CALL_ID_JOB,
				       TICKER_MAYFLY_CALL_ID_JOB,
				       chain,
				       &m);
		}
		break;

		default:
			LL_ASSERT(0);
			break;
		}
		break;

	case TICKER_CALL_ID_PROGRAM:
		switch (callee_id) {
		case TICKER_CALL_ID_JOB:
		{
			static memq_link_t link;
			static struct mayfly m = {0, 0, &link, NULL,
						  ticker_job};

			m.param = instance;

			/* TODO: scheduler lock, if preemptive threads used */
			mayfly_enqueue(TICKER_MAYFLY_CALL_ID_PROGRAM,
				       TICKER_MAYFLY_CALL_ID_JOB,
				       chain,
				       &m);
		}
		break;

		default:
			LL_ASSERT(0);
			break;
		}
		break;

	default:
		LL_ASSERT(0);
		break;
	}
}

void hal_ticker_instance0_trigger_set(u32_t value)
{
	cntr_cmp_set(0, value);
}
