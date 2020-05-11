.. _tfm_ipc:

TF-M IPC
########

Overview
********

This is a simple TF-M integration example that can be used with an ARMv8-M
supported board.

It uses **IPC Mode** for communication, where TF-M API calls are made to the
secure image via an IPC mechanism, as opposed to **Library Mode**, where the
IPC mechanism is bypassed in favor of direct calls.

Zephyr uses Trusted Firmware (TF-M) Platform Security Architecture (PSA) APIs
to run this sample in a secure configuration, with Zephyr itself running in a
non-secure configuration.

The sample prints test info to the console either as a single-thread or
multi-thread application.

Building and Running
********************

This project outputs test status and info to the console. It can be built and
executed on MPS2+ AN521.

On MPS2+ AN521:
===============

#. Build Zephyr with a non-secure configuration (``-DBOARD=mps2_an521_nonsecure``).

   .. code-block:: bash

      cd $ZEPHYR_ROOT/samples/tfm_integration/tfm_ipc/
      mkdir build
      cd build
      cmake -DBOARD=mps2_an521_nonsecure ..
      make

You can also use west as follows:

   .. code-block:: bash

      $ west build -p -b mps2_an521_nonsecure zephyr/samples/tfm_integration/tfm_ipc


#. Copy application binary files (mcuboot.bin and tfm_sign.bin) to ``<MPS2 device name>/SOFTWARE/``.
#. Open ``<MPS2 device name>/MB/HBI0263C/AN521/images.txt``. Edit the ``AN521/images.txt`` file as follows:

   .. code-block:: bash

      TITLE: Versatile Express Images Configuration File

      [IMAGES]
      TOTALIMAGES: 2 ;Number of Images (Max: 32)

      IMAGE0ADDRESS: 0x10000000
      IMAGE0FILE: \SOFTWARE\mcuboot.bin  ; BL2 bootloader

      IMAGE1ADDRESS: 0x10080000
      IMAGE1FILE: \SOFTWARE\tfm_sign.bin ; TF-M with application binary blob

#. Reset MPS2+ board.

If you get the following error when running cmake:

   .. code-block:: bash

      CMake Error at cmake/Common/FindGNUARM.cmake:121 (message):
      arm-none-eabi-gcc can not be found.  Either put arm-none-eabi-gcc on the
      PATH or set GNUARM_PATH properly.

You may need to edit the ``CMakeLists.txt`` file in the ``tfm_ipc`` project
folder to update the ``-DGNUARM_PATH=/opt/toolchain/arm-none-eabi`` path.

On QEMU:
========

The MPS2+ AN521 target (``mps2_an521_nonsecure``), which is based on a
dual core ARM Cortex-M33 setup, also allows you to run TF-M tests using QEMU if
you don't have access to a supported ARMv8-M development board.

As part of the normal build process above, a binary is also produced that can
be run via ``qemu-system-arm``. The binary can be executed as follows:

   .. code-block:: bash

      qemu-system-arm -M mps2-an521 -device loader,file=tfm_qemu.hex -serial stdio

You can also run the binary as part of the ``west`` build process by appending
the ``-t run`` flag to the end of your build command, or in the case of
ninja or make, adding the ``run`` commands:

   .. code-block:: bash

      $ west build -b mps2_an521_nonsecure zephyr/samples/tfm_integration/tfm_ipc -t run

Or, post build:

   .. code-block:: bash

      $ ninja run

Sample Output
=============

.. code-block:: console

   [INF] Starting bootloader
   [INF] Swap type: none
   [INF] Bootloader chainload address offset: 0x80000
   [INF] Jumping to the first image slot
   [Sec Thread] Secure image initializing!
   TFM level is: 1 [Sec Thread] Jumping to non-secure code...
   **** Booting Zephyr OS build zephyr-v1.14.0-2904-g89616477b115 ****
   The version of the PSA Framework API is 256.
   The minor version is 1.
   Connect success!
   TFM service support minor version is 1.
   psa_call is successful!
   outvec1 is: It is just for IPC call test.
   outvec2 is: It is just for IPC call test.
   Connect success!
   Call IPC_INIT_BASIC_TEST service Pass Connect success!
   Call PSA RoT access APP RoT memory test service Pass
   TF-M IPC on (.*)


.. _TF-M build instruction:
   https://git.trustedfirmware.org/trusted-firmware-m.git/tree/docs/user_guides/tfm_build_instruction.rst

.. _TF-M secure boot:
   https://git.trustedfirmware.org/trusted-firmware-m.git/tree/docs/user_guides/tfm_secure_boot.rst
