/*
 * Copyright (c) 2024 Abhinav Srivastava
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/posix/stropts.h>
#include <errno.h>
#include <zephyr/kernel.h>

int putmsg(int fildes, const struct strbuf *ctlptr, const struct strbuf *dataptr, int flags)
{
	ARG_UNUSED(fildes);
	ARG_UNUSED(ctlptr);
	ARG_UNUSED(dataptr);
	ARG_UNUSED(flags);

	errno = ENOSYS;
	return -1;
}
