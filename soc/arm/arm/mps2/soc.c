/*
 * Copyright (c) 2017 Linaro Limited
 *
 * Initial contents based on soc/arm/ti_lm3s6965/soc.c which is:
 * Copyright (c) 2013-2015 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <arch/cpu.h>
#include <drivers/gpio/gpio_mmio32.h>
#include <init.h>
#include <soc.h>

/* Setup GPIO drivers for accessing FPGAIO registers */
GPIO_MMIO32_INIT(fpgaio_led0, DT_FPGAIO_LED0_GPIO_NAME, DT_FPGAIO_LED0,
				BIT_MASK(DT_FPGAIO_LED0_NUM));
GPIO_MMIO32_INIT(fpgaio_button, DT_FPGAIO_BUTTON_GPIO_NAME, DT_FPGAIO_BUTTON,
				BIT_MASK(DT_FPGAIO_BUTTON_NUM));
GPIO_MMIO32_INIT(fpgaio_misc, DT_FPGAIO_MISC_GPIO_NAME, DT_FPGAIO_MISC,
				BIT_MASK(DT_FPGAIO_MISC_NUM));

/* (Secure System Control) Base Address */
#define SSE_200_SYSTEM_CTRL_S_BASE	(0x50021000UL)
#define SSE_200_SYSTEM_CTRL_INITSVTOR1	(SSE_200_SYSTEM_CTRL_S_BASE + 0x114)
#define SSE_200_SYSTEM_CTRL_CPU_WAIT	(SSE_200_SYSTEM_CTRL_S_BASE + 0x118)
#define SSE_200_CPU_ID_UNIT_BASE	(0x5001F000UL)

/* The base address that the application image will start at on the secondary
 * (non-TrustZone) Cortex-M33 mcu.
 */
#define CPU1_FLASH_ADDRESS      (0x100000)

/* The memory map offset for the application image, which is used
 * to determine the location of the reset vector at startup.
 */
#define CPU1_FLASH_OFFSET       (0x10000000)

/* Space reserved for TF-M's secure bootloader on the secondary mcu.
 * This space is reserved whether BL2 is used or not.
 */
#define BL2_HEADER_SIZE         (0x400)

/**
 * @brief Wake up CPU 1 from another CPU, this is plaform specific.
 */
void wakeup_cpu1(void)
{
	/* Set the Initial Secure Reset Vector Register for CPU 1 */
	*(u32_t *)(SSE_200_SYSTEM_CTRL_INITSVTOR1) =
		CONFIG_FLASH_BASE_ADDRESS +
		BL2_HEADER_SIZE +
		CPU1_FLASH_ADDRESS -
		CPU1_FLASH_OFFSET;

	/* Set the CPU Boot wait control after reset */
	*(u32_t *)(SSE_200_SYSTEM_CTRL_CPU_WAIT) = 0;
}

/**
 * @brief Get the current CPU ID, this is plaform specific.
 *
 * @return Current CPU ID
 */
u32_t sse_200_platform_get_cpu_id(void)
{
	volatile u32_t *p_cpu_id = (volatile u32_t *)SSE_200_CPU_ID_UNIT_BASE;

	return (u32_t)*p_cpu_id;
}

/**
 * @brief Perform basic hardware initialization at boot.
 *
 * @return 0
 */
static int arm_mps2_init(struct device *arg)
{
	ARG_UNUSED(arg);

	/*
	 * Install default handler that simply resets the CPU
	 * if configured in the kernel, NOP otherwise
	 */
	NMI_INIT();

	return 0;
}

SYS_INIT(arm_mps2_init, PRE_KERNEL_1, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT);
