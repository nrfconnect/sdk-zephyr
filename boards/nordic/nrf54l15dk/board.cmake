# Copyright (c) 2024 Nordic Semiconductor ASA
# SPDX-License-Identifier: Apache-2.0

if(CONFIG_SOC_NRF54L05_CPUAPP OR CONFIG_SOC_NRF54L10_CPUAPP OR
		CONFIG_SOC_NRF54L15_CPUAPP)
  board_runner_args(jlink "--device=cortex-m33" "--speed=4000")
elseif(CONFIG_SOC_NRF54L05_CPUFLPR OR CONFIG_SOC_NRF54L10_CPUFLPR OR
		CONFIG_SOC_NRF54L15_CPUFLPR)
  board_runner_args(jlink "--speed=4000")
endif()

include(${ZEPHYR_BASE}/boards/common/nrfutil.board.cmake)
include(${ZEPHYR_BASE}/boards/common/jlink.board.cmake)
