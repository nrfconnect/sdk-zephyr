.. _dt-bindings:

Devicetree bindings
###################

A devicetree on its own is only half the story for describing hardware, as it
is a relatively unstructured format. *Devicetree bindings* provide the other
half.

A devicetree binding declares requirements on the contents of nodes, and
provides semantic information about the contents of valid nodes. Zephyr
devicetree bindings are YAML files in a custom format (Zephyr does not use the
dt-schema tools used by the Linux kernel).

This page introduces bindings, describes what they do, notes where they are
found, and explains their data format.

.. note::

   See the :ref:`devicetree_binding_index` for reference information on
   bindings built in to Zephyr.

.. contents::
   :local:
   :depth: 2

.. _dt-binding-compat:

Introduction
************

Devicetree nodes are matched to bindings using their :ref:`compatible
properties <dt-important-props>`.

During the :ref:`build_configuration_phase`, the build system tries to match
each node in the devicetree to a binding file. When this succeeds, the build
system uses the information in the binding file both when validating the node's
contents and when generating macros for the node.

.. _dt-bindings-simple-example:

A simple example
================

Here is an example devicetree node:

.. code-block:: devicetree

   /* Node in a DTS file */
   bar-device {
   	compatible = "foo-company,bar-device";
        num-foos = <3>;
   };

Here is a minimal binding file which matches the node:

.. code-block:: yaml

   # A YAML binding matching the node

   compatible: "foo-company,bar-device"

   properties:
     num-foos:
       type: int
       required: true

The build system matches the ``bar-device`` node to its YAML binding because
the node's ``compatible`` property matches the binding's ``compatible:`` line.

What the build system does with bindings
========================================

