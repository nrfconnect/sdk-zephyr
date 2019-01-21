.. _mimxrt1050_evk:

NXP MIMXRT1050-EVK
##################

Overview
********

The i.MX RT1050 is a new processor family featuring NXP's advanced
implementation of the ARM Cortex-M7 Core. It provides high CPU performance and
real-time response. The i.MX RT1050 provides various memory interfaces,
including SDRAM, Raw NAND FLASH, NOR FLASH, SD/eMMC, Quad SPI, HyperBus and a
wide range of other interfaces for connecting peripherals, such as WLAN,
Bluetooth™, GPS, displays, and camera sensors. As with other i.MX processors,
i.MX RT1050 also has rich audio and video features, including LCD display,
basic 2D graphics, camera interface, SPDIF, and I2S audio interface.

The following document refers to the discontinued MIMXRT1050-EVK board. For the
MIMXRT1050-EVKB board, refer to `Board Revisions`_ section.

.. image:: mimxrt1050_evk.jpg
   :width: 720px
   :align: center
   :alt: MIMXRT1050-EVK

Hardware
********

- MIMXRT1052DVL6A MCU (600 MHz, 512 KB TCM)

- Memory

  - 256 KB SDRAM
  - 64 Mbit QSPI Flash
  - 512 Mbit Hyper Flash

- Display

  - LCD connector
  - Touch connector

- Ethernet

  - 10/100 Mbit/s Ethernet PHY

- USB

  - USB 2.0 OTG connector
  - USB 2.0 host connector

- Audio

  - 3.5 mm audio stereo headphone jack
  - Board-mounted microphone
  - Left and right speaker out connectors

- Power

  - 5 V DC jack

- Debug

  - JTAG 20-pin connector
  - OpenSDA with DAPLink

- Sensor

  - FXOS8700CQ 6-axis e-compass
  - CMOS camera sensor interface

- Expansion port

  - Arduino interface

- CAN bus connector

For more information about the MIMXRT1050 SoC and MIMXRT1050-EVK board, see
these references:

- `i.MX RT1050 Website`_
- `i.MX RT1050 Datasheet`_
- `i.MX RT1050 Reference Manual`_
- `MIMXRT1050-EVK Website`_
- `MIMXRT1050-EVK User Guide`_
- `MIMXRT1050-EVK Schematics`_

Supported Features
==================

The mimxrt1050_evk board configuration supports the following hardware
features:

+-----------+------------+-------------------------------------+
| Interface | Controller | Driver/Component                    |
+===========+============+=====================================+
| NVIC      | on-chip    | nested vector interrupt controller  |
+-----------+------------+-------------------------------------+
| SYSTICK   | on-chip    | systick                             |
+-----------+------------+-------------------------------------+
| GPIO      | on-chip    | gpio                                |
+-----------+------------+-------------------------------------+
| I2C       | on-chip    | i2c                                 |
+-----------+------------+-------------------------------------+
| SPI       | on-chip    | spi                                 |
+-----------+------------+-------------------------------------+
| UART      | on-chip    | serial port-polling;                |
|           |            | serial port-interrupt               |
+-----------+------------+-------------------------------------+
| ENET      | on-chip    | ethernet                            |
+-----------+------------+-------------------------------------+

The default configuration can be found in the defconfig file:

	``boards/arm/mimxrt1050_evk/mimxrt1050_evk_defconfig``

Other hardware features are not currently supported by the port.

Connections and IOs
===================

The MIMXRT1050 SoC has five pairs of pinmux/gpio controllers.

+---------------+-----------------+---------------------------+
| Name          | Function        | Usage                     |
+===============+=================+===========================+
| GPIO_AD_B0_00 | LPSPI3_SCK      | SPI                       |
+---------------+-----------------+---------------------------+
| GPIO_AD_B0_01 | LPSPI3_SDO      | SPI                       |
+---------------+-----------------+---------------------------+
| GPIO_AD_B0_02 | LPSPI3_SDI      | SPI                       |
+---------------+-----------------+---------------------------+
| GPIO_AD_B0_03 | LPSPI3_PCS0     | SPI                       |
+---------------+-----------------+---------------------------+
| GPIO_AD_B0_09 | GPIO/ENET_RST   | LED                       |
+---------------+-----------------+---------------------------+
| GPIO_AD_B0_10 | GPIO/ENET_INT   | GPIO/Ethernet             |
+---------------+-----------------+---------------------------+
| GPIO_AD_B0_12 | LPUART1_TX      | UART Console              |
+---------------+-----------------+---------------------------+
| GPIO_AD_B0_13 | LPUART1_RX      | UART Console              |
+---------------+-----------------+---------------------------+
| GPIO_AD_B1_00 | LPI2C1_SCL      | I2C                       |
+---------------+-----------------+---------------------------+
| GPIO_AD_B1_01 | LPI2C1_SDA      | I2C                       |
+---------------+-----------------+---------------------------+
| GPIO_AD_B1_06 | LPUART3_TX      | UART BT HCI               |
+---------------+-----------------+---------------------------+
| GPIO_AD_B1_07 | LPUART3_RX      | UART BT HCI               |
+---------------+-----------------+---------------------------+
| WAKEUP        | GPIO            | SW0                       |
+---------------+-----------------+---------------------------+
| GPIO_B1_04    | ENET_RX_DATA00  | Ethernet                  |
+---------------+-----------------+---------------------------+
| GPIO_B1_05    | ENET_RX_DATA01  | Ethernet                  |
+---------------+-----------------+---------------------------+
| GPIO_B1_06    | ENET_RX_EN      | Ethernet                  |
+---------------+-----------------+---------------------------+
| GPIO_B1_07    | ENET_TX_DATA00  | Ethernet                  |
+---------------+-----------------+---------------------------+
| GPIO_B1_08    | ENET_TX_DATA01  | Ethernet                  |
+---------------+-----------------+---------------------------+
| GPIO_B1_09    | ENET_TX_EN      | Ethernet                  |
+---------------+-----------------+---------------------------+
| GPIO_B1_10    | ENET_REF_CLK    | Ethernet                  |
+---------------+-----------------+---------------------------+
| GPIO_B1_11    | ENET_RX_ER      | Ethernet                  |
+---------------+-----------------+---------------------------+
| GPIO_EMC_40   | ENET_MDC        | Ethernet                  |
+---------------+-----------------+---------------------------+
| GPIO_EMC_41   | ENET_MDIO       | Ethernet                  |
+---------------+-----------------+---------------------------+
| GPIO_AD_B0_09 | ENET_RST        | Ethernet                  |
+---------------+-----------------+---------------------------+
| GPIO_AD_B0_10 | ENET_INT        | Ethernet                  |
+---------------+-----------------+---------------------------+

