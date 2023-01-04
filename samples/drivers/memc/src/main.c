/*
 * Copyright 2022 NXP
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>

#if DT_HAS_COMPAT_STATUS_OKAY(nxp_imx_flexspi)
/* FlexSPI memory mapped region is second register property of parent dev */
#define MEMC_BASE DT_REG_ADDR_BY_IDX(DT_PARENT(DT_ALIAS(sram_ext)), 1)
#define MEMC_SIZE (DT_PROP(DT_ALIAS(sram_ext), size) / 8)
#endif

void dump_memory(uint8_t *p, uint32_t size)
{
	uint32_t i, j;

	for (i = 0, j = 0; j < (size / 16); i += 16, j++) {
		printk("%02x %02x %02x %02x %02x %02x %02x %02x ",
			p[i], p[i+1], p[i+2], p[i+3],
			p[i+4], p[i+5], p[i+6], p[i+7]);
		printk("%02x %02x %02x %02x %02x %02x %02x %02x\n",
			p[i+8], p[i+9], p[i+10], p[i+11],
			p[i+12], p[i+13], p[i+14], p[i+15]);
		/* Split dump at 256B boundaries */
		if (((i + 16) & 0xFF) == 0) {
			printk("\n");
		}
	}
	/* Dump any remaining data after 16 byte blocks */
	for (; i < size; i++) {
		printk("%02x ", p[i]);
	}
	printk("\n");
}

#define BUF_SIZE 1024

uint8_t memc_write_buffer[BUF_SIZE];
uint8_t memc_read_buffer[BUF_SIZE];

void main(void)
{
	uint8_t *memc = (uint8_t *)MEMC_BASE;
	uint32_t i, j;

	/* Initialize write buffer */
	for (uint32_t i = 0; i < BUF_SIZE; i++) {
		memc_write_buffer[i] = (uint8_t)i;
	}
	printk("Writing to memory region with base 0x%0x, size 0x%0x\n\n",
		MEMC_BASE, MEMC_SIZE);
	/* Copy write buffer into memc region */
	for (i = 0, j = 0; j < (MEMC_SIZE / BUF_SIZE); i += BUF_SIZE, j++) {
		memcpy(memc + i, memc_write_buffer, BUF_SIZE);
	}
	/* Copy any remaining space bytewise */
	for (; i < MEMC_SIZE; i++) {
		memc[i] = memc_write_buffer[i];
	}
	/* Read from memc region into buffer */
	for (i = 0, j = 0; j < (MEMC_SIZE / BUF_SIZE); i += BUF_SIZE, j++) {
		memcpy(memc_read_buffer, memc + i, BUF_SIZE);
		/* Compare memory */
		if (memcmp(memc_read_buffer, memc_write_buffer, BUF_SIZE)) {
			printk("Error: read data differs in range [0x%x- 0x%x]\n",
				i, i + (BUF_SIZE - 1));
			return;
		}
	}
	/* Copy any remaining space bytewise */
	for (; i < MEMC_SIZE; i++) {
		memc_read_buffer[i] = memc[i];
		if (memc_write_buffer[i] != memc_read_buffer[i]) {
			printk("Error: read data differs at offset 0x%x\n", i);
			return;
		}
	}
	printk("First 1KB of Data in memory:\n");
	printk("===========================\n");
	dump_memory(memc, MIN(MEMC_SIZE, KB(1)));
	printk("Read data matches written data\n");
}
