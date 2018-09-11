.. _application:

Application Development Primer
##############################

.. note::

   In this document, we'll assume your **application directory** is
   :file:`<home>/app`, and that its **build directory** is
   :file:`<home>/app/build`.
   (These terms are defined in the following Overview.)
   On Linux/macOS, <home> is equivalent to ``~``, whereas on Windows it's
   ``%userprofile%``.

Overview
********

Zephyr's build system is based on CMake.

The build system is application-centric, and requires Zephyr-based applications
to initiate building the kernel source tree. The application build controls
the configuration and build process of both the application and Zephyr itself,
compiling them into a single binary.

Zephyr's base directory hosts Zephyr's own source code, its kernel
configuration options, and its build definitions.

The files in the **application directory** link Zephyr with the
application. This directory contains all application-specific files, such as
configuration options and source code.

An application in its simplest form has the following contents:

.. code-block:: none

   <home>/app
   ├── CMakeLists.txt
   ├── prj.conf
   └── src
       └── main.c

These contents are:

* **CMakeLists.txt**: This file tells the build system where to find the other
  application files, and links the application directory with Zephyr's CMake
  build system. This link provides features supported by Zephyr's build system,
  such as board-specific kernel configuration files, the ability to run and
  debug compiled binaries on real or emulated hardware, and more.

* **Kernel configuration files**: An application typically provides a
  configuration file (usually called :file:`prj.conf`) that specifies
  application-specific values for one or more kernel configuration options.
  These application settings are merged with board-specific settings to produce
  a kernel configuration.

  The :ref:`application_kconfig` section below goes over application
  configuration in detail.

* **Application source code files**: An application typically provides one
  or more application-specific files, written in C or assembly language. These
  files are usually located in a sub-directory called :file:`src`.

Once an application has been defined, you can use CMake to create project files
for building it from a directory where you want to host these files. This is
known as the **build directory**. Application build artifacts are always
generated in a build directory; Zephyr does not support "in-tree" builds.

.. important::

   You must create a build directory yourself, and call ``cmake`` from
   there. The Zephyr build system does not create build directories for you.

The following sections describe how to create, build, and run Zephyr
applications, followed by more detailed reference material.

Creating an Application
***********************

Follow these steps to create a new application directory. (Refer to
:ref:`samples-and-demos` for existing applications provided as part of Zephyr.)

#. Create an application directory on your workstation computer, outside of the
   Zephyr base directory.  Usually you'll want to create it somewhere under
   your user's home directory.

   For example, in a Unix shell, navigate to a location where you want your
   application to reside, then enter:

   .. code-block:: console

      mkdir app

#. It's recommended to place all application source code in a subdirectory
   named :file:`src`.  This makes it easier to distinguish between project
   files and sources.

   Continuing the Unix shell example from the previous step, enter:

   .. code-block:: console

      cd app
      mkdir src

#. Create an empty :file:`CMakeLists.txt` file in your application directory.

#. Add boilerplate code that sets the minimum CMake version and pulls
   in the Zephyr build system:

   .. code-block:: cmake

      cmake_minimum_required(VERSION 3.8.2)
      include($ENV{ZEPHYR_BASE}/cmake/app/boilerplate.cmake NO_POLICY_SCOPE)
      project(NONE)

   .. note:: cmake_minimum_required is also invoked from
             :file:`boilerplate.cmake`. The most recent of the two
             versions will be enforced by CMake.


#. Place your application source code in the :file:`src` sub-directory. For
   this example, we'll assume you created a file named :file:`src/main.c`.

#. Add your source code files to the ``app`` target in your application
   directory's :file:`CMakeLists.txt`. For example, to add :file:`src/main.c`,
   add the following line to your :file:`CMakeLists.txt`:

   .. code-block:: cmake

      target_sources(app PRIVATE src/main.c)

#. Configure features used by your application. Zephyr's configuration uses
   the same Kconfig and Device Tree systems used by the Linux kernel, but with
   its own configuration trees. Usually, you just create a file named
   :file:`prj.conf` in your application directory, where you enable or disable
   features provided by Zephyr's Kconfig configuration system. Optionally you
   can also configure any Device Tree overlays needed by your application
   (this is usually not necessary; see :ref:`application_dt` below for more
   details). You can use existing samples to get started. For more information,
   see :ref:`application_configuration` below.

Applications integrate with the Zephyr build system using the boilerplate code
shown above in :file:`CMakeLists.txt`. The following important variables
configure the Zephyr build system:

* :makevar:`ZEPHYR_BASE`: Sets the path to the Zephyr base directory.  This is
  usually an environment variable set by the :file:`zephyr-env.sh` script on
  Linux/macOS or manually on Windows, as you learned when getting started
  with Zephyr in :ref:`getting_started_run_sample`. You can also set
  :makevar:`ZEPHYR_BASE`: explicitly on Linux and macOS if you want to.

