/*
 * Copyright (c) 2017 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Include esp-idf headers first to avoid redefining BIT() macro */
#include "soc.h"
#include <soc/rtc_cntl_reg.h>
#include <soc/timer_group_reg.h>
#include <zephyr/drivers/interrupt_controller/intc_esp32.h>
#include <xtensa/config/core-isa.h>
#include <xtensa/corebits.h>

#include <zephyr/kernel_structs.h>
#include <string.h>
#include <zephyr/toolchain/gcc.h>
#include <zephyr/types.h>
#include <zephyr/linker/linker-defs.h>
#include <kernel_internal.h>

#include "esp_private/system_internal.h"
#include "esp32/rom/cache.h"
#include "hal/soc_ll.h"
#include "soc/cpu.h"
#include "soc/gpio_periph.h"
#include "esp_spi_flash.h"
#include "esp_err.h"
#include "esp_timer.h"
#include "esp32/spiram.h"
#include "esp_app_format.h"
#include "hal/wdt_hal.h"

#ifndef CONFIG_SOC_ESP32_NET
#include "esp_clk_internal.h"
#endif /* CONFIG_SOC_ESP32_NET */

#ifdef CONFIG_MCUBOOT
#include "bootloader_init.h"
#endif /* CONFIG_MCUBOOT */
#include <zephyr/sys/printk.h>

extern void z_cstart(void);

#ifdef CONFIG_ESP32_NETWORK_CORE
extern const unsigned char esp32_net_fw_array[];
extern const int esp_32_net_fw_array_size;

void __attribute__((section(".iram1"))) start_esp32_net_cpu(void)
{
	esp_image_header_t *header = (esp_image_header_t *)&esp32_net_fw_array[0];
	esp_image_segment_header_t *segment =
		(esp_image_segment_header_t *)&esp32_net_fw_array[sizeof(esp_image_header_t)];
	uint8_t *segment_payload;
	uint32_t entry_addr = header->entry_addr;
	uint32_t idx = sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t);

	for (int i = 0; i < header->segment_count; i++) {
		segment_payload = (uint8_t *)&esp32_net_fw_array[idx];

		if (segment->load_addr >= SOC_IRAM_LOW && segment->load_addr < SOC_IRAM_HIGH) {
			/* IRAM segment only accepts 4 byte access, avoid memcpy usage here */
			volatile uint32_t *src = (volatile uint32_t *)segment_payload;
			volatile uint32_t *dst =
				(volatile uint32_t *)segment->load_addr;

			for (int i = 0; i < segment->data_len/4 ; i++) {
				dst[i] = src[i];
			}
		} else if (segment->load_addr >= SOC_DRAM_LOW &&
			segment->load_addr < SOC_DRAM_HIGH) {

			memcpy((void *)segment->load_addr,
				(const void *)segment_payload,
				segment->data_len);
		}

		idx += segment->data_len;
		segment = (esp_image_segment_header_t *)&esp32_net_fw_array[idx];
		idx += sizeof(esp_image_segment_header_t);
	}

	esp_appcpu_start((void *)entry_addr);
}
#endif /* CONFIG_ESP32_NETWORK_CORE */

/*
 * This is written in C rather than assembly since, during the port bring up,
 * Zephyr is being booted by the Espressif bootloader.  With it, the C stack
 * is already set up.
 */
void __attribute__((section(".iram1"))) __esp_platform_start(void)
{
	extern uint32_t _init_start;

	/* Move the exception vector table to IRAM. */
	__asm__ __volatile__ (
		"wsr %0, vecbase"
		:
		: "r"(&_init_start));

	z_bss_zero();

	__asm__ __volatile__ (
		""
		:
		: "g"(&__bss_start)
		: "memory");

	/* Disable normal interrupts. */
	__asm__ __volatile__ (
		"wsr %0, PS"
		:
		: "r"(PS_INTLEVEL(XCHAL_EXCM_LEVEL) | PS_UM | PS_WOE));

	/* Initialize the architecture CPU pointer.  Some of the
	 * initialization code wants a valid _current before
	 * arch_kernel_init() is invoked.
	 */
	__asm__ volatile("wsr.MISC0 %0; rsync" : : "r"(&_kernel.cpus[0]));

#ifdef CONFIG_MCUBOOT
	/* MCUboot early initialisation. */
	if (bootloader_init()) {
		abort();
	}
#else
	/* ESP-IDF/MCUboot 2nd stage bootloader enables RTC WDT to check
	 * on startup sequence related issues in application. Hence disable that
	 * as we are about to start Zephyr environment.
	 */
	wdt_hal_context_t rtc_wdt_ctx = {.inst = WDT_RWDT, .rwdt_dev = &RTCCNTL};

	wdt_hal_write_protect_disable(&rtc_wdt_ctx);
	wdt_hal_disable(&rtc_wdt_ctx);
	wdt_hal_write_protect_enable(&rtc_wdt_ctx);

#ifndef CONFIG_SOC_ESP32_NET
	/* Configures the CPU clock, RTC slow and fast clocks, and performs
	 * RTC slow clock calibration.
	 */
	esp_clk_init();
#endif

	esp_timer_early_init();

#if CONFIG_ESP32_NETWORK_CORE
	/* start the esp32 network core before
	 * start zephyr
	 */
	start_esp32_net_cpu();
#endif

#if CONFIG_ESP_SPIRAM
	esp_err_t err = esp_spiram_init();

	if (err != ESP_OK) {
		printk("Failed to Initialize SPIRAM, aborting.\n");
		abort();
	}
	esp_spiram_init_cache();
	if (esp_spiram_get_size() < CONFIG_ESP_SPIRAM_SIZE) {
		printk("SPIRAM size is less than configured size, aborting.\n");
		abort();
	}
#endif

/* Scheduler is not started at this point. Hence, guard functions
 * must be initialized after esp_spiram_init_cache which internally
 * uses guard functions. Setting guard functions before SPIRAM
 * cache initialization will result in a crash.
 */
#if CONFIG_SOC_FLASH_ESP32 || CONFIG_ESP_SPIRAM
	spi_flash_guard_set(&g_flash_guard_default_ops);
#endif

#endif /* CONFIG_MCUBOOT */

	esp_intr_initialize();

	/* Start Zephyr */
	z_cstart();

	CODE_UNREACHABLE;
}

