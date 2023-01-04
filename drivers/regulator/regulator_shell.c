/*
 * Copyright 2022 NXP
 * Copyright 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include <zephyr/device.h>
#include <zephyr/shell/shell.h>
#include <zephyr/drivers/regulator.h>
#include <zephyr/toolchain.h>

static int strtomicro(char *inp, char units, int32_t *val)
{
	size_t len, start, end;
	int32_t mult, decdiv = 1;

	len = strlen(inp);
	if (len < 2) {
		return -EINVAL;
	}

	/* suffix */
	if (tolower(inp[len - 1]) != units) {
		return -EINVAL;
	}

	if ((len > 2) && (inp[len - 2] == 'u')) {
		mult = 1;
		end = len - 3;
	} else if ((len > 2) && (inp[len - 2] == 'm')) {
		mult = 1000;
		end = len - 3;
	} else if (isdigit((unsigned char)inp[len - 2]) > 0) {
		mult = 1000000;
		end = len - 2;
	} else {
		return -EINVAL;
	}

	/* optional prefix (sign) */
	if (inp[0] == '-') {
		mult *= -1;
		start = 1;
	} else if (inp[0] == '+') {
		start = 1;
	} else {
		start = 0;
	}

	/* numeric part */
	*val = 0;
	for (size_t i = start; i <= end; i++) {
		if (isdigit((unsigned char)inp[i]) > 0) {
			*val = *val * 10 / decdiv +
			       (int32_t)(inp[i] - '0') * mult / decdiv;
		} else if (inp[i] == '.') {
			decdiv = 10;
		} else {
			return -EINVAL;
		}
	}

	return 0;
}

static void microtoshell(const struct shell *sh, char unit, int32_t val)
{
	if (val > 100000) {
		shell_print(sh, "%d.%03d %c", val / 1000000,
			    (val % 1000000) / 1000, unit);
	} else if (val > 1000) {
		shell_print(sh, "%d.%03d m%c", val / 1000, val % 1000, unit);
	} else {
		shell_print(sh, "%d u%c", val, unit);
	}
}

static int cmd_enable(const struct shell *sh, size_t argc, char **argv)
{
	const struct device *dev;
	int ret;

	ARG_UNUSED(argc);

	dev = device_get_binding(argv[1]);
	if (dev == NULL) {
		shell_error(sh, "Regulator device %s not available", argv[1]);
		return -ENODEV;
	}

	ret = regulator_enable(dev);
	if (ret < 0) {
		shell_error(sh, "Could not enable regulator (%d)", ret);
		return ret;
	}

	return 0;
}

static int cmd_disable(const struct shell *sh, size_t argc, char **argv)
{
	const struct device *dev;
	int ret;

	ARG_UNUSED(argc);

	dev = device_get_binding(argv[1]);
	if (dev == NULL) {
		shell_error(sh, "Regulator device %s not available", argv[1]);
		return -ENODEV;
	}

	ret = regulator_disable(dev);
	if (ret < 0) {
		shell_error(sh, "Could not disable regulator (%d)", ret);
		return ret;
	}

	return 0;
}

static int cmd_vlist(const struct shell *sh, size_t argc, char **argv)
{
	const struct device *dev;
	unsigned int volt_cnt;
	int32_t last_volt_uv;

	ARG_UNUSED(argc);

	dev = device_get_binding(argv[1]);
	if (dev == NULL) {
		shell_error(sh, "Regulator device %s not available", argv[1]);
		return -ENODEV;
	}

	volt_cnt = regulator_count_voltages(dev);

	for (unsigned int i = 0U; i < volt_cnt; i++) {
		int32_t volt_uv;

		(void)regulator_list_voltage(dev, i, &volt_uv);

		/* do not print repeated voltages */
		if ((i == 0U) || (last_volt_uv != volt_uv)) {
			microtoshell(sh, 'V', volt_uv);
		}

		last_volt_uv = volt_uv;
	}

	return 0;
}