The build system uses bindings both to validate devicetree nodes and to convert
the devicetree's contents into the generated :ref:`devicetree_generated.h
<dt-outputs>` header file.

For example, the build system would use the above binding to check that the
required ``num-foos`` property is present in the ``bar-device`` node, and that
its value, ``<3>``, has the correct type.

The build system will then generate a macro for the ``bar-device`` node's
``num-foos`` property, which will expand to the integer literal ``3``. This
macro lets you get the value of the property in C code using the API which is
discussed later in this guide in :ref:`dt-from-c`.

For another example, the following node would cause a build error, because it
has no ``num-foos`` property, and this property is marked required in the
binding:

.. code-block:: devicetree

   bad-node {
   	compatible = "foo-company,bar-device";
   };

Other ways nodes are matched to bindings
========================================

If a node has more than one string in its ``compatible`` property, the build
system looks for compatible bindings in the listed order and uses the first
match.

Take this node as an example:

.. code-block:: devicetree

   baz-device {
   	compatible = "foo-company,baz-device", "generic-baz-device";
   };

The ``baz-device`` node would get matched to a binding with a ``compatible:
"generic-baz-device"`` line if the build system can't find a binding with a
``compatible: "foo-company,baz-device"`` line.

Nodes without compatible properties can be matched to bindings associated with
their parent nodes. These are called "child bindings". If a node describes
hardware on a bus, like I2C or SPI, then the bus type is also taken into
account when matching nodes to bindings. (The :ref:`dt-bindings-file-syntax`
section below describes how to write child bindings and bus-specific bindings.)

Some special nodes without ``compatible`` properties are matched to
:ref:`dt-inferred-bindings`. For these nodes, the build system generates macros
based on the properties in the final devicetree.

.. _dt-where-bindings-are-located:

Where bindings are located
==========================

Binding file names usually match their ``compatible:`` lines. For example, the
above example binding would be named :file:`foo-company,bar-device.yaml` by
convention.

The build system looks for bindings in :file:`dts/bindings`
subdirectories of the following places:

- the zephyr repository
- your :ref:`application source directory <application>`
- your :ref:`board directory <board_porting_guide>`
- any directories in the :ref:`DTS_ROOT <dts_root>` CMake variable
- any :ref:`module <modules>` that defines a ``dts_root`` in its
  :ref:`modules_build_settings`

The build system will consider any YAML file in any of these, including in any
subdirectories, when matching nodes to bindings. A file is considered YAML if
its name ends with ``.yaml`` or ``.yml``.

.. warning::

   The binding files must be located somewhere inside the :file:`dts/bindings`
   subdirectory of the above places.

   For example, if :file:`my-app` is your application directory, then you must
   place application-specific bindings inside :file:`my-app/dts/bindings`. So
   :file:`my-app/dts/bindings/serial/my-company,my-serial-port.yaml` would be
   found, but :file:`my-app/my-company,my-serial-port.yaml` would be ignored.

.. _dt-bindings-file-syntax:

Bindings file syntax
********************

Zephyr bindings files are YAML files. The top-level value in the file is a
mapping. A :ref:`simple example <dt-bindings-simple-example>` is given above.

The top-level keys in the mapping look like this:

.. code-block:: yaml

   # A high level description of the device the binding applies to:
   description: |
      This is the Vendomatic company's foo-device.

      Descriptions which span multiple lines (like this) are OK,
      and are encouraged for complex bindings.

      See https://yaml-multiline.info/ for formatting help.

   # You can include definitions from other bindings using this syntax:
   include: other.yaml

   # Used to match nodes to this binding as discussed above:
   compatible: "manufacturer,foo-device"

   properties:
     # Requirements for and descriptions of the properties that this
     # binding's nodes need to satisfy go here.

   child-binding:
     # You can constrain the children of the nodes matching this binding
     # using this key.

   # If the node describes bus hardware, like an SPI bus controller
   # on an SoC, use 'bus:' to say which one, like this:
   bus: spi

   # If the node instead appears as a device on a bus, like an external
   # SPI memory chip, use 'on-bus:' to say what type of bus, like this.
   # Like 'compatible', this key also influences the way nodes match
   # bindings.
   on-bus: spi

   foo-cells:
     # "Specifier" cell names for the 'foo' domain go here; example 'foo'
     # values are 'gpio', 'pwm', and 'dma'. See below for more information.

The following sections describe these keys in more detail:

- :ref:`dt-bindings-description`
- :ref:`dt-bindings-compatible`
- :ref:`dt-bindings-properties`
- :ref:`dt-bindings-child`
- :ref:`dt-bindings-bus`
- :ref:`dt-bindings-on-bus`
- :ref:`dt-bindings-cells`
- :ref:`dt-bindings-include`

The ``include:`` key usually appears early in the binding file, but it is
documented last here because you need to know how the other keys work before
understanding ``include:``.

.. _dt-bindings-description:

Description
===========

A free-form description of node hardware goes here. You can put links to
datasheets or example nodes or properties as well.

.. _dt-bindings-compatible:

Compatible
==========

This key is used to match nodes to this binding as described above.
It should look like this in a binding file:

.. code-block:: YAML

   # Note the comma-separated vendor prefix and device name
   compatible: "manufacturer,device"

This devicetree node would match the above binding:

.. code-block:: devicetree

   device {
   	compatible = "manufacturer,device";
   };

Assuming no binding has ``compatible: "manufacturer,device-v2"``, it would also
match this node:

.. code-block:: devicetree

    device-2 {
        compatible = "manufacturer,device-v2", "manufacturer,device";
    };

Each node's ``compatible`` property is tried in order. The first matching
binding is used. The :ref:`on-bus: <dt-bindings-on-bus>` key can be used to
refine the search.

If more than one binding for a compatible is found, an error is raised.

The ``manufacturer`` prefix identifies the device vendor. See
:zephyr_file:`dts/bindings/vendor-prefixes.txt` for a list of accepted vendor
prefixes. The ``device`` part is usually from the datasheet.

Some bindings apply to a generic class of devices which do not have a specific
vendor. In these cases, there is no vendor prefix. One example is the
:dtcompatible:`gpio-leds` compatible which is commonly used to describe board
LEDs connected to GPIOs.

If more than one binding for a compatible is found, an error is raised.

.. _dt-bindings-properties:

Properties
==========

The ``properties:`` key describes the properties that nodes which match the
binding can contain.

For example, a binding for a UART peripheral might look something like this:

.. code-block:: YAML

   compatible: "manufacturer,serial"

   properties:
     reg:
       type: array
       description: UART peripheral MMIO register space
       required: true
     current-speed:
       type: int
       description: current baud rate
       required: true

The properties in the following node would be validated by the above binding:

.. code-block:: devicetree

   my-serial@deadbeef {
   	compatible = "manufacturer,serial";
   	reg = <0xdeadbeef 0x1000>;
   	current-speed = <115200>;
   };

This is used to check that required properties appear, and to control the
format of output generated for them.

Except for some special properties, like ``reg``, whose meaning is defined by
the devicetree specification itself, only properties listed in the
``properties:`` key will have generated macros.

.. _dt-bindings-example-properties:

Example property definitions
++++++++++++++++++++++++++++

Here are some more examples.

.. code-block:: YAML

   properties:
       # Describes a property like 'current-speed = <115200>;'. We pretend that
       # it's obligatory for the example node and set 'required: true'.
       current-speed:
           type: int
           required: true
           description: Initial baud rate for bar-device

       # Describes an optional property like 'keys = "foo", "bar";'
       keys:
           type: string-array
           description: Keys for bar-device

       # Describes an optional property like 'maximum-speed = "full-speed";'
       # the enum specifies known values that the string property may take
       maximum-speed:
           type: string
           description: Configures USB controllers to work up to a specific speed.
           enum:
              - "low-speed"
              - "full-speed"
              - "high-speed"
              - "super-speed"

       # Describes an optional property like 'resolution = <16>;'
       # the enum specifies known values that the int property may take
       resolution:
         type: int
         enum:
          - 8
          - 16
          - 24
          - 32

       # Describes a required property '#address-cells = <1>';  the const
       # specifies that the value for the property is expected to be the value 1
       "#address-cells":
           type: int
           required: true
           const: 1

       int-with-default:
           type: int
           default: 123
           description: Value for int register, default is power-up configuration.

       array-with-default:
           type: array
           default: [1, 2, 3] # Same as 'array-with-default = <1 2 3>'

       string-with-default:
           type: string
           default: "foo"

       string-array-with-default:
           type: string-array
           default: ["foo", "bar"] # Same as 'string-array-with-default = "foo", "bar"'

       uint8-array-with-default:
           type: uint8-array
           default: [0x12, 0x34] # Same as 'uint8-array-with-default = [12 34]'

Property entry syntax
+++++++++++++++++++++

As shown by the above examples, each property entry in a binding looks like
this:

.. code-block:: none

   <property name>:
     required: <true | false>
     type: <string | int | boolean | array | uint8-array | string-array |
            phandle | phandles | phandle-array | path | compound>
     deprecated: <true | false>
     default: <default>
     description: <description of the property>
     enum:
       - <item1>
       - <item2>
       ...
       - <itemN>
     const: <string | int | array | uint8-array | string-array>

Required properties
+++++++++++++++++++

If a node matches a binding but is missing any property which the binding
defines with ``required: true``, the build fails.

Note: A property is implicitly optional unless ``required: true`` is
specified. Using ``required: false`` is therefore redundant and strongly
discouraged.

Property types
++++++++++++++

The type of a property constrains its values.
The following types are available. See :ref:`dt-writing-property-values`
for more details about writing values of each type in a DTS file.

.. list-table::
   :header-rows: 1
   :widths: 1 3 2

   * - Type
     - Description
     - Example in DTS

   * - ``string``
     - exactly one string
     - ``status = "disabled";``

   * - ``int``
     - exactly one 32-bit value (cell)
     - ``current-speed = <115200>;``

   * - ``boolean``
     - flags that don't take a value when true, and are absent if false
     - ``hw-flow-control;``

   * - ``array``
     - zero or more 32-bit values (cells)
     - ``offsets = <0x100 0x200 0x300>;``

   * - ``uint8-array``
     - zero or more bytes, in hex ('bytestring' in the Devicetree specification)
     - ``local-mac-address = [de ad be ef 12 34];``

   * - ``string-array``
     - zero or more strings
     - ``dma-names = "tx", "rx";``

   * - ``phandle``
     - exactly one phandle
     - ``interrupt-parent = <&gic>;``

   * - ``phandles``
     - zero or more phandles
     - ``pinctrl-0 = <&usart2_tx_pd5 &usart2_rx_pd6>;``

   * - ``phandle-array``
     - a list of phandles and 32-bit cells (usually specifiers)
     - ``dmas = <&dma0 2>, <&dma0 3>;``

   * - ``path``
     - a path to a node as a phandle path reference or path string
     - ``zephyr,bt-c2h-uart = &uart0;`` or
       ``foo = "/path/to/some/node";``

   * - ``compound``
     - a catch-all for more complex types (no macros will be generated)
     - ``foo = <&label>, [01 02];``

Deprecated properties
+++++++++++++++++++++

A property with ``deprecated: true`` indicates to both the user and the tooling
that the property is meant to be phased out.

The tooling will report a warning if the devicetree includes the property that
is flagged as deprecated. (This warning is upgraded to an error in the
:ref:`twister_script` for upstream pull requests.)

.. _dt-bindings-default:

Default values for properties
+++++++++++++++++++++++++++++

The optional ``default:`` setting gives a value that will be used if the
property is missing from the devicetree node.

For example, with this binding fragment:

.. code-block:: YAML

   properties:
     foo:
       type: int
       default: 3

If property ``foo`` is missing in a matching node, then the output will be as
if ``foo = <3>;`` had appeared in the DTS (except YAML data types are used for
the default value).

Note that combining ``default:`` with ``required: true`` will raise an error.

For rules related to ``default`` in upstream Zephyr bindings, see
:ref:`dt-bindings-default-rules`.

See :ref:`dt-bindings-example-properties` for examples. Putting ``default:`` on
any property type besides those used in the examples will raise an error.

Enum values
+++++++++++

The ``enum:`` line is followed by a list of values the property may contain. If
a property value in DTS is not in the ``enum:`` list in the binding, an error
is raised. See :ref:`dt-bindings-example-properties` for examples.

Const
+++++

This specifies a constant value the property must take. It is mainly useful for
constraining the values of common properties for a particular piece of
hardware.

.. _dt-bindings-child:

Child-binding
=============

``child-binding`` can be used when a node has children that all share the same
properties. Each child gets the contents of ``child-binding`` as its binding,
though an explicit ``compatible = ...`` on the child node takes precedence, if
a binding is found for it.

Consider a binding for a PWM LED node like this one, where the child nodes are
required to have a ``pwms`` property:

.. code-block:: devicetree

   pwmleds {
           compatible = "pwm-leds";

           red_pwm_led {
                   pwms = <&pwm3 4 15625000>;
           };
           green_pwm_led {
                   pwms = <&pwm3 0 15625000>;
           };
           /* ... */
   };

The binding would look like this:

.. code-block:: YAML

   compatible: "pwm-leds"

   child-binding:
     description: LED that uses PWM

     properties:
       pwms:
         type: phandle-array
         required: true

``child-binding`` also works recursively. For example, this binding:

.. code-block:: YAML

   compatible: foo

   child-binding:
     child-binding:
       properties:
         my-property:
           type: int
           required: true

will apply to the ``grandchild`` node in this DTS:

.. code-block:: devicetree

   parent {
           compatible = "foo";
           child {
                   grandchild {
                           my-property = <123>;
                   };
           };
   };

.. _dt-bindings-bus:

Bus
===

If the node is a bus controller, use ``bus:`` in the binding to say what type
of bus. For example, a binding for a SPI peripheral on an SoC would look like
this:

.. code-block:: YAML

   compatible: "manufacturer,spi-peripheral"
   bus: spi
   # ...

The presence of this key in the binding informs the build system that the
children of any node matching this binding appear on this type of bus.

This in turn influences the way ``on-bus:`` is used to match bindings for the
child nodes.

For a single bus supporting multiple protocols, e.g. I3C and I2C, the ``bus:``
in the binding can have a list as value:

.. code-block:: YAML

   compatible: "manufacturer,i3c-controller"
   bus: [i3c, i2c]
   # ...

.. _dt-bindings-on-bus:

On-bus
======

If the node appears as a device on a bus, use ``on-bus:`` in the binding to say
what type of bus.

For example, a binding for an external SPI memory chip should include this line:

.. code-block:: YAML

   on-bus: spi

And a binding for an I2C based temperature sensor should include this line:

.. code-block:: YAML

   on-bus: i2c

When looking for a binding for a node, the build system checks if the binding
for the parent node contains ``bus: <bus type>``. If it does, then only
bindings with a matching ``on-bus: <bus type>`` and bindings without an
explicit ``on-bus`` are considered. Bindings with an explicit ``on-bus: <bus
type>`` are searched for first, before bindings without an explicit ``on-bus``.
The search repeats for each item in the node's ``compatible`` property, in
order.

This feature allows the same device to have different bindings depending on
what bus it appears on. For example, consider a sensor device with compatible
``manufacturer,sensor`` which can be used via either I2C or SPI.

The sensor node may therefore appear in the devicetree as a child node of
either an SPI or an I2C controller, like this:

.. code-block:: devicetree

   spi-bus@0 {
      /* ... some compatible with 'bus: spi', etc. ... */

      sensor@0 {
          compatible = "manufacturer,sensor";
          reg = <0>;
          /* ... */
      };
   };

   i2c-bus@0 {
      /* ... some compatible with 'bus: i2c', etc. ... */

      sensor@79 {
          compatible = "manufacturer,sensor";
          reg = <79>;
          /* ... */
      };
   };

You can write two separate binding files which match these individual sensor
nodes, even though they have the same compatible:

.. code-block:: YAML

   # manufacturer,sensor-spi.yaml, which matches sensor@0 on the SPI bus:
   compatible: "manufacturer,sensor"
   on-bus: spi

   # manufacturer,sensor-i2c.yaml, which matches sensor@79 on the I2C bus:
   compatible: "manufacturer,sensor"
   properties:
     uses-clock-stretching:
       type: boolean
   on-bus: i2c

Only ``sensor@79`` can have a ``use-clock-stretching`` property. The
bus-sensitive logic ignores :file:`manufacturer,sensor-i2c.yaml` when searching
for a binding for ``sensor@0``.

.. _dt-bindings-cells:

Specifier cell names (\*-cells)
===============================

Specifier cells are usually used with ``phandle-array`` type properties briefly
introduced above.

To understand the purpose of ``*-cells``, assume that some node has the
following ``pwms`` property with type ``phandle-array``:

.. code-block:: none

   my-device {
   	pwms = <&pwm0 1 2>, <&pwm3 4>;
   };

The tooling strips the final ``s`` from the property name of such properties,
resulting in ``pwm``. Then the value of the ``#pwm-cells`` property is
looked up in each of the PWM controller nodes ``pwm0`` and ``pwm3``, like so:

.. code-block:: devicetree

   pwm0: pwm@0 {
   	compatible = "foo,pwm";
   	#pwm-cells = <2>;
   };

   pwm3: pwm@3 {
   	compatible = "bar,pwm";
   	#pwm-cells = <1>;
   };

The ``&pwm0 1 2`` part of the property value has two cells, ``1`` and ``2``,
which matches ``#pwm-cells = <2>;``, so these cells are considered the
*specifier* associated with ``pwm0`` in the phandle array.

Similarly, the cell ``4`` is the specifier associated with ``pwm3``.

The number of PWM cells in the specifiers in ``pwms`` must match the
``#pwm-cells`` values, as shown above. If there is a mismatch, an error is
raised. For example, this node would result in an error:

.. code-block:: devicetree

   my-bad-device {
   	/* wrong: 2 cells given in the specifier, but #pwm-cells is 1 in pwm3. */
   	pwms = <&pwm3 5 6>;
   };

The binding for each PWM controller must also have a ``*-cells`` key, in this
case ``pwm-cells``, giving names to the cells in each specifier:

.. code-block:: YAML

   # foo,pwm.yaml
   compatible: "foo,pwm"
   ...
   pwm-cells:
     - channel
     - period

   # bar,pwm.yaml
   compatible: "bar,pwm"
   ...
   pwm-cells:
     - period

A ``*-names`` (e.g. ``pwm-names``) property can appear on the node as well,
giving a name to each entry.

This allows the cells in the specifiers to be accessed by name, e.g. using APIs
like :c:macro:`DT_PWMS_CHANNEL_BY_NAME`.

Because other property names are derived from the name of the property by
removing the final ``s``, the property name must end in ``s``. An error is
raised if it doesn't.

An alternative is using a ``specifier-space`` property to indicate the base
property name for ``*-names`` and ``*-cells``.

``*-gpios`` properties are special-cased so that e.g. ``foo-gpios`` resolves to
``#gpio-cells`` rather than ``#foo-gpio-cells``.

If the specifier is empty (e.g. ``#clock-cells = <0>``), then ``*-cells`` can
either be omitted (recommended) or set to an empty array. Note that an empty
array is specified as e.g. ``clock-cells: []`` in YAML.

All ``phandle-array`` type properties support mapping through ``*-map``
properties, e.g. ``gpio-map``, as defined by the Devicetree specification.

.. _dt-bindings-include:

Include
=======

Bindings can include other files, which can be used to share common property
definitions between bindings. Use the ``include:`` key for this. Its value is
either a string or a list.

In the simplest case, you can include another file by giving its name as a
string, like this:

.. code-block:: YAML

   include: foo.yaml

If any file named :file:`foo.yaml` is found (see
:ref:`dt-where-bindings-are-located` for the search process), it will be
included into this binding.

Included files are merged into bindings with a simple recursive dictionary
merge. The build system will check that the resulting merged binding is
well-formed. It is allowed to include at any level, including ``child-binding``,
like this:

.. code-block:: YAML

   # foo.yaml will be merged with content at this level
   include: foo.yaml

   child-binding:
     # bar.yaml will be merged with content at this level
     include: bar.yaml

It is an error if a key appears with a different value in a binding and in a
file it includes, with one exception: a binding can have ``required: true`` for
a :ref:`property definition <dt-bindings-properties>` for which the included
file has ``required: false``. The ``required: true`` takes precedence, allowing
bindings to strengthen requirements from included files.

Note that weakening requirements by having ``required: false`` where the
included file has ``required: true`` is an error. This is meant to keep the
organization clean.

The file :zephyr_file:`base.yaml <dts/bindings/base/base.yaml>` contains
definitions for many common properties. When writing a new binding, it is a
good idea to check if :file:`base.yaml` already defines some of the needed
properties, and include it if it does.

Note that you can make a property defined in base.yaml obligatory like this,
taking :ref:`reg <dt-important-props>` as an example:

.. code-block:: YAML

   reg:
     required: true

This relies on the dictionary merge to fill in the other keys for ``reg``, like
``type``.

To include multiple files, you can use a list of strings:

.. code-block:: YAML

   include:
     - foo.yaml
     - bar.yaml

This includes the files :file:`foo.yaml` and :file:`bar.yaml`. (You can
write this list in a single line of YAML as ``include: [foo.yaml, bar.yaml]``.)

When including multiple files, any overlapping ``required`` keys on properties
in the included files are ORed together. This makes sure that a ``required:
true`` is always respected.

In some cases, you may want to include some property definitions from a file,
but not all of them. In this case, ``include:`` should be a list, and you can
filter out just the definitions you want by putting a mapping in the list, like
this:

.. code-block:: YAML

   include:
     - name: foo.yaml
       property-allowlist:
         - i-want-this-one
         - and-this-one
     - name: bar.yaml
       property-blocklist:
         - do-not-include-this-one
         - or-this-one

Each map element must have a ``name`` key which is the filename to include, and
may have ``property-allowlist`` and ``property-blocklist`` keys that filter
which properties are included.

You cannot have a single map element with both ``property-allowlist`` and
``property-blocklist`` keys. A map element with neither ``property-allowlist``
nor ``property-blocklist`` is valid; no additional filtering is done.

You can freely intermix strings and mappings in a single ``include:`` list:

.. code-block:: YAML

   include:
     - foo.yaml
     - name: bar.yaml
       property-blocklist:
         - do-not-include-this-one
         - or-this-one

Finally, you can filter from a child binding like this:

.. code-block:: YAML

   include:
     - name: bar.yaml
       child-binding:
         property-allowlist:
           - child-prop-to-allow

.. _dt-writing-bindings:

Rules for mainline bindings
***************************

This section includes general rules for writing bindings that you want to
submit to the mainline Zephyr Project. (You don't need to follow these rules
for bindings you don't intend to contribute to the Zephyr Project, but it's a
good idea.)

Decisions made by the Zephyr devicetree maintainer override the contents of
this section. If that happens, though, please let them know so they can update
this page, or you can send a patch yourself.

Always check for existing bindings
==================================

Zephyr aims for devicetree :ref:`dt-source-compatibility`. Therefore, if there
is an existing binding for your device in an authoritative location, you should
try to replicate its properties when writing a Zephyr binding, and you must
justify any Zephyr-specific divergences.

In particular, this rule applies if:

- There is an existing binding in the mainline Linux kernel. See
  :file:`Documentation/devicetree/bindings` in `Linus's tree`_ for existing
  bindings and the `Linux devicetree documentation`_ for more information.

