/*
 * Copyright (c) 2017, Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */


#ifndef _ZEPHYR_SYSCALL_H_
#define _ZEPHYR_SYSCALL_H_

#ifndef _ASMLANGUAGE
#include <zephyr/types.h>
#include <syscall_list.h>
#include <syscall_macros.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * System Call Declaration macros
 *
 * These macros are used in public header files to declare system calls.
 * They generate inline functions which have different implementations
 * depending on the current compilation context:
 *
 * - Kernel-only code, or CONFIG_USERSPACE disabled, these inlines will
 *   directly call the implementation
 * - User-only code, these inlines will marshal parameters and elevate
 *   privileges
 * - Mixed or indeterminate code, these inlines will do a runtime check
 *   to determine what course of action is needed.
 *
 * All system calls require a handler function and an implementation function.
 * These must follow a naming convention. For a system call named k_foo():
 *
 * - The handler function will be named _handler_k_foo(). Handler functions
 *   are always of type _k_syscall_handler_t, verify arguments passed up
 *   from userspace, and call the implementation function. See
 *   documentation for that typedef for more information.
 * - The implementation function will be named _impl_k_foo(). This is the
 *   actual implementation of the system call.
 *
 * The basic declartion macros are as follows. System calls with 0 to 10
 * parameters are supported. For a system call with N parameters, that returns
 * a value and is* not implemented inline, the macro is as follows (N noted
 * as {N} for clarity):
 *
 * K_SYSCALL_DECLARE{N}(id, name, ret, t0, p0, ... , t{N-1}, p{N-1})

 * @param id System call ID, one of K_SYSCALL_* defines
 * @param name Symbol name of the system call used to invoke it
 * @param ret Data type of return value
 * @param tX Data type of parameter X
 * @param pX Name of parameter x
 *
 * For system calls that return no value:
 *
 * K_SYSCALL_DECLARE{n}_VOID(id, name, t0, p0, .... , t{N-1}, p{N-1})
 *
 * This is identical to above except there is no 'ret' parameter.
 *
 * For system calls where the implementation is an inline function, we have
 *
 * K_SYSCALL_DECLARE{n}_INLINE(id, name, ret, t0, p0, ... , t{N-1}, p{N-1})
 * K_SYSCALL_DECLARE{n}_VOID_INLINE(id, name, t0, p0, ... , t{N-1}, p{N-1})
 *
 * These are used in the same way as their non-INLINE counterparts.
 *
 * These macros are generated by scripts/gen_syscall_header.py and can be
 * found in $OUTDIR/include/generated/syscall_macros.h
 */

/**
 * @typedef _k_syscall_handler_t
 * @brief System call handler function type
 *
 * These are kernel-side skeleton functions for system calls. They are
 * necessary to sanitize the arguments passed into the system call:
 *
 * - Any kernel object or device pointers are validated with _SYSCALL_IS_OBJ()
 * - Any memory buffers passed in are checked to ensure that the calling thread
 *   actually has access to them
 * - Many kernel calls do no sanity checking of parameters other than
 *   assertions. The handler must check all of these conditions using
 *   _SYSCALL_ASSERT()
 * - If the system call has more than 6 arguments, then arg6 will be a pointer
 *   to some struct containing arguments 6+. The struct itself needs to be
 *   validated like any other buffer passed in from userspace, and its members
 *   individually validated (if necessary) and then passed to the real
 *   implementation like normal arguments
 *
 * Even if the system call implementation has no return value, these always
 * return something, even 0, to prevent register leakage to userspace.
 *
 * Once everything has been validated, the real implementation will be executed.
 *
 * @param arg1 system call argument 1
 * @param arg2 system call argument 2
 * @param arg3 system call argument 3
 * @param arg4 system call argument 4
 * @param arg5 system call argument 5
 * @param arg6 system call argument 6
 * @param ssf System call stack frame pointer. Used to generate kernel oops
 *            via _arch_syscall_oops_at(). Contents are arch-specific.
 * @return system call return value, or 0 if the system call implementation
 *         return void
 *
 */
typedef u32_t (*_k_syscall_handler_t)(u32_t arg1, u32_t arg2, u32_t arg3,
				      u32_t arg4, u32_t arg5, u32_t arg6,
				      void *ssf);
#ifdef CONFIG_USERSPACE

/**
 * Indicate whether we are currently running in user mode
 *
 * @return nonzero if the CPU is currently running with user permissions
 */
static inline int _arch_is_user_context(void);

/**
 * Indicate whether the CPU is currently in user mode
 *
 * @return nonzero if the CPU is currently running with user permissions
 */
static inline int _is_user_context(void)
{
	return _arch_is_user_context();
}

