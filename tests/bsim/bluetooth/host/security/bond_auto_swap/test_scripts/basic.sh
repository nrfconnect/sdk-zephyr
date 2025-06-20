#!/usr/bin/env bash
# Copyright 2025 Nordic Semiconductor ASA
# SPDX-License-Identifier: Apache-2.0

# Verify that re-connection with 2 bonded devices works using undirected advertisements where
# central has the same identity address.
#
# Test procedure:
# 1. Peripheral creates 2 identities.
# 2. Peripheral starts advertising with the first identity.
# 3. Central connects to the peripheral and bonds.
# 4. Central disconnects.
# 5. Peripheral starts advertising with the second identity.
# 6. Central connects to the peripheral and bonds.
# 7. Central disconnects.
# 8. Peripheral starts advertising again with the first identity.
# 9. Central connects to the peripheral and encripts the link.
# 10. Central disconnects.
# 11. Peripheral starts advertising with the second identity.
# 12. Central connects to the peripheral and encripts the link.
# 13. Central disconnects.
# 14. Repeat steps 8-13.

set -eu

source ${ZEPHYR_BASE}/tests/bsim/sh_common.source

simulation_id="security_bond_keep_basic"
verbosity_level=2
central_exe="${BSIM_OUT_PATH}/bin/bs_${BOARD_TS}_$(guess_test_long_name)_central_prj_conf"
peripheral_exe="${BSIM_OUT_PATH}/bin/bs_${BOARD_TS}_$(guess_test_long_name)_peripheral_prj_conf"

cd ${BSIM_OUT_PATH}/bin

Execute "$central_exe" \
    -v=${verbosity_level} -s=${simulation_id} -d=0 -testid=central_basic -RealEncryption=1

Execute "$peripheral_exe" \
    -v=${verbosity_level} -s=${simulation_id} -d=1 -testid=peripheral_basic -RealEncryption=1

Execute ./bs_2G4_phy_v1 -v=${verbosity_level} -s=${simulation_id} \
    -D=2 -sim_length=60e6 $@

wait_for_background_jobs
