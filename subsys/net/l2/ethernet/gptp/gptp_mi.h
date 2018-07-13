/*
 * Copyright (c) 2017 Intel Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief GPTP Media Independent interface
 *
 * This is not to be included by the application.
 */

#ifndef __GPTP_MI_H
#define __GPTP_MI_H

#ifdef __cplusplus
extern "C" {
#endif

#if defined(CONFIG_NET_GPTP)

#include "gptp_md.h"

/**
 * @brief Media Independent Sync Information.
 *
 * This structure applies for MDSyncReceive as well as MDSyncSend.
 */
struct gptp_mi_port_sync_sync {
	/** Port to which the Sync Information belongs to. */
	u16_t local_port_number;

	/** Time at which the sync receipt timeout occurs. */
	u64_t sync_receipt_timeout_time;

	/** Copy of the gptp_md_sync_info to be transmitted. */
	struct gptp_md_sync_info sync_info;
};

/**
 * @brief Initialize all Media Independent State Machines.
 */
void gptp_mi_init_state_machine(void);

/**
 * @brief Run all Media Independent Port Sync State Machines.
 *
 * @param port Number of the port the State Machines needs to be run on.
 */
void gptp_mi_port_sync_state_machines(int port);

/**
 * @brief Run all Media Independent Port BMCA State Machines.
 *
 * @param port Number of the port the State Machines needs to be run on.
 */
void gptp_mi_port_bmca_state_machines(int port);

/**
 * @brief Run all Media Independent State Machines.
 */
void gptp_mi_state_machines(void);

/**
 * @brief Return current time in nanoseconds.
 *
 * @param port Port number of the clock to use.
 *
 * @return Current time in nanoseconds.
 */
u64_t gptp_get_current_time_nanosecond(int port);

#endif /* CONFIG_NET_GPTP */

#ifdef __cplusplus
}
#endif

#endif /* __GPTP_MI_H */
