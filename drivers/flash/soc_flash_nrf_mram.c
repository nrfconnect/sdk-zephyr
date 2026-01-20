/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>

#include <zephyr/drivers/flash.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/barrier.h>
#include <zephyr/cache.h>
#if defined(CONFIG_MRAM_LATENCY)
#include "../soc/nordic/common/mram_latency.h"
#endif
#if defined(CONFIG_NRF_IRONSIDE_BOOT_REPORT)
#include "../soc/nordic/ironside/include/nrf_ironside/boot_report.h"
#endif

LOG_MODULE_REGISTER(flash_nrf_mram, CONFIG_FLASH_LOG_LEVEL);

#define DT_DRV_COMPAT nordic_mram

#define MRAM_START DT_INST_REG_ADDR(0)
#define MRAM_SIZE  DT_INST_REG_SIZE(0)

#define MRAM_WORD_SIZE 16
#define MRAM_WORD_MASK 0xf

#define WRITE_BLOCK_SIZE DT_INST_PROP_OR(0, write_block_size, MRAM_WORD_SIZE)
#define ERASE_BLOCK_SIZE DT_INST_PROP_OR(0, erase_block_size, WRITE_BLOCK_SIZE)

#define ERASE_VALUE 0xff

#define SOC_NRF_MRAM_BANK_11_OFFSET  0x100000
#define SOC_NRF_MRAM_BANK_11_ADDRESS (MRAM_START + SOC_NRF_MRAM_BANK_11_OFFSET)
#define SOC_NRF_MRAMC_BASE_ADDR_10   0x5f092000
#define SOC_NRF_MRAMC_BASE_ADDR_11   0x5f093000
#define SOC_NRF_MRAMC_READY_REG_0    (SOC_NRF_MRAMC_BASE_ADDR_10 + 0x400)
#define SOC_NRF_MRAMC_READY_REG_1    (SOC_NRF_MRAMC_BASE_ADDR_11 + 0x400)

#define IRONSIDE_SE_VER_MASK 0x000000FF /* This is the Mask for Ironside SE Seqnum */
#define IRONSIDE_SE_SUPPORT_READY_VER 21

#define MRAM_WRITE_MAX_RETRIES 3

BUILD_ASSERT(MRAM_START > 0, "nordic,mram: start address expected to be non-zero");
BUILD_ASSERT((ERASE_BLOCK_SIZE % WRITE_BLOCK_SIZE) == 0,
	     "erase-block-size expected to be a multiple of write-block-size");

struct nrf_mram_data_t {
	uint8_t ironside_se_ver;
};

enum write_mode_t {
	WRITE_WORD,
	ERASE_WORD
};

static inline uint32_t nrf_mram_ready(uint32_t addr, uint8_t ironside_se_ver)
{
	if (ironside_se_ver < IRONSIDE_SE_SUPPORT_READY_VER) {
		return 1;
	}

	if (addr < SOC_NRF_MRAM_BANK_11_ADDRESS) {
		return sys_read32(SOC_NRF_MRAMC_READY_REG_0);
	} else {
		return sys_read32(SOC_NRF_MRAMC_READY_REG_1);
	}
}

/**
 * @param[in,out] offset      Relative offset into memory, from the driver API.
 * @param[in]     len         Number of bytes for the intended operation.
 * @param[in]     must_align  Require MRAM word alignment, if applicable.
 *
 * @return Absolute address in MRAM, or NULL if @p offset or @p len are not
 *         within bounds or appropriately aligned.
 */
static uintptr_t validate_and_map_addr(off_t offset, size_t len, bool must_align)
{
	if (unlikely(offset < 0 || offset >= MRAM_SIZE || len > MRAM_SIZE - offset)) {
		LOG_ERR("invalid offset: %ld:%zu", offset, len);
		return 0;
	}

	const uintptr_t addr = MRAM_START + offset;

	if (WRITE_BLOCK_SIZE > 1 && must_align &&
	    unlikely((addr % WRITE_BLOCK_SIZE) != 0 || (len % WRITE_BLOCK_SIZE) != 0)) {
		LOG_ERR("invalid alignment: %p:%zu", (void *)addr, len);
		return 0;
	}

	return addr;
}

/**
 * @param[in] addr_end  Last modified MRAM address (not inclusive).
 */
