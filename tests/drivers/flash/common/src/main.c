/*
 * Copyright (c) 2020-2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/ztest.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/devicetree.h>
#include <zephyr/storage/flash_map.h>

#if defined(CONFIG_NORDIC_QSPI_NOR)
#define TEST_AREA_DEV_NODE	DT_INST(0, nordic_qspi_nor)
#elif defined(CONFIG_SPI_NOR)
#define TEST_AREA_DEV_NODE	DT_INST(0, jedec_spi_nor)
#else
#define TEST_AREA	storage_partition
#endif

/* TEST_AREA is only defined for configurations that realy on
 * fixed-partition nodes.
 */
#ifdef TEST_AREA
#define TEST_AREA_OFFSET	FIXED_PARTITION_OFFSET(TEST_AREA)
#define TEST_AREA_SIZE		FIXED_PARTITION_SIZE(TEST_AREA)
#define TEST_AREA_MAX		(TEST_AREA_OFFSET + TEST_AREA_SIZE)
#define TEST_AREA_DEVICE	FIXED_PARTITION_DEVICE(TEST_AREA)

#elif defined(TEST_AREA_DEV_NODE)

#define TEST_AREA_DEVICE	DEVICE_DT_GET(TEST_AREA_DEV_NODE)
#define TEST_AREA_OFFSET	0xff000

#if DT_NODE_HAS_PROP(TEST_AREA_DEV_NODE, size_in_bytes)
#define TEST_AREA_MAX DT_PROP(TEST_AREA_DEV_NODE, size_in_bytes)
#else
#define TEST_AREA_MAX (DT_PROP(TEST_AREA_DEV_NODE, size) / 8)
#endif

#else
#error "Unsupported configuraiton"
#endif

#define TEST_FLASH_START (DT_REG_ADDR(DT_CHOSEN(zephyr_flash)))
#if (DT_PROP(DT_CHOSEN(zephyr_flash), size))
/* When flash is defined in DTS as:
 * size = < 0x4000000 >;
 * reg = < 0x0 >;
 */
#define TEST_FLASH_SIZE (DT_PROP(DT_CHOSEN(zephyr_flash), size))
#else
/* When flash is defined in DTS as:
 * reg = < 0xe000000 0x200000 >;
 */
#define TEST_FLASH_SIZE (DT_REG_SIZE(DT_CHOSEN(zephyr_flash)))
#endif /* #if (DT_PROP(DT_CHOSEN(zephyr_flash), size)) */

#define EXPECTED_SIZE	512

static const struct device *const flash_dev = TEST_AREA_DEVICE;
static struct flash_pages_info page_info;
static uint8_t __aligned(4) expected[EXPECTED_SIZE];
static const struct flash_parameters *flash_params;
static uint8_t erase_value;

static void *flash_driver_setup(void)
{
	int rc;

	TC_PRINT("Test will run on device %s\n", flash_dev->name);
	zassert_true(device_is_ready(flash_dev));

	flash_params = flash_get_parameters(flash_dev);
	erase_value = flash_params->erase_value;

	/* For tests purposes use page (in nrf_qspi_nor page = 64 kB) */
	flash_get_page_info_by_offs(flash_dev, TEST_AREA_OFFSET,
				    &page_info);

	/* Check if test region is not empty */
	uint8_t buf[EXPECTED_SIZE];

	rc = flash_read(flash_dev, TEST_AREA_OFFSET,
			buf, EXPECTED_SIZE);
	zassert_equal(rc, 0, "Cannot read flash");

	/* Fill test buffer with random data */
	for (int i = 0, val = 0; i < EXPECTED_SIZE; i++, val++) {
		/* Skip erase value */
		if (val == erase_value) {
			val++;
		}
		expected[i] = val;
	}

	/* Check if tested region fits in flash */
	zassert_true((TEST_AREA_OFFSET + EXPECTED_SIZE) <= TEST_AREA_MAX,
		     "Test area exceeds flash size");

	/* Check if flash is cleared */
	bool is_buf_clear = true;

	for (off_t i = 0; i < EXPECTED_SIZE; i++) {
		if (buf[i] != erase_value) {
			is_buf_clear = false;
			break;
		}
	}

	if (!is_buf_clear) {
		/* Erase a nb of pages aligned to the EXPECTED_SIZE */
		rc = flash_erase(flash_dev, page_info.start_offset,
				(page_info.size *
				((EXPECTED_SIZE + page_info.size - 1)
				/ page_info.size)));

		zassert_equal(rc, 0, "Flash memory not properly erased");
	}

	return NULL;
}