- Your hardware vendor provides an official binding outside of the Linux
  kernel.

.. _Linus's tree:
   https://github.com/torvalds/linux/

.. _Linux devicetree documentation:
   https://www.kernel.org/doc/html/latest/devicetree/index.html

General rules
=============

- Bindings which match a compatible must have file names based on the compatible.

  - For example, a binding for compatible ``vnd,foo`` must be named ``vnd,foo.yaml``.
  - If the binding is bus-specific, you can append the bus to the file name;
    for example, if the binding YAML has ``on-bus: bar``, you may name the file
    ``vnd,foo-bar.yaml``.

- All recommendations in :ref:`dt-bindings-default` are requirements when
  submitting the binding.

  In particular, if you use the ``default:`` feature, you must justify the
  value in the property's description.

- There are two ways to write property ``description:`` strings that are always
  OK.

  If your description is short, it's fine to use this style:

  .. code-block:: yaml

     description: my short string

  If your description is long or spans multiple lines, you must use this
  style:

  .. code-block:: yaml

     description: |
       My very long string
       goes here.
       Look at all these lines!

  This ``|`` style prevents YAML parsers from removing the newlines in
  multi-line descriptions. This in turn makes these long strings
  display properly in the :ref:`devicetree_binding_index`.

  Do not use any other style for long or multi-line strings.

