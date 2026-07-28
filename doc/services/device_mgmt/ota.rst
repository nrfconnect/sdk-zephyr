.. _ota:

Over-the-Air Update
###################

Overview
********

Over-the-Air (OTA) Update is a method for delivering firmware updates to remote
devices using a network connection. Although the name implies a wireless
connection, updates received over a wired connection (such as Ethernet)
are still commonly referred to as OTA updates. This approach requires server
infrastructure to host the firmware binary and implement a method of signaling
when an update is available. Security is a concern with OTA updates; firmware
binaries should be cryptographically signed and verified before upgrading.

The :ref:`dfu` section discusses upgrading Zephyr firmware using MCUboot. The
same method can be used as part of OTA. The binary is first downloaded
into an unoccupied code partition, usually named ``slot1_partition``, then
upgraded using the :ref:`mcuboot` process.

nRF Cloud from Nordic Semiconductor
***********************************

`nRF Cloud`_ is Nordic Semiconductor's cloud platform, providing device
management, observability, and location services.

nRF Cloud provides OTA infrastructure and orchestration tools for managing
firmware updates across a device fleet, including cohort-based deployments and
staged rollouts. It also tracks the update process end-to-end through audit
logs, OTA analytics, and configurable alerts.

nRF Cloud OTA support is integrated into the nRF Connect SDK and the
`nRF Connect Device Manager mobile app`_ (available for iOS and Android). Once
onboarded, devices check in with nRF Cloud periodically and can download and
install available updates.

To get started, create an account at `nrfcloud.com <nRF Cloud_>`_.

The nRF91 Series
================

Nordic cellular devices can perform OTA updates out-of-the-box via CoAP or
HTTPS using the nRF Connect SDK and the `Memfault Firmware SDK`_.

For setup instructions, see the `Quickstart Documentation for nRF91 Series`_.

The nRF52, nRF53, and nRF54 Series
==================================

Nordic Bluetooth LE devices can perform OTA updates out-of-the-box using the
Simple Management Protocol (SMP), `Memfault MCUmgr Command Group`_, and the
`Memfault Diagnostics Service (MDS)`_. Paired with the
`nRF Connect Device Manager mobile app`_, it checks for updates and fetches
payloads on behalf of the device.

For setup instructions, see
`Quickstart Documentation for nRF52, nRF53, and nRF54 Series`_.

.. _nRF Cloud: https://nrfcloud.com/#/
.. _nRF Connect Device Manager mobile app: https://www.nordicsemi.com/Products/Development-tools/nRF-Connect-Device-Manager
.. _Memfault Firmware SDK: https://docs.nordicsemi.com/bundle/ncs-latest/page/zephyr/develop/manifest/external/memfault-firmware-sdk.html#external-module-memfault-firmware-sdk
.. _Quickstart Documentation for nRF91 Series: https://docs.nrfcloud.com/docs/mcu/quickstart-nrf9160
.. _Memfault MCUmgr Command Group: https://docs.nrfcloud.com/docs/mcu/mcumgr
.. _Memfault Diagnostics Service (MDS): https://docs.nrfcloud.com/docs/mcu/mds
.. _Quickstart Documentation for nRF52, nRF53, and nRF54 Series: https://docs.nrfcloud.com/docs/mcu/quickstart-nrf5x-ncs
