# SPDX-License-Identifier: Apache-2.0
#
# Copyright (c) 2022, Nordic Semiconductor ASA

# FindDeprecated module provides a single location for deprecated CMake build code.
# Whenever CMake code is deprecated it should be moved to this module and
# corresponding COMPONENTS should be created with name identifying the deprecated code.
#
# This makes it easier to maintain deprecated code and cleanup such code when it
# has been deprecated for two releases.
#
# Example:
# CMakeList.txt contains deprecated code, like:
# if(DEPRECATED_VAR)
#   deprecated()
# endif()
#
# such code can easily be around for a long time, so therefore such code should
# be moved to this module and can then be loaded as:
# FindDeprecated.cmake
# if(<deprecated_name> IN_LIST Deprecated_FIND_COMPONENTS)
#   # This code has been deprecated after Zephyr x.y
#   if(DEPRECATED_VAR)
#     deprecated()
#   endif()
# endif()
#
# and then in the original CMakeLists.txt, this code is inserted instead:
# find_package(Deprecated COMPONENTS <deprecated_name>)
#
# The module defines the following variables:
#
# 'Deprecated_FOUND', 'DEPRECATED_FOUND'
# True if the Deprecated component was found and loaded.

if("${Deprecated_FIND_COMPONENTS}" STREQUAL "")
  message(WARNING "find_package(Deprecated) missing required COMPONENTS keyword")
endif()

if("XCC_USE_CLANG" IN_LIST Deprecated_FIND_COMPONENTS)
  list(REMOVE_ITEM Deprecated_FIND_COMPONENTS XCC_USE_CLANG)
  # This code was deprecated after Zephyr v3.0.0
  # Keep XCC_USE_CLANG behaviour for a while.
  if(NOT DEFINED ZEPHYR_TOOLCHAIN_VARIANT)
    set(ZEPHYR_TOOLCHAIN_VARIANT $ENV{ZEPHYR_TOOLCHAIN_VARIANT})
  endif()

  if ("${ZEPHYR_TOOLCHAIN_VARIANT}" STREQUAL "xcc"
      AND "$ENV{XCC_USE_CLANG}" STREQUAL "1")
    set(ZEPHYR_TOOLCHAIN_VARIANT xt-clang CACHE STRING "Zephyr toolchain variant" FORCE)
    message(DEPRECATION "XCC_USE_CLANG is deprecated. Please set ZEPHYR_TOOLCHAIN_VARIANT to 'xt-clang'")
  endif()

  if("${ZEPHYR_TOOLCHAIN_VARIANT}" STREQUAL "xcc-clang")
    set(ZEPHYR_TOOLCHAIN_VARIANT xt-clang CACHE STRING "Zephyr toolchain variant" FORCE)
    message(DEPRECATION "ZEPHYR_TOOLCHAIN_VARIANT 'xcc-clang' is deprecated. Please set ZEPHYR_TOOLCHAIN_VARIANT to 'xt-clang'")
  endif()
endif()

if("CROSS_COMPILE" IN_LIST Deprecated_FIND_COMPONENTS)
  list(REMOVE_ITEM Deprecated_FIND_COMPONENTS CROSS_COMPILE)
  # This code was deprecated after Zephyr v3.1.0
  if(NOT DEFINED ZEPHYR_TOOLCHAIN_VARIANT)
    set(ZEPHYR_TOOLCHAIN_VARIANT $ENV{ZEPHYR_TOOLCHAIN_VARIANT})
  endif()

  if(NOT ZEPHYR_TOOLCHAIN_VARIANT AND
     (CROSS_COMPILE OR (DEFINED ENV{CROSS_COMPILE})))
      set(ZEPHYR_TOOLCHAIN_VARIANT cross-compile CACHE STRING "Zephyr toolchain variant" FORCE)
      message(DEPRECATION  "Setting CROSS_COMPILE without setting ZEPHYR_TOOLCHAIN_VARIANT is deprecated."
                           "Please set ZEPHYR_TOOLCHAIN_VARIANT to 'cross-compile'"
      )
  endif()
endif()

if("XTOOLS" IN_LIST Deprecated_FIND_COMPONENTS)
  list(REMOVE_ITEM Deprecated_FIND_COMPONENTS XTOOLS)
  # This code was deprecated after Zephyr v3.3.0
  # When removing support for `xtools`, remember to also remove:
  # cmake/toolchain/xtools (folder with files)
  # doc/develop/toolchains/crosstool_ng.rst and update the index.rst file.
  message(DEPRECATION "XTOOLS toolchain variant is deprecated. "
                      "Please set ZEPHYR_TOOLCHAIN_VARIANT to 'zephyr'")
endif()

if("SPARSE" IN_LIST Deprecated_FIND_COMPONENTS)
  list(REMOVE_ITEM Deprecated_FIND_COMPONENTS SPARSE)
  # This code was deprecated after Zephyr v3.2.0
  if(SPARSE)
    message(DEPRECATION
        "Setting SPARSE='${SPARSE}' is deprecated. "
        "Please set ZEPHYR_SCA_VARIANT to 'sparse'"
    )
    if("${SPARSE}" STREQUAL "y")
      set_ifndef(ZEPHYR_SCA_VARIANT sparse)
    endif()
  endif()
endif()

if("SOURCES" IN_LIST Deprecated_FIND_COMPONENTS)
  list(REMOVE_ITEM Deprecated_FIND_COMPONENTS SOURCES)
  if(SOURCES)
    message(DEPRECATION
        "Setting SOURCES prior to calling find_package() for unit tests is deprecated."
        " To add sources after find_package() use:\n"
        "    target_sources(testbinary PRIVATE <source-file.c>)")
  endif()
endif()

if("PRJ_BOARD" IN_LIST Deprecated_FIND_COMPONENTS)
  # This code was deprecated after Zephyr v3.3.0
  list(REMOVE_ITEM Deprecated_FIND_COMPONENTS PRJ_BOARD)
  message(DEPRECATION "'prj_<board>.conf' files are deprecated and should be "
                      "replaced with board Kconfig fragments instead.")
endif()

if(NOT "${Deprecated_FIND_COMPONENTS}" STREQUAL "")
  message(STATUS "The following deprecated component(s) could not be found: "
                 "${Deprecated_FIND_COMPONENTS}")
endif()

set(Deprecated_FOUND True)
set(DEPRECATED_FOUND True)
