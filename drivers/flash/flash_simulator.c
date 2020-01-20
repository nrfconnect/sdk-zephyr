/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <device.h>
#include <drivers/flash.h>
#include <init.h>
#include <kernel.h>
#include <sys/util.h>
#include <random/rand32.h>
#include <stats/stats.h>
#include <string.h>

#ifdef CONFIG_ARCH_POSIX

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <errno.h>

#include "cmdline.h"
#include "soc.h"

#endif /* CONFIG_ARCH_POSIX */

/* configuration derived from DT */
#ifdef CONFIG_ARCH_POSIX
#define FLASH_SIMULATOR_BASE_OFFSET DT_FLASH_BASE_ADDRESS
#define FLASH_SIMULATOR_ERASE_UNIT DT_FLASH_ERASE_BLOCK_SIZE
#define FLASH_SIMULATOR_PROG_UNIT DT_FLASH_WRITE_BLOCK_SIZE
#define FLASH_SIMULATOR_FLASH_SIZE (DT_FLASH_SIZE * 1024)
#define FLASH_SIMULATOR_DEV_NAME DT_FLASH_DEV_NAME
#else
#define FLASH_SIMULATOR_BASE_OFFSET DT_FLASH_SIM_BASE_ADDRESS
#define FLASH_SIMULATOR_ERASE_UNIT DT_FLASH_SIM_ERASE_BLOCK_SIZE
#define FLASH_SIMULATOR_PROG_UNIT DT_FLASH_SIM_WRITE_BLOCK_SIZE
#define FLASH_SIMULATOR_FLASH_SIZE DT_FLASH_SIM_SIZE
#define FLASH_SIMULATOR_DEV_NAME "FLASH_SIMULATOR"
#endif /* CONFIG_ARCH_POSIX */

#define FLASH_SIMULATOR_PAGE_COUNT (FLASH_SIMULATOR_FLASH_SIZE / \
				    FLASH_SIMULATOR_ERASE_UNIT)

#if (FLASH_SIMULATOR_ERASE_UNIT % FLASH_SIMULATOR_PROG_UNIT)
#error "Erase unit must be a multiple of program unit"
#endif

#define FLASH(addr) (mock_flash + (addr) - FLASH_SIMULATOR_BASE_OFFSET)

/* maximum number of pages that can be tracked by the stats module */
#define STATS_PAGE_COUNT_THRESHOLD 256

#define STATS_SECT_EC(N, _) STATS_SECT_ENTRY32(erase_cycles_unit##N)
#define STATS_NAME_EC(N, _) STATS_NAME(flash_sim_stats, erase_cycles_unit##N)

#define STATS_SECT_DIRTYR(N, _) STATS_SECT_ENTRY32(dirty_read_unit##N)
#define STATS_NAME_DIRTYR(N, _) STATS_NAME(flash_sim_stats, dirty_read_unit##N)

/* increment a unit erase cycles counter */
#define ERASE_CYCLES_INC(U)						     \
	do {								     \
		if (U < STATS_PAGE_COUNT_THRESHOLD) {			     \
			(*(&flash_sim_stats.erase_cycles_unit0 + (U)) += 1); \
		}							     \
	} while (0)

#if (defined(CONFIG_STATS) && \
     (CONFIG_FLASH_SIMULATOR_STAT_PAGE_COUNT > STATS_PAGE_COUNT_THRESHOLD))
       /* Limitation above is caused by used UTIL_REPEAT                    */
       /* Using FLASH_SIMULATOR_FLASH_PAGE_COUNT allows to avoid terrible   */
       /* error logg at the output and work with the stats module partially */
       #define FLASH_SIMULATOR_FLASH_PAGE_COUNT STATS_PAGE_COUNT_THRESHOLD
#else
#define FLASH_SIMULATOR_FLASH_PAGE_COUNT CONFIG_FLASH_SIMULATOR_STAT_PAGE_COUNT
#endif

