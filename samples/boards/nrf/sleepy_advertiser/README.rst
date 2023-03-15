.. _nrf-sleepy-advertiser-sample:

Sleepy Advertiser
#################

Overview
********

This sample is based on the system_off demo and the beacon demo.

It can be used for power measurements when advertising.
On Nordic platforms, deep sleep is entered after advertising.
On nRF21540 systems, it can be used to verify the state of FEM controls pins.

The default behavior is:
* Use immediate logging
* Advertise for 30 seconds with console UART suspended
* Turn the system off

Requirements
************

* A board with BLE support

Building, Flashing and Running
******************************

.. zephyr-app-commands::
   :zephyr-app: samples/nrf/sleepy_advertiser
   :board: nrf21540dk_nrf52840
   :goals: build flash
   :compact:

Running:

1. Open UART terminal.
2. Power Cycle Device.
3. Device will advertise. Current can be measured. RF power can be measured.
4. Device will turn itself off and current can be measured.

Mock BL5340PA FEM control on nrf21540dk_nrf52840:

west build -p -b nrf21540dk_nrf52840 -- -DMOCK_LCZ_FEM=y

nRF5340DK with nRF21540 Shield:

west build -p -b nrf5340dk_nrf5340_cpuapp -- -DSHIELD=nrf21540_ek_fwd -Dhci_rpmsg_SHIELD=nrf21540_ek

Mock BL5340PA FEM control on nRF5340DK with nRF21540 Shield with UART logging:

west build -p -b nrf5340dk_nrf5340_cpuapp -- -DSHIELD=nrf21540_ek_fwd -Dhci_rpmsg_SHIELD=nrf21540_ek -DMOCK_LCZ_FEM=y -DLOG_TYPE=uart

Disable advertisement and select internal antenna

west build -p -b bl5340pa_dvk_cpuapp -- -Dhci_rpmsg_CONFIG_LCZ_FEM_INTERNAL_ANTENNA=y -DCONFIG_ADVERTISE=n

Sample Output
=================

.. code-block:: console

   *** Booting Zephyr OS build v3.2.99-ncs1-1553-gcc118a4a4e52 ***
   <inf> main: nrf21540dk_nrf52840 BT sleepy advertiser
   <inf> main: Bluetooth ready: 0
   <inf> main: Advertising start: 0
   <inf> main: Sleep 30 s with UART off
   <inf> main: suspend: 0, resume: 0
   <inf> main: Entering system off; press reset button to restart