- Do not use uppercase letters (``A`` through ``Z``) or underscores (``_``) in
  property names. Use lowercase letters (``a`` through ``z``) instead of
  uppercase. Use dashes (``-``) instead of underscores. (The one exception to
  this rule is if you are replicating a well-established binding from somewhere
  like Linux.)

Rules for vendor prefixes
=========================

The following general rules apply to vendor prefixes in :ref:`compatible
<dt-important-props>` properties.

- If your device is manufactured by a specific vendor, then its compatible
  should have a vendor prefix.

  If your binding describes hardware with a well known vendor from the list in
  :zephyr_file:`dts/bindings/vendor-prefixes.txt`, you must use that vendor
  prefix.

- If your device is not manufactured by a specific hardware vendor, do **not**
  invent a vendor prefix. Vendor prefixes are not mandatory parts of compatible
  properties, and compatibles should not include them unless they refer to an
  actual vendor. There are some exceptions to this rule, but the practice is
  strongly discouraged.

- Do not submit additions to Zephyr's :file:`dts/bindings/vendor-prefixes.txt`
  file unless you also include users of the new prefix. This means at least a
  binding and a devicetree using the vendor prefix, and should ideally include
  a device driver handling that compatible.

  For custom bindings, you can add a custom
  :file:`dts/bindings/vendor-prefixes.txt` file to any directory in your
  :ref:`DTS_ROOT <dts_root>`. The devicetree tooling will respect these
  prefixes, and will not generate warnings or errors if you use them in your
  own bindings or devicetrees.

