/*
 * Copyright (c) 2018 Oticon A/S
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Pseudo-random entropy generator for the ARCH_POSIX architecture:
 * Following the principle of reproducibility of the native_posix board
 * this entropy device will always generate the same random sequence when
 * initialized with the same seed
 *
 * This entropy source should only be used for testing.
 */

#include "device.h"
#include "entropy.h"
#include "init.h"
#include "misc/util.h"
#include <stdlib.h>
#include <string.h>
#include "posix_soc_if.h"
#include "soc.h"
#include "cmdline.h" /* native_posix command line options header */

static unsigned int seed = 0x5678;

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

static int entropy_native_posix_get_entropy_isr(struct device *dev, u8_t *buf,
						u16_t len, u32_t flags)
{
	ARG_UNUSED(flags);

	/*
	 * entropy_native_posix_get_entropy() is also safe for ISRs
	 * and always produces data.
	 */
	return entropy_native_posix_get_entropy(dev, buf, len);
}

static int entropy_native_posix_init(struct device *dev)
{
	ARG_UNUSED(dev);
	srandom(seed);
	posix_print_warning("WARNING: "
			    "Using a test - not safe - entropy source\n");
	return 0;
}

static const struct entropy_driver_api entropy_native_posix_api_funcs = {
	.get_entropy     = entropy_native_posix_get_entropy,
	.get_entropy_isr = entropy_native_posix_get_entropy_isr
};

DEVICE_AND_API_INIT(entropy_native_posix, CONFIG_ENTROPY_NAME,
		    entropy_native_posix_init, NULL, NULL,
		    PRE_KERNEL_1, CONFIG_KERNEL_INIT_PRIORITY_DEVICE,
		    &entropy_native_posix_api_funcs);

static void add_fake_entropy_option(void)
{
	static struct args_struct_t entropy_options[] = {
		/*
		 * Fields:
		 * manual, mandatory, switch,
		 * option_name, var_name ,type,
		 * destination, callback,
		 * description
		 */
		{false, false, false,
		"seed", "r_seed", 'u',
		(void *)&seed, NULL,
		"A 32-bit integer seed value for the entropy device, such as "
		"97229 (decimal), 0x17BCD (hex), or 0275715 (octal)"},
		ARG_TABLE_ENDMARKER
	};

	native_add_command_line_opts(entropy_options);
}

NATIVE_TASK(add_fake_entropy_option, PRE_BOOT_1, 10);
