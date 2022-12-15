/*
 * Copyright (c) 2018-2021 mcumgr authors
 * Copyright (c) 2022 Laird Connectivity
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/sys/util.h>
#include <zephyr/fs/fs.h>
#include <zephyr/mgmt/mcumgr/mgmt/mgmt.h>
#include <zephyr/mgmt/mcumgr/smp/smp.h>
#include <zephyr/mgmt/mcumgr/grp/fs_mgmt/fs_mgmt.h>
#include <zephyr/mgmt/mcumgr/grp/fs_mgmt/fs_mgmt_hash_checksum.h>
#include <assert.h>
#include <limits.h>
#include <string.h>

#include <zcbor_common.h>
#include <zcbor_decode.h>
#include <zcbor_encode.h>

#include <mgmt/mcumgr/util/zcbor_bulk.h>
#include <mgmt/mcumgr/grp/fs_mgmt/fs_mgmt_config.h>

#if defined(CONFIG_FS_MGMT_CHECKSUM_IEEE_CRC32)
#include <mgmt/mcumgr/grp/fs_mgmt/fs_mgmt_hash_checksum_crc32.h>
#endif

#if defined(CONFIG_FS_MGMT_HASH_SHA256)
#include <mgmt/mcumgr/grp/fs_mgmt/fs_mgmt_hash_checksum_sha256.h>
#endif

#if defined(CONFIG_MCUMGR_MGMT_NOTIFICATION_HOOKS)
#include <zephyr/mgmt/mcumgr/mgmt/callbacks.h>
#endif

#ifdef CONFIG_FS_MGMT_CHECKSUM_HASH
/* Define default hash/checksum */
#if defined(CONFIG_FS_MGMT_CHECKSUM_IEEE_CRC32)
#define FS_MGMT_CHECKSUM_HASH_DEFAULT "crc32"
#elif defined(CONFIG_FS_MGMT_HASH_SHA256)
#define FS_MGMT_CHECKSUM_HASH_DEFAULT "sha256"
#else
#error "Missing mcumgr fs checksum/hash algorithm selection?"
#endif

/* Define largest hach/checksum output size (bytes) */
#if defined(CONFIG_FS_MGMT_HASH_SHA256)
#define FS_MGMT_CHECKSUM_HASH_LARGEST_OUTPUT_SIZE 32
#elif defined(CONFIG_FS_MGMT_CHECKSUM_IEEE_CRC32)
#define FS_MGMT_CHECKSUM_HASH_LARGEST_OUTPUT_SIZE 4
#endif
#endif

#define LOG_LEVEL CONFIG_MCUMGR_LOG_LEVEL
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(fs_mgmt);

#define HASH_CHECKSUM_TYPE_SIZE 8

#define HASH_CHECKSUM_SUPPORTED_COLUMNS_MAX 4

static struct {
	/** Whether an upload is currently in progress. */
	bool uploading;

	/** Expected offset of next upload request. */
	size_t off;

	/** Total length of file currently being uploaded. */
	size_t len;
} fs_mgmt_ctxt;

static const struct mgmt_handler fs_mgmt_handlers[];

#if defined(CONFIG_FS_MGMT_CHECKSUM_HASH)
/* Hash/checksum iterator information passing structure */
struct fs_mgmt_hash_checksum_iterator_info {
	zcbor_state_t *zse;
	bool ok;
};
#endif

static int fs_mgmt_filelen(const char *path, size_t *out_len)
{
	struct fs_dirent dirent;
	int rc;

	rc = fs_stat(path, &dirent);

	if (rc == -EINVAL) {
		return MGMT_ERR_EINVAL;
	} else if (rc == -ENOENT) {
		return MGMT_ERR_ENOENT;
	} else if (rc != 0) {
		return MGMT_ERR_EUNKNOWN;
	}

	if (dirent.type != FS_DIR_ENTRY_FILE) {
		return MGMT_ERR_EUNKNOWN;
	}

	*out_len = dirent.size;

	return 0;
}

/**
 * Encodes a file upload response.
 */
