/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Public API for ZSAI drivers
 */

#ifndef ZEPHYR_INCLUDE_DRIVERS_ZSAI_H_
#define ZEPHYR_INCLUDE_DRIVERS_ZSAI_H_

/**
 * @brief ZSAI internal Interface
 * @defgroup zsai_internal_interface ZSAI internal Interface
 * @ingroup io_interfaces
 * @{
 */

#include <errno.h>

#include <zephyr/types.h>
#include <stddef.h>
#include <sys/types.h>
#include <zephyr/device.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @}
 */

/**
 * @brief ZSAI Interface
 * @defgroup zsai_interface ZSAI Interface
 * @ingroup io_interfaces
 * @{
 */

/**
 * @}
 */

/**
 * @addtogroup zsai_internal_interface
 * @{
 */

/**
 * @brief Device read
 *
 * @param	dev		: device to read from.
 * @param	data		: pointer to buffer for data,
 * @param	offset		: offset on device to start write at,
 * @param	size		: size of data to write.
 *
 * @return 0 on success, -ENOTSUP when not supported. Negative errno type code in
 * case of other error.
 */
typedef int (*zsai_api_read)(const struct device *dev, void *data, off_t offset,
			     size_t size);
/**
 * @brief Device write
 *
 * @note Any necessary write protection management must be performed by
 * the driver, with the driver responsible for ensuring the "write-protect"
 * after the operation completes (successfully or not) matches the write-protect
 * state when the operation was started.
 *
 * @param	dev		: device to write to.
 * @param	data		: pointer to buffer with data.
 * @param	offset		: offset on device to start write at.
 * @param	size		: size of data to write.
 *
 * @note @p offset and @p size should be aligned to write-block-size of device.
 *
 * @return 0 on success, -ENOTSUP when not supported. Negative errno type code in
 * case of other error.
 */
typedef int (*zsai_api_write)(const struct device *dev, const void *data, off_t offset,
			      size_t size);

/**
 * @brief IOCTL handler requiring syscall level
 *
 * @note The system and user level IOCTL handlers use the same IDs for
 * IOCTL operations. Internal implementation should reject user level
 * IOCTL operation, by returning -ENOTSUP, when requested in syscall level handler.
 *
 * @param	dev		: device to perform operation on.
 * @param	id		: IOCTL operation ID
 * @param	in		: input structure or ID of sub-operation
 * @param	in_out		: additional parameter or pointer to output buffer.
 *
 * @return 0 on success, -ENOTSUP when not supported. Negative errno type code in
 * case of other error.
 */
typedef int (*zsai_api_sys_ioctl)(const struct device *dev, uint32_t id, const uintptr_t in,
				  uintptr_t in_out);

struct zsai_infoword {
	uint32_t erase_required		: 1;
	uint32_t erase_bit_value	: 1;
	uint32_t uniform_page_size	: 1;
	uint32_t write_block_size	: 4;
};

struct zsai_device_generic_config {
	struct zsai_infoword infoword;
};

__subsystem struct zsai_driver_api {
	zsai_api_read read;
	zsai_api_write write;
	zsai_api_sys_ioctl sys_ioctl;
};

#define ZSAI_API_PTR(dev) ((const struct zsai_driver_api *)dev->api)
#define ZSAI_DEV_CONFIG(dev) ((const struct zsai_device_generic_config *)dev->config)
#define ZSAI_DEV_INFOWORD(dev) (ZSAI_DEV_CONFIG(dev)->infoword)

/**
 * @}
 */

/**
 * @addtogroup zsai_interface
 * @{
 */

/**
 *  @brief  Read data from zsai
 *
 *  All zsai drivers support reads without alignment restrictions on
 *  the read offset, the read size, or the destination address.
 *
 *  @param  dev             : zsai dev
 *  @param  data            : Buffer to store read data
 *  @param  offset          : Offset (byte aligned) to read
 *  @param  size            : Number of bytes to read.
 *
 *  @return  0 on success, negative errno code on fail.
 */
__syscall int zsai_read(const struct device *dev, void *data, off_t offset,
			size_t size);

static inline int z_impl_zsai_read(const struct device *dev, void *data,
				   off_t offset, size_t size)
{
	const struct zsai_driver_api *api = ZSAI_API_PTR(dev);

	return api->read(dev, data, offset, size);
}

/**
 *  @brief  Write buffer into zsai memory.
 *
 *  All zsai drivers support a source buffer located either in RAM or
 *  SoC zsai, without alignment restrictions on the source address.
 *  Write size and offset must be multiples of the minimum write block size
 *  supported by the driver.
 *
 *  Any necessary write protection management is performed by the driver
 *  write implementation itself.
 *
 *  @param  dev             : zsai device
 *  @param  data            : data to write
 *  @param  offset          : starting offset for the write
 *  @param  size            : Number of bytes to write
 *
 *  @return  0 on success, negative errno code on fail.
 */
__syscall int zsai_write(const struct device *dev, const void *data,
			 off_t offset, size_t size);

static inline int z_impl_zsai_write(const struct device *dev, const void *data,
				    off_t offset, size_t size)
{
	const struct zsai_driver_api *api = ZSAI_API_PTR(dev);
	int rc;

	rc = api->write(dev, data, offset, size);

	return rc;
}

/**
 * @brief IOCTL invocation at syscall level
 *
 * This can be called by user directly although it has been designed to be called by
 * zsai_ioctl, which will decide whether userspace or syscall space call for
 * IOCTL ID is needed.
 *
 * @param dev		: zsai device
 * @param id		: IOCTL operation ID
 * @param in		: input structure or identifier of sub-operation
 * @param in_out	: additional parameter or pointer to output buffer.
 */
__syscall int zsai_ioctl(const struct device *dev, uint32_t id,
			      const uintptr_t in, uintptr_t in_out);

static inline int z_impl_zsai_ioctl(const struct device *dev, uint32_t id,
					 const uintptr_t in, uintptr_t in_out)
{
	const struct zsai_driver_api *api = ZSAI_API_PTR(dev);

	return api->sys_ioctl(dev, id, in, in_out);
}

int zsai_ioctl(const struct device *dev, uint32_t id, const uintptr_t in,
	       uintptr_t in_out);

#ifdef __cplusplus
}
#endif

/**
 * @}
 */

#include <syscalls/zsai.h>

#endif /* ZEPHYR_INCLUDE_DRIVERS_ZSAI_H_ */
