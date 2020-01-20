# SPDX-License-Identifier: Apache-2.0
board_runner_args(pyocd "--target=stm32g071rb")
board_runner_args(pyocd "--flash-opt=-O reset_type=hw")
board_runner_args(pyocd "--flash-opt=-O connect_mode=under-reset")

include(${ZEPHYR_BASE}/boards/common/pyocd.board.cmake)
