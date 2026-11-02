/*
 * Copyright (c) 2026 Nordic Semiconductor
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/ztest.h>
#include <zephyr/ipc/ipc_service.h>
#include <hal/nrf_vpr.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(ipc_service_error_cases, LOG_LEVEL_DBG);

static const struct device *ipc_dut = DEVICE_DT_GET(DT_ALIAS(dut_ipc));
static const struct device *invalid = DEVICE_DT_GET(DT_ALIAS(dut_invalid));

/* Default endpoint configuration shared between tests. */
static void dut_bound_cb(void *priv)
{
	bool *bound = priv;
	*bound = true;
	LOG_INF("DUT endpoint bound");
}

static void dut_unbound_cb(void *priv)
{
	bool *bound = priv;
	*bound = false;
	LOG_INF("DUT endpoint unbound");
}

static void dut_error_cb(const char *message, void *priv)
{
	ARG_UNUSED(priv);
	LOG_ERR("Endpoint error: %s", message);
}

static void dut_received_cb(const void *data, size_t len, void *priv)
{
	ARG_UNUSED(data);
	ARG_UNUSED(priv);
	LOG_INF("DUT received %u", len);
}

static bool dut_bound_flag = false;
static struct ipc_ept_cfg ept_dut_cfg = {
	.name = "ept0",
	.cb = {
		.bound    = dut_bound_cb,
		.unbound  = dut_unbound_cb,
		.error    = dut_error_cb,
		.received = dut_received_cb,
	},
	.priv     = &dut_bound_flag,
};

/**
 * @brief Poll until a volatile flag becomes true or timeout expires.
 *
 * Function supports no-multithreading mode.
 *
 * @param flag Pointer to the flag to wait on.
 * @param timeout_ms Maximum wait time in milliseconds.
 *
 * @return true if the flag was set before timeout, false otherwise.
 */
static inline bool wait_for_flag(volatile bool *flag, uint32_t timeout_ms)
{
	uint32_t start = k_uptime_get_32();

	while (!*flag) {
		k_msleep(1);
		if ((k_uptime_get_32() - start) >= timeout_ms) {
			return false;
		}
	}

	return true;
}

/**
 * @brief Restart remote core to clear IPC status.
 */
static inline void restart_remote_v1()
{
	NRF_VPR_Type *vpr = (NRF_VPR_Type *)DT_REG_ADDR(DT_NODELABEL(cpuflpr_vpr));
	uintptr_t exec_addr = DT_REG_ADDR(DT_PHANDLE_BY_IDX(DT_NODELABEL(cpuflpr_vpr), execution_memory, 0));
	LOG_DBG("CPURUN A: %u", nrf_vpr_cpurun_get(vpr));
	nrf_vpr_cpurun_set(vpr, false);
	LOG_DBG("CPURUN B: %u", nrf_vpr_cpurun_get(vpr));
	LOG_DBG("DMACTIVE %u", nrf_vpr_debugif_dmcontrol_get(vpr, NRF_VPR_DMCONTROL_DMACTIVE));
	LOG_DBG("NDMRESET %u", nrf_vpr_debugif_dmcontrol_get(vpr, NRF_VPR_DMCONTROL_NDMRESET));
	nrf_vpr_debugif_dmcontrol_set(vpr, NRF_VPR_DMCONTROL_DMACTIVE, true);
	nrf_vpr_debugif_dmcontrol_set(vpr, NRF_VPR_DMCONTROL_NDMRESET, true);
	k_msleep(100);
	nrf_vpr_debugif_dmcontrol_set(vpr, NRF_VPR_DMCONTROL_NDMRESET, false);
	//nrf_vpr_debugif_dmcontrol_set(vpr, NRF_VPR_DMCONTROL_DMACTIVE, false);
	LOG_DBG("VPR started from %p", (void *)exec_addr);
	nrf_vpr_initpc_set(vpr, exec_addr);
	LOG_DBG("CPURUN C: %u", nrf_vpr_cpurun_get(vpr));
	nrf_vpr_cpurun_set(vpr, true);
	LOG_DBG("CPURUN D: %u", nrf_vpr_cpurun_get(vpr));
	k_msleep(100);
}

/**
 * @brief Restart remote core to clear IPC status.
 */
