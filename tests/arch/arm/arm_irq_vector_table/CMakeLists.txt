# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(arm_irq_vector_table)

FILE(GLOB app_sources src/*.c)
target_sources(app PRIVATE ${app_sources})

zephyr_linker_sources(ROM_START SORT_KEY 0x0vectors irq-vector-table.ld)
if(CONFIG_SOC_NRF54H20_IRON)
  target_sources(app PRIVATE periphconf_migrated_src/periphconf_migrated_from_uicr.c)
endif()