static int cmd_vset(const struct shell *sh, size_t argc, char **argv)
{
	const struct device *dev;
	int32_t min_uv, max_uv;
	int ret;

	dev = device_get_binding(argv[1]);
	if (dev == NULL) {
		shell_error(sh, "Regulator device %s not available", argv[1]);
		return -ENODEV;
	}

	ret = strtomicro(argv[2], 'v', &min_uv);
	if (ret < 0) {
		shell_error(sh, "Invalid min. voltage: %s", argv[2]);
		return ret;
	}

	if (argc == 4) {
		ret = strtomicro(argv[3], 'v', &max_uv);
		if (ret < 0) {
			shell_error(sh, "Invalid max. voltage: %s", argv[3]);
			return ret;
		}
	} else {
		max_uv = min_uv;
	}

	ret = regulator_set_voltage(dev, min_uv, max_uv);
	if (ret < 0) {
		shell_error(sh, "Could not set voltage (%d)", ret);
		return ret;
	}

	return 0;
}

static int cmd_vget(const struct shell *sh, size_t argc, char **argv)
{
	const struct device *dev;
	int32_t volt_uv;
	int ret;

	ARG_UNUSED(argc);

	dev = device_get_binding(argv[1]);
	if (dev == NULL) {
		shell_error(sh, "Regulator device %s not available", argv[1]);
		return -ENODEV;
	}

	ret = regulator_get_voltage(dev, &volt_uv);
	if (ret < 0) {
		shell_error(sh, "Could not get voltage (%d)", ret);
		return ret;
	}

	microtoshell(sh, 'V', volt_uv);

	return 0;
}

static int cmd_iset(const struct shell *sh, size_t argc, char **argv)
{
	const struct device *dev;
	int32_t min_ua, max_ua;
	int ret;

	dev = device_get_binding(argv[1]);
	if (dev == NULL) {
		shell_error(sh, "Regulator device %s not available", argv[1]);
		return -ENODEV;
	}

	ret = strtomicro(argv[2], 'a', &min_ua);
	if (ret < 0) {
		shell_error(sh, "Invalid min. current: %s", argv[2]);
		return ret;
	}
	if (argc == 4) {
		ret = strtomicro(argv[3], 'a', &max_ua);
		if (ret < 0) {
			shell_error(sh, "Invalid max. current: %s", argv[3]);
			return ret;
		}
	} else {
		max_ua = min_ua;
	}

	ret = regulator_set_current_limit(dev, min_ua, max_ua);
	if (ret < 0) {
		shell_error(sh, "Could not set current limit (%d)", ret);
		return ret;
	}

	return 0;
}

static int cmd_iget(const struct shell *sh, size_t argc, char **argv)
{
	const struct device *dev;
	int32_t curr_ua;
	int ret;

	ARG_UNUSED(argc);

	dev = device_get_binding(argv[1]);
	if (dev == NULL) {
		shell_error(sh, "Regulator device %s not available", argv[1]);
		return -ENODEV;
	}

	ret = regulator_get_current_limit(dev, &curr_ua);
	if (ret < 0) {
		shell_error(sh, "Could not get current limit (%d)", ret);
		return ret;
	}

	microtoshell(sh, 'A', curr_ua);

	return 0;
}

static int cmd_modeset(const struct shell *sh, size_t argc, char **argv)
{
	const struct device *dev;
	regulator_mode_t mode;
	int ret;

	ARG_UNUSED(argc);

	dev = device_get_binding(argv[1]);
	if (dev == NULL) {
		shell_error(sh, "Regulator device %s not available", argv[1]);
		return -ENODEV;
	}

	mode = (regulator_mode_t)strtoul(argv[2], NULL, 10);

	ret = regulator_set_mode(dev, mode);
	if (ret < 0) {
		shell_error(sh, "Could not set mode (%d)", ret);
		return ret;
	}

	return 0;
}

