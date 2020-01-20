.. _waveshare_e_paper_raw_panel_shield:

WAVESHARE e-Paper Raw Panel Shield
##################################

Overview
********

The WAVESHARE e-Paper Raw Panel Shield is a universal driver shield.
The shield can be used to drive various Electrophoretic (electronic ink)
Display (EPD) with a SPI interface.
This shield includes a 23LC1024 1Mb SPI Serial SRAM that is
not currently supported by the Zephyr RTOS.

More information about the shield can be found
at the `Universal e-Paper Raw Panel Driver Shield website`_.

Pins Assignment of the e-Paper Shield
=====================================

+-----------------------+------------+----------------------------+
| Shield Connector Pin  | Function   |                            |
+=======================+============+============================+
| D5                    | RAM CSn    |  RAM Chip Select           |
+-----------------------+------------+----------------------------+
| D6                    | SD CSn     |  EPD SD Card Chip Select   |
+-----------------------+------------+----------------------------+
| D7                    | EPD BUSY   |  EPD Busy Output           |
+-----------------------+------------+----------------------------+
| D8                    | EPD RESETn |  EPD Reset Input           |
+-----------------------+------------+----------------------------+
| D9                    | EPD DC     |  EPD Data/Command Input    |
+-----------------------+------------+----------------------------+
| D10                   | EPD CSn    |  EPD Chip Select Input     |
+-----------------------+------------+----------------------------+
| D11                   | SPI MOSI   |  Serial Data Input         |
+-----------------------+------------+----------------------------+
| D12                   | SPI MISO   |  Serial Data Out           |
+-----------------------+------------+----------------------------+
| D13                   | SPI SCK    |  Serial Clock Input        |
+-----------------------+------------+----------------------------+

Current supported displays
==========================

+--------------+-----------------+--------------+------------------------------+
| Display      | Ribbon Cable    | Controller / | Shield Designation           |
|              | Label           | Driver       |                              |
+==============+=================+==============+==============================+
| Good Display | HINK-E0213      | SSD1673 /    | waveshare_epaper_gdeh0213b1  |
| GDEH0213B1   |                 | ssd16xx      |                              |
+--------------+-----------------+--------------+------------------------------+
| Good Display | HINK-E0213A22   | IL3897 /     | waveshare_epaper_gdeh0213b72 |
| GDEH0213B72  |                 | ssd16xx      |                              |
+--------------+-----------------+--------------+------------------------------+
| Good Display | E029A01         | IL3820 /     | waveshare_epaper_gdeh029a1   |
| GDEH029A1    |                 | ssd16xx      |                              |
+--------------+-----------------+--------------+------------------------------+


Requirements
************

This shield can only be used with a board that provides a configuration
for Arduino connectors and defines node aliases for SPI and GPIO interfaces
(see :ref:`shields` for more details).

Programming
***********

Correct shield designation (see the table above) for your display must
be entered when you invoke ``west build``.
For example:

.. zephyr-app-commands::
   :zephyr-app: samples/gui/lvgl
   :board: nrf52840_pca10056
   :shield: waveshare_epaper_gdeh0213b1
   :goals: build

References
**********

.. target-notes::

.. _Universal e-Paper Raw Panel Driver Shield website:
   https://www.waveshare.com/e-paper-shield.htm
