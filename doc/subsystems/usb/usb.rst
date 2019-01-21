.. _usb_device_stack:

USB device stack
################

.. contents::
   :depth: 2
   :local:
   :backlinks: top

USB Vendor and Product identifiers
**********************************

The USB Vendor ID for the Zephyr project is 0x2FE3. The default USB Product
ID for the Zephyr project is 0x100. The USB bcdDevice Device Release Number
represents the Zephyr kernel major and minor versions as a binary coded
decimal value. When a vendor integrates the Zephyr USB subsystem into a
product, the vendor must use the USB Vendor and Product ID assigned to them.
A vendor integrating the Zephyr USB subsystem in a product must not use the
Vendor ID of the Zephyr project.

The USB maintainer, if one is assigned, or otherwise the Zephyr Technical
Steering Committee, may allocate other USB Product IDs based on well-motivated
and documented requests.

USB device controller drivers
*****************************

The Device Controller Driver Layer implements the low level control routines
to deal directly with the hardware. All device controller drivers should
implement the APIs described in file usb_dc.h. This allows the integration of
new USB device controllers to be done without changing the upper layers.

USB device core layer
*********************

The USB Device core layer is a hardware independent interface between USB
device controller driver and USB device class drivers or customer applications.
It's a port of the LPCUSB device stack. It provides the following
functionalities:

   * Responds to standard device requests and returns standard descriptors,
     essentially handling 'Chapter 9' processing, specifically the standard
     device requests in table 9-3 from the universal serial bus specification
     revision 2.0.
   * Provides a programming interface to be used by USB device classes or
     customer applications. The APIs are described in the usb_device.h file.
   * Uses the APIs provided by the device controller drivers to interact with
     the USB device controller.

USB device class drivers
************************

To initialize the device class driver instance the USB device class driver
should call :c:func:`usb_set_config()` passing as parameter the instance's
configuration structure.

For example, for USB loopback application:

.. literalinclude:: ../../../subsys/usb/class/loopback.c
   :language: c
   :start-after: usb.rst config structure start
   :end-before: usb.rst config structure end
   :linenos:

Endpoint configuration:

.. literalinclude:: ../../../subsys/usb/class/loopback.c
   :language: c
   :start-after: usb.rst endpoint configuration start
   :end-before: usb.rst endpoint configuration end
   :linenos:

USB Device configuration structure:

.. literalinclude:: ../../../subsys/usb/class/loopback.c
   :language: c
   :start-after: usb.rst device config data start
   :end-before: usb.rst device config data end
   :linenos:

For the Composite USB Device configuration is done by composite layer,
otherwise:

.. literalinclude:: ../../../subsys/usb/class/loopback.c
   :language: c
   :start-after: usb.rst configure USB controller start
   :end-before: usb.rst configure USB controller end
   :linenos:

To enable the USB device for host/device connection:

.. literalinclude:: ../../../subsys/usb/class/loopback.c
   :language: c
   :start-after: usb.rst enable USB controller start
   :end-before:  usb.rst enable USB controller end
   :linenos:

The vendor device requests are forwarded by the USB stack core driver to the
class driver through the registered class handler.

For the loopback class driver, :c:func:`loopback_vendor_handler` processes
the vendor requests:

.. literalinclude:: ../../../subsys/usb/class/loopback.c
   :language: c
   :start-after: usb.rst vendor handler start
   :end-before:  usb.rst vendor handler end
   :linenos:

The class driver waits for the :makevar:`USB_DC_CONFIGURED` device status code
before transmitting any data.

Further reading
***************

More information on the stack and its usage can be found in the following
subsections:

.. toctree::
   :maxdepth: 2

   ../../api/usb_api.rst
