# Zephyr code can configure itself based on a KConfig'uration with the
# header file autoconf.h. There exists an analogous file
# generated_dts_board.h that allows configuration based on information
# encoded in DTS.
#
# Here we call on dtc, the gcc preprocessor, and
# scripts/dts/extract_dts_includes.py to generate this header file at
# CMake configure-time.
#
# See ~/zephyr/doc/dts
set(GENERATED_DTS_BOARD_H    ${PROJECT_BINARY_DIR}/include/generated/generated_dts_board.h)
set(GENERATED_DTS_BOARD_CONF ${PROJECT_BINARY_DIR}/include/generated/generated_dts_board.conf)
set_ifndef(DTS_SOURCE ${BOARD_ROOT}/boards/${ARCH}/${BOARD_FAMILY}/${BOARD}.dts)
set_ifndef(DTS_COMMON_OVERLAYS ${PROJECT_SOURCE_DIR}/dts/common/common.dts)

message(STATUS "Generating zephyr/include/generated/generated_dts_board.h")

if(CONFIG_HAS_DTS)

  if(DTC_OVERLAY_FILE)
    # Convert from space-separated files into file list
    string(REPLACE " " ";" DTC_OVERLAY_FILE_AS_LIST ${DTC_OVERLAY_FILE})
  endif()

  # Prepend common overlays
  set(DTC_OVERLAY_FILE_AS_LIST ${DTS_COMMON_OVERLAYS} ${DTC_OVERLAY_FILE_AS_LIST})

  set(
    dts_files
    ${DTS_SOURCE}
    ${DTC_OVERLAY_FILE_AS_LIST}
  )

  unset(DTC_INCLUDE_FLAG_FOR_DTS)
  foreach(dts_file ${dts_files})
    list(APPEND DTC_INCLUDE_FLAG_FOR_DTS
         -include ${dts_file})
  endforeach()

  # TODO: Cut down on CMake configuration time by avoiding
  # regeneration of generated_dts_board.h on every configure. How
  # challenging is this? What are the dts dependencies? We run the
  # preprocessor, and it seems to be including all kinds of
  # directories with who-knows how many header files.

  # Run the C preprocessor on an empty C source file that has one or
  # more DTS source files -include'd into it to create the
  # intermediary file *.dts.pre.tmp
  execute_process(
    COMMAND ${CMAKE_C_COMPILER}
    -x assembler-with-cpp
    -nostdinc
    -I${PROJECT_SOURCE_DIR}/arch/${ARCH}/soc
    -isystem ${PROJECT_SOURCE_DIR}/include
    -isystem ${PROJECT_SOURCE_DIR}/dts/${ARCH}
    -isystem ${PROJECT_SOURCE_DIR}/dts
    -include ${AUTOCONF_H}
    ${DTC_INCLUDE_FLAG_FOR_DTS}  # include the DTS source and overlays
    -I${PROJECT_SOURCE_DIR}/dts/common
    -I${PROJECT_SOURCE_DIR}/drivers
    -undef -D__DTS__
    -P
    -E ${ZEPHYR_BASE}/misc/empty_file.c
    -o ${BOARD}.dts.pre.tmp
    WORKING_DIRECTORY ${PROJECT_BINARY_DIR}
    RESULT_VARIABLE ret
    )
  if(NOT "${ret}" STREQUAL "0")
    message(FATAL_ERROR "command failed with return code: ${ret}")
  endif()

  # Run the DTC on *.dts.pre.tmp to create the intermediary file *.dts_compiled
  execute_process(
    COMMAND ${DTC}
    -O dts
    -o ${BOARD}.dts_compiled
    -b 0
    ${BOARD}.dts.pre.tmp
    WORKING_DIRECTORY ${PROJECT_BINARY_DIR}
    RESULT_VARIABLE ret
    )
  if(NOT "${ret}" STREQUAL "0")
    message(FATAL_ERROR "command failed with return code: ${ret}")
  endif()

  # Run extract_dts_includes.py for the header file
  # generated_dts_board.h
  set_ifndef(DTS_BOARD_FIXUP_FILE ${BOARD_ROOT}/boards/${ARCH}/${BOARD_FAMILY}/dts.fixup)
  if(EXISTS ${DTS_BOARD_FIXUP_FILE})
    set(DTS_BOARD_FIXUP -f ${DTS_BOARD_FIXUP_FILE})
  endif()
  set_ifndef(DTS_SOC_FIXUP_FILE ${PROJECT_SOURCE_DIR}/arch/${ARCH}/soc/${SOC_PATH}/dts.fixup)
  if(EXISTS ${DTS_SOC_FIXUP_FILE})
    set(DTS_SOC_FIXUP -f ${DTS_SOC_FIXUP_FILE})
  endif()
  if(EXISTS ${APPLICATION_SOURCE_DIR}/dts.fixup)
    set(DTS_APP_FIXUP -f ${APPLICATION_SOURCE_DIR}/dts.fixup)
  endif()
  set(CMD_EXTRACT_DTS_INCLUDES ${PYTHON_EXECUTABLE} ${PROJECT_SOURCE_DIR}/scripts/dts/extract_dts_includes.py
    --dts ${BOARD}.dts_compiled
    --yaml ${PROJECT_SOURCE_DIR}/dts/bindings
    ${DTS_SOC_FIXUP} ${DTS_BOARD_FIXUP} ${DTS_APP_FIXUP}
    )
  execute_process(
    COMMAND ${CMD_EXTRACT_DTS_INCLUDES}
    OUTPUT_VARIABLE STDOUT
    WORKING_DIRECTORY ${PROJECT_BINARY_DIR}
    RESULT_VARIABLE ret
    )
  if(NOT "${ret}" STREQUAL "0")
    message(FATAL_ERROR "command failed with return code: ${ret}")
  endif()

  # extract_dts_includes.py writes the header file contents to stdout,
  # which we capture in the variable STDOUT and then finaly write into
  # the header file.
  file(WRITE ${GENERATED_DTS_BOARD_H} "${STDOUT}" )

  # Run extract_dts_includes.py to create a .conf file that can be
  # included into the CMake namespace
  execute_process(
    COMMAND ${CMD_EXTRACT_DTS_INCLUDES} --keyvalue
    OUTPUT_VARIABLE STDOUT
    WORKING_DIRECTORY ${PROJECT_BINARY_DIR}
    RESULT_VARIABLE ret
    )
  if(NOT "${ret}" STREQUAL "0")
    message(FATAL_ERROR "command failed with return code: ${ret}")
  endif()

  file(WRITE ${GENERATED_DTS_BOARD_CONF} "${STDOUT}" )
  import_kconfig(${GENERATED_DTS_BOARD_CONF})

else()
  file(WRITE ${GENERATED_DTS_BOARD_H} "/* WARNING. THIS FILE IS AUTO-GENERATED. DO NOT MODIFY! */")
endif()