* :makevar:`BOARD`: Selects the board that the application's build will use for
  the default configuration. This can be defined in the environment, in your
  application's :file:`CMakeLists.txt` file, or in the ``cmake`` command line.

* :makevar:`CONF_FILE`: Indicates the name of one or more configuration
  fragment files.  Multiple filenames can either be separated by a single space
  or a single semicolon.  Each file includes Kconfig configuration values that
  override the default configuration values.  Like :makevar:`BOARD`, this can
  also be defined in the environment, in your application's
  :file:`CMakeLists.txt` file, or in the ``cmake`` command line.

* :makevar:`DTC_OVERLAY_FILE`: Indicates the name of one or more Device Tree
  overlay files.  Each file includes Device Tree values that
  override the default DT values.  Like :makevar:`CONF_FILE`, this
  can also be defined in the environment, in your application's
  :file:`CMakeLists.txt` file, or in the ``cmake`` command line.

.. _build_an_application:

Build an Application
********************

The Zephyr build system compiles and links all components of an application
into a single application image that can be run on simulated hardware or real
hardware.

As described in :ref:`getting_started_cmake`, on Linux and macOS you can choose
between the `make` and `ninja` generators, whereas on Windows you need to use
`ninja`. For simplicity we will use `ninja` throughout this guide.

Basics
======

#. Navigate to the application directory :file:`<home>/app`.

#. Enter the following commands to build the application's
   :file:`zephyr.elf` image using the configuration settings for the
   board type specified in the application's :file:`CMakeLists.txt`.

   .. code-block:: console

       mkdir build
       cd build
       cmake -GNinja ..
       ninja

   If desired, you can build the application using the configuration settings
   specified in an alternate :file:`.conf` file using the :code:`CONF_FILE`
   parameter. These settings will override the settings in the application's
   :file:`.config` file or its default :file:`.conf` file. For example:

   .. code-block:: console

       # On Linux/macOS
       export CONF_FILE=prj.alternate.conf
       # On Windows
       set CONF_FILE=prj.alternate.conf

       cmake -GNinja ..
       ninja

   If desired, you can generate project files for a different board
   type than the one specified in the application's
   :file:`CMakeLists.txt` by defining the environment variable
   :code:`BOARD`.

   Both the :code:`CONF_FILE` and :code:`BOARD` parameters can be specified
   when building the application.

Build Directory Contents
========================

When using the Ninja generator a build directory looks like this:

.. code-block:: none

   <home>/app/build
   ├── build.ninja
   ├── CMakeCache.txt
   ├── CMakeFiles
   ├── cmake_install.cmake
   ├── rules.ninja
   └── zephyr

The most notable files in the build directory are:

* :file:`build.ninja`, which can be invoked to build the application.

* A :file:`zephyr` directory, which is the working directory of the
  generated build system, and where most generated files are created and
  stored.

After running ``ninja``, the following build output files will be written to
the :file:`zephyr` sub-directory of the build directory. (This is **not the
Zephyr base directory**, which contains the Zephyr source code etc. and is
described above.)

* :file:`.config`, which contains the configuration settings
  used to build the application.

* Various object files (:file:`.o` files and :file:`.a` files) containing
  compiled kernel and application code.

* :file:`zephyr.elf`, which contains the final combined application and
  kernel binary. Other binary output formats, such as :file:`.hex` and
  :file:`.bin`, are also supported.

Rebuilding an Application
=========================

Application development is usually fastest when changes are continually tested.
Frequently rebuilding your application makes debugging less painful
as the application becomes more complex. It's usually a good idea to
rebuild and test after any major changes to the application's source files,
CMakeLists.txt files, or configuration settings.

.. important::

    The Zephyr build system rebuilds only the parts of the application image
    potentially affected by the changes. Consequently, rebuilding an application
    is often significantly faster than building it the first time.

Sometimes the build system doesn't rebuild the application correctly
because it fails to recompile one or more necessary files. You can force
the build system to rebuild the entire application from scratch with the
following procedure:


#. Open a terminal console on your host computer, and navigate to the
   build directory :file:`<home>/app/build`.

#. Enter the following command to delete the application's generated
   files, except for the :file:`.config` file that contains the
   application's current configuration information.

   .. code-block:: console

       ninja clean

   Alternatively, enter the following command to delete *all*
   generated files, including the :file:`.config` files that contain
   the application's current configuration information for those board
   types.

   .. code-block:: console

       ninja pristine

#. Rebuild the application normally following the steps specified
   in :ref:`build_an_application` above.


.. _application_run:

Run an Application
******************

An application image can be run on a real board or emulated hardware.

Running on a Board
==================

