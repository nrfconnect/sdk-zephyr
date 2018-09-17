#
# Copyright (c) 2018 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: Apache-2.0
#

zephyr_library()

zephyr_library_sources_ifdef(
  CONFIG_BT_LL_NORDIC
  crypto.c
  hci_driver.c
  )

zephyr_library_link_libraries(subsys__bluetooth)
