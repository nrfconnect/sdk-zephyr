/*
 * Copyright (c) 2022 Codecoup
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/types.h>

/**
 * @brief Initialize the BAP Unicast Server role
 *
 * @return 0 if success, errno on failure.
 */
int bap_unicast_sr_init(void);

/**
 * @brief Initialize the CSIP Set Member role
 *
 * @return 0 if success, errno on failure.
 */
int csip_set_member_init(void (*rsi_changed)(const uint8_t *rsi));

/**
 * @brief Initialize the VCP Volume Renderer role
 *
 * @return 0 if success, errno on failure.
 */
int vcp_vol_renderer_init(void);

/**
 * @brief Initialize the MICP Microphone Device role
 *
 * @return 0 if success, errno on failure.
 */
int micp_mic_dev_init(void);

/**
 * @brief Initialize the CCP Call Control Client role
 *
 * @return 0 if success, errno on failure.
 */
int ccp_call_ctrl_init(void);

/**
 * @brief Initialize the HAS Server
 *
 * This will register hearing aid sample presets.
 *
 * @return 0 if success, errno on failure.
 */
int has_server_init(void);
