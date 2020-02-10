/*
 * Copyright (c) 2019 Peter Bigot Consulting, LLC
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Basic littlefs operations:
 * * create
 * * write
 * * stat
 * * read
 * * seek
 * * tell
 * * truncate
 * * unlink
 * * sync
 */

#include <string.h>
#include <ztest.h>
#include "testfs_tests.h"
#include "testfs_lfs.h"
#include <lfs.h>

#include <fs/littlefs.h>

#define HELLO "hello"
#define GOODBYE "goodbye"

static int mount(struct fs_mount_t *mp)
{
	TC_PRINT("mounting %s\n", mp->mnt_point);

	zassert_equal(fs_mount(mp), 0,
		      "mount failed");

	return TC_PASS;
}

static int clear_partition(struct fs_mount_t *mp)
{
	TC_PRINT("clearing partition %s\n", mp->mnt_point);

	zassert_equal(testfs_lfs_wipe_partition(mp),
		      TC_PASS,
		      "failed to wipe partition");

	return TC_PASS;
}

static int clean_statvfs(const struct fs_mount_t *mp)
{
	struct fs_statvfs stat;

	TC_PRINT("checking clean statvfs of %s\n", mp->mnt_point);

	zassert_equal(fs_statvfs(mp->mnt_point, &stat), 0,
		      "statvfs failed");

	TC_PRINT("%s: bsize %lu ; frsize %lu ; blocks %lu ; bfree %lu\n",
		 mp->mnt_point,
		 stat.f_bsize, stat.f_frsize, stat.f_blocks, stat.f_bfree);
	zassert_equal(stat.f_bsize, 16,
		      "bsize fail");
	zassert_equal(stat.f_frsize, 4096,
		      "frsize fail");
	zassert_equal(stat.f_blocks, 16,
		      "blocks fail");
	zassert_equal(stat.f_bfree, stat.f_blocks - 2U,
		      "bfree fail");

	return TC_PASS;
}

static int create_write_hello(const struct fs_mount_t *mp)
{
	struct testfs_path path;
	struct fs_file_t file;

	TC_PRINT("creating and writing file\n");

	zassert_equal(fs_open(&file,
			      testfs_path_init(&path, mp,
					       HELLO,
					       TESTFS_PATH_END)),
		      0,
		      "open hello failed");

	struct fs_dirent stat;

	zassert_equal(fs_stat(path.path, &stat),
		      0,
		      "stat new hello failed");

	zassert_equal(stat.type, FS_DIR_ENTRY_FILE,
		      "stat new hello not file");
	zassert_equal(strcmp(stat.name, HELLO), 0,
		      "stat new hello not hello");
	zassert_equal(stat.size, 0,
		      "stat new hello not empty");

	zassert_equal(testfs_write_incrementing(&file, 0, TESTFS_BUFFER_SIZE),
		      TESTFS_BUFFER_SIZE,
		      "write constant failed");

	zassert_equal(fs_stat(path.path, &stat),
		      0,
		      "stat written hello failed");

	zassert_equal(stat.type, FS_DIR_ENTRY_FILE,
		      "stat written hello not file");
	zassert_equal(strcmp(stat.name, HELLO), 0,
		      "stat written hello not hello");

	/* Anomalous behavior requiring upstream response */
	if (true) {
		/* VARIATION POINT: littlefs does not update the file size of
		 * an open file.  See upstream issue #250.
		 */
		zassert_equal(stat.size, 0,
			      "stat written hello bad size");
	}

	zassert_equal(fs_close(&file), 0,
		      "close hello failed");

	zassert_equal(fs_stat(path.path, &stat),
		      0,
		      "stat closed hello failed");

	zassert_equal(stat.type, FS_DIR_ENTRY_FILE,
		      "stat closed hello not file");
	zassert_equal(strcmp(stat.name, HELLO), 0,
		      "stat closed hello not hello");
	zassert_equal(stat.size, TESTFS_BUFFER_SIZE,
		      "stat closed hello badsize");

	return TC_PASS;
}

