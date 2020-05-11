#SPDX-License-Identifier: Apache-2.0

set(TFM_TARGET_PLATFORM "MUSCA_B1")

board_set_debugger_ifnset(pyocd)
board_set_flasher_ifnset(pyocd)

board_runner_args(pyocd "--target=musca_b1")

include(${ZEPHYR_BASE}/boards/common/pyocd.board.cmake)
