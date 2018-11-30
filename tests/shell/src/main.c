/*
 * Copyright (c) 2017 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/** @file
 *  @brief Interactive shell test suite
 *
 */

#include <zephyr.h>
#include <ztest.h>

#include <shell/shell.h>

#define MAX_CMD_SYNTAX_LEN	(11)
static char dynamic_cmd_buffer[][MAX_CMD_SYNTAX_LEN] = {
		"dynamic",
		"command"
};

static void test_shell_execute_cmd(const char *cmd, int result)
{
	int ret;

	ret = shell_execute_cmd(NULL, cmd);

	TC_PRINT("shell_execute_cmd(%s): %d\n", cmd, ret);

	zassert_true(ret == result, cmd);
}

static void test_cmd_help(void)
{
	test_shell_execute_cmd("help", 0);
	test_shell_execute_cmd("help -h", 0);
	test_shell_execute_cmd("help --help", 0);
	test_shell_execute_cmd("help dummy", 0);
	test_shell_execute_cmd("help dummy dummy", 0);
}

static void test_cmd_clear(void)
{
	test_shell_execute_cmd("clear", 0);
	test_shell_execute_cmd("clear -h", 1);
	test_shell_execute_cmd("clear --help", 1);
	test_shell_execute_cmd("clear dummy", -EINVAL);
	test_shell_execute_cmd("clear dummy dummy", -EINVAL);
}

static void test_cmd_shell(void)
{
	test_shell_execute_cmd("shell -h", 1);
	test_shell_execute_cmd("shell --help", 1);
	test_shell_execute_cmd("shell dummy", -EINVAL);
	test_shell_execute_cmd("shell dummy dummy", -EINVAL);

	/* subcommand: backspace_mode */
	test_shell_execute_cmd("shell backspace_mode -h", 1);
	test_shell_execute_cmd("shell backspace_mode --help", 1);
	test_shell_execute_cmd("shell backspace_mode dummy", -EINVAL);

	test_shell_execute_cmd("shell backspace_mode backspace", 0);
	test_shell_execute_cmd("shell backspace_mode backspace -h", 1);
	test_shell_execute_cmd("shell backspace_mode backspace --help", 1);
	test_shell_execute_cmd("shell backspace_mode backspace dummy", -EINVAL);
	test_shell_execute_cmd("shell backspace_mode backspace dummy dummy",
				-EINVAL);

	test_shell_execute_cmd("shell backspace_mode delete", 0);
	test_shell_execute_cmd("shell backspace_mode delete -h", 1);
	test_shell_execute_cmd("shell backspace_mode delete --help", 1);
	test_shell_execute_cmd("shell backspace_mode delete dummy", -EINVAL);
	test_shell_execute_cmd("shell backspace_mode delete dummy dummy",
				-EINVAL);

	/* subcommand: colors */
	test_shell_execute_cmd("shell colors -h", 1);
	test_shell_execute_cmd("shell colors --help", 1);
	test_shell_execute_cmd("shell colors dummy", -EINVAL);
	test_shell_execute_cmd("shell colors dummy dummy", -EINVAL);

	test_shell_execute_cmd("shell colors off", 0);
	test_shell_execute_cmd("shell colors off -h", 1);
	test_shell_execute_cmd("shell colors off --help", 1);
	test_shell_execute_cmd("shell colors off dummy", -EINVAL);
	test_shell_execute_cmd("shell colors off dummy dummy", -EINVAL);

	test_shell_execute_cmd("shell colors on", 0);
	test_shell_execute_cmd("shell colors on -h", 1);
	test_shell_execute_cmd("shell colors on --help", 1);
	test_shell_execute_cmd("shell colors on dummy", -EINVAL);
	test_shell_execute_cmd("shell colors on dummy dummy", -EINVAL);

	/* subcommand: echo */
	test_shell_execute_cmd("shell echo", 0);
	test_shell_execute_cmd("shell echo -h", 1);
	test_shell_execute_cmd("shell echo --help", 1);
	test_shell_execute_cmd("shell echo dummy", -EINVAL);
	test_shell_execute_cmd("shell echo dummy dummy", -EINVAL);

	test_shell_execute_cmd("shell echo off", 0);
	test_shell_execute_cmd("shell echo off -h", 1);
	test_shell_execute_cmd("shell echo off --help", 1);
	test_shell_execute_cmd("shell echo off dummy", -EINVAL);
	test_shell_execute_cmd("shell echo off dummy dummy", -EINVAL);

	test_shell_execute_cmd("shell echo on", 0);
	test_shell_execute_cmd("shell echo on -h", 1);
	test_shell_execute_cmd("shell echo on --help", 1);
	test_shell_execute_cmd("shell echo on dummy", -EINVAL);
	test_shell_execute_cmd("shell echo on dummy dummy", -EINVAL);

	/* subcommand: stats */
	test_shell_execute_cmd("shell stats", -EINVAL);
	test_shell_execute_cmd("shell stats -h", 1);
	test_shell_execute_cmd("shell stats --help", 1);
	test_shell_execute_cmd("shell stats dummy", -EINVAL);
	test_shell_execute_cmd("shell stats dummy dummy", -EINVAL);

	test_shell_execute_cmd("shell stats reset", 0);
	test_shell_execute_cmd("shell stats reset -h", 1);
	test_shell_execute_cmd("shell stats reset --help", 1);
	test_shell_execute_cmd("shell stats reset dummy", -EINVAL);
	test_shell_execute_cmd("shell stats reset dummy dummy", -EINVAL);

	test_shell_execute_cmd("shell stats show", 0);
	test_shell_execute_cmd("shell stats show -h", 1);
	test_shell_execute_cmd("shell stats show --help", 1);
	test_shell_execute_cmd("shell stats show dummy", -EINVAL);
	test_shell_execute_cmd("shell stats show dummy dummy", -EINVAL);
}

