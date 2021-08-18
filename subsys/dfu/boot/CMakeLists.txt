# SPDX-License-Identifier: Apache-2.0

if(CONFIG_MCUBOOT_IMG_MANAGER)
zephyr_library()
zephyr_library_sources(mcuboot.c)
zephyr_library_sources_ifdef(CONFIG_MCUBOOT_SHELL mcuboot_shell.c)
if(CONFIG_BOOT_IMAGE_ACCESS_HOOKS)
  if (EXISTS ${ZEPHYR_MCUBOOT_MODULE_DIR}/boot/zephyr/${CONFIG_BOOT_IMAGE_ACCESS_HOOKS_FILE})
    zephyr_library_sources(
      ${ZEPHYR_MCUBOOT_MODULE_DIR}/boot/zephyr/${CONFIG_BOOT_IMAGE_ACCESS_HOOKS_FILE}
    )
  else()
    message(FATAL_ERROR "Can't access hooks implementation file.")
  endif()
endif()
zephyr_library_link_libraries(MCUBOOT_BOOTUTIL)
endif()
