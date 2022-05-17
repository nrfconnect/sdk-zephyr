/*
 * Copyright (c) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <ztest.h>
#include <devicetree.h>
#include <device.h>
#include <drivers/spi.h>
#include <logging/log.h>

LOG_MODULE_REGISTER(test, CONFIG_LOG_DEFAULT_LEVEL);

static void test_dt_spec(void)
{
	const struct spi_dt_spec spi_cs =
		SPI_DT_SPEC_GET(DT_NODELABEL(test_spi_dev_cs), 0, 0);

	LOG_DBG("spi_cs.bus = %p", spi_cs.bus);
	LOG_DBG("spi_cs.config.cs->gpio.port = %p", spi_cs.config.cs->gpio.port);
	LOG_DBG("spi_cs.config.cs->gpio.pin = %u", spi_cs.config.cs->gpio.pin);

	zassert_equal(spi_cs.bus, DEVICE_DT_GET(DT_NODELABEL(test_spi_cs)), "");
	zassert_equal(spi_cs.config.cs->gpio.port, DEVICE_DT_GET(DT_NODELABEL(test_gpio)), "");
	zassert_equal(spi_cs.config.cs->gpio.pin, 0x10, "");

	const struct spi_dt_spec spi_no_cs =
		SPI_DT_SPEC_GET(DT_NODELABEL(test_spi_dev_no_cs), 0, 0);

	LOG_DBG("spi_no_cs.bus = %p", spi_no_cs.bus);
	LOG_DBG("spi_no_cs.config.cs = %p", spi_no_cs.config.cs);

	zassert_equal(spi_no_cs.bus, DEVICE_DT_GET(DT_NODELABEL(test_spi_no_cs)), "");
	zassert_is_null(spi_no_cs.config.cs, "");
}

void test_main(void)
{
	ztest_test_suite(spi_dt_spec,
			 ztest_unit_test(test_dt_spec)
			);
	ztest_run_test_suite(spi_dt_spec);
}
