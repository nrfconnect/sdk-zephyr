cmake_minimum_required(VERSION 3.20.0)
# Top-level CMakeLists.txt for the skeleton application.
#
# Copyright (c) 2017 Open Source Foundries Limited
#
# SPDX-License-Identifier: Apache-2.0
#
# This provides a basic application structure suitable for communication using
# mcumgr.  It can be used as a starting point for new applications.

# Standard Zephyr application boilerplate.

if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/configuration/${BOARD}")
  message(WARNING
          "Board specific configuration will be used for ${BOARD} "
          "When providing -DOVERLAY_CONFIG= remember that all overlay "
          "configuration paths are relative to "
          "${CMAKE_CURRENT_SOURCE_DIR}/configuration/${BOARD}")
  set(APPLICATION_CONFIG_DIR "configuration/\${BOARD}")
else()
  message(NOTICE
          "Board ${BOARD} does not have board specific configuration.\n")
endif()

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(smp_svr)

target_sources(app PRIVATE src/main.c)
target_sources_ifdef(CONFIG_MCUMGR_TRANSPORT_BT app PRIVATE src/bluetooth.c)
