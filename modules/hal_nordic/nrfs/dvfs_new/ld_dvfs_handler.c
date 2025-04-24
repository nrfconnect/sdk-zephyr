/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ld_dvfs_handler.h"

#include <hal/nrf_hsfll.h>
#include <dvfs_oppoint.h>
//#include <nrfs_dvfs.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(LD_DVFS_NEW_LIB, CONFIG_LOCAL_DOMAIN_DVFS_NEW_LIB_LOG_LEVEL);

/*TODO: get rid of nrfs dependency, oppoint*/
#include <sdfw/sdfw_services/dvfs_service.h>

static volatile enum dvfs_frequency_setting current_freq_setting;
static volatile enum dvfs_frequency_setting requested_freq_setting;
static dvfs_service_handler_callback dvfs_frequency_change_applied_clb;

/*
 * wait max 500ms with 10us intervals for hsfll freq change event
 */
#define HSFLL_FREQ_CHANGE_MAX_DELAY_MS 500UL
#define HSFLL_FREQ_CHANGE_CHECK_INTERVAL_US 10
#define HSFLL_FREQ_CHANGE_CHECK_MAX_ATTEMPTS                                                       \
	((HSFLL_FREQ_CHANGE_MAX_DELAY_MS) * (USEC_PER_MSEC) / (HSFLL_FREQ_CHANGE_CHECK_INTERVAL_US))

/**
 * @brief Configure hsfll depending on selected oppoint
 *
 * @param enum oppoint target operation point
 * @return 0 value indicates no error.
 * @return -EINVAL invalid oppoint or domain.
 * @return -ETIMEDOUT frequency change took more than HSFLL_FREQ_CHANGE_MAX_DELAY_MS
 */
static int32_t ld_dvfs_configure_hsfll(enum dvfs_frequency_setting oppoint)
{
	nrf_hsfll_trim_t hsfll_trim = {};

	if (oppoint >= DVFS_FREQ_COUNT) {
		LOG_ERR("Not valid oppoint %d", oppoint);
		return -EINVAL;
	}

	uint8_t freq_trim = get_dvfs_oppoint_data(oppoint)->new_f_trim_entry;

#if defined(NRF_APPLICATION)
	hsfll_trim.vsup	  = NRF_FICR->TRIM.APPLICATION.HSFLL.TRIM.VSUP;
	hsfll_trim.coarse = NRF_FICR->TRIM.APPLICATION.HSFLL.TRIM.COARSE[freq_trim];
	hsfll_trim.fine	  = NRF_FICR->TRIM.APPLICATION.HSFLL.TRIM.FINE[freq_trim];
#else
	hsfll_trim.vsup	  = NRF_FICR->TRIM.SECURE.HSFLL.TRIM.VSUP;
	hsfll_trim.coarse = NRF_FICR->TRIM.SECURE.HSFLL.TRIM.COARSE[freq_trim];
	hsfll_trim.fine	  = NRF_FICR->TRIM.SECURE.HSFLL.TRIM.FINE[freq_trim];
#endif

#if defined(CONFIG_NRFS_LOCAL_DOMAIN_DVFS_TEST)
	LOG_DBG("%s oppoint: %d", __func__, oppoint);
	LOG_DBG("REGW: NRF_HSFLL->MIRROR 0x%x, V: 0x%x", (uint32_t)&NRF_HSFLL->MIRROR, 1);
	LOG_DBG("REGW: NRF_HSFLL->TRIM.COARSE 0x%x, V: 0x%x",
		(uint32_t)&NRF_HSFLL->TRIM.COARSE,
		hsfll_trim.coarse);
	LOG_DBG("REGW: NRF_HSFLL->TRIM.FINE 0x%x, V: 0x%x",
		(uint32_t)&NRF_HSFLL->TRIM.FINE,
		hsfll_trim.fine);
	LOG_DBG("REGW: NRF_HSFLL->MIRROR 0x%x, V: 0x%x", (uint32_t)&NRF_HSFLL->MIRROR, 0);

	LOG_DBG("REGW: NRF_HSFLL->CLOCKCTRL.MULT 0x%x, V: 0x%x",
		(uint32_t)&NRF_HSFLL->CLOCKCTRL.MULT,
		get_dvfs_oppoint_data(oppoint)->new_f_mult);

	LOG_DBG("REGW: NRF_HSFLL->NRF_HSFLL_TASK_FREQ_CHANGE 0x%x, V: 0x%x",
		(uint32_t)NRF_HSFLL + NRF_HSFLL_TASK_FREQ_CHANGE,
		0x1);
	return 0;
#else

	nrf_hsfll_trim_set(NRF_HSFLL, &hsfll_trim);
	nrf_barrier_w();

	nrf_hsfll_clkctrl_mult_set(NRF_HSFLL, get_dvfs_oppoint_data(oppoint)->new_f_mult);
	nrf_hsfll_task_trigger(NRF_HSFLL, NRF_HSFLL_TASK_FREQ_CHANGE);
	/* Trigger hsfll task one more time, SEE PAC-4078 */
	nrf_hsfll_task_trigger(NRF_HSFLL, NRF_HSFLL_TASK_FREQ_CHANGE);

	bool hsfll_freq_changed = false;

	NRFX_WAIT_FOR(nrf_hsfll_event_check(NRF_HSFLL, NRF_HSFLL_EVENT_FREQ_CHANGED),
		      HSFLL_FREQ_CHANGE_CHECK_MAX_ATTEMPTS,
		      HSFLL_FREQ_CHANGE_CHECK_INTERVAL_US,
		      hsfll_freq_changed);

	if (hsfll_freq_changed) {
		return 0;
	}

	return -ETIMEDOUT;
#endif
}


