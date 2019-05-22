/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <ctype.h>
#include <stdlib.h>
#include <atomic.h>
#include <shell/shell.h>
#include <shell/shell_dummy.h>
#include "shell_ops.h"
#include "shell_help.h"
#include "shell_utils.h"
#include "shell_vt100.h"
#include "shell_wildcard.h"

/* 2 == 1 char for cmd + 1 char for '\0' */
#if (CONFIG_SHELL_CMD_BUFF_SIZE < 2)
	#error too small CONFIG_SHELL_CMD_BUFF_SIZE
#endif

#if (CONFIG_SHELL_PRINTF_BUFF_SIZE < 1)
	#error too small SHELL_PRINTF_BUFF_SIZE
#endif

#define SHELL_MSG_CMD_NOT_FOUND		": command not found"

#define SHELL_INIT_OPTION_PRINTER	(NULL)


static inline void receive_state_change(const struct shell *shell,
					enum shell_receive_state state)
{
	shell->ctx->receive_state = state;
}

static void cmd_buffer_clear(const struct shell *shell)
{
	shell->ctx->cmd_buff[0] = '\0'; /* clear command buffer */
	shell->ctx->cmd_buff_pos = 0;
	shell->ctx->cmd_buff_len = 0;
}

static void shell_internal_help_print(const struct shell *shell)
{
	if (!IS_ENABLED(CONFIG_SHELL_HELP)) {
		return;
	}

	shell_help_cmd_print(shell);
	shell_help_subcmd_print(shell);
}

/**
 * @brief Prints error message on wrong argument count.
 *	  Optionally, printing help on wrong argument count.
 *
 * @param[in] shell	  Pointer to the shell instance.
 * @param[in] arg_cnt_ok  Flag indicating valid number of arguments.
 *
 * @return 0		  if check passed
 * @return -EINVAL	  if wrong argument count
 */
static int cmd_precheck(const struct shell *shell,
			bool arg_cnt_ok)
{
	if (!arg_cnt_ok) {
		shell_internal_fprintf(shell, SHELL_ERROR,
				       "%s: wrong parameter count\n",
				       shell->ctx->active_cmd.syntax);

		if (IS_ENABLED(CONFIG_SHELL_HELP_ON_WRONG_ARGUMENT_COUNT)) {
			shell_internal_help_print(shell);
		}

		return -EINVAL;
	}

	return 0;
}

static void state_set(const struct shell *shell, enum shell_state state)
{
	shell->ctx->state = state;

	if (state == SHELL_STATE_ACTIVE) {
		cmd_buffer_clear(shell);
		shell_print_prompt_and_cmd(shell);
	}
}

static void tab_item_print(const struct shell *shell, const char *option,
			   u16_t longest_option)
{
	static const char *tab = "  ";
	u16_t columns;
	u16_t diff;

	/* Function initialization has been requested. */
	if (option == NULL) {
		shell->ctx->vt100_ctx.printed_cmd = 0;
		return;
	}

	longest_option += shell_strlen(tab);

	columns = (shell->ctx->vt100_ctx.cons.terminal_wid
			- shell_strlen(tab)) / longest_option;
	diff = longest_option - shell_strlen(option);

	if (shell->ctx->vt100_ctx.printed_cmd++ % columns == 0U) {
		shell_internal_fprintf(shell, SHELL_OPTION, "\n%s%s", tab,
				       option);
	} else {
		shell_internal_fprintf(shell, SHELL_OPTION, "%s", option);
	}

	shell_op_cursor_horiz_move(shell, diff);
}

static void history_init(const struct shell *shell)
{
	if (!IS_ENABLED(CONFIG_SHELL_HISTORY)) {
		return;
	}

	shell_history_init(shell->history);
}

static void history_purge(const struct shell *shell)
{
	if (!IS_ENABLED(CONFIG_SHELL_HISTORY)) {
		return;
	}

	shell_history_purge(shell->history);
}

static void history_mode_exit(const struct shell *shell)
{
	if (!IS_ENABLED(CONFIG_SHELL_HISTORY)) {
		return;
	}

	flag_history_exit_set(shell, false);
	shell_history_mode_exit(shell->history);
}

static void history_put(const struct shell *shell, u8_t *line, size_t length)
{
	if (!IS_ENABLED(CONFIG_SHELL_HISTORY)) {
		return;
	}

	shell_history_put(shell->history, line, length);
}

static void history_handle(const struct shell *shell, bool up)
{
	bool history_mode;
	u16_t len;

	/*optional feature */
	if (!IS_ENABLED(CONFIG_SHELL_HISTORY)) {
		return;
	}

	/* Checking if history process has been stopped */
	if (flag_history_exit_get(shell)) {
		flag_history_exit_set(shell, false);
		shell_history_mode_exit(shell->history);
	}

	/* Backup command if history is entered */
	if (!shell_history_active(shell->history)) {
		if (up) {
			u16_t cmd_len = shell_strlen(shell->ctx->cmd_buff);

			if (cmd_len) {
				strcpy(shell->ctx->temp_buff,
				       shell->ctx->cmd_buff);
			} else {
				shell->ctx->temp_buff[0] = '\0';
			}
		} else {
			/* Pressing 'down' not in history mode has no effect. */
			return;
		}
	}

	/* Start by checking if history is not empty. */
	history_mode = shell_history_get(shell->history, up,
					 shell->ctx->cmd_buff, &len);

	/* On exiting history mode print backed up command. */
	if (!history_mode) {
		strcpy(shell->ctx->cmd_buff, shell->ctx->temp_buff);
		len = shell_strlen(shell->ctx->cmd_buff);
	}

	shell_op_cursor_home_move(shell);
	clear_eos(shell);
	shell_print_cmd(shell);
	shell->ctx->cmd_buff_pos = len;
	shell->ctx->cmd_buff_len = len;
	shell_op_cond_next_line(shell);
}