static bool fs_mgmt_file_rsp(zcbor_state_t *zse, int rc, uint64_t off)
{
	bool ok;

	ok = zcbor_tstr_put_lit(zse, "rc")	&&
	     zcbor_int32_put(zse, rc)		&&
	     zcbor_tstr_put_lit(zse, "off")	&&
	     zcbor_uint64_put(zse, off);

	return ok;
}

static int fs_mgmt_read(const char *path, size_t offset, size_t len,
			void *out_data, size_t *out_len)
{
	struct fs_file_t file;
	ssize_t bytes_read;
	int rc;

	fs_file_t_init(&file);
	rc = fs_open(&file, path, FS_O_READ);
	if (rc != 0) {
		return MGMT_ERR_ENOENT;
	}

	rc = fs_seek(&file, offset, FS_SEEK_SET);
	if (rc != 0) {
		goto done;
	}

	bytes_read = fs_read(&file, out_data, len);
	if (bytes_read < 0) {
		goto done;
	}

	*out_len = bytes_read;

done:
	fs_close(&file);

	if (rc < 0) {
		return MGMT_ERR_EUNKNOWN;
	}

	return 0;
}

/**
 * Command handler: fs file (read)
 */
static int fs_mgmt_file_download(struct smp_streamer *ctxt)
{
	uint8_t file_data[FS_MGMT_DL_CHUNK_SIZE];
	char path[CONFIG_FS_MGMT_PATH_SIZE + 1];
	uint64_t off = ULLONG_MAX;
	size_t bytes_read = 0;
	size_t file_len;
	int rc;
	zcbor_state_t *zse = ctxt->writer->zs;
	zcbor_state_t *zsd = ctxt->reader->zs;
	bool ok;
	struct zcbor_string name = { 0 };
	size_t decoded;

	struct zcbor_map_decode_key_val fs_download_decode[] = {
		ZCBOR_MAP_DECODE_KEY_VAL(off, zcbor_uint64_decode, &off),
		ZCBOR_MAP_DECODE_KEY_VAL(name, zcbor_tstr_decode, &name),
	};

#if defined(CONFIG_MCUMGR_GRP_FS_FILE_ACCESS_HOOK)
	struct fs_mgmt_file_access file_access_data = {
		.upload = false,
		.filename = path,
	};
#endif

	ok = zcbor_map_decode_bulk(zsd, fs_download_decode,
		ARRAY_SIZE(fs_download_decode), &decoded) == 0;

	if (!ok || name.len == 0 || name.len > (sizeof(path) - 1)) {
		return MGMT_ERR_EINVAL;
	}

	memcpy(path, name.value, name.len);
	path[name.len] = '\0';

#if defined(CONFIG_MCUMGR_GRP_FS_FILE_ACCESS_HOOK)
	/* Send request to application to check if access should be allowed or not */
	rc = mgmt_callback_notify(MGMT_EVT_OP_FS_MGMT_FILE_ACCESS, &file_access_data,
				  sizeof(file_access_data));

	if (rc != MGMT_ERR_EOK) {
		return rc;
	}
#endif

	/* Only the response to the first download request contains the total file
	 * length.
	 */
	if (off == 0) {
		rc = fs_mgmt_filelen(path, &file_len);
		if (rc != 0) {
			return rc;
		}
	}

	/* Read the requested chunk from the file. */
	rc = fs_mgmt_read(path, off, FS_MGMT_DL_CHUNK_SIZE, file_data, &bytes_read);
	if (rc != 0) {
		return rc;
	}

	/* Encode the response. */
	ok = fs_mgmt_file_rsp(zse, MGMT_ERR_EOK, off)				&&
	     zcbor_tstr_put_lit(zse, "data")					&&
	     zcbor_bstr_encode_ptr(zse, file_data, bytes_read)			&&
	     ((off != 0)							||
		(zcbor_tstr_put_lit(zse, "len") && zcbor_uint64_put(zse, file_len)));

	return ok ? MGMT_ERR_EOK : MGMT_ERR_EMSGSIZE;
}

