zephyr_sources_ifdef(CONFIG_ARM_MPU arm_mpu_regions.c)
zephyr_sources(stm32cube_hal.c)

add_linker_snippet_ifdef(DT_CCM_BASE_ADDRESS SECTIONS
  "GROUP_START(CCM)
	SECTION_PROLOGUE(_CCM_BSS_SECTION_NAME, (NOLOAD OPTIONAL),SUBALIGN(4))
	{
		__ccm_start = .;
		__ccm_bss_start = .;
		*(.ccm_bss)
		*(\".ccm_bss.*\")
	} GROUP_LINK_IN(CCM)

	__ccm_bss_end = .;

	SECTION_PROLOGUE(_CCM_NOINIT_SECTION_NAME, (NOLOAD OPTIONAL),SUBALIGN(4))
	{
		__ccm_noinit_start = .;
		*(.ccm_noinit)
		*(\".ccm_noinit.*\")
	} GROUP_LINK_IN(CCM)

	__ccm_noinit_end = .;

	SECTION_PROLOGUE(_CCM_DATA_SECTION_NAME, (OPTIONAL),SUBALIGN(4))
	{
		__ccm_data_start = .;
		*(.ccm_data)
		*(\".ccm_data.*\")
	} GROUP_LINK_IN(CCM AT> ROMABLE_REGION)

	__ccm_data_end = .;
	__ccm_end = .;

	__ccm_data_rom_start = LOADADDR(_CCM_DATA_SECTION_NAME);

GROUP_END(CCM)"
)
