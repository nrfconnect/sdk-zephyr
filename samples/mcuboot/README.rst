.. _mcuboot_sample:

MCUBoot Sample
##############

Overview
********
A simple hello world sample that is booted by the bootloader
MCUBoot. This sample can be used with any platform that supports
MCUBoot.

Building and Running
********************

This project outputs output from the bootloader and then 'Hello World'
from the application.  It can be built and executed on nRF52_pca10040
as follows:

.. zephyr-app-commands::
   :zephyr-app: samples/mcuboot
   :host-os: unix
   :board: nrf52_pca10040
   :goals: run
   :compact:

Sample Output
=============

.. code-block:: console

    ***** Booting Zephyr OS v1.14.0-rc1-447-gb30b83a6eb *****
    [00:00:00.004,638] <inf> mcuboot: Starting bootloader
    [00:00:00.011,505] <inf> mcuboot: Image 0: magic=unset, copy_done=0x3, image_ok=0x3
    [00:00:00.020,690] <inf> mcuboot: Scratch: magic=unset, copy_done=0xe0, image_ok=0x3
    [00:00:00.029,998] <inf> mcuboot: Boot source: slot 0
    [00:00:00.039,062] <inf> mcuboot: Swap type: none
    [00:00:00.132,904] <inf> mcuboot: Bootloader chainload address offset: 0xc000
    [00:00:00.141,479] <inf> mcuboot: Jumping to the first image slot
    ***** Booting Zephyr OS v1.14.0-rc1-447-gb30b83a6eb *****
    Hello World! nrf52_pca10040