/*
 * Helper data structures for system calls with large argument lists
 */

struct _syscall_7_args {
	u32_t arg6;
	u32_t arg7;
};

struct _syscall_8_args {
	u32_t arg6;
	u32_t arg7;
	u32_t arg8;
};

struct _syscall_9_args {
	u32_t arg6;
	u32_t arg7;
	u32_t arg8;
	u32_t arg9;
};

struct _syscall_10_args {
	u32_t arg6;
	u32_t arg7;
	u32_t arg8;
	u32_t arg9;
	u32_t arg10;
};

/*
 * Interfaces for invoking system calls
 */

static inline u32_t _arch_syscall_invoke0(u32_t call_id);

static inline u32_t _arch_syscall_invoke1(u32_t arg1, u32_t call_id);

static inline u32_t _arch_syscall_invoke2(u32_t arg1, u32_t arg2,
					  u32_t call_id);

static inline u32_t _arch_syscall_invoke3(u32_t arg1, u32_t arg2, u32_t arg3,
					  u32_t call_id);

static inline u32_t _arch_syscall_invoke4(u32_t arg1, u32_t arg2, u32_t arg3,
					  u32_t arg4, u32_t call_id);

static inline u32_t _arch_syscall_invoke5(u32_t arg1, u32_t arg2, u32_t arg3,
					  u32_t arg4, u32_t arg5,
					  u32_t call_id);

static inline u32_t _arch_syscall_invoke6(u32_t arg1, u32_t arg2, u32_t arg3,
					  u32_t arg4, u32_t arg5, u32_t arg6,
					  u32_t call_id);

static inline u32_t _syscall_invoke7(u32_t arg1, u32_t arg2, u32_t arg3,
				    u32_t arg4, u32_t arg5, u32_t arg6,
				    u32_t arg7, u32_t call_id) {
	struct _syscall_7_args args = {
		.arg6 = arg6,
		.arg7 = arg7,
	};

	return _arch_syscall_invoke6(arg1, arg2, arg3, arg4, arg5, (u32_t)&args,
				     call_id);
}

static inline u32_t _syscall_invoke8(u32_t arg1, u32_t arg2, u32_t arg3,
				    u32_t arg4, u32_t arg5, u32_t arg6,
				    u32_t arg7, u32_t arg8, u32_t call_id)
{
	struct _syscall_8_args args = {
		.arg6 = arg6,
		.arg7 = arg7,
		.arg8 = arg8,
	};

	return _arch_syscall_invoke6(arg1, arg2, arg3, arg4, arg5, (u32_t)&args,
				     call_id);
}

static inline u32_t _syscall_invoke9(u32_t arg1, u32_t arg2, u32_t arg3,
				    u32_t arg4, u32_t arg5, u32_t arg6,
				    u32_t arg7, u32_t arg8, u32_t arg9,
				    u32_t call_id)
{
	struct _syscall_9_args args = {
		.arg6 = arg6,
		.arg7 = arg7,
		.arg8 = arg8,
		.arg9 = arg9,
	};

	return _arch_syscall_invoke6(arg1, arg2, arg3, arg4, arg5, (u32_t)&args,
				     call_id);
}

static inline u32_t _syscall_invoke10(u32_t arg1, u32_t arg2, u32_t arg3,
				     u32_t arg4, u32_t arg5, u32_t arg6,
				     u32_t arg7, u32_t arg8, u32_t arg9,
				     u32_t arg10, u32_t call_id)
{
	struct _syscall_10_args args = {
		.arg6 = arg6,
		.arg7 = arg7,
		.arg8 = arg8,
		.arg9 = arg9,
		.arg10 = arg10
	};

	return _arch_syscall_invoke6(arg1, arg2, arg3, arg4, arg5, (u32_t)&args,
				     call_id);
}

static inline u64_t _syscall_ret64_invoke0(u32_t call_id)
{
	u64_t ret;

	_arch_syscall_invoke1((u32_t)&ret, call_id);
	return ret;
}

static inline u64_t _syscall_ret64_invoke1(u32_t arg1, u32_t call_id)
{
	u64_t ret;

	_arch_syscall_invoke2(arg1, (u32_t)&ret, call_id);
	return ret;
}

static inline u64_t _syscall_ret64_invoke2(u32_t arg1, u32_t arg2,
					   u32_t call_id)
{
	u64_t ret;

	_arch_syscall_invoke3(arg1, arg2, (u32_t)&ret, call_id);
	return ret;
}

#endif /* CONFIG_USERSPACE */

#ifdef __cplusplus
}
#endif

#endif /* _ASMLANGUAGE */

#endif