static int fs_mgmt_write(const char *path, size_t offset,
			 const void *data, size_t len)
{
	struct fs_file_t file;
	int rc;
	size_t file_size = 0;

	if (offset == 0) {
		rc = fs_mgmt_filelen(path, &file_size);
	}

	fs_file_t_init(&file);
	rc = fs_open(&file, path, FS_O_CREATE | FS_O_WRITE);
	if (rc != 0) {
		return MGMT_ERR_EUNKNOWN;
	}

	if (offset == 0 && file_size > 0) {
		/* Offset is 0 and existing file exists with data, attempt to truncate the file
		 * size to 0
		 */
		rc = fs_truncate(&file, 0);

		if (rc == -ENOTSUP) {
			/* Truncation not supported by filesystem, therefore close file, delete
			 * it then re-open it
			 */
			fs_close(&file);

			rc = fs_unlink(path);
			if (rc < 0 && rc != -ENOENT) {
				return rc;
			}

			rc = fs_open(&file, path, FS_O_CREATE | FS_O_WRITE);
		}

		if (rc < 0) {
			/* Failed to truncate file */
			return MGMT_ERR_EUNKNOWN;
		}
	} else if (offset > 0) {
		rc = fs_seek(&file, offset, FS_SEEK_SET);
		if (rc != 0) {
			goto done;
		}
	}

	rc = fs_write(&file, data, len);
	if (rc < 0) {
		goto done;
	}

done:
	fs_close(&file);

	if (rc < 0) {
		return MGMT_ERR_EUNKNOWN;
	}

	return 0;
}

/**
 * Command handler: fs file (write)
 */
static int fs_mgmt_file_upload(struct smp_streamer *ctxt)
{
	char file_name[CONFIG_FS_MGMT_PATH_SIZE + 1];
	unsigned long long len = ULLONG_MAX;
	unsigned long long off = ULLONG_MAX;
	size_t new_off;
	bool ok;
	int rc;
	zcbor_state_t *zse = ctxt->writer->zs;
	zcbor_state_t *zsd = ctxt->reader->zs;
	struct zcbor_string name = { 0 };
	struct zcbor_string file_data = { 0 };
	size_t decoded = 0;

	struct zcbor_map_decode_key_val fs_upload_decode[] = {
		ZCBOR_MAP_DECODE_KEY_VAL(off, zcbor_uint64_decode, &off),
		ZCBOR_MAP_DECODE_KEY_VAL(name, zcbor_tstr_decode, &name),
		ZCBOR_MAP_DECODE_KEY_VAL(data, zcbor_bstr_decode, &file_data),
		ZCBOR_MAP_DECODE_KEY_VAL(len, zcbor_uint64_decode, &len),
	};

#if defined(CONFIG_MCUMGR_GRP_FS_FILE_ACCESS_HOOK)
	struct fs_mgmt_file_access file_access_data = {
		.upload = false,
		.filename = file_name,
	};
#endif

	ok = zcbor_map_decode_bulk(zsd, fs_upload_decode,
		ARRAY_SIZE(fs_upload_decode), &decoded) == 0;

	if (!ok || off == ULLONG_MAX || name.len == 0 ||
	    name.len > (sizeof(file_name) - 1)) {
		return MGMT_ERR_EINVAL;
	}

	memcpy(file_name, name.value, name.len);
	file_name[name.len] = '\0';

#if defined(CONFIG_MCUMGR_GRP_FS_FILE_ACCESS_HOOK)
	/* Send request to application to check if access should be allowed or not */
	rc = mgmt_callback_notify(MGMT_EVT_OP_FS_MGMT_FILE_ACCESS, &file_access_data,
				  sizeof(file_access_data));

	if (rc != MGMT_ERR_EOK) {
		return rc;
	}
#endif

	if (off == 0) {
		/* Total file length is a required field in the first chunk request. */
		if (len == ULLONG_MAX) {
			return MGMT_ERR_EINVAL;
		}

		fs_mgmt_ctxt.uploading = true;
		fs_mgmt_ctxt.off = 0;
		fs_mgmt_ctxt.len = len;
	} else {
		if (!fs_mgmt_ctxt.uploading) {
			return MGMT_ERR_EINVAL;
		}

		if (off != fs_mgmt_ctxt.off) {
			/* Invalid offset.  Drop the data and send the expected offset. */
			return fs_mgmt_file_rsp(zse, MGMT_ERR_EINVAL, fs_mgmt_ctxt.off);
		}
	}

	new_off = fs_mgmt_ctxt.off + file_data.len;
	if (new_off > fs_mgmt_ctxt.len) {
		/* Data exceeds image length. */
		return MGMT_ERR_EINVAL;
	}

	if (file_data.len > 0) {
		/* Write the data chunk to the file. */
		rc = fs_mgmt_write(file_name, off, file_data.value, file_data.len);
		if (rc != 0) {
			return rc;
		}
		fs_mgmt_ctxt.off = new_off;
	}

	if (fs_mgmt_ctxt.off == fs_mgmt_ctxt.len) {
		/* Upload complete. */
		fs_mgmt_ctxt.uploading = false;
	}

	/* Send the response. */
	return fs_mgmt_file_rsp(zse, MGMT_ERR_EOK, fs_mgmt_ctxt.off) ?
			MGMT_ERR_EOK : MGMT_ERR_EMSGSIZE;
}

