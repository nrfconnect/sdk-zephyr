/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/ztest.h>
#include <zephyr/net/buf.h>
#include <zephyr/mgmt/mcumgr/mgmt/mgmt.h>
#include <zephyr/mgmt/mcumgr/transport/smp_dummy.h>
#include <zephyr/mgmt/mcumgr/grp/os_mgmt/os_mgmt.h>

#define SMP_RESPONSE_WAIT_TIME 3

/* Test os_mgmt echo command with 40 bytes of data: "short MCUMGR test application message..." */
static const uint8_t command[] = {
	0x02, 0x00, 0x00, 0x2e, 0x00, 0x00, 0x01, 0x00,
	0xbf, 0x61, 0x64, 0x78, 0x28, 0x73, 0x68, 0x6f,
	0x72, 0x74, 0x20, 0x4d, 0x43, 0x55, 0x4d, 0x47,
	0x52, 0x20, 0x74, 0x65, 0x73, 0x74, 0x20, 0x61,
	0x70, 0x70, 0x6c, 0x69, 0x63, 0x61, 0x74, 0x69,
	0x6f, 0x6e, 0x20, 0x6d, 0x65, 0x73, 0x73, 0x61,
	0x67, 0x65, 0x2e, 0x2e, 0x2e, 0xff,
};

/* Expected response from mcumgr */
static const uint8_t expected_response[] = {
	0x03, 0x00, 0x00, 0x2e, 0x00, 0x00, 0x01, 0x00,
	0xbf, 0x61, 0x72, 0x78, 0x28, 0x73, 0x68, 0x6f,
	0x72, 0x74, 0x20, 0x4d, 0x43, 0x55, 0x4d, 0x47,
	0x52, 0x20, 0x74, 0x65, 0x73, 0x74, 0x20, 0x61,
	0x70, 0x70, 0x6c, 0x69, 0x63, 0x61, 0x74, 0x69,
	0x6f, 0x6e, 0x20, 0x6d, 0x65, 0x73, 0x73, 0x61,
	0x67, 0x65, 0x2e, 0x2e, 0x2e, 0xff
};

ZTEST(os_mgmt_echo, test_echo)
{
	struct net_buf *nb;

	/* Register os_mgmt mcumgr group */
	os_mgmt_register_group();

	/* Enable dummy SMP backend and ready for usage */
	smp_dummy_enable();
	smp_dummy_clear_state();

	/* Send test echo command to dummy SMP backend */
	(void)smp_dummy_tx_pkt(command, sizeof(command));
	smp_dummy_add_data();

	/* For a short duration to see if response has been received */
	bool received = smp_dummy_wait_for_data(SMP_RESPONSE_WAIT_TIME);

	zassert_true(received, "Expected to receive data but timed out\n");

	/* Retrieve response buffer and ensure validity */
	nb = smp_dummy_get_outgoing();
	smp_dummy_disable();

	zassert_equal(sizeof(expected_response), nb->len,
		      "Expected to receive %d bytes but got %d\n", sizeof(expected_response),
		      nb->len);

	zassert_mem_equal(expected_response, nb->data, nb->len,
			  "Expected received data mismatch");
}

ZTEST_SUITE(os_mgmt_echo, NULL, NULL, NULL, NULL, NULL);
