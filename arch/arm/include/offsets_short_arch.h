/*
 * Copyright (c) 2016 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _offsets_short_arch__h_
#define _offsets_short_arch__h_

#include <offsets.h>

/* kernel */

/* nothing for now */

/* end - kernel */

/* threads */

#define _thread_offset_to_basepri \
	(___thread_t_arch_OFFSET + ___thread_arch_t_basepri_OFFSET)

#define _thread_offset_to_swap_return_value \
	(___thread_t_arch_OFFSET + ___thread_arch_t_swap_return_value_OFFSET)

#define _thread_offset_to_preempt_float \
	(___thread_t_arch_OFFSET + ___thread_arch_t_preempt_float_OFFSET)

#ifdef CONFIG_USERSPACE
#define _thread_offset_to_mode \
	(___thread_t_arch_OFFSET + ___thread_arch_t_mode_OFFSET)

#define _thread_offset_to_priv_stack_start \
	(___thread_t_arch_OFFSET + ___thread_arch_t_priv_stack_start_OFFSET)
#endif

/* end - threads */

#endif /* _offsets_short_arch__h_ */
