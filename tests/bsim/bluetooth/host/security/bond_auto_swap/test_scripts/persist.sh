#!/usr/bin/env bash
# Copyright 2025 Nordic Semiconductor ASA
# SPDX-License-Identifier: Apache-2.0

# Verify that BT_KEYS_ID_CONFLICT flag is recovered after a reboot.
#
# Test procedure:
# First test:
# 1. Peripheral creates 2 identities.
# 2. Peripheral bonds with Central alternately using each identity.
#
# Second test:
# 1. Pepherial verifies that the bond data is correctly restored from flash.
# 2. Peripheral alternately re-connects to Central using each identity.

set -eu

source ${ZEPHYR_BASE}/tests/bsim/sh_common.source

simulation_id="security_bond_keep_persist"
verbosity_level=2
central_exe="${BSIM_OUT_PATH}/bin/bs_${BOARD_TS}_$(guess_test_long_name)_persist_central_prj_conf"
# Split into 2 lines to avoid exceeding line length limit
peripheral_exe="bs_${BOARD_TS}_$(guess_test_long_name)_persist_peripheral_prj_conf"
peripheral_exe="${BSIM_OUT_PATH}/bin/${peripheral_exe}"
central_flash=../results/${simulation_id}/${simulation_id}_central.bin
peripheral_flash=../results/${simulation_id}/${simulation_id}_peripheral.bin

cd ${BSIM_OUT_PATH}/bin

# Run the first test to save the bond data
Execute "$central_exe" \
    -v=${verbosity_level} -s=${simulation_id} -d=0 -testid=central_persist_bond -RealEncryption=1 \
    -flash_erase -flash_file=${central_flash}
Execute "$peripheral_exe" \
    -v=${verbosity_level} -s=${simulation_id} -d=1 -testid=peripheral_persist_bond \
    -RealEncryption=1 -flash_erase -flash_file=${peripheral_flash}
Execute ./bs_2G4_phy_v1 -v=${verbosity_level} -s=${simulation_id} -D=2 -sim_length=60e6 $@
wait_for_background_jobs

# Run the second test to verify the bond data is still present after a reset and bond data is
# correctly restored
Execute "$central_exe" \
    -v=${verbosity_level} -s=${simulation_id} -d=0 -testid=central_persist_reconnect \
    -RealEncryption=1 -flash_file=${central_flash}
Execute "$peripheral_exe" \
    -v=${verbosity_level} -s=${simulation_id} -d=1 -testid=peripheral_persist_reconnect \
    -RealEncryption=1 -flash_file=${peripheral_flash}
Execute ./bs_2G4_phy_v1 -v=${verbosity_level} -s=${simulation_id} -D=2 -sim_length=60e6 $@

wait_for_background_jobs
