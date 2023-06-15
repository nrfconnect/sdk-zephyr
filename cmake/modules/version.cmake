# SPDX-License-Identifier: Apache-2.0

#.rst:
# version.cmake
# -------------
#
# Inputs:
#
#   ``*VERSION*`` and other constants set by
#   maintainers in ``${ZEPHYR_BASE}/VERSION``
#
# Outputs with examples::
#
#   PROJECT_VERSION           1.14.99.07
#   KERNEL_VERSION_STRING    "1.14.99-extraver"
#
#   KERNEL_VERSION_MAJOR       1
#   KERNEL_VERSION_MINOR        14
#   KERNEL_PATCHLEVEL             99
#   KERNELVERSION            0x10E6307
#   KERNEL_VERSION_NUMBER    0x10E63
#   ZEPHYR_VERSION_CODE        69219
#
# Most outputs are converted to C macros, see ``version.h.in``
#
# See also: independent and more dynamic ``BUILD_VERSION`` in
# ``git.cmake``.

# Note: version.cmake is loaded multiple times by ZephyrConfigVersion.cmake to
# determine this Zephyr package version and thus the correct Zephyr CMake
# package to load.
# Therefore `version.cmake` should not use include_guard(GLOBAL).
# The final load of `version.cmake` will setup correct build version values.

include(${ZEPHYR_BASE}/cmake/hex.cmake)

if(NOT DEFINED VERSION_FILE AND NOT DEFINED VERSION_TYPE)
  set(VERSION_FILE ${ZEPHYR_BASE}/VERSION ${APPLICATION_SOURCE_DIR}/VERSION)
  set(VERSION_TYPE KERNEL                 APP)
endif()

foreach(type file IN ZIP_LISTS VERSION_TYPE VERSION_FILE)
  if(NOT EXISTS ${file})
    break()
  endif()
  file(READ ${file} ver)
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${file})

  string(REGEX MATCH "VERSION_MAJOR = ([0-9]*)" _ ${ver})
  set(${type}_VERSION_MAJOR ${CMAKE_MATCH_1})

  string(REGEX MATCH "VERSION_MINOR = ([0-9]*)" _ ${ver})
  set(${type}_VERSION_MINOR ${CMAKE_MATCH_1})

  string(REGEX MATCH "PATCHLEVEL = ([0-9]*)" _ ${ver})
  set(${type}_PATCHLEVEL ${CMAKE_MATCH_1})

  string(REGEX MATCH "VERSION_TWEAK = ([0-9]*)" _ ${ver})
  set(${type}_VERSION_TWEAK ${CMAKE_MATCH_1})

  string(REGEX MATCH "EXTRAVERSION = ([a-z0-9]*)" _ ${ver})
  set(${type}_VERSION_EXTRA ${CMAKE_MATCH_1})

  # Temporary convenience variable
  set(${type}_VERSION_WITHOUT_TWEAK ${${type}_VERSION_MAJOR}.${${type}_VERSION_MINOR}.${${type}_PATCHLEVEL})


  set(MAJOR ${${type}_VERSION_MAJOR}) # Temporary convenience variable
  set(MINOR ${${type}_VERSION_MINOR}) # Temporary convenience variable
  set(PATCH ${${type}_PATCHLEVEL})    # Temporary convenience variable
  set(TWEAK ${${type}_VERSION_TWEAK}) # Temporary convenience variable

  math(EXPR ${type}_VERSION_NUMBER_INT "(${MAJOR} << 16) + (${MINOR} << 8)  + (${PATCH})")
  math(EXPR ${type}VERSION_INT         "(${MAJOR} << 24) + (${MINOR} << 16) + (${PATCH} << 8) + (${TWEAK})")

  to_hex(${${type}_VERSION_NUMBER_INT} ${type}_VERSION_NUMBER)
  to_hex(${${type}VERSION_INT}         ${type}VERSION)

  if(${type}_VERSION_EXTRA)
    set(${type}_VERSION_STRING     "${${type}_VERSION_WITHOUT_TWEAK}-${${type}_VERSION_EXTRA}")
  else()
    set(${type}_VERSION_STRING     "${${type}_VERSION_WITHOUT_TWEAK}")
  endif()

  if(type STREQUAL KERNEL)
    set(PROJECT_VERSION_MAJOR      ${${type}_VERSION_MAJOR})
    set(PROJECT_VERSION_MINOR      ${${type}_VERSION_MINOR})
    set(PROJECT_VERSION_PATCH      ${${type}_PATCHLEVEL})
    set(PROJECT_VERSION_TWEAK      ${${type}_VERSION_TWEAK})
    set(PROJECT_VERSION_EXTRA      ${${type}_VERSION_EXTRA})

    if(PROJECT_VERSION_EXTRA)
      set(PROJECT_VERSION_EXTRA_STR "-${PROJECT_VERSION_EXTRA}")
    endif()

    if(${type}_VERSION_TWEAK)
      set(PROJECT_VERSION ${${type}_VERSION_WITHOUT_TWEAK}.${${type}_VERSION_TWEAK})
    else()
      set(PROJECT_VERSION ${${type}_VERSION_WITHOUT_TWEAK})
    endif()

    set(PROJECT_VERSION_STR ${PROJECT_VERSION}${PROJECT_VERSION_EXTRA_STR})

    set(ZEPHYR_VERSION_CODE ${${type}_VERSION_NUMBER_INT})
    set(ZEPHYR_VERSION TRUE)

    if(DEFINED BUILD_VERSION)
      set(BUILD_VERSION_STR ", build: ${BUILD_VERSION}")
    endif()

    if (NOT NO_PRINT_VERSION)
        message(STATUS "Zephyr version: ${PROJECT_VERSION_STR} (${ZEPHYR_BASE})${BUILD_VERSION_STR}")
    endif()
  endif()

  # Cleanup convenience variables
  unset(MAJOR)
  unset(MINOR)
  unset(PATCH)
  unset(${type}_VERSION_WITHOUT_TWEAK)
endforeach()