static const struct shell_static_entry *find_cmd(
					     const struct shell_cmd_entry *cmd,
					     size_t lvl,
					     char *cmd_str,
					     struct shell_static_entry *d_entry)
{
	const struct shell_static_entry *entry = NULL;
	size_t idx = 0;

	do {
		shell_cmd_get(cmd, lvl, idx++, &entry, d_entry);
		if (entry && (strcmp(cmd_str, entry->syntax) == 0)) {
			return entry;
		}
	} while (entry);

	return NULL;
}

/** @brief Function for getting last valid command in list of arguments. */
static const struct shell_static_entry *get_last_command(
					     const struct shell *shell,
					     size_t argc,
					     char *argv[],
					     size_t *match_arg,
					     struct shell_static_entry *d_entry)
{
	const struct shell_static_entry *prev_entry = NULL;
	const struct shell_cmd_entry *prev_cmd = NULL;
	const struct shell_static_entry *entry = NULL;
	*match_arg = SHELL_CMD_ROOT_LVL;

	while (*match_arg < argc) {

		if (IS_ENABLED(CONFIG_SHELL_WILDCARD)) {
			/* ignore wildcard argument */
			if (shell_wildcard_character_exist(argv[*match_arg])) {
				(*match_arg)++;
				continue;
			}
		}

		entry = find_cmd(prev_cmd, *match_arg, argv[*match_arg],
				 d_entry);
		if (entry) {
			prev_cmd = entry->subcmd;
			prev_entry = entry;
			(*match_arg)++;
		} else {
			break;
		}
	}

	return entry;
}

static inline u16_t completion_space_get(const struct shell *shell)
{
	u16_t space = (CONFIG_SHELL_CMD_BUFF_SIZE - 1) -
			shell->ctx->cmd_buff_len;
	return space;
}

/* Prepare arguments and return number of space available for completion. */
static bool tab_prepare(const struct shell *shell,
			const struct shell_static_entry **cmd,
			char **argv, size_t *argc,
			size_t *complete_arg_idx,
			struct shell_static_entry *d_entry)
{
	u16_t compl_space = completion_space_get(shell);
	size_t search_argc;

	if (compl_space == 0U) {
		return false;
	}

	/* Copy command from its beginning to cursor position. */
	memcpy(shell->ctx->temp_buff, shell->ctx->cmd_buff,
			shell->ctx->cmd_buff_pos);
	shell->ctx->temp_buff[shell->ctx->cmd_buff_pos] = '\0';

	/* Create argument list. */
	(void)shell_make_argv(argc, argv, shell->ctx->temp_buff,
			      CONFIG_SHELL_ARGC_MAX);

	/* If last command is not completed (followed by space) it is treated
	 * as uncompleted one.
	 */
	int space = isspace((int)shell->ctx->cmd_buff[
						shell->ctx->cmd_buff_pos - 1]);

	/* root command completion */
	if ((*argc == 0) || ((space == 0) && (*argc == 1))) {
		*complete_arg_idx = SHELL_CMD_ROOT_LVL;
		*cmd = NULL;
		return true;
	}

	search_argc = space ? *argc : *argc - 1;

	*cmd = get_last_command(shell, search_argc, argv, complete_arg_idx,
				d_entry);

	/* if search_argc == 0 (empty command line) get_last_command will return
	 * NULL tab is allowed, otherwise not.
	 */
	if ((*cmd == NULL) && (search_argc != 0)) {
		return false;
	}

	return true;
}

/* Empty command is identified by null handler and subcommand but contrary
 * to array termination null command, it has non-null syntax address.
 */
static inline bool is_empty_cmd(const struct shell_static_entry *entry)
{
	return entry->syntax &&
		(entry->handler == NULL) && (entry->subcmd == NULL);
}

static inline bool is_completion_candidate(const char *candidate,
					   const char *str, size_t len)
{
	return (strncmp(candidate, str, len) == 0) ? true : false;
}

static void find_completion_candidates(const struct shell_static_entry *cmd,
				       const char *incompl_cmd,
				       size_t *first_idx, size_t *cnt,
				       u16_t *longest)
{
	size_t incompl_cmd_len = shell_strlen(incompl_cmd);
	const struct shell_static_entry *candidate;
	struct shell_static_entry dynamic_entry;
	bool found = false;
	size_t idx = 0;

	*longest = 0U;
	*cnt = 0;

	while (true) {
		bool is_empty;
		bool is_candidate;

		shell_cmd_get(cmd ? cmd->subcmd : NULL, cmd ? 1 : 0,
			      idx, &candidate, &dynamic_entry);

		if (!candidate) {
			break;
		}

		is_empty = is_empty_cmd(candidate);
		is_candidate = is_completion_candidate(candidate->syntax,
						incompl_cmd, incompl_cmd_len);
		if (!is_empty && is_candidate) {
			size_t slen = strlen(candidate->syntax);

			*longest = (slen > *longest) ? slen : *longest;
			(*cnt)++;

			if (!found) {
				*first_idx = idx;
			}

			found = true;
		}
		idx++;
	}
}

