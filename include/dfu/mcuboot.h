/*
 * Copyright (c) 2017 Nordic Semiconductor ASA
 * Copyright (c) 2016 Linaro Limited
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __MCUBOOT_H__
#define __MCUBOOT_H__

#include <stdbool.h>
#include <stddef.h>

#include <zephyr/types.h>

/** Attempt to boot the contents of slot 0. */
#define BOOT_SWAP_TYPE_NONE     1

/** Swap to slot 1.  Absent a confirm command, revert back on next boot. */
#define BOOT_SWAP_TYPE_TEST     2

/** Swap to slot 1, and permanently switch to booting its contents. */
#define BOOT_SWAP_TYPE_PERM     3

/** Swap back to alternate slot.  A confirm changes this state to NONE. */
#define BOOT_SWAP_TYPE_REVERT   4

/** Swap failed because image to be run is not valid */
#define BOOT_SWAP_TYPE_FAIL     5

#define BOOT_IMG_VER_STRLEN_MAX 25  /* 255.255.65535.4294967295\0 */

/**
 * @brief MCUboot image header representation for image version
 *
 * The header for an MCUboot firmware image contains an embedded
 * version number, in semantic versioning format. This structure
 * represents the information it contains.
 */
struct mcuboot_img_sem_ver {
	u8_t major;
	u8_t minor;
	u16_t revision;
	u32_t build_num;
};

/**
 * @brief Model for the MCUboot image header as of version 1
 *
 * This represents the data present in the image header, in version 1
 * of the header format.
 *
 * Some information present in the header but not currently relevant
 * to applications is omitted.
 */
struct mcuboot_img_header_v1 {
	/** The size of the image, in bytes. */
	u32_t image_size;
	/** The image version. */
	struct mcuboot_img_sem_ver sem_ver;
};

/**
 * @brief Model for the MCUBoot image header
 *
 * This contains the decoded image header, along with the major
 * version of MCUboot that the header was built for.
 *
 * (The MCUboot project guarantees that incompatible changes to the
 * image header will result in major version changes to the bootloader
 * itself, and will be detectable in the persistent representation of
 * the header.)
 */
struct mcuboot_img_header {
	/**
	 * The version of MCUboot the header is built for.
	 *
	 * The value 1 corresponds to MCUboot versions 1.x.y.
	 */
	u32_t mcuboot_version;
	/**
	 * The header information. It is only valid to access fields
	 * in the union member corresponding to the mcuboot_version
	 * field above.
	 */
	union {
		/** Header information for MCUboot version 1. */
		struct mcuboot_img_header_v1 v1;
	} h;
};

/**
 * @brief Read the MCUboot image header information from an image bank.
 *
 * This attempts to parse the image header, which must begin at offset
 * @a bank_offset from the beginning of the flash device used by
 * MCUboot.
 *
 * @param bank_offset Offset of the image header from the start of the
 *                    flash device used by MCUboot to store firmware.
 * @param header On success, the returned header information is available
 *               in this structure.
 * @param header_size Size of the header structure passed by the caller.
 *                    If this is not large enough to contain all of the
 *                    necessary information, an error is returned.
 * @return Zero on success, a negative value on error.
 */
int boot_read_bank_header(u32_t bank_offset,
			  struct mcuboot_img_header *header,
			  size_t header_size);

/**
 * @brief Check if the currently running image is confirmed as OK.
 *
 * MCUboot can perform "test" upgrades. When these occur, a new
 * firmware image is installed and booted, but the old version will be
 * reverted at the next reset unless the new image explicitly marks
 * itself OK.
 *
 * This routine can be used to check if the currently running image
 * has been marked as OK.
 *
 * @return True if the image is confirmed as OK, false otherwise.
 * @see boot_write_img_confirmed()
 */
bool boot_is_img_confirmed(void);

/**
 * @brief Marks the currently running image as confirmed.
 *
 * This routine attempts to mark the currently running firmware image
 * as OK, which will install it permanently, preventing MCUboot from
 * reverting it for an older image at the next reset.
 *
 * This routine is safe to call if the current image has already been
 * confirmed. It will return a successful result in this case.
 *
 * @return 0 on success, negative errno code on fail.
 */
int boot_write_img_confirmed(void);

/**
 * @brief Determines the action, if any, that mcuboot will take on the next
 * reboot.
 * @return a BOOT_SWAP_TYPE_[...] constant on success, negative errno code on
 * fail.
 */
int boot_swap_type(void);

/**
 * @brief Marks the image in slot 1 as pending. On the next reboot, the system
 * will perform a boot of the slot 1 image.
 *
 * @param permanent Whether the image should be used permanently or
 * only tested once:
 *   0=run image once, then confirm or revert.
 *   1=run image forever.
 * @return 0 on success, negative errno code on fail.
 */
int boot_request_upgrade(int permanent);

/**
 * @brief Erase the image Bank.
 *
 * @param bank_offset address of the image bank
 * @return 0 on success, negative errno code on fail.
 */
int boot_erase_img_bank(u32_t bank_offset);

#endif  /* __MCUBOOT_H__ */
