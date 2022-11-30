.. _kconfig-functions:

Custom Kconfig Preprocessor Functions
#####################################

Kconfiglib supports custom Kconfig preprocessor functions written in Python.
These functions are defined in
:zephyr_file:`scripts/kconfig/kconfigfunctions.py`.

.. note::

   The official Kconfig preprocessor documentation can be found `here
   <https://www.kernel.org/doc/html/latest/kbuild/kconfig-macro-language.html>`__.

Most of the custom preprocessor functions are used to get devicetree
information into Kconfig. For example, the default value of a Kconfig symbol
can be fetched from a devicetree ``reg`` property.

Devicetree-related Functions
****************************

The functions listed below are used to get devicetree information into Kconfig.
See the Python docstrings in :zephyr_file:`scripts/kconfig/kconfigfunctions.py`
for detailed documentation.

The ``*_int`` version of each function returns the value as a decimal integer,
while the ``*_hex`` version returns a hexadecimal value starting with ``0x``.

.. code-block:: none

   $(dt_has_compat,<compatible string>)
   $(dt_compat_enabled,<compatible string>)
   $(dt_compat_on_bus,<compatible string>,<bus>)
   $(dt_chosen_label,<property in /chosen>)
   $(dt_chosen_enabled,<property in /chosen>)
   $(dt_chosen_path,<property in /chosen>)
   $(dt_chosen_has_compat,<property in /chosen>)
   $(dt_path_enabled,<node path>)
   $(dt_alias_enabled,<node alias>)
   $(dt_nodelabel_enabled,<node label>)
   $(dt_nodelabel_enabled_with_compat,<node label>,<compatible string>)
   $(dt_chosen_reg_addr_int,<property in /chosen>[,<index>,<unit>])
   $(dt_chosen_reg_addr_hex,<property in /chosen>[,<index>,<unit>])
   $(dt_chosen_reg_size_int,<property in /chosen>[,<index>,<unit>])
   $(dt_chosen_reg_size_hex,<property in /chosen>[,<index>,<unit>])
   $(dt_node_reg_addr_int,<node path>[,<index>,<unit>])
   $(dt_node_reg_addr_hex,<node path>[,<index>,<unit>])
   $(dt_node_reg_size_int,<node path>[,<index>,<unit>])
   $(dt_node_reg_size_hex,<node path>[,<index>,<unit>])
   $(dt_compat_enabled,<compatible string>)
   $(dt_chosen_enabled,<property in /chosen>)
   $(dt_node_bool_prop,<node path>,<prop>)
   $(dt_nodelabel_bool_prop,<node label>,<prop>)
   $(dt_node_has_prop,<node path>,<prop>)
   $(dt_nodelabel_has_prop,<node label>,<prop>)
   $(dt_node_int_prop_int,<node path>,<prop>[,<unit>])
   $(dt_node_int_prop_hex,<node path>,<prop>[,<unit>])
   $(dt_node_str_prop_equals,<node path>,<prop>,<value>)
   $(dt_nodelabel_has_compat,<node label>,<compatible string>)
   $(dt_node_has_compat,<node path>,<compatible string>)
   $(dt_nodelabel_path,<node label>)
   $(dt_node_parent,<node path>)
   $(shields_list_contains,<shield name>)


Example Usage
=============

Assume that the devicetree for some board looks like this:

.. code-block:: none

   {
   	soc {
   		#address-cells = <1>;
   		#size-cells = <1>;

   		spi0: spi@10014000 {
   			compatible = "sifive,spi0";
   			reg = <0x10014000 0x1000 0x20010000 0x3c0900>;
   			reg-names = "control", "mem";
   			...
   		};
   };

The second entry in ``reg`` in ``spi@1001400`` (``<0x20010000 0x3c0900>``)
corresponds to ``mem``, and has the address ``0x20010000``. This address can be
inserted into Kconfig as follows:

.. code-block:: none

   config FLASH_BASE_ADDRESS
   	default $(dt_node_reg_addr_hex,/soc/spi@1001400,1)

After preprocessor expansion, this turns into the definition below:

.. code-block:: none

   config FLASH_BASE_ADDRESS
   	default 0x20010000
