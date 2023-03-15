# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

if(BOARD STREQUAL bl5340pa_dvk_cpuapp)
    # If child_image/hci_rpmsg.conf exists, then it must be explicitly set
    # before using append.
    list(APPEND hci_rpmsg_OVERLAY_CONFIG ${CMAKE_SOURCE_DIR}/boards/fem.conf)
endif()

# Allow testing of Laird Connectivity FEM driver with Nordic DKs
if(MOCK_LCZ_FEM AND BOARD STREQUAL nrf5340dk_nrf5340_cpuapp
    AND hci_rpmsg_SHIELD STREQUAL nrf21540_ek)
    message("Mocking Laird Connectivity Zephyr FEM Driver on nrf5340dk")
    list(APPEND hci_rpmsg_OVERLAY_CONFIG ${CMAKE_SOURCE_DIR}/boards/fem.conf)
    list(APPEND hci_rpmsg_DTC_OVERLAY_FILE ${CMAKE_SOURCE_DIR}/child_image/mock_lcz_fem.nrf21540_ek.overlay)
endif()

if(MOCK_LCZ_FEM AND BOARD STREQUAL nrf21540dk_nrf52840)
    message("Mocking Laird Connectivity Zephyr FEM Driver on nrf21540dk")
    list(APPEND OVERLAY_CONFIG ${CMAKE_SOURCE_DIR}/boards/fem.conf)
    list(APPEND DTC_OVERLAY_FILE ${CMAKE_SOURCE_DIR}/boards/mock_lcz_fem.overlay)
endif()

# Network cpu doesn't have low power uart control
# If SPI control of FEM is used, then network UART isn't available and
# either RTT or USB can be used.
if(BOARD STREQUAL bl5340_dvk_cpuapp OR BOARD STREQUAL bl5340pa_dvk_cpuapp
    OR BOARD STREQUAL nrf5340dk_nrf5340_cpuapp)
if(LOG_TYPE STREQUAL uart)
    list(APPEND hci_rpmsg_OVERLAY_CONFIG ${CMAKE_SOURCE_DIR}/child_image/uart_log.conf)
elseif(LOG_TYPE STREQUAL rtt)
    list(APPEND hci_rpmsg_OVERLAY_CONFIG ${CMAKE_SOURCE_DIR}/child_image/rtt_log.conf)
    list(APPEND hci_rpmsg_DTC_OVERLAY_FILE ${CMAKE_SOURCE_DIR}/child_image/disable_uart.overlay)
else()
    list(APPEND hci_rpmsg_OVERLAY_CONFIG ${CMAKE_SOURCE_DIR}/child_image/no_log.conf)
    list(APPEND hci_rpmsg_DTC_OVERLAY_FILE ${CMAKE_SOURCE_DIR}/child_image/disable_uart.overlay)
endif()
endif() #CPUNET

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(sleepy_advertiser)

target_sources(app PRIVATE src/main.c)