#if defined(CONFIG_FS_MGMT_FILE_STATUS)
/**
 * Command handler: fs stat (read)
 */
static int fs_mgmt_file_status(struct smp_streamer *ctxt)
{
	char path[CONFIG_FS_MGMT_PATH_SIZE + 1];
	size_t file_len;
	int rc;
	zcbor_state_t *zse = ctxt->writer->zs;
	zcbor_state_t *zsd = ctxt->reader->zs;
	bool ok;
	struct zcbor_string name = { 0 };
	size_t decoded;

	struct zcbor_map_decode_key_val fs_status_decode[] = {
		ZCBOR_MAP_DECODE_KEY_VAL(name, zcbor_tstr_decode, &name),
	};

	ok = zcbor_map_decode_bulk(zsd, fs_status_decode,
		ARRAY_SIZE(fs_status_decode), &decoded) == 0;

	if (!ok || name.len == 0 || name.len > (sizeof(path) - 1)) {
		return MGMT_ERR_EINVAL;
	}

	/* Copy path and ensure it is null-teminated */
	memcpy(path, name.value, name.len);
	path[name.len] = '\0';

	/* Retrieve file size */
	rc = fs_mgmt_filelen(path, &file_len);

	/* Encode the response. */
	if (rc == 0) {
		/* No error, only encode file status (length) */
		ok = zcbor_tstr_put_lit(zse, "len")	&&
		     zcbor_uint64_put(zse, file_len);
	} else {
		/* Error, only encode error result code */
		ok = zcbor_tstr_put_lit(zse, "rc")	&&
		     zcbor_int32_put(zse, rc);
	}

	if (!ok) {
		return MGMT_ERR_EMSGSIZE;
	}

	return MGMT_ERR_EOK;
}
#endif

#if defined(CONFIG_FS_MGMT_CHECKSUM_HASH)
/**
 * Command handler: fs hash/checksum (read)
 */
