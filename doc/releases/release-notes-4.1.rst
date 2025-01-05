:orphan:

.. _zephyr_4.1:

Zephyr 4.1.0 (Working Draft)
############################

We are pleased to announce the release of Zephyr version 4.1.0.

Major enhancements with this release include:

An overview of the changes required or recommended when migrating your application from Zephyr
v4.0.0 to Zephyr v4.1.0 can be found in the separate :ref:`migration guide<migration_4.1>`.

The following sections provide detailed lists of changes by component.

Security Vulnerability Related
******************************
The following CVEs are addressed by this release:

More detailed information can be found in:
https://docs.zephyrproject.org/latest/security/vulnerabilities.html

API Changes
***********

Removed APIs in this release
============================

Deprecated in this release
==========================

Architectures
*************

* ARC

* ARM

* ARM64

* RISC-V

* Xtensa

Kernel
******

Bluetooth
*********

* Audio

* Host

  * :kconfig:option:`CONFIG_BT_BUF_ACL_RX_COUNT` has been deprecated and
    :kconfig:option:`CONFIG_BT_BUF_ACL_RX_COUNT_EXTRA` has been added.

* HCI Drivers

Boards & SoC Support
********************

* Added support for these SoC series:

* Made these changes in other SoC series:

* Added support for these boards:

* Made these board changes:

* Added support for the following shields:

Build system and Infrastructure
*******************************

Drivers and Sensors
*******************

* ADC

* Battery

* CAN

  * :kconfig:option:`CONFIG_MBEDTLS_PSA_STATIC_KEY_SLOTS`
  * :kconfig:option:`CONFIG_MBEDTLS_PSA_KEY_SLOT_COUNT`

* Other

  * :kconfig:option:`CONFIG_BT_BUF_ACL_RX_COUNT_EXTRA`
  * :c:macro:`DT_ANY_INST_HAS_BOOL_STATUS_OKAY`
  * :c:struct:`led_dt_spec`
  * :kconfig:option:`CONFIG_STEP_DIR_STEPPER`

New Boards
**********
..
  You may update this list as you contribute a new board during the release cycle, in order to make
  is visible to people who might be looking at the working draft of the release notes. However, note
  that this list will be recomputed at the time of the release, so you don't *have* to update it.
  In any case, just link the board, further details go in the board description.

* Adafruit Industries, LLC

   * :zephyr:board:`adafruit_feather_m4_express` (``adafruit_feather_m4_express``)
   * :zephyr:board:`adafruit_qt_py_esp32s3` (``adafruit_qt_py_esp32s3``)

* Advanced Micro Devices (AMD), Inc.

   * :zephyr:board:`acp_6_0_adsp` (``acp_6_0_adsp``)

* Analog Devices, Inc.

   * :zephyr:board:`max78000evkit` (``max78000evkit``)
   * :zephyr:board:`max78000fthr` (``max78000fthr``)
   * :zephyr:board:`max78002evkit` (``max78002evkit``)

* Antmicro

   * :zephyr:board:`myra_sip_baseboard` (``myra_sip_baseboard``)

* BeagleBoard.org Foundation

   * :zephyr:board:`beagley_ai` (``beagley_ai``)

* FANKE Technology Co., Ltd.

   * :zephyr:board:`fk750m1_vbt6` (``fk750m1_vbt6``)

* Google, Inc.

   * :zephyr:board:`google_icetower` (``google_icetower``)
   * :zephyr:board:`google_quincy` (``google_quincy``)

* Infineon Technologies

   * :zephyr:board:`cy8ckit_062s2_ai` (``cy8ckit_062s2_ai``)

* Lilygo Shenzhen Xinyuan Electronic Technology Co., Ltd

   * :zephyr:board:`ttgo_t7v1_5` (``ttgo_t7v1_5``)
   * :zephyr:board:`ttgo_t8s3` (``ttgo_t8s3``)

* M5Stack

   * :zephyr:board:`m5stack_cores3` (``m5stack_cores3``)

* Makerbase Co., Ltd.

   * :zephyr:board:`mks_canable_v20` (``mks_canable_v20``)

* MediaTek Inc.

   * MT8186 (``mt8186``)
   * MT8188 (``mt8188``)
   * MT8196 (``mt8196``)

* NXP Semiconductors

   * :zephyr:board:`mimxrt700_evk` (``mimxrt700_evk``)

* Nordic Semiconductor

   * :zephyr:board:`nrf54l09pdk` (``nrf54l09pdk``)

* Norik Systems

   * :zephyr:board:`octopus_io_board` (``octopus_io_board``)
   * :zephyr:board:`octopus_som` (``octopus_som``)

* Qorvo, Inc.

   * :zephyr:board:`decawave_dwm3001cdk` (``decawave_dwm3001cdk``)

* Raspberry Pi Foundation

   * :zephyr:board:`rpi_pico2` (``rpi_pico2``)

