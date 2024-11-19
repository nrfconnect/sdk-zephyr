#
# Copyright (c) 2024 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(remote_icmsg)

zephyr_include_directories(../common)

FILE(GLOB remote_sources src/*.c)
target_sources(app PRIVATE ${remote_sources})
