set_ifndef(C++ g++)

# Configures CMake for using GCC, this script is re-used by several
# GCC-based toolchains

find_program(CMAKE_C_COMPILER ${CROSS_COMPILE}${CC}   PATH ${TOOLCHAIN_HOME} NO_DEFAULT_PATH)
find_program(CMAKE_OBJCOPY    ${CROSS_COMPILE}objcopy PATH ${TOOLCHAIN_HOME} NO_DEFAULT_PATH)
find_program(CMAKE_OBJDUMP    ${CROSS_COMPILE}objdump PATH ${TOOLCHAIN_HOME} NO_DEFAULT_PATH)
find_program(CMAKE_AS         ${CROSS_COMPILE}as      PATH ${TOOLCHAIN_HOME} NO_DEFAULT_PATH)
find_program(CMAKE_LINKER     ${CROSS_COMPILE}ld      PATH ${TOOLCHAIN_HOME} NO_DEFAULT_PATH)
find_program(CMAKE_AR         ${CROSS_COMPILE}ar      PATH ${TOOLCHAIN_HOME} NO_DEFAULT_PATH)
find_program(CMAKE_RANLIB     ${CROSS_COMPILE}ranlib  PATH ${TOOLCHAIN_HOME} NO_DEFAULT_PATH)
find_program(CMAKE_READELF    ${CROSS_COMPILE}readelf PATH ${TOOLCHAIN_HOME} NO_DEFAULT_PATH)
find_program(CMAKE_GDB        ${CROSS_COMPILE}gdb     PATH ${TOOLCHAIN_HOME} NO_DEFAULT_PATH)
find_program(CMAKE_NM         ${CROSS_COMPILE}nm      PATH ${TOOLCHAIN_HOME} NO_DEFAULT_PATH)

# x86_64 should pick up a proper cross compiler if one is provided,
# but falling back to using the host toolchain is a very sane behavior
# too.
if(CONFIG_X86_64)
  if(CMAKE_C_COMPILER STREQUAL CMAKE_C_COMPILER-NOTFOUND)
    find_program(CMAKE_C_COMPILER   gcc    )
    find_program(CMAKE_OBJCOPY      objcopy)
    find_program(CMAKE_OBJDUMP      objdump)
    find_program(CMAKE_AR           ar     )
    find_program(CMAKE_RANLILB      ranlib )
    find_program(CMAKE_READELF      readelf)
    find_program(CMAKE_GDB          gdb    )
  endif()

  # When building in x32 mode with a host compiler, there is no libgcc
  # shipped (because it's an x86_64 compiler, not x32).  That's
  # actually non-fatal, as no known features we hit in existing code
  # actually require the library.  But I can't find an exaustive list
  # of exactly what can break, so this is fragile.  Long term we
  # really need to be blessing a proper cross toolchain.
  set(no_libgcc Y)
endif()

if(CONFIG_CPLUSPLUS)
  set(cplusplus_compiler ${CROSS_COMPILE}${C++})
else()
  if(EXISTS ${CROSS_COMPILE}${C++})
    set(cplusplus_compiler ${CROSS_COMPILE}${C++})
  else()
    # When the toolchain doesn't support C++, and we aren't building
    # with C++ support just set it to something so CMake doesn't
    # crash, it won't actually be called
    set(cplusplus_compiler ${CMAKE_C_COMPILER})
  endif()
endif()
find_program(CMAKE_CXX_COMPILER ${cplusplus_compiler} PATH ${TOOLCHAIN_HOME} NO_DEFAULT_PATH)

set(NOSTDINC "")

# Note that NOSYSDEF_CFLAG may be an empty string, and
# set_ifndef() does not work with empty string.
if(NOT DEFINED NOSYSDEF_CFLAG)
  set(NOSYSDEF_CFLAG -undef)
endif()

foreach(file_name include include-fixed)
  execute_process(
    COMMAND ${CMAKE_C_COMPILER} --print-file-name=${file_name}
    OUTPUT_VARIABLE _OUTPUT
    )
  string(REGEX REPLACE "\n" "" _OUTPUT "${_OUTPUT}")

  list(APPEND NOSTDINC ${_OUTPUT})