static int verify_hello(const struct fs_mount_t *mp)
{
	struct testfs_path path;
	struct fs_file_t file;

	TC_PRINT("opening and verifying file\n");

	zassert_equal(fs_open(&file,
			      testfs_path_init(&path, mp,
					       HELLO,
					       TESTFS_PATH_END)),
		      0,
		      "verify hello open failed");

	zassert_equal(fs_tell(&file), 0U,
		      "verify hello open tell failed");

	zassert_equal(testfs_verify_incrementing(&file, 0, TESTFS_BUFFER_SIZE),
		      TESTFS_BUFFER_SIZE,
		      "verify hello at start failed");

	zassert_equal(fs_tell(&file), TESTFS_BUFFER_SIZE,
		      "verify hello read tell failed");

	zassert_equal(fs_close(&file), 0,
		      "verify close hello failed");

	return TC_PASS;
}

static int seek_within_hello(const struct fs_mount_t *mp)
{
	struct testfs_path path;
	struct fs_file_t file;

	TC_PRINT("seek and tell in file\n");

	zassert_equal(fs_open(&file,
			      testfs_path_init(&path, mp,
					       HELLO,
					       TESTFS_PATH_END)),
		      0,
		      "verify hello open failed");

	zassert_equal(fs_tell(&file), 0U,
		      "verify hello open tell failed");

	struct fs_dirent stat;

	zassert_equal(fs_stat(path.path, &stat),
		      0,
		      "stat old hello failed");
	zassert_equal(stat.size, TESTFS_BUFFER_SIZE,
		      "stat old hello bad size");

	off_t pos = stat.size / 4;

	zassert_equal(fs_seek(&file, pos, FS_SEEK_SET),
		      0,
		      "verify hello seek near mid failed");

	zassert_equal(fs_tell(&file), pos,
		      "verify hello tell near mid failed");

	zassert_equal(testfs_verify_incrementing(&file, pos, TESTFS_BUFFER_SIZE),
		      TESTFS_BUFFER_SIZE - pos,
		      "verify hello at middle failed");

	zassert_equal(fs_tell(&file), stat.size,
		      "verify hello read middle tell failed");

	zassert_equal(fs_seek(&file, -stat.size, FS_SEEK_CUR),
		      0,
		      "verify hello seek back from cur failed");

	zassert_equal(fs_tell(&file), 0U,
		      "verify hello tell back from cur failed");

	zassert_equal(fs_seek(&file, -pos, FS_SEEK_END),
		      0,
		      "verify hello seek from end failed");

	zassert_equal(fs_tell(&file), stat.size - pos,
		      "verify hello tell from end failed");

	zassert_equal(testfs_verify_incrementing(&file, stat.size - pos,
						 TESTFS_BUFFER_SIZE),
		      pos,
		      "verify hello at post middle failed");

	zassert_equal(fs_close(&file), 0,
		      "verify close hello failed");

	return TC_PASS;
}

static int truncate_hello(const struct fs_mount_t *mp)
{
	struct testfs_path path;
	struct fs_file_t file;

	TC_PRINT("truncate in file\n");

	zassert_equal(fs_open(&file,
			      testfs_path_init(&path, mp,
					       HELLO,
					       TESTFS_PATH_END)),
		      0,
		      "verify hello open failed");

	struct fs_dirent stat;

	zassert_equal(fs_stat(path.path, &stat),
		      0,
		      "stat old hello failed");
	zassert_equal(stat.size, TESTFS_BUFFER_SIZE,
		      "stat old hello bad size");

	off_t pos = 3 * stat.size / 4;

	zassert_equal(fs_tell(&file), 0U,
		      "truncate initial tell failed");

	zassert_equal(fs_truncate(&file, pos),
		      0,
		      "truncate 3/4 failed");

	zassert_equal(fs_tell(&file), 0U,
		      "truncate post tell failed");

	zassert_equal(fs_stat(path.path, &stat),
		      0,
		      "stat open 3/4 failed");

	/* Anomalous behavior requiring upstream response */
	if (true) {
		/* VARIATION POINT: littlefs does not update the file size of
		 * an open file.  See upstream issue #250.
		 */
		zassert_equal(stat.size, TESTFS_BUFFER_SIZE,
			      "stat open 3/4 bad size");
	}

	zassert_equal(testfs_verify_incrementing(&file, 0, 64),
		      48,
		      "post truncate content unexpected");

	zassert_equal(fs_close(&file), 0,
		      "post truncate close failed");

	/* After close size is correct. */
	zassert_equal(fs_stat(path.path, &stat),
		      0,
		      "stat closed truncated failed");
	zassert_equal(stat.size, pos,
		      "stat closed truncated bad size");

	return TC_PASS;
}