ZTEST(flash_driver, test_read_unaligned_address)
{
	int rc;
	uint8_t buf[EXPECTED_SIZE];
	const uint8_t canary = erase_value;

	rc = flash_write(flash_dev,
			 page_info.start_offset,
			 expected, EXPECTED_SIZE);
	zassert_equal(rc, 0, "Cannot write to flash");

	/* read buffer length*/
	for (off_t len = 0; len < 25; len++) {
		/* address offset */
		for (off_t ad_o = 0; ad_o < 4; ad_o++) {
			/* buffer offset; leave space for buffer guard */
			for (off_t buf_o = 1; buf_o < 5; buf_o++) {
				/* buffer overflow protection */
				buf[buf_o - 1] = canary;
				buf[buf_o + len] = canary;
				memset(buf + buf_o, 0, len);
				rc = flash_read(flash_dev,
						page_info.start_offset + ad_o,
						buf + buf_o, len);
				zassert_equal(rc, 0, "Cannot read flash");
				zassert_equal(memcmp(buf + buf_o,
						expected + ad_o,
						len),
					0, "Flash read failed at len=%d, "
					"ad_o=%d, buf_o=%d", len, ad_o, buf_o);
				/* check buffer guards */
				zassert_equal(buf[buf_o - 1], canary,
					"Buffer underflow at len=%d, "
					"ad_o=%d, buf_o=%d", len, ad_o, buf_o);
				zassert_equal(buf[buf_o + len], canary,
					"Buffer overflow at len=%d, "
					"ad_o=%d, buf_o=%d", len, ad_o, buf_o);
			}
		}
	}
}

ZTEST(flash_driver, test_flash_erase)
{
	int rc;
	uint8_t read_buf[EXPECTED_SIZE];
	bool comparison_result;
	const struct flash_parameters *fparams = flash_get_parameters(flash_dev);

	erase_value = fparams->erase_value;

	/* Write test data */
	rc = flash_write(flash_dev, page_info.start_offset, expected, EXPECTED_SIZE);
	zassert_equal(rc, 0, "Cannot write to flash");

	/* Confirm write operation */
	rc = flash_read(flash_dev, page_info.start_offset, read_buf, EXPECTED_SIZE);
	zassert_equal(rc, 0, "Cannot read flash");

	comparison_result = true;
	for (int i = 0; i < EXPECTED_SIZE; i++) {
		if (read_buf[i] != expected[i]) {
			comparison_result = false;
			TC_PRINT("i=%d:\tread_buf[i]=%d\texpected[i]=%d\n", i, read_buf[i],
				 expected[i]);
		}
	}
	zassert_true(comparison_result, "Write operation failed");
	/* Cross check - confirm that expected data is pseudo-random */
	zassert_not_equal(read_buf[0], expected[1], "These values shall be different");

	/* Erase a nb of pages aligned to the EXPECTED_SIZE */
	rc = flash_erase(
		flash_dev, page_info.start_offset,
		(page_info.size * ((EXPECTED_SIZE + page_info.size - 1) / page_info.size)));
	zassert_equal(rc, 0, "Flash memory not properly erased");

	/* Confirm erase operation */
	rc = flash_read(flash_dev, page_info.start_offset, read_buf, EXPECTED_SIZE);
	zassert_equal(rc, 0, "Cannot read flash");

	comparison_result = true;
	for (int i = 0; i < EXPECTED_SIZE; i++) {
		if (read_buf[i] != erase_value) {
			comparison_result = false;
			TC_PRINT("i=%d:\tread_buf[i]=%d\texpected=%d\n", i, read_buf[i],
				 erase_value);
		}
	}
	zassert_true(comparison_result, "Write operation failed");
	/* Cross check - confirm that expected data
	 * doesn't contain erase_value
	 */
	zassert_not_equal(expected[0], erase_value, "These values shall be different");
}

ZTEST(flash_driver, test_negative_flash_erase)
{
	int rc;

#if !defined(TEST_AREA)
	/* Flash memory boundaries are correctly calculated
	 * only for storage_partition.
	 */
	ztest_test_skip();
#endif

	TC_PRINT("TEST_FLASH_START = 0x%lx\n", (unsigned long)TEST_FLASH_START);
	TC_PRINT("TEST_FLASH_SIZE = 0x%lx\n", (unsigned long)TEST_FLASH_SIZE);

	/* Check error returned when erasing memory at wrong address (too low) */
	rc = flash_erase(flash_dev, (TEST_FLASH_START - 1), EXPECTED_SIZE);
	zassert_true(rc < 0, "Invalid use of flash_erase returned %d", rc);

	/* Check error returned when erasing memory at wrong address (too high) */
	rc = flash_erase(flash_dev, (TEST_FLASH_START + TEST_FLASH_SIZE), EXPECTED_SIZE);
	zassert_true(rc < 0, "Invalid use of flash_erase returned %d", rc);

	/* Check error returned when erasing to large chunk of memory */
	rc = flash_erase(flash_dev, TEST_AREA_OFFSET, (TEST_FLASH_SIZE - TEST_AREA_OFFSET + 1));
	zassert_true(rc < 0, "Invalid use of flash_erase returned %d", rc);

	/* Erasing 0 bytes shall succeed */
	rc = flash_erase(flash_dev, TEST_AREA_OFFSET, 0);
	zassert_true(rc == 0, "flash_erase 0 bytes returned %d", rc);
}

