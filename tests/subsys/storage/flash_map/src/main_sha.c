/*
 * Copyright (c) 2017-2024 Nordic Semiconductor ASA
 * Copyright (c) 2015 Runtime Inc
 * Copyright (c) 2020 Gerson Fernando Budke <nandojve@gmail.com>
 * Copyright 2024 NXP
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/ztest.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/storage/flash_map.h>

#define SLOT1_PARTITION		slot1_partition
#define SLOT1_PARTITION_ID	FIXED_PARTITION_ID(SLOT1_PARTITION)
#define SLOT1_PARTITION_DEV	FIXED_PARTITION_DEVICE(SLOT1_PARTITION)
#define SLOT1_PARTITION_NODE	DT_NODELABEL(SLOT1_PARTITION)
#define SLOT1_PARTITION_OFFSET	FIXED_PARTITION_OFFSET(SLOT1_PARTITION)

ZTEST(flash_map_sha, test_flash_area_check_int_sha256)
{
	/* for i in {1..16}; do echo $'0123456789abcdef\nfedcba98765432' >> tst.sha; done
	 * hexdump tst.sha
	 */
	uint8_t tst_vec[] = { 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
			      0x38, 0x39, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66,
			      0x0a, 0x66, 0x65, 0x64, 0x63, 0x62, 0x61, 0x39,
			      0x38, 0x37, 0x36, 0x35, 0x34, 0x33, 0x32, 0x0a,
			      0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
			      0x38, 0x39, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66,
			      0x0a, 0x66, 0x65, 0x64, 0x63, 0x62, 0x61, 0x39,
			      0x38, 0x37, 0x36, 0x35, 0x34, 0x33, 0x32, 0x0a,
			      0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
			      0x38, 0x39, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66,
			      0x0a, 0x66, 0x65, 0x64, 0x63, 0x62, 0x61, 0x39,
			      0x38, 0x37, 0x36, 0x35, 0x34, 0x33, 0x32, 0x0a,
			      0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
			      0x38, 0x39, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66,
			      0x0a, 0x66, 0x65, 0x64, 0x63, 0x62, 0x61, 0x39,
			      0x38, 0x37, 0x36, 0x35, 0x34, 0x33, 0x32, 0x0a,
			      0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
			      0x38, 0x39, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66,
			      0x0a, 0x66, 0x65, 0x64, 0x63, 0x62, 0x61, 0x39,
			      0x38, 0x37, 0x36, 0x35, 0x34, 0x33, 0x32, 0x0a,
			      0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
			      0x38, 0x39, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66,
			      0x0a, 0x66, 0x65, 0x64, 0x63, 0x62, 0x61, 0x39,
			      0x38, 0x37, 0x36, 0x35, 0x34, 0x33, 0x32, 0x0a,
			      0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
			      0x38, 0x39, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66,
			      0x0a, 0x66, 0x65, 0x64, 0x63, 0x62, 0x61, 0x39,
			      0x38, 0x37, 0x36, 0x35, 0x34, 0x33, 0x32, 0x0a,
			      0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
			      0x38, 0x39, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66,
			      0x0a, 0x66, 0x65, 0x64, 0x63, 0x62, 0x61, 0x39,
			      0x38, 0x37, 0x36, 0x35, 0x34, 0x33, 0x32, 0x0a,
			      0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
			      0x38, 0x39, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66,
			      0x0a, 0x66, 0x65, 0x64, 0x63, 0x62, 0x61, 0x39,
			      0x38, 0x37, 0x36, 0x35, 0x34, 0x33, 0x32, 0x0a,
			      0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
			      0x38, 0x39, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66,
			      0x0a, 0x66, 0x65, 0x64, 0x63, 0x62, 0x61, 0x39,
			      0x38, 0x37, 0x36, 0x35, 0x34, 0x33, 0x32, 0x0a,
			      0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
			      0x38, 0x39, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66,
			      0x0a, 0x66, 0x65, 0x64, 0x63, 0x62, 0x61, 0x39,
			      0x38, 0x37, 0x36, 0x35, 0x34, 0x33, 0x32, 0x0a,
			      0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
			      0x38, 0x39, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66,
			      0x0a, 0x66, 0x65, 0x64, 0x63, 0x62, 0x61, 0x39,
			      0x38, 0x37, 0x36, 0x35, 0x34, 0x33, 0x32, 0x0a,
			      0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
			      0x38, 0x39, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66,
			      0x0a, 0x66, 0x65, 0x64, 0x63, 0x62, 0x61, 0x39,
			      0x38, 0x37, 0x36, 0x35, 0x34, 0x33, 0x32, 0x0a,
			      0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
			      0x38, 0x39, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66,
			      0x0a, 0x66, 0x65, 0x64, 0x63, 0x62, 0x61, 0x39,
			      0x38, 0x37, 0x36, 0x35, 0x34, 0x33, 0x32, 0x0a,
			      0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
			      0x38, 0x39, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66,
			      0x0a, 0x66, 0x65, 0x64, 0x63, 0x62, 0x61, 0x39,
			      0x38, 0x37, 0x36, 0x35, 0x34, 0x33, 0x32, 0x0a,
			      0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
			      0x38, 0x39, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66,
			      0x0a, 0x66, 0x65, 0x64, 0x63, 0x62, 0x61, 0x39,
			      0x38, 0x37, 0x36, 0x35, 0x34, 0x33, 0x32, 0x0a };
	/* sha256sum tst.sha */
	uint8_t tst_sha[] = { 0xae, 0xed, 0x7d, 0x59, 0x53, 0xbd, 0xb7, 0x28,
			      0x3e, 0x59, 0xc2, 0x65, 0x59, 0x62, 0xe3, 0x7e,
			      0xfa, 0x97, 0xbd, 0x76, 0xf6, 0xac, 0xc3, 0x92,
			      0x59, 0x48, 0x4e, 0xc0, 0xaf, 0xa8, 0x49, 0x65 };

	const struct flash_area *fa;
	struct flash_area_check fac = { NULL, 0, -1, NULL, 0 };
	uint8_t buffer[16];
	int rc;

	rc = flash_area_open(SLOT1_PARTITION_ID, &fa);
	zassert_true(rc == 0, "flash_area_open() fail, error %d\n", rc);
	rc = flash_area_erase(fa, 0, fa->fa_size);
	zassert_true(rc == 0, "Flash erase failure (%d), error %d\n", rc);
	rc = flash_area_write(fa, 0, tst_vec, sizeof(tst_vec));
	zassert_true(rc == 0, "Flash img write, error %d\n", rc);

	rc = flash_area_check_int_sha256(NULL, NULL);
	zassert_true(rc == -EINVAL, "Flash area check int 256 params 1, 2\n");
	rc = flash_area_check_int_sha256(NULL, &fac);
	zassert_true(rc == -EINVAL, "Flash area check int 256 params 2\n");
	rc = flash_area_check_int_sha256(fa, NULL);
	zassert_true(rc == -EINVAL, "Flash area check int 256 params 1\n");

	rc = flash_area_check_int_sha256(fa, &fac);
	zassert_true(rc == -EINVAL, "Flash area check int 256 fac match\n");
	fac.match = tst_sha;
	rc = flash_area_check_int_sha256(fa, &fac);
	zassert_true(rc == -EINVAL, "Flash area check int 256 fac clen\n");
	fac.clen = sizeof(tst_vec);
	rc = flash_area_check_int_sha256(fa, &fac);
	zassert_true(rc == -EINVAL, "Flash area check int 256 fac off\n");
	fac.off = 0;
	rc = flash_area_check_int_sha256(fa, &fac);
	zassert_true(rc == -EINVAL, "Flash area check int 256 fac rbuf\n");
	fac.rbuf = buffer;
	rc = flash_area_check_int_sha256(fa, &fac);
	zassert_true(rc == -EINVAL, "Flash area check int 256 fac rblen\n");
	fac.rblen = sizeof(buffer);

	rc = flash_area_check_int_sha256(fa, &fac);
	zassert_true(rc == 0, "Flash area check int 256 OK, error %d\n", rc);
	tst_sha[0] = 0x00;
	rc = flash_area_check_int_sha256(fa, &fac);
	zassert_false(rc == 0, "Flash area check int 256 wrong sha\n");

	flash_area_close(fa);
}

ZTEST_SUITE(flash_map_sha, NULL, NULL, NULL, NULL, NULL);
