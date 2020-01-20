#!/usr/bin/env bash
# Copyright 2019 Oticon A/S
# SPDX-License-Identifier: Apache-2.0

# LL regression tests based on the EDTTool (part 1)

CWD=`pwd`"/"`dirname $0`

export SIMULATION_ID="edtt_ll_set1"
export TEST_FILE=${CWD}"/ll.set1.test_list"
export TEST_MODULE="ll_verification"

${CWD}/_controller_tests_inner.sh
