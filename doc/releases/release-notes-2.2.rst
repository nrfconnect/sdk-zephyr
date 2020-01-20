:orphan:

.. _zephyr_2.2:

Zephyr 2.2.0 (Working Draft)
############################

We are pleased to announce the release of Zephyr kernel version 2.2.0.

Major enhancements with this release include:

* <TBD>

The following sections provide detailed lists of changes by component.

Security Vulnerability Related
******************************

No security vulnerabilities received.

API Changes
***********

Deprecated in this release
==========================

* Settings

  * SETTINGS_USE_BASE64, encoding values in base64 is marked for removal.

Stable API changes in this release
==================================

* PWM

  * The pwm_pin_set_cycles(), pwm_pin_set_usec(), and
    pwm_pin_set_nsec() functions now take a flags parameter. The newly
    introduced flags are PWM_POLARITY_NORMAL and PWM_POLARITY_INVERTED
    for specifying the polarity of the PWM signal. The flags parameter
    can be set to 0 if no flags are required (the default is
    PWM_POLARITY_NORMAL).
  * Similarly, the pwm_pin_set_t PWM driver API function function now
    takes a flags parameter. The PWM controller driver must check the
    value of the flags parameter and return -ENOTSUP if any
    unsupported flag is set.

* USB

  * The usb_enable() function, which was previously invoked automatically
    by the USB stack, now needs to be explicitly called by the application
    in order to enable the USB subsystem.
  * The usb_enable() function now takes a parameter, usb_dc_status_callback
    which can be set by the application to a callback to receive status events
    from the USB stack. The parameter can also be set to NULL if no callback is required.

Removed APIs in this release
============================

* Shell

  * SHELL_CREATE_STATIC_SUBCMD_SET (deprecated), replaced by
    SHELL_STATIC_SUBCMD_SET_CREATE
  * SHELL_CREATE_DYNAMIC_CMD (deprecated), replaced by SHELL_DYNAMIC_CMD_CREATE

Kernel
******

* <TBD>

Architectures
*************

* ARC:

  * <TBD>

* ARM:

  * <TBD>

* POSIX:

  * <TBD>

* RISC-V:

  * <TBD>

* x86:

  * <TBD>

Boards & SoC Support
********************

* Added support for these SoC series:

.. rst-class:: rst-columns

   * <TBD>

* Added support for these ARM boards:

  .. rst-class:: rst-columns

     * <TBD>

* Added support for these following shields:

  .. rst-class:: rst-columns

     * <TBD>

Drivers and Sensors
*******************

* ADC

  * <TBD>

* Bluetooth

  * <TBD>

* CAN

  * <TBD>

* Clock Control

  * <TBD>

* Console

  * <TBD>

* Counter

  * <TBD>

* Display

  * <TBD>

* DMA

  * <TBD>

* EEPROM

  * <TBD>

* Entropy

  * <TBD>

* Ethernet

  * <TBD>

* Flash

  * <TBD>

* GPIO

  * <TBD>

* Hardware Info

  * <TBD>

* I2C

  * <TBD>

* I2S

  * <TBD>

* IEEE 802.15.4

  * <TBD>

* Interrupt Controller

  * <TBD>

* IPM

  * <TBD>

* Keyboard Scan

  * <TBD>

* LED

  * <TBD>

* Modem

  * <TBD>

* Pinmux

  * <TBD>

* PS/2

  * <TBD>

* PWM

  * <TBD>

* Sensor

  * <TBD>

* Serial

  * <TBD>

* SPI

  * <TBD>

* Timer

  * <TBD>

* USB

  * <TBD>

* Video

  * <TBD>

* Watchdog

  * <TBD>

* WiFi

  * <TBD>

Networking
**********

* <TBD>

Bluetooth
*********

* Host:

  * <TBD>

* BLE split software Controller:

  * <TBD>

* BLE legacy software Controller:

  * <TBD>

Build and Infrastructure
************************

* The minimum Python version supported by Zephyr's build system and tools is
  now 3.6.
* <Other items TBD>

Libraries / Subsystems
***********************

* Random

  * <TBD>

HALs
****

* HALs are now moved out of the main tree as external modules and reside in
  their own standalone repositories.

Documentation
*************

* <TBD>

Tests and Samples
*****************

* <TBD>

Issue Related Items
*******************

These GitHub issues were addressed since the previous 2.1.0 tagged
release:

.. comment  List derived from GitHub Issue query: ...
   * :github:`issuenumber` - issue title
