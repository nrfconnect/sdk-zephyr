.. _blinky-sample:

Blinky Application
##################

Overview
********

The Blinky example shows how to configure GPIO pins as outputs which can also be
used to drive LEDs on the hardware usually delivered as "User LEDs" on many of
the supported boards in Zephyr.

Requirements
************

The demo assumes that an LED is connected to one of GPIO lines. The
sample code is configured to work on boards that have defined the led0
alias in their board device tree description file. Doing so will generate
these variables:

- LED0_GPIO_CONTROLLER
- LED0_GPIO_PIN


Building and Running
********************

This samples does not output anything to the console.  It can be built and
flashed to a board as follows:

.. zephyr-app-commands::
   :zephyr-app: samples/basic/blinky
   :board: arduino_101
   :goals: build flash
   :compact:

After flashing the image to the board, the user LED on the board should start to
blink.
