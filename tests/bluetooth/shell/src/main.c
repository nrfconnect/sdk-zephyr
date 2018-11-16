/** @file
 *  @brief Interactive Bluetooth LE shell application
 *
 *  Application allows implement Bluetooth LE functional commands performing
 *  simple diagnostic interaction between LE host stack and LE controller
 */

/*
 * Copyright (c) 2017 Nordic Semiconductor ASA
 * Copyright (c) 2015-2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <zephyr/types.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <misc/printk.h>
#include <misc/byteorder.h>
#include <zephyr.h>

#include <shell/shell.h>

#include <gatt/hrs.h>

#define DEVICE_NAME CONFIG_BT_DEVICE_NAME

#if defined(CONFIG_BT_CONN)
static bool hrs_simulate;

static int cmd_hrs_simulate(const struct shell *shell,
			    size_t argc, char *argv[])
{
	int err = shell_cmd_precheck(shell, (argc == 2), NULL, 0);

	if (err) {
		return err;
	}

	if (!strcmp(argv[1], "on")) {
		static bool hrs_registered;

		if (!hrs_registered) {
			shell_print(shell, "Registering HRS Service");
			hrs_init(0x01);
			hrs_registered = true;
		}

		shell_print(shell, "Start HRS simulation");
		hrs_simulate = true;
	} else if (!strcmp(argv[1], "off")) {
		shell_print(shell, "Stop HRS simulation");
		hrs_simulate = false;
	} else {
		shell_print(shell, "Incorrect value: %s", argv[1]);
		shell_help_print(shell, NULL, 0);
		return -ENOEXEC;
	}

	return 0;
}
#endif /* CONFIG_BT_CONN */

#define HELP_NONE "[none]"
#define HELP_ADDR_LE "<address: XX:XX:XX:XX:XX:XX> <type: (public|random)>"

SHELL_CREATE_STATIC_SUBCMD_SET(hrs_cmds) {
#if defined(CONFIG_BT_CONN)
	SHELL_CMD(hrs-simulate, NULL,
		  "register and simulate Heart Rate Service <value: on, off>",
		  cmd_hrs_simulate),
#endif /* CONFIG_BT_CONN */
	SHELL_SUBCMD_SET_END
};

static int cmd_hrs(const struct shell *shell, size_t argc, char **argv)
{
	int err = shell_cmd_precheck(shell, (argc == 2), NULL, 0);

	if (argc == 1) {
		shell_help_print(shell, NULL, 0);
		/* shell_cmd_precheck returns 1 when help is printed */
		return 1;
	}

	if (err) {
		return err;
	}

	shell_error(shell, "%s unknown parameter: %s", argv[0], argv[1]);

	return -ENOEXEC;
}

SHELL_CMD_REGISTER(hrs, &hrs_cmds, "Heart Rate Service shell commands",
		   cmd_hrs);

void main(void)
{
	printk("Type \"help\" for supported commands.");
	printk("Before any Bluetooth commands you must `bt init` to initialize"
	       " the stack.\n");

	while (1) {
		k_sleep(MSEC_PER_SEC);

#if defined(CONFIG_BT_CONN)
		/* Heartrate measurements simulation */
		if (hrs_simulate) {
			hrs_notify();
		}
#endif /* CONFIG_BT_CONN */
	}
}
