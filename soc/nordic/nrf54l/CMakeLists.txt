# Copyright (c) 2024 Nordic Semiconductor ASA
# SPDX-License-Identifier: Apache-2.0

zephyr_library_sources(
  soc.c
  ../validate_rram_partitions.c
  )
zephyr_include_directories(.)

if(CONFIG_ELV_GRTC_LFXO_ALLOWED)
  message(WARNING "WARNING! ELV mode feature is EXPERIMENTAL and may brick your device!")
endif()

# We need a buffer in memory in a static location which can be used by
# the KMU peripheral. The KMU has a static destination address so we need
# to make sure that this buffer is always at the start of RAM.
if(NOT CONFIG_BUILD_WITH_TFM)
zephyr_linker_sources(RAM_SECTIONS kmu_push_area_section.ld)
endif()