Most boards supported by Zephyr let you flash a compiled binary using
the CMake ``flash`` target to copy the binary to the board and run it.
Follow these instructions to flash and run an application on real
hardware:

#. Build your application, as described in :ref:`build_an_application`.

#. Make sure your board is attached to your host computer. Usually, you'll do
   this via USB.

#. Run this console command from the build directory, :file:`<home>/app/build`,
   to flash the compiled Zephyr binary and run it on your board:

   .. code-block:: console

      ninja flash

The Zephyr build system integrates with the board support files to
use hardware-specific tools to flash the Zephyr binary to your
hardware, then run it.

Each time you run the flash command, your application is rebuilt and flashed
again.

In cases where board support is incomplete, flashing via the Zephyr build
system may not be supported. If you receive an error message about flash
support being unavailable, consult :ref:`your board's documentation <boards>`
for additional information on how to flash your board.

.. note:: When developing on Linux, it's common to need to install
          board-specific udev rules to enable USB device access to
          your board as a non-root user. If flashing fails,
          consult your board's documentation to see if this is
          necessary.


Running in an Emulator
======================

The kernel has built-in emulator support for QEMU (on Linux/macOS only, this
is not yet supported on Windows). It allows you to run and test an application
virtually, before (or in lieu of) loading and running it on actual target
hardware. Follow these instructions to run an application via QEMU:

#. Build your application for one of the QEMU boards, as described in
   :ref:`build_an_application`.

   For example, you could set ``BOARD`` to:

   - ``qemu_x86`` to emulate running on an x86-based board
   - ``qemu_cortex_m3`` to emulate running on an ARM Cortex M3-based board

#. Run this console command from the build directory, :file:`<home>/app/build`,
   to flash the compiled Zephyr binary and run it in QEMU:

   .. code-block:: console

      ninja run

#. Press :kbd:`Ctrl A, X` to stop the application from running
   in QEMU.

   The application stops running and the terminal console prompt
   redisplays.

Each time you execute the run command, your application is rebuilt and run
again.


.. note:: The ``run`` target will use the QEMU binary available from the Zephyr
          SDK by default. To use an alternate version of QEMU, for example the
          version installed on your host or a custom version, set the
          environment variable ``QEMU_BIN_PATH`` to the alternate path.

.. _application_debugging:
.. _custom_board_definition:

Custom Board Definition
***********************

In cases where the board or platform you are developing for is not yet supported
by Zephyr, you can add the board definition to your application and build for
this board without having to add it to the Zephyr tree.

The structure needed to support out-of-tree board development
is similar to how boards are maintained in the Zephyr tree.  By using
this structure, it will be much easier to upstream your board work into
the Zephyr tree after your initial development is done.

Add the custom board to your application using the following structure:

.. code-block:: console

   boards/
   CMakeLists.txt
   prj.conf
   README.rst
   src/

where the ``boards`` directory hosts the board you are building for:

.. code-block:: console

   .
   ├── boards
   │   └── x86
   │       └── my_custom_board
   │           ├── doc
   │           │   └── img
   │           └── support
   └── src


Use the proper architecture folder name (e.g., ``x86``, ``arm``, etc.)
under ``boards`` for ``my_custom_board``.  (See  :ref:`boards` for a
list of board architectures.)

Documentation (under ``doc/``) and support files (under ``support/``) are optional, but
will be needed when submitting to Zephyr.

The contents of ``my_custom_board`` should follow the same guidelines for any
Zephyr board, and provide the following files::

    my_custom_board_defconfig
    my_custom_board.dts
    my_custom_board.yaml
    board.cmake
    board.h
    CMakeLists.txt
    doc/
    dts.fixup
    Kconfig.board
    Kconfig.defconfig
    pinmux.c
    support/


Once the board structure is in place, you can build your application
targeting this board by specifying the location of your custom board
information with the ``-DBOARD_ROOT`` parameter to the CMake
build system::

   cmake -DBOARD=<board name> -DBOARD_ROOT=<path to boards> ..


This will use your custom board configuration and will generate the
Zephyr binary into your application directory.

You can also define the ``BOARD_ROOT`` variable in the application
:file:`CMakeLists.txt` file.

Application Debugging
*********************

This section is a quick hands-on reference to start debugging your
application with QEMU. Most content in this section is already covered in
`QEMU`_ and `GNU_Debugger`_ reference manuals.

.. _QEMU: http://wiki.qemu.org/Main_Page

.. _GNU_Debugger: http://www.gnu.org/software/gdb

In this quick reference, you'll find shortcuts, specific environmental
variables, and parameters that can help you to quickly set up your debugging
environment.

The simplest way to debug an application running in QEMU is using the GNU
Debugger and setting a local GDB server in your development system through QEMU.

