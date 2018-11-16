#!/usr/bin/env bash
# Copyright 2018 Oticon A/S
# SPDX-License-Identifier: Apache-2.0

# Compile all the applications needed by the bsim_bt tests

#set -x #uncomment this line for debugging
set -ue

: "${BSIM_OUT_PATH:?BSIM_OUT_PATH must be defined}"
: "${BSIM_COMPONENTS_PATH:?BSIM_COMPONENTS_PATH must be defined}"
: "${ZEPHYR_BASE:?ZEPHYR_BASE must be set to point to the zephyr root\
 directory}"

WORK_DIR="${WORK_DIR:-${ZEPHYR_BASE}/bsim_bt_out}"
BOARD="${BOARD:-nrf52_bsim}"
BOARD_ROOT="${BOARD_ROOT:-${ZEPHYR_BASE}}"

mkdir -p ${WORK_DIR}

function compile(){
  local APP_ROOT="${APP_ROOT:-${ZEPHYR_BASE}}"
  local CONF_FILE="${CONF_FILE:-prj.conf}"
  local CMAKE_ARGS="${CMAKE_ARGS:-"-DCONFIG_COVERAGE=y"}"
  local NINJA_ARGS="${NINJA_ARGS:-""}"

  local EXE_NAME="${EXE_NAME:-bs_${BOARD}_${APP}_${CONF_FILE}}"
  local EXE_NAME=${EXE_NAME//\//_}
  local EXE_NAME=${EXE_NAME//./_}
  local EXE_NAME=${BSIM_OUT_PATH}/bin/$EXE_NAME
  local MAP_FILE_NAME=${EXE_NAME}.Tsymbols

  echo "Building $EXE_NAME"

  [ -d "${WORK_DIR}/${APP}" ] && rm ${WORK_DIR}/${APP} -rf
  mkdir -p ${WORK_DIR}/${APP} && cd ${WORK_DIR}/${APP}
  cmake -GNinja -DBOARD_ROOT=${BOARD_ROOT} -DBOARD=${BOARD} \
        -DCONF_FILE=${CONF_FILE} ${CMAKE_ARGS} ${APP_ROOT}/${APP} \
      &> cmake.out || { cat cmake.out && return 0; }
  ninja ${NINJA_ARGS} &> ninja.out || { cat ninja.out && return 0; }
  cp ${WORK_DIR}/${APP}/zephyr/zephyr.exe ${EXE_NAME}

  nm ${EXE_NAME} | grep -v " [U|w] " | sort | cut -d" " -f1,3 > ${MAP_FILE_NAME}
  sed -i "1i $(wc -l ${MAP_FILE_NAME} | cut -d" " -f1)" ${MAP_FILE_NAME}
}

APP=samples/bluetooth/peripheral compile
APP=tests/bluetooth/bsim_bt/bsim_test_app compile