/* simulator statistcs */
STATS_SECT_START(flash_sim_stats)
STATS_SECT_ENTRY32(bytes_read)		/* total bytes read */
STATS_SECT_ENTRY32(bytes_written)       /* total bytes written */
STATS_SECT_ENTRY32(double_writes)       /* num. of writes to non-erased units */
STATS_SECT_ENTRY32(flash_read_calls)    /* calls to flash_read() */
STATS_SECT_ENTRY32(flash_read_time_us)  /* time spent in flash_read() */
STATS_SECT_ENTRY32(flash_write_calls)   /* calls to flash_write() */
STATS_SECT_ENTRY32(flash_write_time_us) /* time spent in flash_write() */
STATS_SECT_ENTRY32(flash_erase_calls)   /* calls to flash_erase() */
STATS_SECT_ENTRY32(flash_erase_time_us) /* time spent in flash_erase() */
/* -- per-unit statistics -- */
/* erase cycle count for unit */
UTIL_EVAL(UTIL_REPEAT(FLASH_SIMULATOR_FLASH_PAGE_COUNT, STATS_SECT_EC))
/* number of read operations on worn out erase units */
UTIL_EVAL(UTIL_REPEAT(FLASH_SIMULATOR_FLASH_PAGE_COUNT, STATS_SECT_DIRTYR))
STATS_SECT_END;

STATS_SECT_DECL(flash_sim_stats) flash_sim_stats;
STATS_NAME_START(flash_sim_stats)
STATS_NAME(flash_sim_stats, bytes_read)
STATS_NAME(flash_sim_stats, bytes_written)
STATS_NAME(flash_sim_stats, double_writes)
STATS_NAME(flash_sim_stats, flash_read_calls)
STATS_NAME(flash_sim_stats, flash_read_time_us)
STATS_NAME(flash_sim_stats, flash_write_calls)
STATS_NAME(flash_sim_stats, flash_write_time_us)
STATS_NAME(flash_sim_stats, flash_erase_calls)
STATS_NAME(flash_sim_stats, flash_erase_time_us)
UTIL_EVAL(UTIL_REPEAT(FLASH_SIMULATOR_FLASH_PAGE_COUNT, STATS_NAME_EC))
UTIL_EVAL(UTIL_REPEAT(FLASH_SIMULATOR_FLASH_PAGE_COUNT, STATS_NAME_DIRTYR))
STATS_NAME_END(flash_sim_stats);

/* simulator dynamic thresholds */
STATS_SECT_START(flash_sim_thresholds)
STATS_SECT_ENTRY32(max_write_calls)
STATS_SECT_ENTRY32(max_erase_calls)
STATS_SECT_ENTRY32(max_len)
STATS_SECT_END;

STATS_SECT_DECL(flash_sim_thresholds) flash_sim_thresholds;
STATS_NAME_START(flash_sim_thresholds)
STATS_NAME(flash_sim_thresholds, max_write_calls)
STATS_NAME(flash_sim_thresholds, max_erase_calls)
STATS_NAME(flash_sim_thresholds, max_len)
STATS_NAME_END(flash_sim_thresholds);

#ifdef CONFIG_ARCH_POSIX
static u8_t *mock_flash;
static int flash_fd = -1;
static const char *flash_file_path;
static const char default_flash_file_path[] = "flash.bin";
#else
static u8_t mock_flash[FLASH_SIMULATOR_FLASH_SIZE];
#endif /* CONFIG_ARCH_POSIX */

static bool write_protection;

static const struct flash_driver_api flash_sim_api;

static int flash_range_is_valid(struct device *dev, off_t offset, size_t len)
{
	ARG_UNUSED(dev);
	if ((offset + len > FLASH_SIMULATOR_FLASH_SIZE +
			    FLASH_SIMULATOR_BASE_OFFSET) ||
	    (offset < FLASH_SIMULATOR_BASE_OFFSET)) {
		return 0;
	}

	return 1;
}

static int flash_wp_set(struct device *dev, bool enable)
{
	ARG_UNUSED(dev);
	write_protection = enable;

	return 0;
}

static bool flash_wp_is_set(void)
{
	return write_protection;
}

static int flash_sim_read(struct device *dev, const off_t offset, void *data,
			  const size_t len)
{
	ARG_UNUSED(dev);

	if (!flash_range_is_valid(dev, offset, len)) {
		return -EINVAL;
	}

	if (!IS_ENABLED(CONFIG_FLASH_SIMULATOR_UNALIGNED_READ)) {
		if ((offset % FLASH_SIMULATOR_PROG_UNIT) ||
		    (len % FLASH_SIMULATOR_PROG_UNIT)) {
			return -EINVAL;
		}
	}

	STATS_INC(flash_sim_stats, flash_read_calls);

	memcpy(data, FLASH(offset), len);
	STATS_INCN(flash_sim_stats, bytes_read, len);

#ifdef CONFIG_FLASH_SIMULATOR_SIMULATE_TIMING
	k_busy_wait(CONFIG_FLASH_SIMULATOR_MIN_READ_TIME_US);
	STATS_INCN(flash_sim_stats, flash_read_time_us,
		   CONFIG_FLASH_SIMULATOR_MIN_READ_TIME_US);
#endif

	return 0;
}

