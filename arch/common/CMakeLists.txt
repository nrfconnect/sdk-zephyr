# Put functions and data in their own binary sections so that ld can
# garbage collect them
zephyr_cc_option(-ffunction-sections -fdata-sections)

zephyr_sources_ifdef(
  CONFIG_GEN_ISR_TABLES
  isr_tables.c
  sw_isr_common.c
  )

zephyr_sources_ifdef(
  CONFIG_EXECUTION_BENCHMARKING
  timing_info_bench.c
  )

if (DEFINED CONFIG_ARCH_HAS_RAMFUNC_SUPPORT)
  add_named_section(
    ".ramfunc"
    REGION
    "RAMABLE_REGION"
    LOAD_REGION
    "ROMABLE_REGION"
    MPU_ALIGN
    INPUT_SECTIONS
    "*(.ramfunc)"
    "*(\".ramfunc.*\")")
endif()
