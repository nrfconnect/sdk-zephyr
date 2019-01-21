/*
 * Copyright (c) 2017 Intel Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _SOC_POWER_H_
#define _SOC_POWER_H_

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

enum power_states {
	SYS_POWER_STATE_CPU_LPS,       /* CONSTLAT Task Activited */
	SYS_POWER_STATE_CPU_LPS_1,     /* LOWPWR Task Activated */
	SYS_POWER_STATE_CPU_LPS_2,     /* Not supported*/
	SYS_POWER_STATE_DEEP_SLEEP,    /* System Off */
	SYS_POWER_STATE_DEEP_SLEEP_1,  /* Not Supported */
	SYS_POWER_STATE_DEEP_SLEEP_2,  /* Not Supported */

	SYS_POWER_STATE_MAX		/* Do nothing */
};

/**
 * @brief Put processor into low power state
 */
void sys_set_power_state(enum power_states state);

/**
 * @brief Check a low power state is supported by SoC
 */
bool sys_is_valid_power_state(enum power_states state);

/**
 * @brief Do any SoC or architecture specific post ops after low power states.
 */
void sys_power_state_post_ops(enum power_states state);

#ifdef __cplusplus
}
#endif

#endif /* _SOC_POWER_H_ */
