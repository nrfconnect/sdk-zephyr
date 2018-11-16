/*
 * Copyright (c) 2018 Linaro Limited
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief File descriptor table
 *
 * This file provides generic file descriptor table implementation, suitable
 * for any I/O object implementing POSIX I/O semantics (i.e. read/write +
 * aux operations).
 */

#include <errno.h>
#include <kernel.h>
#include <misc/fdtable.h>

struct fd_entry {
	void *obj;
	const struct fd_op_vtable *vtable;
};

/* A few magic values for fd_entry::obj used in the code. */
#define FD_OBJ_RESERVED (void *)1
#define FD_OBJ_STDIN  (void *)0x10
#define FD_OBJ_STDOUT (void *)0x11
#define FD_OBJ_STDERR (void *)0x12

#ifdef CONFIG_POSIX_API
static const struct fd_op_vtable stdinout_fd_op_vtable;
#endif

__kernel static struct fd_entry fdtable[CONFIG_POSIX_MAX_FDS] = {
#ifdef CONFIG_POSIX_API
	/*
	 * Predefine entries for stdin/stdout/stderr. Object pointer
	 * is unused and just should be !0 (random different values
	 * are used to posisbly help with debugging).
	 */
	{FD_OBJ_STDIN,  &stdinout_fd_op_vtable},
	{FD_OBJ_STDOUT, &stdinout_fd_op_vtable},
	{FD_OBJ_STDERR, &stdinout_fd_op_vtable},
#endif
};

static K_MUTEX_DEFINE(fdtable_lock);

static int _find_fd_entry(void)
{
	int fd;

	for (fd = 0; fd < ARRAY_SIZE(fdtable); fd++) {
		if (fdtable[fd].obj == NULL) {
			return fd;
		}
	}

	errno = ENFILE;
	return -1;
}

static int _check_fd(int fd)
{
	if (fd < 0 || fd >= ARRAY_SIZE(fdtable) || fdtable[fd].obj == NULL) {
		errno = EBADF;
		return -1;
	}

	return 0;
}

void *z_get_fd_obj(int fd, const struct fd_op_vtable *vtable, int err)
{
	struct fd_entry *fd_entry;

	if (_check_fd(fd) < 0) {
		return NULL;
	}

	fd_entry = &fdtable[fd];

	if (vtable != NULL && fd_entry->vtable != vtable) {
		errno = err;
		return NULL;
	}

	return fd_entry->obj;
}

int z_reserve_fd(void)
{
	int fd;

	(void)k_mutex_lock(&fdtable_lock, K_FOREVER);

	fd = _find_fd_entry();
	if (fd >= 0) {
		/* Mark entry as used, z_finalize_fd() will fill it in. */
		fdtable[fd].obj = FD_OBJ_RESERVED;
	}

	k_mutex_unlock(&fdtable_lock);

	return fd;
}

void z_finalize_fd(int fd, void *obj, const struct fd_op_vtable *vtable)
{
	/* Assumes fd was already bounds-checked. */
	fdtable[fd].obj = obj;
	fdtable[fd].vtable = vtable;
}

void z_free_fd(int fd)
{
	/* Assumes fd was already bounds-checked. */
	fdtable[fd].obj = NULL;
}

int z_alloc_fd(void *obj, const struct fd_op_vtable *vtable)
{
	int fd;

	fd = z_reserve_fd();
	if (fd >= 0) {
		z_finalize_fd(fd, obj, vtable);
	}

	return fd;
}

#ifdef CONFIG_POSIX_API

ssize_t read(int fd, void *buf, size_t sz)
{
	if (_check_fd(fd) < 0) {
		return -1;
	}

	return fdtable[fd].vtable->read(fdtable[fd].obj, buf, sz);
}

ssize_t write(int fd, const void *buf, size_t sz)
{
	if (_check_fd(fd) < 0) {
		return -1;
	}

	return fdtable[fd].vtable->write(fdtable[fd].obj, buf, sz);
}

int close(int fd)
{
	int res;

	if (_check_fd(fd) < 0) {
		return -1;
	}

	res = fdtable[fd].vtable->ioctl(fdtable[fd].obj, ZFD_IOCTL_CLOSE);
	z_free_fd(fd);

	return res;
}

int fsync(int fd)
{
	int res;

	if (_check_fd(fd) < 0) {
		return -1;
	}

	res = fdtable[fd].vtable->ioctl(fdtable[fd].obj, ZFD_IOCTL_FSYNC);
	z_free_fd(fd);

	return res;
}

off_t lseek(int fd, off_t offset, int whence)
{
	if (_check_fd(fd) < 0) {
		return -1;
	}

	return fdtable[fd].vtable->ioctl(fdtable[fd].obj, ZFD_IOCTL_LSEEK,
					 offset, whence);
}

/*
 * fd operations for stdio/stdout/stderr
 */

int _impl__zephyr_write(const char *buf, int nbytes);

static ssize_t stdinout_read_vmeth(void *obj, void *buffer, size_t count)
{
	return 0;
}

static ssize_t stdinout_write_vmeth(void *obj, const void *buffer, size_t count)
{
#if defined(CONFIG_BOARD_NATIVE_POSIX)
	return write(1, buffer, count);
#elif defined(CONFIG_NEWLIB_LIBC)
	return _impl__zephyr_write(buffer, count);
#else
	return 0;
#endif
}

static int stdinout_ioctl_vmeth(void *obj, unsigned int request, ...)
{
	errno = EINVAL;
	return -1;
}


static const struct fd_op_vtable stdinout_fd_op_vtable = {
	.read = stdinout_read_vmeth,
	.write = stdinout_write_vmeth,
	.ioctl = stdinout_ioctl_vmeth,
};

#endif /* CONFIG_POSIX_API */
