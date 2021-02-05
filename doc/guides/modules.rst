.. _modules:

Modules (External projects)
############################

Zephyr relies on the source code of several externally maintained projects in
order to avoid reinventing the wheel and to reuse as much well-established,
mature code as possible when it makes sense. In the context of Zephyr's build
system those are called *modules*. These modules must be integrated with the
Zephyr build system, as described in more detail in other sections on
this page.

To be classified as a candidate for being included in the default list of
modules, an external project is required to have its own life-cycle outside
the Zephyr Project, that is, reside in its own repository, and have its own
contribution and maintenance workfow and release process. Zephyr modules
should not contain code that is written exclusively for Zephyr. Instead,
such code should be contributed to the main zephyr tree.

Modules to be included in the default manifest of the Zephyr project need to
provide functionality or features endorsed and approved by the project Technical
Steering Committee and should comply with the
:ref:`module licensing requirements<modules_licensing>` and
:ref:`contribute guidelines<modules_contributing>`. They should also have a
Zephyr developer that is committed to maintain the module codebase.

Zephyr depends on several categories of modules, including but not limited to:

- Debugger integration
- Silicon vendor Hardware Abstraction Layers (HALs)
- Cryptography libraries
- File Systems
- Inter-Process Communication (IPC) libraries

This page summarizes a list of policies and best practices which aim at
better organizing the workflow in Zephyr modules.

Module Repositories
*******************

* All modules included in the default manifest shall be hosted in repositories
  under the zephyrproject-rtos GitHub organization.

* The module repository codebase shall include a *module.yml* file in a
  :file:`zephyr/` folder at the root of the repository.

* Module repository names should follow the convention of using lowercase
  letters and dashes instead of underscores. This rule will apply to all
  new module repositories, except for repositories that are directly
  tracking external projects (hosted in Git repositories); such modules
  may be named as their external project counterparts.

  .. note::

     Existing module repositories that do not conform to the above convention
     do not need to be renamed to comply with the above convention.

* Modules should use "zephyr" as the default name for the repository main
  branch. Branches for specific purposes, for example, a module branch for
  an LTS Zephyr version, shall have names starting with the 'zephyr\_' prefix.

* If the module has an external (upstream) project repository, the module
  repository should preserve the upstream repository folder structure.

  .. note::

     It is not required in module repositories to maintain a 'master'
     branch mirroring the master branch of the external repository. It
     is not recommended as this may generate confusion around the module's
     main branch, which should be 'zephyr'.

.. _modules_synchronization:

Synchronizing with upstream
===========================

It is preferred to synchronize a module respository with the latest stable
release of the corresponding external project. It is permitted, however, to
update a Zephyr module repository with the latest development branch tip,
if this is required to get important updates in the module codebase. When
synchronizing a module with upstream it is mandatory to document the
rationale for performing the particular update.

Requirements for allowed practices
----------------------------------

Changes to the main branch of a module repository, including synchronization
with upstream code base, may only be applied via pull requests. These pull
requests shall be *verifiable* by Zephyr CI and *mergeable* (e.g. with the
*Rebase and merge*, or *Create a merge commit* option using Github UI). This
ensures that the incoming changes are always **reviewable**, and the
*downstream* module repository history is incremental (that is, existing
commits, tags, etc. are always preserved). This policy also allows to run
Zephyr CI, git lint, identity, and license checks directly on the set of
changes that are to be brought into the module repository.

.. note::

     Force-pushing to a module's main branch is not allowed.

Allowed practices
---------------------

The following practices conform to the above requirements and should be
followed in all modules repositories. It is up to the module code owner
to select the preferred synchronization practice, however, it is required
that the selected practice is consistently followed in the respective
module repository.

**Updating modules with a diff from upstream:**
Upstream changes brought as a single *snapshot* commit (manual diff) in a
pull request against the module's main branch, which may be merged using
the *Rebase & merge* operation. This approach is simple and
should be applicable to all modules with the downside of supressing the
upstream history in the module repository.

  .. note::

     The above practice is the only allowed practice in modules where
     the external project is not hosted in an upstream Git repository.

The commit message is expected to identify the upstream project URL, the
version to which the module is updated (upstream version, tag, commit SHA,
if applicable, etc.), and the reason for the doing the update.

