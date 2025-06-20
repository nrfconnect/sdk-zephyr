#!/usr/bin/env bash
# Copyright 2025 Nordic Semiconductor ASA
# SPDX-License-Identifier: Apache-2.0

# Verify that Host rejects multiple advertisements from Peripheral to the same Central.
#
# Test procedure:
# 1. Peripheral creates 2 identities.
# 2. Peripheral starts advertising with the first identity.
# 3. Central connects to the peripheral and bonds.
# 4. Central disconnects.
# 5. Peripheral starts advertising with the second identity.
# 6. Central connects to the peripheral and bonds.
# 7. Central disconnects.
# 8. Peripheral starts advertising with the first identity, while Central doesn't initiate
# connection.
# 9. Peripheral starts advertising with the second identity.
# 10. Peripheral verifies that Host returns -EPERM error for the second advertisement.
# 11. Peripheral waits for timeout on the first advertisement.
# 12. Peripheral starts advertising with the second identity again.
# 13. Central connects to the peripheral and encripts the link.
# 14. Central disconnects.

set -eu

source ${ZEPHYR_BASE}/tests/bsim/sh_common.source

simulation_id="security_bond_keep_ext_mult_adv_reject"
verbosity_level=2
central_exe="${BSIM_OUT_PATH}/bin/bs_${BOARD_TS}_$(guess_test_long_name)_central_prj_conf"
peripheral_exe="${BSIM_OUT_PATH}/bin/bs_${BOARD_TS}_$(guess_test_long_name)_peripheral_prj_conf"

cd ${BSIM_OUT_PATH}/bin

Execute "$central_exe" \
    -v=${verbosity_level} -s=${simulation_id} -d=0 -testid=central_mult_adv_reject -RealEncryption=1

Execute "$peripheral_exe" \
    -v=${verbosity_level} -s=${simulation_id} -d=1 -testid=peripheral_mult_adv_reject \
    -RealEncryption=1

Execute ./bs_2G4_phy_v1 -v=${verbosity_level} -s=${simulation_id} \
    -D=2 -sim_length=60e6 $@

wait_for_background_jobs
