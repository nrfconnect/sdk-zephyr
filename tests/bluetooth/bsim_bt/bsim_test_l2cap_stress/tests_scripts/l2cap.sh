#!/usr/bin/env bash
# Copyright (c) 2022 Nordic Semiconductor
# SPDX-License-Identifier: Apache-2.0

# EATT test
simulation_id="l2cap_stress"
verbosity_level=2
process_ids=""; exit_code=0

function Execute(){
  if [ ! -f $1 ]; then
    echo -e "  \e[91m`pwd`/`basename $1` cannot be found (did you forget to\
 compile it?)\e[39m"
    exit 1
  fi
  # timeout 30 $@ & process_ids="$process_ids $!"
  $@ & process_ids="$process_ids $!"
}

: "${BSIM_OUT_PATH:?BSIM_OUT_PATH must be defined}"

#Give a default value to BOARD if it does not have one yet:
BOARD="${BOARD:-nrf52_bsim}"

cd ${BSIM_OUT_PATH}/bin

bsim_exe=./bs_${BOARD}_tests_bluetooth_bsim_bt_bsim_test_l2cap_stress_prj_conf

Execute "${bsim_exe}" -v=${verbosity_level} -s=${simulation_id} -d=0 -testid=central -rs=43

Execute "${bsim_exe}" -v=${verbosity_level} -s=${simulation_id} -d=1 -testid=peripheral -rs=42
Execute "${bsim_exe}" -v=${verbosity_level} -s=${simulation_id} -d=2 -testid=peripheral -rs=10
Execute "${bsim_exe}" -v=${verbosity_level} -s=${simulation_id} -d=3 -testid=peripheral -rs=23
Execute "${bsim_exe}" -v=${verbosity_level} -s=${simulation_id} -d=4 -testid=peripheral -rs=7884
Execute "${bsim_exe}" -v=${verbosity_level} -s=${simulation_id} -d=5 -testid=peripheral -rs=230
Execute "${bsim_exe}" -v=${verbosity_level} -s=${simulation_id} -d=6 -testid=peripheral -rs=9

Execute ./bs_2G4_phy_v1 -v=${verbosity_level} -s=${simulation_id} -D=7 -sim_length=270e6 $@

for process_id in $process_ids; do
  wait $process_id || let "exit_code=$?"
done
exit $exit_code #the last exit code != 0
