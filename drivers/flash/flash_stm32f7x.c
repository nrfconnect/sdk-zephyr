/*
 * Copyright (c) 2018 Aurelien Jarno
 * Copyright (c) 2018 Yong Jin
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <kernel.h>
#include <device.h>
#include <string.h>
#include <flash.h>
#include <init.h>
#include <soc.h>

#include "flash_stm32.h"

#define STM32F7X_SECTOR_MASK		((u32_t) 0xFFFFFF07)

bool flash_stm32_valid_range(struct device *dev, off_t offset, u32_t len,
			     bool write)
{
	ARG_UNUSED(write);

	return flash_stm32_range_exists(dev, offset, len);
}

static int write_byte(struct device *dev, off_t offset, u8_t val)
{
	struct stm32f7x_flash *regs = FLASH_STM32_REGS(dev);
	int rc;

	/* if the control register is locked, do not fail silently */
	if (regs->cr & FLASH_CR_LOCK) {
		return -EIO;
	}

	rc = flash_stm32_wait_flash_idle(dev);
	if (rc < 0) {
		return rc;
	}

	/* prepare to write a single byte */
	regs->cr = (regs->cr & CR_PSIZE_MASK) |
		   FLASH_PSIZE_BYTE | FLASH_CR_PG;
	/* flush the register write */
	__DSB();

	/* write the data */
	*((u8_t *) offset + CONFIG_FLASH_BASE_ADDRESS) = val;
	/* flush the register write */
	__DSB();

	rc = flash_stm32_wait_flash_idle(dev);
	regs->cr &= (~FLASH_CR_PG);

	return rc;
}

static int erase_sector(struct device *dev, u32_t sector)
{
	struct stm32f7x_flash *regs = FLASH_STM32_REGS(dev);
	int rc;

	/* if the control register is locked, do not fail silently */
	if (regs->cr & FLASH_CR_LOCK) {
		return -EIO;
	}

	rc = flash_stm32_wait_flash_idle(dev);
	if (rc < 0) {
		return rc;
	}

	/* Dual bank mode, SNB MSB selects the bank2,
	 * others select sector, so we remap sector number.
	 */
#if defined(FLASH_OPTCR_nDBANK) && FLASH_SECTOR_TOTAL == 24
#if CONFIG_FLASH_SIZE == 2048
	if (sector > 11) {
		sector += 4U;
	}
#elif CONFIG_FLASH_SIZE == 1024
	if (sector > 7) {
		sector += 8U;
	}
#endif /* CONFIG_FLASH_SIZE */
#endif /* defined(FLASH_OPTCR_nDBANK) && FLASH_SECTOR_TOTAL == 24 */

	regs->cr = (regs->cr & (CR_PSIZE_MASK | STM32F7X_SECTOR_MASK)) |
		   FLASH_PSIZE_BYTE |
		   FLASH_CR_SER |
		   (sector << FLASH_CR_SNB_Pos) |
		   FLASH_CR_STRT;
	/* flush the register write */
	__DSB();

	rc = flash_stm32_wait_flash_idle(dev);
	regs->cr &= ~(FLASH_CR_SER | FLASH_CR_SNB);

	return rc;
}

int flash_stm32_block_erase_loop(struct device *dev, unsigned int offset,
				 unsigned int len)
{
	struct flash_pages_info info;
	u32_t start_sector, end_sector;
	u32_t i;
	int rc = 0;

	rc = flash_get_page_info_by_offs(dev, offset, &info);
	if (rc) {
		return rc;
	}
	start_sector = info.index;
	rc = flash_get_page_info_by_offs(dev, offset + len - 1, &info);
	if (rc) {
		return rc;
	}
	end_sector = info.index;

	for (i = start_sector; i <= end_sector; i++) {
		rc = erase_sector(dev, i);
		if (rc < 0) {
			break;
		}
	}

	return rc;
}

int flash_stm32_write_range(struct device *dev, unsigned int offset,
			    const void *data, unsigned int len)
{
	int i, rc = 0;

	for (i = 0; i < len; i++, offset++) {
		rc = write_byte(dev, offset, ((const u8_t *) data)[i]);
		if (rc < 0) {
			return rc;
		}
	}

	return rc;
}