static void commit_changes(uintptr_t addr_end)
{
	/* Barrier following our last write. */
	barrier_dmem_fence_full();

	if ((WRITE_BLOCK_SIZE & MRAM_WORD_MASK) == 0 || (addr_end & MRAM_WORD_MASK) == 0) {
		/* Our last operation was MRAM word-aligned, so we're done.
		 * Note: if WRITE_BLOCK_SIZE is a multiple of MRAM_WORD_SIZE,
		 * then this was already checked in validate_and_map_addr().
		 */
		return;
	}

	/* Get the most significant byte (MSB) of the last MRAM word we were modifying.
	 * Writing to this byte makes the MRAM controller commit other pending writes to that word.
	 */
	addr_end |= MRAM_WORD_MASK;

	/* Issue a dummy write, since we didn't have anything to write here.
	 * Doing this lets us finalize our changes before we exit the driver API.
	 */
	sys_write8(sys_read8(addr_end), addr_end);
}

static inline int nrf_mram_write_and_verify(uint32_t addr, void *data, size_t len,
					    enum write_mode_t mode, uint8_t ironside_se_ver)
{
	int result = 0;

	__asm__ volatile(
		".globl nrf_mram_verif_start\n"
		".globl nrf_mram_verif_end\n"

		/* r8 = retry counter, r9 = addr, r10 = data_ptr, r11 = mode, r4 = len */
		"mov r8, %[max_retries]\n"
		"mov r9, %[addr_val]\n"
		"mov r10, %[data_ptr]\n"
		"mov r11, %[mode_val]\n"
		"mov r4, %[len_val]\n"

		"nrf_mram_verif_start:\n"
		/* Check retries */
		".L_retry_check:\n"
		"cmp r8, %[max_retries]\n"  /* Is this first attempt? */
		"beq .L_first_attempt\n"
		/* Not first attempt - this is a retry after bus fault */
		"subs r8, r8, #1\n"
		"ble .L_return_eio\n"
		"b .L_store_to_mram_retry\n"      /* Skip to store, data already in r0-r3 */
		".L_first_attempt:\n"
		"subs r8, r8, #1\n"

		/* Wait until MRAM is ready */
		"mov r12, %[se_ver]\n"
		"cmp r12, %[support_ver]\n"
		"blt .L_ready_done\n"
		"cmp r9, %[bank11]\n"
		"blt .L_use_ready0\n"
		"mov r0, %[ready1]\n"
		"b .L_poll_ready\n"
		".L_use_ready0:\n"
		"mov r0, %[ready0]\n"
		".L_poll_ready:\n"
		"ldr r1, [r0]\n"
		"cmp r1, #0\n"
		"beq .L_poll_ready\n"
		".L_ready_done:\n"

		/* Check mode: 0=WRITE_WORD, 1=ERASE_WORD */
		"cmp r11, #0\n"
		"beq .L_write_mode\n"
		"cmp r11, #1\n"
		"beq .L_erase_mode\n"
		"b .L_return_einval\n"

		/* WRITE_WORD mode: Handle partial or full writes */
		".L_write_mode:\n"
		"cmp r10, #0\n"
		"beq .L_return_einval\n"
		"cmp r4, %[word_size]\n"
		"beq .L_write_full\n"

		/* Partial write: load len bytes into r0-r3 */
		"mov r0, #0\n"          /* Clear r0 */
		"mov r1, #0\n"          /* Clear r1 */
		"mov r2, #0\n"          /* Clear r2 */
		"mov r3, #0\n"          /* Clear r3 */
		"mov r5, #0\n"          /* r5 = byte counter */
		".L_load_write_bytes:\n"
		"cmp r5, r4\n"          /* if counter >= len, done */
		"bge .L_write_load_msb\n"
		"ldrb r6, [r10], #1\n"  /* Load byte from src, increment */
		"and r12, r5, #3\n"     /* r12 = byte position in word (0-3) */
		"lsl r12, r12, #3\n"    /* r12 = bit shift (0, 8, 16, 24) */
		"lsl r6, r6, r12\n"     /* Shift byte to position */
		"mov r12, r5\n"
		"lsr r12, r12, #2\n"    /* r12 = word index (0-3) */
		"cmp r12, #0\n"
		"bne 1f\n"
		"orr r0, r0, r6\n"      /* Store in r0 */
		"b 2f\n"
		"1: cmp r12, #1\n"
		"bne 1f\n"
		"orr r1, r1, r6\n"      /* Store in r1 */
		"b 2f\n"
		"1: cmp r12, #2\n"
		"bne 1f\n"
		"orr r2, r2, r6\n"      /* Store in r2 */
		"b 2f\n"
		"1: orr r3, r3, r6\n"   /* Store in r3 */
		"2: add r5, r5, #1\n"
		"b .L_load_write_bytes\n"

		/* Load MSB from aligned MRAM word into r3 */
		".L_write_load_msb:\n"
		"mov r5, r9\n"          /* r5 = addr */
		"orr r5, r5, #0xF\n"    /* Align to MRAM word MSB */
		"ldrb r6, [r5]\n"       /* Load MSB byte */
		/* Calculate bit position for MSB in r3 */
		"mov r5, #15\n"         /* MSB is at byte 15 */
		"and r7, r5, #3\n"      /* Byte position in word (15 & 3 = 3) */
		"lsl r7, r7, #3\n"      /* Bit shift = 24 */
		"lsl r6, r6, r7\n"      /* Shift MSB to position */
		"orr r3, r3, r6\n"      /* Insert MSB into r3 */
		"b .L_store_to_mram\n"

		/* Full write: load 16 bytes from data into r0-r3 */
		".L_write_full:\n"
		"ldmia r10!, {r0, r1, r2, r3}\n"
		"b .L_store_to_mram\n"

		/* ERASE_WORD mode: Handle partial or full erase */
		".L_erase_mode:\n"
		"cmp r4, %[word_size]\n"
		"beq .L_erase_full\n"

		/* Partial erase: load 0xFF for len bytes into r0-r3 */
		"mov r0, #0\n"          /* Clear r0 */
		"mov r1, #0\n"          /* Clear r1 */
		"mov r2, #0\n"          /* Clear r2 */
		"mov r3, #0\n"          /* Clear r3 */
		"mov r5, #0\n"          /* r5 = byte counter */
		".L_load_erase_bytes:\n"
		"cmp r5, r4\n"          /* if counter >= len, done */
		"bge .L_erase_load_msb\n"
		"mov r6, #0xFF\n"       /* Erase value */
		"and r7, r5, #3\n"      /* r7 = byte position in word */
		"lsl r7, r7, #3\n"      /* r7 = bit shift (0, 8, 16, 24) */
		"lsl r6, r6, r7\n"      /* Shift 0xFF to position */
		"mov r7, r5\n"
		"lsr r7, r7, #2\n"      /* r7 = word index (0-3) */
		"cmp r7, #0\n"
		"bne 1f\n"
		"orr r0, r0, r6\n"      /* Store in r0 */
		"b 2f\n"
		"1: cmp r7, #1\n"
		"bne 1f\n"
		"orr r1, r1, r6\n"      /* Store in r1 */
		"b 2f\n"
		"1: cmp r7, #2\n"
		"bne 1f\n"
		"orr r2, r2, r6\n"      /* Store in r2 */
		"b 2f\n"
		"1: orr r3, r3, r6\n"   /* Store in r3 */
		"2: add r5, r5, #1\n"
		"b .L_load_erase_bytes\n"

		/* Load MSB from aligned MRAM word into r3 */
		".L_erase_load_msb:\n"
		"mov r5, r9\n"          /* r5 = addr */
		"orr r5, r5, #0xF\n"    /* Align to MRAM word MSB */
		"ldrb r6, [r5]\n"       /* Load MSB byte */
		/* Calculate bit position for MSB in r3 */
		"mov r5, #15\n"         /* MSB is at byte 15 */
		"and r7, r5, #3\n"      /* Byte position in word (15 & 3 = 3) */
		"lsl r7, r7, #3\n"      /* Bit shift = 24 */
		"lsl r6, r6, r7\n"      /* Shift MSB to position */
		"orr r3, r3, r6\n"      /* Insert MSB into r3 */
		"b .L_store_to_mram\n"

		/* Full erase: load 0xFFFFFFFF into r0-r3 */
		".L_erase_full:\n"
		"movw r0, #0xFFFF\n"
		"movt r0, #0xFFFF\n"
		"mov r1, r0\n"
		"mov r2, r0\n"
		"mov r3, r0\n"
		"b .L_store_to_mram\n"

		/* Wait until MRAM is ready */
		".L_store_to_mram_retry:\n"
		"mov r12, %[se_ver]\n"
		"cmp r12, %[support_ver]\n"
		"blt .L_ready_done_retry\n"
		"cmp r9, %[bank11]\n"
		"blt .L_use_ready0_retry\n"
		"mov r0, %[ready1]\n"
		"b .L_poll_ready_retry\n"
		".L_use_ready0_retry:\n"
		"mov r0, %[ready0]\n"
		".L_poll_ready_retry:\n"
		"ldr r1, [r0]\n"
		"cmp r1, #0\n"
		"beq .L_poll_ready_retry\n"
		".L_ready_done_retry:\n"

		/* Common store section: write r0-r3 to MRAM at addr (r9) */
		".L_store_to_mram:\n"
		"stmia r9!, {r0, r1, r2, r3}\n"
		"dmb\n"
		"sub r9, r9, #16\n"

		/* Verify: Read back 16 bytes into r4-r7 */
		".L_verify:\n"
		"ldmia r9, {r4, r5, r6, r7}\n"
		"dmb\n"

		"nrf_mram_verif_end:\n"
		/* Verification complete - return success */
		"b .L_return_ok\n"

		/* Return codes */
		".L_return_eio:\n"
		"mov %[result], %[eio_val]\n"
		"b .L_done\n"

		".L_return_einval:\n"
		"mov %[result], %[einval_val]\n"
		"b .L_done\n"

		".L_return_ok:\n"
		"mov %[result], #0\n"

		".L_done:\n"

		: [result] "=r" (result)
		: [addr_val] "r" (addr),
		  [data_ptr] "r" (data),
		  [len_val] "r" (len),
		  [mode_val] "r" (mode),
		  [se_ver] "r" (ironside_se_ver),
		  [support_ver] "i" (IRONSIDE_SE_SUPPORT_READY_VER),
		  [bank11] "r" (SOC_NRF_MRAM_BANK_11_ADDRESS),
		  [ready0] "r" (SOC_NRF_MRAMC_READY_REG_0),
		  [ready1] "r" (SOC_NRF_MRAMC_READY_REG_1),
		  [word_size] "i" (MRAM_WORD_SIZE),
		  [max_retries] "i" (MRAM_WRITE_MAX_RETRIES),
		  [eio_val] "r" (-EIO),
		  [einval_val] "r" (-EINVAL)
		: "memory", "cc"
	);

