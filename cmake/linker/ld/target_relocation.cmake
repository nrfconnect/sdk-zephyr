# SPDX-License-Identifier: Apache-2.0

# See root CMakeLists.txt for description and expectations of these macros

macro(toolchain_ld_relocation)
  set(MEM_RELOCATAION_LD   "${PROJECT_BINARY_DIR}/include/generated/linker_relocate.ld")
  set(MEM_RELOCATAION_CODE "${PROJECT_BINARY_DIR}/code_relocation.c")

  add_custom_command(
    OUTPUT ${MEM_RELOCATAION_CODE} ${MEM_RELOCATAION_LD}
    COMMAND
    ${PYTHON_EXECUTABLE}
    ${ZEPHYR_BASE}/scripts/gen_relocate_app.py
    $<$<BOOL:${CMAKE_VERBOSE_MAKEFILE}>:--verbose>
    -d ${APPLICATION_BINARY_DIR}
    -i '$<TARGET_PROPERTY:${IMAGE}code_data_relocation_target,COMPILE_DEFINITIONS>'
    -o ${MEM_RELOCATAION_LD}
    -c ${MEM_RELOCATAION_CODE}
    DEPENDS ${KERNEL_LIBRARY} ${ZEPHYR_LIBS_PROPERTY}
    )

  add_library(${IMAGE}code_relocation_source_lib  STATIC ${MEM_RELOCATAION_CODE})
  target_link_libraries(${IMAGE}code_relocation_source_lib ${IMAGE}zephyr_interface)
endmacro()