ZTEST(flash_driver, test_negative_flash_fill)
{
	int rc;
	uint8_t fill_val = 0xA; /* Dummy value */

#if !defined(TEST_AREA)
	/* Flash memory boundaries are correctly calculated
	 * only for storage_partition.
	 */
	ztest_test_skip();
#endif

	/* Check error returned when filling memory at wrong address (too low) */
	rc = flash_fill(flash_dev, fill_val, (TEST_FLASH_START - 1), EXPECTED_SIZE);
	zassert_true(rc < 0, "Invalid use of flash_fill returned %d", rc);

	/* Check error returned when filling memory at wrong address (too high) */
	rc = flash_fill(flash_dev, fill_val, (TEST_FLASH_START + TEST_FLASH_SIZE), EXPECTED_SIZE);
	zassert_true(rc < 0, "Invalid use of flash_fill returned %d", rc);

	/* Check error returned when filling to large chunk of memory */
	rc = flash_fill(flash_dev, fill_val, TEST_AREA_OFFSET,
			(TEST_FLASH_SIZE - TEST_AREA_OFFSET + 1));
	zassert_true(rc < 0, "Invalid use of flash_fill returned %d", rc);

	/* Filling 0 bytes shall succeed */
	rc = flash_fill(flash_dev, fill_val, TEST_AREA_OFFSET, 0);
	zassert_true(rc == 0, "flash_fill 0 bytes returned %d", rc);
}

ZTEST(flash_driver, test_negative_flash_flatten)
{
	int rc;

#if !defined(TEST_AREA)
	/* Flash memory boundaries are correctly calculated
	 * only for storage_partition.
	 */
	ztest_test_skip();
#endif

	/* Check error returned when flatten memory at wrong address (too low) */
	rc = flash_flatten(flash_dev, (TEST_FLASH_START - 1), EXPECTED_SIZE);
	zassert_true(rc < 0, "Invalid use of flash_flatten returned %d", rc);

	/* Check error returned when flatten memory at wrong address (too high) */
	rc = flash_flatten(flash_dev, (TEST_FLASH_START + TEST_FLASH_SIZE), EXPECTED_SIZE);
	zassert_true(rc < 0, "Invalid use of flash_flatten returned %d", rc);

	/* Check error returned when flatten to large chunk of memory */
	rc = flash_flatten(flash_dev, TEST_AREA_OFFSET, (TEST_FLASH_SIZE - TEST_AREA_OFFSET + 1));
	zassert_true(rc < 0, "Invalid use of flash_flatten returned %d", rc);

	/* Flatten 0 bytes shall succeed */
	rc = flash_flatten(flash_dev, TEST_AREA_OFFSET, 0);
	zassert_true(rc == 0, "flash_flatten 0 bytes returned %d", rc);
}

ZTEST(flash_driver, test_negative_flash_read)
{
	int rc;
	uint8_t read_buf[EXPECTED_SIZE];

#if !defined(TEST_AREA)
	/* Flash memory boundaries are correctly calculated
	 * only for storage_partition.
	 */
	ztest_test_skip();
#endif

	/* Check error returned when reading from a wrong address (too low) */
	rc = flash_read(flash_dev, (TEST_FLASH_START - 1), read_buf, EXPECTED_SIZE);
	zassert_true(rc < 0, "Invalid use of flash_read returned %d", rc);

	/* Check error returned when reading from a wrong address (too high) */
	rc = flash_read(flash_dev, (TEST_FLASH_START + TEST_FLASH_SIZE), read_buf, EXPECTED_SIZE);
	zassert_true(rc < 0, "Invalid use of flash_read returned %d", rc);

	/* Check error returned when reading too many data */
	rc = flash_read(flash_dev, TEST_AREA_OFFSET, read_buf,
			(TEST_FLASH_SIZE - TEST_AREA_OFFSET + 1));
	zassert_true(rc < 0, "Invalid use of flash_read returned %d", rc);

	/* Reading 0 bytes shall succeed */
	rc = flash_read(flash_dev, TEST_AREA_OFFSET, read_buf, 0);
	zassert_true(rc == 0, "flash_read 0 bytes returned %d", rc);
}

