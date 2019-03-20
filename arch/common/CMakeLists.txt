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


add_linker_snippet_ifdef(CONFIG_ARCH_HAS_RAMFUNC_SUPPORT
  RAM_SECTIONS
  "SECTION_DATA_PROLOGUE(.ramfunc,,)
  {
          MPU_ALIGN(_ramfunc_ram_size);
          _ramfunc_ram_start = .;
          *(.ramfunc)
          *(\".ramfunc.*\")
          MPU_ALIGN(_ramfunc_ram_size);
          _ramfunc_ram_end = .;
  } GROUP_DATA_LINK_IN(RAMABLE_REGION, ROMABLE_REGION)
  _ramfunc_ram_size = _ramfunc_ram_end - _ramfunc_ram_start;
  _ramfunc_rom_start = LOADADDR(.ramfunc);"
)

add_linker_snippet_ifdef(CONFIG_NOCACHE_MEMORY
  RAM_SECTIONS
  "/* Non-cached region of RAM */
  SECTION_PROLOGUE(_NOCACHE_SECTION_NAME,(NOLOAD),)
  {
          MPU_ALIGN(_nocache_ram_size);
          _nocache_ram_start = .;
          *(.nocache)
          *(\".nocache.*\")
          MPU_ALIGN(_nocache_ram_size);
          _nocache_ram_end = .;
  } GROUP_LINK_IN(RAMABLE_REGION)
  _nocache_ram_size = _nocache_ram_end - _nocache_ram_start;"
)
