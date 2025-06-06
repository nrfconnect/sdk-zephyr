# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(kernel_common)

if(CONFIG_PRINTK)
target_sources(app PRIVATE
	src/printk.c
  )
endif()
target_sources(app PRIVATE
	src/atomic.c
	src/bitarray.c
	src/byteorder.c
	src/clock.c
	src/main.c
	src/timeout_order.c
	src/multilib.c
	src/errno.c
	src/boot_delay.c
	src/pow2.c
  )

target_sources_ifdef(
	CONFIG_STATIC_INIT_GNU
	app PRIVATE
	src/constructor.c
)

target_sources_ifdef(
	CONFIG_IRQ_OFFLOAD
	app PRIVATE
	src/irq_offload.c
)
if(CONFIG_SOC_NRF54H20_IRON)
  target_sources(app PRIVATE periphconf_migrated_src/periphconf_migrated_from_uicr.c)
endif()