static void autocomplete(const struct shell *shell,
			 const struct shell_static_entry *cmd,
			 const char *arg,
			 size_t subcmd_idx)
{
	const struct shell_static_entry *match;
	u16_t cmd_len;
	u16_t arg_len = shell_strlen(arg);

	/* shell->ctx->active_cmd can be safely used outside of command context
	 * to save stack
	 */
	shell_cmd_get(cmd ? cmd->subcmd : NULL, cmd ? 1 : 0,
		      subcmd_idx, &match, &shell->ctx->active_cmd);
	cmd_len = shell_strlen(match->syntax);

	/* no exact match found */
	if (cmd_len != arg_len) {
		shell_op_completion_insert(shell,
					   match->syntax + arg_len,
					   cmd_len - arg_len);
	}

	/* Next character in the buffer is not 'space'. */
	if (!isspace((int) shell->ctx->cmd_buff[
					shell->ctx->cmd_buff_pos])) {
		if (flag_insert_mode_get(shell)) {
			flag_insert_mode_set(shell, false);
			shell_op_char_insert(shell, ' ');
			flag_insert_mode_set(shell, true);
		} else {
			shell_op_char_insert(shell, ' ');
		}
	} else {
		/*  case:
		 * | | -> cursor
		 * cons_name $: valid_cmd valid_sub_cmd| |argument  <tab>
		 */
		shell_op_cursor_move(shell, 1);
		/* result:
		 * cons_name $: valid_cmd valid_sub_cmd |a|rgument
		 */
	}
}

static size_t str_common(const char *s1, const char *s2, size_t n)
{
	size_t common = 0;

	while ((n > 0) && (*s1 == *s2) && (*s1 != '\0')) {
		s1++;
		s2++;
		n--;
		common++;
	}

	return common;
}

static void tab_options_print(const struct shell *shell,
			      const struct shell_static_entry *cmd,
			      const char *str, size_t first, size_t cnt,
			      u16_t longest)
{
	const struct shell_static_entry *match;
	size_t str_len = shell_strlen(str);
	size_t idx = first;

	/* Printing all matching commands (options). */
	tab_item_print(shell, SHELL_INIT_OPTION_PRINTER, longest);

	while (cnt) {
		bool is_empty;

		/* shell->ctx->active_cmd can be safely used outside of command
		 * context to save stack
		 */
		shell_cmd_get(cmd ? cmd->subcmd : NULL, cmd ? 1 : 0,
			      idx, &match, &shell->ctx->active_cmd);
		idx++;
		is_empty = is_empty_cmd(match);
		if (is_empty || (str && match->syntax &&
		    !is_completion_candidate(match->syntax, str, str_len))) {
			continue;
		}

		tab_item_print(shell, match->syntax, longest);
		cnt--;
	}

	cursor_next_line_move(shell);
	shell_print_prompt_and_cmd(shell);
}

static u16_t common_beginning_find(const struct shell_static_entry *cmd,
				   const char **str,
				   size_t first, size_t cnt, u16_t arg_len)
{
	struct shell_static_entry dynamic_entry;
	const struct shell_static_entry *match;
	u16_t common = UINT16_MAX;
	size_t idx = first + 1;

	__ASSERT_NO_MSG(cnt > 1);

	shell_cmd_get(cmd ? cmd->subcmd : NULL, cmd ? 1 : 0,
		      first, &match, &dynamic_entry);

	*str = match->syntax;

	while (cnt > 1) {
		struct shell_static_entry dynamic_entry2;
		const struct shell_static_entry *match2;
		int curr_common;

		shell_cmd_get(cmd ? cmd->subcmd : NULL, cmd ? 1 : 0,
			      idx++, &match2, &dynamic_entry2);

		if (match2 == NULL) {
			break;
		}

		curr_common = str_common(match->syntax, match2->syntax,
					 UINT16_MAX);
		if ((arg_len == 0U) || (curr_common >= arg_len)) {
			--cnt;
			common = (curr_common < common) ? curr_common : common;
		}
	}

	return common;
}

static void partial_autocomplete(const struct shell *shell,
				 const struct shell_static_entry *cmd,
				 const char *arg,
				 size_t first, size_t cnt)
{
	const char *completion;
	u16_t arg_len = shell_strlen(arg);
	u16_t common = common_beginning_find(cmd, &completion,
			first, cnt, arg_len);

	if (common) {
		shell_op_completion_insert(shell, &completion[arg_len],
					   common - arg_len);
	}
}

static int exec_cmd(const struct shell *shell, size_t argc, char **argv,
		    const struct shell_static_entry *help_entry)
{
	int ret_val = 0;

	if (shell->ctx->active_cmd.handler == NULL) {
		if ((help_entry != NULL) && IS_ENABLED(CONFIG_SHELL_HELP)) {
			if (help_entry->help == NULL) {
				return -ENOEXEC;
			}
			if (help_entry->help != shell->ctx->active_cmd.help) {
				shell->ctx->active_cmd = *help_entry;
			}
			shell_internal_help_print(shell);
			return SHELL_CMD_HELP_PRINTED;
		} else {
			shell_internal_fprintf(shell, SHELL_ERROR,
					       SHELL_MSG_SPECIFY_SUBCOMMAND);
			return -ENOEXEC;
		}
	}

	if (shell->ctx->active_cmd.args.mandatory) {
		u8_t mand = shell->ctx->active_cmd.args.mandatory;
		u8_t opt = shell->ctx->active_cmd.args.optional;
		bool in_range = (argc >= mand) && (argc <= (mand + opt));

		/* Check if argc is within allowed range */
		ret_val = cmd_precheck(shell, in_range);
	}

	if (!ret_val) {
		/* Unlock thread mutex in case command would like to borrow
		 * shell context to other thread to avoid mutex deadlock.
		 */
		k_mutex_unlock(&shell->ctx->wr_mtx);
		flag_cmd_ctx_set(shell, 1);
		ret_val = shell->ctx->active_cmd.handler(shell, argc, argv);
		flag_cmd_ctx_set(shell, 0);
		/* Bring back mutex to shell thread. */
		k_mutex_lock(&shell->ctx->wr_mtx, K_FOREVER);
	}

	return ret_val;
}

