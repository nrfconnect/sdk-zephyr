# Kconfig - nRF91 PCA20035 board configuration
#
# Copyright (c) 2018 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: Apache-2.0

zephyr_library()
zephyr_library_sources_ifdef(
        CONFIG_TRUSTED_EXECUTION_NONSECURE
        board_nonsecure.c)
zephyr_library_sources_ifdef(
        CONFIG_TRUSTED_EXECUTION_SECURE
        board_secure.c)
add_subdirectory_ifdef(
        CONFIG_TRUSTED_EXECUTION_NONSECURE
        adp536x)