static int flash_sim_write(struct device *dev, const off_t offset,
			   const void *data, const size_t len)
{
	ARG_UNUSED(dev);

	if (!flash_range_is_valid(dev, offset, len)) {
		return -EINVAL;
	}

	if ((offset % FLASH_SIMULATOR_PROG_UNIT) ||
	    (len % FLASH_SIMULATOR_PROG_UNIT)) {
		return -EINVAL;
	}

	if (flash_wp_is_set()) {
		return -EACCES;
	}

	STATS_INC(flash_sim_stats, flash_write_calls);

	/* check if any unit has been already programmed */
	for (u32_t i = 0; i < len; i += FLASH_SIMULATOR_PROG_UNIT) {

		u8_t buf[FLASH_SIMULATOR_PROG_UNIT];

		memset(buf, 0xFF, sizeof(buf));
		if (memcmp(buf, FLASH(offset + i), sizeof(buf))) {
			STATS_INC(flash_sim_stats, double_writes);
#if !CONFIG_FLASH_SIMULATOR_DOUBLE_WRITES
			return -EIO;
#endif
		}
	}

	bool data_part_ignored = false;

	if (flash_sim_thresholds.max_write_calls != 0) {
		if (flash_sim_stats.flash_write_calls >
			flash_sim_thresholds.max_write_calls) {
			return 0;
		} else if (flash_sim_stats.flash_write_calls ==
				flash_sim_thresholds.max_write_calls) {
			if (flash_sim_thresholds.max_len == 0) {
				return 0;
			}

			data_part_ignored = true;
		}
	}

	for (u32_t i = 0; i < len; i++) {
		if (data_part_ignored) {
			if (i >= flash_sim_thresholds.max_len) {
				return 0;
			}
		}

		/* only pull bits to zero */
		*(FLASH(offset + i)) &= *((u8_t *)data + i);
	}

	STATS_INCN(flash_sim_stats, bytes_written, len);

#ifdef CONFIG_FLASH_SIMULATOR_SIMULATE_TIMING
	/* wait before returning */
	k_busy_wait(CONFIG_FLASH_SIMULATOR_MIN_WRITE_TIME_US);
	STATS_INCN(flash_sim_stats, flash_write_time_us,
		   CONFIG_FLASH_SIMULATOR_MIN_WRITE_TIME_US);
#endif

	return 0;
}

static void unit_erase(const u32_t unit)
{
	const off_t unit_addr = FLASH_SIMULATOR_BASE_OFFSET +
				(unit * FLASH_SIMULATOR_ERASE_UNIT);

	/* byte pattern to fill the flash with */
	u8_t byte_pattern = 0xFF;

	/* erase the memory unit by pulling all bits to one */
	memset(FLASH(unit_addr), byte_pattern,
	       FLASH_SIMULATOR_ERASE_UNIT);
}

static int flash_sim_erase(struct device *dev, const off_t offset,
			   const size_t len)
{
	ARG_UNUSED(dev);

	if (!flash_range_is_valid(dev, offset, len)) {
		return -EINVAL;
	}

#ifdef CONFIG_FLASH_SIMULATOR_ERASE_PROTECT
	if (flash_wp_is_set()) {
		return -EACCES;
	}
#endif
	/* erase operation must be aligned to the erase unit boundary */
	if ((offset % FLASH_SIMULATOR_ERASE_UNIT) ||
	    (len % FLASH_SIMULATOR_ERASE_UNIT)) {
		return -EINVAL;
	}

	STATS_INC(flash_sim_stats, flash_erase_calls);

	if ((flash_sim_thresholds.max_erase_calls != 0) &&
	    (flash_sim_stats.flash_erase_calls >=
		flash_sim_thresholds.max_erase_calls)){
		return 0;
	}

	/* the first unit to be erased */
	u32_t unit_start = (offset - FLASH_SIMULATOR_BASE_OFFSET) /
			   FLASH_SIMULATOR_ERASE_UNIT;

	/* erase as many units as necessary and increase their erase counter */
	for (u32_t i = 0; i < len / FLASH_SIMULATOR_ERASE_UNIT; i++) {
		ERASE_CYCLES_INC(unit_start + i);
		unit_erase(unit_start + i);
	}

#ifdef CONFIG_FLASH_SIMULATOR_SIMULATE_TIMING
	/* wait before returning */
	k_busy_wait(CONFIG_FLASH_SIMULATOR_MIN_ERASE_TIME_US);
	STATS_INCN(flash_sim_stats, flash_erase_time_us,
		   CONFIG_FLASH_SIMULATOR_MIN_ERASE_TIME_US);
#endif

	return 0;
}