**Updating modules by merging the upstream branch:**
Upstream changes brought in by performing a Git merge of the intended upstream
branch (e.g. main branch, latest release branch, etc.) submitting the result in
pull request against the module main branch, and merging the pull request using
the *Create a merge commit* operation.
This approach is applicable to modules with an upstream project Git repository.
The main advantages of this approach is that the upstream repository history
(that is, the original commit SHAs) is preserved in the module repository. The
downside of this approach is that two additional merge commits are generated in
the downstream main branch.


Contributing to Zephyr modules
******************************

.. _modules_contributing:


Individual Roles & Responsibilities
===================================

To facilitate management of Zephyr module repositories, the following
individual roles are defined.

**Administrator:** Each Zephyr module shall have an administrator
who is responsible for managing access to the module repository,
for example, for adding individuals as Collaborators in the repository
at the request of the module owner. Module administrators are
members of the Administrators team, that is a group of project
members with admin rights to module GitHub repositories.

**Module owner:** Each module shall have a module code owner. Module
owners will have the overall responsibility of the contents of a
Zephyr module repository. In particular, a module owner will:

* coordinate code reviewing in the module repository
* be the default assignee in pull-requests against the repository's
  main branch
* request additional collaborators to be added to the repository, as
  they see fit
* regularly synchronize the module repository with its upstream
  counterpart following the policies described in
  :ref:`modules_synchronization`
* be aware of security vulnerability issues in the external project
  and update the module repository to include security fixes, as
  soon as the fixes are available in the upstream code base
* list any known security vulnerability issues, present in the
  module codebase, in Zephyr release notes.


  .. note::

     Module owners are not required to be Zephyr
     :ref:`Maintainers <project_roles>`.

**Merger:** The Zephyr Release Engineering team has the right and the
responsibility to merge approved pull requests in the main branch of a
module repository.


Maintaining the module codebase
===============================

Updates in the zephyr main tree, for example, in public Zephyr APIs,
may require patching a module's codebase. The responsibility for keeping
the module codebase up to date is shared between the **contributor** of
such updates in Zephyr and the module **owner**. In particular:

* the contributor of the original changes in Zephyr is required to submit
  the corresponding changes that are required in module repositories, to
  ensure that Zephyr CI on the pull request with the original changes, as
  well as the module integration testing are successful.

* the module owner has the overall responsibility for synchronizing
  and testing the module codebase with the zephyr main tree.
  This includes occasional advanced testing of the module's codebase
  in addition to the testing performed by Zephyr's CI.
  The module owner is required to fix issues in the module's codebase that
  have not been caught by Zephyr pull request CI runs.



Contributing changes to modules
===============================

Submitting and merging changes directly to a module's codebase, that is,
before they have been merged in the corresponding external project
repository, should be limited to:

* changes required due to updates in the zephyr main tree
* urgent changes that should not wait to be merged in the external project
  first, such as fixes to security vulnerabilities.

Non-trivial changes to a module's codebase, including changes in the module
design or functionality should be discouraged, if the module has an upstream
project repository. In that case, such changes shall be submitted to the
upstream project, directly.

:ref:`Submitting changes to modules <submitting_new_modules>` describes in
detail the process of contributing changes to module repositories.

Contribution guidelines
-----------------------

Contributing to Zephyr modules shall follow the generic project
:ref:`Contribution guidelines <contribute_guidelines>`.

**Pull Requests:** may be merged with minimum of 2 approvals, including
an approval by the PR assignee. In addition to this, pull requests in module
repositories may only be merged if the introduced changes are verified
with Zephyr CI tools, as described in more detail in other sections on
this page.

The merging of pull requests in the main branch of a module
repository must be coupled with the corresponding manifest
file update in the zephyr main tree.

**Issue Reporting:** GitHub issues are intentionally disabled in module
repositories, in
favor of a centralized policy for issue reporting. Tickets concerning, for
example, bugs or enhancements in modules shall be opened in the main
zephyr repository. Issues should be appropriately labeled using GitHub
labels corresponding to each module, where applicable.

  .. note::

     It is allowed to file bug reports for zephyr modules to track
     the corresponding upstream project bugs in Zephyr. These bug reports
     shall not affect the
     :ref:`Release Quality Criteria<release_quality_criteria>`.


