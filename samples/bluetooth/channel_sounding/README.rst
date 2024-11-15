.. zephyr:code-sample:: ble_cs
   :name: Channel Sounding
   :relevant-api: bt_gap bluetooth

   Use Channel Sounding functionality.


Bluetooth: Channel Sounding
###########################

Overview
********

These samples demonstrates how to use the Bluetooth Channel Sounding feature.

The CS Test sample shows how to us the CS test command to override randomization of certain channel
sounding parameters, experiment with different configurations, or evaluate the RF medium. It can
be found under :zephyr_file:`samples/bluetooth/channel_sounding/cs_test`.

The connected CS sample shows how to set up regular channel sounding procedures on a connection
between two devices.
It can be found under :zephyr_file:`samples/bluetooth/channel_sounding/connected_cs`

A basic distance estimation algorithm is included in both.
The Channel Sounding feature does not mandate a specific algorithm for computing distance estimates,
but the mathematical representation described in [#phase_and_amplitude]_ and [#rtt_packets]_ is used
as a starting point for these samples.

Distance estimation using channel sounding requires data from two devices, and for that reason
the channel sounding results in the sample are exchanged in a simple way using a GATT characteristic.
This limits the amount of data that can be processed at once to about 512 bytes from each device,
which is enough to estimate distance using a handful of RTT timings and PBR phase samples across
about 35-40 channels, assuming a single antenna path.

Both samples will perform channel sounding procedures repeatedly and print regular distance estimates to
the console. They are designed assuming a single subevent per procedure.

Requirements
************

* A board with BLE support
* A controller that supports the Channel Sounding feature

Building and Running
********************

These samples can be found under :zephyr_file:`samples/bluetooth/channel_sounding` in
the Zephyr tree.

See :zephyr:code-sample-category:`bluetooth` samples for details.

References
**********

.. [#phase_and_amplitude] Bluetooth Core Specification v. 6.0: Vol. 1, Part A, 9.2
.. [#rtt_packets] Bluetooth Core Specification v. 6.0: Vol. 1, Part A, 9.3
