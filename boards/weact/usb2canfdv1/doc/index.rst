.. zephyr:board:: usb2canfdv1

Overview
********

The WeAct Studio USB2CANFDV1 is a dedicated USB to CAN FD adapter board. More information can be
found on the `USB2CANFDV1 website`_.

Hardware
********

The USB2CANFDV1 is equipped with a STM32G0B1CBT6 microcontroller and features a USB-C connector, a
terminal block for connecting to the CAN bus, and three LEDs.

Supported Features
==================

The ``usb2canfdv1`` board configuration supports the following hardware features:

+-----------+------------+-------------------------------------+
| Interface | Controller | Driver/Component                    |
+===========+============+=====================================+
| NVIC      | on-chip    | nested vector interrupt controller  |
+-----------+------------+-------------------------------------+
| PINMUX    | on-chip    | pinmux                              |
+-----------+------------+-------------------------------------+
| FLASH     | on-chip    | flash memory                        |
+-----------+------------+-------------------------------------+
| GPIO      | on-chip    | gpio                                |
+-----------+------------+-------------------------------------+
| USB       | on-chip    | USB                                 |
+-----------+------------+-------------------------------------+
| FDCAN1    | on-chip    | CAN controller                      |
+-----------+------------+-------------------------------------+

The default configuration can be found in the defconfig file:
:zephyr_file:`boards/weact/usb2canfdv1/usb2canfdv1_defconfig`.

Other hardware features are not currently supported by the port.

System Clock
============

The STM32G0B1CBT6 PLL is driven by an external crystal oscillator (HSE) running at 16 MHz and
configured to provide a system clock of 60 MHz. This allows generating a FDCAN1 core clock of 80
MHz.

Programming and Debugging
*************************

Build and flash applications as usual (see :ref:`build_an_application` and
:ref:`application_run` for more details).

Here is an example for the :zephyr:code-sample:`blinky` application.

.. zephyr-app-commands::
   :zephyr-app: samples/basic/blinky
   :board: usb2canfdv1
   :goals: flash

.. _USB2CANFDV1 website:
   https://github.com/WeActStudio/WeActStudio.USB2CANFDV1
