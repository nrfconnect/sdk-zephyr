/*
 * Copyright (c) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <ztest.h>
#include <cmsis_os2.h>

#define ONESHOT_TIME_TICKS	100
#define PERIOD_TICKS		50
#define NUM_PERIODS		5

u32_t num_oneshots_executed;
u32_t num_periods_executed;

const osTimerAttr_t timer_attr = {
	"myTimer",
	0,
	NULL,
	0U
};

void Timer1_Callback(void *arg)
{
	u32_t Tmr = *(u32_t *)arg;

	num_oneshots_executed++;
	TC_PRINT("oneshot_callback (Timer %d) = %d\n",
		 Tmr, num_oneshots_executed);
}

void Timer2_Callback(void *arg)
{
	u32_t Tmr = *(u32_t *)arg;

	num_periods_executed++;
	TC_PRINT("periodic_callback (Timer %d) = %d\n",
		 Tmr, num_periods_executed);
}

void test_timer(void)
{
	osTimerId_t id1;
	osTimerId_t id2;
	u32_t  exec1;
	u32_t  exec2;
	osStatus_t status;
	u32_t timerDelay;
	const char *name;

	/* Create one-shot timer */
	exec1 = 1;
	id1 = osTimerNew(Timer1_Callback, osTimerOnce, &exec1, &timer_attr);
	zassert_true(id1 != NULL, "error creating one-shot timer");

	name = osTimerGetName(id1);
	zassert_true(strcmp(timer_attr.name, name) == 0,
		"Error getting Timer name");

	/* Stop the timer before start */
	status = osTimerStop(id1);
	zassert_true(status == osErrorResource,
		"error while stopping non-active timer");

	timerDelay = ONESHOT_TIME_TICKS;
	status = osTimerStart(id1, timerDelay);
	zassert_true(status == osOK, "error starting one-shot timer");

	zassert_equal(osTimerIsRunning(id1), 1, "Error: Timer not running");

	/* Timer should fire only once if setup in one shot
	 * mode. Wait for 3 times the one-shot time to see
	 * if it fires more than once.
	 */
	osDelay(timerDelay*3 + 10);
	zassert_true(num_oneshots_executed == 1,
			"error setting up one-shot timer");

	status = osTimerStop(id1);
	zassert_true(status == osOK, "error stopping one-shot timer");

	status = osTimerDelete(id1);
	zassert_true(status == osOK, "error deleting one-shot timer");

	/* Create periodic timer */
	exec2 = 2;
	id2 = osTimerNew(Timer2_Callback, osTimerPeriodic, &exec2, NULL);
	zassert_true(id2 != NULL, "error creating periodic timer");

	zassert_equal(osTimerIsRunning(id2), 0, "Error: Timer is running");

	timerDelay = PERIOD_TICKS;
	status = osTimerStart(id2, timerDelay);
	zassert_true(status == osOK, "error starting periodic timer");

	/* Timer should fire periodically if setup in periodic
	 * mode. Wait for NUM_PERIODS periods to see if it is
	 * fired NUM_PERIODS times.
	 */
	osDelay(timerDelay*NUM_PERIODS + 10);

	/* The first firing of the timer should be ignored.
	 * Hence checking for NUM_PERIODS + 1.
	 */
	zassert_true(num_periods_executed == NUM_PERIODS + 1,
			"error setting up periodic timer");

	/* Delete the timer before stop */
	status = osTimerDelete(id2);
	zassert_true(status == osOK, "error deleting periodic timer");
}
