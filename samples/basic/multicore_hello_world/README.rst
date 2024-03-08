.. zephyr:code-sample:: multicore_hello_world
   :name: Multicore Hello World

   Run a hello world sample on multiple cores

Overview
********

The sample demonstrates how to build a multicore Hello World application with
the :ref:`sysbuild`. When building with Zephyr Sysbuild, the build system adds
child images based on the options selected in the project's additional
configuration and build files.

Both the application and remote cores use the same :file:`main.c` that prints
the name of the board on which the application is programmed.

Building and Running
********************

This sample needs to be built with Sysbuild by using the ``--sysbuild`` option.
The remote board needs to be specified using ``SB_CONFIG_REMOTE_BOARD``. Some
additional settings may be required depending on the platform, for example,
to boot the remote core.

.. note::
   It is recommended to use sample setups from
   :zephyr_file:`samples/basic/multicore_hello_world/sample.yaml` using the
   ``-T`` option.

Here's an example to build and flash the sample for the nRF54H20 PDK board,
using application and radio cores:

.. zephyr-app-commands::
   :zephyr-app: samples/basic/multicore_hello_world
   :board: nrf54h20pdk_nrf54h20_cpuapp
   :west-args: --sysbuild
   :gen-args: -DSB_CONFIG_REMOTE_BOARD='"nrf54h20pdk_nrf54h20_cpurad"'
   :goals: build flash
   :compact:

The same can be achieved by using the
:zephyr_file:`samples/basic/multicore_hello_world/sample.yaml` setup:

.. zephyr-app-commands::
   :zephyr-app: samples/basic/multicore_hello_world
   :board: nrf54h20pdk_nrf54h20_cpuapp
   :west-args: -T sample.basic.multicore_hello_world.nrf54h20pdk_cpuapp_cpurad
   :goals: build flash
   :compact:

After programming the sample to your board, you should observe a hello world
message in the Zephyr console configured on each core. For example, for the
sample above:

Application core

   .. code-block:: console

      *** Booting Zephyr OS build v3.6.0-274-g466084bd8c5d ***
      Hello world from nrf54h20pdk_nrf54h20_cpuapp

Radio core

   .. code-block:: console

      *** Booting Zephyr OS build v3.6.0-274-g466084bd8c5d ***
      Hello world from nrf54h20pdk_nrf54h20_cpurad