- We sometimes synchronize Zephyr's vendor-prefixes.txt file with the Linux
  kernel's equivalent file; this process is exempt from the previous rule.

- If your binding is describing an abstract class of hardware with Zephyr
  specific drivers handling the nodes, it's usually best to use ``zephyr`` as
  the vendor prefix. See :ref:`dt_vendor_zephyr` for examples.

.. _dt-bindings-default-rules:

Rules for default values
========================

In any case where ``default:`` is used in a devicetree binding, the
``description:`` for that property **must** explain *why* the value was
selected and any conditions that would make it necessary to provide a different
value. Additionally, if changing one property would require changing another to
create a consistent configuration, then those properties should be made
required.

There is no need to document the default value itself; this is already present
in the :ref:`devicetree_binding_index` output.

There is a risk in using ``default:`` when the value in the binding may be
incorrect for a particular board or hardware configuration.  For example,
defaulting the capacity of the connected power cell in a charging IC binding
is likely to be incorrect.  For such properties it's better to make the
property ``required: true``, forcing the user to make an explicit choice.

Driver developers should use their best judgment as to whether a value can be
safely defaulted. Candidates for default values include:

- delays that would be different only under unusual conditions
  (such as intervening hardware)
- configuration for devices that have a standard initial configuration (such as
  a USB audio headset)