static int unlink_hello(const struct fs_mount_t *mp)
{
	struct testfs_path path;

	TC_PRINT("unlink hello\n");

	testfs_path_init(&path, mp,
			 HELLO,
			 TESTFS_PATH_END);

	struct fs_dirent stat;

	zassert_equal(fs_stat(path.path, &stat),
		      0,
		      "stat existing hello failed");
	zassert_equal(fs_unlink(path.path),
		      0,
		      "unlink hello failed");
	zassert_equal(fs_stat(path.path, &stat),
		      -ENOENT,
		      "stat existing hello failed");

	return TC_PASS;
}

static int sync_goodbye(const struct fs_mount_t *mp)
{
	struct testfs_path path;
	struct fs_file_t file;

	TC_PRINT("sync goodbye\n");

	zassert_equal(fs_open(&file,
			      testfs_path_init(&path, mp,
					       GOODBYE,
					       TESTFS_PATH_END)),
		      0,
		      "sync goodbye failed");

	struct fs_dirent stat;

	zassert_equal(fs_stat(path.path, &stat),
		      0,
		      "stat existing hello failed");
	zassert_equal(stat.size, 0,
		      "stat new goodbye not empty");

	zassert_equal(testfs_write_incrementing(&file, 0, TESTFS_BUFFER_SIZE),
		      TESTFS_BUFFER_SIZE,
		      "write goodbye failed");

	zassert_equal(fs_tell(&file), TESTFS_BUFFER_SIZE,
		      "tell goodbye failed");

	if (true) {
		/* Upstream issue #250 */
		zassert_equal(stat.size, 0,
			      "stat new goodbye not empty");
	}

	zassert_equal(fs_sync(&file), 0,
		      "sync goodbye failed");

	zassert_equal(fs_tell(&file), TESTFS_BUFFER_SIZE,
		      "tell synced moved");

	zassert_equal(fs_stat(path.path, &stat),
		      0,
		      "stat existing hello failed");
	printk("sync size %u\n", (u32_t)stat.size);

	zassert_equal(stat.size, TESTFS_BUFFER_SIZE,
		      "stat synced goodbye not correct");

	zassert_equal(fs_close(&file), 0,
		      "post sync close failed");

	/* After close size is correct. */
	zassert_equal(fs_stat(path.path, &stat),
		      0,
		      "stat sync failed");
	zassert_equal(stat.size, TESTFS_BUFFER_SIZE,
		      "stat sync bad size");

	return TC_PASS;
}

static int verify_goodbye(const struct fs_mount_t *mp)
{
	struct testfs_path path;
	struct fs_file_t file;

	TC_PRINT("verify goodbye\n");

	zassert_equal(fs_open(&file,
			      testfs_path_init(&path, mp,
					       GOODBYE,
					       TESTFS_PATH_END)),
		      0,
		      "verify goodbye failed");

	zassert_equal(testfs_verify_incrementing(&file, 0, TESTFS_BUFFER_SIZE),
		      TESTFS_BUFFER_SIZE,
		      "write goodbye failed");

	zassert_equal(fs_close(&file), 0,
		      "post sync close failed");

	return TC_PASS;
}