/* Some SoC can run in single or dual bank mode, others can't.
 * Different SoC flash layouts are specified in various reference
 * manuals, but the flash layout for a given number of sectors is
 * consistent across these manuals. The number of sectors is given
 * by the HAL as FLASH_SECTOR_TOTAL. And some SoC that with same
 * FLASH_SECTOR_TOTAL have different flash size.
 *
 * In case of 8 sectors and 24 sectors we need to differentiate
 * between two cases by using the memory size.
 * In case of 24 sectors we need to check if the SoC is running
 * in single or dual bank mode.
 */
#ifndef FLASH_SECTOR_TOTAL
#error "Unknown flash layout"
#elif FLASH_SECTOR_TOTAL == 2
static const struct flash_pages_layout stm32f7_flash_layout[] = {
	/* RM0385, table 4: STM32F750xx */
	{.pages_count = 2, .pages_size = KB(32)},
};
#elif FLASH_SECTOR_TOTAL == 4
static const struct flash_pages_layout stm32f7_flash_layout[] = {
	/* RM0431, table 4: STM32F730xx */
	{.pages_count = 4, .pages_size = KB(16)},
};
#elif FLASH_SECTOR_TOTAL == 8
#if CONFIG_FLASH_SIZE == 512
static const struct flash_pages_layout stm32f7_flash_layout[] = {
	/* RM0431, table 3: STM32F72xxx and STM32F732xx/F733xx */
	{.pages_count = 4, .pages_size = KB(16)},
	{.pages_count = 1, .pages_size = KB(64)},
	{.pages_count = 3, .pages_size = KB(128)},
};
#elif CONFIG_FLASH_SIZE == 1024
static const struct flash_pages_layout stm32f7_flash_layout[] = {
	/* RM0385, table 3: STM32F756xx and STM32F74xxx */
	{.pages_count = 4, .pages_size = KB(32)},
	{.pages_count = 1, .pages_size = KB(128)},
	{.pages_count = 3, .pages_size = KB(256)},
};
#endif /* CONFIG_FLASH_SIZE */
#elif FLASH_SECTOR_TOTAL == 24
static const struct flash_pages_layout stm32f7_flash_layout_single_bank[] = {
	/* RM0410, table 3: STM32F76xxx and STM32F77xxx in single bank */
	{.pages_count = 4, .pages_size = KB(32)},
	{.pages_count = 1, .pages_size = KB(128)},
	{.pages_count = 7, .pages_size = KB(256)},
};
static const struct flash_pages_layout stm32f7_flash_layout_dual_bank[] = {
	/* RM0410, table 4: STM32F76xxx and STM32F77xxx in dual bank */
	{.pages_count = 4, .pages_size = KB(16)},
	{.pages_count = 1, .pages_size = KB(64)},
	{.pages_count = 7, .pages_size = KB(128)},
	{.pages_count = 4, .pages_size = KB(16)},
	{.pages_count = 1, .pages_size = KB(64)},
	{.pages_count = 7, .pages_size = KB(128)},
};
#else
#error "Unknown flash layout"
#endif/* !defined(FLASH_SECTOR_TOTAL) */

void flash_stm32_page_layout(struct device *dev,
			     const struct flash_pages_layout **layout,
			     size_t *layout_size)
{
#if FLASH_OPTCR_nDBANK
	if (FLASH_STM32_REGS(dev)->optcr & FLASH_OPTCR_nDBANK) {
		*layout = stm32f7_flash_layout_single_bank;
		*layout_size = ARRAY_SIZE(stm32f7_flash_layout_single_bank);
	} else {
		*layout = stm32f7_flash_layout_dual_bank;
		*layout_size = ARRAY_SIZE(stm32f7_flash_layout_dual_bank);
	}
#else
	ARG_UNUSED(dev);
	*layout = stm32f7_flash_layout;
	*layout_size = ARRAY_SIZE(stm32f7_flash_layout);
#endif
}
