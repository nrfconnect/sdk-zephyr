/*
 * Copyright (c) 2018 Oticon A/S
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Pseudo-random entropy generator for the ARCH_POSIX architecture:
 * Following the principle of reproducibility of the native_posix board
 * this entropy device will always generate the same random sequence when
 * initialized with the same seed
 */

#include "device.h"
#include "entropy.h"
#include "init.h"
#include "misc/util.h"
#include <stdlib.h>
#include <string.h>

static unsigned int seed = 0x5678;

void entropy_native_posix_set_seed(unsigned int seed_i)
{
	seed = seed_i;
}

static int entropy_native_posix_get_entropy(struct device *dev, u8_t *buffer,
					    u16_t length)
{
	ARG_UNUSED(dev);

	while (length) {
		/*
		 * Note that only 1 thread (Zephyr thread or HW models), runs at
		 * a time, therefore there is no need to use random_r()
		 */
		long int value = random();

		size_t to_copy = min(length, sizeof(long int));

		memcpy(buffer, &value, to_copy);
		buffer += to_copy;
		length -= to_copy;
	}

	return 0;
}

static int entropy_native_posix_init(struct device *dev)
{
	ARG_UNUSED(dev);
	srandom(seed);
	return 0;
}

static const struct entropy_driver_api entropy_native_posix_api_funcs = {
	.get_entropy = entropy_native_posix_get_entropy
};

DEVICE_AND_API_INIT(entropy_native_posix, CONFIG_ENTROPY_NAME,
		    entropy_native_posix_init, NULL, NULL,
		    PRE_KERNEL_2, CONFIG_KERNEL_INIT_PRIORITY_DEVICE,
		    &entropy_native_posix_api_funcs);

