/*
 * Copyright (c) 2013-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief ARM kernel structure member offset definition file
 *
 * This module is responsible for the generation of the absolute symbols whose
 * value represents the member offsets for various ARM kernel structures.
 *
 * All of the absolute symbols defined by this module will be present in the
 * final kernel ELF image (due to the linker's reference to the _OffsetAbsSyms
 * symbol).
 *
 * INTERNAL
 * It is NOT necessary to define the offset for every member of a structure.
 * Typically, only those members that are accessed by assembly language routines
 * are defined; however, it doesn't hurt to define all fields for the sake of
 * completeness.
 */

#include <gen_offset.h>
#include <kernel_structs.h>
#include <kernel_offsets.h>

GEN_OFFSET_SYM(_thread_arch_t, basepri);
GEN_OFFSET_SYM(_thread_arch_t, swap_return_value);

#ifdef CONFIG_USERSPACE
GEN_OFFSET_SYM(_thread_arch_t, mode);
GEN_OFFSET_SYM(_thread_arch_t, priv_stack_start);
#endif

#ifdef CONFIG_FLOAT
GEN_OFFSET_SYM(_thread_arch_t, preempt_float);
#endif

GEN_OFFSET_SYM(_esf_t, a1);
GEN_OFFSET_SYM(_esf_t, a2);
GEN_OFFSET_SYM(_esf_t, a3);
GEN_OFFSET_SYM(_esf_t, a4);
GEN_OFFSET_SYM(_esf_t, ip);
GEN_OFFSET_SYM(_esf_t, lr);
GEN_OFFSET_SYM(_esf_t, pc);
GEN_OFFSET_SYM(_esf_t, xpsr);

#ifdef CONFIG_FLOAT
GEN_OFFSET_SYM(_esf_t, s);
GEN_OFFSET_SYM(_esf_t, fpscr);
#endif

GEN_ABSOLUTE_SYM(___esf_t_SIZEOF, sizeof(_esf_t));

GEN_OFFSET_SYM(_callee_saved_t, v1);
GEN_OFFSET_SYM(_callee_saved_t, v2);
GEN_OFFSET_SYM(_callee_saved_t, v3);
GEN_OFFSET_SYM(_callee_saved_t, v4);
GEN_OFFSET_SYM(_callee_saved_t, v5);
GEN_OFFSET_SYM(_callee_saved_t, v6);
GEN_OFFSET_SYM(_callee_saved_t, v7);
GEN_OFFSET_SYM(_callee_saved_t, v8);
GEN_OFFSET_SYM(_callee_saved_t, psp);

/* size of the entire preempt registers structure */

GEN_ABSOLUTE_SYM(___callee_saved_t_SIZEOF, sizeof(struct _callee_saved));

/*
 * size of the struct k_thread structure sans save area for floating
 * point registers.
 */

#ifdef CONFIG_FLOAT
GEN_ABSOLUTE_SYM(_K_THREAD_NO_FLOAT_SIZEOF, sizeof(struct k_thread) -
					    sizeof(struct _preempt_float));
#else
GEN_ABSOLUTE_SYM(_K_THREAD_NO_FLOAT_SIZEOF, sizeof(struct k_thread));
#endif

GEN_ABS_SYM_END
