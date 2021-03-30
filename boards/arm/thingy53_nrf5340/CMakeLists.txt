# Copyright (c) 2021 Nordic Semiconductor ASA
# SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic

if ((CONFIG_BOARD_THINGY53_NRF5340_CPUAPP OR CONFIG_BOARD_THINGY53_NRF5340_CPUAPPNS)
    AND CONFIG_BOARD_ENABLE_CPUNET)
	zephyr_library()
	zephyr_library_sources(nrf5340_cpunet_reset.c)
endif()

if (CONFIG_BOARD_THINGY53_NRF5340_CPUAPP OR CONFIG_BOARD_THINGY53_NRF5340_CPUAPPNS)
	zephyr_library_sources(board.c)
endif()
