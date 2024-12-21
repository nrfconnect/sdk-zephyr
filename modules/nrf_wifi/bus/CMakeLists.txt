#
# Copyright (c) 2024 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: BSD-3-Clause
#

# Check if ZEPHYR_BASE is set
if(NOT DEFINED ENV{ZEPHYR_BASE})
    message(FATAL_ERROR "ZEPHYR_BASE environment variable is not set. Please set it to the Zephyr base directory.")
endif()

if (CONFIG_NRF70_BUSLIB)
  zephyr_library_named(nrf70-buslib)
  zephyr_library_include_directories(
    inc
  )
  zephyr_library_sources(
    device.c
  )
  zephyr_library_sources_ifdef(CONFIG_NRF70_ON_QSPI
    rpu_hw_if.c
    qspi_if.c
  )
  zephyr_library_sources_ifdef(CONFIG_NRF70_ON_SPI
    rpu_hw_if.c
    spi_if.c
  )
  zephyr_library_sources_ifdef(CONFIG_NRF71_ON_IPC
    ipc_if.c
  )
endif()
