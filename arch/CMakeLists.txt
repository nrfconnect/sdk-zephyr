add_definitions(-D__ZEPHYR_SUPERVISOR__)

add_subdirectory(common)
add_subdirectory_ifdef(CONFIG_KERNEL ${ARCH_DIR}/${ARCH} arch/${ARCH})
