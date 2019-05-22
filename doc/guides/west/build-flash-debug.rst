.. _west-build-flash-debug:

Building, Flashing and Debugging
################################

Zephyr provides several :ref:`west extension commands <west-extensions>` for
building, flashing, and interacting with Zephyr programs running on a board:
``build``, ``flash``, ``debug``, ``debugserver`` and ``attach``.

These use information stored in the CMake cache [#cmakecache]_ to
flash or attach a debugger to a board supported by Zephyr. The exception is
starting a clean build (i.e. with no previous artifacts) which will in fact
run CMake thus creating the corresponding cache.
The CMake build system commands with the same names (i.e. all but ``build``)
directly delegate to West.

.. Add a per-page contents at the top of the page. This page is nested
   deeply enough that it doesn't have any subheadings in the main nav.

.. only:: html

   .. contents::
      :local:

.. _west-building:

Building: ``west build``
************************

.. tip:: Run ``west build -h`` for a quick overview.

The ``build`` command helps you build Zephyr applications from source. You can
use :ref:`west config <west-config-cmd>` to configure its behavior.

This command attempts to "do what you mean" when run from a Zephyr application
source or build directory:

- When you run ``west build`` in an existing build directory, the board, source
  directory, etc. are obtained from the CMake cache, and that build directory
  is re-compiled.

- The same is true if a Zephyr build directory named :file:`build` exists in
  your current working directory.

- Otherwise, the source directory defaults to the current working directory, so
  running ``west build`` from a Zephyr application's source directory compiles
  it.

Basics
======

The easiest way to use ``west build`` is to go to an application's root
directory (i.e. the folder containing the application's :file:`CMakeLists.txt`)
and then run::

  west build -b <BOARD>

Where ``<BOARD>`` is the name of the board you want to build for. This is
exactly the same name you would supply to CMake if you were to invoke it with:
``cmake -DBOARD=<BOARD>``.

.. tip::

   You can use the :ref:`west boards <west-boards>` command to list all
   supported boards.

A build directory named :file:`build` will be created, and the application will
be compiled there after ``west build`` runs CMake to create a build system in
that directory. If you run ``west build`` with an existing build directory, the
application is incrementally re-compiled without re-running CMake (you can
force CMake to run again with ``--cmake``).

You don't need to use the ``--board`` option if you've already got an existing
build directory; ``west build`` can figure out the board from the CMake cache.
For new builds, the ``--board`` option, :envvar:`BOARD` environment variable,
or ``build.board`` configuration option are checked (in that order).

To configure ``west build`` to build for the ``reel_board`` by default::

  west config build.board reel_board

(You can use any other board supported by Zephyr here; it doesn't have to be
``reel_board``.)

To use another build directory, use ``--build-dir`` (or ``-d``)::

  west build -b <BOARD> --build-dir path/to/build/directory

To specify the application source directory explicitly, give its path as a
positional argument::

  west build -b <BOARD> path/to/source/directory

To specify the build system target to run, use ``--target`` (or ``-t``).

For example, on host platforms with QEMU, you can use the ``run`` target to
build and run the :ref:`hello_world` sample for the emulated :ref:`qemu_x86
<qemu_x86>` board in one command::

  west build -b qemu_x86 -t run samples/hello_world

As another example, to use ``-t`` to list all build system targets::

  west build -t help

As a final example, to use ``-t`` to run the ``pristine`` target, which deletes
all the files in the build directory::

  west build -t pristine

To have ``west build`` run the ``pristine`` target before re-running CMake to
generate a build system, use the ``--pristine`` (or ``-p``) option. For
example, to switch board and application (which requires a pristine build
directory) in one command::

  west build -b qemu_x86 samples/philosophers
  west build -p -b reel_board samples/hello_world

To let west decide for you if a pristine build is needed, use ``-p auto``::

  west build -p auto -b reel_board samples/hello_world

.. tip::

   You can run ``west config build.pristine auto`` to make this setting
   permanent.


.. _west-building-generator:

To add additional arguments to the CMake invocation performed by ``west
build``, pass them after a ``--`` at the end of the command line.

For example, to use the Unix Makefiles CMake generator instead of Ninja (which
``west build`` uses by default), run::

  west build -b reel_board -- -G'Unix Makefiles'

.. note::

   Passing additional CMake arguments like this forces ``west build`` to re-run
   CMake, even if a build system has already been generated.

As another example, to use Unix Makefiles and enable the
`CMAKE_VERBOSE_MAKEFILE`_ option::

  west build -b reel_board -- -G'Unix Makefiles' -DCMAKE_VERBOSE_MAKEFILE=ON

Notice how the ``--`` only appears once, even though multiple CMake arguments
are given. All command-line arguments to ``west build`` after a ``--`` are
passed to CMake.

As a final example, to merge the :file:`file.conf` Kconfig fragment into your
build's :file:`.config`::

  west build -- -DOVERLAY_CONFIG=file.conf

To force a CMake re-run, use the ``--cmake`` (or ``--c``) option::

  west build -c

Configuration Options
=====================

You can :ref:`configure <west-config-cmd>` ``west build`` using these options.

.. NOTE: docs authors: keep this table sorted alphabetically

.. list-table::
   :widths: 10 30
   :header-rows: 1

   * - Option
     - Description
   * - ``build.board``
     - String. If given, this the board used by :ref:`west build
       <west-building>` when ``--board`` is not given and :envvar:`BOARD`
       is unset in the environment.
   * - ``build.board_warn``
     - Boolean, default ``true``. If ``false``, disables warnings when
       ``west build`` can't figure out the target board.
   * - ``build.generator``
     - String, default ``Ninja``. The `CMake Generator`_ to use to create a
       build system. (To set a generator for a single build, see the
       :ref:`above example <west-building-generator>`)
   * - ``build.pristine``
     - String. Controls the way in which ``west build`` may clean the build
       folder before building. Can take the following values:

         - ``never`` (default): Never automatically make the build folder
           pristine.
         - ``auto``:  ``west build`` will automatically make the build folder
           pristine before building, if a build system is present and the build
           would fail otherwise (e.g. the user has specified a different board
           or application from the one previously used to make the build
           directory).
         - ``always``: Always make the build folder pristine before building, if
           a build system is present.

.. _west-flashing:

Flashing: ``west flash``
************************

.. tip:: Run ``west flash -h`` for additional help.

Basics
======

From a Zephyr build directory, re-build the binary and flash it to
your board::

  west flash

Without options, the behavior is the same as ``ninja flash`` (or
``make flash``, etc.).

To specify the build directory, use ``--build-dir`` (or ``-d``)::

  west flash --build-dir path/to/build/directory

Since the build directory defaults to :file:`build`, if you do not specify
a build directory but a folder named :file:`build` is present, that will be
used, allowing you to flash from outside the :file:`build` folder with no
additional parameters.

Choosing a Runner
=================

If your board's Zephyr integration supports flashing with multiple
programs, you can specify which one to use using the ``--runner`` (or
``-r``) option. For example, if West flashes your board with
``nrfjprog`` by default, but it also supports JLink, you can override
the default with::

  west flash --runner jlink

See :ref:`west-runner` below for more information on the ``runner``
library used by West. The list of runners which support flashing can
be obtained with ``west flash -H``; if run from a build directory or
with ``--build-dir``, this will print additional information on
available runners for your board.

Configuration Overrides
=======================

The CMake cache contains default values West uses while flashing, such
as where the board directory is on the file system, the path to the
kernel binaries to flash in several formats, and more. You can
override any of this configuration at runtime with additional options.

For example, to override the HEX file containing the Zephyr image to
flash (assuming your runner expects a HEX file), but keep other
flash configuration at default values::

  west flash --kernel-hex path/to/some/other.hex

The ``west flash -h`` output includes a complete list of overrides
supported by all runners.

Runner-Specific Overrides
=========================

Each runner may support additional options related to flashing. For
example, some runners support an ``--erase`` flag, which mass-erases
the flash storage on your board before flashing the Zephyr image.

To view all of the available options for the runners your board
supports, as well as their usage information, use ``--context`` (or
``-H``)::

  west flash --context

.. important::

   Note the capital H in the short option name. This re-runs the build
   in order to ensure the information displayed is up to date!

When running West outside of a build directory, ``west flash -H`` just
prints a list of runners. You can use ``west flash -H -r
<runner-name>`` to print usage information for options supported by
that runner.

For example, to print usage information about the ``jlink`` runner::

  west flash -H -r jlink

.. _west-debugging:

Debugging: ``west debug``, ``west debugserver``
***********************************************

.. tip::

   Run ``west debug -h`` or ``west debugserver -h`` for additional help.

Basics
======

From a Zephyr build directory, to attach a debugger to your board and
open up a debug console (e.g. a GDB session)::

  west debug

To attach a debugger to your board and open up a local network port
you can connect a debugger to (e.g. an IDE debugger)::

  west debugserver

Without options, the behavior is the same as ``ninja debug`` and
``ninja debugserver`` (or ``make debug``, etc.).

To specify the build directory, use ``--build-dir`` (or ``-d``)::

  west debug --build-dir path/to/build/directory
  west debugserver --build-dir path/to/build/directory

Since the build directory defaults to :file:`build`, if you do not specify
a build directory but a folder named :file:`build` is present, that will be
used, allowing you to debug from outside the :file:`build` folder with no
additional parameters.

Choosing a Runner
=================

If your board's Zephyr integration supports debugging with multiple
programs, you can specify which one to use using the ``--runner`` (or
``-r``) option. For example, if West debugs your board with
``pyocd-gdbserver`` by default, but it also supports JLink, you can
override the default with::

  west debug --runner jlink
  west debugserver --runner jlink

See :ref:`west-runner` below for more information on the ``runner``
library used by West. The list of runners which support debugging can
be obtained with ``west debug -H``; if run from a build directory or
with ``--build-dir``, this will print additional information on
available runners for your board.

Configuration Overrides
=======================

The CMake cache contains default values West uses for debugging, such
as where the board directory is on the file system, the path to the
kernel binaries containing symbol tables, and more. You can override
any of this configuration at runtime with additional options.

For example, to override the ELF file containing the Zephyr binary and
symbol tables (assuming your runner expects an ELF file), but keep
other debug configuration at default values::

  west debug --kernel-elf path/to/some/other.elf
  west debugserver --kernel-elf path/to/some/other.elf

The ``west debug -h`` output includes a complete list of overrides
supported by all runners.

Runner-Specific Overrides
=========================

Each runner may support additional options related to debugging. For
example, some runners support flags which allow you to set the network
ports used by debug servers.

To view all of the available options for the runners your board
supports, as well as their usage information, use ``--context`` (or
``-H``)::

  west debug --context

(The command ``west debugserver --context`` will print the same output.)

.. important::

   Note the capital H in the short option name. This re-runs the build
   in order to ensure the information displayed is up to date!

When running West outside of a build directory, ``west debug -H`` just
prints a list of runners. You can use ``west debug -H -r
<runner-name>`` to print usage information for options supported by
that runner.

For example, to print usage information about the ``jlink`` runner::

  west debug -H -r jlink

.. _west-runner:

Implementation Details
**********************

The flash and debug commands are implemented as west *extension
commands*: that is, they are west commands whose source code lives
outside the west repository. Some reasons this choice was made are:

- Their implementations are tightly coupled to the Zephyr build
  system, e.g. due to their reliance on CMake cache variables.

- Pull requests adding features to them are almost always motivated by
  a corresponding change to an upstream board, so it makes sense to
  put them in Zephyr to avoid needing pull requests in multiple
  repositories.

- Many users find it natural to search for their implementations in
  the Zephyr source tree.

The extension commands are a thin wrapper around a package called
``runners`` (this package is also in the Zephyr tree, in
:zephyr_file:`scripts/west_commands/runners`).

The central abstraction within this library is ``ZephyrBinaryRunner``,
an abstract class which represents *runner* objects, which can flash
and/or debug Zephyr programs. The set of available runners is
determined by the imported subclasses of ``ZephyrBinaryRunner``.
``ZephyrBinaryRunner`` is available in the ``runners.core`` module;
individual runner implementations are in other submodules, such as
``runners.nrfjprog``, ``runners.openocd``, etc.

Hacking and APIs
****************

Developers can add support for new ways to flash and debug Zephyr
programs by implementing additional runners. To get this support into
upstream Zephyr, the runner should be added into a new or existing
``runners`` module, and imported from :file:`runner/__init__.py`.

.. note::

   The test cases in :zephyr_file:`scripts/west_commands/tests` add unit test
   coverage for the runners package and individual runner classes.

   Please try to add tests when adding new runners. Note that if your
   changes break existing test cases, CI testing on upstream pull
   requests will fail.

API Documentation for the ``runners.core`` module can be found in
:ref:`west-apis`.

Doing it By Hand
****************

If you prefer not to use West to flash or debug your board, simply
inspect the build directory for the binaries output by the build
system. These will be named something like ``zephyr/zephyr.elf``,
``zephyr/zephyr.hex``, etc., depending on your board's build system
integration. These binaries may be flashed to a board using
alternative tools of your choice, or used for debugging as needed,
e.g. as a source of symbol tables.

By default, these West commands rebuild binaries before flashing and
debugging. This can of course also be accomplished using the usual
targets provided by Zephyr's build system (in fact, that's how these
commands do it).

.. rubric:: Footnotes

.. [#cmakecache]

   The CMake cache is a file containing saved variables and values
   which is created by CMake when it is first run to generate a build
   system. See the `cmake(1)`_ manual for more details.

.. _cmake(1):
   https://cmake.org/cmake/help/latest/manual/cmake.1.html

.. _CMAKE_VERBOSE_MAKEFILE:
   https://cmake.org/cmake/help/latest/variable/CMAKE_VERBOSE_MAKEFILE.html

.. _CMake Generator:
   https://cmake.org/cmake/help/latest/manual/cmake-generators.7.html
