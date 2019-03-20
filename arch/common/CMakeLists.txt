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


# This creates a special section which is not included by the final binary,
# instead it is consumed by the gen_isr_tables.py script.
#
# What we create here is a data structure:
#
# struct {
#	u32_t num_vectors;       <- typically CONFIG_NUM_IRQS
#	struct _isr_list isrs[]; <- Usually of smaller size than num_vectors
# }
#
# Which indicates the memory address of the number of isrs that were
# defined, the total number of IRQ lines in the system, followed by
# an appropriate number of instances of struct _isr_list. See
# include/sw_isr_table.h
#
# You will need to declare a bogus memory region for IDT_LIST. It doesn't
# matter where this region goes as it is stripped from the final ELF image.
# The address doesn't even have to be valid on the target. However, it
# shouldn't overlap any other regions. On most arches the following should be
# fine:
#
# MEMORY {
#	.. other regions ..
#	IDT_LIST        : ORIGIN = 0xfffff7ff, LENGTH = 2K
# }
#
add_linker_snippet_ifdef(CONFIG_GEN_ISR_TABLES
  SECTIONS
  "#ifndef LINKER_PASS2
  SECTION_PROLOGUE(.intList,,)
  {
  	KEEP(*(.irq_info))
  	KEEP(*(.intList))
  } GROUP_LINK_IN(IDT_LIST)
  #else
  /DISCARD/ :
  {
  	KEEP(*(.irq_info))
  	KEEP(*(.intList))
  }
  #endif"
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
