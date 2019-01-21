/*
 * Copyright (c) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <kernel.h>
#include <limits.h>
#include <posix/unistd.h>
#include <posix/dirent.h>
#include <string.h>
#include <misc/fdtable.h>

BUILD_ASSERT_MSG(PATH_MAX > MAX_FILE_NAME,
		"PATH_MAX is less than MAX_FILE_NAME");

struct posix_fs_desc {
	union {
		struct fs_file_t file;
		struct fs_dir_t	dir;
	};
	bool is_dir;
	bool used;
};

static struct posix_fs_desc desc_array[CONFIG_POSIX_MAX_OPEN_FILES];

static struct fs_dirent fdirent;
static struct dirent pdirent;

static struct fd_op_vtable fs_fd_op_vtable;

static struct posix_fs_desc *posix_fs_alloc_obj(bool is_dir)
{
	int i;
	struct posix_fs_desc *ptr = NULL;
	unsigned int key = irq_lock();

	for (i = 0; i < CONFIG_POSIX_MAX_OPEN_FILES; i++) {
		if (desc_array[i].used == false) {
			ptr = &desc_array[i];
			ptr->used = true;
			ptr->is_dir = is_dir;
			break;
		}
	}
	irq_unlock(key);

	return ptr;
}

static inline void posix_fs_free_obj(struct posix_fs_desc *ptr)
{
	ptr->used = false;
}

/**
 * @brief Open a file.
 *
 * See IEEE 1003.1
 */
int open(const char *name, int flags)
{
	int rc, fd;
	struct posix_fs_desc *ptr = NULL;

	ARG_UNUSED(flags);

	fd = z_reserve_fd();
	if (fd < 0) {
		return -1;
	}

	ptr = posix_fs_alloc_obj(false);
	if (ptr == NULL) {
		z_free_fd(fd);
		errno = EMFILE;
		return -1;
	}

	(void)memset(&ptr->file, 0, sizeof(ptr->file));

	rc = fs_open(&ptr->file, name);
	if (rc < 0) {
		posix_fs_free_obj(ptr);
		z_free_fd(fd);
		errno = -rc;
		return -1;
	}

	z_finalize_fd(fd, ptr, &fs_fd_op_vtable);

	return fd;
}

static int fs_ioctl_vmeth(void *obj, unsigned int request, va_list args)
{
	int rc;
	struct posix_fs_desc *ptr = obj;

	switch (request) {
	case ZFD_IOCTL_CLOSE:
		rc = fs_close(&ptr->file);
		break;

	case ZFD_IOCTL_LSEEK: {
		off_t offset;
		int whence;

		offset = va_arg(args, off_t);
		whence = va_arg(args, int);

		rc = fs_seek(&ptr->file, offset, whence);
		break;
	}

	default:
		errno = EOPNOTSUPP;
		return -1;
	}

	if (rc < 0) {
		errno = -rc;
		return -1;
	}

	return 0;
}

/**
 * @brief Write to a file.
 *
 * See IEEE 1003.1
 */
static ssize_t fs_write_vmeth(void *obj, const void *buffer, size_t count)
{
	ssize_t rc;
	struct posix_fs_desc *ptr = obj;

	rc = fs_write(&ptr->file, buffer, count);
	if (rc < 0) {
		errno = -rc;
		return -1;
	}

	return rc;
}

/**
 * @brief Read from a file.
 *
 * See IEEE 1003.1
 */
static ssize_t fs_read_vmeth(void *obj, void *buffer, size_t count)
{
	ssize_t rc;
	struct posix_fs_desc *ptr = obj;

	rc = fs_read(&ptr->file, buffer, count);
	if (rc < 0) {
		errno = -rc;
		return -1;
	}

	return rc;
}

static struct fd_op_vtable fs_fd_op_vtable = {
	.read = fs_read_vmeth,
	.write = fs_write_vmeth,
	.ioctl = fs_ioctl_vmeth,
};

/**
 * @brief Open a directory stream.
 *
 * See IEEE 1003.1
 */
DIR *opendir(const char *dirname)
{
	int rc;
	struct posix_fs_desc *ptr;

	ptr = posix_fs_alloc_obj(true);
	if (ptr == NULL) {
		errno = EMFILE;
		return NULL;
	}

	(void)memset(&ptr->dir, 0, sizeof(ptr->dir));

	rc = fs_opendir(&ptr->dir, dirname);
	if (rc < 0) {
		posix_fs_free_obj(ptr);
		errno = -rc;
		return NULL;
	}

	return ptr;
}

/**
 * @brief Close a directory stream.
 *
 * See IEEE 1003.1
 */
int closedir(DIR *dirp)
{
	int rc;
	struct posix_fs_desc *ptr = dirp;

	if (dirp == NULL) {
		errno = EBADF;
		return -1;
	}

	rc = fs_closedir(&ptr->dir);

	posix_fs_free_obj(ptr);

	if (rc < 0) {
		errno = -rc;
		return -1;
	}

	return 0;
}

/**
 * @brief Read a directory.
 *
 * See IEEE 1003.1
 */
struct dirent *readdir(DIR *dirp)
{
	int rc;
	struct posix_fs_desc *ptr = dirp;

	if (dirp == NULL) {
		errno = EBADF;
		return NULL;
	}

	rc = fs_readdir(&ptr->dir, &fdirent);
	if (rc < 0) {
		errno = -rc;
		return NULL;
	}

	rc = strlen(fdirent.name);
	rc = (rc < PATH_MAX) ? rc : (PATH_MAX - 1);
	(void)memcpy(pdirent.d_name, fdirent.name, rc);

	/* Make sure the name is NULL terminated */
	pdirent.d_name[rc] = '\0';
	return &pdirent;
}

/**
 * @brief Rename a file.
 *
 * See IEEE 1003.1
 */
int rename(const char *old, const char *new)
{
	int rc;

	rc = fs_rename(old, new);
	if (rc < 0) {
		errno = -rc;
		return -1;
	}

	return 0;
}

/**
 * @brief Remove a directory entry.
 *
 * See IEEE 1003.1
 */
int unlink(const char *path)
{
	int rc;

	rc = fs_unlink(path);
	if (rc < 0) {
		errno = -rc;
		return -1;
	}
	return 0;
}

/**
 * @brief Get file status.
 *
 * See IEEE 1003.1
 */
int stat(const char *path, struct stat *buf)
{
	int rc;
	struct fs_statvfs stat;

	if (buf == NULL) {
		errno = EBADF;
		return -1;
	}

	rc = fs_statvfs(path, &stat);
	if (rc < 0) {
		errno = -rc;
		return -1;
	}

	buf->st_size = stat.f_bsize * stat.f_blocks;
	buf->st_blksize = stat.f_bsize;
	buf->st_blocks = stat.f_blocks;
	return 0;
}

/**
 * @brief Make a directory.
 *
 * See IEEE 1003.1
 */
int mkdir(const char *path, mode_t mode)
{
	int rc;

	ARG_UNUSED(mode);

	rc = fs_mkdir(path);
	if (rc < 0) {
		errno = -rc;
		return -1;
	}

	return 0;
}
