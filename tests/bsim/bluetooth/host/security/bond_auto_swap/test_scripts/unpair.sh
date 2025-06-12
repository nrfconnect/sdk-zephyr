#!/usr/bin/env bash
# Copyright 2025 Nordic Semiconductor ASA
# SPDX-License-Identifier: Apache-2.0

# Verify that bt_unpair works when Peripheral was bonded with 2 Centrals with the same identity
# address.
#
# Also, verify that BT_KEYS_ID_CONFLICT stays set for each identity until conflict is resolved by
# unpairing the last conflicting Central.
#
# Test procedure:
# 1. Peripheral creates 3 identities.
# 2. Peripheral starts advertising with the first identity.
# 3. Central connects to the peripheral and bonds.
# 4. Central disconnects.
# 5. Peripheral starts advertising with the second identity.
# 6. Central connects to the peripheral and bonds.
# 7. Central disconnects.
# 8. Peripheral starts advertising with the thirs identity.
# 9. Central connects to the peripheral and bonds.
# 10. Central disconnects.
# 11. Peripheral starts advertising with the first identity.
# 12. Central connects to the peripheral and encripts the link.
# 13. Peripheral initiates unpairing with the Central and disconnects from Central.
# 14. Peripheral checks that BT_KEYS_ID_CONFLICT flag is set for the remaining identities.
# 15. Peripheral starts advertising with the second identity.
# 16. Central connects to the peripheral and encripts the link.
# 17. Peripheral initiates unpairing with the Central and disconnects from Central.
# 18. Peripheral checks that BT_KEYS_ID_CONFLICT flag is now unset for the last identity.
# 19. Peripheral starts advertising with the third identity.
# 20. Central connects to the peripheral and encripts the link.
# 21. Peripheral initiates unpairing with the Central and disconnects from Central.

set -eu

source ${ZEPHYR_BASE}/tests/bsim/sh_common.source

simulation_id="security_bond_keep_unpair"
verbosity_level=2
central_exe="${BSIM_OUT_PATH}/bin/bs_${BOARD_TS}_$(guess_test_long_name)_central_prj_conf"
peripheral_exe="${BSIM_OUT_PATH}/bin/bs_${BOARD_TS}_$(guess_test_long_name)_peripheral_prj_conf"

cd ${BSIM_OUT_PATH}/bin

Execute "$central_exe" \
    -v=${verbosity_level} -s=${simulation_id} -d=0 -testid=central_unpair -RealEncryption=1

Execute "$peripheral_exe" \
    -v=${verbosity_level} -s=${simulation_id} -d=1 -testid=peripheral_unpair -RealEncryption=1

Execute ./bs_2G4_phy_v1 -v=${verbosity_level} -s=${simulation_id} \
    -D=2 -sim_length=60e6 $@

wait_for_background_jobs