/* Function is analyzing the command buffer to find matching commands. Next, it
 * invokes the  last recognized command which has a handler and passes the rest
 * of command buffer as arguments.
 */
static int execute(const struct shell *shell)
{
	struct shell_static_entry d_entry; /* Memory for dynamic commands. */
	char *argv[CONFIG_SHELL_ARGC_MAX + 1]; /* +1 reserved for NULL */
	const struct shell_static_entry *p_static_entry = NULL;
	const struct shell_cmd_entry *p_cmd = NULL;
	struct shell_static_entry help_entry;
	size_t cmd_lvl = SHELL_CMD_ROOT_LVL;
	size_t cmd_with_handler_lvl = 0;
	bool wildcard_found = false;
	size_t cmd_idx = 0;
	size_t argc;
	char quote;

	shell_op_cursor_end_move(shell);
	if (!shell_cursor_in_empty_line(shell)) {
		cursor_next_line_move(shell);
	}

	memset(&shell->ctx->active_cmd, 0, sizeof(shell->ctx->active_cmd));

	shell_cmd_trim(shell);

	history_put(shell, shell->ctx->cmd_buff,
		    shell->ctx->cmd_buff_len);

	if (IS_ENABLED(CONFIG_SHELL_WILDCARD)) {
		shell_wildcard_prepare(shell);
	}

	/* create argument list */
	quote = shell_make_argv(&argc, &argv[0], shell->ctx->cmd_buff,
				CONFIG_SHELL_ARGC_MAX);

	if (!argc) {
		return -ENOEXEC;
	}

	if (quote != 0) {
		shell_internal_fprintf(shell, SHELL_ERROR,
				       "not terminated: %c\n",
				       quote);
		return -ENOEXEC;
	}

	/* Initialize help variable */
	help_entry.help = NULL;

	/* Below loop is analyzing subcommands of found root command. */
	while (true) {
		if (cmd_lvl >= argc) {
			break;
		}

		if (IS_ENABLED(CONFIG_SHELL_HELP) && (cmd_lvl > 0) &&
		    (!strcmp(argv[cmd_lvl], "-h") ||
		     !strcmp(argv[cmd_lvl], "--help"))) {
			/* Command called with help option so it makes no sense
			 * to search deeper commands.
			 */
			if (help_entry.help) {
				shell->ctx->active_cmd = help_entry;
				shell_internal_help_print(shell);
				return SHELL_CMD_HELP_PRINTED;
			}

			shell_internal_fprintf(shell, SHELL_ERROR,
					       SHELL_MSG_SPECIFY_SUBCOMMAND);
			return -ENOEXEC;
		}

		if (IS_ENABLED(CONFIG_SHELL_WILDCARD) && (cmd_lvl > 0)) {
			enum shell_wildcard_status status;

			status = shell_wildcard_process(shell, p_cmd,
							argv[cmd_lvl]);
			/* Wildcard character found but there is no matching
			 * command.
			 */
			if (status == SHELL_WILDCARD_CMD_NO_MATCH_FOUND) {
				break;
			}

			/* Wildcard character was not found function can process
			 * argument.
			 */
			if (status != SHELL_WILDCARD_NOT_FOUND) {
				++cmd_lvl;
				wildcard_found = true;
				continue;
			}
		}

		shell_cmd_get(p_cmd, cmd_lvl, cmd_idx++, &p_static_entry,
			      &d_entry);

		if ((cmd_idx == 0) || (p_static_entry == NULL)) {
			if (cmd_lvl == 0) {
				shell_internal_fprintf(shell, SHELL_ERROR,
						       "%s%s\n", argv[0],
						       SHELL_MSG_CMD_NOT_FOUND);
				return -ENOEXEC;
			}
			break;
		}

		if (strcmp(argv[cmd_lvl], p_static_entry->syntax) == 0) {
			/* checking if command has a handler */
			if (p_static_entry->handler != NULL) {
				if (IS_ENABLED(CONFIG_SHELL_WILDCARD) &&
				    (wildcard_found)) {
					shell_op_cursor_end_move(shell);
					shell_op_cond_next_line(shell);

					/* An error occurred, fnmatch  argument
					 * cannot be followed by argument with
					 * a handler to avoid multiple function
					 * calls.
					 */
					shell_internal_fprintf(shell,
						SHELL_ERROR,
						"Error: requested multiple"
						" function executions\n");

					return -ENOEXEC;
				}

				shell->ctx->active_cmd = *p_static_entry;
				cmd_with_handler_lvl = cmd_lvl;
			}
			/* checking if function has a help handler */
			if (p_static_entry->help != NULL) {
				help_entry = *p_static_entry;
			}

			cmd_lvl++;
			cmd_idx = 0;
			p_cmd = p_static_entry->subcmd;
		}
	}

	if (IS_ENABLED(CONFIG_SHELL_WILDCARD) && wildcard_found) {
		shell_wildcard_finalize(shell);
		/* cmd_buffer has been overwritten by function finalize function
		 * with all expanded commands. Hence shell_make_argv needs to
		 * be called again.
		 */
		(void)shell_make_argv(&argc, &argv[0],
				      shell->ctx->cmd_buff,
				      CONFIG_SHELL_ARGC_MAX);
	}

	/* Executing the deepest found handler. */
	return exec_cmd(shell, argc - cmd_with_handler_lvl,
			&argv[cmd_with_handler_lvl], &help_entry);
}

