# SPDX-License-Identifier: Apache-2.0

add_library(subsys__bluetooth INTERFACE)

target_include_directories(subsys__bluetooth INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

if(NOT CONFIG_BT_HOST_NORDIC)
  add_subdirectory(common)
  add_subdirectory_ifdef(CONFIG_BT_HCI host)
endif()
add_subdirectory_ifdef(CONFIG_BT_CONN services)
add_subdirectory_ifdef(CONFIG_BT_MESH mesh)
add_subdirectory_ifdef(CONFIG_BT_AUDIO audio)
if(NOT CONFIG_BT_HOST_NORDIC)
  add_subdirectory_ifdef(CONFIG_BT_CRYPTO crypto)
endif()

if(CONFIG_BT_LL_SW_SPLIT)
  add_subdirectory(controller)
endif()

zephyr_include_directories(${ZEPHYR_BASE}/subsys/bluetooth)

if(NOT CONFIG_BT_HOST_NORDIC)
  add_subdirectory(lib)
endif()
