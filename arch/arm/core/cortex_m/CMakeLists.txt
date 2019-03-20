zephyr_library()

zephyr_library_sources(
  vector_table.S
  reset.S
  nmi_on_reset.S
  prep_c.c
  scb.c
  nmi.c
  exc_manage.c
  )

add_linker_snippet_ifdef(CONFIG_SW_VECTOR_RELAY RAM_SECTIONS
  "/* Reserved 4 bytes to save vector table base address */
  SECTION_PROLOGUE(.vt_pointer,(NOLOAD),)
  {
          *(.vt_pointer_section)
          *(\".vt_pointer_section.*\")
  }"
)
