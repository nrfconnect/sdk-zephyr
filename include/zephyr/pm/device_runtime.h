/*
 * Copyright (c) 2015 Intel Corporation.
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_INCLUDE_PM_DEVICE_RUNTIME_H_
#define ZEPHYR_INCLUDE_PM_DEVICE_RUNTIME_H_

#include <zephyr/device.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Device Runtime Power Management API
 * @defgroup subsys_pm_device_runtime Device Runtime
 * @ingroup subsys_pm
 * @{
 */

#if defined(CONFIG_PM_DEVICE_RUNTIME) || defined(__DOXYGEN__)
/**
 * @brief Enable device runtime PM
 *
 * This function will enable runtime PM on the given device. If the device is
 * in #PM_DEVICE_STATE_ACTIVE state, the device will be suspended.
 *
 * @funcprops \pre_kernel_ok
 *
 * @param dev Device instance.
 *
 * @retval 0 If the device runtime PM is enabled successfully.
 * @retval -EPERM If device has power state locked.
 * @retval -ENOSYS If the functionality is not available.
 * @retval -errno Other negative errno, result of suspending the device.
 *
 * @see pm_device_init_suspended()
 */
int pm_device_runtime_enable(const struct device *dev);

/**
 * @brief Disable device runtime PM
 *
 * If the device is currently suspended it will be resumed.
 *
 * @funcprops \pre_kernel_ok
 *
 * @param dev Device instance.
 *
 * @retval 0 If the device runtime PM is disabled successfully.
 * @retval -ENOSYS If the functionality is not available.
 * @retval -errno Other negative errno, result of resuming the device.
 */
int pm_device_runtime_disable(const struct device *dev);

/**
 * @brief Resume a device based on usage count.
 *
 * This function will resume the device if the device is suspended (usage count
 * equal to 0). In case of a resume failure, usage count and device state will
 * be left unchanged. In all other cases, usage count will be incremented.
 *
 * If the device is still being suspended as a result of calling
 * pm_device_runtime_put_async(), this function will wait for the operation to
 * finish to then resume the device.
 *
 * @funcprops \pre_kernel_ok
 *
 * @param dev Device instance.
 *
 * @retval 0 If it succeeds. In case device runtime PM is not enabled or not
 * available this function will be a no-op and will also return 0.
 * @retval -errno Other negative errno, result of the PM action callback.
 */
int pm_device_runtime_get(const struct device *dev);

/**
 * @brief Suspend a device based on usage count.
 *
 * This function will suspend the device if the device is no longer required
 * (usage count equal to 0). In case of suspend failure, usage count and device
 * state will be left unchanged. In all other cases, usage count will be
 * decremented (down to 0).
 *
 * @funcprops \pre_kernel_ok
 *
 * @param dev Device instance.
 *
 * @retval 0 If it succeeds. In case device runtime PM is not enabled or not
 * available this function will be a no-op and will also return 0.
 * @retval -EALREADY If device is already suspended (can only happen if get/put
 * calls are unbalanced).
 * @retval -errno Other negative errno, result of the action callback.
 *
 * @see pm_device_runtime_put_async()
 */
int pm_device_runtime_put(const struct device *dev);

/**
 * @brief Suspend a device based on usage count (asynchronously).
 *
 * This function will schedule the device suspension if the device is no longer
 * required (usage count equal to 0). In all other cases, usage count will be
 * decremented (down to 0).
 *
 * @note Asynchronous operations are not supported when in pre-kernel mode. In
 * this case, the function will be blocking (equivalent to
 * pm_device_runtime_put()).
 *
 * @funcprops \pre_kernel_ok, \async
 *
 * @param dev Device instance.
 *
 * @retval 0 If it succeeds. In case device runtime PM is not enabled or not
 * available this function will be a no-op and will also return 0.
 * @retval -EALREADY If device is already suspended (can only happen if get/put
 * calls are unbalanced).
 *
 * @see pm_device_runtime_put()
 */
int pm_device_runtime_put_async(const struct device *dev);

/**
 * @brief Check if device runtime is enabled for a given device.
 *
 * @funcprops \pre_kernel_ok
 *
 * @param dev Device instance.
 *
 * @retval true If device has device runtime PM enabled.
 * @retval false If the device has device runtime PM disabled.
 *
 * @see pm_device_runtime_enable()
 */
bool pm_device_runtime_is_enabled(const struct device *dev);

#else
static inline int pm_device_runtime_enable(const struct device *dev) { return -ENOSYS; }
static inline int pm_device_runtime_disable(const struct device *dev) { return -ENOSYS; }
static inline int pm_device_runtime_get(const struct device *dev) { return 0; }
static inline int pm_device_runtime_put(const struct device *dev) { return 0; }
static inline int pm_device_runtime_put_async(const struct device *dev) { return 0; }
static inline bool pm_device_runtime_is_enabled(const struct device *dev) { return false; }
#endif

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* ZEPHYR_INCLUDE_PM_DEVICE_RUNTIME_H_ */