static void tab_handle(const struct shell *shell)
{
	/* +1 reserved for NULL in function shell_make_argv */
	char *argv[CONFIG_SHELL_ARGC_MAX + 1];
	/* d_entry - placeholder for dynamic command */
	struct shell_static_entry d_entry;
	const struct shell_static_entry *cmd;
	size_t first = 0;
	size_t arg_idx;
	u16_t longest;
	size_t argc;
	size_t cnt;

	bool tab_possible = tab_prepare(shell, &cmd, argv, &argc,
					      &arg_idx, &d_entry);

	if (tab_possible == false) {
		return;
	}

	find_completion_candidates(cmd, argv[arg_idx], &first, &cnt, &longest);

	if (cnt == 1) {
		/* Autocompletion.*/
		autocomplete(shell, cmd, argv[arg_idx], first);
	} else if (cnt > 1) {
		tab_options_print(shell, cmd, argv[arg_idx], first, cnt,
				  longest);
		partial_autocomplete(shell, cmd, argv[arg_idx], first, cnt);
	}
}

static void alt_metakeys_handle(const struct shell *shell, char data)
{
	/* Optional feature */
	if (!IS_ENABLED(CONFIG_SHELL_METAKEYS)) {
		return;
	}
	if (data == SHELL_VT100_ASCII_ALT_B) {
		shell_op_cursor_word_move(shell, -1);
	} else if (data == SHELL_VT100_ASCII_ALT_F) {
		shell_op_cursor_word_move(shell, 1);
	}
}

static void ctrl_metakeys_handle(const struct shell *shell, char data)
{
	/* Optional feature */
	if (!IS_ENABLED(CONFIG_SHELL_METAKEYS)) {
		return;
	}

	switch (data) {
	case SHELL_VT100_ASCII_CTRL_A: /* CTRL + A */
		shell_op_cursor_home_move(shell);
		break;

	case SHELL_VT100_ASCII_CTRL_B: /* CTRL + B */
		shell_op_left_arrow(shell);
		break;

	case SHELL_VT100_ASCII_CTRL_C: /* CTRL + C */
		shell_op_cursor_end_move(shell);
		if (!shell_cursor_in_empty_line(shell)) {
			cursor_next_line_move(shell);
		}
		flag_history_exit_set(shell, true);
		state_set(shell, SHELL_STATE_ACTIVE);
		break;

	case SHELL_VT100_ASCII_CTRL_D: /* CTRL + D */
		shell_op_char_delete(shell);
		break;

	case SHELL_VT100_ASCII_CTRL_E: /* CTRL + E */
		shell_op_cursor_end_move(shell);
		break;

	case SHELL_VT100_ASCII_CTRL_F: /* CTRL + F */
		shell_op_right_arrow(shell);
		break;

	case SHELL_VT100_ASCII_CTRL_K: /* CTRL + K */
		shell_op_delete_from_cursor(shell);
		break;

	case SHELL_VT100_ASCII_CTRL_L: /* CTRL + L */
		SHELL_VT100_CMD(shell, SHELL_VT100_CURSORHOME);
		SHELL_VT100_CMD(shell, SHELL_VT100_CLEARSCREEN);
		shell_print_prompt_and_cmd(shell);
		break;

	case SHELL_VT100_ASCII_CTRL_U: /* CTRL + U */
		shell_op_cursor_home_move(shell);
		cmd_buffer_clear(shell);
		flag_history_exit_set(shell, true);
		clear_eos(shell);
		break;

	case SHELL_VT100_ASCII_CTRL_W: /* CTRL + W */
		shell_op_word_remove(shell);
		flag_history_exit_set(shell, true);
		break;

	default:
		break;
	}
}

/* Functions returns true if new line character shall be processed */
static bool process_nl(const struct shell *shell, u8_t data)
{
	if ((data != '\r') && (data != '\n')) {
		flag_last_nl_set(shell, 0);
		return false;
	}

	if ((flag_last_nl_get(shell) == 0U) ||
	    (data == flag_last_nl_get(shell))) {
		flag_last_nl_set(shell, data);
		return true;
	}

	return false;
}

#define SHELL_ASCII_MAX_CHAR (127u)
static inline int ascii_filter(const char data)
{
	return (u8_t) data > SHELL_ASCII_MAX_CHAR ? -EINVAL : 0;
}