- defaults which match the vendor-specified power-on reset value
  (as long as they are independent from other properties)

Examples of how to write descriptions according to these rules:

.. code-block:: yaml

   properties:
     cs-interval:
       type: int
       default: 0
       description: |
         Minimum interval between chip select deassertion and assertion.
         The default corresponds to the reset value of the register field.
     hold-time-ms:
       type: int
       default: 20
       description: |
         Amount of time to hold the power enable GPIO asserted before
         initiating communication. The default was recommended in the
         manufacturer datasheet, and would only change under very
         cold temperatures.

Some examples of what **not** to do, and why:

.. code-block:: yaml

   properties:
     # Description doesn't mention anything about the default
     foo:
       type: int
       default: 1
       description: number of foos

     # Description mentions the default value instead of why it
     # was chosen
     bar:
       type: int
       default: 2
       description: bar size; default is 2

     # Explanation of the default value is in a comment instead
     # of the description. This won't be shown in the bindings index.
     baz:
       type: int
       # This is the recommended value chosen by the manufacturer.
       default: 2
       description: baz time in milliseconds

.. _dt-inferred-bindings:
.. _dt-zephyr-user:

Inferred bindings
*****************

Zephyr's devicetree scripts can "infer" a binding for the special
``/zephyr,user`` node based on the values observed in its properties.

