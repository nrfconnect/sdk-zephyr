.. _shell:

Shell
######

Overview
********

This module allows you to create and handle a shell with a user-defined command
set. You can use it in examples where more than simple button or LED user
interaction is required. This module is a Unix-like shell with these features:

* Support for multiple instances.
* Advanced cooperation with the :ref:`logger`.
* Support for static and dynamic commands.
* Smart command completion with the :kbd:`Tab` key.
* Built-in commands: :command:`clear`, :command:`shell`, :command:`colors`,
  :command:`echo`, :command:`history` and :command:`resize`.
* Viewing recently executed commands using keys: :kbd:`↑` :kbd:`↓`.
* Text edition using keys: :kbd:`←`, :kbd:`→`, :kbd:`Backspace`,
  :kbd:`Delete`, :kbd:`End`, :kbd:`Home`, :kbd:`Insert`.
* Support for ANSI escape codes: ``VT100`` and ``ESC[n~`` for cursor control
  and color printing.
* Support for multiline commands.
* Built-in handler to display help for the commands.
* Support for wildcards: ``*`` and ``?``.
* Support for meta keys.
* Kconfig configuration to optimize memory usage.

The module can be connected to any transport for command input and output.
At this point, the following transport layers are implemented:

* UART
* Segger RTT
* DUMMY - not a physical transport layer

See the :ref:`shell_api` documentation for more information.

Connecting to Segger RTT via TCP (on macOS, for example)
========================================================

On macOS JLinkRTTClient won't let you enter input. Instead, please use following procedure:

* Open up a first Terminal window and enter:

  .. code-block:: none

     JLinkRTTLogger -Device NRF52840_XXAA -RTTChannel 1 -if SWD -Speed 4000 ~/rtt.log

  (change device if required)

* Open up a second Terminal window and enter:

  .. code-block:: none

     nc localhost 19021

* Now you should have a network connection to RTT that will let you enter input to the shell.


Commands
********

Shell commands are organized in a tree structure and grouped into the following
types:

* Root command (level 0): Gathered and alphabetically sorted in a dedicated
  memory section.
* Static subcommand (level > 0): Number and syntax must be known during compile
  time. Created in the software module.
* Dynamic subcommand (level > 0): Number and syntax does not need to be known
  during compile time. Created in the software module.

Creating commands
=================

Use the following macros for adding shell commands:

* :c:macro:`SHELL_CMD_REGISTER` - Create root command. All root commands must
  have different name.
* :c:macro:`SHELL_CMD_ARG_REGISTER` - Create root command with arguments.
  All root commands must have different name.
* :c:macro:`SHELL_CMD` - Initialize a command.
* :c:macro:`SHELL_CMD_ARG` - Initialize a command with arguments.
* :c:macro:`SHELL_CREATE_STATIC_SUBCMD_SET` - Create a static subcommands
  array.
* :c:macro:`SHELL_SUBCMD_SET_END` - shall be placed as last in
  :c:macro:`SHELL_CREATE_STATIC_SUBCMD_SET` macro.
* :c:macro:`SHELL_CREATE_DYNAMIC_CMD` - Create a dynamic subcommands array.

Commands can be created in any file in the system that includes
:file:`include/shell/shell.h`. All created commands are available for all
shell instances.

Static commands
---------------

Example code demonstrating how to create a root command with static
subcommands.

.. image:: images/static_cmd.PNG
      :align: center
      :alt: Command tree with static commands.

.. code-block:: c

	/* Creating subcommands (level 1 command) array for command "demo".
	 * Subcommands must be added in alphabetical order to ensure correct
	 * command autocompletion.
	 */
	SHELL_CREATE_STATIC_SUBCMD_SET(sub_demo)
	{
		SHELL_CMD(params, NULL, "Print params command.",
						       cmd_demo_params),
		SHELL_CMD(ping,   NULL, "Ping command.", cmd_demo_ping),
		SHELL_SUBCMD_SET_END /* Array terminated. */
	};
	/* Creating root (level 0) command "demo" */
	SHELL_CMD_REGISTER(demo, &sub_demo, "Demo commands", NULL);

Example implementation can be found under following location:
:file:`samples/subsys/shell/shell_module/src/main.c`.

Dynamic commands
----------------

Example code demonstrating how to create a root command with static and dynamic
subcommands. At the beginning dynamic command list is empty. New commands
can be added by typing:

.. code-block:: none

	dynamic add <new_dynamic_command>

Newly added commands can be prompted or autocompleted with the :kbd:`Tab` key.

.. image:: images/dynamic_cmd.PNG
      :align: center
      :alt: Command tree with static and dynamic commands.

.. code-block:: c

	/* Buffer for 10 dynamic commands */
	static char dynamic_cmd_buffer[10][50];

	/* commands counter */
	static u8_t dynamic_cmd_cnt;

	/* Function returning command dynamically created
	 * in  dynamic_cmd_buffer.
	 */
	static void dynamic_cmd_get(size_t idx,
				    struct shell_static_entry *entry)
	{
		if (idx < dynamic_cmd_cnt) {
			entry->syntax = dynamic_cmd_buffer[idx];
			entry->handler  = NULL;
			entry->subcmd = NULL;
			entry->help = "Show dynamic command name.";
		} else {
			/* if there are no more dynamic commands available
			 * syntax must be set to NULL.
			 */
			entry->syntax = NULL;
		}
	}

	SHELL_CREATE_DYNAMIC_CMD(m_sub_dynamic_set, dynamic_cmd_get);
	SHELL_CREATE_STATIC_SUBCMD_SET(m_sub_dynamic)
	{
		SHELL_CMD(add, NULL,"Add new command to dynamic_cmd_buffer and"
			  " sort them alphabetically.",
			  cmd_dynamic_add),
		SHELL_CMD(execute, &m_sub_dynamic_set,
			  "Execute a command.", cmd_dynamic_execute),
		SHELL_CMD(remove, &m_sub_dynamic_set,
			  "Remove a command from dynamic_cmd_buffer.",
			  cmd_dynamic_remove),
		SHELL_CMD(show, NULL,
			  "Show all commands in dynamic_cmd_buffer.",
			  cmd_dynamic_show),
		SHELL_SUBCMD_SET_END
	};
	SHELL_CMD_REGISTER(dynamic, &m_sub_dynamic,
		   "Demonstrate dynamic command usage.", cmd_dynamic);

Example implementation can be found under following location:
:file:`samples/subsys/shell/shell_module/src/dynamic_cmd.c`.

Commands execution
==================

Each command or subcommand may have a handler. The shell executes the handler
that is found deepest in the command tree and further subcommands (without a
handler) are passed as arguments. Characters within parentheses are treated
as one argument. If shell wont find a handler it will display an error message.

Commands can be also executed from a user application using any active backend
and a function :cpp:func:`shell_execute_cmd`, as shown in this example:

.. code-block:: c

	void main(void)
	{
		/* Below code will execute "clear" command on a DUMMY backend */
		shell_execute_cmd(NULL, "clear");

		/* Below code will execute "shell colors off" command on
		 * an UART backend
		 */
		shell_execute_cmd(shell_backend_uart_get_ptr(),
				  "shell colors off");
	}

Enable the DUMMY backend by setting the Kconfig
:option:`CONFIG_SHELL_BACKEND_DUMMY` option.


Command handler
----------------

Simple command handler implementation:

.. code-block:: c

	static int cmd_handler(const struct shell *shell, size_t argc,
				char **argv)
	{
		ARG_UNUSED(argc);
		ARG_UNUSED(argv);

		shell_print(shell, "Print simple text.");

		shell_warn(shell, "Print warning text.");

		shell_error(shell, "Print error text.");

		return 0;
	}

.. warning::
	Do not use function :cpp:func:`shell_fprintf` outside of the command
	handler because this might lead to incorrect text display on the
	screen. If any text should be displayed outside of the command context,
	then use the :ref:`logger`.

Command help
------------

Every user-defined command, subcommand, or option can have its own help
description. The help for commands and subcommands can be created with
respective macros: :c:macro:`SHELL_CMD_REGISTER` and :c:macro:`SHELL_CMD`.
In addition, you can define options for commands or subcommands using the
macro :c:macro:`SHELL_OPT`. By default, each and every command or subcommand
has these two options implemented: ``-h`` and ``--help``.

In order to add help functionality to a command or subcommand, you must
implement the help handler by either calling :cpp:func:`shell_cmd_precheck`
or pair of functions :cpp:func:`shell_help_requested` and
:cpp:func:`shell_help_print`. The former is more convenient as it also
checks for valid arguments count.

.. code-block:: c

	static int cmd_dummy_1(const struct shell *shell, size_t argc,
			       char **argv)
	{
		ARG_UNUSED(argv);

		/* Function shell_cmd_precheck will do one of below actions:
		 * 1. print help if command called with -h or --help
		 * 2. print error message if argc > 2
		 *
		 * Each of these actions can be deactivated in Kconfig.
		 */
		int err = shell_cmd_precheck(shell, (argc <= 2), NULL, 0);

		if (err) {
			return err;
		}

		shell_fprintf(shell, SHELL_NORMAL,
			      "Command called with no -h or --help option."
			      "\n");
		return 0;
	}

	static int cmd_dummy_2(const struct shell *shell, size_t argc,
			       char **argv)
	{
		ARG_UNUSED(argc);
		ARG_UNUSED(argv);

		if (hell_help_requested(shell) {
			shell_help_print(shell, NULL, 0);
		} else {
			shell_fprintf(shell, SHELL_NORMAL,
			      "Command called with no -h or --help option."
			      "\n");
		}

		return 0;
	}

Command options
---------------

When possible, use subcommands instead of options.  Options apply mainly in the
case when an argument with ``-`` or ``--`` is requested. The main benefit of
using subcommands is that they can be prompted or completed with the :kbd:`Tab`
key. In addition, subcommands can have their own handler, which limits the
usage of ``if - else if`` statements combination with the ``strcmp`` function
in command handler.


.. code-block:: c

	static int cmd_with_options(const struct shell *shell, size_t argc,
			            char **argv)
	{
		int err;
		/* Dummy options showing options usage */
		static const struct shell_getopt_option opt[] = {
			SHELL_OPT(
				"--test",
				"-t",
				"test option help string"
			),
			SHELL_OPT(
				"--dummy",
				"-d",
				"dummy option help string"
			)
		};

		/* If command will be called with -h or --help option
		 * all declared options will be listed in the help message
		 */
		err = shell_cmd_precheck(shell, (argc <= 2), opt,
					 sizeof(opt)/sizeof(opt[1]));
		if (err) {
			return err;
		}

		/* checking if command was called with test option */
		if (!strcmp(argv[1], "-t") || !strcmp(argv[1], "--test")) {
		    shell_fprintf(shell, SHELL_NORMAL, "Command called with -t"
				  " or --test option.\n");
		    return 0;
		}

		/* checking if command was called with dummy option */
		if (!strcmp(argv[1], "-d") || !strcmp(argv[1], "--dummy")) {
		    shell_fprintf(shell, SHELL_NORMAL, "Command called with -d"
				  " or --dummy option.\n");
		    return 0;
		}

		shell_fprintf(shell, SHELL_WARNING,
			      "Command called with no valid option.\n");
		return 0;
	}

Parent commands
---------------

In the subcommand handler, you can access both the parameters passed to
commands or the parent commands, depending on how you index ``argv``.

* When indexing ``argv`` with positive numbers, you can access the parameters.
* When indexing ``argv`` with negative numbers, you can access the parent
  commands.
* The subcommand to which the handler belongs has the ``argv`` value of 0.

.. code-block:: c

	static int cmd_handler(const struct shell *shell, size_t argc,
			       char **argv)
	{
		ARG_UNUSED(argc);

		/* If it is a subcommand handler parent command syntax
		 * can be found using argv[-1].
		 */
		shell_fprintf(shell, SHELL_NORMAL,
			      "This command has a parent command: %s\n",
			      argv[-1]);

		/* Print this command syntax */
		shell_fprintf(shell, SHELL_NORMAL,
			      "This command syntax is: %s\n",
			      argv[0]);

		/* Print first argument */
		shell_fprintf(shell, SHELL_NORMAL,
			      argv[1]);

		return 0;
	}

Built-in commands
=================

* :command:`clear` - Clears the screen.
* :command:`history` - Shows the recently entered commands.
* :command:`resize` - Must be executed when terminal width is different than 80
  characters or after each change of terminal width. It ensures proper
  multiline text display and :kbd:`←`, :kbd:`→`, :kbd:`End`, :kbd:`Home` keys
  handling. Currently this command works only with UART flow control switched
  on. It can be also called with a subcommand:

	* :command:`default` - Shell will send terminal width = 80 to the
	  terminal and assume successful delivery.

* :command:`shell` - Root command with useful shell-related subcommands like:

	* :command:`echo` - Toggles shell echo.
        * :command:`colors` - Toggles colored syntax. This might be helpful in
          case of Bluetooth shell to limit the amount of transferred bytes.
	* :command:`stats` - Shows shell statistics.

Wildcards
*********

The shell module can handle wildcards. Wildcards are interpreted correctly
when expanded command and its subcommands do not have a handler. For example,
if you want to set logging level to ``err`` for the ``app`` and ``app_test``
modules you can execute the following command:

.. code-block:: none

	log enable err a*

.. image:: images/wildcard.png
      :align: center
      :alt: Wildcard usage example

Meta keys
*********

The shell module supports the following meta keys:

.. list-table:: Implemented meta keys
   :widths: 10 40
   :header-rows: 1

   * - Meta keys
     - Action
   * - ctrl + a
     - Moves the cursor to the beginning of the line.
   * - ctrl + c
     - Preserves the last command on the screen and starts a new command in
       a new line.
   * - ctrl + e
     - Moves the cursor to the end of the line.
   * - ctrl + l
     - Clears the screen and leaves the currently typed command at the top of
       the screen.
   * - ctrl + u
     - Clears the currently typed command.
   * - ctrl + w
     - Removes the word or part of the word to the left of the cursor. Words
       separated by period instead of space are treated as one word.

Usage
*****

To create a new shell instance user needs to activate requested
backend using `menuconfig`.

The following code shows a simple use case of this library:

.. code-block:: c

	void main(void)
	{

	}

	static int cmd_demo_ping(const struct shell *shell, size_t argc,
				 char **argv)
	{
		ARG_UNUSED(argc);
		ARG_UNUSED(argv);

		shell_fprintf(shell, SHELL_NORMAL, "pong\n");
		return 0;
	}

	static int cmd_demo_params(const struct shell *shell, size_t argc,
				   char **argv)
	{
		int cnt;

		shell_fprintf(shell, SHELL_NORMAL, "argc = %d\n", argc);
		for (cnt = 0; cnt < argc; cnt++) {
			shell_fprintf(shell, SHELL_NORMAL,
					"  argv[%d] = %s\n", cnt, argv[cnt]);
		}
		return 0;
	}

	/* Creating subcommands (level 1 command) array for command "demo".
	 * Subcommands must be added in alphabetical order
	 */
	SHELL_CREATE_STATIC_SUBCMD_SET(sub_demo)
	{
		SHELL_CMD(params, NULL, "Print params command.",
						       cmd_demo_params),
		SHELL_CMD(ping,   NULL, "Ping command.", cmd_demo_ping),
		SHELL_SUBCMD_SET_END /* Array terminated. */
	};
	/* Creating root (level 0) command "demo" without a handler */
	SHELL_CMD_REGISTER(demo, &sub_demo, "Demo commands", NULL);

	/* Creating root (level 0) command "version" */
	SHELL_CMD_REGISTER(version, NULL, "Show kernel version", cmd_version);


Users may use the :kbd:`Tab` key to complete a command/subcommand or to see the
available subcommands for the currently entered command level.
For example, when the cursor is positioned at the beginning of the command
line and the :kbd:`Tab` key is pressed, the user will see all root (level 0)
commands:

.. code-block:: none

	  clear  demo  shell  history  log  resize  version


.. note::
	To view the subcommands that are available for a specific command, you
	must first type a :kbd:`space` after this command and then hit
	:kbd:`Tab`.

These commands are registered by various modules, for example:

* :command:`clear`, :command:`shell`, :command:`history`, and :command:`resize`
  are built-in commands which have been registered by
  :file:`subsys/shell/shell.c`
* :command:`demo` and :command:`version` have been registered in example code
  above by main.c
* :command:`log` has been registered by :file:`subsys/logging/log_cmds.c`

Then, if a user types a :command:`demo` command and presses the :kbd:`Tab` key,
the shell will only print the subcommands registered for this command:

.. code-block:: none

	  params  ping