static inline void restart_remote_v2()
{
	NRF_VPR_Type *vpr = (NRF_VPR_Type *)DT_REG_ADDR(DT_NODELABEL(cpuflpr_vpr));
	uintptr_t exec_addr = DT_REG_ADDR(DT_PHANDLE_BY_IDX(DT_NODELABEL(cpuflpr_vpr), execution_memory, 0));

	nrf_vpr_initpc_set(vpr, exec_addr);

	nrf_vpr_debugif_dmcontrol_set(vpr, NRF_VPR_DMCONTROL_DMACTIVE, true);
	nrf_vpr_debugif_dmcontrol_set(vpr, NRF_VPR_DMCONTROL_NDMRESET, true);
	k_msleep(100);
	nrf_vpr_debugif_dmcontrol_set(vpr, NRF_VPR_DMCONTROL_NDMRESET, false);
	nrf_vpr_debugif_dmcontrol_set(vpr, NRF_VPR_DMCONTROL_DMACTIVE, false);
	LOG_DBG("VPR started from %p", (void *)exec_addr);
	LOG_DBG("CPURUN D: %u", nrf_vpr_cpurun_get(vpr));
	k_msleep(100);
}

/**
 * @brief Restart remote core to clear IPC status.
 */
static inline void restart_remote()
{
	NRF_VPR_Type *vpr = (NRF_VPR_Type *)DT_REG_ADDR(DT_NODELABEL(cpuflpr_vpr));
	uintptr_t exec_addr = DT_REG_ADDR(DT_PHANDLE_BY_IDX(DT_NODELABEL(cpuflpr_vpr), execution_memory, 0));
	uintptr_t src_addr = DT_REG_ADDR(DT_PHANDLE_BY_IDX(DT_NODELABEL(cpuflpr_vpr), source_memory, 0));
	size_t mem_size = DT_REG_SIZE(DT_PHANDLE_BY_IDX(DT_NODELABEL(cpuflpr_vpr), execution_memory, 0));

	nrf_vpr_cpurun_set(vpr, false);
	LOG_DBG("DMACTIVE %u", nrf_vpr_debugif_dmcontrol_get(vpr, NRF_VPR_DMCONTROL_DMACTIVE));
	LOG_DBG("NDMRESET %u", nrf_vpr_debugif_dmcontrol_get(vpr, NRF_VPR_DMCONTROL_NDMRESET));
	nrf_vpr_debugif_dmcontrol_set(vpr, NRF_VPR_DMCONTROL_DMACTIVE, true);
	nrf_vpr_debugif_dmcontrol_set(vpr, NRF_VPR_DMCONTROL_NDMRESET, true);
	k_msleep(100);
	nrf_vpr_debugif_dmcontrol_set(vpr, NRF_VPR_DMCONTROL_NDMRESET, false);
	nrf_vpr_debugif_dmcontrol_set(vpr, NRF_VPR_DMCONTROL_DMACTIVE, false);
	LOG_DBG("CPURUN should be 0: %u", nrf_vpr_cpurun_get(vpr));

	memcpy((void *)exec_addr, (void *)src_addr, mem_size);
#if defined(CONFIG_DCACHE)
	LOG_DBG("Cache flush");
	sys_cache_data_flush_range((void *)exec_addr, mem_size);
#endif

	nrf_vpr_initpc_set(vpr, exec_addr);
	nrf_vpr_cpurun_set(vpr, true);
	LOG_DBG("VPR started from %p", (void *)exec_addr);
	LOG_DBG("CPURUN should be 1: %u", nrf_vpr_cpurun_get(vpr));
	k_msleep(100);
}







/**
 * @brief ipc_service_open_instance() negative test
 *
 * Confirm that ipc_service_open_instance() returns
 * -EINVAL when instance configuration is invalid.
 *
 */
ZTEST(ipc_service_error_cases, test_01_ipc_service_open_invalid_instance)
{
	int ret;
	
	ret = ipc_service_open_instance(invalid);
	zassert_true(ret == -EINVAL,
		     "ipc_service_open_instance() returned %d while expected was -EINVAL (-22)",
		     ret);
}

/**
 * @brief ipc_service_open_instance() negative test
 *
 * Confirm that ipc_service_open_instance() returns
 * -EALREADY when the instance is already opened (or being opened).
 *
 */
ZTEST(ipc_service_error_cases, test_02a_ipc_service_open_instance_already_opened)
{
	int ret;
	struct ipc_ept ept_dut;
k_msleep(2000);
	restart_remote();
	ret = ipc_service_open_instance(ipc_dut);
	zassert_true(ret == 0,
		     "ipc_service_open_instance() returned %d while expected was 0", ret);

	ret = ipc_service_open_instance(ipc_dut);
	zassert_true(ret == -EALREADY,
		     "Second call to ipc_service_open_instance() shall return -EALREADY (-120), "
		     "got unexpected value of %d",
		     ret);

	/* Cleanup. */
	ret = ipc_service_register_endpoint(ipc_dut, &ept_dut, &ept_dut_cfg);
	zassert_true(ret == 0,
		     "ipc_service_register_endpoint(dut) returned %d while expected was 0", ret);

	ret = ipc_service_deregister_endpoint(&ept_dut);
	zassert_true(ret == 0,
		     "ipc_service_deregister_endpoint(dut) returned %d while expected was 0", ret);

	ret = ipc_service_close_instance(ipc_dut);
	zassert_true(ret == 0,
		     "ipc_service_close_instance() returned %d while expected was 0", ret);
}