endforeach()

if("${ZEPHYR_TOOLCHAIN_VARIANT}" STREQUAL "xcc")

  list(APPEND TOOLCHAIN_LIBS
    gcc
    hal
    )

else()
  include(${ZEPHYR_BASE}/cmake/gcc-m-cpu.cmake)

  if("${ARCH}" STREQUAL "arm")
    list(APPEND TOOLCHAIN_C_FLAGS
      -mthumb
      -mcpu=${GCC_M_CPU}
      )
    list(APPEND TOOLCHAIN_LD_FLAGS
      -mthumb
      -mcpu=${GCC_M_CPU}
      )

    include(${ZEPHYR_BASE}/cmake/fpu-for-gcc-m-cpu.cmake)

    if(CONFIG_FLOAT)
      list(APPEND TOOLCHAIN_C_FLAGS -mfpu=${FPU_FOR_${GCC_M_CPU}})
      list(APPEND TOOLCHAIN_LD_FLAGS -mfpu=${FPU_FOR_${GCC_M_CPU}})
      if    (CONFIG_FP_SOFTABI)
        list(APPEND TOOLCHAIN_C_FLAGS -mfloat-abi=softfp)
        list(APPEND TOOLCHAIN_LD_FLAGS -mfloat-abi=softfp)
      elseif(CONFIG_FP_HARDABI)
        list(APPEND TOOLCHAIN_C_FLAGS -mfloat-abi=hard)
        list(APPEND TOOLCHAIN_LD_FLAGS -mfloat-abi=hard)
      endif()
    endif()
  elseif("${ARCH}" STREQUAL "arc")
    list(APPEND TOOLCHAIN_C_FLAGS
      -mcpu=${GCC_M_CPU}
      )
  endif()

  if(NOT no_libgcc)
    execute_process(
      COMMAND ${CMAKE_C_COMPILER} ${TOOLCHAIN_C_FLAGS} --print-libgcc-file-name
      OUTPUT_VARIABLE LIBGCC_FILE_NAME
      OUTPUT_STRIP_TRAILING_WHITESPACE
      )

    assert_exists(LIBGCC_FILE_NAME)

    get_filename_component(LIBGCC_DIR ${LIBGCC_FILE_NAME} DIRECTORY)

    assert_exists(LIBGCC_DIR)

    LIST(APPEND LIB_INCLUDE_DIR "-L\"${LIBGCC_DIR}\"")
    LIST(APPEND TOOLCHAIN_LIBS gcc)
  endif()

  if(SYSROOT_DIR)
    # The toolchain has specified a sysroot dir that we can use to set
    # the libc path's
    execute_process(
      COMMAND ${CMAKE_C_COMPILER} ${TOOLCHAIN_C_FLAGS} --print-multi-directory
      OUTPUT_VARIABLE NEWLIB_DIR
      OUTPUT_STRIP_TRAILING_WHITESPACE
      )

    set(LIBC_LIBRARY_DIR "\"${SYSROOT_DIR}\"/lib/${NEWLIB_DIR}")
    set(LIBC_INCLUDE_DIR ${SYSROOT_DIR}/include)
  endif()

endif()

# For CMake to be able to test if a compiler flag is supported by the
# toolchain we need to give CMake the necessary flags to compile and
# link a dummy C file.
#
# CMake checks compiler flags with check_c_compiler_flag() (Which we
# wrap with target_cc_option() in extentions.cmake)
foreach(isystem_include_dir ${NOSTDINC})
  list(APPEND isystem_include_flags -isystem "\"${isystem_include_dir}\"")
endforeach()
set(CMAKE_REQUIRED_FLAGS -nostartfiles -nostdlib ${isystem_include_flags} -Wl,--unresolved-symbols=ignore-in-object-files)
string(REPLACE ";" " " CMAKE_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS}")
