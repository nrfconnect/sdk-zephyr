:orphan:

.. _zephyr_3.3:

Zephyr 3.3.0 (Working Draft)
############################

We are pleased to announce the release of Zephyr version 3.3.0.

Major enhancements with this release include:

The following sections provide detailed lists of changes by component.

Security Vulnerability Related
******************************

API Changes
***********

Changes in this release
=======================

* Bluetooth: :kconfig:option:`CONFIG_BT_PER_ADV_SYNC_TRANSFER_RECEIVER`
  and :kconfig:option:`CONFIG_BT_PER_ADV_SYNC_TRANSFER_SENDER` have been
  added to enable the PAST implementation rather than
  :kconfig:option:`CONFIG_BT_CONN`.
* Flashdisk: :kconfig:option:`CONFIG_DISK_FLASH_VOLUME_NAME`,
  :kconfig:option:`CONFIG_DISK_FLASH_DEV_NAME`,
  :kconfig:option:`CONFIG_DISK_FLASH_START`,
  :kconfig:option:`CONFIG_DISK_FLASH_MAX_RW_SIZE`,
  :kconfig:option:`CONFIG_DISK_ERASE_BLOCK_SIZE`,
  :kconfig:option:`CONFIG_DISK_FLASH_ERASE_ALIGNMENT`,
  :kconfig:option:`CONFIG_DISK_VOLUME_SIZE` and
  :kconfig:option:`CONFIG_DISK_FLASH_SECTOR_SIZE` Kconfig options have been
  removed in favor of new :dtcompatible:`zephyr,flash-disk` devicetree binding.

* Regulator APIs previously located in ``<zephyr/drivers/regulator/consumer.h>``
  are now part of ``<zerphyr/drivers/regulator.h>``.

* Starting from this release ``zephyr-`` prefixed tags won't be created
  anymore. The project will continue using ``v`` tags, for example ``v3.3.0``.

* Bluetooth: Deprecate the Bluetooth logging subsystem in favor of the Zephyr
  standard logging system. To enable debugging for a particular module in the
  Bluetooth subsystem, enable `CONFIG_BT_(module name)_LOG_LEVEL_DBG` instead of
  `CONFIG_BT_DEBUG_(module name)`.

Removed APIs in this release
============================

* Removed :kconfig:option:`CONFIG_COUNTER_RTC_STM32_LSE_DRIVE*`
  This should now be configured using the ``driving_capability`` property of
  LSE clock

* Removed :kconfig:option:`CONFIG_COUNTER_RTC_STM32_LSE_BYPASS`
  This should now be configured using the new ``lse_bypass`` property of
  LSE clock

* Removed :kconfig:option:`CONFIG_COUNTER_RTC_STM32_BACKUP_DOMAIN_RESET`

Deprecated in this release
==========================

* MCUmgr subsystem, specifically the SMP transport API, is dropping `zephyr_`
  prefix, deprecating prefixed functions and callback type definitions with the
  prefix and replacing them with with prefix-less variants.
  The :c:struct:`zephyr_smp_transport` type, representing transport object,
  is now replaced with :c:struct:`smp_transport`, and the later one is used,
  instead of the former one, by all prefix-less functions.

  Deprecated functions and their replacements:

  .. table::
     :align: center

     +-------------------------------------+---------------------------------------+
     | Deprecated                          | Drop in replacement                   |
     +=====================================+=======================================+
     | :c:func:`zephyr_smp_transport_init` | :c:func:`smp_transport_init`          |
     +-------------------------------------+---------------------------------------+
     | :c:func:`zephyr_smp_rx_req`         | :c:func:`smp_rx_req`                  |
     +-------------------------------------+---------------------------------------+
     | :c:func:`zephyr_smp_alloc_rsp`      | :c:func:`smp_alloc_rsp`               |
     +-------------------------------------+---------------------------------------+
     | :c:func:`zephyr_smp_free_buf`       | :c:func:`smp_free_buf`                |
     +-------------------------------------+---------------------------------------+

  Deprecated callback types and their replacements:

  .. table::
     :align: center

     +---------------------------------------------+---------------------------------------+
     | Deprecated                                  | Drop in replacement                   |
     +=============================================+=======================================+
     | :c:func:`zephyr_smp_transport_out_fn`       | :c:func:`smp_transport_out_fn`        |
     +---------------------------------------------+---------------------------------------+
     | :c:func:`zephyr_smp_transport_get_mtu_fn`   | :c:func:`smp_transport_get_mtu_fn`    |
     +---------------------------------------------+---------------------------------------+
     | :c:func:`zephyr_smp_transport_ud_copy_fn`   | :c:func:`smp_transport_ud_copy_fn`    |
     +---------------------------------------------+---------------------------------------+
     | :c:func:`zephyr_smp_transport_ud_free_fn`   | :c:func:`smp_transport_ud_free_fn`    |
     +---------------------------------------------+---------------------------------------+

  NOTE: Only functions are marked as ``__deprecated``, type definitions are not.