#ifdef CONFIG_FLASH_PAGE_LAYOUT
static const struct flash_pages_layout flash_sim_pages_layout = {
	.pages_count = FLASH_SIMULATOR_PAGE_COUNT,
	.pages_size = FLASH_SIMULATOR_ERASE_UNIT,
};

static void flash_sim_page_layout(struct device *dev,
				  const struct flash_pages_layout **layout,
				  size_t *layout_size)
{
	*layout = &flash_sim_pages_layout;
	*layout_size = 1;
}
#endif

static const struct flash_driver_api flash_sim_api = {
	.read = flash_sim_read,
	.write = flash_sim_write,
	.erase = flash_sim_erase,
	.write_protection = flash_wp_set,
	.write_block_size = FLASH_SIMULATOR_PROG_UNIT,
#ifdef CONFIG_FLASH_PAGE_LAYOUT
	.page_layout = flash_sim_page_layout,
#endif
};

#ifdef CONFIG_ARCH_POSIX

static int flash_mock_init(struct device *dev)
{
	if (flash_file_path == NULL) {
		flash_file_path = default_flash_file_path;
	}

	flash_fd = open(flash_file_path, O_RDWR | O_CREAT, (mode_t)0600);
	if (flash_fd == -1) {
		posix_print_warning("Failed to open flash device file "
				    "%s: %s\n",
				    flash_file_path, strerror(errno));
		return -EIO;
	}

	if (ftruncate(flash_fd, FLASH_SIMULATOR_FLASH_SIZE) == -1) {
		posix_print_warning("Failed to resize flash device file "
				    "%s: %s\n",
				    flash_file_path, strerror(errno));
		return -EIO;
	}

	mock_flash = mmap(NULL, FLASH_SIMULATOR_FLASH_SIZE,
			  PROT_WRITE | PROT_READ, MAP_SHARED, flash_fd, 0);
	if (mock_flash == MAP_FAILED) {
		posix_print_warning("Failed to mmap flash device file "
				    "%s: %s\n",
				    flash_file_path, strerror(errno));
		return -EIO;
	}

	return 0;
}

#else

static int flash_mock_init(struct device *dev)
{
	memset(mock_flash, 0xFF, ARRAY_SIZE(mock_flash));
	return 0;
}

#endif /* CONFIG_ARCH_POSIX */

static int flash_init(struct device *dev)
{
	STATS_INIT_AND_REG(flash_sim_stats, STATS_SIZE_32, "flash_sim_stats");
	STATS_INIT_AND_REG(flash_sim_thresholds, STATS_SIZE_32,
			   "flash_sim_thresholds");
	return flash_mock_init(dev);
}

DEVICE_AND_API_INIT(flash_simulator, FLASH_SIMULATOR_DEV_NAME, flash_init,
		    NULL, NULL, POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEVICE,
		    &flash_sim_api);

#ifdef CONFIG_ARCH_POSIX

static void flash_native_posix_cleanup(void)
{
	if ((mock_flash != MAP_FAILED) && (mock_flash != NULL)) {
		munmap(mock_flash, FLASH_SIMULATOR_FLASH_SIZE);
	}

	if (flash_fd != -1) {
		close(flash_fd);
	}
}

static void flash_native_posix_options(void)
{
	static struct args_struct_t flash_options[] = {
		{ .manual = false,
		  .is_mandatory = false,
		  .is_switch = false,
		  .option = "flash",
		  .name = "path",
		  .type = 's',
		  .dest = (void *)&flash_file_path,
		  .call_when_found = NULL,
		  .descript = "Path to binary file to be used as flash" },
		ARG_TABLE_ENDMARKER
	};

	native_add_command_line_opts(flash_options);
}


NATIVE_TASK(flash_native_posix_options, PRE_BOOT_1, 1);
NATIVE_TASK(flash_native_posix_cleanup, ON_EXIT, 1);

#endif /* CONFIG_ARCH_POSIX */
