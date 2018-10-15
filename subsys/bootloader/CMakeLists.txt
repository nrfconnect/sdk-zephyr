add_library(bootloader boot.c)

add_dependencies(bootloader offsets_h)

target_link_libraries(bootloader zephyr_interface)
