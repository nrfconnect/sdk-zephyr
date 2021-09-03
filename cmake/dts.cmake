# SPDX-License-Identifier: Apache-2.0

file(MAKE_DIRECTORY ${PROJECT_BINARY_DIR}/include/generated)

# Zephyr code can configure itself based on a KConfig'uration with the
# header file autoconf.h. There exists an analogous file devicetree_unfixed.h
# that allows configuration based on information encoded in DTS.
#
# Here we call on dtc, the gcc preprocessor and
# scripts/dts/gen_defines.py to generate various DT-related files at
# CMake configure-time.
#
# See the Devicetree user guide in the Zephyr documentation for details.
set(GEN_DEFINES_SCRIPT          ${ZEPHYR_BASE}/scripts/dts/gen_defines.py)
set(ZEPHYR_DTS                  ${PROJECT_BINARY_DIR}/zephyr.dts)
# This contains the edtlib.EDT object created from zephyr.dts in Python's
# pickle data marshalling format (https://docs.python.org/3/library/pickle.html)
#
# Its existence is an implementation detail used to speed up further
# use of the devicetree by processes that run later on in the build,
# and should not be made part of the documentation.
set(EDT_PICKLE                  ${PROJECT_BINARY_DIR}/edt.pickle)
set(DEVICETREE_UNFIXED_H        ${PROJECT_BINARY_DIR}/include/generated/devicetree_unfixed.h)
set(DEVICE_EXTERN_H             ${PROJECT_BINARY_DIR}/include/generated/device_extern.h)
set(DTS_POST_CPP                ${PROJECT_BINARY_DIR}/${BOARD}.dts.pre.tmp)

set_ifndef(DTS_SOURCE ${BOARD_DIR}/${BOARD}.dts)

zephyr_file(APPLICATION_ROOT DTS_ROOT)

# 'DTS_ROOT' is a list of directories where a directory tree with DT
# files may be found. It always includes the application directory,
# the board directory, and ${ZEPHYR_BASE}.
list(APPEND
  DTS_ROOT
  ${APPLICATION_SOURCE_DIR}
  ${BOARD_DIR}
  ${SHIELD_DIRS}
  ${ZEPHYR_BASE}
  )
list(REMOVE_DUPLICATES
  DTS_ROOT
  )

# TODO: What to do about non-posix platforms where NOT CONFIG_HAS_DTS (xtensa)?
# Drop support for NOT CONFIG_HAS_DTS perhaps?
if(EXISTS ${DTS_SOURCE})
  set(SUPPORTS_DTS 1)
  if(BOARD_REVISION AND EXISTS ${BOARD_DIR}/${BOARD}_${BOARD_REVISION_STRING}.overlay)
    list(APPEND DTS_SOURCE ${BOARD_DIR}/${BOARD}_${BOARD_REVISION_STRING}.overlay)
  endif()
else()
  set(SUPPORTS_DTS 0)
endif()

set(dts_files
  ${DTS_SOURCE}
  ${shield_dts_files}
  )

