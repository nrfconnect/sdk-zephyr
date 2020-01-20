.. _board_porting_guide:

Board Porting Guide
###################

When building an application you must specify the target hardware and
the exact board or model. Specifying the board name results in a binary that
is suited for the target hardware by selecting the right Zephyr features and
components and setting the right Zephyr configuration for that specific target
hardware.

A board is defined as a special configuration of an SoC with possible additional
components.
For example, a board might have sensors and flash memory implemented as
additional features on top of what the SoC provides. Such additional hardware is
configured and referenced in the Zephyr board configuration.

The board implements at least one SoC and thus inherits all of the features
that are provided by the SoC. When porting a board to Zephyr, you should
first make sure the SoC is implemented in Zephyr.

Hardware Configuration Hierarchy
********************************

Hardware definitions in Zephyr follow a well-defined hierarchy of configurations
and layers, below are the layers from top to bottom:

- Board
- SoC
- SoC Series
- SoC Family
- CPU Core
- Architecture

This design contributes to code reuse and implementation of device drivers and
features at the bottom of the hierarchy making a board configuration as simple
as a selection of features that are implemented by the underlying layers. The
figures below shows this hierarchy with a few example of boards currently
available in the source tree:

.. figure:: board/hierarchy.png
   :width: 500px
   :align: center
   :alt: Configuration Hierarchy

   Configuration Hierarchy


Hierarchy Example

+------------+-----------+--------------+------------+--------------+---------+
|Board       |FRDM K64F  |nRF52 NITROGEN|nRF51XX     |Quark SE C1000|Arduino  |
|            |           |              |            |Devboard      |101      |
+============+===========+==============+============+==============+=========+
|SOC         |MK64F12    |nRF52832      |nRF51XX     |Quark SE C1000|Curie    |
+------------+-----------+--------------+------------+--------------+---------+
|SOC Series  |Kinetis K6x|Nordic NRF52  |Nordic NRF51|Quark SE      |Quark SE |
|            |Series     |              |            |              |         |
+------------+-----------+--------------+------------+--------------+---------+
|SOC Family  |NXP Kinetis|Nordic NRF5   |Nordic NRF5 |Quark         |Quark    |
+------------+-----------+--------------+------------+--------------+---------+
|CPU Core    |Cortex-M4  |Cortex-M4     |Cortex-M0   |Lakemont      |Lakemont |
+------------+-----------+--------------+------------+--------------+---------+
|Architecture|ARM        |ARM           |ARM         |x86           |x86      |
+------------+-----------+--------------+------------+--------------+---------+


Architecture
============
If your CPU architecture is already supported by Zephyr, there is no
architecture work involved in porting to your board.  If your CPU architecture
is not supported by the Zephyr kernel, you can add support by following the
instructions available at :ref:`architecture_porting_guide`.

CPU Core
========

Some OS code depends on the CPU core that your board is using. For
example, a given CPU core has a specific assembly language instruction set, and
may require special cross compiler or compiler settings to use the appropriate
instruction set.

If your CPU architecture is already supported by Zephyr, there is no CPU core
work involved in porting to your platform or board. You need only to select the
appropriate CPU in your configuration and the rest will be taken care of by the
configuration system in Zephyr which will select the features implemented
by the corresponding CPU.

Platform
========

This layer implements most of the features that need porting and is split into
three layers to allow for code reuse when dealing with implementations with
slight differences.

SoC Family
----------

This layer is a container of all SoCs of the same class that, for example
implement one single type of CPU core but differ in peripherals and features.
The base hardware will in most cases be the same across all SoCs and MCUs of
this family.

SoC Series
----------

Moving closer to the SoC, the series is derived from an SoC family. A series is
defined by a feature set that serves the purpose of distinguishing different
SoCs belonging to the same family.

SoC
---

Finally, an SoC is actual hardware component that is physically available on a
board.

Board
=====

A board implements an SoC with all its features, together with peripherals
available on the board that differentiates the board with additional interfaces
and features not available in the SoC.

A board port on Zephyr typically consists of two parts:

- A hardware description (usually done by device tree), which specifies embedded
  SoC reference, connectors and any other hardware components such as LEDs,
  buttons, sensors or communication peripherals (USB, BLE controller, ...).

- A software configuration (done using Kconfig) of features and peripheral
  drivers. This default board configuration is subordinated to features
  activation which is application responsibility. Though, it should also enable
  a minimal set of features common to many applications and to applicable
  project-provided :ref:`samples-and-demos`.


.. _default_board_configuration:

Default board configuration
***************************

When porting Zephyr to a board, you must provide the board's default
Kconfig configuration, which is used in application builds unless explicitly
overridden.

Setting Kconfig configuration values is documented in detail in
:ref:`setting_configuration_values`, which you should go through. Note that the
default board configuration might involve both :file:`<BOARD>_defconfig` and
:file:`Kconfig.defconfig` files. The rest of this section contains some
board-specific guidelines.

In order to provide consistency across the various boards and ease the work of
users providing applications that are not board specific, the following
guidelines should be followed when porting a board. Unless explicitly stated,
peripherals should be disabled by default.

- Configure and enable a working clock configuration, along with a tick source.

- Provide pin and driver configuration that matches the board's valuable
  components such as sensors, buttons or LEDs, and communication interfaces
  such as USB, Ethernet connector, or Bluetooth/Wi-Fi chip.

- When a well-known connector is present (such as used on an Arduino or
  96board), configure pins to fit this connector.

- Configure components that enable the use of these pins, such as
  configuring an SPI instance for Arduino SPI.

- If available, configure and enable a serial output for the console.

- Propose and configure a default network interface.

- Enable all GPIO ports.

- If available, enable pinmux and interrupt controller drivers.
