# SPDX-License-Identifier: Apache-2.0

add_subdirectory(src)
zephyr_include_directories(
  include
  )

if (CONFIG_SETTINGS_FCB OR CONFIG_SETTINGS_NVS)
  # These settings backends require a partition in flash
  add_partition_manager_config(pm.yml)
endif()