static int fs_mgmt_file_hash_checksum(struct smp_streamer *ctxt)
{
	char path[CONFIG_FS_MGMT_PATH_SIZE + 1];
	char type_arr[HASH_CHECKSUM_TYPE_SIZE + 1] = FS_MGMT_CHECKSUM_HASH_DEFAULT;
	char output[FS_MGMT_CHECKSUM_HASH_LARGEST_OUTPUT_SIZE];
	uint64_t len = ULLONG_MAX;
	uint64_t off = 0;
	size_t file_len;
	int rc;
	zcbor_state_t *zse = ctxt->writer->zs;
	zcbor_state_t *zsd = ctxt->reader->zs;
	bool ok;
	struct zcbor_string type = { 0 };
	struct zcbor_string name = { 0 };
	size_t decoded;
	struct fs_file_t file;
	const struct fs_mgmt_hash_checksum_group *group = NULL;

	struct zcbor_map_decode_key_val fs_hash_checksum_decode[] = {
		ZCBOR_MAP_DECODE_KEY_VAL(type, zcbor_tstr_decode, &type),
		ZCBOR_MAP_DECODE_KEY_VAL(name, zcbor_tstr_decode, &name),
		ZCBOR_MAP_DECODE_KEY_VAL(off, zcbor_uint64_decode, &off),
		ZCBOR_MAP_DECODE_KEY_VAL(len, zcbor_uint64_decode, &len),
	};

	ok = zcbor_map_decode_bulk(zsd, fs_hash_checksum_decode,
		ARRAY_SIZE(fs_hash_checksum_decode), &decoded) == 0;

	if (!ok || name.len == 0 || name.len > (sizeof(path) - 1) ||
	    type.len > (sizeof(type_arr) - 1) || len == 0) {
		return MGMT_ERR_EINVAL;
	}

	/* Copy strings and ensure they are null-teminated */
	memcpy(path, name.value, name.len);
	path[name.len] = '\0';

	if (type.len != 0) {
		memcpy(type_arr, type.value, type.len);
		type_arr[type.len] = '\0';
	}

	/* Search for supported hash/checksum */
	group = fs_mgmt_hash_checksum_find_handler(type_arr);

	if (group == NULL) {
		return MGMT_ERR_EINVAL;
	}

	/* Check provided offset is valid for target file */
	rc = fs_mgmt_filelen(path, &file_len);

	if (rc != 0) {
		return MGMT_ERR_ENOENT;
	}

	if (file_len <= off) {
		/* Requested offset is larger than target file size */
		return MGMT_ERR_EINVAL;
	}

	/* Open file for reading and pass to hash/checksum generation function */
	fs_file_t_init(&file);
	rc = fs_open(&file, path, FS_O_READ);

	if (rc != 0) {
		return MGMT_ERR_ENOENT;
	}

	/* Seek to file's desired offset, if parameter was provided */
	if (off != 0) {
		rc = fs_seek(&file, off, FS_SEEK_SET);

		if (rc != 0) {
			fs_close(&file);
			return MGMT_ERR_EINVAL;
		}
	}

	/* Calculate hash/checksum using function */
	file_len = 0;
	rc = group->function(&file, output, &file_len, len);

	fs_close(&file);

	/* Encode the response */
	if (rc != 0) {
		ok = zcbor_tstr_put_lit(zse, "rc")	&&
		     zcbor_int32_put(zse, rc);
	} else {
		ok = zcbor_tstr_put_lit(zse, "type")	&&
		     zcbor_tstr_put_term(zse, type_arr);

		if (off != 0) {
			ok &= zcbor_tstr_put_lit(zse, "off")	&&
			      zcbor_uint64_put(zse, off);
		}

		ok &= zcbor_tstr_put_lit(zse, "len")	&&
		      zcbor_uint64_put(zse, file_len)	&&
		      zcbor_tstr_put_lit(zse, "output");

		if (group->byte_string == true) {
			/* Output is a byte string */
			ok &= zcbor_bstr_encode_ptr(zse, output, group->output_size);
		} else {
			/* Output is a number */
			uint64_t tmp_val = 0;

			if (group->output_size == sizeof(uint8_t)) {
				tmp_val = (uint64_t)(*(uint8_t *)output);
#if FS_MGMT_CHECKSUM_HASH_LARGEST_OUTPUT_SIZE > 1
			} else if (group->output_size == sizeof(uint16_t)) {
				tmp_val = (uint64_t)(*(uint16_t *)output);
#if FS_MGMT_CHECKSUM_HASH_LARGEST_OUTPUT_SIZE > 2
			} else if (group->output_size == sizeof(uint32_t)) {
				tmp_val = (uint64_t)(*(uint32_t *)output);
#if FS_MGMT_CHECKSUM_HASH_LARGEST_OUTPUT_SIZE > 4
			} else if (group->output_size == sizeof(uint64_t)) {
				tmp_val = (*(uint64_t *)output);
#endif
#endif
#endif
			} else {
				LOG_ERR("Unable to handle numerical checksum size %u",
					group->output_size);

				return MGMT_ERR_EUNKNOWN;
			}

			ok &= zcbor_uint64_put(zse, tmp_val);
		}
	}

	if (!ok) {
		return MGMT_ERR_EMSGSIZE;
	}

	return MGMT_ERR_EOK;
}