ZTEST(flash_driver, test_negative_flash_write)
{
	int rc;

#if !defined(TEST_AREA)
	/* Flash memory boundaries are correctly calculated
	 * only for storage_partition.
	 */
	ztest_test_skip();
#endif

	/* Check error returned when writing to a wrong address (too low) */
	rc = flash_write(flash_dev, (TEST_FLASH_START - 1), expected, EXPECTED_SIZE);
	zassert_true(rc < 0, "Invalid use of flash_write returned %d", rc);

	/* Check error returned when writing to a wrong address (too high) */
	rc = flash_write(flash_dev, (TEST_FLASH_START + TEST_FLASH_SIZE), expected, EXPECTED_SIZE);
	zassert_true(rc < 0, "Invalid use of flash_write returned %d", rc);

	/* Check error returned when writing too many data */
	rc = flash_write(flash_dev, TEST_AREA_OFFSET, expected,
			 (TEST_FLASH_SIZE - TEST_AREA_OFFSET + 1));
	zassert_true(rc < 0, "Invalid use of flash_write returned %d", rc);

	/* Writing 0 bytes shall succeed */
	rc = flash_write(flash_dev, TEST_AREA_OFFSET, expected, 0);
	zassert_true(rc == 0, "flash_write 0 bytes returned %d", rc);
}

struct test_cb_data_type {
	uint32_t page_counter; /* used to count how many pages was iterated */
	uint32_t exit_page;    /* terminate iteration when this page is reached */
};

static bool flash_callback(const struct flash_pages_info *info, void *data)
{
	struct test_cb_data_type *cb_data = (struct test_cb_data_type *)data;

	cb_data->page_counter++;

	if (cb_data->page_counter >= cb_data->exit_page) {
		return false;
	}

	return true;
}

ZTEST(flash_driver, test_flash_page_layout)
{
	int rc;
	struct flash_pages_info page_info_off = {0};
	struct flash_pages_info page_info_idx = {0};
	size_t page_count;
	struct test_cb_data_type test_cb_data = {0};

#if !defined(CONFIG_FLASH_PAGE_LAYOUT)
	ztest_test_skip();
#endif

	/* Get page info with flash_get_page_info_by_offs() */
	rc = flash_get_page_info_by_offs(flash_dev, TEST_AREA_OFFSET, &page_info_off);
	zassert_true(rc == 0, "flash_get_page_info_by_offs returned %d", rc);
	TC_PRINT("start_offset=0x%lx\tsize=%d\tindex=%d\n", page_info_off.start_offset,
		 (int)page_info_off.size, page_info_off.index);
	zassert_true(page_info_off.start_offset >= 0, "start_offset is %d", rc);
	zassert_true(page_info_off.size > 0, "size is %d", rc);
	zassert_true(page_info_off.index >= 0, "index is %d", rc);

	/* Get info for the same page with flash_get_page_info_by_idx() */
	rc = flash_get_page_info_by_idx(flash_dev, page_info_off.index, &page_info_idx);
	zassert_true(rc == 0, "flash_get_page_info_by_offs returned %d", rc);
	zassert_equal(page_info_off.start_offset, page_info_idx.start_offset);
	zassert_equal(page_info_off.size, page_info_idx.size);
	zassert_equal(page_info_off.index, page_info_idx.index);

	page_count = flash_get_page_count(flash_dev);
	TC_PRINT("page_count=%d\n", (int)page_count);
	zassert_true(page_count > 0, "flash_get_page_count returned %d", rc);
	zassert_true(page_count >= page_info_off.index);

	/* Test that callback is executed for every page */
	test_cb_data.exit_page = page_count + 1;
	flash_page_foreach(flash_dev, flash_callback, &test_cb_data);
	zassert_true(page_count == test_cb_data.page_counter,
		     "page_count = %d not equal to pages counted with cb = %d", page_count,
		     test_cb_data.page_counter);

	/* Test that callback can cancell iteration */
	test_cb_data.page_counter = 0;
	test_cb_data.exit_page = page_count >> 1;
	flash_page_foreach(flash_dev, flash_callback, &test_cb_data);
	zassert_true(test_cb_data.exit_page == test_cb_data.page_counter,
		     "%d pages were iterated while it shall stop on page %d",
		     test_cb_data.page_counter, test_cb_data.exit_page);
}

ZTEST_SUITE(flash_driver, NULL, flash_driver_setup, NULL, NULL, NULL);