static void state_collect(const struct shell *shell)
{
	size_t count = 0;
	char data;

	while (true) {
		(void)shell->iface->api->read(shell->iface, &data,
					      sizeof(data), &count);
		if (count == 0) {
			return;
		}

		if (ascii_filter(data) != 0) {
			continue;
		}

		switch (shell->ctx->receive_state) {
		case SHELL_RECEIVE_DEFAULT:
			if (process_nl(shell, data)) {
				if (!shell->ctx->cmd_buff_len) {
					history_mode_exit(shell);
					cursor_next_line_move(shell);
				} else {
					/* Command execution */
					(void)execute(shell);
				}
				/* Function responsible for printing prompt
				 * on received NL.
				 */
				state_set(shell, SHELL_STATE_ACTIVE);
				continue;
			}

			switch (data) {
			case SHELL_VT100_ASCII_ESC: /* ESCAPE */
				receive_state_change(shell, SHELL_RECEIVE_ESC);
				break;

			case '\0':
				break;

			case '\t': /* TAB */
				if (flag_echo_get(shell)) {
					/* If the Tab key is pressed, "history
					 * mode" must be terminated because
					 * tab and history handlers are sharing
					 * the same array: temp_buff.
					 */
					flag_history_exit_set(shell, true);
					tab_handle(shell);
				}
				break;

			case SHELL_VT100_ASCII_BSPACE: /* BACKSPACE */
				if (flag_echo_get(shell)) {
					flag_history_exit_set(shell, true);
					shell_op_char_backspace(shell);
				}
				break;

			case SHELL_VT100_ASCII_DEL: /* DELETE */
				if (flag_echo_get(shell)) {
					flag_history_exit_set(shell, true);
					if (flag_mode_delete_get(shell)) {
						shell_op_char_backspace(shell);

					} else {
						shell_op_char_delete(shell);
					}
				}
				break;

			default:
				if (isprint((int) data)) {
					flag_history_exit_set(shell, true);
					shell_op_char_insert(shell, data);
				} else if (flag_echo_get(shell)) {
					ctrl_metakeys_handle(shell, data);
				}
				break;
			}
			break;

		case SHELL_RECEIVE_ESC:
			if (data == '[') {
				receive_state_change(shell,
						SHELL_RECEIVE_ESC_SEQ);
				break;
			} else if (flag_echo_get(shell)) {
				alt_metakeys_handle(shell, data);
			}
			receive_state_change(shell, SHELL_RECEIVE_DEFAULT);
			break;

		case SHELL_RECEIVE_ESC_SEQ:
			receive_state_change(shell, SHELL_RECEIVE_DEFAULT);

			if (!flag_echo_get(shell)) {
				continue;
			}

			switch (data) {
			case 'A': /* UP arrow */
				history_handle(shell, true);
				break;

			case 'B': /* DOWN arrow */
				history_handle(shell, false);
				break;

			case 'C': /* RIGHT arrow */
				shell_op_right_arrow(shell);
				break;

			case 'D': /* LEFT arrow */
				shell_op_left_arrow(shell);
				break;

			case '4': /* END Button in ESC[n~ mode */
				receive_state_change(shell,
						SHELL_RECEIVE_TILDE_EXP);
				/* fall through */
				/* no break */
			case 'F': /* END Button in VT100 mode */
				shell_op_cursor_end_move(shell);
				break;

			case '1': /* HOME Button in ESC[n~ mode */
				receive_state_change(shell,
						SHELL_RECEIVE_TILDE_EXP);
				/* fall through */
				/* no break */
			case 'H': /* HOME Button in VT100 mode */
				shell_op_cursor_home_move(shell);
				break;

			case '2': /* INSERT Button in ESC[n~ mode */
				receive_state_change(shell,
						SHELL_RECEIVE_TILDE_EXP);
				/* fall through */
				/* no break */
			case 'L': {/* INSERT Button in VT100 mode */
				bool status = flag_insert_mode_get(shell);
				flag_insert_mode_set(shell, !status);
				break;
			}

			case '3':/* DELETE Button in ESC[n~ mode */
				receive_state_change(shell,
						SHELL_RECEIVE_TILDE_EXP);
				if (flag_echo_get(shell)) {
					shell_op_char_delete(shell);
				}
				break;

			default:
				break;
			}
			break;

		case SHELL_RECEIVE_TILDE_EXP:
			receive_state_change(shell, SHELL_RECEIVE_DEFAULT);
			break;

		default:
			receive_state_change(shell, SHELL_RECEIVE_DEFAULT);
			break;
		}
	}

	transport_buffer_flush(shell);
}

static void transport_evt_handler(enum shell_transport_evt evt_type, void *ctx)
{
	struct shell *shell = (struct shell *)ctx;
	struct k_poll_signal *signal;

	signal = (evt_type == SHELL_TRANSPORT_EVT_RX_RDY) ?
			&shell->ctx->signals[SHELL_SIGNAL_RXRDY] :
			&shell->ctx->signals[SHELL_SIGNAL_TXDONE];
	k_poll_signal_raise(signal, 0);
}

static void shell_log_process(const struct shell *shell)
{
	bool processed = false;
	int signaled = 0;
	int result;

	do {
		if (!IS_ENABLED(CONFIG_LOG_IMMEDIATE)) {
			shell_cmd_line_erase(shell);

			processed = shell_log_backend_process(shell->log_backend);
		}

		struct k_poll_signal *signal =
			&shell->ctx->signals[SHELL_SIGNAL_RXRDY];

		shell_print_prompt_and_cmd(shell);

		/* Arbitrary delay added to ensure that prompt is
		 * readable and can be used to enter further commands.
		 */
		if (shell->ctx->cmd_buff_len) {
			k_sleep(K_MSEC(15));
		}

		k_poll_signal_check(signal, &signaled, &result);

	} while (processed && !signaled);
}

