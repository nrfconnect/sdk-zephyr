.. _spi-nor-sample:

JEDEC SPI-NOR Sample
####################

Overview
********

This sample demonstrates using the flash API on a SPI NOR serial flash
memory device.  While trivial it is an example of direct access and
allows confirmation that the flash is working and that automatic power
savings is correctly implemented.

Building and Running
********************

The application will build only for a target that has a :ref:`devicetree <dt-guide>`
``spi-flash0`` alias that refers to an entry with one of the following bindings as a compatible:

* :dtcompatible:`jedec,spi-nor`,
* :dtcompatible:`st,stm32-qspi-nor`,
* :dtcompatible:`st,stm32-ospi-nor`,
* :dtcompatible:`nordic,qspi-nor`.

.. zephyr-app-commands::
   :zephyr-app: samples/drivers/spi_flash
   :board: nrf52840dk_nrf52840
   :goals: build flash
   :compact:

Sample Output
=============

.. code-block:: console

   *** Booting Zephyr OS build zephyr-v2.3.0-2142-gca01d2e1d748  ***

   JEDEC QSPI-NOR SPI flash testing
   ==========================

   Test 1: Flash erase
   Flash erase succeeded!

   Test 2: Flash write
   Attempting to write 4 bytes
   Data read matches data written. Good!