.. _modules_licensing:

Licensing requirements and policies
***********************************

All source files in a module's codebase shall include a license header,
unless the module repository has **main license file** that covers source
files that do not include license headers.

Main license files shall be added in the module's codebase by Zephyr
developers, only if they exist as part of the external project,
and they contain a permissive OSI-compliant license. Main license files
should preferably contain the full license text instead of including an
SPDX license identifier. If multiple main license files are present it
shall be made clear which license applies to each source file in a module's
codebase.

Individual license headers in module source files supersede the main license.

Any new content to be added in a module repository will require to have
license coverage.

  .. note::

     Zephyr recommends conveying module licensing via individual license
     headers and main license files. This not a hard requirement; should
     an external project have its own practice of conveying how licensing
     applies in the module's codebase (for example, by having a single or
     multiple main license files), this practice may be accepted by and
     be referred to in the Zephyr module, as long as licensing requirements,
     for example OSI compliance, are satisfied.

License policies
================

When creating a module repository a developer shall:

* import the main license files, if they exist in the external project, and
* document (for example in the module README or .yml file) the default license
  that covers the module's codebase.

License checks
--------------

License checks (via CI tools) shall be enabled on every pull request that
adds new content in module repositories.


Documentation requirements
**************************

All Zephyr module repositories shall include an .rst file documenting:

* the scope and the purpose of the module
* how the module integrates with Zephyr
* the owner of the module repository
* synchronization information with the external project (commit, SHA, version etc.)
* licensing information as described in :ref:`modules_licensing`.

The file shall be required for the inclusion of the module and the contained
information should be kept up to date.


Testing requirements
********************

All Zephyr modules should provide some level of **integration** testing,
ensuring that the integration with Zephyr works correctly.
Integration tests:

* may be in the form of a minimal set of samples and tests that reside
  in the zephyr main tree
* should verify basic usage of the module (configuration,
  functional APIs, etc.) that is integrated with Zephyr.
* shall be built and executed (for example in QEMU) as part of
  twister runs in pull requests that introduce changes in module
  repositories.

  .. note::

     New modules, that are candidates for being included in the Zephyr
     default manifest, shall provide some level of integration testing.

  .. note::

     Vendor HALs are implicitly tested via Zephyr tests built or executed
     on target platforms, so they do not need to provide integration tests.

The purpose of integration testing is not to provide functional verification
of the module; this should be part of the testing framework of the external
project.

Certain external projects provide test suites that reside in the upstream
testing infrastructure but are written explicitly for Zephyr. These tests
may (but are not required to) be part of the Zephyr test framework.

Deprecating and removing modules
*********************************

Modules may be deprecated for reasons including, but not limited to:

* Lack of maintainership in the module
* Licensing changes in the external project
* Codebase becoming obsolete

The module information shall indicate whether a module is
deprecated and the build system shall issue a warning
when trying to build Zephyr using a deprecated module.

Deprecated modules may be removed from the Zephyr default manifest
after 2 Zephyr releases.

  .. note::

     Repositories of removed modules shall remain accessible via their
     original URL, as they are required by older Zephyr versions.


Integrate modules in Zephyr build system
****************************************

The build system variable :makevar:`ZEPHYR_MODULES` is a `CMake list`_ of
absolute paths to the directories containing Zephyr modules. These modules
contain :file:`CMakeLists.txt` and :file:`Kconfig` files describing how to
build and configure them, respectively. Module :file:`CMakeLists.txt` files are
added to the build using CMake's `add_subdirectory()`_ command, and the
:file:`Kconfig` files are included in the build's Kconfig menu tree.

If you have :ref:`west <west>` installed, you don't need to worry about how
this variable is defined unless you are adding a new module. The build system
knows how to use west to set :makevar:`ZEPHYR_MODULES`. You can add additional
modules to this list by setting the :makevar:`ZEPHYR_EXTRA_MODULES` CMake
variable or by adding a :makevar:`ZEPHYR_EXTRA_MODULES` line to ``.zephyrrc``
(See the section on :ref:`env_vars` for more details). This can be
useful if you want to keep the list of modules found with west and also add
your own.

