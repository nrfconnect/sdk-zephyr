.. _SDHC_disks:

SDHC disks
##########

Zephyr includes support for connecting an SDHC card via the SPI bus.
This can be used with Zephyr's built-in filesystem support to read and
write FAT formatted cards.

The system has been tested with cards from Samsung, SanDisk, and 4V
with sizes from 2 GiB to 32 GiB in single partition mode.  Higher
capacity cards should also work but haven't been tested.  Please let
us know if they work!

MMC and SDSC (<= 2 GiB) cards are not supported and will be ignored.

.. note:: The system does not support inserting or removing cards while the
   system is running. The cards must be present at boot and must not be
   removed. This may be fixed in future releases.

   FAT filesystems are not power safe so the filesystem may become
   corrupted if power is lost or if the card is removed.

Enabling
********

For example, this device tree fragment adds an SDHC card slot on `spi1`,
uses `PA27` for chip select, and runs the SPI bus at 24 MHz once the
SDHC card has been initialized:

.. code-block:: none

    &spi1 {
            status = "ok";
            cs-gpios = <&porta 27 0>;

            sdhc0: sdhc@0 {
                    compatible = "zephyr,mmc-spi-slot";
                    reg = <0>;
                    status = "ok";
                    label = "SDHC0";
                    spi-max-frequency = <24000000>;
            };
    };

Usage
*****

The SDHC card will be automatically detected and initialized by the
filesystem driver when the board boots.

To read and write files and directories, see the :ref:`file_system` in
:file:`include/fs.h` such as :c:func:`fs_open()`,
:c:func:`fs_read()`, and :c:func:`fs_write()`.