#if defined(CONFIG_MCUMGR_GRP_FS_CHECKSUM_HASH_SUPPORTED_CMD)
/* Callback for supported hash/checksum types to encode details on one type into CBOR map */
static void fs_mgmt_supported_hash_checksum_callback(
					const struct fs_mgmt_hash_checksum_group *group,
					void *user_data)
{
	struct fs_mgmt_hash_checksum_iterator_info *ctx =
			(struct fs_mgmt_hash_checksum_iterator_info *)user_data;

	if (!ctx->ok) {
		return;
	}

	ctx->ok = zcbor_tstr_encode_ptr(ctx->zse, group->group_name, strlen(group->group_name))	&&
		  zcbor_map_start_encode(ctx->zse, HASH_CHECKSUM_SUPPORTED_COLUMNS_MAX)		&&
		  zcbor_tstr_put_lit(ctx->zse, "format")					&&
		  zcbor_uint32_put(ctx->zse, (uint32_t)group->byte_string)			&&
		  zcbor_tstr_put_lit(ctx->zse, "size")						&&
		  zcbor_uint32_put(ctx->zse, (uint32_t)group->output_size)			&&
		  zcbor_map_end_encode(ctx->zse, HASH_CHECKSUM_SUPPORTED_COLUMNS_MAX);
}

/**
 * Command handler: fs supported hash/checksum (read)
 */
static int
fs_mgmt_supported_hash_checksum(struct smp_streamer *ctxt)
{
	zcbor_state_t *zse = ctxt->writer->zs;
	struct fs_mgmt_hash_checksum_iterator_info itr_ctx = {
		.zse = zse,
	};

	itr_ctx.ok = zcbor_tstr_put_lit(zse, "types");

	zcbor_map_start_encode(zse, CONFIG_MCUMGR_GRP_FS_CHECKSUM_HASH_SUPPORTED_MAX_TYPES);

	fs_mgmt_hash_checksum_find_handlers(fs_mgmt_supported_hash_checksum_callback, &itr_ctx);

	if (!itr_ctx.ok ||
	    !zcbor_map_end_encode(zse, CONFIG_MCUMGR_GRP_FS_CHECKSUM_HASH_SUPPORTED_MAX_TYPES)) {
		return MGMT_ERR_EMSGSIZE;
	}

	return MGMT_ERR_EOK;
}
#endif
#endif

static const struct mgmt_handler fs_mgmt_handlers[] = {
	[FS_MGMT_ID_FILE] = {
		.mh_read = fs_mgmt_file_download,
		.mh_write = fs_mgmt_file_upload,
	},
#if defined(CONFIG_FS_MGMT_FILE_STATUS)
	[FS_MGMT_ID_STAT] = {
		.mh_read = fs_mgmt_file_status,
		.mh_write = NULL,
	},
#endif
#if defined(CONFIG_FS_MGMT_CHECKSUM_HASH)
	[FS_MGMT_ID_HASH_CHECKSUM] = {
		.mh_read = fs_mgmt_file_hash_checksum,
		.mh_write = NULL,
	},
#if defined(CONFIG_MCUMGR_GRP_FS_CHECKSUM_HASH_SUPPORTED_CMD)
	[FS_MGMT_ID_SUPPORTED_HASH_CHECKSUM] = {
		.mh_read = fs_mgmt_supported_hash_checksum,
		.mh_write = NULL,
	},
#endif
#endif
};

#define FS_MGMT_HANDLER_CNT ARRAY_SIZE(fs_mgmt_handlers)

static struct mgmt_group fs_mgmt_group = {
	.mg_handlers = fs_mgmt_handlers,
	.mg_handlers_count = FS_MGMT_HANDLER_CNT,
	.mg_group_id = MGMT_GROUP_ID_FS,
};

void fs_mgmt_register_group(void)
{
	mgmt_register_group(&fs_mgmt_group);

#if defined(CONFIG_FS_MGMT_CHECKSUM_HASH)
	/* Register any supported hash or checksum functions */
#if defined(CONFIG_FS_MGMT_CHECKSUM_IEEE_CRC32)
	fs_mgmt_hash_checksum_register_crc32();
#endif

#if defined(CONFIG_FS_MGMT_HASH_SHA256)
	fs_mgmt_hash_checksum_register_sha256();
#endif
#endif
}
