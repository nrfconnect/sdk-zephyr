/*
 * Copyright (c) 2016-2019 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

enum wpanusb_requests {
	RESET,
	TX,
	XMIT_ASYNC,
	ED,
	SET_CHANNEL,
	START,
	STOP,
	SET_SHORT_ADDR,
	SET_PAN_ID,
	SET_IEEE_ADDR,
	SET_TXPOWER,
	SET_CCA_MODE,
	SET_CCA_ED_LEVEL,
	SET_CSMA_PARAMS,
	SET_PROMISCUOUS_MODE,
};

struct set_channel {
	u8_t page;
	u8_t channel;
} __packed;

struct set_short_addr {
	u16_t short_addr;
} __packed;

struct set_pan_id {
	u16_t pan_id;
} __packed;

struct set_ieee_addr {
	u64_t ieee_addr;
} __packed;