You will need an Executable and Linkable Format (ELF) binary image for
debugging purposes.  The build system generates the image in the build
directory.  By default, the kernel binary name is
:file:`zephyr.elf`. The name can be changed using a Kconfig option.

We will use the standard 1234 TCP port to open a :abbr:`GDB (GNU Debugger)`
server instance. This port number can be changed for a port that best suits the
development environment.

You can run QEMU to listen for a "gdb connection" before it starts executing any
code to debug it.

.. code-block:: bash

   qemu -s -S <image>

will setup Qemu to listen on port 1234 and wait for a GDB connection to it.

The options used above have the following meaning:

* ``-S`` Do not start CPU at startup; rather, you must type 'c' in the
  monitor.
* ``-s`` Shorthand for :literal:`-gdb tcp::1234`: open a GDB server on
  TCP port 1234.

To debug with QEMU and to start a GDB server and wait for a remote connect, run
the following inside the build directory of an application:

.. code-block:: bash

   ninja debugserver

The build system will start a QEMU instance with the CPU halted at startup
and with a GDB server instance listening at the TCP port 1234.

Using a local GDB configuration :file:`.gdbinit` can help initialize your GDB
instance on every run.
In this example, the initialization file points to the GDB server instance.
It configures a connection to a remote target at the local host on the TCP
port 1234. The initialization sets the kernel's root directory as a
reference.

The :file:`.gdbinit` file contains the following lines:

.. code-block:: bash

   target remote localhost:1234
   dir ZEPHYR_BASE

.. note::

   Substitute ZEPHYR_BASE for the current kernel's root directory.

Execute the application to debug from the same directory that you chose for
the :file:`gdbinit` file. The command can include the ``--tui`` option
to enable the use of a terminal user interface. The following commands
connects to the GDB server using :file:`gdb`. The command loads the symbol
table from the elf binary file. In this example, the elf binary file name
corresponds to :file:`zephyr.elf` file:

.. code-block:: bash

   ..../path/to/gdb --tui zephyr.elf

.. note::

   The GDB version on the development system might not support the --tui
   option. Please make sure you use the GDB binary from the SDK which
   corresponds to the toolchain that has been used to build the binary.

If you are not using a .gdbinit file, issue the following command inside GDB to
connect to the remote GDB server on port 1234:

.. code-block:: bash

   (gdb) target remote localhost:1234

Finally, the command below connects to the GDB server using the Data
Displayer Debugger (:file:`ddd`). The command loads the symbol table from the
elf binary file, in this instance, the :file:`zephyr.elf` file.

The :abbr:`DDD (Data Displayer Debugger)` may not be installed in your
development system by default. Follow your system instructions to install
it. For example, use ``sudo apt-get install ddd`` on an Ubuntu system.

.. code-block:: bash

   ddd --gdb --debugger "gdb zephyr.elf"


Both commands execute the :abbr:`gdb (GNU Debugger)`. The command name might
change depending on the toolchain you are using and your cross-development
tools.

Eclipse Debugging
*****************

Overview
========

CMake supports generating a project description file that can be imported into
the Eclipse Integrated Development Environment (IDE) and used for graphical
debugging.

The `GNU MCU Eclipse plug-ins`_ provide a mechanism to debug ARM projects in
Eclipse with pyOCD, Segger J-Link, and OpenOCD debugging tools.

The following tutorial demonstrates how to debug a Zephyr application in
Eclipse with pyOCD in Windows. It assumes you have already installed the GCC
ARM Embedded toolchain and pyOCD.

Set Up the Eclipse Development Environment
==========================================

#. Download and install `Eclipse IDE for C/C++ Developers`_.

#. In Eclipse, install the GNU MCU Eclipse plug-ins by opening the menu
   ``Window->Eclipse Marketplace...``, searching for ``GNU MCU Eclipse``, and
   clicking ``Install`` on the matching result.

#. Configure the path to the pyOCD GDB server by opening the menu
   ``Window->Preferences``, navigating to ``MCU``, and setting the ``Global
   pyOCD Path``.

Generate and Import an Eclipse Project
======================================

#. At a command line, configure your environment to use the GCC ARM Embedded
   compiler as shown in :ref:`third_party_x_compilers`.

#. Navigate to a folder outside of the Zephyr tree to build your application.

   .. code-block:: console

      # On Windows
      cd %userprofile%

   .. note::
      If the build directory is a subdirectory of the source directory, as is
      usually done in Zephyr, CMake will warn:

      "The build directory is a subdirectory of the source directory.

      This is not supported well by Eclipse.  It is strongly recommended to use
      a build directory which is a sibling of the source directory."

#. Configure your application with CMake and build it with ninja. Note the
   different CMake generator specified by the ``-G"Eclipse CDT4 - Ninja"``
   argument. This will generate an Eclipse project description file,
   :file:`.project`, in addition to the usual ninja build files.

   .. code-block:: console

      # On Windows
      mkdir build && cd build
      cmake -G"Eclipse CDT4 - Ninja" -DBOARD=frdm_k64f %ZEPHYR_BASE%\samples\synchronization
      ninja

