# Bluetooth GATT Battery service

# Copyright (c) 2018 Nordic Semiconductor ASA
# SPDX-License-Identifier: Apache-2.0

menuconfig BT_BAS
	bool "Enable GATT Battery service"
	select SENSOR

if BT_BAS

module = BT_BAS
module-str = BAS
source "${ZEPHYR_BASE}/subsys/logging/Kconfig.template.log_config"

endif # BT_BAS