System Clock
============

The MIMXRT1050 SoC is configured to use the 24 MHz external oscillator on the
board with the on-chip PLL to generate a 600 MHz core clock.

Serial Port
===========

The MIMXRT1050 SoC has eight UARTs. ``LPUART1`` is configured for the console,
``LPUART3`` for the Bluetooth Host Controller Interface (BT HCI), and the
remaining are not used.

Programming and Debugging
*************************

The MIMXRT1050-EVK includes the :ref:`nxp_opensda` serial and debug adapter
built into the board to provide debugging, flash programming, and serial
communication over USB.

To use the Segger J-Link tools with OpenSDA, follow the instructions in the
:ref:`nxp_opensda_jlink` page using the `Segger J-Link OpenSDA V2.1 Firmware`_.
The Segger J-Link tools are the default for this board, therefore it is not
necessary to set ``OPENSDA_FW=jlink`` explicitly when you invoke ``make
debug``.

With these mechanisms, applications for the ``mimxrt1050_evk`` board
configuration can be built and debugged in the usual way (see
:ref:`build_an_application` and :ref:`application_run` for more details).

The pyOCD tools do not yet support this SoC.

Flashing
========

The Segger J-Link firmware does not support command line flashing, therefore
the usual ``flash`` build system target is not supported.

Debugging
=========

This example uses the :ref:`hello_world` sample with the
:ref:`nxp_opensda_jlink` tools. Run the following to build your Zephyr
application, invoke the J-Link GDB server, attach a GDB client, and program
your Zephyr application to flash. It will leave you at a GDB prompt.

.. zephyr-app-commands::
   :zephyr-app: samples/hello_world
   :board: mimxrt1050_evk
   :goals: debug

Board Revisions
***************

The original MIMXRT1050-EVK (rev A0) board was updated with a newer
MIMXRT1050-EVKB (rev A1) board, with these major hardware differences::

- SoC changed from MIMXRT1052DVL6**A** to MIMXRT1052DVL6**B**
- Hardware bug fixes for: power, interfaces, and memory
- Arduino headers included

For more details, please see the following `NXP i.MXRT1050 A0 to A1 Migration Guide`_.

Current Zephyr build supports the new MIMXRT1050-EVKB

.. _MIMXRT1050-EVK Website:
   https://www.nxp.com/products/microcontrollers-and-processors/arm-based-processors-and-mcus/i.mx-applications-processors/i.mx-rt-series/i.mx-rt1050-evaluation-kit:MIMXRT1050-EVK

.. _MIMXRT1050-EVK User Guide:
   https://www.nxp.com/webapp/Download?colCode=IMXRT1050EVKBHUG

.. _MIMXRT1050-EVK Schematics:
   https://www.nxp.com/webapp/Download?colCode=MIMXRT1050-EVK-DESIGNFILES

.. _i.MX RT1050 Website:
   https://www.nxp.com/products/microcontrollers-and-processors/arm-based-processors-and-mcus/i.mx-applications-processors/i.mx-rt-series/i.mx-rt1050-crossover-processor-with-arm-cortex-m7-core:i.MX-RT1050

.. _i.MX RT1050 Datasheet:
   https://www.nxp.com/docs/en/data-sheet/IMXRT1050CEC.pdf

.. _i.MX RT1050 Reference Manual:
   https://www.nxp.com/docs/en/reference-manual/IMXRT1050RM.pdf

.. _DAPLink FRDM-K64F Firmware:
   http://www.nxp.com/assets/downloads/data/en/ide-debug-compile-build-tools/OpenSDAv2.2_DAPLink_frdmk64f_rev0242.zip

.. _Segger J-Link OpenSDA V2.1 Firmware:
   https://www.segger.com/downloads/jlink/OpenSDA_V2_1.bin

.. _NXP i.MXRT1050 A0 to A1 Migration Guide:
   https://www.nxp.com/docs/en/nxp/application-notes/AN12146.pdf
