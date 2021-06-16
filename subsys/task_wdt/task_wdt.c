/*
 * Copyright (c) 2020 Libre Solar Technologies GmbH
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "task_wdt/task_wdt.h"

#include <drivers/watchdog.h>
#include <sys/reboot.h>
#include <device.h>
#include <errno.h>

#define LOG_LEVEL CONFIG_WDT_LOG_LEVEL
#include <logging/log.h>
LOG_MODULE_REGISTER(task_wdt);

/*
 * This dummy channel is used to continue feeding the hardware watchdog if the
 * task watchdog timeouts are too long for regular updates
 */
#define TASK_WDT_BACKGROUND_CHANNEL (-1)

/*
 * Task watchdog channel data
 */
struct task_wdt_channel {
	/* period in milliseconds used to reset the timeout, set to 0 to
	 * indicate that the channel is available
	 */
	uint32_t reload_period;
	/* abs. ticks when this channel expires (updated by task_wdt_feed) */
	int64_t timeout_abs_ticks;
	/* user data passed to the callback function */
	void *user_data;
	/* function to be called when watchdog timer expired */
	task_wdt_callback_t callback;
};

/* array of all task watchdog channels */
static struct task_wdt_channel channels[CONFIG_TASK_WDT_CHANNELS];

/* timer used for watchdog handling */
static struct k_timer timer;

#ifdef CONFIG_TASK_WDT_HW_FALLBACK
/* pointer to the hardware watchdog used as a fallback */
static const struct device *hw_wdt_dev;
static int hw_wdt_channel;
static bool hw_wdt_started;
#endif

/**
 * @brief Task watchdog timer callback.
 *
 * If the device operates as intended, this function will never be called,
 * as the timer is continuously restarted with the next due timeout in the
 * task_wdt_feed() function.
 *
 * If all task watchdogs have longer timeouts than the hardware watchdog,
 * this function is called regularly (via the background channel). This
 * should be avoided by setting CONFIG_TASK_WDT_MIN_TIMEOUT to the minimum
 * task watchdog timeout used in the application.
 *
 * @param timer_id Pointer to the timer which called the function
 */
static void task_wdt_trigger(struct k_timer *timer_id)
{
	int channel_id = (int)k_timer_user_data_get(timer_id);

#ifdef CONFIG_TASK_WDT_HW_FALLBACK
	if (channel_id == TASK_WDT_BACKGROUND_CHANNEL) {
		if (hw_wdt_dev) {
			wdt_feed(hw_wdt_dev, 0);
		}
		return;
	}
#endif

	if (channels[channel_id].reload_period == 0) {
		/* channel was deleted */
		return;
	} else if (channels[channel_id].callback) {
		channels[channel_id].callback(channel_id,
			channels[channel_id].user_data);
	} else {
		sys_reboot(SYS_REBOOT_COLD);
	}
}

int task_wdt_init(const struct device *hw_wdt)
{
	if (hw_wdt) {
#ifdef CONFIG_TASK_WDT_HW_FALLBACK
		struct wdt_timeout_cfg wdt_config;

		wdt_config.flags = WDT_FLAG_RESET_SOC;
		wdt_config.window.min = 0U;
		wdt_config.window.max = CONFIG_TASK_WDT_MIN_TIMEOUT +
			CONFIG_TASK_WDT_HW_FALLBACK_DELAY;
		wdt_config.callback = NULL;

		hw_wdt_dev = hw_wdt;
		hw_wdt_channel = wdt_install_timeout(hw_wdt_dev, &wdt_config);
#else
		return -ENOTSUP;
#endif
	}

	k_timer_init(&timer, task_wdt_trigger, NULL);

	return 0;
}

int task_wdt_add(uint32_t reload_period, task_wdt_callback_t callback,
		 void *user_data)
{
	if (reload_period == 0) {
		return -EINVAL;
	}

	/* look for unused channel (reload_period set to 0) */
	for (int id = 0; id < ARRAY_SIZE(channels); id++) {
		if (channels[id].reload_period == 0) {
			channels[id].reload_period = reload_period;
			channels[id].user_data = user_data;
			channels[id].timeout_abs_ticks = K_TICKS_FOREVER;
			channels[id].callback = callback;
			task_wdt_feed(id);

#ifdef CONFIG_TASK_WDT_HW_FALLBACK
			if (!hw_wdt_started && hw_wdt_dev) {
				/* also start fallback hw wdt */
				wdt_setup(hw_wdt_dev,
					WDT_OPT_PAUSE_HALTED_BY_DBG);
				hw_wdt_started = true;
			}
#endif
			return id;
		}
	}

	return -ENOMEM;
}

int task_wdt_delete(int channel_id)
{
	if (channel_id < 0 || channel_id >= ARRAY_SIZE(channels)) {
		return -EINVAL;
	}

	channels[channel_id].reload_period = 0;

	return 0;
}

int task_wdt_feed(int channel_id)
{
	int64_t current_ticks;
	int next_channel_id;	/* channel which will time out next */
	int64_t next_timeout;   /* timeout in absolute ticks of this channel */

	if (channel_id < 0 || channel_id >= ARRAY_SIZE(channels)) {
		return -EINVAL;
	}

	/*
	 * We need a critical section instead of a mutex while updating the
	 * channels array in order to prevent priority inversion. Otherwise,
	 * a low priority thread could be preempted before releasing the mutex
	 * and block a high priority thread that wants to feed its task wdt.
	 */
	k_sched_lock();

	current_ticks = sys_clock_tick_get();

	/* feed the specified channel */
	channels[channel_id].timeout_abs_ticks = current_ticks +
		k_ms_to_ticks_ceil64(channels[channel_id].reload_period);

#ifdef CONFIG_TASK_WDT_HW_FALLBACK
	next_channel_id = TASK_WDT_BACKGROUND_CHANNEL;
	next_timeout = current_ticks +
		k_ms_to_ticks_ceil64(CONFIG_TASK_WDT_MIN_TIMEOUT);
#else
	next_channel_id = 0;
	next_timeout = INT64_MAX;
#endif

	/* find minimum timeout of all channels */
	for (int id = 0; id < ARRAY_SIZE(channels); id++) {
		if (channels[id].reload_period != 0 &&
		    channels[id].timeout_abs_ticks < next_timeout) {
			next_channel_id = id;
			next_timeout = channels[id].timeout_abs_ticks;
		}
	}

	/* update task wdt kernel timer */
	k_timer_user_data_set(&timer, (void *)next_channel_id);
	k_timer_start(&timer, K_TIMEOUT_ABS_TICKS(next_timeout),
		K_TIMEOUT_ABS_TICKS(next_timeout));

#ifdef CONFIG_TASK_WDT_HW_FALLBACK
	if (hw_wdt_dev) {
		wdt_feed(hw_wdt_dev, 0);
	}
#endif

	k_sched_unlock();

	return 0;
}