* STM32 RTC source clock should now be configured using devicetree.
  Related Kconfig :kconfig:option:`CONFIG_COUNTER_RTC_STM32_CLOCK_LSI` and
  :kconfig:option:`CONFIG_COUNTER_RTC_STM32_CLOCK_LSE` options are now
  deprecated.

* File backend for settings APIs and Kconfig options were deprecated:

  :c:func:`settings_mount_fs_backend` in favor of :c:func:`settings_mount_file_backend`

  :kconfig:option:`CONFIG_SETTINGS_FS` in favor of :kconfig:option:`CONFIG_SETTINGS_FILE`

  :kconfig:option:`CONFIG_SETTINGS_FS_DIR` in favor of :kconfig:option:`CONFIG_SETTINGS_FILE_DIR`

  :kconfig:option:`CONFIG_SETTINGS_FS_FILE` in favor of :kconfig:option:`CONFIG_SETTINGS_FILE_PATH`

  :kconfig:option:`CONFIG_SETTINGS_FS_MAX_LINES` in favor of :kconfig:option:`CONFIG_SETTINGS_FILE_MAX_LINES`

* PCIe APIs :c:func:`pcie_probe` and :c:func:`pcie_bdf_lookup` have been
  deprecated in favor of a centralized scan of available PCIe devices.

Stable API changes in this release
==================================

* MCUmgr events have been reworked to use a single, unified callback system.
  This allows better customisation of the callbacks with a lower flash size.
  Applications using the existing callback system will need to be upgraded to
  use the new API by following the :ref:`migration guide <mcumgr_cb_migration>`

* :c:func:`net_pkt_get_frag`, :c:func:`net_pkt_get_reserve_tx_data` and
  :c:func:`net_pkt_get_reserve_rx_data` functions are now requiring to specify
  the minimum fragment length to allocate, so that they work correctly also in
  case :kconfig:option:`CONFIG_NET_BUF_VARIABLE_DATA_SIZE` is enabled.
  Applications using this APIs will need to be updated to provide the expected
  fragment length.

New APIs in this release
========================

Kernel
******

Architectures
*************

* ARM

* ARM

* ARM64

* RISC-V

* Xtensa

Bluetooth
*********

* Audio

* Direction Finding

* Host

  * Fixed missing calls to bt_le_per_adv_sync_cb.term when deleting a periodic
    advertising sync object.

  * Added local advertising address to bt_le_ext_adv_info.

* Mesh

  * Change default advertiser to be extended advertiser.

* Controller

* HCI Driver

Boards & SoC Support
********************

* Added support for these SoC series:

* Removed support for these SoC series:

* Made these changes in other SoC series:

* Changes for ARC boards:

* Added support for these ARM boards:

* Added support for these ARM64 boards:

* Removed support for these ARM boards:

* Removed support for these X86 boards:

* Added support for these RISC-V boards:

* Added support for these Xtensa boards:

* Removed support for these Xtensa boards:

* Made these changes in other boards:

* Added support for these following shields:

Build system and infrastructure
*******************************

Drivers and Sensors
*******************

* ADC

* CAN

* Clock control

* Counter

  * STM32 RTC based counter should now be configured using device tree.

* Crypto

* DAC

* DFU

* Disk

* Display

* DMA

* EEPROM

* Entropy

* ESPI

* Ethernet

* Flash

  * NRF: Added CONFIG_SOC_FLASH_NRF_TIMEOUT_MULTIPLIER to allow tweaking the timeout of flash operations.

  * spi_nor: Added property mxicy,mx25r-power-mode to jedec,spi-nor binding for controlling low power/high performance mode on Macronix MX25R* Ultra Low Power flash devices.

* GPIO

* I2C

* I2S

* I3C

* IEEE 802.15.4

* Interrupt Controller

* IPM

* KSCAN

* LED

* MBOX

* MEMC

* PCIE

* PECI

* Pin control

  * Common pin control properties are now defined at root level in a single
    file: :zephyr_file:`dts/bindings/pinctrl/pincfg-node.yaml`. Pin control
    bindings are expected to include it at the level they need. For example,
    drivers using the grouping representation approach need to include it at
    grandchild level, while drivers using the node approach need to include it
    at the child level. This change will only impact out-of-tree pin control
    drivers, sinc all in-tree drivers have been updated.

* PWM

* Power domain

* Reset

* SDHC

* Sensor

* Serial

