# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(code_relocation_nocopy)

FILE(GLOB app_sources src/*.c)
target_sources(app PRIVATE ${app_sources})

# For nRF54L15, use SRAM2 region for code relocation
# SRAM2 is a 4KB region carved from the top of SRAM (0x2002E000 - 0x2002F000)
# This keeps KMU section at exact RAM start address 0x20000000
if(CONFIG_SOC_NRF54L15_CPUAPP)
  # Relocate sram_code.c to SRAM2 region (defined in overlay and linker script)
  zephyr_code_relocate(FILES src/sram_code.c LOCATION SRAM2)
else()
  # For other platforms, use default RAM location
  zephyr_code_relocate(FILES src/sram_code.c LOCATION RAM)
endif()
