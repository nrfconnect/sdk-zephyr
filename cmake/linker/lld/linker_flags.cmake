# Copyright (c) 2022 Google LLC
# SPDX-License-Identifier: Apache-2.0

# Since lld is a drop in replacement for ld, we can just use ld's flags
include(${ZEPHYR_BASE}/cmake/linker/ld/${COMPILER}/linker_flags.cmake OPTIONAL)