#. In Eclipse, import your generated project by opening the menu
   ``File->Import...`` and selecting the option ``Existing Projects into
   Workspace``. Browse to your application build directory in the choice,
   ``Select root directory:``. Check the box for your project in the list of
   projects found and click the ``Finish`` button.

Create a Debugger Configuration
===============================

#. Open the menu ``Run->Debug Configurations...``.

#. Select ``GDB PyOCD Debugging``, click the ``New`` button, and configure the
   following options:

   - In the Main tab:

     - Project: NONE@build
     - C/C++ Application: :file:`zephyr/zephyr.elf`

   - In the Debugger tab:

     - pyOCD Setup

       - Executable path: :file:`${pyocd_path}\\${pyocd_executable}`
       - Uncheck "Allocate console for semihosting"

     - Board Setup

       - Bus speed: 8000000 Hz
       - Uncheck "Enable semihosting"

     - GDB Client Setup

       - Executable path:
         :file:`C:\\gcc-arm-none-eabi-6_2017-q2-update\\bin\\arm-none-eabi-gdb.exe`

   - In the SVD Path tab:

     - File path: :file:`<zephyr
       base>\\ext\\hal\\nxp\\mcux\\devices\\MK64F12\\MK64F12.xml`

     .. note::
	This is optional. It provides the SoC's memory-mapped register
	addresses and bitfields to the debugger.

#. Click the ``Debug`` button to start debugging.

RTOS Awareness
==============

Support for Zephyr RTOS awareness is implemented in `pyOCD v0.11.0`_ and later.
It is compatible with GDB PyOCD Debugging in Eclipse, but you must enable
CONFIG_OPENOCD_SUPPORT=y in your application.

CMake Details
*************

Overview
========

CMake is used to build your application together with the Zephyr kernel. A
CMake build is done in two stages. The first stage is called
**configuration**. During configuration, the CMakeLists.txt build scripts are
executed. After configuration is finished, CMake has an internal model of the
Zephyr build, and can generate build scripts that are native to the host
platform.

CMake supports generating scripts for several build systems, but only Ninja and
Make are tested and supported by Zephyr. After configuration, you begin the
**build** stage by executing the generated build scripts. These build scripts
can recompile the application without involving CMake following
most code changes. However, after certain changes, the configuration step must
be executed again before building. The build scripts can detect some of these
situations and reconfigure automatically, but there are cases when this must be
done manually.

Zephyr uses CMake's concept of a 'target' to organize the build. A
target can be an executable, a library, or a generated file. For
application developers, the library target is the most important to
understand. All source code that goes into a Zephyr build does so by
being included in a library target, even application code.

Library targets have source code, that is added through CMakeLists.txt
build scripts like this:

.. code-block:: cmake

   target_sources(app PRIVATE src/main.c)

In the above :file:`CMakeLists.txt`, an existing library target named ``app``
is configured to include the source file :file:`src/main.c`. The ``PRIVATE``
keyword indicates that we are modifying the internals of how the library is
being built. Using the keyword ``PUBLIC`` would modify how other
libraries that link with app are built. In this case, using ``PUBLIC``
would cause libraries that link with ``app`` to also include the
source file :file:`src/main.c`, behavior that we surely do not want. The
``PUBLIC`` keyword could however be useful when modifying the include
paths of a target library.

Application CMakeLists.txt
==========================

Every application must have a :file:`CMakeLists.txt` file. This file is the
entry point, or top level, of the build system. The final :file:`zephyr.elf`
image contains both the application and the kernel libraries.

This section describes some of what you can do in your :file:`CMakeLists.txt`.
Make sure to follow these steps in order.

#. If you only want to build for one board, add the name of the board
   configuration for your application on a new line. For example:

   .. code-block:: cmake

      set(BOARD qemu_x86)

   Refer to :ref:`boards` for more information on available boards.

   The Zephyr build system determines a value for :makevar:`BOARD` by checking
   the following, in order (when a BOARD value is found, CMake stops looking
   further down the list):

   - Any previously used value as determined by the CMake cache takes highest
     precedence. This ensures you don't try to run a build with a different
     :makevar:`BOARD` value than you set during the build configuration step.

   - Any value given on the CMake command line using ``-DBOARD=YOUR_BOARD``
     will be checked for and used next.

   - If an environment variable ``BOARD`` is set, its value will then be used.

   - Finally, if you set ``BOARD`` in your application :file:`CMakeLists.txt`
     as described in this step, this value will be used.

