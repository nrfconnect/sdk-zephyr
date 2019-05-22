/*
 * Copyright (c) 2019 Henrik Brix Andersen <henrik@brixandersen.dk>
 *
 * SPDX-License-Identifier: Apache-2.0
 */


#ifndef ZEPHYR_INCLUDE_DRIVERS_LED_HT16K33_H_
#define ZEPHYR_INCLUDE_DRIVERS_LED_HT16K33_H_

#include <device.h>
#include <zephyr/types.h>

/**
 * Register a HT16K33 keyscan device to be notified of relevant
 * keyscan events by the keyscan interrupt thread in the HT16K33
 * parent driver.
 *
 * @param parent      HT16K33 parent device.
 * @param child       HT16K33 keyscan child device.
 * @param keyscan_idx Index of the keyscan line handled by the keyscan
 *		      child device (0, 1, or 2).
 * @return 0 if successful, negative errne code on failure.
 */
int ht16k33_register_keyscan_device(struct device *parent,
				    struct device *child,
				    u8_t keyscan_idx);

/**
 * Check if a HT16K33 keyscan interrupt is pending.
 *
 * @param  parent HT16K33 parent device.
 * @return status != 0 if an interrupt is pending.
 */
u32_t ht16k33_get_pending_int(struct device *parent);

/**
 * Dispatch keyscan row data from a keyscan event to be handled by a
 * HT16K33 keyscan GPIO child device.
 *
 * @param child HT16K33 keyscan child device.
 * @param keys  Bitmask of key state for the row.
 */
void ht16k33_process_keyscan_row_data(struct device *child,
				      u32_t keys);

#endif /* ZEPHYR_INCLUDE_DRIVERS_LED_HT16K33_H_ */