static enum dvfs_frequency_setting dvfs_service_handler_get_current_oppoint(void)
{
	LOG_DBG("Current LD freq setting: %d", current_freq_setting);
	return current_freq_setting;
}

static void dvfs_service_handler_error(int err)
{
	if (err != 0) {
		LOG_ERR("Failed with error: %d", err);
	}
}

static bool dvfs_service_handler_freq_setting_allowed(enum dvfs_frequency_setting freq_setting)
{
	if (freq_setting == DVFS_FREQ_HIGH || freq_setting == DVFS_FREQ_MEDLOW ||
	    freq_setting == DVFS_FREQ_LOW) {
		return true;
	}

	return false;
}

/* Function to check if current operation is down-scaling */
static bool dvfs_service_handler_is_downscaling(enum dvfs_frequency_setting target_freq_setting)
{
	if (dvfs_service_handler_freq_setting_allowed(target_freq_setting)) {
		LOG_DBG("Checking if downscaling %s",
			(dvfs_service_handler_get_current_oppoint() < target_freq_setting) ? "YES" :
											     "NO");
		return dvfs_service_handler_get_current_oppoint() < target_freq_setting;
	}

	return false;
}

/* Function handling steps for scaling preparation. */
static void dvfs_service_handler_prepare_to_scale(enum dvfs_frequency_setting oppoint_freq)
{
	LOG_DBG("Prepare to scale, oppoint freq %d", oppoint_freq);
	enum dvfs_frequency_setting new_oppoint	    = oppoint_freq;
	enum dvfs_frequency_setting current_oppoint = dvfs_service_handler_get_current_oppoint();

	if (new_oppoint == current_oppoint) {
		LOG_DBG("New oppoint is same as previous, no change");
	} else {

		if (dvfs_service_handler_is_downscaling(new_oppoint)) {
			int32_t err = ld_dvfs_configure_hsfll(new_oppoint);

			if (err != 0) {
				dvfs_service_handler_error(err);
			}
		}
	}
}

/* Function to set hsfll to highest frequency when switched to ABB. */
static int32_t dvfs_service_handler_set_initial_hsfll_config(void)
{
	int32_t err = ld_dvfs_configure_hsfll(DVFS_FREQ_HIGH);

	current_freq_setting = DVFS_FREQ_HIGH;
	requested_freq_setting = DVFS_FREQ_HIGH;
	if (err != 0) {
		dvfs_service_handler_error(err);
	}

	return err;
}

static int ld_dvfs_handler_init(void)
{
	LOG_DBG("LD DVFS handler init");
	int32_t err = dvfs_service_handler_set_initial_hsfll_config();

	return err;
}
SYS_INIT(ld_dvfs_handler_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);

/* Update MDK variable which is used by nrfx_coredep_delay_us (k_busy_wait). */
static void dvfs_service_update_core_clock(enum dvfs_frequency_setting oppoint_freq)
{
	extern uint32_t SystemCoreClock;

	SystemCoreClock = oppoint_freq == DVFS_FREQ_HIGH ? 320000000 :
			  oppoint_freq == DVFS_FREQ_MEDLOW ? 128000000 : 64000000;
}

/* Perform scaling finnish procedure. */
static void dvfs_service_handler_scaling_finish(enum dvfs_frequency_setting oppoint_freq)
{

	LOG_DBG("Scaling finnish oppoint freq %d", oppoint_freq);
	if (!dvfs_service_handler_is_downscaling(oppoint_freq)) {
		int32_t err = ld_dvfs_configure_hsfll(oppoint_freq);

		if (err != 0) {
			dvfs_service_handler_error(err);
		}
	}

	current_freq_setting = oppoint_freq;
	dvfs_service_update_core_clock(oppoint_freq);
	LOG_DBG("Current LD freq setting: %d", current_freq_setting);
	if (dvfs_frequency_change_applied_clb) {
		dvfs_frequency_change_applied_clb(current_freq_setting);
	}
}

int32_t dvfs_service_handler_change_freq_setting(enum dvfs_frequency_setting freq_setting)
{
	static bool change_in_progress = false;
	if (change_in_progress) {
		LOG_ERR("Change in progress, please wait.");
		return -EBUSY;
	}

	if (!dvfs_service_handler_freq_setting_allowed(freq_setting)) {
		LOG_ERR("Requested frequency setting %d not supported.", freq_setting);
		return -ENXIO;
	}

	if(freq_setting == current_freq_setting) {
		LOG_DBG("Requested frequency setting %d is same as current.", freq_setting);
		return 0;
	}

	change_in_progress = true;

	requested_freq_setting = freq_setting;

	dvfs_service_handler_prepare_to_scale(requested_freq_setting);

	int status = ssf_dvfs_set_oppoint(requested_freq_setting);

	if (status != 0) {
		LOG_ERR("Failed to set DVFS frequency: %d", status);
		return status;
	}

	dvfs_service_handler_scaling_finish(requested_freq_setting);

	change_in_progress = false;

	return 0;
}

void dvfs_service_handler_register_freq_setting_applied_callback(dvfs_service_handler_callback clb)
{
	if (clb) {
		LOG_DBG("Registered frequency applied callback");
		dvfs_frequency_change_applied_clb = clb;
	} else {
		LOG_ERR("Invalid callback function provided!");
	}
}
