/*
 * Copyright (c) 2013-2014, Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * DESCRIPTION
 * Platform independent, commonly used macros and defines related to linker
 * script.
 *
 * This file may be included by:
 * - Linker script files: for linker section declarations
 * - C files: for external declaration of address or size of linker section
 * - Assembly files: for external declaration of address or size of linker
 *   section
 */

#ifndef ZEPHYR_INCLUDE_LINKER_LINKER_DEFS_H_
#define ZEPHYR_INCLUDE_LINKER_LINKER_DEFS_H_

#include <toolchain.h>
#include <linker/sections.h>
#include <misc/util.h>
#include <offsets.h>

/* include platform dependent linker-defs */
#ifdef CONFIG_X86
/* Nothing yet to include */
#elif defined(CONFIG_ARM)
/* Nothing yet to include */
#elif defined(CONFIG_ARC)
/* Nothing yet to include */
#elif defined(CONFIG_NIOS2)
/* Nothing yet to include */
#elif defined(CONFIG_RISCV32)
/* Nothing yet to include */
#elif defined(CONFIG_XTENSA)
/* Nothing yet to include */
#elif defined(CONFIG_ARCH_POSIX)
/* Nothing yet to include */
#else
#error Arch not supported.
#endif

#ifdef _LINKER


/*
 * Space for storing per device busy bitmap. Since we do not know beforehand
 * the number of devices, we go through the below mechanism to allocate the
 * required space.
 */
#ifdef CONFIG_DEVICE_POWER_MANAGEMENT
#define DEVICE_COUNT \
	((__device_init_end - __device_init_start) / _DEVICE_STRUCT_SIZEOF)
#define DEV_BUSY_SZ	(((DEVICE_COUNT + 31) / 32) * 4)
#define DEVICE_BUSY_BITFIELD()			\
		FILL(0x00) ;			\
		__device_busy_start = .;	\
		. = . + DEV_BUSY_SZ;		\
		__device_busy_end = .;
#else
#define DEVICE_BUSY_BITFIELD()
#endif

/*
 * generate a symbol to mark the start of the device initialization objects for
 * the specified level, then link all of those objects (sorted by priority);
 * ensure the objects aren't discarded if there is no direct reference to them
 */

