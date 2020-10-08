:orphan:

.. _zephyr_2.5:

Zephyr 2.5.0 (Working Draft)
############################

We are pleased to announce the release of Zephyr RTOS version 2.5.0.

Major enhancements with this release include:

The following sections provide detailed lists of changes by component.

Security Vulnerability Related
******************************

The following CVEs are addressed by this release:

More detailed information can be found in:
https://docs.zephyrproject.org/latest/security/vulnerabilities.html

Known issues
************

You can check all currently known issues by listing them using the GitHub
interface and listing all issues with the `bug label
<https://github.com/zephyrproject-rtos/zephyr/issues?q=is%3Aissue+is%3Aopen+label%3Abug>`_.

API Changes
***********

* The :c:func:`lwm2m_rd_client_start` function now accepts an additional
  ``flags`` parameter, which allows to configure current LwM2M client session,
  for instance enable bootstrap procedure in the curent session.

Deprecated in this release
==========================

Removed APIs in this release
============================

Stable API changes in this release
==================================

Kernel
******

Architectures
*************

* ARC

* ARM

  * AARCH32

  * AARCH64

* POSIX

* RISC-V

* x86

Boards & SoC Support
********************

* Added support for these SoC series:

* Made these changes in other SoC series:

* Changes for ARC boards:

* Added support for these ARM boards:

* Made these changes in other boards:

* Added support for these following shields:

Drivers and Sensors
*******************

* ADC

* Audio

* Bluetooth

* CAN

* Clock Control

* Console

* Counter

* Crypto

* DAC

* Debug

* Display

* DMA

* EEPROM

* Entropy

* ESPI

* Ethernet

* Flash

* GPIO

* Hardware Info

* I2C

* I2S

* IEEE 802.15.4

* Interrupt Controller

* IPM

* Keyboard Scan

* LED

* LED Strip

* LoRa

* Modem

* PECI

* Pinmux

* PS/2

* PWM

* Sensor

* Serial

* SPI

* Timer

* USB

* Video

* Watchdog

* WiFi

Networking
**********

Bluetooth
*********

* Host

* Mesh

* BLE split software Controller

* HCI Driver

Build and Infrastructure
************************

* Improved support for additional toolchains:

* Devicetree

Libraries / Subsystems
**********************

* Disk

* Management

  * MCUmgr

  * updatehub

* Settings

* Random

* POSIX subsystem

* Power management

* Logging

* LVGL

* Shell

* Storage

* Tracing

* Debug

HALs
****

* HALs are now moved out of the main tree as external modules and reside in
  their own standalone repositories.

Documentation
*************

Tests and Samples
*****************

Issue Related Items
*******************

These GitHub issues were addressed since the previous 2.4.0 tagged
release:
