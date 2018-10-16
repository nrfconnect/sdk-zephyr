# Do not add using zephyr cmake extensions.
# This because our library should not be added to the 'ZEPHYR_LIBS' list,
# since it should not be linked to the zephyr elf itself.
add_library(bootloader 
	bootloader.c 
	startup.c
	${ZEPHYR_BASE}/ext/hal/nordic/nrfx/mdk/system_nrf52840.c #Nordic specific for startup code
)

add_dependencies(bootloader offsets_h)

target_link_libraries(bootloader zephyr_interface)