* SPI

* Timer

* USB

* W1

* Watchdog

* WiFi

Networking
**********

IPv4 packet fragmentation support has been added, this allows large packets to
be split up before sending or reassembled during receive for packets that are
larger than the network device MTU. This is disabled by default but can be
enabled with :kconfig:option:`CONFIG_NET_IPV4_FRAGMENT`.

USB
***

Devicetree
**********

* Bindings

  * New:

    * :dtcompatible:`zephyr,flash-disk`

    * STM32 SoCs:

      * :dtcompatible: `st,stm32-lse-clock`: new ``lse-bypass`` property

Libraries / Subsystems
**********************

* Management

  * MCUmgr functionality deprecated in 3.1 has been removed:
    CONFIG_FS_MGMT_UL_CHUNK_SIZE, CONFIG_IMG_MGMT_UL_CHUNK_SIZE,
    CONFIG_OS_MGMT_ECHO_LENGTH
  * MCUmgr fs_mgmt issue with erasing a file prior to writing the first block
    of data has been worked around by only truncating/deleting the file data
    if the file exists. This can help work around an issue whereby logging is
    enabled and the command is sent on the same UART as the logging system, in
    which a filesystem error was emitted.
  * A MCUmgr bug when using the smp_svr sample with Bluetooth transport that
    could have caused a stack overflow has been fixed.
  * A MCUmgr issue with Bluetooth transport that could cause a deadlock of the
    mcumgr thread if the remote device disconnected before the output message
    was sent has been fixed.
  * A MCUmgr img_mgmt bug whereby the state of an image upload could persist
    when it was no longer valid (e.g. after an image erase command) has been
    fixed.
  * MCUmgr fs_mgmt command has been added that allows querying/listing the
    supported hash/checksum types.
  * MCUmgr Bluetooth transport will now clear unprocessed commands sent if a
    remote device disconnects instead of processing them.
  * A new MCUmgr transport function pointer has been added which needs
    registering in ``smp_transport_init`` for removing invalid packets for
    connection-orientated transports. If this is unimplemented, the function
    pointer can be set to NULL.
  * MCUmgr command handler definitions have changed, the ``mgmt_ctxt`` struct
    has been replaced with the ``smp_streamer`` struct, the zcbor objects need
    to replace ``cnbe`` object access with ``writer`` and ``cnbd`` object
    access with ``reader`` to successfully build.
  * MCUmgr callback system has been reworked with a unified singular interface
    which supports status passing to the handler (:ref:`mcumgr_callbacks`).
  * MCUmgr subsystem directory structure has been flattened and contents of the
    lib subdirectory has been redistributed into following directories:

    .. table::
       :align: center

       +----------------+-------------------------------------------+
       | Subdirectory   | MCUmgr area                               |
       +================+===========================================+
       | mgmt           | MCUmgr management functions, group        |
       |                | registration, and so on;                  |
       +----------------+-------------------------------------------+
       | smp            | Simple Management Protocol processing;    |
       +----------------+-------------------------------------------+
       | transport      | Transport support and transport API;      |
       +----------------+-------------------------------------------+
       | grp            | Command groups, formerly lib/cmd;         |
       |                | each group, which has Zephyr built in     |
       |                | support has its own directory here;       |
       +----------------+-------------------------------------------+
       | util           | Utilities used by various subareas of     |
       |                | MCUmgr.                                   |
       +----------------+-------------------------------------------+

    Public API interfaces for above areas are now exported through zephyr_interface,
    and headers for them reside in ``zephyr/mgmt/mcumgr/<mcumgr_subarea>/``.
    For example to access mgmt API include ``<zephyr/mgmt/mcumgr/mgmt/mgmt.h>``.

    Private headers for above areas can be accessed, when required, using paths:
    ``mgmt/mcumgr/mgmt/<mcumgr_subarea>/``.

* LwM2M

  * The ``lwm2m_senml_cbor_*`` files have been regenerated using zcbor 0.6.0.

HALs
****

MCUboot
*******

Trusted Firmware-M
******************

zcbor
*****

Upgraded zcbor to 0.6.0. Among other things, this brings in a few convenient
changes for Zephyr:

* In the zcbor codebase, the ``ARRAY_SIZE`` macro has been renamed to
  ``ZCBOR_ARRAY_SIZE`` to not collide with Zephyr's :c:macro:`ARRAY_SIZE` macro.
* The zcbor codebase now better supports being used in C++ code.

The entire release notes can be found at
https://github.com/zephyrproject-rtos/zcbor/blob/0.6.0/RELEASE_NOTES.md

Documentation
*************

Tests and Samples
*****************

Issue Related Items
*******************

Known Issues
============

Addressed issues
================