.. note::
   If the module ``FOO`` is provided by :ref:`west <west>` but also given with
   ``-DZEPHYR_EXTRA_MODULES=/<path>/foo`` then the module given by the command
   line variable :makevar:`ZEPHYR_EXTRA_MODULES` will take precedence.
   This allows you to use a custom version of ``FOO`` when building and still
   use other Zephyr modules provided by :ref:`west <west>`.
   This can for example be useful for special test purposes.

See the section about :ref:`west-multi-repo` for more details.

Finally, you can also specify the list of modules yourself in various ways, or
not use modules at all if your application doesn't need them.


Module yaml file description
****************************

A module can be described using a file named :file:`zephyr/module.yml`.
The format of :file:`zephyr/module.yml` is described in the following:

Module name
===========

Each Zephyr module is given a name by which it can be referred to in the build
system.

The name may be specified in the :file:`zephyr/module.yml` file:

.. code-block:: yaml

   name: <name>

In CMake the location of the Zephyr module can then be referred to using the
CMake variable ``ZEPHYR_<MODULE_NAME>_MODULE_DIR`` and the variable
``ZEPHYR_<MODULE_NAME>_CMAKE_DIR`` holds the location of the directory
containing the module's :file:`CMakeLists.txt` file.

.. note::
   When used for CMake and Kconfig variables, all letters in module names are
   converted to uppercase and all non-alphanumeric characters are converted
   to underscores (_).
   As example, the module ``foo-bar`` must be referred to as
   ``ZEPHYR_FOO_BAR_MODULE_DIR`` in CMake and Kconfig.

Here is an example for the Zephyr module ``foo``:

.. code-block:: yaml

   name: foo

.. note::
   If the ``name`` field is not specified then the Zephyr module name will be
   set to the name of the module folder.
   As example, the Zephyr module located in :file:`<workspace>/modules/bar` will
   use ``bar`` as its module name if nothing is specified in
   :file:`zephyr/module.yml`.

Module integration files (in-module)
====================================

Inclusion of build files, :file:`CMakeLists.txt` and :file:`Kconfig`, can be
described as:

.. code-block:: yaml

   build:
     cmake: <cmake-directory>
     kconfig: <directory>/Kconfig

The ``cmake: <cmake-directory>`` part specifies that
:file:`<cmake-directory>` contains the :file:`CMakeLists.txt` to use. The
``kconfig: <directory>/Kconfig`` part specifies the Kconfig file to use.
Neither is required: ``cmake`` defaults to ``zephyr``, and ``kconfig``
defaults to ``zephyr/Kconfig``.

Here is an example :file:`module.yml` file referring to
:file:`CMakeLists.txt` and :file:`Kconfig` files in the root directory of the
module:

.. code-block:: yaml

   build:
     cmake: .
     kconfig: Kconfig

Build system integration
========================

When a module has a :file:`module.yml` file, it will automatically be included into
the Zephyr build system. The path to the module is then accessible through Kconfig
and CMake variables.

In both Kconfig and CMake, the variable ``ZEPHYR_<MODULE_NAME>_MODULE_DIR``
contains the absolute path to the module.

In CMake, ``ZEPHYR_<MODULE_NAME>_CMAKE_DIR`` contains the
absolute path to the directory containing the :file:`CMakeLists.txt` file that
is included into CMake build system. This variable's value is empty if the
module.yml file does not specify a CMakeLists.txt.

To read these variables for a Zephyr module named ``foo``:

- In CMake: use ``${ZEPHYR_FOO_MODULE_DIR}`` for the module's top level directory, and ``${ZEPHYR_FOO_CMAKE_DIR}`` for the directory containing its :file:`CMakeLists.txt`
- In Kconfig: use ``$(ZEPHYR_FOO_MODULE_DIR)`` for the module's top level directory

Notice how a lowercase module name ``foo`` is capitalized to ``FOO``
in both CMake and Kconfig.

These variables can also be used to test whether a given module exists.
For example, to verify that ``foo`` is the name of a Zephyr module:

.. code-block:: cmake

  if(ZEPHYR_FOO_MODULE_DIR)
    # Do something if FOO exists.
  endif()