	return result;
}

static int nrf_mram_read(const struct device *dev, off_t offset, void *data, size_t len)
{
	ARG_UNUSED(dev);

	const uintptr_t addr = validate_and_map_addr(offset, len, false);

	if (!addr) {
		return -EINVAL;
	}

	LOG_DBG("read: %p:%zu", (void *)addr, len);

	memcpy(data, (void *)addr, len);

	return 0;
}

static int nrf_mram_write(const struct device *dev, off_t offset, const void *data, size_t len)
{
	struct nrf_mram_data_t *nrf_mram_data = dev->data;
	uint8_t ironside_se_ver = nrf_mram_data->ironside_se_ver;

	const uintptr_t addr = validate_and_map_addr(offset, len, true);

	if (!addr) {
		return -EINVAL;
	}

	LOG_DBG("write: %p:%zu", (void *)addr, len);

	if (ironside_se_ver >= IRONSIDE_SE_SUPPORT_READY_VER) {
#if defined(CONFIG_MRAM_LATENCY)
		mram_no_latency_sync_request();
#endif
	}
	for (uint32_t i = 0; i < (len / MRAM_WORD_SIZE); i++) {
		nrf_mram_write_and_verify(addr + (i * MRAM_WORD_SIZE),
					  (void *)((uintptr_t)data + (i * MRAM_WORD_SIZE)),
					  MRAM_WORD_SIZE, WRITE_WORD, ironside_se_ver);
	}

	if (len % MRAM_WORD_SIZE) {
		nrf_mram_write_and_verify(addr + (len & ~MRAM_WORD_MASK),
					  (void *)((uintptr_t)data + (len & ~MRAM_WORD_MASK)),
					  len & MRAM_WORD_MASK, WRITE_WORD, ironside_se_ver);
	}

	if (ironside_se_ver >= IRONSIDE_SE_SUPPORT_READY_VER) {
#if defined(CONFIG_MRAM_LATENCY)
		mram_no_latency_sync_release();
#endif
	}

	return 0;
}

