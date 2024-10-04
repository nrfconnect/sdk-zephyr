/* main.c - Application main entry point */

/*
 * Copyright (c) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <stddef.h>
#include <zephyr/ztest.h>

#include <zephyr/settings/settings.h>

#include "ll_settings.h"

ZTEST_SUITE(test_ll_settings, NULL, NULL, NULL, NULL, NULL);

ZTEST(test_ll_settings, test_company_id)
{
	uint16_t cid;
	int err;

	cid = 0x1234;
	err = settings_runtime_set("bt/ctlr/company", &cid, sizeof(cid));
	zassert_equal(err, 0, "Setting Company Id failed");
	zassert_equal(ll_settings_company_id(), cid,
		      "Company Id does not match");

	cid = 0x5678;
	err = settings_runtime_set("bt/ctlr/company", &cid, sizeof(cid));
	zassert_equal(err, 0, "Changing Company Id failed");
	zassert_equal(ll_settings_company_id(), cid,
		      "Company ID does not match");
}

ZTEST(test_ll_settings, test_subversion_number)
{
	uint16_t svn;
	int err;

	svn = 0x1234;
	err = settings_runtime_set("bt/ctlr/subver", &svn, sizeof(svn));
	zassert_equal(err, 0, "Setting Subversion number failed");
	zassert_equal(ll_settings_subversion_number(), svn,
		      "Subversion number does not match");

	svn = 0x5678;
	err = settings_runtime_set("bt/ctlr/subver", &svn, sizeof(svn));
	zassert_equal(err, 0, "Changing Subversion number failed");
	zassert_equal(ll_settings_subversion_number(), svn,
		      "Subversion number does not match");
}
