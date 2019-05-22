# SPDX-License-Identifier: Apache-2.0

# Configuration for host installed clang
#

set(NOSTDINC "")

# Note that NOSYSDEF_CFLAG may be an empty string, and
# set_ifndef() does not work with empty string.
if(NOT DEFINED NOSYSDEF_CFLAG)
  set(NOSYSDEF_CFLAG -undef)
endif()

if(DEFINED TOOLCHAIN_HOME)
  set(find_program_clang_args PATH ${TOOLCHAIN_HOME} NO_DEFAULT_PATH)
  set(find_program_binutils_args PATH ${TOOLCHAIN_HOME})
endif()


find_program(CMAKE_C_COMPILER    clang          ${find_program_clang_args})
find_program(CMAKE_CXX_COMPILER  clang++        ${find_program_clang_args})
find_program(CMAKE_AR            llvm-ar        ${find_program_clang_args})
find_program(CMAKE_LINKER        llvm-link      ${find_program_clang_args})
find_program(CMAKE_NM            llvm-nm        ${find_program_clang_args})
find_program(CMAKE_OBJDUMP       llvm-objdump   ${find_program_clang_args})
find_program(CMAKE_RANLIB        llvm-ranlib    ${find_program_clang_args})

find_program(CMAKE_OBJCOPY       objcopy        ${find_program_binutils_args})
find_program(CMAKE_READELF       readelf        ${find_program_binutils_args})

if(NOT "${ARCH}" STREQUAL "posix")

  foreach(file_name include include-fixed)
    execute_process(
      COMMAND ${CMAKE_C_COMPILER} --print-file-name=${file_name}
      OUTPUT_VARIABLE _OUTPUT
      )
    string(REGEX REPLACE "\n" "" _OUTPUT ${_OUTPUT})

    list(APPEND NOSTDINC ${_OUTPUT})
  endforeach()

  foreach(isystem_include_dir ${NOSTDINC})
    list(APPEND isystem_include_flags -isystem ${isystem_include_dir})
  endforeach()

  # This libgcc code is partially duplicated in compiler/*/target.cmake
  execute_process(
    COMMAND ${CMAKE_C_COMPILER} ${TOOLCHAIN_C_FLAGS} --print-libgcc-file-name
    OUTPUT_VARIABLE LIBGCC_FILE_NAME
    OUTPUT_STRIP_TRAILING_WHITESPACE
    )

  assert_exists(LIBGCC_FILE_NAME)

  get_filename_component(LIBGCC_DIR ${LIBGCC_FILE_NAME} DIRECTORY)

  assert_exists(LIBGCC_DIR)

  list(APPEND LIB_INCLUDE_DIR "-L\"${LIBGCC_DIR}\"")
  list(APPEND TOOLCHAIN_LIBS gcc)

  set(CMAKE_REQUIRED_FLAGS -nostartfiles -nostdlib ${isystem_include_flags} -Wl,--unresolved-symbols=ignore-in-object-files)
  string(REPLACE ";" " " CMAKE_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS}")

endif()

# Load toolchain_cc-family macros

macro(toolchain_cc_nostdinc)
  if(NOT "${ARCH}" STREQUAL "posix")
    zephyr_compile_options( -nostdinc)
  endif()
endmacro()

# Clang and GCC are almost feature+flag compatible, so reuse freestanding gcc
include(${ZEPHYR_BASE}/cmake/compiler/gcc/target_security_canaries.cmake)
include(${ZEPHYR_BASE}/cmake/compiler/gcc/target_optimizations.cmake)
include(${ZEPHYR_BASE}/cmake/compiler/gcc/target_cpp.cmake)
include(${ZEPHYR_BASE}/cmake/compiler/gcc/target_asm.cmake)
include(${ZEPHYR_BASE}/cmake/compiler/gcc/target_baremetal.cmake)

macro(toolchain_cc_security_fortify)
  # No op, clang doesn't understand fortify at all
endmacro()

macro(toolchain_cc_no_freestanding_options)
  # No op, this is used by the native_posix, clang has problems
  # compiling the native_posix with -fno-freestanding.
endmacro()
