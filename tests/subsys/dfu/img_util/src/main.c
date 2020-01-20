/*
 * Copyright (c) 2017 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <ztest.h>
#include <storage/flash_map.h>
#include <dfu/flash_img.h>

void test_collecting(void)
{
	const struct flash_area *fa;
	struct flash_img_context ctx;
	u32_t i, j;
	u8_t data[5], temp, k;
	int ret;

	ret = flash_img_init(&ctx);
	zassert_true(ret == 0, "Flash img init");

#ifdef CONFIG_IMG_ERASE_PROGRESSIVELY
	u8_t erase_buf[8];
	(void)memset(erase_buf, 0xff, sizeof(erase_buf));

	ret = flash_area_open(DT_FLASH_AREA_IMAGE_1_ID, &fa);
	if (ret) {
		printf("Flash driver was not found!\n");
		return;
	}

	/* ensure image payload area dirt */
	for (i = 0U; i < 300 * sizeof(data) / sizeof(erase_buf); i++) {
		ret = flash_area_write(fa, i * sizeof(erase_buf), erase_buf,
				       sizeof(erase_buf));
		zassert_true(ret == 0, "Flash write failure (%d)", ret);
	}

	/* ensure that the last page dirt */
	ret = flash_area_write(fa, fa->fa_size - sizeof(erase_buf), erase_buf,
			       sizeof(erase_buf));
	zassert_true(ret == 0, "Flash write failure (%d)", ret);
#else
	ret = flash_area_erase(ctx.flash_area, 0, ctx.flash_area->fa_size);
	zassert_true(ret == 0, "Flash erase failure (%d)", ret);
#endif

	zassert(flash_img_bytes_written(&ctx) == 0, "pass", "fail");

	k = 0U;
	for (i = 0U; i < 300; i++) {
		for (j = 0U; j < ARRAY_SIZE(data); j++) {
			data[j] = k++;
		}
		ret = flash_img_buffered_write(&ctx, data, sizeof(data), false);
		zassert_true(ret == 0, "image colletion fail: %d\n", ret);
	}

	zassert(flash_img_buffered_write(&ctx, data, 0, true) == 0, "pass",
					 "fail");


	ret = flash_area_open(DT_FLASH_AREA_IMAGE_1_ID, &fa);
	if (ret) {
		printf("Flash driver was not found!\n");
		return;
	}

	k = 0U;
	for (i = 0U; i < 300 * sizeof(data); i++) {
		zassert(flash_area_read(fa, i, &temp, 1) == 0, "pass", "fail");
		zassert(temp == k, "pass", "fail");
		k++;
	}

#ifdef CONFIG_IMG_ERASE_PROGRESSIVELY
	u8_t buf[sizeof(erase_buf)];

	ret = flash_area_read(fa, fa->fa_size - sizeof(buf), buf, sizeof(buf));
	zassert_true(ret == 0, "Flash read failure (%d)", ret);
	zassert_true(memcmp(erase_buf, buf, sizeof(buf)) == 0,
		     "Image trailer was not cleared");
#endif
}

void test_main(void)
{
	ztest_test_suite(test_util,
			ztest_unit_test(test_collecting));
	ztest_run_test_suite(test_util);
}
