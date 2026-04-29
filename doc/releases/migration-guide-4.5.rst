:orphan:

..
  See
  https://docs.zephyrproject.org/latest/releases/index.html#migration-guides
  for details of what is supposed to go into this document.

.. _migration_4.5:

Migration guide to Zephyr v4.5.0 (Working Draft)
################################################

This document describes the changes required when migrating your application from Zephyr v4.4.0 to
Zephyr v4.5.0.

Any other changes (not directly related to migrating applications) can be found in
the :ref:`release notes<zephyr_4.5>`.

.. contents::
    :local:
    :depth: 2

Common
******

Build System
************

Kernel
******

Boards
******

* The Kconfig options :kconfig:option:`CONFIG_SRAM_SIZE` and
  :kconfig:option:`CONFIG_SRAM_BASE_ADDRESS` have been deprecated, boards should instead use the
  devicetree ``zephyr.sram`` chosen node to specify the RAM node which will be used (whose values
  populated the Kconfig values). If either option is manually adjusted, it will cause
  :kconfig:option:`CONFIG_SRAM_DEPRECATED_KCONFIG_SET` to be set which indicates this deprecation.

Device Drivers and Devicetree
*****************************

.. Group contents in this section by subsystem, e.g.:
..
.. ADC
.. ===
..
.. ...

.. zephyr-keep-sorted-start re(^\w) ignorecase


.. zephyr-keep-sorted-stop

Bluetooth
*********


Networking
**********

Other subsystems
****************

Modules
*******

hal_nxp
=======

* S32K344: The pinmux header file for this SoC was renamed from ``S32K344-172MQFP-pinctrl.h`` to
  ``S32K344_K324_K314_172HDQFP-pinctrl.h``. Out-of-tree boards must update their include directive accordingly::

    #include <nxp/s32/S32K344_K324_K314_172HDQFP-pinctrl.h>

Mbed TLS
========

* :kconfig:option:`CONFIG_MBEDTLS_SSL_EARLY_DATA` is now an explicit opt-in and is no longer
  implicitly enabled by :kconfig:option:`CONFIG_MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_PSK_ENABLED`.
  Out-of-tree applications or board configurations that rely on TLS 1.3 PSK early data (0-RTT)
  must now explicitly enable :kconfig:option:`CONFIG_MBEDTLS_SSL_EARLY_DATA`.

Architectures
*************