* Realtek Semiconductor Corp.

   * :zephyr:board:`rts5912_evb` (``rts5912_evb``)

* Renesas Electronics Corporation

   * :zephyr:board:`fpb_ra4e1` (``fpb_ra4e1``)
   * :zephyr:board:`rzg3s_smarc` (``rzg3s_smarc``)
   * :zephyr:board:`voice_ra4e1` (``voice_ra4e1``)

* STMicroelectronics

   * :zephyr:board:`nucleo_c071rb` (``nucleo_c071rb``)
   * :zephyr:board:`nucleo_f072rb` (``nucleo_f072rb``)
   * :zephyr:board:`nucleo_h7s3l8` (``nucleo_h7s3l8``)
   * :zephyr:board:`nucleo_wb07cc` (``nucleo_wb07cc``)
   * :zephyr:board:`stm32f413h_disco` (``stm32f413h_disco``)

* Seeed Technology Co., Ltd

   * :zephyr:board:`xiao_esp32c6` (``xiao_esp32c6``)

* Shenzhen Fuyuansheng Electronic Technology Co., Ltd.

   * :zephyr:board:`ucan` (``ucan``)

* Silicon Laboratories

   * :zephyr:board:`xg23_rb4210a` (``xg23_rb4210a``)
   * :zephyr:board:`xg24_ek2703a` (``xg24_ek2703a``)
   * :zephyr:board:`xg29_rb4412a` (``xg29_rb4412a``)

* Toradex AG

   * :zephyr:board:`verdin_imx8mm` (``verdin_imx8mm``)

* Waveshare Electronics

   * :zephyr:board:`rp2040_zero` (``rp2040_zero``)

* WeAct Studio

   * :zephyr:board:`mini_stm32h7b0` (``mini_stm32h7b0``)

* WinChipHead

   * :zephyr:board:`ch32v003evt` (``ch32v003evt``)

* Würth Elektronik GmbH.

   * :zephyr:board:`we_oceanus1ev` (``we_oceanus1ev``)
   * :zephyr:board:`we_orthosie1ev` (``we_orthosie1ev``)

* others

   * :zephyr:board:`canbardo` (``canbardo``)
   * :zephyr:board:`candlelight` (``candlelight``)
   * :zephyr:board:`candlelightfd` (``candlelightfd``)
   * :zephyr:board:`esp32c3_supermini` (``esp32c3_supermini``)
   * :zephyr:board:`promicro_nrf52840` (``promicro_nrf52840``)

New Drivers
***********
..
  Same as above for boards, this will also be recomputed at the time of the release.
  Just link the driver, further details go in the binding description

* :abbr:`ADC (Analog to Digital Converter)`

   * :dtcompatible:`adi,ad4114-adc`
   * :dtcompatible:`ti,ads131m02`
   * :dtcompatible:`ti,tla2022`
   * :dtcompatible:`ti,tla2024`
   * :dtcompatible:`ti,ads114s06`
   * :dtcompatible:`ti,ads124s06`
   * :dtcompatible:`ti,ads124s08`

* ARM architecture

   * :dtcompatible:`nxp,nbu`

* Bluetooth

   * :dtcompatible:`renesas,bt-hci-da1453x`
   * :dtcompatible:`st,hci-stm32wb0`

* Clock control

* Counter

* DAC

* Disk

* Display

* Ethernet

* Flash

* GNSS

* GPIO

* Hardware info

* I2C

* I2S

* I3C

* Input

* LED

* LED Strip

* LoRa

* Mailbox

* MDIO

* MFD

* Modem

* MIPI-DBI

* MSPI

* Pin control

* PWM

* Regulators

* Reset

* RTC

* RTIO

* SDHC

* Sensors

* Serial

* SPI

* USB

* Video

* Watchdog

* Wi-Fi

Networking
**********

* ARP:

* CoAP:

* Connection manager:

* DHCPv4:

* DHCPv6:

* DNS/mDNS/LLMNR:

* gPTP/PTP:

* HTTP:

* IPSP:

* IPv4:

* IPv6:

* LwM2M:

* Misc:

* MQTT:

* Network Interface:

* OpenThread

* PPP

* Shell:

* Sockets:

* Syslog:

* TCP:

* Websocket:

* Wi-Fi:

* zperf:

USB
***

Devicetree
**********

Kconfig
*******

Libraries / Subsystems
**********************

* Debug

* Demand Paging

* Formatted output

* Management

* Logging

* Modem modules

* Power management

* Crypto

* CMSIS-NN

* FPGA

* Random

* SD

* State Machine Framework

* Storage

* Task Watchdog

* POSIX API

* LoRa/LoRaWAN

* ZBus

HALs
****

* Nordic

* STM32

* ADI

* Espressif

MCUboot
*******

OSDP
****

Trusted Firmware-M
******************

LVGL
****

Tests and Samples
*****************

Issue Related Items
*******************

Known Issues
============
