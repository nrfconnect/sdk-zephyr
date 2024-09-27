/*
 * Copyright (c) 2022 Laird Connectivity
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef H_MCUMGR_IMG_MGMT_CALLBACKS_
#define H_MCUMGR_IMG_MGMT_CALLBACKS_

#include <stdbool.h>
#include <stdint.h>
#include <zcbor_common.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Dummy definitions, include zephyr/mgmt/mcumgr/grp/img_mgmt/img_mgmt.h for actual definitions */
struct img_mgmt_upload_action;
struct img_mgmt_upload_req;

/**
 * @brief MCUmgr img_mgmt callback API
 * @defgroup mcumgr_callback_api_img_mgmt MCUmgr img_mgmt callback API
 * @ingroup mcumgr_callback_api
 * @{
 */

/**
 * Structure provided in the #MGMT_EVT_OP_IMG_MGMT_DFU_CHUNK notification callback: This callback
 * function is used to notify the application about a pending firmware upload packet from a client
 * and authorise or deny it. Upload will be allowed so long as all notification handlers return
 * #MGMT_ERR_EOK, if one returns an error then the upload will be denied.
 */
struct img_mgmt_upload_check {
	/** Action to take */
	struct img_mgmt_upload_action *action;

	/** Upload request information */
	struct img_mgmt_upload_req *req;
};

/**
 * Structure provided in the #MGMT_EVT_OP_IMG_MGMT_IMAGE_SLOT_STATE notification callback: This
 * callback function is used to allow applications or modules append custom fields to the image
 * slot state response.
 */
struct img_mgmt_state_slot_encode {
	bool *ok;
	zcbor_state_t *zse;
	const uint32_t slot;
	const char *version;
	const uint8_t *hash;
	const int flags;
};

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif
