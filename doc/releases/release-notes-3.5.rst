:orphan:

.. _zephyr_3.5:

Zephyr 3.5.0 (Working Draft)
############################

We are pleased to announce the release of Zephyr version 3.5.0.

Major enhancements with this release include:

The following sections provide detailed lists of changes by component.

Security Vulnerability Related
******************************

API Changes
***********

Changes in this release
=======================

Removed APIs in this release
============================

Deprecated in this release
==========================

* Setting the GIC architecture version by selecting
  :kconfig:option:`CONFIG_GIC_V1`, :kconfig:option:`CONFIG_GIC_V2` and
  :kconfig:option:`CONFIG_GIC_V3` directly in Kconfig has been deprecated.
  The GIC version should now be specified by adding the appropriate compatible, for
  example :dtcompatible:`arm,gic-v2`, to the GIC node in the device tree.

Stable API changes in this release
==================================

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

* Mesh

* Controller

* HCI Driver

Boards & SoC Support
********************

* Added support for these SoC series:

* Removed support for these SoC series:

* Made these changes in other SoC series:

* Added support for these ARC boards:

* Added support for these ARM boards:

* Added support for these ARM64 boards:

* Added support for these RISC-V boards:

* Added support for these X86 boards:

* Added support for these Xtensa boards:

* Made these changes for ARC boards:

* Made these changes for ARM boards:

* Made these changes for ARM64 boards:

* Made these changes for RISC-V boards:

* Made these changes for X86 boards:

* Made these changes for Xtensa boards:

* Removed support for these ARC boards:

* Removed support for these ARM boards:

* Removed support for these ARM64 boards:

* Removed support for these RISC-V boards:

* Removed support for these X86 boards:

* Removed support for these Xtensa boards:

* Made these changes in other boards:

* Added support for these following shields:

Build system and infrastructure
*******************************

Drivers and Sensors
*******************

* ADC

* Battery-backed RAM

* CAN

* Clock control

* Counter

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

* FPGA

* Fuel Gauge

* GPIO

* hwinfo

* I2C

* I2S

* I3C

* IEEE 802.15.4

* Interrupt Controller

  * GIC: Architecture version selection is now based on the device tree

* IPM

* KSCAN

* LED

* MBOX

* MEMC

* PCIE

* PECI

Trusted Firmware-M
******************
* Pin control

* PWM

* Power domain

* Regulators

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

USB
***

Devicetree
**********

Libraries / Subsystems
**********************

* Management

  * Introduced MCUmgr client support with handlers for img_mgmt and os_mgmt.

  * Added response checking to MCUmgr's :c:enumerator:`MGMT_EVT_OP_CMD_RECV`
    notification callback to allow applications to reject MCUmgr commands.

  * MCUmgr SMP version 2 error translation (to legacy MCUmgr error code) is now
    supported in function handlers by setting ``mg_translate_error`` of
    :c:struct:`mgmt_group` when registering a transport. See
    :c:type:`smp_translate_error_fn` for function details.
* shell

  * Added an application shell module with helpers common to all applications.

  * Fixed an issue with MCUmgr img_mgmt group whereby the size of the upload in
    the initial packet was not checked.

  * Fixed an issue with MCUmgr fs_mgmt group whereby some status codes were not
    checked properly, this meant that the error returned might not be the
    correct error, but would only occur in situations where an error was
    already present.

  * Fixed an issue whereby the SMP response function did not check to see if
    the initial zcbor map was created successfully.

  * Fixes an issue with MCUmgr shell_mgmt group whereby the length of a
    received command was not properly checked.

  * Added optional mutex locking support to MCUmgr img_mgmt group, which can
    be enabled with :kconfig:option:`CONFIG_MCUMGR_GRP_IMG_MUTEX`.

  * Added MCUmgr settings management group, which allows for manipulation of
    zephyr settings from a remote device, see :ref:`mcumgr_smp_group_3` for
    details.

  * Added :kconfig:option:`CONFIG_MCUMGR_GRP_IMG_ALLOW_CONFIRM_NON_ACTIVE_IMAGE_SECONDARY`
    and :kconfig:option:`CONFIG_MCUMGR_GRP_IMG_ALLOW_CONFIRM_NON_ACTIVE_IMAGE_ANY`
    that allow to control whether MCUmgr client will be allowed to confirm
    non-active images.

  * Added :kconfig:option:`CONFIG_MCUMGR_GRP_IMG_ALLOW_ERASE_PENDING` that allows
    to erase slots pending for next boot, that are not revert slots.

* File systems

  * Added support for ext2 file system.
  * Added support of mounting littlefs on the block device from the shell/fs.
  * Added alignment parameter to FS_LITTLEFS_DECLARE_CUSTOM_CONFIG macro, it can speed up read/write
    operation for SDMMC devices in case when we align buffers on CONFIG_SDHC_BUFFER_ALIGNMENT,
    because we can avoid extra copy of data from card bffer to read/prog buffer.

HALs
****

MCUboot
*******

Storage
*******

Trusted Firmware-M
******************

zcbor
*****

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
