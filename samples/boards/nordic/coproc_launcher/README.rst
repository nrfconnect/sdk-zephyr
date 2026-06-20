.. zephyr:code-sample:: coproc_launcher
   :name: Nordic coprocessor launcher

   Launch coprocessors.

Overview
********

A sample which allocates all peripherals to, and starts, coprocessors. Board specific overlays can
be added to deallocate all peripherals not needed to boot the coprocessors. The sample is designed
to be combined with snippets to start the required coprocessors, and included in builds for the
target coprocessors using sysbuild.

Building and Running
********************

This application can be built using the following command to start the cpurad core from cpuapp
of an nrf54h20.

.. zephyr-app-commands::
   :zephyr-app: samples/boards/nordic/coproc_launcher
   :host-os: unix
   :board: nrf54h20dk/nrf54h20/cpuapp
   :goals: run
   :gen-args: --snippet nordic-rad --sysbuild
   :compact:

To automatically include the application in a build targeting the cpurad core, the following
command can be used:

.. zephyr-app-commands::
   :zephyr-app: samples/hello_world
   :host-os: unix
   :board: nrf54h20dk/nrf54h20/cpurad
   :goals: run
   :gen-args: --sysbuild
   :compact:
