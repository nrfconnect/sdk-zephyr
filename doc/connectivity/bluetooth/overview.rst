.. _bluetooth-overview:

Overview
########

.. contents::
    :local:
    :depth: 2

Since its inception, Zephyr has had a strong focus on Bluetooth and, in
particular, on Bluetooth Low Energy (BLE). Through the contributions of
several companies and individuals involved in existing open source
implementations of the Bluetooth specification (Linux's BlueZ) as well as the
design and development of BLE radio hardware, the protocol stack in Zephyr has
grown to be mature and feature-rich, as can be seen in the section below.

Supported Features
******************

Zephyr comes integrated with a feature-rich and highly configurable
Bluetooth stack.

* Bluetooth v5.3 compliant

  * Highly configurable

      * Features, buffer sizes/counts, stack sizes, etc.

  * Portable to all architectures supported by Zephyr (including big and
    little endian, alignment flavors and more)

  * Support for all combinations of Host and Controller builds:

    * Controller-only (HCI) over UART, SPI, USB and IPC physical transports
    * Host-only over UART, SPI, and IPC (shared memory)
    * Combined (Host + Controller)

* Bluetooth-SIG qualified

  * Controller on Nordic Semiconductor hardware
  * Conformance tests run regularly on all layers

* Bluetooth Low Energy Controller support (LE Link Layer)

  * Unlimited role and connection count, all roles supported
  * All v5.3 specification features supported (except a few minor items)
  * Concurrent multi-protocol support ready
  * Intelligent scheduling of roles to minimize overlap
  * Portable design to any open BLE radio, currently supports Nordic
    Semiconductor nRF51 and nRF52, as well as proprietary radios
  * Supports little and big endian architectures, and abstracts the hard
    real-time specifics so that they can be encapsulated in a hardware-specific
    module
  * Support for Controller (HCI) builds over different physical transports

* Bluetooth Host support

  * Generic Access Profile (GAP) with all possible LE roles

    * Peripheral & Central
    * Observer & Broadcaster
    * Multiple PHY support (2Mbit/s, Coded)
    * Extended Advertising
    * Periodic Advertising (including Sync Transfer)

  * GATT (Generic Attribute Profile)

    * Server (to be a sensor)
    * Client (to connect to sensors)
    * Enhanced ATT (EATT)
    * GATT Database Hash
    * GATT Multiple Notifications

  * Pairing support, including the Secure Connections feature from Bluetooth 4.2

  * Non-volatile storage support for permanent storage of Bluetooth-specific
    settings and data

  * Bluetooth mesh support

    * Relay, Friend Node, Low-Power Node (LPN) and GATT Proxy features
    * Both Provisioning roles and bearers supported (PB-ADV & PB-GATT)
    * Foundation Models included
    * Highly configurable, fits as small as 16k RAM devices

  * IPSP/6LoWPAN for IPv6 connectivity over Bluetooth LE

    * IPSP node sample application

  * Basic Bluetooth BR/EDR (Classic) support

    * Generic Access Profile (GAP)
    * Logical Link Control and Adaptation Protocol (L2CAP)
    * Serial Port emulation (RFCOMM protocol)
    * Service Discovery Protocol (SDP)

  * Clean HCI driver abstraction

    * 3-Wire (H:5) & 5-Wire (H:4) UART
    * SPI
    * Local controller support as a virtual HCI driver

  * Verified with multiple popular controllers

* LE Audio in Host and Controller

  * Isochronous channels

    * Full Host support
    * Initial Controller support

  * Generic Audio Framework

    * Basic Audio Profile

      * Unicast server and client
      * Broadcast source and sink
      * Broadcast assistant
      * Scan delegator

    * Common Audio Profile

      * Acceptor

    * Volume Control Profile, Microphone Control Profile
    * Call Control Profile, Media Control Profile
    * Coordinated Set Identification Profile