In Kconfig, the variable may be used to find additional files to include.
For example, to include the file :file:`some/Kconfig` in module ``foo``:

.. code-block:: kconfig

  source "$(ZEPHYR_FOO_MODULE_DIR)/some/Kconfig"

During CMake processing of each Zephyr module, the following two variables are
also available:

- the current module's top level directory: ``${ZEPHYR_CURRENT_MODULE_DIR}``
- the current module's :file:`CMakeLists.txt` directory: ``${ZEPHYR_CURRENT_CMAKE_DIR}``

This removes the need for a Zephyr module to know its own name during CMake
processing. The module can source additional CMake files using these ``CURRENT``
variables. For example:

.. code-block:: cmake

  include(${ZEPHYR_CURRENT_MODULE_DIR}/cmake/code.cmake)

Zephyr module dependencies
==========================

A Zephyr module may be dependent on other Zephyr modules to be present in order
to function correctly. Or it might be that a given Zephyr module must be
processed after another Zephyr module, due to dependencies of certain CMake
targets.

Such a dependency can be described using the ``depends`` field.

.. code-block:: yaml

   build:
     depends:
       - <module>

Here is an example for the Zephyr module ``foo`` that is dependent on the Zephyr
module ``bar`` to be present in the build system:

.. code-block:: yaml

   name: foo
   build:
     depends:
     - bar

This example will ensure that ``bar`` is present when ``foo`` is included into
the build system, and it will also ensure that ``bar`` is processed before
``foo``.

.. _modules_module_ext_root:

Module integration files (external)
===================================

Module integration files can be located externally to the Zephyr module itself.
The ``MODULE_EXT_ROOT`` variable holds a list of roots containing integration
files located externally to Zephyr modules.

Module integration files in Zephyr
----------------------------------

The Zephyr repository contain :file:`CMakeLists.txt` and :file:`Kconfig` build
files for certain known Zephyr modules.

Those files are located under

.. code-block:: none

   <ZEPHYR_BASE>
   └── modules
       └── <module_name>
           ├── CMakeLists.txt
           └── Kconfig

Module integration files in a custom location
---------------------------------------------

You can create a similar ``MODULE_EXT_ROOT`` for additional modules, and make
those modules known to Zephyr build system.

Create a ``MODULE_EXT_ROOT`` with the following structure

.. code-block:: none

   <MODULE_EXT_ROOT>
   └── modules
       ├── modules.cmake
       └── <module_name>
           ├── CMakeLists.txt
           └── Kconfig

and then build your application by specifying ``-DMODULE_EXT_ROOT`` parameter to
the CMake build system. The ``MODULE_EXT_ROOT`` accepts a CMake list of roots as
argument.

A Zephyr module can automatically be added to the ``MODULE_EXT_ROOT``
list using the module description file :file:`zephyr/module.yml`, see
:ref:`modules_build_settings`.

.. note::

   ``ZEPHYR_BASE`` is always added as a ``MODULE_EXT_ROOT`` with the lowest
   priority.
   This allows you to overrule any integration files under
   ``<ZEPHYR_BASE>/modules/<module_name>`` with your own implementation your own
   ``MODULE_EXT_ROOT``.

The :file:`modules.cmake` file must contain the logic that specifies the
integration files for Zephyr modules via specifically named CMake variables.

To include a module's CMake file, set the variable ``ZEPHYR_<MODULE_NAME>_CMAKE_DIR``
to the path containing the CMake file.

To include a module's Kconfig file, set the variable ``ZEPHYR_<MODULE_NAME>_KCONFIG``
to the path to the Kconfig file.

The following is an example on how to add support the the ``FOO`` module.

Create the following structure

.. code-block:: none

   <MODULE_EXT_ROOT>
   └── modules
       ├── modules.cmake
       └── foo
           ├── CMakeLists.txt
           └── Kconfig

and inside the :file:`modules.cmake` file, add the following content

.. code-block:: cmake

   set(ZEPHYR_FOO_CMAKE_DIR ${CMAKE_CURRENT_LIST_DIR}/foo)
   set(ZEPHYR_FOO_KCONFIG   ${CMAKE_CURRENT_LIST_DIR}/foo/Kconfig)

