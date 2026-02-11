/*
 * Copyright (c) 2025 Nordic Semiconductor ASA.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef UICR_DEPLOY_H__
#define UICR_DEPLOY_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Enable UICR.LOCK to lock contents of UICR and NVR0 from further configuration.
 *
 * @note The configuration does not take effect until after a reset.
 * @note This action can only be undone by performing an ERASEALL operation.
 *
 * @retval 0 on success.
 * @retval -1 if the UICR configuration is already locked.
 */
int uicr_deploy_lock_contents(void);

/**
 * @brief Enable UICR.ERASEPROTECT to prevent the device from being erased by an ERASEALL.
 *
 * @note The configuration does not take effect until after a reset.
 * @note Locking the UICR after setting this configuration is a one-time operation that cannot
 *       be undone.
 *
 * @retval 0 on success.
 * @retval -1 if the UICR configuration is locked.
 */
int uicr_deploy_block_eraseall(void);

#ifdef __cplusplus
}
#endif

#endif /* UICR_DEPLOY_H__ */
