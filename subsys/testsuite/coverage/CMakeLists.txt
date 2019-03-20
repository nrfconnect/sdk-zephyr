zephyr_sources_ifdef(CONFIG_COVERAGE_GCOV coverage.c)

add_linker_snippet_ifdef(CONFIG_COVERAGE_GCOV RAM_SECTIONS
  "SECTION_DATA_PROLOGUE(_GCOV_BSS_SECTION_NAME,(NOLOAD),)
  {

  #ifdef CONFIG_USERSPACE
      MPU_ALIGN(__gcov_bss_end - __gcov_bss_start );
  #else  /* CONFIG_MPU_REQUIRES_POWER_OF_TWO_ALIGNMENT && CONFIG_USERSPACE */
      . = ALIGN(_region_min_align);
  #endif /* CONFIG_USERSPACE */

      __gcov_bss_start = .;
      KEEP(*(\".bss.__gcov0.*\"));

  #ifdef CONFIG_USERSPACE
      MPU_ALIGN(__gcov_bss_end - __gcov_bss_start );
  #else  /* CONFIG_USERSPACE */
      . = ALIGN(_region_min_align);
  #endif /* CONFIG_USERSPACE */

      __gcov_bss_end = .;

  } GROUP_LINK_IN(RAMABLE_REGION)

  __gcov_bss_num_words = ((__gcov_bss_end - __gcov_bss_start) >> 2);
  __gcov_bss_size = __gcov_bss_end - __gcov_bss_start;"
)

add_linker_snippet_ifdef(CONFIG_COVERAGE_GCOV SECTIONS
#ifdef CONFIG_COVERAGE_GCOV
	/* Section needed by gcov when coverage is turned on.*/
	SECTION_PROLOGUE (gcov, (OPTIONAL),)
	{
		PROVIDE_HIDDEN (__init_array_start = .);
		KEEP (*(SORT(.init_array.*)))
		KEEP (*(.init_array*))
		PROVIDE_HIDDEN (__init_array_end = .);
	} GROUP_LINK_IN(ROMABLE_REGION)
#endif  /* CONFIG_COVERAGE_GCOV */
