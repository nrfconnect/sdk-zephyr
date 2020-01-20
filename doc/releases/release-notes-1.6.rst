.. _zephyr_1.6:

Zephyr Kernel 1.6.0
####################

We are pleased to announce the release of Zephyr kernel version 1.6.0. This
release introduces the unified Kernel replacing the separate nano- and
micro-kernels and simplifying the overall Zephyr architecture and programming
interfaces.
Support for the ARM Cortex-M0/M0+ family was added and board support for
Cortex-M was expanded.
Additionally, this release adds many improvements for documentation, build
infrastructure, and testing.

Major enhancements included with the release:

* Introduced the Unified Kernel; the nano and micro kernel were removed.
* The legacy API is still supported but deprecated.
* Legacy tests and samples were moved to tests/legacy and samples/legacy.
* Unified kernel documentation was added and legacy nanokernel/microkernel
  documentation was removed.
* Added support for several ARM Cortex-M boards
* Added support for USB mass storage and access to the filesystem.
* Added native Bluetooth Controller support. Currently nRF51 & nRF52 are supported.

A detailed list of changes since v1.5.0 by component follows:

Kernel
******

* Introduced the unified kernel.
* Removed deprecated Tasks IRQs.
* Removed deprecated dynamic interrupt API.
* Added DLIST to operate in all elements of a doubly-linked list.
* SLIST: Added sys_slist_get() to fetch and remove the head, also Added
  append_list and merge_slist.
* Added nano_work_pending to check if it is pending execution.
* Unified: Added support for k_malloc and k_free.
* Renamed kernel objects event to alert and memory map to memory slab.
* Changed memory pool, memory maps, message queues and event handling APIs.

Architectures
*************

* ARC: Removed CONFIG_TIMER0_CLOCK_FREQ.
* ARC: Unified linker scripts.
* ARC: Removed dynamic interrupts.
* ARM: Added choice to use floating point ABI.
* ARM: Added NXP Kinetis kconfig options to configure clocks.
* ARM: Removed dynamic interrupts and exceptions.
* ARM: Atmel: Added constants and structures for watchdog registers.
* ARM: Added support for ARM Cortex-M0/M0+.
* x86: Removed dynamic interrupts and exceptions.
* x86: Declared internal API for interrupt controllers.
* x86: Changed IRQ controller to return -1 if cannot determine source vector.
* x86: Grouped Quark SoC's under intel_quark family.
* x86: Optimized and simplified IRQ and exception stubs.

Boards
******

* Renamed board Quark SE devboard to Quark SE C1000 devboard.
* Renamed board Quark SE SSS devboard to Quark SE C1000 SS devboard.
* Quark SE C1000: Disabled IPM and enabled UART0 on the Sensor Subsystem.
* Removed basic_cortex_m3 and basic_minuteia boards.
* Arduino 101: Removed backup/restore scripts. To restore original bootloader
  use flashpack utility instead.
* Renamed nRF52 Nitrogen to 96Boards Nitrogen.
* Added ARM LTD Beetle SoC and V2M Beetle board.
* Added Texas Instruments CC3200 LaunchXL support.
* Added support for Nordic Semiconductor nRF51822.
* Added support for NXP Hexiwear board.

Drivers and Sensors
*******************

* SPI: Fixed typos in SPI port numbers.
* Pinmux: Removed Quark dev unused file.
* I2C: Added KSDK shim driver.
* Ethernet: Added KSDK shim driver.
* Flash: Added KSDK shim driver
* I2C: Changed config parameters to SoC specific.
* QMSI: Implemented suspend and resume functions QMSI shim drivers
* Added HP206C sensor.
* Changed config_info pointers to const.
* Added support for SoCWatch driver.
* Added FXOS8700 accelerometer / magnetometer sensor driver.

Networking
**********

* Minor fixes to uIP networking stack (This will be deprecated in 1.7)

Bluetooth
*********

* Added native Bluetooth Controller support. Currently nRF51 & nRF52 are supported.
* New location for Controller & Host implementations: subsys/bluetooth/
* Added raw HCI API to enable physical HCI transport for a Controller-only build.
* Added sample raw HCI apps for USB and UART.
* Added cross-transport pairing support for the Security Manager Protocol.
* Added RFCOMM support (for Bluetooth Classic)
* Added basic persistent storage support (filesystem-backed)
* Renamed bt_driver API to bt_hci_driver, in anticipation of Bluetooth radio drivers.

Build Infrastructure
********************

* Makefile: Changed outdir into board-specific directory to avoid build collisions.
* Makefile: Changed to use HOST_OS environment variable.
* Makefile: Added support for third party build systems.
* Sanity: Added support to filter using environment variables.
* Sanity: Added support for multiple toolchains.
* Sanity: Added ISSM and ARM GCC embedded toolchains to the supported toolchains.
* Sanity: Added extra arguments to be passed to the build.
* Sanity: Removed linker VMA/LMA offset check.
* Sysgen: Added --kernel_type argument.
* Modified build infrastructure to support unified kernel.
* SDK: Zephyr: Added check for minimum required version.
* Imported get_maintainer.pl from Linux kernel.

Libraries
*********

* libc: Added subset of standard types in inttypes.h.
* libc: Added support for 'z' length specifier.
* libc: Removed stddef.h which is provided by the compiler.
* libc: printf: Improved code for printing.
* printk: Added support for modifiers.
* Added CoAP implementation for Zephyr.
* File system: Added API to grow or shrink a file.
* File system: Added API to get volume statistics.
* File system: Added API to flush cache of an opened file.

HALs
****

* QMSI: Updated to version 1.3.1.
* HAL: Imported CC3200 SDK.
* Imported Nordic MDK nRF51 files.
* Imported Kinetis SDK Ethernet phy driver.
* Imported SDK RNGA driver.

Documentation
*************

* Drivers: Improved Zephyr Driver model.
* Updated device power management API.
* Unified Kernel primer.
* Moved supported board information to the wiki.zephyrproject.org site.
* Revised documentation for Kernel Event logger and Timing.

Test and Samples
****************

* Fixed incorrect printk usage.
* Removed test for dynamic exceptions.
* Added USB sample.
* Added tests and samples for CoAP client and server.
* Added philosophers unified sample.
* Removed printf/printk wrappers.
* Added Unified kernel API samples.
* Imported TinyCrypt test cases for CTR, ECC DSA and ECC DH algorithm.

Deprecations
************

* Deprecated microkernel and nanokernel APIs.
* Removed dynamic IRQs and exceptions.
* Removed Tasks IRQs.

JIRA Related Items
******************

* :jira:`ZEP-308` - Build System cleanup and Kernel / Application build separation
* :jira:`ZEP-334` - Unified Kernel
* :jira:`ZEP-766` - USB Mass Storage access to internal filesystem
* :jira:`ZEP-1090` - CPU x86 save/restore using new QMSI bootloader flow
* :jira:`ZEP-1173` - Add support for bonding remove
* :jira:`ZEP-48` - define API for interrupt controllers
* :jira:`ZEP-181` - Persistent storage APIs
* :jira:`ZEP-233` - Support USB mass storage device class
* :jira:`ZEP-237` - Support pre-built host tools
* :jira:`ZEP-240` - printk/printf usage in samples
* :jira:`ZEP-248` - Add a BOARD/SOC porting guide
* :jira:`ZEP-342` - USB DFU
* :jira:`ZEP-451` - Quark SE output by default redirected to IPM
* :jira:`ZEP-521` - ARM - add choice to floating point ABI selection
* :jira:`ZEP-546` - UART interrupts not triggered on ARC
* :jira:`ZEP-584` - warn user if SDK is out of date
* :jira:`ZEP-592` - Sanitycheck support for multiple toolchains
* :jira:`ZEP-605` - SMP over BR/EDR
* :jira:`ZEP-614` - Port TinyCrypt 2.0 test cases to Zephyr
* :jira:`ZEP-622` - Add FS API to truncate/shrink a file
* :jira:`ZEP-627` - Port Trickle support from Contiki into current stack
* :jira:`ZEP-635` - Add FS API to grow a file
* :jira:`ZEP-636` - Add FS API to get volume total and free space
* :jira:`ZEP-640` - Remove dynamic IRQs/exceptions from Zephyr
* :jira:`ZEP-653` - QMSI shim driver: Watchdog: Implement suspend and resume callbacks
* :jira:`ZEP-654` - QMSI shim driver: I2C: Implement suspend and resume callbacks
* :jira:`ZEP-657` - QMSI shim driver: AONPT: Implement suspend and resume callbacks
* :jira:`ZEP-661` - QMSI shim driver: SPI: Implement suspend and resume callbacks
* :jira:`ZEP-688` - unify duplicated sections of arch linker scripts
* :jira:`ZEP-715` - Add K64F clock configurations
* :jira:`ZEP-716` - Add Hexiwear board support
* :jira:`ZEP-717` - Add ksdk I2C shim driver
* :jira:`ZEP-718` - Add ksdk ethernet shim driver
* :jira:`ZEP-721` - Add FXOS8700 accelerometer/magnetometer sensor driver
* :jira:`ZEP-737` - Update host tools from upstream: fixdep.c
* :jira:`ZEP-740` - PWM API: Check if 'flags' argument is really required
* :jira:`ZEP-745` - Revisit design of PWM Driver API
* :jira:`ZEP-750` - Arduino 101 board should support one configuration using original bootloader
* :jira:`ZEP-758` - Rename Quark SE Devboard to its official name: Quark SE C1000
* :jira:`ZEP-767` - Add FS API to flush cache of an open file
* :jira:`ZEP-775` - Enable USB CDC by default on Arduino 101 and redirect serial to USB
* :jira:`ZEP-783` - ARM Cortex-M0/M0+ support
* :jira:`ZEP-784` - Add support for Nordic Semiconductor nRF51822 SoC
* :jira:`ZEP-850` - remove obsolete boards basic_minuteia and basic_cortex_m3
* :jira:`ZEP-906` - [unified] Add scheduler time slicing support
* :jira:`ZEP-907` - Test memory pool support (with mailboxes)
* :jira:`ZEP-908` - Add task offload to fiber support
* :jira:`ZEP-909` - Adapt tickless idle + power management for ARM
* :jira:`ZEP-910` - Adapt tickless idle for x86
* :jira:`ZEP-912` - Finish renaming kernel object types
* :jira:`ZEP-916` - Eliminate kernel object API anomalies
* :jira:`ZEP-920` - Investigate malloc/free support
* :jira:`ZEP-921` - Miscellaneous documentation work
* :jira:`ZEP-922` - Revise documentation for Kernel Event Logger
* :jira:`ZEP-923` - Revise documentation for Timing
* :jira:`ZEP-924` - Revise documentation for Interrupts
* :jira:`ZEP-925` - API changes to message queues
* :jira:`ZEP-926` - API changes to memory pools
* :jira:`ZEP-927` - API changes to memory maps
* :jira:`ZEP-928` - API changes to event handling
* :jira:`ZEP-930` - Cut over to unified kernel
* :jira:`ZEP-933` - Unified kernel ARC port
* :jira:`ZEP-934` - NIOS_II port
* :jira:`ZEP-935` - Kernel logger support (validation)
* :jira:`ZEP-954` - Update device PM API to allow setting additional power states
* :jira:`ZEP-957` - Create example sample for new unified kernel API usage
* :jira:`ZEP-959` - sync checkpatch.pl with upstream Linux
* :jira:`ZEP-966` - need support for EM7D SOC on em_starterkit
* :jira:`ZEP-975` - DNS client port to new IP stack
* :jira:`ZEP-981` - Add doxygen documentation to both include/kernel.h and include/legacy.h
* :jira:`ZEP-989` - Cache next ready thread instead of finding out the long way
* :jira:`ZEP-993` - Quark SE (x86): Refactor save/restore execution context feature
* :jira:`ZEP-994` - Quark SE (ARC): Add PMA sample
* :jira:`ZEP-996` - Refactor save/restore feature from i2c_qmsi driver
* :jira:`ZEP-997` - Refactor save/restore feature from spi_qmsi driver
* :jira:`ZEP-998` - Refactor save/restore feature from uart_qmsi driver
* :jira:`ZEP-999` - Refactor save/restore feature from gpio_qmsi driver
* :jira:`ZEP-1000` - Refactor save/restore feature from rtc_qmsi driver
* :jira:`ZEP-1001` - Refactor save/restore feature from wdt_qmsi driver
* :jira:`ZEP-1002` - Refactor save/restore feature from counter_qmsi_aonpt driver
* :jira:`ZEP-1004` - Extend counter_qmsi_aon driver to support save/restore peripheral context
* :jira:`ZEP-1005` - Extend dma_qmsi driver to support save/restore peripheral context
* :jira:`ZEP-1006` - Extend soc_flash_qmsi driver to support save/restore peripheral context
* :jira:`ZEP-1008` - Extend pwm_qmsi driver to support save/restore peripheral context
* :jira:`ZEP-1023` - workq in Kernel primer for unified kernel
* :jira:`ZEP-1030` - Enable QMSI shim drivers of SoC peripherals on the sensor subsystem
* :jira:`ZEP-1043` - Update QMSI to 1.2
* :jira:`ZEP-1045` - Add/Enhance shim layer to wrap SOC specific PM implementations
* :jira:`ZEP-1046` - Implement RAM sharing between bootloader and Zephyr
* :jira:`ZEP-1047` - Adapt to new PM related boot flow changes in QMSI boot loader
* :jira:`ZEP-1106` - Fix all test failures from TCF
* :jira:`ZEP-1107` - Update QMSI to 1.3
* :jira:`ZEP-1109` - Texas Instruments CC3200 LaunchXL Support
* :jira:`ZEP-1119` - move top level usb/ to sys/usb
* :jira:`ZEP-1120` - move top level fs/ to sys/fs
* :jira:`ZEP-1121` - Add config support for enabling SoCWatch in Zephyr
* :jira:`ZEP-1140` - Add a unified kernel version of power_mgr sample app for testing PM code with the new kernel
* :jira:`ZEP-1188` - Add an API to retrieve pending interrupts for wake events
* :jira:`ZEP-1191` - Create wiki page for Hexiwear board
* :jira:`ZEP-1235` - Basic shell support for file system browsing
* :jira:`ZEP-1245` - ARM LTD V2M Beetle Support
* :jira:`ZEP-1313` - porting and user guides must include a security section
* :jira:`ZEP-1386` - Revise power management document to reflect latest changes
* :jira:`ZEP-199` - Zephyr driver model is undocumented
* :jira:`ZEP-436` - Test case tests/kernel/test_mem_safe fails on ARM hardware
* :jira:`ZEP-471` - Ethernet packet with multicast address is not working
* :jira:`ZEP-472` - Ethernet packets are getting missed if sent in quick succession.
* :jira:`ZEP-517` - build on windows failed "zephyr/Makefile:869: \*\*\* multiple target patterns"
* :jira:`ZEP-528` - ARC has 2 almost identical copies of the linker script
* :jira:`ZEP-577` - Sample application source does not compile on Windows
* :jira:`ZEP-601` - enable CONFIG_DEBUG_INFO
* :jira:`ZEP-602` - unhandled CPU exceptions/interrupts report wrong faulting vector if triggered by CPU
* :jira:`ZEP-615` - Un-supported flash erase size listed in SPI flash w25qxxdv driver header file
* :jira:`ZEP-639` - device_pm_ops structure should be defined as static
* :jira:`ZEP-686` - docs: Info in "Application Development Primer" and "Developing an Application and the Build System" is largely duplicated
* :jira:`ZEP-698` - samples/task_profiler issues
* :jira:`ZEP-707` - mem_safe test stomps on top of .data and bottom of .noinit
* :jira:`ZEP-724` - build on windows failed: 'make: execvp: uname: File or path name too long'
* :jira:`ZEP-733` - Minimal libc shouldn't be providing stddef.h
* :jira:`ZEP-762` - unexpected "abspath" and "notdir" from mingw make system
* :jira:`ZEP-777` - samples/driver/i2c_stts751: kconfig build warning from "select DMA_QMSI"
* :jira:`ZEP-778` - Samples/drivers/i2c_lsm9ds0: kconfig build warning from "select DMA_QMSI"
* :jira:`ZEP-779` - Using current MinGW gcc version 5.3.0 breaks Zephyr build on Windows
* :jira:`ZEP-845` - UART for ARC on Arduino 101 behaves unexpectedly
* :jira:`ZEP-905` - hello_world compilation for arduino_due target fails when using CROSS_COMPILE
* :jira:`ZEP-940` - Fail to get ATT response
* :jira:`ZEP-950` - USB: Device is not listed by USB20CV test suite
* :jira:`ZEP-961` - samples: other cases cannot execute after run aon_counter case
* :jira:`ZEP-967` - Sanity doesn't build 'samples/usb/dfu' with assertions (-R)
* :jira:`ZEP-970` - Sanity doesn't build 'tests/kernel/test_build' with assertions (-R)
* :jira:`ZEP-982` - Minimal libc has EWOULDBLOCK != EAGAIN
* :jira:`ZEP-1014` - [TCF] tests/bluetooth/init build fail
* :jira:`ZEP-1025` - Unified kernel build sometimes breaks on a missing .d dependency file.
* :jira:`ZEP-1027` - Documentation for GCC ARM is not accurate
* :jira:`ZEP-1031` - qmsi: dma: driver test fails with LLVM
* :jira:`ZEP-1048` - grove_lcd sample: sample does not work if you disable serial
* :jira:`ZEP-1051` - mpool allocation failed after defrag twice...
* :jira:`ZEP-1062` - Unified kernel isn't compatible with CONFIG_NEWLIB_LIBC
* :jira:`ZEP-1074` - ATT retrying misbehaves when ATT insufficient Authentication is received
* :jira:`ZEP-1076` - "samples/philosophers/unified" build failed with dynamic stack
* :jira:`ZEP-1077` - "samples/philosophers/unified" build warnings with NUM_PHIL<6
* :jira:`ZEP-1079` - Licensing not clear for imported components
* :jira:`ZEP-1097` - ENC28J60 driver fails on concurrent tx and rx
* :jira:`ZEP-1098` - ENC28J60 fails to receive big data frames
* :jira:`ZEP-1100` - Current master still identifies itself as 1.5.0
* :jira:`ZEP-1101` - SYS_KERNEL_VER_PATCHLEVEL() and friends artificially limit version numbers to 4 bits
* :jira:`ZEP-1124` - tests/kernel/test_sprintf/microkernel/testcase.ini#test failure on frdm_k64f
* :jira:`ZEP-1130` - region 'RAM' overflowed occurs while building test_hmac_prng
* :jira:`ZEP-1138` - Received packets not being passed to upper layer from IP stack when using ENC28J60 driver
* :jira:`ZEP-1139` - Fix build error when power management is built with unified kernel
* :jira:`ZEP-1141` - TinyCrypt SHA256 test fails with system crash using unified kernel type
* :jira:`ZEP-1144` - TinyCrypt AES128 fixed-key with variable-text test fails using unified kernel type
* :jira:`ZEP-1145` - system hang after TinyCrypt HMAC test
* :jira:`ZEP-1146` - zephyrproject.org home page needs technical scrub for 1.6 release
* :jira:`ZEP-1149` - port ztest framework to unified kernel
* :jira:`ZEP-1154` - tests/samples failing with unified kernel
* :jira:`ZEP-1155` - Fix filesystem API namespace
* :jira:`ZEP-1163` - LIB_INCLUDE_DIR is clobbered in Makefile second pass
* :jira:`ZEP-1164` - ztest skip waiting the test case to finish its execution
* :jira:`ZEP-1179` - Build issues when compiling with LLVM from ISSM (icx)
* :jira:`ZEP-1182` - kernel.h doxygen show unexpected "asm" blocks
* :jira:`ZEP-1183` - btshell return "panic: errcode -1" when init bt
* :jira:`ZEP-1195` - Wrong ATT error code passed to the application
* :jira:`ZEP-1199` - [L2CAP] No credits to receive packet
* :jira:`ZEP-1219` - [L2CAP] Data sent exceeds maximum PDU size
* :jira:`ZEP-1221` - Connection Timeout during pairing
* :jira:`ZEP-1226` - cortex M7 port assembler error
* :jira:`ZEP-1227` - ztest native testing not working in unified kernel
* :jira:`ZEP-1232` - Daily build is failing asserts
* :jira:`ZEP-1234` - Removal of fiber* APIs due to unified migration breaks USB mass storage patchset
* :jira:`ZEP-1247` - Test tests/legacy/benchmark/latency_measure is broken for daily sanitycheck
* :jira:`ZEP-1252` - Test test_chan_blen_transfer does not build for quark_d2000_crb
* :jira:`ZEP-1277` - Flash driver (w25qxxdv) erase function is not checking for offset alignment
* :jira:`ZEP-1278` - Incorrect boundary check in flash driver (w25qxxdv) for erase offset
* :jira:`ZEP-1287` - ARC SPI 1 Port is not working
* :jira:`ZEP-1289` - Race condition with k_sem_take
* :jira:`ZEP-1291` - libzephyr.a dependency on phony "gcc" target
* :jira:`ZEP-1293` - ENC28J60 driver doesn't work on Arduino 101
* :jira:`ZEP-1295` - incorrect doxygen comment in kernel.h:k_work_pending()
* :jira:`ZEP-1297` - test/legacy/kernel/test_mail: failure on ARC platforms
* :jira:`ZEP-1299` - System can't resume completely with DMA suspend and resume operation
* :jira:`ZEP-1302` - ENC28J60 fails with rx/tx of long frames
* :jira:`ZEP-1303` - Configuration talks about >32 thread prios, but the kernel does not support it
* :jira:`ZEP-1309` - ARM uses the end of memory for its init stack
* :jira:`ZEP-1310` - ARC uses the end of memory for its init stack
* :jira:`ZEP-1312` - ARC: software crashed at k_mbox_get() with async sending a message
* :jira:`ZEP-1319` - Zephyr is unable to compile when CONFIG_RUNTIME_NMI is enabled on ARM platforms
* :jira:`ZEP-1341` - power_states test app passes wrong value as power state to post_ops functions
* :jira:`ZEP-1343` - tests/drivers/pci_enum: failing on QEMU ARM and X86 due to missing commit
* :jira:`ZEP-1345` - cpu context save and restore could corrupt stack
* :jira:`ZEP-1349` - ARC sleep needs to pass interrupt priority threshold when interrupts are enabled
* :jira:`ZEP-1353` - FDRM k64f Console output broken on normal flash mode

Known Issues
************

* :jira:`ZEP-1405` - function l2cap_br_conn_req in /subsys/bluetooth/host/l2cap_br.c
  references uninitialized pointer
