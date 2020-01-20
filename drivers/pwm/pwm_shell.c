/*
 * Copyright (c) 2019 Vestas Wind Systems A/S
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief PWM shell commands.
 */

#include <shell/shell.h>
#include <drivers/pwm.h>
#include <stdlib.h>

struct args_index {
	u8_t device;
	u8_t pwm;
	u8_t period;
	u8_t pulse;
	u8_t flags;
};

static const struct args_index args_indx = {
	.device = 1,
	.pwm = 2,
	.period = 3,
	.pulse = 4,
	.flags = 5,
};

static int cmd_cycles(const struct shell *shell, size_t argc, char **argv)
{
	pwm_flags_t flags = 0;
	struct device *dev;
	u32_t period;
	u32_t pulse;
	u32_t pwm;
	int err;

	dev = device_get_binding(argv[args_indx.device]);
	if (!dev) {
		shell_error(shell, "PWM device not found");
		return -EINVAL;
	}

	pwm = strtoul(argv[args_indx.pwm], NULL, 0);
	period = strtoul(argv[args_indx.period], NULL, 0);
	pulse = strtoul(argv[args_indx.pulse], NULL, 0);

	if (argc == (args_indx.flags + 1)) {
		flags = strtoul(argv[args_indx.flags], NULL, 0);
	}

	err = pwm_pin_set_cycles(dev, pwm, period, pulse, flags);
	if (err) {
		shell_error(shell, "failed to setup PWM (err %d)",
			    err);
		return err;
	}

	return 0;
}

static int cmd_usec(const struct shell *shell, size_t argc, char **argv)
{
	pwm_flags_t flags = 0;
	struct device *dev;
	u32_t period;
	u32_t pulse;
	u32_t pwm;
	int err;

	dev = device_get_binding(argv[args_indx.device]);
	if (!dev) {
		shell_error(shell, "PWM device not found");
		return -EINVAL;
	}

	pwm = strtoul(argv[args_indx.pwm], NULL, 0);
	period = strtoul(argv[args_indx.period], NULL, 0);
	pulse = strtoul(argv[args_indx.pulse], NULL, 0);

	if (argc == (args_indx.flags + 1)) {
		flags = strtoul(argv[args_indx.flags], NULL, 0);
	}

	err = pwm_pin_set_usec(dev, pwm, period, pulse, flags);
	if (err) {
		shell_error(shell, "failed to setup PWM (err %d)", err);
		return err;
	}

	return 0;
}

static int cmd_nsec(const struct shell *shell, size_t argc, char **argv)
{
	pwm_flags_t flags = 0;
	struct device *dev;
	u32_t period;
	u32_t pulse;
	u32_t pwm;
	int err;

	dev = device_get_binding(argv[args_indx.device]);
	if (!dev) {
		shell_error(shell, "PWM device not found");
		return -EINVAL;
	}

	pwm = strtoul(argv[args_indx.pwm], NULL, 0);
	period = strtoul(argv[args_indx.period], NULL, 0);
	pulse = strtoul(argv[args_indx.pulse], NULL, 0);

	if (argc == (args_indx.flags + 1)) {
		flags = strtoul(argv[args_indx.flags], NULL, 0);
	}

	err = pwm_pin_set_nsec(dev, pwm, period, pulse, flags);
	if (err) {
		shell_error(shell, "failed to setup PWM (err %d)", err);
		return err;
	}

	return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(pwm_cmds,
	SHELL_CMD_ARG(cycles, NULL, "<device> <pwm> <period in cycles> "
		      "<pulse width in cycles> [flags]", cmd_cycles, 5, 1),
	SHELL_CMD_ARG(usec, NULL, "<device> <pwm> <period in usec> "
		      "<pulse width in usec> [flags]", cmd_usec, 5, 1),
	SHELL_CMD_ARG(nsec, NULL, "<device> <pwm> <period in nsec> "
		      "<pulse width in nsec> [flags]", cmd_nsec, 5, 1),
	SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(pwm, &pwm_cmds, "PWM shell commands", NULL);