static int nrf_mram_erase(const struct device *dev, off_t offset, size_t size)
{
	struct nrf_mram_data_t *nrf_mram_data = dev->data;
	uint8_t ironside_se_ver = nrf_mram_data->ironside_se_ver;

	const uintptr_t addr = validate_and_map_addr(offset, size, true);

	if (!addr) {
		return -EINVAL;
	}

	LOG_DBG("erase: %p:%zu", (void *)addr, size);

	/* Ensure that the mramc banks are powered on */
	if (ironside_se_ver >= IRONSIDE_SE_SUPPORT_READY_VER) {
#if defined(CONFIG_MRAM_LATENCY)
		mram_no_latency_sync_request();
#endif
	}
	for (uint32_t i = 0; i < (size / MRAM_WORD_SIZE); i++) {
		nrf_mram_write_and_verify(addr + (i * MRAM_WORD_SIZE), NULL, MRAM_WORD_SIZE,
					  ERASE_WORD, ironside_se_ver);
	}
	if (size % MRAM_WORD_SIZE) {
		nrf_mram_write_and_verify(addr + (size & ~MRAM_WORD_MASK), NULL,
					  size & MRAM_WORD_MASK, ERASE_WORD, ironside_se_ver);
	}

	if (ironside_se_ver >= IRONSIDE_SE_SUPPORT_READY_VER) {
#if defined(CONFIG_MRAM_LATENCY)
		mram_no_latency_sync_release();
#endif
	}

	return 0;
}

