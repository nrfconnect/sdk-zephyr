/*
 * Copyright (c) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <adc.h>
#include <syscall_handler.h>
#include <kernel.h>

Z_SYSCALL_HANDLER(adc_channel_setup, dev, user_channel_cfg)
{
	struct adc_channel_cfg channel_cfg;

	Z_OOPS(Z_SYSCALL_DRIVER_ADC(dev, channel_setup));
	Z_OOPS(z_user_from_copy(&channel_cfg,
				(struct adc_channel_cfg *)user_channel_cfg,
				sizeof(struct adc_channel_cfg)));

	return z_impl_adc_channel_setup((struct device *)dev, &channel_cfg);
}

static bool copy_sequence(struct adc_sequence *dst,
			  struct adc_sequence_options *options,
			  struct adc_sequence *src)
{
	if (z_user_from_copy(dst, src, sizeof(struct adc_sequence)) != 0) {
		printk("couldn't copy adc_sequence struct\n");
		return false;
	}

	if (dst->options) {
		if (z_user_from_copy(options, dst->options,
				sizeof(struct adc_sequence_options)) != 0) {
			printk("couldn't copy adc_options struct\n");
			return false;
		}
		dst->options = options;
	}

	if (Z_SYSCALL_MEMORY_WRITE(dst->buffer, dst->buffer_size) != 0) {
		printk("no access to buffer memory\n");
		return false;
	}
	return true;
}


Z_SYSCALL_HANDLER(adc_read, dev, user_sequence)
{
	struct adc_sequence sequence;
	struct adc_sequence_options options;

	Z_OOPS(Z_SYSCALL_DRIVER_ADC(dev, read));
	Z_OOPS(Z_SYSCALL_VERIFY_MSG(copy_sequence(&sequence, &options,
					(struct adc_sequence *)user_sequence),
				    "invalid ADC sequence"));
	if (sequence.options != NULL) {
		Z_OOPS(Z_SYSCALL_VERIFY_MSG(sequence.options->callback == NULL,
			    "ADC sequence callbacks forbidden from user mode"));
	}

	return z_impl_adc_read((struct device *)dev, &sequence);
}

#ifdef CONFIG_ADC_ASYNC
Z_SYSCALL_HANDLER(adc_read_async, dev, user_sequence, async)
{
	struct adc_sequence sequence;
	struct adc_sequence_options options;

	Z_OOPS(Z_SYSCALL_DRIVER_ADC(dev, read_async));
	Z_OOPS(Z_SYSCALL_VERIFY_MSG(copy_sequence(&sequence, &options,
					(struct adc_sequence *)user_sequence),
				    "invalid ADC sequence"));
	if (sequence.options != NULL) {
		Z_OOPS(Z_SYSCALL_VERIFY_MSG(sequence.options->callback == NULL,
			    "ADC sequence callbacks forbidden from user mode"));
	}
	Z_OOPS(Z_SYSCALL_OBJ(async, K_OBJ_POLL_SIGNAL));

	return z_impl_adc_read_async((struct device *)dev, &sequence,
				     (struct k_poll_signal *)async);
}
#endif /* CONFIG_ADC_ASYNC */
