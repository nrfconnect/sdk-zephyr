/*
 * Copyright (c) 2023 Pawel Osypiuk <pawelosyp@gmail.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <soc.h>
#include <zephyr/device.h>
#include <nrf53_cpunet_mgmt.h>
#if defined(CONFIG_BT_CTLR_DEBUG_PINS_CPUAPP)
#include <../subsys/bluetooth/controller/ll_sw/nordic/hal/nrf5/debug.h>
#else
#define DEBUG_SETUP()
#endif /* defined(CONFIG_BT_CTLR_DEBUG_PINS_CPUAPP) */

#define LOG_LEVEL CONFIG_BT_HCI_DRIVER_LOG_LEVEL
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(bt_hci_nrf53_support);

int bt_hci_transport_teardown(const struct device *dev)
{
	ARG_UNUSED(dev);
    /* Put the Network MCU in Forced-OFF mode. */
	nrf53_cpunet_enable(false);
	LOG_DBG("Network MCU placed in Forced-OFF mode");

	return 0;
}

int bt_hci_transport_setup(const struct device *dev)
{
	ARG_UNUSED(dev);
#if !defined(CONFIG_TRUSTED_EXECUTION_NONSECURE) || defined(CONFIG_BUILD_WITH_TFM)
	/* Route Bluetooth Controller Debug Pins */
	DEBUG_SETUP();
#endif /* !defined(CONFIG_TRUSTED_EXECUTION_NONSECURE) || defined(CONFIG_BUILD_WITH_TFM) */

	/* Release the Network MCU, 'Release force off signal' */
	nrf53_cpunet_enable(true);

	return 0;
}
