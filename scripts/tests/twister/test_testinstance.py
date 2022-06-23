#!/usr/bin/env python3
# Copyright (c) 2020 Intel Corporation
#
# SPDX-License-Identifier: Apache-2.0
# pylint: disable=line-too-long
"""
Tests for testinstance class
"""

import os
import sys
import pytest

ZEPHYR_BASE = os.getenv("ZEPHYR_BASE")
sys.path.insert(0, os.path.join(ZEPHYR_BASE, "scripts/pylib/twister"))
from twister.testinstance import TestInstance
from twister.error import BuildError, TwisterException
from twister.testsuite import TestSuite


TESTDATA_1 = [
    (False, False, "console", "na", "qemu", False, [], (False, True)),
    (False, False, "console", "native", "qemu", False, [], (False, True)),
    (True, False, "console", "native", "nsim", False, [], (True, False)),
    (True, True, "console", "native", "renode", False, [], (True, False)),
    (False, False, "sensor", "native", "", False, [], (True, False)),
    (False, False, "sensor", "na", "", False, [], (True, False)),
    (False, True, "sensor", "native", "", True, [], (True, False)),
]
@pytest.mark.parametrize("build_only, slow, harness, platform_type, platform_sim, device_testing,fixture, expected", TESTDATA_1)
def test_check_build_or_run(class_testplan, monkeypatch, all_testsuites_dict, platforms_list, build_only, slow, harness, platform_type, platform_sim, device_testing, fixture, expected):
    """" Test to check the conditions for build_only and run scenarios
    Scenario 1: Test when different parameters are passed, build_only and run are set correctly
    Scenario 2: Test if build_only is enabled when the OS is Windows"""

    class_testplan.testsuites = all_testsuites_dict
    testsuite = class_testplan.testsuites.get('scripts/tests/twister/test_data/testsuites/tests/test_a/test_a.check_1')


    class_testplan.platforms = platforms_list
    platform = class_testplan.get_platform("demo_board_2")
    platform.type = platform_type
    platform.simulation = platform_sim
    testsuite.harness = harness
    testsuite.build_only = build_only
    testsuite.slow = slow

    testinstance = TestInstance(testsuite, platform, class_testplan.env.outdir)
    run = testinstance.check_runnable(slow, device_testing, fixture)
    _, r = expected
    assert run == r

    monkeypatch.setattr("os.name", "nt")
    run = testinstance.check_runnable()
    assert not run

TESTDATA_2 = [
    (True, True, True, ["demo_board_2"], "native", '\nCONFIG_COVERAGE=y\nCONFIG_COVERAGE_DUMP=y\nCONFIG_ASAN=y\nCONFIG_UBSAN=y'),
    (True, False, True, ["demo_board_2"], "native", '\nCONFIG_COVERAGE=y\nCONFIG_COVERAGE_DUMP=y\nCONFIG_ASAN=y'),
    (False, False, True, ["demo_board_2"], 'native', '\nCONFIG_COVERAGE=y\nCONFIG_COVERAGE_DUMP=y'),
    (True, False, True, ["demo_board_2"], 'mcu', '\nCONFIG_COVERAGE=y\nCONFIG_COVERAGE_DUMP=y'),
    (False, False, False, ["demo_board_2"], 'native', ''),
    (False, False, True, ['demo_board_1'], 'native', ''),
    (True, False, False, ["demo_board_2"], 'native', '\nCONFIG_ASAN=y'),
    (False, True, False, ["demo_board_2"], 'native', '\nCONFIG_UBSAN=y'),
]

@pytest.mark.parametrize("enable_asan, enable_ubsan, enable_coverage, coverage_platform, platform_type, expected_content", TESTDATA_2)
def test_create_overlay(class_testplan, all_testsuites_dict, platforms_list, enable_asan, enable_ubsan, enable_coverage, coverage_platform, platform_type, expected_content):
    """Test correct content is written to testcase_extra.conf based on if conditions
    TO DO: Add extra_configs to the input list"""
    class_testplan.testsuites = all_testsuites_dict
    testcase = class_testplan.testsuites.get('scripts/tests/twister/test_data/testsuites/samples/test_app/sample_test.app')
    class_testplan.platforms = platforms_list
    platform = class_testplan.get_platform("demo_board_2")

    testinstance = TestInstance(testcase, platform, class_testplan.env.outdir)
    platform.type = platform_type
    assert testinstance.create_overlay(platform, enable_asan, enable_ubsan, enable_coverage, coverage_platform) == expected_content

def test_calculate_sizes(class_testplan, all_testsuites_dict, platforms_list):
    """ Test Calculate sizes method for zephyr elf"""
    class_testplan.testsuites = all_testsuites_dict
    testcase = class_testplan.testsuites.get('scripts/tests/twister/test_data/testsuites/samples/test_app/sample_test.app')
    class_testplan.platforms = platforms_list
    platform = class_testplan.get_platform("demo_board_2")
    testinstance = TestInstance(testcase, platform, class_testplan.env.outdir)

    with pytest.raises(BuildError):
        assert testinstance.calculate_sizes() == "Missing/multiple output ELF binary"

TESTDATA_3 = [
    (ZEPHYR_BASE + '/scripts/tests/twister/test_data/testsuites', ZEPHYR_BASE, '/scripts/tests/twister/test_data/testsuites/tests/test_a/test_a.check_1', '/scripts/tests/twister/test_data/testsuites/tests/test_a/test_a.check_1'),
    (ZEPHYR_BASE, '.', 'test_a.check_1', 'test_a.check_1'),
    (ZEPHYR_BASE, '/scripts/tests/twister/test_data/testsuites/test_b', 'test_b.check_1', '/scripts/tests/twister/test_data/testsuites/test_b/test_b.check_1'),
    (os.path.join(ZEPHYR_BASE, '/scripts/tests'), '.', 'test_b.check_1', 'test_b.check_1'),
    (os.path.join(ZEPHYR_BASE, '/scripts/tests'), '.', '.', '.'),
    (ZEPHYR_BASE, '.', 'test_a.check_1.check_2', 'test_a.check_1.check_2'),
]
@pytest.mark.parametrize("testcase_root, workdir, name, expected", TESTDATA_3)
def test_get_unique(testcase_root, workdir, name, expected):
    '''Test to check if the unique name is given for each testcase root and workdir'''
    unique = TestSuite(testcase_root, workdir, name)
    assert unique.name == expected

TESTDATA_4 = [
    (ZEPHYR_BASE, '.', 'test_c', 'Tests should reference the category and subsystem with a dot as a separator.'),
    (os.path.join(ZEPHYR_BASE, '/scripts/tests'), '.', '', 'Tests should reference the category and subsystem with a dot as a separator.'),
]
@pytest.mark.parametrize("testcase_root, workdir, name, exception", TESTDATA_4)
def test_get_unique_exception(testcase_root, workdir, name, exception):
    '''Test to check if tests reference the category and subsystem with a dot as a separator'''

    with pytest.raises(TwisterException):
        unique = TestSuite(testcase_root, workdir, name)
        assert unique == exception