#. If your application uses a configuration file or files other than
   the usual :file:`prj.conf` (or :file:`prj_YOUR_BOARD.conf`, where
   ``YOUR_BOARD`` is a board name), add lines setting the
   :makevar:`CONF_FILE` variable to these files appropriately.
   If multiple filenames are given, separate them by a single space or
   semicolon.  CMake lists can be used to build up configuration fragment
   files in a modular way when you want to avoid setting :makevar:`CONF_FILE`
   in a single place. For example:

   .. code-block:: cmake

     set(CONF_FILE "fragment_file1.conf")
     list(APPEND CONF_FILE "fragment_file2.conf")

   More details are available below in :ref:`application_kconfig`.

#. If your application uses a Device Tree overlay file or files other than
   the usual :file:`<board>.overlay`, add lines setting the
   :makevar:`DTC_OVERLAY_FILE` variable to these files appropriately.

   More details are available below in :ref:`application_dt`.

#. If your application has its own kernel configuration options, add a
   line setting the location of the Kconfig file that defines them.

   An (unlikely) advanced use case would be if your application has its own
   unique configuration **options** that are set differently depending on the
   build configuration.

   If you just want to set application specific **values** for existing Zephyr
   configuration options, refer to the :makevar:`CONF_FILE` description above.

   For example, if you have a file named :file:`Kconfig` in the same directory
   as your application's :file:`CMakeLists.txt`, add the following line:

   .. code-block:: cmake

      set(KCONFIG_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/Kconfig)

   Structure your :file:`Kconfig` file like this:

   .. literalinclude:: application-kconfig.include

   .. note::

       Environment variables in ``source`` statements are expanded directly,
       so you do not need to define an ``option env="ZEPHYR_BASE"`` Kconfig
       "bounce" symbol. If you use such a symbol, it must have the same name as
       the environment variable.

       See the :ref:`kconfig_extensions` section in the
       :ref:`board_porting_guide` for more information.

#. Now include the mandatory boilerplate that integrates the
   application with the Zephyr build system on a new line, **after any
   lines added from the steps above**:

   .. code-block:: cmake

      include($ENV{ZEPHYR_BASE}/cmake/app/boilerplate.cmake NO_POLICY_SCOPE)
      project(NONE)

#. Now add any application source files to the 'app' target
   library, each on their own line, like so:

   .. code-block:: cmake

      target_sources(app PRIVATE src/main.c)

Below is a simple example :file:`CMakeList.txt`:

.. code-block:: cmake

   set(BOARD qemu_x86)

   include($ENV{ZEPHYR_BASE}/cmake/app/boilerplate.cmake NO_POLICY_SCOPE)
   project(NONE)

   target_sources(app PRIVATE src/main.c)

CMakeCache.txt
==============

CMake uses a CMakeCache.txt file as persistent key/value string
storage used to cache values between runs, including compile and build
options and paths to library dependencies. This cache file is created
when CMake is run in an empty build folder.

For more details about the CMakeCache.txt file see the official CMake
documentation `runningcmake`_ .

.. _runningcmake: http://cmake.org/runningcmake/

.. _application_configuration:

Application Configuration
*************************

.. _application_kconfig:

Kconfig Configuration
=====================

The initial configuration for an application is produced by merging
configuration settings from three sources:

1. A :makevar:`BOARD`-specific configuration file, stored in
   :file:`boards/ARCHITECTURE/BOARD/BOARD_defconfig` in the Zephyr base
   directory.

2. Any CMakeCache entries entries that are prefixed with :makevar:`CONFIG_`.

3. One or more application-specific configuration files.

The application-specific configuration file(s) can be specified in any of the
following ways. The simplest option is to just have a single :file:`prj.conf`
file.

1. If :makevar:`CONF_FILE` is set in :file:`CMakeLists.txt` (**before including
   the boilerplate.cmake file**), or is present in the CMake variable cache,
   the configuration files specified in it are merged and used as the
   application-specific settings.

   Alternatively, an application may define a CMake command, macro, or function
   called ``set_conf_file``, which is invoked and is expected to set
   :makevar:`CONF_FILE`.

2. Otherwise (if (1.) does not apply), if a file :file:`prj_BOARD.conf` exists
   in the application directory, where :makevar:`BOARD` is the BOARD value set
   earlier, the settings in it are used.

3. Otherwise, if a file :file:`prj.conf` exists in the application directory,
   the settings in it are used.

Configuration settings that have not been specified fall back on their
default value, as given in the :file:`Kconfig` files.

The merged configuration is saved in :file:`zephyr/.config` in the build
directory.

As long as :file:`zephyr/.config` exists and is up-to-date (is newer than the
:makevar:`BOARD` and application configuration files), it will be used in
preference to producing a new merged configuration. This can be used during
development, as described below in :ref:`override_kernel_conf`.

For more information on Zephyr's Kconfig configuration scheme, see the
:ref:`setting_configuration_values` section in the :ref:`board_porting_guide`.

