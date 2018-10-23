set(BOARD_FLASH_RUNNER intel_s1000)
set(BOARD_DEBUG_RUNNER intel_s1000)

board_finalize_runner_args(intel_s1000
  "--xt-ocd-dir=/opt/tensilica/xocd-12.0.4/xt-ocd"
  "--ocd-topology=topology_dsp0_flyswatter2.xml"
  "--ocd-jtag-instr=dsp0_gdb.txt"
  "--gdb-flash-file=load_elf.txt"
)

if(NOT "${ZEPHYR_TOOLCHAIN_VARIANT}" STREQUAL "xcc")
  message(FATAL_ERROR "ZEPHYR_TOOLCHAIN_VARIANT != xcc. This requires xcc to build!")
endif()
