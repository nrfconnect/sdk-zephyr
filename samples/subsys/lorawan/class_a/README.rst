.. zephyr:code-sample:: lorawan-class-a
   :name: LoRaWAN class A device
   :relevant-api: lorawan_api

   Join a LoRaWAN network and send a message periodically.

Overview
********

A simple application to demonstrate the :ref:`LoRaWAN subsystem <lorawan_api>` of Zephyr.

Building and Running
********************

This sample can be found under
:zephyr_file:`samples/subsys/lorawan/class_a` in the Zephyr tree.

Before building the sample, make sure to select the correct region in the
``prj.conf`` file.

The following commands build and flash the sample.

.. zephyr-app-commands::
   :zephyr-app: samples/subsys/lorawan/class_a
   :board: nucleo_wl55jc
   :goals: build flash
   :compact:

Extended Configuration
**********************

This sample can be configured to run the application-layer clock
synchronization service and/or the remote multicast setup service
in the background.

The following commands build and flash the sample with clock synchronization
enabled.

.. zephyr-app-commands::
   :zephyr-app: samples/subsys/lorawan/class_a
   :board: nucleo_wl55jc
   :goals: build flash
   :gen-args: -DEXTRA_CONF_FILE=overlay-clock-sync.conf
   :compact:

The following commands build and flash the sample with remote multicast setup
enabled.

.. zephyr-app-commands::
   :zephyr-app: samples/subsys/lorawan/class_a
   :board: nucleo_wl55jc
   :goals: build flash
   :gen-args: -DEXTRA_CONF_FILE=overlay-multicast.conf
   :compact:
