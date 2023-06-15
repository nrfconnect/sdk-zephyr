.. _usb_device_stack:

USB device support
##################

.. contents::
    :local:
    :depth: 3

Overview
********

The USB device stack is a hardware independent interface between USB
device controller driver and USB device class drivers or customer applications.
It is a port of the LPCUSB device stack and has been modified and expanded
over time. It provides the following functionalities:

* Uses the :ref:`usb_dc_api` provided by the device controller drivers to interact with
  the USB device controller.
* Responds to standard device requests and returns standard descriptors,
  essentially handling 'Chapter 9' processing, specifically the standard
  device requests in table 9-3 from the universal serial bus specification
  revision 2.0.
* Provides a programming interface to be used by USB device classes or
  customer applications. The APIs is described in
  :zephyr_file:`include/zephyr/usb/usb_device.h`

The device stack and :ref:`usb_dc_api` have some limitations, such as not being
able to support more than one controller instance at runtime and only supporting
one USB device configuration. We are actively working on new USB support, which
means we will continue to maintain the device stack described here until all
supported USB classes are ported, but do not expect any new features or enhancements.

Supported USB classes
*********************

Audio
=====

There is an experimental implementation of the Audio class. It follows specification
version 1.00 (``bcdADC 0x0100``) and supports synchronous synchronisation type only.
See :ref:`usb_audio_headphones_microphone` and :ref:`usb_audio_headset` for reference.

Bluetooth HCI USB transport layer
=================================

Bluetooth HCI USB transport layer implementation uses :ref:`bt_hci_raw`
to expose HCI interface to the host. It is not fully in line with the description
in the Bluetooth specification and consists only of an interface with the endpoint
configuration:

* HCI commands through control endpoint (host-to-device only)
* HCI events through interrupt IN endpoint
* ACL data through one bulk IN and one bulk OUT endpoints

A second interface for the voice channels has not been implemented as there is
no support for this type in :ref:`bluetooth`. It is not a big problem under Linux
if HCI USB transport layer is the only interface that appears in the configuration,
the btusb driver would not try to claim a second (isochronous) interface.
The consequence is that if HCI USB is used in a composite configuration and is
the first interface, then the Linux btusb driver will claim both the first and
the next interface, preventing other composite functions from working.
Because of this problem, HCI USB should not be used in a composite configuration.
This problem is fixed in the implementation for new USB support.

See :ref:`bluetooth-hci-usb-sample` sample for reference.

.. _usb_device_cdc_acm:

CDC ACM
=======

The CDC ACM class is used as backend for different subsystems in Zephyr.
However, its configuration may not be easy for the inexperienced user.
Below is a description of the different use cases and some pitfalls.

The interface for CDC ACM user is :ref:`uart_api` driver API.
But there are two important differences in behavior to a real UART controller:

* Data transfer is only possible after the USB device stack has been
  initialized and started, until then any data is discarded
* If device is connected to the host, it still needs an application
  on the host side which requests the data

The devicetree compatible property for CDC ACM UART is
:dtcompatible:`zephyr,cdc-acm-uart`.
CDC ACM support is automatically selected when USB device support is enabled
and a compatible node in the devicetree sources is present. If necessary,
CDC ACM support can be explicitly disabled by :kconfig:option:`CONFIG_USB_CDC_ACM`.
About four CDC ACM UART instances can be defined and used,
limited by the maximum number of supported endpoints on the controller.

CDC ACM UART node is supposed to be child of a USB device controller node.
Since the designation of the controller nodes varies from vendor to vendor,
and our samples and application should be as generic as possible,
the default USB device controller is usually assigned an ``zephyr_udc0``
node label. Often, CDC ACM UART is described in a devicetree overlay file
and looks like this:

.. code-block:: devicetree

	&zephyr_udc0 {
		cdc_acm_uart0: cdc_acm_uart0 {
			compatible = "zephyr,cdc-acm-uart";
			label = "CDC_ACM_0";
		};
	};

Samples :ref:`usb_cdc-acm` and :ref:`usb_hid-cdc` have similar overlay files.
And since no special properties are present, it may seem overkill to use
devicetree to describe CDC ACM UART.  The motivation behind using devicetree
is the easy interchangeability of a real UART controller and CDC ACM UART
in applications.

Console over CDC ACM UART
-------------------------

With the CDC ACM UART node from above and ``zephyr,console`` property of the
chosen node, we can describe that CDC ACM UART is to be used with the console.
A similar overlay file is used by :ref:`cdc-acm-console`.

.. code-block:: devicetree

	/ {
		chosen {
			zephyr,console = &cdc_acm_uart0;
		};
	};

	&zephyr_udc0 {
		cdc_acm_uart0: cdc_acm_uart0 {
			compatible = "zephyr,cdc-acm-uart";
			label = "CDC_ACM_0";
		};
	};

Before the application uses the console, it is recommended to wait for
the DTR signal:

.. code-block:: c

	const struct device *const dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));
	uint32_t dtr = 0;

	if (usb_enable(NULL)) {
		return;
	}

	while (!dtr) {
		uart_line_ctrl_get(dev, UART_LINE_CTRL_DTR, &dtr);
		k_sleep(K_MSEC(100));
	}

	printk("nuqneH\n");

CDC ACM UART as backend
-----------------------

As for the console sample, it is possible to configure CDC ACM UART as
backend for other subsystems by setting :ref:`devicetree-chosen-nodes`
properties.

List of few Zephyr specific chosen properties which can be used to select
CDC ACM UART as backend for a subsystem or application:

* ``zephyr,bt-c2h-uart`` used in Bluetooth,
  for example see :ref:`bluetooth-hci-uart-sample`
* ``zephyr,ot-uart`` used in OpenThread,
  for example see :ref:`coprocessor-sample`
* ``zephyr,shell-uart`` used by shell for serial backend,
  for example see :zephyr_file:`samples/subsys/shell/shell_module`
* ``zephyr,uart-mcumgr`` used by :ref:`smp_svr_sample`

DFU
===

USB DFU class implementation is tightly coupled to :ref:`dfu` and :ref:`mcuboot_api`.
This means that the target platform must support the :ref:`flash_img_api` API.

See :ref:`usb_dfu` for reference.

USB Human Interface Devices (HID) support
=========================================

HID support abuses :ref:`device_model_api` simply to allow applications to use
the :c:func:`device_get_binding`. Note that there is no HID device API as such,
instead the interface is provided by :c:struct:`hid_ops`.
The default instance name is ``HID_n``, where n can be {0, 1, 2, ...} depending on
the :kconfig:option:`CONFIG_USB_HID_DEVICE_COUNT`.

Each HID instance requires a HID report descriptor. The interface to the core
and the report descriptor must be registered using :c:func:`usb_hid_register_device`.

As the USB HID specification is not only used by the USB subsystem, the USB HID API
reference is split into two parts, :ref:`usb_hid_common` and :ref:`usb_hid_device`.
HID helper macros from :ref:`usb_hid_common` should be used to compose a
HID report descriptor. Macro names correspond to those used in the USB HID specification.

For the HID class interface, an IN interrupt endpoint is required for each instance,
an OUT interrupt endpoint is optional. Thus, the minimum implementation requirement
for :c:struct:`hid_ops` is to provide ``int_in_ready`` callback.

.. code-block:: c

	#define REPORT_ID		1
	static bool configured;
	static const struct device *hdev;

	static void int_in_ready_cb(const struct device *dev)
	{
		static uint8_t report[2] = {REPORT_ID, 0};

		if (hid_int_ep_write(hdev, report, sizeof(report), NULL)) {
			LOG_ERR("Failed to submit report");
		} else {
			report[1]++;
		}
	}

	static void status_cb(enum usb_dc_status_code status, const uint8_t *param)
	{
		if (status == USB_DC_RESET) {
			configured = false;
		}

		if (status == USB_DC_CONFIGURED && !configured) {
			int_in_ready_cb(hdev);
			configured = true;
		}
	}

	static const uint8_t hid_report_desc[] = {
		HID_USAGE_PAGE(HID_USAGE_GEN_DESKTOP),
		HID_USAGE(HID_USAGE_GEN_DESKTOP_UNDEFINED),
		HID_COLLECTION(HID_COLLECTION_APPLICATION),
		HID_LOGICAL_MIN8(0x00),
		HID_LOGICAL_MAX16(0xFF, 0x00),
		HID_REPORT_ID(REPORT_ID),
		HID_REPORT_SIZE(8),
		HID_REPORT_COUNT(1),
		HID_USAGE(HID_USAGE_GEN_DESKTOP_UNDEFINED),
		HID_INPUT(0x02),
		HID_END_COLLECTION,
	};

	static const struct hid_ops my_ops = {
		.int_in_ready = int_in_ready_cb,
	};

	int main(void)
	{
		int ret;

		hdev = device_get_binding("HID_0");
		if (hdev == NULL) {
			return -ENODEV;
		}

		usb_hid_register_device(hdev, hid_report_desc, sizeof(hid_report_desc),
					&my_ops);

		ret = usb_hid_init(hdev);
		if (ret) {
			return ret;
		}

		return usb_enable(status_cb);
	}


If the application wishes to receive output reports via the OUT interrupt endpoint,
it must enable :kconfig:option:`CONFIG_ENABLE_HID_INT_OUT_EP` and provide
``int_out_ready`` callback.
The disadvantage of this is that Kconfig options such as
:kconfig:option:`CONFIG_ENABLE_HID_INT_OUT_EP` or
:kconfig:option:`CONFIG_HID_INTERRUPT_EP_MPS` apply to all instances. This design
issue will be fixed in the HID class implementation for the new USB support.

See :ref:`usb_hid` or :ref:`usb_hid-mouse` for reference.

Mass Storage Class
==================

MSC follows Bulk-Only Transport specification and uses :ref:`disk_access_api` to
access and expose a RAM disk, emulated block device on a flash partition,
or SD Card to the host. Only one disk instance can be exported at a time.

The disc to be used by the implementation is set by the
:kconfig:option:`CONFIG_MASS_STORAGE_DISK_NAME` and should be equal to one
of the options used by the disc access driver that the application wants to expose to
the host, :kconfig:option:`CONFIG_DISK_RAM_VOLUME_NAME`,
:kconfig:option:`CONFIG_MMC_VOLUME_NAME`, or :kconfig:option:`CONFIG_SDMMC_VOLUME_NAME`.

For the emulated block device on a flash partition, the flash partition and
flash disk to be used must be described in the devicetree. If a storage partition
is already described at the board level, application devicetree overlay must also
delete ``storage_partition`` node first. :kconfig:option:`CONFIG_MASS_STORAGE_DISK_NAME`
should be the same as ``disk-name`` property.

.. code-block:: devicetree

	/delete-node/ &storage_partition;

	&mx25r64 {
		partitions {
			compatible = "fixed-partitions";
			#address-cells = <1>;
			#size-cells = <1>;

			storage_partition: partition@0 {
				label = "storage";
				reg = <0x00000000 0x00020000>;
			};
		};
	};

	/ {
		msc_disk0 {
			compatible = "zephyr,flash-disk";
			partition = <&storage_partition>;
			disk-name = "NAND";
			cache-size = <4096>;
		};
	};

The ``disk-property`` "NAND" may be confusing, but it is simply how some file
systems identifies the disc. Therefore, if the application also accesses the
file system on the exposed disc, default names should be used, see
:ref:`usb_mass` for reference.

Networking
==========

There are three implementations that work in a similar way, providing a virtual
Ethernet connection between the remote (USB host) and Zephyr network support.

* CDC ECM class, enabled with :kconfig:option:`CONFIG_USB_DEVICE_NETWORK_ECM`
* CDC EEM class, enabled with :kconfig:option:`CONFIG_USB_DEVICE_NETWORK_EEM`
* RNDIS support, enabled with :kconfig:option:`CONFIG_USB_DEVICE_NETWORK_RNDIS`

See :ref:`zperf-sample` or :ref:`sockets-dumb-http-server-sample` for reference.
Typically, users will need to add a configuration file overlay to the build,
such as :zephyr_file:`samples/net/zperf/overlay-netusb.conf`.

Applications using RNDIS support should enable :kconfig:option:`CONFIG_USB_DEVICE_OS_DESC`
for a better user experience on a host running Microsoft Windows OS.

Binary Device Object Store (BOS) support
****************************************

BOS handling can be enabled with Kconfig option :kconfig:option:`CONFIG_USB_DEVICE_BOS`.
This option also has the effect of changing device descriptor ``bcdUSB`` to ``0210``.
The application should register descriptors such as Capability Descriptor
using :c:func:`usb_bos_register_cap`. Registered descriptors are added to the root
BOS descriptor and handled by the stack.

See :ref:`webusb-sample` for reference.

Implementing a non-standard USB class
*************************************

The configuration of USB device is done in the stack layer.

The following structures and callbacks need to be defined:

* Part of USB Descriptor table
* USB Endpoint configuration table
* USB Device configuration structure
* Endpoint callbacks
* Optionally class, vendor and custom handlers

For example, for the USB loopback application:

.. literalinclude:: ../../../../subsys/usb/device/class/loopback.c
   :language: c
   :start-after: usb.rst config structure start
   :end-before: usb.rst config structure end
   :linenos:

Endpoint configuration:

.. literalinclude:: ../../../../subsys/usb/device/class/loopback.c
   :language: c
   :start-after: usb.rst endpoint configuration start
   :end-before: usb.rst endpoint configuration end
   :linenos:

USB Device configuration structure:

.. literalinclude:: ../../../../subsys/usb/device/class/loopback.c
   :language: c
   :start-after: usb.rst device config data start
   :end-before: usb.rst device config data end
   :linenos:


The vendor device requests are forwarded by the USB stack core driver to the
class driver through the registered vendor handler.

For the loopback class driver, :c:func:`loopback_vendor_handler` processes
the vendor requests:

.. literalinclude:: ../../../../subsys/usb/device/class/loopback.c
   :language: c
   :start-after: usb.rst vendor handler start
   :end-before:  usb.rst vendor handler end
   :linenos:

The class driver waits for the :makevar:`USB_DC_CONFIGURED` device status code
before transmitting any data.

.. _testing_USB_native_posix:

Testing over USPIP in native_posix
***********************************

A virtual USB controller implemented through USBIP might be used to test the USB
device stack. Follow the general build procedure to build the USB sample for
the native_posix configuration.

Run built sample with:

.. code-block:: console

   west build -t run

In a terminal window, run the following command to list USB devices:

.. code-block:: console

   $ usbip list -r localhost
   Exportable USB devices
   ======================
    - 127.0.0.1
           1-1: unknown vendor : unknown product (2fe3:0100)
              : /sys/devices/pci0000:00/0000:00:01.2/usb1/1-1
              : (Defined at Interface level) (00/00/00)
              :  0 - Vendor Specific Class / unknown subclass / unknown protocol (ff/00/00)

In a terminal window, run the following command to attach the USB device:

.. code-block:: console

   $ sudo usbip attach -r localhost -b 1-1

The USB device should be connected to your Linux host, and verified with the
following commands:

.. code-block:: console

   $ sudo usbip port
   Imported USB devices
   ====================
   Port 00: <Port in Use> at Full Speed(12Mbps)
          unknown vendor : unknown product (2fe3:0100)
          7-1 -> usbip://localhost:3240/1-1
              -> remote bus/dev 001/002
   $ lsusb -d 2fe3:0100
   Bus 007 Device 004: ID 2fe3:0100

USB Vendor and Product identifiers
**********************************

The USB Vendor ID for the Zephyr project is ``0x2FE3``.
This USB Vendor ID must not be used when a vendor
integrates Zephyr USB device support into its own product.

Each USB :ref:`sample<usb-samples>` has its own unique Product ID.
The USB maintainer, if one is assigned, or otherwise the Zephyr Technical
Steering Committee, may allocate other USB Product IDs based on well-motivated
and documented requests.

The following Product IDs are currently used:

+-------------------------------------+--------+
| Sample                              | PID    |
+=====================================+========+
| :ref:`usb_cdc-acm`                  | 0x0001 |
+-------------------------------------+--------+
| :ref:`usb_cdc-acm_composite`        | 0x0002 |
+-------------------------------------+--------+
| :ref:`usb_hid-cdc`                  | 0x0003 |
+-------------------------------------+--------+
| :ref:`cdc-acm-console`              | 0x0004 |
+-------------------------------------+--------+
| :ref:`usb_dfu`                      | 0x0005 |
+-------------------------------------+--------+
| :ref:`usb_hid`                      | 0x0006 |
+-------------------------------------+--------+
| :ref:`usb_hid-mouse`                | 0x0007 |
+-------------------------------------+--------+
| :ref:`usb_mass`                     | 0x0008 |
+-------------------------------------+--------+
| :ref:`testusb-app`                  | 0x0009 |
+-------------------------------------+--------+
| :ref:`webusb-sample`                | 0x000A |
+-------------------------------------+--------+
| :ref:`bluetooth-hci-usb-sample`     | 0x000B |
+-------------------------------------+--------+
| :ref:`bluetooth-hci-usb-h4-sample`  | 0x000C |
+-------------------------------------+--------+
| :ref:`wpanusb-sample`               | 0x000D |
+-------------------------------------+--------+

The USB device descriptor field ``bcdDevice`` (Device Release Number) represents
the Zephyr kernel major and minor versions as a binary coded decimal value.
