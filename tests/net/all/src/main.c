/* main.c - Application main entry point */

/* We are just testing that this program compiles ok with all possible
 * network related Kconfig options enabled.
 */

/*
 * Copyright (c) 2017 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_MODULE_NAME net_test
#define NET_LOG_LEVEL LOG_LEVEL_DBG

#include <ztest.h>

#include <net/net_if.h>
#include <net/net_pkt.h>
#include <net/dummy.h>

static struct offload_context {
	void *none;
} offload_context_data = {
	.none = NULL
};

static struct dummy_api offload_if_api = {
	.iface_api.init = NULL,
	.send = NULL,
};

NET_DEVICE_OFFLOAD_INIT(net_offload, "net_offload",
			NULL, &offload_context_data, NULL,
			CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,
			&offload_if_api, 0);

static void ok(void)
{
	zassert_true(true, "This test should never fail");
}

void test_main(void)
{
	ztest_test_suite(net_compile_all_test,
			 ztest_unit_test(ok)
			 );

	ztest_run_test_suite(net_compile_all_test);
}