Module integration files (zephyr/module.yml)
--------------------------------------------

The module description file :file:`zephyr/module.yml` can be used to specify
that the build files, :file:`CMakeLists.txt` and :file:`Kconfig`, are located
in a :ref:`modules_module_ext_root`.

Build files located in a ``MODULE_EXT_ROOT`` can be described as:

.. code-block:: yaml

   build:
     cmake-ext: True
     kconfig-ext: True

This allows control of the build inclusion to be described externally to the
Zephyr module.

The Zephyr repository itself is always added as a Zephyr module ext root.

.. _modules_build_settings:

Build settings
==============

It is possible to specify additional build settings that must be used when
including the module into the build system.

All ``root`` settings are relative to the root of the module.

Build settings supported in the :file:`module.yml` file are:

- ``board_root``: Contains additional boards that are available to the build
  system. Additional boards must be located in a :file:`<board_root>/boards`
  folder.
- ``dts_root``: Contains additional dts files related to the architecture/soc
  families. Additional dts files must be located in a :file:`<dts_root>/dts`
  folder.
- ``soc_root``: Contains additional SoCs that are available to the build
  system. Additional SoCs must be located in a :file:`<soc_root>/soc` folder.
- ``arch_root``: Contains additional architectures that are available to the
  build system. Additional architectures must be located in a
  :file:`<arch_root>/arch` folder.
- ``module_ext_root``: Contains :file:`CMakeLists.txt` and :file:`Kconfig` files
  for Zephyr modules, see also :ref:`modules_module_ext_root`.

Example of a :file:`module.yaml` file containing additional roots, and the
corresponding file system layout.

.. code-block:: yaml

   build:
     settings:
       board_root: .
       dts_root: .
       soc_root: .
       arch_root: .
       module_ext_root: .


requires the following folder structure:

.. code-block:: none

   <zephyr-module-root>
   ├── arch
   ├── boards
   ├── dts
   ├── modules
   └── soc



Twister (Test Runner)
=====================

To execute both tests and samples available in modules, the Zephyr test runner
(twister) should be pointed to the directories containing those samples and
tests. This can be done by specifying the path to both samples and tests in the
:file:`zephyr/module.yml` file.  Additionally, if a module defines out of tree
boards, the module file can point twister to the path where those files
are maintained in the module. For example:


.. code-block:: yaml

    build:
      cmake: .
    samples:
      - samples
    tests:
      - tests
    boards:
      - boards


Module Inclusion
================

.. _modules_using_west:

Using West
----------

If west is installed and :makevar:`ZEPHYR_MODULES` is not already set, the
build system finds all the modules in your :term:`west installation` and uses
those. It does this by running :ref:`west list <west-multi-repo-misc>` to get
the paths of all the projects in the installation, then filters the results to
just those projects which have the necessary module metadata files.

Each project in the ``west list`` output is tested like this:

- If the project contains a file named :file:`zephyr/module.yml`, then the
  content of that file will be used to determine which files should be added
  to the build, as described in the previous section.

- Otherwise (i.e. if the project has no :file:`zephyr/module.yml`), the
  build system looks for :file:`zephyr/CMakeLists.txt` and
  :file:`zephyr/Kconfig` files in the project. If both are present, the project
  is considered a module, and those files will be added to the build.

- If neither of those checks succeed, the project is not considered a module,
  and is not added to :makevar:`ZEPHYR_MODULES`.

Without West
------------

If you don't have west installed or don't want the build system to use it to
find Zephyr modules, you can set :makevar:`ZEPHYR_MODULES` yourself using one
of the following options. Each of the directories in the list must contain
either a :file:`zephyr/module.yml` file or the files
:file:`zephyr/CMakeLists.txt` and :file:`Kconfig`, as described in the previous
section.

#. At the CMake command line, like this:

   .. code-block:: console

      cmake -DZEPHYR_MODULES=<path-to-module1>[;<path-to-module2>[...]] ...

#. At the top of your application's top level :file:`CMakeLists.txt`, like this:

   .. code-block:: cmake

      set(ZEPHYR_MODULES <path-to-module1> <path-to-module2> [...])
      find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

   If you choose this option, make sure to set the variable **before**  calling
   ``find_package(Zephyr ...)``, as shown above.

