:orphan:

..
  See
  https://docs.zephyrproject.org/latest/releases/index.html#migration-guides
  for details of what is supposed to go into this document.

.. _migration_4.4:

Migration guide to Zephyr v4.4.0 (Working Draft)
################################################

This document describes the changes required when migrating your application from Zephyr v4.3.0 to
Zephyr v4.4.0.

Any other changes (not directly related to migrating applications) can be found in
the :ref:`release notes<zephyr_4.4>`.

.. contents::
    :local:
    :depth: 2

Build System
************

Kernel
******

Boards
******

Device Drivers and Devicetree
*****************************

Bluetooth
*********

Bluetooth Host
==============

* :kconfig:option:`CONFIG_BT_SIGNING` has been deprecated.
* :c:macro:`BT_GATT_CHRC_AUTH` has been deprecated.
* :c:member:`bt_conn_le_info.interval` has been deprecated. Use
  :c:member:`bt_conn_le_info.interval_us` instead. Note that the units have changed: ``interval``
  was in units of 1.25 milliseconds, while ``interval_us`` is in microseconds.
* Legacy Bluetooth LE pairing using the passkey entry method no longer grants authenticated (MITM)
  protection as of the Bluetooth Core Specification v6.2. Stored bonds that were generated using
  this method will be downgraded to unauthenticated when loaded from persistent storage, resulting
  in a lower security level.

Networking
**********

Other subsystems
****************

Modules
*******

Trusted Firmware-M
==================

* The ``SECURE_UART1`` TF-M define is now controlled by Zephyr's
  :kconfig:option:`CONFIG_TFM_SECURE_UART`. This option will override any platform values previously
  specified in the TF-M repository.

Architectures
*************
