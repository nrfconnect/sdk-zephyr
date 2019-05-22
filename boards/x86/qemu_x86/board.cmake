# SPDX-License-Identifier: Apache-2.0

set(EMU_PLATFORM qemu)

if(NOT CONFIG_REBOOT)
  set(REBOOT_FLAG -no-reboot)
endif()

set(QEMU_CPU_TYPE_${ARCH} qemu32,+nx,+pae)
set(QEMU_FLAGS_${ARCH}
  -m 8
  -cpu ${QEMU_CPU_TYPE_${ARCH}}
  -device isa-debug-exit,iobase=0xf4,iosize=0x04
  ${REBOOT_FLAG}
  -nographic
  -no-acpi
  )

# TODO: Support debug
# set(BOARD_DEBUG_RUNNER qemu)
# debugserver: QEMU_EXTRA_FLAGS += -s -S
# debugserver: qemu
