.. _bluetooth-samples:

Bluetooth samples
#################

To build any of the Bluetooth samples, follow the same steps as building
any other Zephyr application. Refer to :ref:`bluetooth-dev` for more information.

Many Bluetooth samples can be run on QEMU or Native POSIX with support for
external Bluetooth Controllers. Refer to the :ref:`bluetooth-hw-setup` section
for further details.

Several of the bluetooth samples will build a Zephyr-based Controller that can
then be used with any external Host (including Zephyr running natively or with
QEMU or Native POSIX), those are named accordingly with an "HCI" prefix in the
documentation and are prefixed with :literal:`hci_` in their folder names.

.. note::
   The mutually-shared encryption key created during host-device paring may get
   old after many test iterations.  If this happens, subsequent host-device
   connections will fail. You can force a re-paring and new key to be created
   by removing the device from the associated devices list on the host.

.. toctree::
   :maxdepth: 1
   :glob:

   **/*
