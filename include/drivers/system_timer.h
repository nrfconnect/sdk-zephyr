/*
 * Copyright (c) 2015 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Timer driver API
 *
 *
 * Declare API implemented by system timer driver and used by kernel components.
 */

#ifndef ZEPHYR_INCLUDE_DRIVERS_SYSTEM_TIMER_H_
#define ZEPHYR_INCLUDE_DRIVERS_SYSTEM_TIMER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <device.h>
#include <stdbool.h>

/**
 * @brief Initialize system clock driver
 *
 * The system clock is a Zephyr device created globally.  This is its
 * initialization callback.  It is a weak symbol that will be
 * implemented as a noop if undefined in the clock driver.
 */
extern int z_clock_driver_init(struct device *device);

/**
 * @brief Initialize system clock driver
 *
 * The system clock is a Zephyr device created globally.  This is its
 * device control callback, used in a few devices for power
 * management.  It is a weak symbol that will be implemented as a noop
 * if undefined in the clock driver.
 */
extern int z_clock_device_ctrl(struct device *device, u32_t ctrl_command,
			       void *context, device_pm_cb cb, void *arg);

/**
 * @brief Set system clock timeout
 *
 * Informs the system clock driver that the next needed call to
 * z_clock_announce() will not be until the specified number of ticks
 * from the the current time have elapsed.  Note that spurious calls
 * to z_clock_announce() are allowed (i.e. it's legal to announce
 * every tick and implement this function as a noop), the requirement
 * is that one tick announcement should occur within one tick BEFORE
 * the specified expiration (that is, passing ticks==1 means "announce
 * the next tick", this convention was chosen to match legacy usage).
 * A ticks value of zero (or even negative) is legal, it simply
 * indicates the kernel would like the next tick announcement as soon
 * as possible.
 *
 * Note that ticks can also be passed the special value K_FOREVER,
 * indicating that no future timer interrupts are expected or required
 * and that the system is permitted to enter an indefinite sleep even
 * if this could cause rollover of the internal counter (i.e. the
 * system uptime counter is allowed to be wrong, see
 * k_enable_sys_clock_always_on().
 *
 * Note also that it is conventional for the kernel to pass INT_MAX
 * for ticks if it wants to preserve the uptime tick count but doesn't
 * have a specific event to await.  The intent here is that the driver
 * will schedule any needed timeout as far into the future as
 * possible.  For the specific case of INT_MAX, the next call to
 * z_clock_announce() may occur at any point in the future, not just
 * at INT_MAX ticks.  But the correspondence between the announced
 * ticks and real-world time must be correct.
 *
 * A final note about SMP: note that the call to z_clock_set_timeout()
 * is made on any CPU, and reflects the next timeout desired globally.
 * The resulting calls(s) to z_clock_announce() must be properly
 * serialized by the driver such that a given tick is announced
 * exactly once across the system.  The kernel does not (cannot,
 * really) attempt to serialize things by "assigning" timeouts to
 * specific CPUs.
 *
 * @param ticks Timeout in tick units
 * @param idle Hint to the driver that the system is about to enter
 *        the idle state immediately after setting the timeout
 */
extern void z_clock_set_timeout(s32_t ticks, bool idle);

/**
 * @brief Timer idle exit notification
 *
 * This notifies the timer driver that the system is exiting the idle
 * and allows it to do whatever bookkeeping is needed to restore timer
 * operation and compute elapsed ticks.
 *
 * @note Legacy timer drivers also use this opportunity to call back
 * into z_clock_announce() to notify the kernel of expired ticks.
 * This is allowed for compatibility, but not recommended.  The kernel
 * will figure that out on its own.
 */
extern void z_clock_idle_exit(void);

/**
 * @brief Announce time progress to the kernel
 *
 * Informs the kernel that the specified number of ticks have elapsed
 * since the last call to z_clock_announce() (or system startup for
 * the first call).
 *
 * @param ticks Elapsed time, in ticks
 */
extern void z_clock_announce(s32_t ticks);

/**
 * @brief Ticks elapsed since last z_clock_announce() call
 *
 * Queries the clock driver for the current time elapsed since the
 * last call to z_clock_announce() was made.  The kernel will call
 * this with appropriate locking, the driver needs only provide an
 * instantaneous answer.
 */
extern u32_t z_clock_elapsed(void);

#ifdef __cplusplus
}
#endif

#endif /* ZEPHYR_INCLUDE_DRIVERS_SYSTEM_TIMER_H_ */
