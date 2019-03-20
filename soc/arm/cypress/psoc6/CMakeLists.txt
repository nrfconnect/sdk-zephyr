#
# Copyright (c) 2018, Cypress
#
# SPDX-License-Identifier: Apache-2.0
#

zephyr_include_directories(.)
zephyr_sources(
	soc.c
)

add_linker_snippet_ifdef(SOC_SERIES_PSOC62 NOINIT
  "/*
   * Extracted from:
   * ext/hal/cypress/.../devices/psoc6/linker/gcc/cy8c6xx6_cm0plus.ld
   *
   * Size of sections are calculated in the startup scripts,
   * so they don't have to be specified here.
   */

  . = ALIGN(8);
  KEEP(*(.ram_vectors))

  . = ALIGN(4);
  KEEP(*(.heap))

  . = ALIGN(4);
  KEEP(*(.stack))"
)

add_linker_snippet_ifdef(SOC_SERIES_PSOC62 RWDATA
  "/*
   * Extracted from:
   * ext/hal/cypress/.../devices/psoc6/linker/gcc/cy8c6xx6_cm0plus.ld
   */

  KEEP(*(.cy_ramfunc))"
)
