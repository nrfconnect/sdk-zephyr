# SPDX-License-Identifier: Apache-2.0

add_subdirectory(src)
zephyr_include_directories(
  include
  )

add_partition_manager_config(pm.yml)