/**
 * @brief ipc_service_open_instance() negative test
 *
 * Confirm that ipc_service_open_instance() returns
 * -EALREADY when the instance is already opened (or being opened).
 * Check after closing the IPC Service instance.
 *
 */
ZTEST(ipc_service_error_cases, test_02b_ipc_service_open_instance_already_opened)
{
	int ret;

	ret = ipc_service_open_instance(ipc_dut);
	zassert_true(ret == 0,
		     "ipc_service_open_instance() returned %d while expected was 0", ret);

	ret = ipc_service_close_instance(ipc_dut);
	zassert_true(ret == 0,
		     "ipc_service_close_instance() returned %d while expected was 0", ret);

	ret = ipc_service_open_instance(ipc_dut);
	zassert_true(ret == 0,
		     "ipc_service_open_instance() returned %d while expected was 0", ret);

	ret = ipc_service_open_instance(ipc_dut);
	zassert_true(ret == -EALREADY,
		     "Second call to ipc_service_open_instance() shall return -EALREADY (-120), "
		     "got unexpected value of %d",
		     ret);

	/* Cleanup. */
	ret = ipc_service_close_instance(ipc_dut);
	zassert_true(ret == 0,
		     "ipc_service_close_instance() returned %d while expected was 0", ret);
}

/**
 * @brief ipc_service_close_instance() negative test
 *
 * Confirm that ipc_service_close_instance() returns
 * -EINVAL when instance configuration is invalid.
 *
 */
ZTEST(ipc_service_error_cases, test_11_ipc_service_close_invalid_instance)
{
	int ret;
	const struct device *invalid = DEVICE_DT_GET(DT_ALIAS(dut_invalid));

	ret = ipc_service_close_instance(invalid);
	zassert_true(ret == -EINVAL,
		     "ipc_service_close_instance() returned %d while expected was -EINVAL (-22)",
		     ret);
}

/**
 * @brief ipc_service_close_instance() negative test
 *
 * Confirm that ipc_service_close_instance() returns
 * -EALREADY when the instance is not already opened.
 *
 */
ZTEST(ipc_service_error_cases, test_12_ipc_service_close_not_opened_instance)
{
	int ret;

	ret = ipc_service_close_instance(ipc_dut);
	zassert_true(ret == -EALREADY,
		     "ipc_service_close_instance() returned %d while expected was -EALREADY (-120)",
		     ret);
}


/**
 * @brief ipc_service_close_instance() negative test
 *
 * Confirm that ipc_service_close_instance() returns
 * -EBUSY when an endpoint exists that hasn't been deregistered.
 *
 */
ZTEST(ipc_service_error_cases, test_13b_ipc_service_close_instance_with_endpoints)
{
	int ret;
	struct ipc_ept ept_dut;
k_msleep(2000);
	restart_remote();

	ret = ipc_service_open_instance(ipc_dut);
	zassert_true(ret == 0,
		     "ipc_service_open_instance(dut) returned %d while expected was 0", ret);

	ret = ipc_service_register_endpoint(ipc_dut, &ept_dut, &ept_dut_cfg);
	zassert_true(ret == 0,
		     "ipc_service_register_endpoint(dut) returned %d while expected was 0", ret);

	ret = ipc_service_close_instance(ipc_dut);
	zassert_true(ret == -EBUSY,
		     "ipc_service_close_instance() returned %d while expected was -EBUSY (-16)",
		     ret);

	/* Cleanup */
	ret = ipc_service_deregister_endpoint(&ept_dut);
	zassert_true(ret == 0,
		     "ipc_service_deregister_endpoint(dut) returned %d while expected was 0", ret);

	ret = ipc_service_close_instance(ipc_dut);
	zassert_true(ret == 0,
		     "ipc_service_close_instance(dut) returned %d while expected was 0",
		     ret);
}

static void *suite_setup(void)
{
	TC_PRINT("Test executed on %s\n", CONFIG_BOARD_TARGET);
	TC_PRINT("===================================================================\n");

	return NULL;
}

static void before_test(void *not_used)
{
	ARG_UNUSED(not_used);
	/* Nothing yet. */
}

static void cleanup_after_test(void *f)
{
	ARG_UNUSED(f);
	/* Nothing yet. */
}

ZTEST_SUITE(ipc_service_error_cases, NULL, suite_setup, before_test, cleanup_after_test, NULL);
