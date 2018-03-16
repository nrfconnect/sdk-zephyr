/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

/*
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdint.h>
#include <stdio.h>
#include <fs.h>
#include <nffs/nffs.h>
#include "nffs_test_utils.h"
#include <ztest.h>
#include <ztest_assert.h>

void test_mkdir(void)
{
	struct fs_file_t file;
	int rc;

	rc = nffs_format_full(nffs_current_area_descs);
	zassert_equal(rc, 0, "cannot format nffs");

	rc = fs_mkdir(NFFS_MNTP"/a/b/c/d");
	zassert_equal(rc, -ENOENT, "cannot create directory");

	rc = fs_mkdir("asdf");
	zassert_equal(rc, -EINVAL, "cannot create directory");

	rc = fs_mkdir(NFFS_MNTP"/a");
	zassert_equal(rc, 0, "cannot create directory");

	rc = fs_mkdir(NFFS_MNTP"/a/b");
	zassert_equal(rc, 0, "cannot create directory");

	rc = fs_mkdir(NFFS_MNTP"/a/b/c");
	zassert_equal(rc, 0, "cannot create directory");

	rc = fs_mkdir(NFFS_MNTP"/a/b/c/d");
	zassert_equal(rc, 0, "cannot create directory");

	rc = fs_open(&file, NFFS_MNTP"/a/b/c/d/myfile.txt");
	zassert_equal(rc, 0, "cannot open file");

	rc = fs_close(&file);
	zassert_equal(rc, 0, "cannot close file");

	struct nffs_test_file_desc *expected_system =
		(struct nffs_test_file_desc[]) { {
			.filename = "",
			.is_dir = 1,
			.children = (struct nffs_test_file_desc[]) { {
				.filename = "a",
				.is_dir = 1,
				.children = (struct nffs_test_file_desc[]) { {
					.filename = "b",
					.is_dir = 1,
					.children = (struct nffs_test_file_desc[]) { {
						.filename = "c",
						.is_dir = 1,
						.children = (struct nffs_test_file_desc[]) { {
							.filename = "d",
							.is_dir = 1,
							.children = (struct nffs_test_file_desc[]) { {
								.filename = "myfile.txt",
								.contents = NULL,
								.contents_len = 0,
							}, {
								.filename = NULL,
							} },
						}, {
							.filename = NULL,
						} },
					}, {
						.filename = NULL,
					} },
				}, {
					.filename = NULL,
				} },
			}, {
				.filename = NULL,
			} },
		} };

	nffs_test_assert_system(expected_system, nffs_current_area_descs);
}