static int instance_init(const struct shell *shell, const void *p_config,
			 bool use_colors)
{
	__ASSERT_NO_MSG((shell->shell_flag == SHELL_FLAG_CRLF_DEFAULT) ||
			(shell->shell_flag == SHELL_FLAG_OLF_CRLF));

	int err = shell->iface->api->init(shell->iface, p_config,
					  transport_evt_handler,
					  (void *) shell);
	if (err != 0) {
		return err;
	}

	memset(shell->ctx, 0, sizeof(*shell->ctx));
	shell->ctx->prompt = shell->default_prompt;

	history_init(shell);

	k_mutex_init(&shell->ctx->wr_mtx);

	for (int i = 0; i < SHELL_SIGNALS; i++) {
		k_poll_signal_init(&shell->ctx->signals[i]);
		k_poll_event_init(&shell->ctx->events[i],
				  K_POLL_TYPE_SIGNAL,
				  K_POLL_MODE_NOTIFY_ONLY,
				  &shell->ctx->signals[i]);
	}

	if (IS_ENABLED(CONFIG_SHELL_STATS)) {
		shell->stats->log_lost_cnt = 0;
	}

	flag_tx_rdy_set(shell, true);
	flag_echo_set(shell, IS_ENABLED(CONFIG_SHELL_ECHO_STATUS));
	flag_mode_delete_set(shell,
			     IS_ENABLED(CONFIG_SHELL_BACKSPACE_MODE_DELETE));
	shell->ctx->state = SHELL_STATE_INITIALIZED;
	shell->ctx->vt100_ctx.cons.terminal_wid = SHELL_DEFAULT_TERMINAL_WIDTH;
	shell->ctx->vt100_ctx.cons.terminal_hei = SHELL_DEFAULT_TERMINAL_HEIGHT;
	shell->ctx->vt100_ctx.cons.name_len = shell_strlen(shell->ctx->prompt);
	flag_use_colors_set(shell, IS_ENABLED(CONFIG_SHELL_VT100_COLORS));

	return 0;
}

static int instance_uninit(const struct shell *shell)
{
	__ASSERT_NO_MSG(shell);
	__ASSERT_NO_MSG(shell->ctx && shell->iface);

	int err;

	if (flag_processing_get(shell)) {
		return -EBUSY;
	}

	if (IS_ENABLED(CONFIG_LOG)) {
		/* todo purge log queue */
		shell_log_backend_disable(shell->log_backend);
	}

	err = shell->iface->api->uninit(shell->iface);
	if (err != 0) {
		return err;
	}

	history_purge(shell);

	shell->ctx->state = SHELL_STATE_UNINITIALIZED;

	return 0;
}

typedef void (*shell_signal_handler_t)(const struct shell *shell);

static void shell_signal_handle(const struct shell *shell,
				enum shell_signal sig_idx,
				shell_signal_handler_t handler)
{
	struct k_poll_signal *signal = &shell->ctx->signals[sig_idx];
	int set;
	int res;

	k_poll_signal_check(signal, &set, &res);

	if (set) {
		k_poll_signal_reset(signal);
		handler(shell);
	}
}

static void kill_handler(const struct shell *shell)
{
	(void)instance_uninit(shell);
	k_thread_abort(k_current_get());
}

void shell_thread(void *shell_handle, void *arg_log_backend,
		  void *arg_log_level)
{
	struct shell *shell = (struct shell *)shell_handle;
	bool log_backend = (bool)arg_log_backend;
	u32_t log_level = (u32_t)arg_log_level;
	int err;

	err = shell->iface->api->enable(shell->iface, false);
	if (err != 0) {
		return;
	}

	if (log_backend && IS_ENABLED(CONFIG_LOG)) {
		shell_log_backend_enable(shell->log_backend, (void *)shell,
					 log_level);
	}

	/* Enable shell and print prompt. */
	err = shell_start(shell);
	if (err != 0) {
		return;
	}

	while (true) {
		/* waiting for all signals except SHELL_SIGNAL_TXDONE */
		err = k_poll(shell->ctx->events, SHELL_SIGNAL_TXDONE,
			     K_FOREVER);

		k_mutex_lock(&shell->ctx->wr_mtx, K_FOREVER);

		if (err != 0) {
			shell_internal_fprintf(shell, SHELL_ERROR,
					       "Shell thread error: %d", err);
			return;
		}

		if (shell->iface->api->update) {
			shell->iface->api->update(shell->iface);
		}

		shell_signal_handle(shell, SHELL_SIGNAL_KILL, kill_handler);
		shell_signal_handle(shell, SHELL_SIGNAL_RXRDY, shell_process);
		if (IS_ENABLED(CONFIG_LOG)) {
			shell_signal_handle(shell, SHELL_SIGNAL_LOG_MSG,
					    shell_log_process);
		}

		k_mutex_unlock(&shell->ctx->wr_mtx);
	}
}

int shell_init(const struct shell *shell, const void *transport_config,
	       bool use_colors, bool log_backend, u32_t init_log_level)
{
	__ASSERT_NO_MSG(shell);
	__ASSERT_NO_MSG(shell->ctx && shell->iface && shell->default_prompt);

	int err = instance_init(shell, transport_config, use_colors);

	if (err != 0) {
		return err;
	}

	k_tid_t tid = k_thread_create(shell->thread,
			      shell->stack, CONFIG_SHELL_STACK_SIZE,
			      shell_thread, (void *)shell, (void *)log_backend,
			      (void *)init_log_level,
			      K_LOWEST_APPLICATION_THREAD_PRIO, 0, K_NO_WAIT);

	shell->ctx->tid = tid;
	k_thread_name_set(tid, shell->thread_name);

	return 0;
}

int shell_uninit(const struct shell *shell)
{
	__ASSERT_NO_MSG(shell);

	if (IS_ENABLED(CONFIG_MULTITHREADING)) {
		struct k_poll_signal *signal =
				&shell->ctx->signals[SHELL_SIGNAL_KILL];

		/* signal kill message */
		(void)k_poll_signal_raise(signal, 0);

		return 0;
	} else {
		return instance_uninit(shell);
	}
}

int shell_start(const struct shell *shell)
{
	__ASSERT_NO_MSG(shell);
	__ASSERT_NO_MSG(shell->ctx && shell->iface && shell->default_prompt);

	if (shell->ctx->state != SHELL_STATE_INITIALIZED) {
		return -ENOTSUP;
	}

	if (IS_ENABLED(CONFIG_SHELL_VT100_COLORS)) {
		shell_vt100_color_set(shell, SHELL_NORMAL);
	}

	shell_raw_fprintf(shell->fprintf_ctx, "\n\n");

	state_set(shell, SHELL_STATE_ACTIVE);

	return 0;
}

int shell_stop(const struct shell *shell)
{
	__ASSERT_NO_MSG(shell);
	__ASSERT_NO_MSG(shell->ctx);

	if ((shell->ctx->state == SHELL_STATE_INITIALIZED) ||
	    (shell->ctx->state == SHELL_STATE_UNINITIALIZED)) {
		return -ENOTSUP;
	}

	state_set(shell, SHELL_STATE_INITIALIZED);

	return 0;
}

void shell_process(const struct shell *shell)
{
	__ASSERT_NO_MSG(shell);
	__ASSERT_NO_MSG(shell->ctx);

	union shell_internal internal;

	internal.value = 0U;
	internal.flags.processing = 1U;

	(void)atomic_or((atomic_t *)&shell->ctx->internal.value,
			internal.value);

	switch (shell->ctx->state) {
	case SHELL_STATE_UNINITIALIZED:
	case SHELL_STATE_INITIALIZED:
		/* Console initialized but not started. */
		break;

	case SHELL_STATE_ACTIVE:
		state_collect(shell);
		break;
	default:
		break;
	}

	internal.value = 0xFFFFFFFF;
	internal.flags.processing = 0U;
	(void)atomic_and((atomic_t *)&shell->ctx->internal.value,
			 internal.value);
}

/* This function mustn't be used from shell context to avoid deadlock.
 * However it can be used in shell command handlers.
 */
void shell_fprintf(const struct shell *shell, enum shell_vt100_color color,
		   const char *fmt, ...)
{
	__ASSERT_NO_MSG(shell);
	__ASSERT(!k_is_in_isr(), "Thread context required.");
	__ASSERT_NO_MSG((shell->ctx->internal.flags.cmd_ctx == 1) ||
			(k_current_get() != shell->ctx->tid));
	__ASSERT_NO_MSG(shell->ctx);
	__ASSERT_NO_MSG(shell->fprintf_ctx);
	__ASSERT_NO_MSG(fmt);

	va_list args = { 0 };

	k_mutex_lock(&shell->ctx->wr_mtx, K_FOREVER);
	if (!flag_cmd_ctx_get(shell)) {
		shell_cmd_line_erase(shell);
	}

	va_start(args, fmt);
	shell_internal_vfprintf(shell, color, fmt, args);
	va_end(args);

	if (!flag_cmd_ctx_get(shell)) {
		shell_print_prompt_and_cmd(shell);
	}
	transport_buffer_flush(shell);
	k_mutex_unlock(&shell->ctx->wr_mtx);
}

void shell_hexdump(const struct shell *shell, const u8_t *data, size_t len)
{
	int n = 0;

	while (len--) {
		if (n % 16 == 0) {
			shell_fprintf(shell, SHELL_NORMAL, "%08X: ", n);
		}

		shell_fprintf(shell, SHELL_NORMAL, "%02X ", *data++);

		n++;
		if (n % 16 == 0) {
			shell_print(shell, "");
		}
	}

	if (n % 16) {
		shell_print(shell, "");
	}
}

int shell_prompt_change(const struct shell *shell, const char *prompt)
{
	__ASSERT_NO_MSG(shell);

	if (prompt == NULL) {
		return -EINVAL;
	}
	shell->ctx->prompt = prompt;
	shell->ctx->vt100_ctx.cons.name_len = shell_strlen(prompt);

	return 0;
}

void shell_help(const struct shell *shell)
{
	k_mutex_lock(&shell->ctx->wr_mtx, K_FOREVER);
	shell_internal_help_print(shell);
	k_mutex_unlock(&shell->ctx->wr_mtx);
}

int shell_execute_cmd(const struct shell *shell, const char *cmd)
{
	u16_t cmd_len = shell_strlen(cmd);
	int ret_val;

	if (cmd == NULL) {
		return -ENOEXEC;
	}

	if (cmd_len > (CONFIG_SHELL_CMD_BUFF_SIZE - 1)) {
		return -ENOMEM;
	}

	if (shell == NULL) {
#if CONFIG_SHELL_BACKEND_DUMMY
		shell = shell_backend_dummy_get_ptr();
#else
		return -EINVAL;
#endif
	}

	__ASSERT(shell->ctx->internal.flags.cmd_ctx == 0,
						"Function cannot be called"
						" from command context");

	strcpy(shell->ctx->cmd_buff, cmd);
	shell->ctx->cmd_buff_len = cmd_len;
	shell->ctx->cmd_buff_pos = cmd_len;

	k_mutex_lock(&shell->ctx->wr_mtx, K_FOREVER);
	ret_val = execute(shell);
	k_mutex_unlock(&shell->ctx->wr_mtx);

	return ret_val;
}
