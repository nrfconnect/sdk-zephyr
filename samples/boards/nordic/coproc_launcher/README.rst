.. zephyr:code-sample:: coproc_launcher
   :name: Nordic coprocessor launcher

   Launch coprocessors.

Overview
********

A sample which allocated all peripherals to, and starts, coprocessors. Board specific overlays can
be added to deallocate all peripherals not needed to boot the coprocessors. The sample is designed
to be combined with snippets to start the required coprocessors, and included in builds for the
target coprocessors using sysbuild.

Building and Running
********************

This application can be built and executed on QEMU as follows:

.. zephyr-app-commands::
   :zephyr-app: samples/board/nordic/coproc_launcher
   :host-os: unix
   :board: nrf54h20dk/nrf54h20/cpuapp
   :goals: run
   :gen-args: --snippet nordic-ppr-xip
   :compact:
