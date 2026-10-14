# Copyright (c) 2026 Nordic Semiconductor ASA
# SPDX-License-Identifier: Apache-2.0

zephyr_include_directories(${ZEPHYR_HAL_NORDIC_MODULE_DIR}/ironside/te/client/include)
zephyr_include_directories(include)

zephyr_library()
zephyr_library_sources(
  ${ZEPHYR_HAL_NORDIC_MODULE_DIR}/ironside/te/client/src/client.c
  ${ZEPHYR_HAL_NORDIC_MODULE_DIR}/ironside/te/client/src/services.c
  client_lock.c
  layout_check.c
)
