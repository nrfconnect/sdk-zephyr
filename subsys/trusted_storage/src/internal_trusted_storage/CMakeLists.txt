# SPDX-License-Identifier: Apache-2.0

zephyr_sources_ifdef(CONFIG_INTERNAL_TRUSTED_STORAGE
		backend_interface.c
)
zephyr_sources_ifdef(CONFIG_INTERNAL_TRUSTED_STORAGE_SETTINGS
		settings_backend.c
)
zephyr_sources_ifdef(CONFIG_INTERNAL_TRUSTED_STORAGE_SETTINGS_TRUST_CHACHAPOLY
		settings_trust.c
		settings_trust_chachapoly.c
)
