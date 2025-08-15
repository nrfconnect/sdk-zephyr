# SPDX-License-Identifier: Apache-2.0
#
# The code in this CMakeLists.txt constructs the arguments for gen_uicr.py
# and creates a flashable zephyr.hex file containing UICR data (no C code compiled)
#

cmake_minimum_required(VERSION 3.20.0)

# Instead of adding all of Zephyr we add just the subset that is
# required to generate uicr.hex.
#
# The generation of uicr.hex is configured by this image, so we
# include modules from zephyr_default up until kconfig.

find_package(Zephyr
  COMPONENTS zephyr_default:kconfig
  REQUIRED HINTS $ENV{ZEPHYR_BASE}
  )

project(uicr)

# Use CMAKE_VERBOSE_MAKEFILE to silence an unused-variable warning.
if(CMAKE_VERBOSE_MAKEFILE)
endif()

set(periphconf_args)
set(periphconf_elfs)
set(zephyr_hex_file ${APPLICATION_BINARY_DIR}/zephyr/zephyr.hex)

# Compute UICR absolute address from this image's devicetree
dt_nodelabel(uicr_path NODELABEL "uicr")

if(NOT DEFINED uicr_path)
  message(FATAL_ERROR "uicr node not found in devicetree")
endif()

dt_reg_addr(UICR_ADDRESS PATH ${uicr_path})

if(NOT DEFINED UICR_ADDRESS)
  message(FATAL_ERROR "Failed to read UICR address from DT")
endif()

if(CONFIG_UICR_SAMPLE_GENERATE_PERIPHCONF)
  # gen_uicr.py parses all zephyr.elf files. To find these files (which
  # have not been built yet) we scan sibling build directories for
  # zephyr.dts
  get_filename_component(SYSBUILD_DIR ${APPLICATION_BINARY_DIR} DIRECTORY)
  file(GLOB _siblings LIST_DIRECTORIES true "${SYSBUILD_DIR}/*")
  foreach(_dir ${_siblings})
    get_filename_component(_name ${_dir} NAME)
    if(_name STREQUAL "uicr")
      # This image is an exception to the rule. It has a zephyr.dts, but
      # no zephyr.elf
      continue()
    endif()

    if(EXISTS ${_dir}/zephyr/zephyr.dts)
      list(APPEND periphconf_elfs ${_dir}/zephyr/zephyr.elf)
    endif()
  endforeach()

  # Compute PERIPHCONF absolute address and size from this image's devicetree
  dt_nodelabel(periph_path NODELABEL "periphconf_partition")
  if(NOT DEFINED periph_path)
    message(FATAL_ERROR "periphconf_partition not found in devicetree")
  endif()

  dt_reg_addr(PERIPHCONF_OFFSET PATH ${periph_path})
  dt_reg_size(PERIPHCONF_SIZE PATH ${periph_path})

  if(NOT DEFINED PERIPHCONF_OFFSET OR NOT DEFINED PERIPHCONF_SIZE)
    message(FATAL_ERROR "Failed to read periphconf addresses from DT")
  endif()

  # Calculate absolute periphconf address
  math(EXPR PERIPHCONF_ADDRESS "${CONFIG_FLASH_BASE_ADDRESS} + ${PERIPHCONF_OFFSET}" OUTPUT_FORMAT HEXADECIMAL)

  # Set up periphconf arguments for gen_uicr.py
  list(APPEND periphconf_args --periphconf-address ${PERIPHCONF_ADDRESS})
  list(APPEND periphconf_args --periphconf-size ${PERIPHCONF_SIZE})

  foreach(elf ${periphconf_elfs})
    list(APPEND periphconf_args --in-periphconf-elf ${elf})
  endforeach()
endif(CONFIG_UICR_SAMPLE_GENERATE_PERIPHCONF)

# Generate zephyr.hex file
add_custom_command(
  OUTPUT ${zephyr_hex_file}
  COMMAND ${CMAKE_COMMAND} -E env PYTHONPATH=${ZEPHYR_BASE}/scripts/dts/python-devicetree/src
          ${PYTHON_EXECUTABLE} ${ZEPHYR_BASE}/soc/nordic/common/uicr/gen_uicr.py
          --uicr-address ${UICR_ADDRESS}
          --out-uicr-hex ${zephyr_hex_file}
          ${periphconf_args}
  DEPENDS ${periphconf_elfs}
  WORKING_DIRECTORY ${APPLICATION_BINARY_DIR}
  COMMENT "Using gen_uicr.py to generate ${zephyr_hex_file}"
)

# Add zephyr subdirectory to handle flash configuration with correct paths
add_subdirectory(zephyr)

add_custom_target(gen_uicr ALL DEPENDS ${zephyr_hex_file})
