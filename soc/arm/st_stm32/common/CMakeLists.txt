zephyr_sources_ifdef(CONFIG_ARM_MPU arm_mpu_regions.c)
zephyr_sources(stm32cube_hal.c)

if (DEFINED DT_CCM_BASE_ADDRESS)
  add_named_section(".ccm_bss" REGION CCM GROUP_START CCM)
  add_named_section(".ccm_noinit" REGION CCM)
  add_named_section(".ccm_data" REGION "CCM\ AT\>\ ROMABLE_REGION" GROUP_END CCM)
endif()