if(SUPPORTS_DTS)
  if(DTC_OVERLAY_FILE)
    # Convert from space-separated files into file list
    string(REPLACE " " ";" DTC_OVERLAY_FILE_RAW_LIST "${DTC_OVERLAY_FILE}")
    foreach(file ${DTC_OVERLAY_FILE_RAW_LIST})
      file(TO_CMAKE_PATH "${file}" cmake_path_file)
      list(APPEND DTC_OVERLAY_FILE_AS_LIST ${cmake_path_file})
    endforeach()
    list(APPEND
      dts_files
      ${DTC_OVERLAY_FILE_AS_LIST}
      )
  endif()

  set(i 0)
  unset(DTC_INCLUDE_FLAG_FOR_DTS)
  foreach(dts_file ${dts_files})
    list(APPEND DTC_INCLUDE_FLAG_FOR_DTS
         -include ${dts_file})

    if(i EQUAL 0)
      message(STATUS "Found BOARD.dts: ${dts_file}")
    else()
      message(STATUS "Found devicetree overlay: ${dts_file}")
    endif()

    math(EXPR i "${i}+1")
  endforeach()

  unset(DTS_ROOT_SYSTEM_INCLUDE_DIRS)
  foreach(dts_root ${DTS_ROOT})
    foreach(dts_root_path
        include
        dts/common
        dts/${ARCH}
        dts
        )
      get_filename_component(full_path ${dts_root}/${dts_root_path} REALPATH)
      if(EXISTS ${full_path})
        list(APPEND
          DTS_ROOT_SYSTEM_INCLUDE_DIRS
          -isystem ${full_path}
          )
      endif()
    endforeach()
  endforeach()

  unset(DTS_ROOT_BINDINGS)
  foreach(dts_root ${DTS_ROOT})
    set(full_path ${dts_root}/dts/bindings)
    if(EXISTS ${full_path})
      list(APPEND
        DTS_ROOT_BINDINGS
        ${full_path}
        )
    endif()
  endforeach()

  # Cache the location of the root bindings so they can be used by
  # scripts which use the build directory.
  set(CACHED_DTS_ROOT_BINDINGS ${DTS_ROOT_BINDINGS} CACHE INTERNAL
    "DT bindings root directories")

  if(NOT DEFINED CMAKE_DTS_PREPROCESSOR)
    set(CMAKE_DTS_PREPROCESSOR ${CMAKE_C_COMPILER})
  endif()

  # TODO: Cut down on CMake configuration time by avoiding
  # regeneration of devicetree_unfixed.h on every configure. How
  # challenging is this? What are the dts dependencies? We run the
  # preprocessor, and it seems to be including all kinds of
  # directories with who-knows how many header files.

  # Run the C preprocessor on an empty C source file that has one or
  # more DTS source files -include'd into it to create the
  # intermediary file *.dts.pre.tmp. Also, generate a dependency file
  # so that changes to DT sources are detected.
  execute_process(
    COMMAND ${CMAKE_DTS_PREPROCESSOR}
    -x assembler-with-cpp
    -nostdinc
    ${DTS_ROOT_SYSTEM_INCLUDE_DIRS}
    ${DTC_INCLUDE_FLAG_FOR_DTS}  # include the DTS source and overlays
    ${NOSYSDEF_CFLAG}
    -D__DTS__
    ${DTS_EXTRA_CPPFLAGS}
    -P
    -E   # Stop after preprocessing
    -MD  # Generate a dependency file as a side-effect
    -MF ${PROJECT_BINARY_DIR}/${BOARD}.dts.pre.d
    -o  ${PROJECT_BINARY_DIR}/${BOARD}.dts.pre.tmp
    ${ZEPHYR_BASE}/misc/empty_file.c
    WORKING_DIRECTORY ${APPLICATION_SOURCE_DIR}
    RESULT_VARIABLE ret
    )
  if(NOT "${ret}" STREQUAL "0")
    message(FATAL_ERROR "command failed with return code: ${ret}")
  endif()

  # Parse the generated dependency file to find the DT sources that
  # were included and then add them to the list of files that trigger
  # a re-run of CMake.
  toolchain_parse_make_rule(
    ${PROJECT_BINARY_DIR}/${BOARD}.dts.pre.d
    include_files # Output parameter
    )

  set_property(DIRECTORY APPEND PROPERTY
    CMAKE_CONFIGURE_DEPENDS
    ${include_files}
    ${GEN_DEFINES_SCRIPT}
    )

  #
  # Run the C devicetree compiler on *.dts.pre.tmp, just to catch any
  # warnings/errors from it. dtlib and edtlib parse the devicetree files
  # themselves, so we don't rely on the C compiler otherwise.
  #

  if(DTC)
  set(DTC_WARN_UNIT_ADDR_IF_ENABLED "")
  check_dtc_flag("-Wunique_unit_address_if_enabled" check)
  if (check)
    set(DTC_WARN_UNIT_ADDR_IF_ENABLED "-Wunique_unit_address_if_enabled")
  endif()
  set(DTC_NO_WARN_UNIT_ADDR "")
  check_dtc_flag("-Wno-unique_unit_address" check)
  if (check)
    set(DTC_NO_WARN_UNIT_ADDR "-Wno-unique_unit_address")
  endif()
  set(VALID_EXTRA_DTC_FLAGS "")
  foreach(extra_opt ${EXTRA_DTC_FLAGS})
    check_dtc_flag(${extra_opt} check)
    if (check)
      list(APPEND VALID_EXTRA_DTC_FLAGS ${extra_opt})
    endif()
  endforeach()
  set(EXTRA_DTC_FLAGS ${VALID_EXTRA_DTC_FLAGS})
  execute_process(
    COMMAND ${DTC}
    -O dts
    -o - # Write output to stdout, which we discard below
    -b 0
    -E unit_address_vs_reg
    ${DTC_NO_WARN_UNIT_ADDR}
    ${DTC_WARN_UNIT_ADDR_IF_ENABLED}
    ${EXTRA_DTC_FLAGS} # User settable
    ${BOARD}.dts.pre.tmp
    OUTPUT_QUIET # Discard stdout
    WORKING_DIRECTORY ${PROJECT_BINARY_DIR}
    RESULT_VARIABLE ret
    )
  if(NOT "${ret}" STREQUAL "0")
    message(FATAL_ERROR "command failed with return code: ${ret}")
  endif()
  endif(DTC)

  #
  # Run gen_defines.py to create a header file, zephyr.dts, and edt.pickle.
  #

  string(REPLACE ";" " " EXTRA_DTC_FLAGS_RAW "${EXTRA_DTC_FLAGS}")
  set(CMD_EXTRACT ${PYTHON_EXECUTABLE} ${GEN_DEFINES_SCRIPT}
  --dts ${BOARD}.dts.pre.tmp
  --dtc-flags '${EXTRA_DTC_FLAGS_RAW}'
  --bindings-dirs ${DTS_ROOT_BINDINGS}
  --header-out ${DEVICETREE_UNFIXED_H}
  --device-header-out ${DEVICE_EXTERN_H}
  --dts-out ${ZEPHYR_DTS} # As a debugging aid
  --edt-pickle-out ${EDT_PICKLE}
  ${EXTRA_GEN_DEFINES_ARGS}
  )

  execute_process(
    COMMAND ${CMD_EXTRACT}
    WORKING_DIRECTORY ${PROJECT_BINARY_DIR}
    RESULT_VARIABLE ret
    )
  if(NOT "${ret}" STREQUAL "0")
    message(FATAL_ERROR "gen_defines.py failed with return code: ${ret}")
  else()
    message(STATUS "Generated zephyr.dts: ${ZEPHYR_DTS}")
    message(STATUS "Generated devicetree_unfixed.h: ${DEVICETREE_UNFIXED_H}")
    message(STATUS "Generated device_extern.h: ${DEVICE_EXTERN_H}")
  endif()

else()
  file(WRITE ${DEVICETREE_UNFIXED_H} "/* WARNING. THIS FILE IS AUTO-GENERATED. DO NOT MODIFY! */")
  file(WRITE ${DEVICE_EXTERN_H} "/* WARNING. THIS FILE IS AUTO-GENERATED. DO NOT MODIFY! */")
endif(SUPPORTS_DTS)
