.. _cmsis_rtos_v2-sync_sample:

Synchronization using CMSI RTOS V2 APIs
#######################################

Overview
********
The sample project illustrates usage of timers and message queues using
CMSIS RTOS V2 APIs.

The main thread creates a preemptive thread which writes message to message queue
and on timer expiry, message is read by main thread.


Building and Running Project
****************************
This project outputs to the console.  It can be built and executed
on QEMU as follows:

.. zephyr-app-commands::
   :zephyr-app: samples/philosophers
   :host-os: unix
   :board: qemu_x86
   :goals: run
   :compact:

Sample Output
=============

.. code-block:: console

    Wrote to message queue: 5
    Read from message queue: 5

    Wrote to message queue: 6
    Read from message queue: 6

    Wrote to message queue: 7
    Read from message queue: 7

    Wrote to message queue: 8
    Read from message queue: 8

    Wrote to message queue: 9
    Read from message queue: 9

    Wrote to message queue: 10
    Read from message queue: 10

    Wrote to message queue: 11
    Read from message queue: 11

    Wrote to message queue: 12
    Read from message queue: 12

    Wrote to message queue: 13
    Read from message queue: 13

    Wrote to message queue: 14
    Read from message queue: 14

    Wrote to message queue: 15
    Read from message queue: 15

    Sample execution successful