/* Boot-time static default printk handler, possibly to be overridden later. */
int IRAM_ATTR arch_printk_char_out(int c)
{
	if (c == '\n') {
		esp_rom_uart_tx_one_char('\r');
	}
	esp_rom_uart_tx_one_char(c);
	return 0;
}

void sys_arch_reboot(int type)
{
	esp_restart_noos();
}

void IRAM_ATTR esp_restart_noos(void)
{
	/* Disable interrupts */
	z_xt_ints_off(0xFFFFFFFF);

	const uint32_t core_id = cpu_hal_get_core_id();
	const uint32_t other_core_id = (core_id == 0) ? 1 : 0;

	soc_ll_reset_core(other_core_id);
	soc_ll_stall_core(other_core_id);

	/* Flush any data left in UART FIFOs */
	esp_rom_uart_tx_wait_idle(0);
	esp_rom_uart_tx_wait_idle(1);
	esp_rom_uart_tx_wait_idle(2);

	/* Disable cache */
	Cache_Read_Disable(0);
	Cache_Read_Disable(1);

	/* 2nd stage bootloader reconfigures SPI flash signals. */
	/* Reset them to the defaults expected by ROM */
	WRITE_PERI_REG(GPIO_FUNC0_IN_SEL_CFG_REG, 0x30);
	WRITE_PERI_REG(GPIO_FUNC1_IN_SEL_CFG_REG, 0x30);
	WRITE_PERI_REG(GPIO_FUNC2_IN_SEL_CFG_REG, 0x30);
	WRITE_PERI_REG(GPIO_FUNC3_IN_SEL_CFG_REG, 0x30);
	WRITE_PERI_REG(GPIO_FUNC4_IN_SEL_CFG_REG, 0x30);
	WRITE_PERI_REG(GPIO_FUNC5_IN_SEL_CFG_REG, 0x30);

	/* Reset wifi/bluetooth/ethernet/sdio (bb/mac) */
	DPORT_SET_PERI_REG_MASK(DPORT_CORE_RST_EN_REG,
				DPORT_BB_RST | DPORT_FE_RST | DPORT_MAC_RST |
				DPORT_BT_RST | DPORT_BTMAC_RST |
				DPORT_SDIO_RST | DPORT_SDIO_HOST_RST |
				DPORT_EMAC_RST | DPORT_MACPWR_RST |
				DPORT_RW_BTMAC_RST | DPORT_RW_BTLP_RST);
	DPORT_REG_WRITE(DPORT_CORE_RST_EN_REG, 0);

	/* Reset timer/spi/uart */
	DPORT_SET_PERI_REG_MASK(
		DPORT_PERIP_RST_EN_REG,
		/* UART TX FIFO cannot be reset correctly on ESP32, */
		/* so reset the UART memory by DPORT here. */
		DPORT_TIMERS_RST | DPORT_SPI01_RST | DPORT_UART_RST |
		DPORT_UART1_RST | DPORT_UART2_RST | DPORT_UART_MEM_RST);
	DPORT_REG_WRITE(DPORT_PERIP_RST_EN_REG, 0);

	/* Clear entry point for APP CPU */
	DPORT_REG_WRITE(DPORT_APPCPU_CTRL_D_REG, 0);

	/* Reset CPUs */
	if (core_id == 0) {
		/* Running on PRO CPU: APP CPU is stalled. Can reset both CPUs. */
		soc_ll_reset_core(1);
		soc_ll_reset_core(0);
	} else {
		/* Running on APP CPU: need to reset PRO CPU and unstall it, */
		/* then reset APP CPU */
		soc_ll_reset_core(0);
		soc_ll_stall_core(0);
		soc_ll_reset_core(1);
	}

	while (true) {
		;
	}
}
