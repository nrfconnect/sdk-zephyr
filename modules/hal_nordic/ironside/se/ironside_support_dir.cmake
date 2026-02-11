# The IronSide source directory can be overridden by setting
# IRONSIDE_SUPPORT_DIR before invoking the build system.
zephyr_get(IRONSIDE_SUPPORT_DIR SYSBUILD GLOBAL)
if(NOT DEFINED IRONSIDE_SUPPORT_DIR)
  set(IRONSIDE_SUPPORT_DIR
     ${ZEPHYR_BASE}/modules/hal_nordic/ironside/se/support_package/ironside
     CACHE PATH "IronSide Support Directory"
  )
endif()