For information on available kernel configuration options, including
inter-dependencies between options, see the :ref:`configuration`.

.. note::

    Dependencies between options can also be viewed in the interactive
    configuration interface, which is explained in the
    :ref:`override_kernel_conf` section. It will have the most up-to-date
    dependencies, and also shows which dependencies are currently unsatisfied.

    To view the dependencies of an option in the configuration interface, jump
    to it with :kbd:`/` and press :kbd:`?`. For each unsatisfied dependency,
    jump to it in turn to check its dependencies.

.. _application_set_conf:

Setting Application Configuration Values
----------------------------------------

This section describes how to edit Zephyr configuration
(:file:`.conf`) files.

- Add each configuration entry on a new line.

- Enable a boolean option by setting its value to ``y``:

  .. code-block:: none

     CONFIG_SOME_BOOL=y

  To ensure that a boolean configuration option is not set, add a line
  like this instead (including the leading ``#`` symbol):

  .. code-block:: none

     # CONFIG_SOME_BOOL is not set

- You can set integer and string options as well, like this:

  .. code-block:: none

     CONFIG_SOME_INT=42
     CONFIG_SOME_STRING="the best value ever"

- Ensure that each entry setting an option contains no spaces
  (including on either side of the = sign).

- Use a # followed by a space to comment a line:

  .. code-block:: none

     # This is a comment.

The example below shows a comment line and an override setting
:option:`CONFIG_PRINTK` to ``y``:

.. code-block:: c

    # Enable printk for debugging
    CONFIG_PRINTK=y

.. _override_kernel_conf:

Overriding the Default Configuration
------------------------------------

An interactive configuration interface is available for making temporary
changes to the configuration. This can be handy during development.

.. note::

   The configuration can also be changed by editing :file:`zephyr/.config` in
   the application build directory by hand. Using the configuration interface
   is safer, as it correctly handles dependencies between configurations
   symbols.

To make a setting permanent, you should set it in a :file:`.conf` file, as
described above in :ref:`application_set_conf`.

The steps below will run the interactive configuration interface:

#. Create a build directory :file:`<home>/app/build` inside your application
   directory and generate build files inside it with CMake, as follows:

   .. code-block:: bash

      # On Linux/macOS
      cd ~/app
      # On Windows
      cd %userprofile%\app

      mkdir build && cd build
      cmake -GNinja ..

#. Run the following command from the build directory (:file:`<home>/app/build`)
   to start the configuration interface:

   .. code-block:: bash

       ninja menuconfig

   The configuration interface is shown below:

   .. image:: figures/app_kernel_conf_1.png
        :align: center
        :alt: Main Configuration Menu

#. Change configuration symbols to their desired values as follows:

   * Use the arrow keys to navigate the menu.

     .. note::

        Common `Vim <https://www.vim.org>`_ key bindings are supported as well.

   * Press :kbd:`Enter` or :kbd:`Space` to enter submenus and choices, which
     appear with ``--->`` next to them. Press :kbd:`ESC` returns to the parent
     menu.

   * Press :kbd:`Space` to toggle or configure a symbol value. Boolean
     configuration symbols are shown with :guilabel:`[ ]` brackets, while
     numeric and string-valued configuration symbols are shown with
     :guilabel:`( )` brackets.

     .. note::

        You can also press :kbd:`Y` or :kbd:`N` to set a boolean configuration
        symbol, to the corresponding value.

   * Press :kbd:`?` to display information about the currently selected symbol.
     Press :kbd:`ESC` or :kbd:`Q` to return from the information display to the
     menu.

#. After configuring the kernel options for your application, press
   :kbd:`Q` to bring up the save-and-quit dialog:

   .. image:: figures/app_kernel_conf_2.png
      :align: center
      :alt: Save and Quit Dialog

#. Press :kbd:`Y` to save the kernel configuration options to the default
   filename (:file:`zephyr/.config`).

   Typically, you will save to the default filename unless you are
   experimenting with various configuration scenarios.

   .. note::

      At present, the configuration file used during building is always
      :file:`zephyr/.config`. If you have another saved configuration that you
      want to build with, copy it to :file:`zephyr/.config`. Make sure to back
      up your original configuration file.

      Also note that filenames starting with ``.`` are not listed by ``ls`` by
      default on Linux and macOS. Use the ``-a`` flag to see them.

Finding a symbol in the menu tree and navigating to it can be tedious. To jump
directly to a symbol, press the :kbd:`/` key. This brings up the following
dialog, where you can search for symbols by name and jump to them:

.. image:: figures/app_kernel_conf_3.png
    :align: center
    :alt: Menuconfig Search Dialog

If you jump to a symbol that isn't currently visible (e.g., due to having
unsatisfied dependencies) then *show-all mode* will be enabled. In show-all
mode, all symbols are displayed, including currently invisible symbols. To
disable show-all mode, press :kbd:`A`.

