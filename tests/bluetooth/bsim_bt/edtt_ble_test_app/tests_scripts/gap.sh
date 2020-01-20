#!/usr/bin/env bash
# Copyright 2019 Oticon A/S
# SPDX-License-Identifier: Apache-2.0

# GAP regression tests based on the EDTTool
CWD=`pwd`"/"`dirname $0`

export SIMULATION_ID="edtt_gap"
export TEST_FILE=${CWD}"/gap.test_list"
export TEST_MODULE="gap_verification"

${CWD}/_controller_tests_inner.sh
