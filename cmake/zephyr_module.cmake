# SPDX-License-Identifier: Apache-2.0

# This cmake file provides functionality to import additional out-of-tree, OoT
# CMakeLists.txt and Kconfig files into Zephyr build system.
# It uses -DZEPHYR_MODULES=<oot-path-to-module>[;<additional-oot-module(s)>]
# given to CMake for a list of folders to search.
# It looks for: <oot-module>/zephyr/module.yml or
#               <oot-module>/zephyr/CMakeLists.txt
# to load the oot-module into Zephyr build system.
# If west is available, it uses `west list` to obtain a list of projects to
# search for zephyr/module.yml

if(ZEPHYR_MODULES)
  set(ZEPHYR_MODULES_ARG "--modules" ${ZEPHYR_MODULES})
endif()

if(ZEPHYR_EXTRA_MODULES)
  set(ZEPHYR_EXTRA_MODULES_ARG "--extra-modules" ${ZEPHYR_EXTRA_MODULES})
endif()

set(KCONFIG_MODULES_FILE ${KCONFIG_BINARY_DIR}/Kconfig.modules)
set(ZEPHYR_SETTINGS_FILE ${CMAKE_BINARY_DIR}/zephyr_settings.txt)

if(WEST)
  set(WEST_ARG "--zephyr-base" ${ZEPHYR_BASE})
endif()

if(WEST OR ZEPHYR_MODULES)
  # Zephyr module uses west, so only call it if west is installed or
  # ZEPHYR_MODULES was provided as argument to CMake.
  execute_process(
    COMMAND
    ${PYTHON_EXECUTABLE} ${ZEPHYR_BASE}/scripts/zephyr_module.py
    ${WEST_ARG}
    ${ZEPHYR_MODULES_ARG}
    ${ZEPHYR_EXTRA_MODULES_ARG}
    --kconfig-out ${KCONFIG_MODULES_FILE}
    --cmake-out ${CMAKE_BINARY_DIR}/zephyr_modules.txt
    --settings-out ${ZEPHYR_SETTINGS_FILE}
    ERROR_VARIABLE
    zephyr_module_error_text
    RESULT_VARIABLE
    zephyr_module_return
  )

  if(${zephyr_module_return})
      message(FATAL_ERROR "${zephyr_module_error_text}")
  endif()

  if(EXISTS ${ZEPHYR_SETTINGS_FILE})
    file(STRINGS ${ZEPHYR_SETTINGS_FILE} ZEPHYR_SETTINGS_TXT ENCODING UTF-8)
    foreach(setting ${ZEPHYR_SETTINGS_TXT})
      # Match <key>:<value> for each line of file, each corresponding to
      # a setting.  The use of quotes is required due to CMake not supporting
      # lazy regexes (it supports greedy only).
      string(REGEX REPLACE "\"(.*)\":\".*\"" "\\1" key ${setting})
      string(REGEX REPLACE "\".*\":\"(.*)\"" "\\1" value ${setting})
      list(APPEND ${key} ${value})
    endforeach()
  endif()

else()

  file(WRITE ${KCONFIG_MODULES_FILE}
    "# No west and no modules\n"
    )

endif()