static int check_medium(void)
{
	struct fs_mount_t *mp = &testfs_medium_mnt;
	struct fs_statvfs stat;

	zassert_equal(clear_partition(mp), TC_PASS,
		      "clear partition failed");

	zassert_equal(fs_mount(mp), 0,
		      "medium mount failed");

	zassert_equal(fs_statvfs(mp->mnt_point, &stat), 0,
		      "statvfs failed");

	TC_PRINT("%s: bsize %lu ; frsize %lu ; blocks %lu ; bfree %lu\n",
		 mp->mnt_point,
		 stat.f_bsize, stat.f_frsize, stat.f_blocks, stat.f_bfree);
	zassert_equal(stat.f_bsize, MEDIUM_IO_SIZE,
		      "bsize fail");
	zassert_equal(stat.f_frsize, 4096,
		      "frsize fail");
	zassert_equal(stat.f_blocks, 240,
		      "blocks fail");
	zassert_equal(stat.f_bfree, stat.f_blocks - 2U,
		      "bfree fail");

	zassert_equal(fs_unmount(mp), 0,
		      "medium unmount failed");

	return TC_PASS;
}

static int check_large(void)
{
	struct fs_mount_t *mp = &testfs_large_mnt;
	struct fs_statvfs stat;

	zassert_equal(clear_partition(mp), TC_PASS,
		      "clear partition failed");

	zassert_equal(fs_mount(mp), 0,
		      "large mount failed");

	zassert_equal(fs_statvfs(mp->mnt_point, &stat), 0,
		      "statvfs failed");

	TC_PRINT("%s: bsize %lu ; frsize %lu ; blocks %lu ; bfree %lu\n",
		 mp->mnt_point,
		 stat.f_bsize, stat.f_frsize, stat.f_blocks, stat.f_bfree);
	zassert_equal(stat.f_bsize, LARGE_IO_SIZE,
		      "bsize fail");
	zassert_equal(stat.f_frsize, 32768,
		      "frsize fail");
	zassert_equal(stat.f_blocks, 96,
		      "blocks fail");
	zassert_equal(stat.f_bfree, stat.f_blocks - 2U,
		      "bfree fail");

	zassert_equal(fs_unmount(mp), 0,
		      "large unmount failed");

	return TC_PASS;
}

void test_lfs_basic(void)
{
	struct fs_mount_t *mp = &testfs_small_mnt;

	zassert_equal(clear_partition(mp), TC_PASS,
		      "clear partition failed");

	zassert_equal(mount(mp), TC_PASS,
		      "clean mount failed");

	zassert_equal(clean_statvfs(mp), TC_PASS,
		      "clean statvfs failed");

	zassert_equal(create_write_hello(mp), TC_PASS,
		      "write hello failed");

	zassert_equal(verify_hello(mp), TC_PASS,
		      "verify hello failed");

	zassert_equal(seek_within_hello(mp), TC_PASS,
		      "seek within hello failed");

	zassert_equal(truncate_hello(mp), TC_PASS,
		      "truncate hello failed");

	zassert_equal(unlink_hello(mp), TC_PASS,
		      "unlink hello failed");

	zassert_equal(sync_goodbye(mp), TC_PASS,
		      "sync goodbye failed");

	TC_PRINT("unmounting %s\n", mp->mnt_point);
	zassert_equal(fs_unmount(mp), 0,
		      "unmount small failed");

	k_sleep(K_MSEC(100));   /* flush log messages */
	TC_PRINT("checking double unmount diagnoses\n");
	zassert_equal(fs_unmount(mp), -EINVAL,
		      "unmount unmounted failed");

	zassert_equal(mount(mp), TC_PASS,
		      "remount failed");

	zassert_equal(verify_goodbye(mp), TC_PASS,
		      "verify goodbye failed");

	zassert_equal(fs_unmount(mp), 0,
		      "unmount2 small failed");

#if !USE_PARTITION_MANAGER
	zassert_equal(check_medium(), TC_PASS,
		      "check medium failed");

	zassert_equal(check_large(), TC_PASS,
		      "check large failed");
#endif

}
