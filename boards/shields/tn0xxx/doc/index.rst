.. _ls0xx_generic_shield:

Kyocera Memory In Pixel TFT display generic shield
###################################

Overview
********

This is a generic shield for Kyocera Memory In Pixel TFT LCD. It supports
displays of TN0XXX type. These displays have an SPI interface and
few other pins. Note that the SCS is active high for this display.

Pins Assignment of the Generic Sharp memory Display Shield
==========================================================

+---------------+---------------------------------------------------------+
| Pin           | Function                                                |
+===============+=========================================================+
| SCS           | Serial Slave Select                                     |
+---------------+---------------------------------------------------------+
| SI            | Serial Data Input                                       |
+---------------+---------------------------------------------------------+
| SCLK          | Serial Clock Input                                      |
+---------------+---------------------------------------------------------+
| RST           | Reset                                     |
+---------------+---------------------------------------------------------+
| VCOM          | VCOM Inversion Polarity Input (VCOM can be controlled   |
|               | through SW)                                             |
+---------------+---------------------------------------------------------+

The zephyr display driver for TN0XXX only controls the SPI lines (SCS, SI, SCLK) - the power up and power down sequence is delegated to the user. This is because the datasheet specifies:

* A power up and down sequence with timings specified between toggling of RST, VCOM and input SPI lines. This MIP display is typically used in low power applications, and as such having user control over the power down (or sleep) and power up (or wake up) options are important. Since the API to power on and off the device is not present in the zephyr driver display API, the control of these GPIOs per the datasheet is delegated to the user. 
* VCOM = “L” is necessary when RST = ”H” When VCOM=“H”, otherwise the display does not turn to black, and current consumption increases by shoot-through-current in panel (several mA). Due to this factor, the VCOM signal is delegated to the user of this display driver API, along with the RST pin.

.. image:: ./images/power_sequence.png
   :align: center

Current supported displays
==========================

Following displays are supported but shield only exists
for TN0216ANVNANN. Other shields can be added by using the TN0216ANVNANN as
a reference and changing the width, height, etc configurations.

* TN0103ANVNANN-GN00
* TN0181ANVNANN-GN00
* TN0216ANVNANN-GN00
* TN0227ANVNANN-GNX03

+----------------------+------------------------------+
| Display              | Shield Designation           |
|                      |                              |
+======================+==============================+
| TN0216ANVNANN        | tn0216anvnann                |
+----------------------+------------------------------+

Requirements
************

This shield can only be used with a board that provides a configuration
for Arduino connectors and defines node aliases for SPI and GPIO interfaces
(see :ref:`shields` for more details).

Programming
***********

Set ``-DSHIELD=tn0216anvnann`` when you invoke ``west build``. For example:

.. zephyr-app-commands::
   :zephyr-app: samples/subsys/display/lvgl
   :board: nrf52833dk_nrf52833
   :shield: tn0216anvnann
   :goals: build

References
**********

Datasheet: https://display.kyocera.com/tn0216anvnann-gn00