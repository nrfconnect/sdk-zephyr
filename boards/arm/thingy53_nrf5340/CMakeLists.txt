# Copyright (c) 2021 Nordic Semiconductor ASA
# SPDX-License-Identifier: Apache-2.0

if (CONFIG_BOARD_THINGY53_NRF5340_CPUAPP OR CONFIG_BOARD_THINGY53_NRF5340_CPUAPPNS)
	zephyr_library()
	zephyr_library_sources(board.c)
endif()

# In NCS Thingy:53 by default uses predefined partition map.
if(CONFIG_BOARD_THINGY53_NRF5340_CPUAPP)
	set(PM_STATIC_YML_FILE ${CMAKE_CURRENT_LIST_DIR}/pm_static.yml CACHE INTERNAL "")
endif()