static int cmd_modeget(const struct shell *sh, size_t argc, char **argv)
{
	const struct device *dev;
	regulator_mode_t mode;
	int ret;

	ARG_UNUSED(argc);

	dev = device_get_binding(argv[1]);
	if (dev == NULL) {
		shell_error(sh, "Regulator device %s not available", argv[1]);
		return -ENODEV;
	}

	ret = regulator_get_mode(dev, &mode);
	if (ret < 0) {
		shell_error(sh, "Could not get mode (%d)", ret);
		return ret;
	}

	shell_print(sh, "Mode: %u", (unsigned int)mode);

	return 0;
}

static int cmd_errors(const struct shell *sh, size_t argc, char **argv)
{
	const struct device *dev;
	regulator_error_flags_t errors;
	int ret;

	ARG_UNUSED(argc);

	dev = device_get_binding(argv[1]);
	if (dev == NULL) {
		shell_error(sh, "Regulator device %s not available", argv[1]);
		return -ENODEV;
	}

	ret = regulator_get_error_flags(dev, &errors);
	if (ret < 0) {
		shell_error(sh, "Could not get error flags (%d)", ret);
		return ret;
	}

	shell_print(sh, "Overvoltage:\t[%s]",
		    ((errors & REGULATOR_ERROR_OVER_VOLTAGE) != 0U) ? "X"
								    : " ");
	shell_print(sh, "Overcurrent:\t[%s]",
		    ((errors & REGULATOR_ERROR_OVER_CURRENT) != 0U) ? "X"
								    : " ");
	shell_print(sh, "Overtemp.:\t[%s]",
		    ((errors & REGULATOR_ERROR_OVER_TEMP) != 0U) ? "X" : " ");

	return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(
	sub_regulator_cmds,
	SHELL_CMD_ARG(enable, NULL,
		      "Enable regulator\n"
		      "Usage: enable <device>",
		      cmd_enable, 2, 0),
	SHELL_CMD_ARG(disable, NULL,
		      "Disable regulator\n"
		      "Usage: disable <device>",
		      cmd_disable, 2, 0),
	SHELL_CMD_ARG(vlist, NULL,
		      "List all supported voltages\n"
		      "Usage: vlist <device>",
		      cmd_vlist, 2, 0),
	SHELL_CMD_ARG(vset, NULL,
		      "Set voltage\n"
		      "Input requires units, e.g. 200mv, 20.5mv, 10uv, 1v...\n"
		      "Usage: vset <device> <minimum> [<maximum>]\n"
		      "If maximum is not set, exact voltage will be requested",
		      cmd_vset, 3, 1),
	SHELL_CMD_ARG(vget, NULL,
		      "Get voltage\n"
		      "Usage: vget <device>",
		      cmd_vget, 2, 0),
	SHELL_CMD_ARG(iset, NULL,
		      "Set current limit\n"
		      "Input requires units, e.g. 200ma, 20.5ma, 10ua, 1a...\n"
		      "Usage: iset <device> <minimum> [<maximum>]"
		      "If maximum is not set, exact current will be requested",
		      cmd_iset, 3, 1),
	SHELL_CMD_ARG(iget, NULL,
		      "Get current limit\n"
		      "Usage: iget <device>",
		      cmd_iget, 2, 0),
	SHELL_CMD_ARG(modeset, NULL,
		      "Set regulator mode\n"
		      "Usage: modeset <device> <mode identifier>",
		      cmd_modeset, 3, 0),
	SHELL_CMD_ARG(modeget, NULL,
		      "Get regulator mode\n"
		      "Usage: modeget <device>",
		      cmd_modeget, 2, 0),
	SHELL_CMD_ARG(errors, NULL,
		      "Get errors\n"
		      "Usage: errors <device>",
		      cmd_errors, 2, 0),
	SHELL_SUBCMD_SET_END);

SHELL_CMD_REGISTER(regulator, &sub_regulator_cmds, "Regulator playground",
		   NULL);