.. note::

    Show-all mode can't be disabled if there are no visible items in the menu.

To figure out why a symbol you jumped to isn't visible, inspect its
dependencies by pressing :kbd:`?`. If you discover that the symbol depends on
another symbol that isn't enabled, you can jump to that symbol, in turn, to see
if it can be enabled.

.. _application_dt:

Device Tree Overlays
====================

As described in :ref:`device-tree`, Zephyr uses Device Tree to
describe the hardware it runs on. This section describes how you can
modify an application build's device tree using overlay files. For additional
information regarding the relationship between Device Tree and Kconfig see
:ref:`dt_vs_kconfig`.

Overlay files, which customarily have the :file:`.overlay` extension,
contain device tree fragments which add to or modify the device tree
used while building a Zephyr application. To add an overlay file or
files to the build, set the CMake variable :makevar:`DTC_OVERLAY_FILE`
to a whitespace-separated list of your overlay files.

The Zephyr build system begins creation of a device tree by running
the C preprocessor on a file which includes the following:

#. Configuration options from :ref:`Kconfig <configuration>`.

#. The board's device tree source file, which by default is the Zephyr
   file :file:`boards/<ARCHITECTURE>/<BOARD>/<BOARD>.dts`. (This location
   can be overridden by setting the :makevar:`DTS_SOURCE` CMake
   variable.)

#. Any "common" overlays provided by the build system. Currently, this
   is just the file :file:`dts/common/common.dts`. (The common
   overlays can be overridden by setting the
   :makevar:`DTS_COMMON_OVERLAYS` CMake variable.)

   The file :file:`common.dts` conditionally includes device tree
   fragments based on Kconfig settings. For example, it includes a
   fragment for MCUboot chain-loading, located at
   :file:`dts/common/mcuboot.overlay`, if
   :option:`CONFIG_BOOTLOADER_MCUBOOT` is set.

#. Any file or files given by the :makevar:`DTC_OVERLAY_FILE` CMake
   variable.

The Zephyr build system determines :makevar:`DTC_OVERLAY_FILE` as
follows:

- Any value given to :makevar:`DTC_OVERLAY_FILE` in your application
  :file:`CMakeLists.txt` (**before including the boilerplate.cmake file**),
  passed to the the CMake command line, or present in the CMake variable cache,
  takes precedence.

- The environment variable :envvar:`DTC_OVERLAY_FILE` is checked
  next. This mechanism is now deprecated; users should set this
  variable using CMake instead of the environment.

- If the file :file:`BOARD.overlay` exists in your application directory,
  where ``BOARD`` is the BOARD value set earlier, it will be used.

If :makevar:`DTC_OVERLAY_FILE` specifies multiple files, they are
included in order by the C preprocessor.

After running the preprocessor, the final device tree used in the
build is created by running the device tree compiler, ``dtc``, on the
preprocessor output.

Application-Specific Code
*************************

Application-specific source code files are normally added to the
application's :file:`src` directory. If the application adds a large
number of files the developer can group them into sub-directories
under :file:`src`, to whatever depth is needed.

Application-specific source code should not use symbol name prefixes that have
been reserved by the kernel for its own use. For more information, see `Naming
Conventions
<https://github.com/zephyrproject-rtos/zephyr/wiki/Naming-Conventions>`_.

Support for building third-party library code
=============================================

It is possible to build library code outside the application's :file:`src`
directory but it is important that both application and library code targets
the same Application Binary Interface (ABI). On most architectures there are
compiler flags that control the ABI targeted, making it important that both
libraries and applications have certain compiler flags in common. It may also
be useful for glue code to have access to Zephyr kernel header files.

To make it easier to integrate third-party components, the Zephyr
build system has defined CMake functions that give application build
scripts access to the zephyr compiler options. The functions are
documented and defined in :file:`$ZEPHYR_BASE/cmake/extensions.cmake`
and follow the naming convention ``zephyr_get_<type>_<format>``.

The following variables will often need to be exported to the
third-party build system.

* ``CMAKE_C_COMPILER``, ``CMAKE_AR``.

* ``ARCH`` and ``BOARD``, together with several variables that identify the
  Zephyr kernel version.

:file:`samples/application_development/external_lib` is a sample
project that demonstrates some of these features.

.. _Eclipse IDE for C/C++ Developers: https://www.eclipse.org/downloads/packages/eclipse-ide-cc-developers/oxygen2
.. _GNU MCU Eclipse plug-ins: https://gnu-mcu-eclipse.github.io/plugins/install/
.. _pyOCD v0.11.0: https://github.com/mbedmicro/pyOCD/releases/tag/v0.11.0
