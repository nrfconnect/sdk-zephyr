.. _cfb_custom_fonts:

Custom Fonts
############

Overview
********
A simple example showing how to generate Character Framebuffer (CFB)
font headers automatically at build time.

This example generates a font with font elements for 6 sided dice from
a PNG image, and then uses the generated header (``cfb_font_dice.h``)
to show the font elements on the display of a supported board.

The source code for this sample application can be found at:
:file:`samples/display/cfb_custom_font`.

Building and Running
********************

There are different configuration files in the cfb_custom_font
directory:

- :file:`prj.conf`
  Generic config file, normally you should use this.

- :file:`boards/reel_board.conf`
  This overlay config enables support for SSD1673 display controller
  on the reel_board.


Example building for the reel_board with SSD1673 display support:

.. zephyr-app-commands::
   :zephyr-app: samples/display/cfb_custom_font
   :host-os: unix
   :board: reel_board
   :goals: flash
   :compact:
