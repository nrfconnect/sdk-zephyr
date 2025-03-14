/*
 * Copyright (c) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "kernel_shell.h"

#include <kernel_internal.h>
#include <zephyr/kernel.h>
#include <stdint.h>
#include <stdlib.h>

static int cmd_kernel_thread_resume(const struct shell *sh, size_t argc, char **argv)
{
	/* thread_id is converetd from hex to decimal */
	k_tid_t thread_id = (k_tid_t)strtoul(argv[1], NULL, 16);

	if (!z_thread_is_valid(thread_id)) {
		shell_error(sh, "Thread ID %p is not valid", thread_id);
		return -EINVAL;
	}

	/*Check if the thread ID is the shell thread */
	if (thread_id == k_current_get()) {
		shell_error(sh, "Error:Shell thread cannot be resumed");
		return -EINVAL;
	}

	k_thread_resume(thread_id);

	shell_print(sh, "Thread %p resumed", thread_id);

	return 0;
}

KERNEL_THREAD_CMD_ARG_ADD(resume, NULL, "kernel thread resume <thread_id>",
			  cmd_kernel_thread_resume, 2, 0);