#. In a separate CMake script which is pre-loaded to populate the CMake cache,
   like this:

   .. code-block:: cmake

      # Put this in a file with a name like "zephyr-modules.cmake"
      set(ZEPHYR_MODULES <path-to-module1> <path-to-module2>
        CACHE STRING "pre-cached modules")

   You can tell the build system to use this file by adding ``-C
   zephyr-modules.cmake`` to your CMake command line.

Not using modules
-----------------

If you don't have west installed and don't specify :makevar:`ZEPHYR_MODULES`
yourself, then no additional modules are added to the build. You will still be
able to build any applications that don't require code or Kconfig options
defined in an external repository.

.. _submitting_new_modules:


Submitting changes to modules
******************************

When submitting new or making changes to existing modules the main repository
Zephyr needs a reference to the changes to be able to verify the changes. In the
main tree this is done using revisions. For code that is already merged and part
of the tree we use the commit hash, a tag, or a branch name. For pull requests
however, we require specifying the pull request number in the revision field to
allow building the zephyr main tree with the changes submitted to the
module.

To avoid merging changes to master with pull request information, the pull
request should be marked as ``DNM`` (Do Not Merge) or preferably a draft pull
request to make sure it is not merged by mistake and to allow for the module to
be merged first and be assigned a permanent commit hash. Once the module is
merged, the revision will need to be changed either by the submitter or by the
maintainer to the commit hash of the module which reflects the changes.

Note that multiple and dependent changes to different modules can be submitted
using exactly the same process. In this case you will change multiple entries of
all modules that have a pull request against them.


Process for submitting a new module
===================================

A request for a new module should be initialized using an RFC issue in the
Zephyr project issue tracking system with details about the module and how it
integrates into the project. If the request is approved, a new repository will
created by the project team and initialized with basic information that would
allow submitting code to the module project following the project contribution
guidelines.

If a module is maintained as a fork of another project on Github, the Zephyr module
related files and changes in relation to upstream need to be maintained in a
special branch named ``zephyr``.

Follow the following steps to request a new module:

#. Use `GitHub issues`_ to open an issue with details about the module to be
   created
#. Propose a name for the repository to be created under the Zephyr project
   organization on Github.
#. Maintainers from the Zephyr project will create the repository and initialize
   it. You will be added as a collaborator in the new repository.
#. Submit the module content (code) to the new repository following the
   guidelines described :ref:`here <modules_using_west>`.
#. Add a new entry to the :zephyr_file:`west.yml` with the following
   information:

   .. code-block:: console

        - name: <name of repository>
          path: <path to where the repository should be cloned>
          revision: <ref pointer to module pull request>


For example, to add *my_module* to the manifest:

.. code-block:: console

    - name: my_module
      path: modules/lib/my_module
      revision: pull/23/head


Where 23 in the example above indicated the pull request number submitted to the
*my_module* repository. Once the module changes are reviewed and merged, the
revision needs to be changed to the commit hash from the module repository.

.. _changes_to_existing_module:

Process for submitting changes to existing modules
==================================================

#. Submit the changes using a pull request to an existing repository following
   the :ref:`contribution guidelines <contribute_guidelines>`.
#. Submit a pull request changing the entry referencing the module into the
   :zephyr_file:`west.yml` of the main Zephyr tree with the following
   information:

   .. code-block:: console

        - name: <name of repository>
          path: <path to where the repository should be cloned>
          revision: <ref pointer to module pull request>


For example, to add *my_module* to the manifest:

.. code-block:: console

    - name: my_module
      path: modules/lib/my_module
      revision: pull/23/head

Where 23 in the example above indicated the pull request number submitted to the
*my_module* repository. Once the module changes are reviewed and merged, the
revision needs to be changed to the commit hash from the module repository.



.. _CMake list: https://cmake.org/cmake/help/latest/manual/cmake-language.7.html#lists
.. _add_subdirectory(): https://cmake.org/cmake/help/latest/command/add_subdirectory.html

.. _GitHub issues: https://github.com/zephyrproject-rtos/zephyr/issues