static void test_cmd_history(void)
{
	test_shell_execute_cmd("history", 0);
	test_shell_execute_cmd("history -h", 1);
	test_shell_execute_cmd("history --help", 1);
	test_shell_execute_cmd("history dummy", -EINVAL);
	test_shell_execute_cmd("history dummy dummy", -EINVAL);
}

static void test_cmd_resize(void)
{
	test_shell_execute_cmd("resize -h", 1);
	test_shell_execute_cmd("resize --help", 1);
	test_shell_execute_cmd("resize dummy", -EINVAL);
	test_shell_execute_cmd("resize dummy dummy", -EINVAL);

	/* subcommand: default */
	test_shell_execute_cmd("resize default", 0);
	test_shell_execute_cmd("resize default -h", 1);
	test_shell_execute_cmd("resize default --help", 1);
	test_shell_execute_cmd("resize default dummy", -EINVAL);
	test_shell_execute_cmd("resize default dummy dummy", -EINVAL);
}

static void test_shell_module(void)
{
	test_shell_execute_cmd("test_shell_cmd", 0);
	test_shell_execute_cmd("test_shell_cmd -h", 1);
	test_shell_execute_cmd("test_shell_cmd --help", 1);
	test_shell_execute_cmd("test_shell_cmd dummy", -EINVAL);
	test_shell_execute_cmd("test_shell_cmd dummy dummy", -EINVAL);

	test_shell_execute_cmd("", -ENOEXEC); /* empty command */
	test_shell_execute_cmd("not existing command", -ENOEXEC);
}

/* test wildcard and static subcommands */
static void test_shell_wildcards_static(void)
{
	test_shell_execute_cmd("test_wildcard", 0);
	test_shell_execute_cmd("test_wildcard argument_1", 1);
	test_shell_execute_cmd("test_wildcard argument?1", 1);
	test_shell_execute_cmd("test_wildcard argu?ent?1", 1);
	test_shell_execute_cmd("test_wildcard a*1", 1);
	test_shell_execute_cmd("test_wildcard ar?u*1", 1);

	test_shell_execute_cmd("test_wildcard *", 3);
	test_shell_execute_cmd("test_wildcard a*", 2);
}

/* test wildcard and dynamic subcommands */
static void test_shell_wildcards_dynamic(void)
{
	test_shell_execute_cmd("test_dynamic", 0);
	test_shell_execute_cmd("test_dynamic d*", 1);
	test_shell_execute_cmd("test_dynamic c*", 1);
	test_shell_execute_cmd("test_dynamic d* c*", 2);
}


static int cmd_test_module(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argv);

	return shell_cmd_precheck(shell, (argc == 1), NULL, 0);
}
SHELL_CMD_REGISTER(test_shell_cmd, NULL, NULL, cmd_test_module);


static int cmd_wildcard(const struct shell *shell, size_t argc, char **argv)
{
	int valid_arguments = 0;

	for (size_t i = 1; i < argc; i++) {
		if (!strcmp("argument_1", argv[i])) {
			valid_arguments++;
			continue;
		}
		if (!strcmp("argument_2", argv[i])) {
			valid_arguments++;
			continue;
		}
		if (!strcmp("dummy", argv[i])) {
			valid_arguments++;
		}
	}

	return valid_arguments;
}

SHELL_CREATE_STATIC_SUBCMD_SET(m_sub_test_shell_cmdl)
{
	SHELL_CMD(argument_1, NULL, NULL, NULL),
	SHELL_CMD(argument_2, NULL, NULL, NULL),
	SHELL_CMD(dummy, NULL, NULL, NULL),
	SHELL_SUBCMD_SET_END
};
SHELL_CMD_REGISTER(test_wildcard, &m_sub_test_shell_cmdl, NULL, cmd_wildcard);


static int cmd_dynamic(const struct shell *shell, size_t argc, char **argv)
{
	int valid_arguments = 0;

	for (size_t i = 1; i < argc; i++) {
		if (!strcmp(dynamic_cmd_buffer[0], argv[i])) {
			valid_arguments++;
			continue;
		}
		if (!strcmp(dynamic_cmd_buffer[1], argv[i])) {
			valid_arguments++;
		}
	}


	return valid_arguments;
}
/* dynamic command creation */
static void dynamic_cmd_get(size_t idx, struct shell_static_entry *entry)
{
	if (idx < ARRAY_SIZE(dynamic_cmd_buffer)) {
		/* m_dynamic_cmd_buffer must be sorted alphabetically to ensure
		 * correct CLI completion
		 */
		entry->syntax = dynamic_cmd_buffer[idx];
		entry->handler  = NULL;
		entry->subcmd = NULL;
		entry->help = NULL;
	} else {
		/* if there are no more dynamic commands available syntax
		 * must be set to NULL.
		 */
		entry->syntax = NULL;
	}
}

SHELL_CREATE_DYNAMIC_CMD(m_sub_test_dynamic, dynamic_cmd_get);
SHELL_CMD_REGISTER(test_dynamic, &m_sub_test_dynamic, NULL, cmd_dynamic);


void test_main(void)
{
	ztest_test_suite(shell_test_suite,
			ztest_unit_test(test_cmd_help),
			ztest_unit_test(test_cmd_clear),
			ztest_unit_test(test_cmd_shell),
			ztest_unit_test(test_cmd_history),
			ztest_unit_test(test_cmd_resize),
			ztest_unit_test(test_shell_module),
			ztest_unit_test(test_shell_wildcards_static),
			ztest_unit_test(test_shell_wildcards_dynamic));

	ztest_run_test_suite(shell_test_suite);
}
