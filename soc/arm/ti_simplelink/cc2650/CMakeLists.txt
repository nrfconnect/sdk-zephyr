zephyr_sources(soc.c)
add_named_section(".ti_ccfg" REGION FLASH_CCFG INPUT_SECTIONS "KEEP(*(TI_CCFG))")

