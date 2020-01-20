.. _decawave_dwm1001_dev:

Decawave DWM1001
#################

Overview
********

The DWM1001 development board includes the DWM1001 module, battery
connector and charging circuit, LEDs, buttons, Raspberry-Pi and USB
connector. In addition, the board comes with J-Link OB adding
debugging and Virtual COM Port capabilities.

See `Decawave DWM1001-DEV website`_ for more information about the development
board, `Decawave DWM1001 website`_ about the board itself, and `nRF52832 website`_ for the official reference on the IC itself.

Programming and Debugging
*************************

Applications for the ``decawave_dwm1001_dev`` board configuration can be
built and flashed in the usual way (see :ref:`build_an_application`
and :ref:`application_run` for more details); however, the standard
debugging targets are not currently available.

Flashing
========

Follow the instructions in the :ref:`nordic_segger` page to install
and configure all the necessary software. Further information can be
found in :ref:`nordic_segger_flashing`. Then build and flash
applications as usual (see :ref:`build_an_application` and
:ref:`application_run` for more details).

Here is an example for the :ref:`hello_world` application.

First, run your favorite terminal program to listen for output.

.. code-block:: console

   $ minicom -D <tty_device> -b 115200

Replace :code:`<tty_device>` with the port where the board nRF52 DK
can be found. For example, under Linux, :code:`/dev/ttyACM0`.

Then build and flash the application in the usual way.

.. zephyr-app-commands::
   :zephyr-app: samples/hello_world
   :board: decawave_dwm1001_dev
   :goals: build flash

References
**********
.. target-notes::

.. _nRF52832 website: https://www.nordicsemi.com/Products/Low-power-short-range-wireless/nRF52832
.. _Decawave DWM1001 website: https://www.decawave.com/product/dwm1001-module
.. _Decawave DWM1001-DEV website: https://www.decawave.com/product/dwm1001-development-board