static int nrf_mram_get_size(const struct device *dev, uint64_t *size)
{
	ARG_UNUSED(dev);

	*size = MRAM_SIZE;

	return 0;
}

static const struct flash_parameters *nrf_mram_get_parameters(const struct device *dev)
{
	ARG_UNUSED(dev);

	static const struct flash_parameters parameters = {
		.write_block_size = WRITE_BLOCK_SIZE,
		.erase_value = ERASE_VALUE,
		.caps = {
			.no_explicit_erase = true,
		},
	};

	return &parameters;
}

#if defined(CONFIG_FLASH_PAGE_LAYOUT)
static void nrf_mram_page_layout(const struct device *dev, const struct flash_pages_layout **layout,
				 size_t *layout_size)
{
	ARG_UNUSED(dev);

	static const struct flash_pages_layout pages_layout = {
		.pages_count = (MRAM_SIZE) / (ERASE_BLOCK_SIZE),
		.pages_size = ERASE_BLOCK_SIZE,
	};

	*layout = &pages_layout;
	*layout_size = 1;
}
#endif

static DEVICE_API(flash, nrf_mram_api) = {
	.read = nrf_mram_read,
	.write = nrf_mram_write,
	.erase = nrf_mram_erase,
	.get_size = nrf_mram_get_size,
	.get_parameters = nrf_mram_get_parameters,
#if defined(CONFIG_FLASH_PAGE_LAYOUT)
	.page_layout = nrf_mram_page_layout,
#endif
};

static int nrf_mram_init(const struct device *dev)
{
	struct nrf_mram_data_t *nrf_mram_data = dev->data;
#if defined(CONFIG_NRF_IRONSIDE_BOOT_REPORT)
	int rc = 0;
	const struct ironside_boot_report *report;

	rc = ironside_boot_report_get(&report);

	if (rc) {
		LOG_ERR("Failed to get Ironside boot report");
		return rc;
	}

	nrf_mram_data->ironside_se_ver = FIELD_GET(IRONSIDE_SE_VER_MASK,
						   report->ironside_se_version_int);
	LOG_INF("Ironside SE version detected: %u",
			nrf_mram_data->ironside_se_ver);
#else
	nrf_mram_data->ironside_se_ver = 0;
#endif
	LOG_DBG("Ironside SE version: %u", nrf_mram_data->ironside_se_ver);

	LOG_INF("NRF MRAM flash driver initialized");
	return 0;
}

static struct nrf_mram_data_t nrf_mram_data;

DEVICE_DT_INST_DEFINE(0, nrf_mram_init, NULL, &nrf_mram_data, NULL, POST_KERNEL,
		      CONFIG_FLASH_INIT_PRIORITY, &nrf_mram_api);