#define DEVICE_INIT_LEVEL(level)				\
		__device_##level##_start = .;			\
		KEEP(*(SORT(.init_##level[0-9])));		\
		KEEP(*(SORT(.init_##level[1-9][0-9])));	\

/*
 * link in device initialization objects for all devices that are automatically
 * initialized by the kernel; the objects are sorted in the order they will be
 * initialized (i.e. ordered by level, sorted by priority within a level)
 */

#define	DEVICE_INIT_SECTIONS()			\
		__device_init_start = .;	\
		DEVICE_INIT_LEVEL(PRE_KERNEL_1)	\
		DEVICE_INIT_LEVEL(PRE_KERNEL_2)	\
		DEVICE_INIT_LEVEL(POST_KERNEL)	\
		DEVICE_INIT_LEVEL(APPLICATION)	\
		__device_init_end = .;		\
		DEVICE_BUSY_BITFIELD()		\


/* define a section for undefined device initialization levels */
#define DEVICE_INIT_UNDEFINED_SECTION()		\
		KEEP(*(SORT(.init_[_A-Z0-9]*)))	\

/*
 * link in shell initialization objects for all modules that use shell and
 * their shell commands are automatically initialized by the kernel.
 */

#define	SHELL_INIT_SECTIONS()				\
		__shell_module_start = .;		\
		KEEP(*(".shell_module_*"));		\
		__shell_module_end = .;			\
		__shell_cmd_start = .;			\
		KEEP(*(".shell_cmd_*"));		\
		__shell_cmd_end = .;			\

/*
 * link in shell initialization objects for all modules that use shell and
 * their shell commands are automatically initialized by the kernel.
 */

#ifdef CONFIG_APPLICATION_MEMORY
/*
 * KERNELSPACE_OBJECT_FILES is a space-separated list of object files
 * and libraries that belong in kernelspace.
 */
#define MAYBE_EXCLUDE_SOME_FILES EXCLUDE_FILE (KERNELSPACE_OBJECT_FILES)
#else
#define MAYBE_EXCLUDE_SOME_FILES
#endif /* CONFIG_APPLICATION_MEMORY */

/*
 * APP_INPUT_SECTION should be invoked on sections that should be in
 * 'app' space. KERNEL_INPUT_SECTION should be invoked on sections
 * that should be in 'kernel' space.
 *
 * NB: APP_INPUT_SECTION must be invoked before
 * KERNEL_INPUT_SECTION. If it is not all sections will end up in
 * kernelspace.
 */
#define APP_INPUT_SECTION(sect)    *(MAYBE_EXCLUDE_SOME_FILES sect)
#define KERNEL_INPUT_SECTION(sect) *(sect)

#define APP_SMEM_SECTION() KEEP(*(SORT(data_smem_[_a-zA-Z0-9]*)))

#ifdef CONFIG_X86 /* LINKER FILES: defines used by linker script */
/* Should be moved to linker-common-defs.h */
#if defined(CONFIG_XIP)
#define ROMABLE_REGION ROM
#else
#define ROMABLE_REGION RAM
#endif
#endif

/*
 * If image is loaded via kexec Linux system call, then program
 * headers need to be page aligned.
 * This can be done by section page aligning.
 */
#ifdef CONFIG_BOOTLOADER_KEXEC
#define KEXEC_PGALIGN_PAD(x) . = ALIGN(x);
#else
#define KEXEC_PGALIGN_PAD(x)
#endif

#elif defined(_ASMLANGUAGE)

/* Assembly FILES: declaration defined by the linker script */
GDATA(__bss_start)
GDATA(__bss_num_words)
#ifdef CONFIG_XIP
GDATA(__data_rom_start)
GDATA(__data_ram_start)
GDATA(__data_num_words)
#endif

#else /* ! _ASMLANGUAGE */

#include <zephyr/types.h>
/*
 * The following are externs symbols from the linker. This enables
 * the dynamic k_mem_domain and k_mem_partition creation and alignment
 * to the section produced in the linker.
 */
extern char _app_smem_start[];
extern char _app_smem_end[];
extern char _app_smem_size[];
extern char _app_smem_rom_start[];

#ifdef CONFIG_APPLICATION_MEMORY
/* Memory owned by the application. Start and end will be aligned for memory
 * management/protection hardware for the target architecture.

 * The policy for this memory will be to configure all of it as user thread
 * accessible. It consists of all non-kernel globals.
 */
extern char __app_ram_start[];
extern char __app_ram_end[];
extern char __app_ram_size[];
#endif

/* Memory owned by the kernel. Start and end will be aligned for memory
 * management/protection hardware for the target architecture..
 *
 * Consists of all kernel-side globals, all kernel objects, all thread stacks,
 * and all currently unused RAM.  If CONFIG_APPLICATION_MEMORY is not enabled,
 * has all globals, not just kernel side.
 *
 * Except for the stack of the currently executing thread, none of this memory
 * is normally accessible to user threads unless specifically granted at
 * runtime.
 */
extern char __kernel_ram_start[];
extern char __kernel_ram_end[];
extern char __kernel_ram_size[];

/* Used by _bss_zero or arch-specific implementation */
extern char __bss_start[];
extern char __bss_end[];
#ifdef CONFIG_APPLICATION_MEMORY
extern char __app_bss_start[];
extern char __app_bss_end[];
#endif

/* Used by _data_copy() or arch-specific implementation */
#ifdef CONFIG_XIP
extern char __data_rom_start[];
extern char __data_ram_start[];
extern char __data_ram_end[];
#ifdef CONFIG_APPLICATION_MEMORY
extern char __app_data_rom_start[];
extern char __app_data_ram_start[];
extern char __app_data_ram_end[];
#endif /* CONFIG_APPLICATION_MEMORY */
#endif /* CONFIG_XIP */

/* Includes text and rodata */
extern char _image_rom_start[];
extern char _image_rom_end[];
extern char _image_rom_size[];

/* Includes all ROMable data, i.e. the size of the output image file. */
extern char _flash_used[];

/* datas, bss, noinit */
extern char _image_ram_start[];
extern char _image_ram_end[];

extern char _image_text_start[];
extern char _image_text_end[];

extern char _image_rodata_start[];
extern char _image_rodata_end[];

extern char _vector_start[];
extern char _vector_end[];

/* end address of image, used by newlib for the heap */
extern char _end[];

#ifdef CONFIG_CCM_BASE_ADDRESS
extern char __ccm_data_rom_start[];
extern char __ccm_start[];
extern char __ccm_data_start[];
extern char __ccm_data_end[];
extern char __ccm_bss_start[];
extern char __ccm_bss_end[];
extern char __ccm_noinit_start[];
extern char __ccm_noinit_end[];
extern char __ccm_end[];
#endif /* CONFIG_CCM_BASE_ADDRESS */

/* Used by the Security Attribution Unit to configure the
 * Non-Secure Callable region.
 */
#ifdef CONFIG_ARM_FIRMWARE_HAS_SECURE_ENTRY_FUNCS
extern char __sg_start[];
extern char __sg_end[];
extern char __sg_size[];
#endif /* CONFIG_ARM_FIRMWARE_HAS_SECURE_ENTRY_FUNCS */

/*
 * Non-cached kernel memory region, currently only available on ARM Cortex-M7
 * with a MPU. Start and end will be aligned for memory management/protection
 * hardware for the target architecture.
 *
 * All the functions with '__nocache' keyword will be placed into this
 * section.
 */
#ifdef CONFIG_NOCACHE_MEMORY
extern char _nocache_ram_start[];
extern char _nocache_ram_end[];
extern char _nocache_ram_size[];
#endif /* CONFIG_NOCACHE_MEMORY */

#endif /* ! _ASMLANGUAGE */

#endif /* ZEPHYR_INCLUDE_LINKER_LINKER_DEFS_H_ */
