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

* If a platform has a CSPRNG source available (i.e. :kconfig:option:`CONFIG_CSPRNG_ENABLED`
  is set), then the Kconfig option :kconfig:option:`CONFIG_MBEDTLS_PSA_CRYPTO_EXTERNAL_RNG`
  is the default choice for random number source instead of
  :kconfig:option:`CONFIG_MBEDTLS_PSA_CRYPTO_LEGACY_RNG`. This helps in reducing
  ROM/RAM footprint of the Mbed TLS library.

* The newly-added Kconfig option :kconfig:option:`CONFIG_MBEDTLS_PSA_KEY_SLOT_COUNT`
  allows to specify the number of key slots available in the PSA Crypto core.
  Previously this value was not explicitly set, so Mbed TLS's default value of
  32 was used. The new Kconfig option defaults to 16 instead in order to find
  a reasonable compromise between RAM consumption and most common use cases.
  It can be further trimmed down to reduce RAM consumption if the final
  application doesn't need that many key slots simultaneously.

Trusted Firmware-M
==================

LVGL
====

* The config option :kconfig:option:`CONFIG_LV_Z_FLUSH_THREAD_PRIO` is now called
  :kconfig:option:`CONFIG_LV_Z_FLUSH_THREAD_PRIORITY` and its value is now interpreted as an
  absolute priority instead of a cooperative one.

Device Drivers and Devicetree
*****************************

Controller Area Network (CAN)
=============================

Display
=======

* Displays using the MIPI DBI driver which set their MIPI DBI mode via the
  ``mipi-mode`` property in devicetree should now use a string property of
  the same name, like so:

  .. code-block:: devicetree

    /* Legacy display definition */

    st7735r: st7735r@0 {
        ...
        mipi-mode = <MIPI_DBI_MODE_SPI_4WIRE>;
        ...
    };

    /* New display definition */

    st7735r: st7735r@0 {
        ...
        mipi-mode = "MIPI_DBI_MODE_SPI_4WIRE";
        ...
    };


Enhanced Serial Peripheral Interface (eSPI)
===========================================

GNSS
====

Input
=====

Interrupt Controller
====================

LED Strip
=========

Pin Control
===========

  * Renamed the ``compatible`` from ``nxp,kinetis-pinctrl`` to :dtcompatible:`nxp,port-pinctrl`.
  * Renamed the ``compatible`` from ``nxp,kinetis-pinmux`` to :dtcompatible:`nxp,port-pinmux`.

Sensors
=======

Serial
======

Stepper
=======

  * Renamed the ``compatible`` from ``zephyr,gpio-steppers`` to :dtcompatible:`zephyr,gpio-stepper`.
  * Renamed the ``stepper_set_actual_position`` function to :c:func:`stepper_set_reference_position`.

Regulator
=========

Video
=====

* The :file:`include/zephyr/drivers/video-controls.h` got updated to have video controls IDs (CIDs)
  matching the definitions in the Linux kernel file ``include/uapi/linux/v4l2-controls.h``.
  In most cases, removing the category prefix is enough: ``VIDEO_CID_CAMERA_GAIN`` becomes
  ``VIDEO_CID_GAIN``.
  The new ``video-controls.h`` source now contains description of each control ID to help
  disambiguating.

Bluetooth
*********

Bluetooth HCI
=============

Bluetooth Mesh
==============

Bluetooth Audio
===============

* The following Kconfig options are not longer automatically enabled by the LE Audio Kconfig
  options and may need to be enabled manually (:github:`81328`):

    * :kconfig:option:`CONFIG_BT_GATT_CLIENT`
    * :kconfig:option:`CONFIG_BT_GATT_AUTO_DISCOVER_CCC`
    * :kconfig:option:`CONFIG_BT_GATT_AUTO_UPDATE_MTU`
    * :kconfig:option:`CONFIG_BT_EXT_ADV`
    * :kconfig:option:`CONFIG_BT_PER_ADV_SYNC`
    * :kconfig:option:`CONFIG_BT_ISO_BROADCASTER`
    * :kconfig:option:`CONFIG_BT_ISO_SYNC_RECEIVER`
    * :kconfig:option:`CONFIG_BT_PAC_SNK`
    * :kconfig:option:`CONFIG_BT_PAC_SRC`

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

* The Prometheus metric creation has changed as user does not need to have a separate
  struct :c:struct:`prometheus_metric` any more. This means that the Prometheus macros
  :c:macro:`PROMETHEUS_COUNTER_DEFINE`, :c:macro:`PROMETHEUS_GAUGE_DEFINE`,
  :c:macro:`PROMETHEUS_HISTOGRAM_DEFINE` and :c:macro:`PROMETHEUS_SUMMARY_DEFINE`
  prototypes have changed. (:github:`81712`)


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

* Common

  * ``_current`` is deprecated, used :c:func:`arch_current_thread` instead.

* native/POSIX

  * :kconfig:option:`CONFIG_NATIVE_APPLICATION` has been deprecated. Out-of-tree boards using this
    option should migrate to the native_simulator runner (:github:`81232`).
    For an example of how this was done with a board in-tree check :github:`61481`.
  * For the native_sim target :kconfig:option:`CONFIG_NATIVE_SIM_NATIVE_POSIX_COMPAT` has been
    switched to ``n`` by default, and this option has been deprecated. Ensure your code does not
    use the :kconfig:option:`CONFIG_BOARD_NATIVE_POSIX` option anymore (:github:`81232`).
