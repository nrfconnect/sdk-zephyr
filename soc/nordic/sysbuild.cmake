# Copyright (c) 2024 Nordic Semiconductor ASA
# SPDX-License-Identifier: Apache-2.0

if(SB_CONFIG_NORDIC_COPROC_LAUNCHER)
  set(launcher_core "cpuapp")
  string(REPLACE "/" ";" launcher_quals ${BOARD_QUALIFIERS})
  list(LENGTH launcher_quals launcher_quals_len)
  list(GET launcher_quals 0 launcher_soc)
  list(GET launcher_quals 1 launcher_coproc)

  string(REPLACE "cpu" "" launcher_coproc ${launcher_coproc})

  if(launcher_quals_len EQUAL 3)
    list(GET launcher_quals 2 launcher_variant)
    set(launcher_coproc ${launcher_coproc}-${launcher_variant})
  endif()

  string(CONCAT launcher_board ${BOARD} "/" ${launcher_soc} "/" ${launcher_core})

  set(image "coproc_launcher")

  ExternalZephyrProject_Add(
    APPLICATION ${image}
    SOURCE_DIR ${ZEPHYR_BASE}/samples/boards/nordic/coproc_launcher
    BOARD ${launcher_board}
  )

  string(CONCAT launcher_snippet "nordic-" ${launcher_coproc})

  sysbuild_cache_set(VAR ${image}_SNIPPET APPEND REMOVE_DUPLICATES ${launcher_snippet})
endif()

if(SB_CONFIG_NRF_GENERATE_UICR)
  include(${CMAKE_CURRENT_LIST_DIR}/common/uicr/sysbuild.cmake)
endif()

if(SB_CONFIG_SOC_NRF71_GENERATE_UICR)
  include(${CMAKE_CURRENT_LIST_DIR}/nrf71/uicr/sysbuild.cmake)
endif()

if(SB_CONFIG_SOC_NRF71_GENERATE_WICR)
  include(${CMAKE_CURRENT_LIST_DIR}/nrf71/wicr/sysbuild.cmake)
endif()
