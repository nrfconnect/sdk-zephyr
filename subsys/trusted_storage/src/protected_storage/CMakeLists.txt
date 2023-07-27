# SPDX-License-Identifier: Apache-2.0

zephyr_sources_ifdef(CONFIG_PROTECTED_STORAGE
	backend_interface.c
)
