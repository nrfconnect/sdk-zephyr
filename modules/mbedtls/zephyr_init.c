/** @file
 * @brief mbed TLS initialization
 *
 * Initialize the mbed TLS library like setup the heap etc.
 */

/*
 * Copyright (c) 2017 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/init.h>
#include <zephyr/app_memory/app_memdomain.h>
#include <zephyr/drivers/entropy.h>
#include <zephyr/random/rand32.h>
#include <mbedtls/entropy.h>

#include <mbedtls/debug.h>

#if defined(CONFIG_MBEDTLS)
#if !defined(CONFIG_MBEDTLS_CFG_FILE)
#include "mbedtls/config.h"
#else
#include CONFIG_MBEDTLS_CFG_FILE
#endif /* CONFIG_MBEDTLS_CFG_FILE */
#endif

#if defined(CONFIG_MBEDTLS_ENABLE_HEAP) && \
					defined(MBEDTLS_MEMORY_BUFFER_ALLOC_C)
#include <mbedtls/memory_buffer_alloc.h>

#if !defined(CONFIG_MBEDTLS_HEAP_SIZE)
#error "Please set heap size to be used. Set value to CONFIG_MBEDTLS_HEAP_SIZE \
option."
#endif

static unsigned char _mbedtls_heap[CONFIG_MBEDTLS_HEAP_SIZE];

static void init_heap(void)
{
	mbedtls_memory_buffer_alloc_init(_mbedtls_heap, sizeof(_mbedtls_heap));
}
#else
#define init_heap(...)
#endif /* CONFIG_MBEDTLS_ENABLE_HEAP && MBEDTLS_MEMORY_BUFFER_ALLOC_C */

static const struct device *const entropy_dev =
			DEVICE_DT_GET_OR_NULL(DT_CHOSEN(zephyr_entropy));

int mbedtls_hardware_poll(void *data, unsigned char *output, size_t len,
			  size_t *olen)
{
	int ret;
	uint16_t request_len = len > UINT16_MAX ? UINT16_MAX : len;

	ARG_UNUSED(data);

	if (output == NULL || olen == NULL || len == 0) {
		return MBEDTLS_ERR_ENTROPY_SOURCE_FAILED;
	}

	if (!IS_ENABLED(CONFIG_ENTROPY_HAS_DRIVER)) {
		sys_rand_get(output, len);
		*olen = len;

		return 0;
	}

	if (!device_is_ready(entropy_dev)) {
		return MBEDTLS_ERR_ENTROPY_SOURCE_FAILED;
	}

	ret = entropy_get_entropy(entropy_dev, (uint8_t *)output, request_len);
	if (ret < 0) {
		return MBEDTLS_ERR_ENTROPY_SOURCE_FAILED;
	}

	*olen = request_len;

	return 0;
}

static int _mbedtls_init(const struct device *device)
{
	ARG_UNUSED(device);

	init_heap();

#if defined(CONFIG_MBEDTLS_DEBUG_LEVEL)
	mbedtls_debug_set_threshold(CONFIG_MBEDTLS_DEBUG_LEVEL);
#endif

	return 0;
}

SYS_INIT(_mbedtls_init, POST_KERNEL, 0);
