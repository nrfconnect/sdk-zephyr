# Copyright (c) 2024 Nordic Semiconductor
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(comparator_gpio_loopback)

target_sources(app PRIVATE src/test.c)
if(CONFIG_SOC_NRF54H20_IRON)
  target_sources(app PRIVATE periphconf_migrated_src/periphconf_migrated_from_uicr.c)
endif()
