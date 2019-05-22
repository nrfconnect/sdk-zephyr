# SPDX-License-Identifier: Apache-2.0

# Determine if we have an IAMCU toolchain or not.
if(CONFIG_X86_IAMCU)
  set(CROSS_COMPILE_TARGET_x86  i586-zephyr-elfiamcu)
else()
  set(CROSS_COMPILE_TARGET_x86 i586-zephyr-elf)
endif()

set(CROSS_COMPILE_TARGET_arm         arm-zephyr-eabi)
set(CROSS_COMPILE_TARGET_nios2     nios2-zephyr-elf)
set(CROSS_COMPILE_TARGET_riscv32 riscv32-zephyr-elf)
set(CROSS_COMPILE_TARGET_mips     mipsel-zephyr-elf)
set(CROSS_COMPILE_TARGET_xtensa   xtensa-zephyr-elf)
set(CROSS_COMPILE_TARGET_arc         arc-zephyr-elf)

set(CROSS_COMPILE_TARGET ${CROSS_COMPILE_TARGET_${ARCH}})
set(SYSROOT_TARGET       ${CROSS_COMPILE_TARGET})

set(CROSS_COMPILE ${TOOLCHAIN_HOME}/${CROSS_COMPILE_TARGET}/bin/${CROSS_COMPILE_TARGET}-)

if("${ARCH}" STREQUAL "xtensa")
  set(SYSROOT_DIR ${TOOLCHAIN_HOME}/${SYSROOT_TARGET})
  set(TOOLCHAIN_INCLUDES
    ${SYSROOT_DIR}/include/arch/include
    ${SYSROOT_DIR}/include
    )

  LIST(APPEND TOOLCHAIN_LIBS hal)
  LIST(APPEND LIB_INCLUDE_DIR -L${SYSROOT_DIR}/lib)
endif()
set(SYSROOT_DIR   ${TOOLCHAIN_HOME}/${SYSROOT_TARGET}/${SYSROOT_TARGET})
