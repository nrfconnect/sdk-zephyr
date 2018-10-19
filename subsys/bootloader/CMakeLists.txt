# Do not add using zephyr cmake extensions.
# This because our library should not be added to the 'ZEPHYR_LIBS' list,
# since it should not be linked to the zephyr elf itself.

# TODO: Add conditionals for debugging and SystemInit

add_library(bootloader 
	bootloader.c 
	startup.c

	)

set(EXTS ${ZEPHYR_BASE}/ext)
set(HAL ${EXTS}/hal)
set(SEGGER ${EXTS}/debug/segger)
set(STD_LIB_C ${ZEPHYR_BASE}/lib/libc/minimal)
set(STD_SOURCE ${STD_LIB_C}/source)
set(STD_INCLUDE ${STD_LIB_C}/include)

add_dependencies(bootloader offsets_h)

target_link_libraries(bootloader zephyr_interface)

# Nordic specific for startup code
target_sources_ifdef(CONFIG_SECURE_BOOT_SYSTEM_INIT
	bootloader
	PRIVATE
	${HAL}/nordic/nrfx/mdk/system_nrf52840.c
	)

# SEGGER files for DEBUG
target_include_directories_ifdef(CONFIG_SECURE_BOOT_DEBUG
	bootloader
	PRIVATE
	${SEGGER}/rtt
	${STD_INCLUDE}
	)

target_sources_ifdef(CONFIG_SECURE_BOOT_DEBUG
	bootloader
	PRIVATE
	${SEGGER}/rtt/SEGGER_RTT.c
	${SEGGER}/rtt/SEGGER_RTT_printf.c
	${SEGGER}/rtt/SEGGER_RTT_Conf_sb.h
	${SEGGER}/rtt/SEGGER_RTT_sb.h
	)

# CONFIG_USE_C_LIB
target_sources_ifdef(CONFIG_SECURE_BOOT_DEBUG
	bootloader
	PRIVATE
	${STD_SOURCE}/stdlib/atoi.c
	${STD_SOURCE}/stdlib/strtol.c
	${STD_SOURCE}/stdlib/strtoul.c
	${STD_SOURCE}/stdlib/malloc.c
	${STD_SOURCE}/string/strncasecmp.c
	${STD_SOURCE}/string/strstr.c
	${STD_SOURCE}/string/string.c
	${STD_SOURCE}/stdout/prf.c
	${STD_SOURCE}/stdout/stdout_console.c
	${STD_SOURCE}/stdout/sprintf.c
	${STD_SOURCE}/stdout/fprintf.c
	)
