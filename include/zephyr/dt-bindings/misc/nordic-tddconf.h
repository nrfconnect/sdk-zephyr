/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_INCLUDE_DT_BINDINGS_MISC_NORDIC_TDDCONF_H_
#define ZEPHYR_INCLUDE_DT_BINDINGS_MISC_NORDIC_TDDCONF_H_

#define NRF_TDDCONF_SOURCE_STMMAINCORE BIT(0)
#define NRF_TDDCONF_SOURCE_ETMMAINCORE BIT(1)
#define NRF_TDDCONF_SOURCE_STMHWEVENTS BIT(2)
#define NRF_TDDCONF_SOURCE_STMPPR      BIT(3)
#define NRF_TDDCONF_SOURCE_STMFLPR     BIT(4)

#endif /* ZEPHYR_INCLUDE_DT_BINDINGS_MISC_NORDIC_TDDCONF_H_ */
