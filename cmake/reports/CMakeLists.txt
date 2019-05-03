# SPDX-License-Identifier: Apache-2.0

set(flag_for_ram_report -r)
set(flag_for_rom_report -F)

foreach(report ram_report rom_report)
  add_custom_target(
    ${IMAGE}${report}
    ${PYTHON_EXECUTABLE}
    ${ZEPHYR_BASE}/scripts/footprint/size_report
    ${flag_for_${report}}
    --objdump ${CMAKE_OBJDUMP}
    --objcopy ${CMAKE_OBJCOPY}
    --nm      ${CMAKE_NM}
    -o ${PROJECT_BINARY_DIR}
    DEPENDS ${logical_target_for_zephyr_elf}
    )
endforeach()

add_custom_command(
  TARGET ${IMAGE}rom_report
  POST_BUILD
  COMMAND
  ${PYTHON_EXECUTABLE}
  ${ZEPHYR_BASE}/../nrf/scripts/partition_manager_report.py
  --input ${CMAKE_BINARY_DIR}/partitions.yml
  "$<$<NOT:$<TARGET_EXISTS:partition_manager>>:--quiet>"
  )