This node matches a binding which is dynamically created by the build system
based on the values of its properties in the final devicetree. It does not have
a ``compatible`` property.

This node is meant for sample code and applications. The devicetree API
provides it as a convenient container when only a few simple properties are
needed, such as storing a hardware-dependent value, phandle(s), or GPIO pin.

For example, with this DTS fragment:

.. code-block:: devicetree

   #include <zephyr/dt-bindings/gpio/gpio.h>

   / {
	zephyr,user {
		boolean;
		bytes = [81 82 83];
		number = <23>;
		numbers = <1>, <2>, <3>;
		string = "text";
		strings = "a", "b", "c";

		handle = <&gpio0>;
		handles = <&gpio0>, <&gpio1>;
		signal-gpios = <&gpio0 1 GPIO_ACTIVE_HIGH>;
	};
   };

You can get the simple values like this:

.. code-block:: C

   #define ZEPHYR_USER_NODE DT_PATH(zephyr_user)

   DT_PROP(ZEPHYR_USER_NODE, boolean) // 1
   DT_PROP(ZEPHYR_USER_NODE, bytes)   // {0x81, 0x82, 0x83}
   DT_PROP(ZEPHYR_USER_NODE, number)  // 23
   DT_PROP(ZEPHYR_USER_NODE, numbers) // {1, 2, 3}
   DT_PROP(ZEPHYR_USER_NODE, string)  // "text"
   DT_PROP(ZEPHYR_USER_NODE, strings) // {"a", "b", "c"}

You can convert the phandles in the ``handle`` and ``handles`` properties to
device pointers like this:

.. code-block:: C

   /*
    * Same thing as:
    *
    * ... my_dev = DEVICE_DT_GET(DT_NODELABEL(gpio0));
    */
   const struct device *my_device =
   	DEVICE_DT_GET(DT_PROP(ZEPHYR_USER_NODE, handle));

   #define PHANDLE_TO_DEVICE(node_id, prop, idx) \
        DEVICE_DT_GET(DT_PHANDLE_BY_IDX(node_id, prop, idx)),

   /*
    * Same thing as:
    *
    * ... *my_devices[] = {
    *         DEVICE_DT_GET(DT_NODELABEL(gpio0)),
    *         DEVICE_DT_GET(DT_NODELABEL(gpio1)),
    * };
    */
   const struct device *my_devices[] = {
   	DT_FOREACH_PROP_ELEM(ZEPHYR_USER_NODE, handles, PHANDLE_TO_DEVICE)
   };

And you can convert the pin defined in ``signal-gpios`` to a ``struct
gpio_dt_spec``, then use it like this:

.. code-block:: C

   #include <zephyr/drivers/gpio.h>

   #define ZEPHYR_USER_NODE DT_PATH(zephyr_user)

   const struct gpio_dt_spec signal =
           GPIO_DT_SPEC_GET(ZEPHYR_USER_NODE, signal_gpios);

   /* Configure the pin */
   gpio_pin_configure_dt(&signal, GPIO_OUTPUT_INACTIVE);

   /* Set the pin to its active level */
   gpio_pin_set_dt(&signal, 1);

(See :c:struct:`gpio_dt_spec`, :c:macro:`GPIO_DT_SPEC_GET`, and
:c:func:`gpio_pin_configure_dt` for details on these APIs.)
