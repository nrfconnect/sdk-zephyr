# SPDX-License-Identifier: Apache-2.0

board_runner_args(openocd --cmd-pre-load "targets 1")
include(${ZEPHYR_BASE}/boards/common/openocd.board.cmake)
