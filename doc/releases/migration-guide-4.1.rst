:orphan:

.. _migration_4.1:

Migration guide to Zephyr v4.1.0 (Working Draft)
################################################

This document describes the changes required when migrating your application from Zephyr v4.0.0 to
Zephyr v4.1.0.

Any other changes (not directly related to migrating applications) can be found in
the :ref:`release notes<zephyr_4.1>`.

.. contents::
    :local:
    :depth: 2

Build System
************

Kernel
******

Boards
******

Modules
*******

Mbed TLS
========

Trusted Firmware-M
==================

LVGL
====

Device Drivers and Devicetree
*****************************

Controller Area Network (CAN)
=============================

Display
=======

Enhanced Serial Peripheral Interface (eSPI)
===========================================

GNSS
====

GPIO
====

* Renamed the device tree property ``pin_mask`` to ``pin-mask``.
* Renamed the device tree property ``pinmux_mask`` to ``pinmux-mask``.
* Renamed the device tree property ``vbatts_pins`` to ``vbatts-pins``.
* Renamed the device tree property ``bit_per_gpio`` to ``bit-per-gpio``.
* Renamed the device tree property ``off_val`` to ``off-val``.
* Renamed the device tree property ``on_val`` to ``on-val``.
* Renamed the ``compatible`` from ``ti,ads114s0x-gpio`` to :dtcompatible:`ti,ads1x4s0x-gpio`.

HWSPINLOCK
==========

* Renamed the DeviceTree property ``num_locks`` to ``num-locks``.

I2C
===

* Renamed the ``compatible`` from ``nxp,imx-lpi2c`` to :dtcompatible:`nxp,lpi2c`.

I2S
===

* Renamed the device tree property from ``fifo_depth`` to ``fifo-depth``.

Input
=====

Interrupt Controller
====================

LED Strip
=========

Sensors
=======

Serial
======

Regulator
=========

Bluetooth
*********

Bluetooth HCI
=============

Bluetooth Mesh
==============

Bluetooth Audio
===============

Bluetooth Classic
=================

Bluetooth Host
==============

* :kconfig:option:`CONFIG_BT_BUF_ACL_RX_COUNT` has been deprecated. The number of ACL RX buffers is
  now computed internally and is equal to :kconfig:option:`CONFIG_BT_MAX_CONN` + 1. If an application
  needs more buffers, it can use the new :kconfig:option:`CONFIG_BT_BUF_ACL_RX_COUNT_EXTRA` to add
  additional ones.

  e.g. if :kconfig:option:`CONFIG_BT_MAX_CONN` was ``3`` and
  :kconfig:option:`CONFIG_BT_BUF_ACL_RX_COUNT` was ``7`` then
  :kconfig:option:`CONFIG_BT_BUF_ACL_RX_COUNT_EXTRA` should be set to ``7 - (3 + 1) = 3``.

  .. warning::

   The default value of :kconfig:option:`CONFIG_BT_BUF_ACL_RX_COUNT` has been set to 0.

Bluetooth Crypto
================

Networking
**********

Other Subsystems
****************

Flash map
=========

hawkBit
=======

MCUmgr
======

Modem
=====

Architectures
*************
