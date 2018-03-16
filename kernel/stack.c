/*
 * Copyright (c) 2010-2016 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @brief fixed-size stack object
 */

#include <kernel.h>
#include <kernel_structs.h>
#include <debug/object_tracing_common.h>
#include <toolchain.h>
#include <linker/sections.h>
#include <ksched.h>
#include <wait_q.h>
#include <misc/__assert.h>
#include <init.h>
#include <syscall_handler.h>
#include <kswap.h>

extern struct k_stack _k_stack_list_start[];
extern struct k_stack _k_stack_list_end[];

#ifdef CONFIG_OBJECT_TRACING

struct k_stack *_trace_list_k_stack;

/*
 * Complete initialization of statically defined stacks.
 */
static int init_stack_module(struct device *dev)
{
	ARG_UNUSED(dev);

	struct k_stack *stack;

	for (stack = _k_stack_list_start; stack < _k_stack_list_end; stack++) {
		SYS_TRACING_OBJ_INIT(k_stack, stack);
	}
	return 0;
}

SYS_INIT(init_stack_module, PRE_KERNEL_1, CONFIG_KERNEL_INIT_PRIORITY_OBJECTS);

#endif /* CONFIG_OBJECT_TRACING */

void _impl_k_stack_init(struct k_stack *stack, u32_t *buffer,
			unsigned int num_entries)
{
	sys_dlist_init(&stack->wait_q);
	stack->next = stack->base = buffer;
	stack->top = stack->base + num_entries;

	SYS_TRACING_OBJ_INIT(k_stack, stack);
	_k_object_init(stack);
}

#ifdef CONFIG_USERSPACE
_SYSCALL_HANDLER(k_stack_init, stack, buffer, num_entries)
{
	_SYSCALL_OBJ_INIT(stack, K_OBJ_STACK);
	_SYSCALL_MEMORY_ARRAY_WRITE(buffer, num_entries, sizeof(u32_t));

	_impl_k_stack_init((struct k_stack *)stack, (u32_t *)buffer,
			   num_entries);
	return 0;
}
#endif

void _impl_k_stack_push(struct k_stack *stack, u32_t data)
{
	struct k_thread *first_pending_thread;
	unsigned int key;

	__ASSERT(stack->next != stack->top, "stack is full");

	key = irq_lock();

	first_pending_thread = _unpend_first_thread(&stack->wait_q);

	if (first_pending_thread) {
		_abort_thread_timeout(first_pending_thread);
		_ready_thread(first_pending_thread);

		_set_thread_return_value_with_data(first_pending_thread,
						   0, (void *)data);

		if (!_is_in_isr() && _must_switch_threads()) {
			(void)_Swap(key);
			return;
		}
	} else {
		*(stack->next) = data;
		stack->next++;
	}

	irq_unlock(key);
}

#ifdef CONFIG_USERSPACE
_SYSCALL_HANDLER(k_stack_push, stack_p, data)
{
	struct k_stack *stack = (struct k_stack *)stack_p;

	_SYSCALL_OBJ(stack, K_OBJ_STACK);
	_SYSCALL_VERIFY_MSG(stack->next != stack->top, "stack is full");

	_impl_k_stack_push(stack, data);
	return 0;
}
#endif

int _impl_k_stack_pop(struct k_stack *stack, u32_t *data, s32_t timeout)
{
	unsigned int key;
	int result;

	key = irq_lock();

	if (likely(stack->next > stack->base)) {
		stack->next--;
		*data = *(stack->next);
		irq_unlock(key);
		return 0;
	}

	if (timeout == K_NO_WAIT) {
		irq_unlock(key);
		return -EBUSY;
	}

	_pend_current_thread(&stack->wait_q, timeout);

	result = _Swap(key);
	if (result == 0) {
		*data = (u32_t)_current->base.swap_data;
	}
	return result;
}

#ifdef CONFIG_USERSPACE
_SYSCALL_HANDLER(k_stack_pop, stack, data, timeout)
{
	_SYSCALL_OBJ(stack, K_OBJ_STACK);
	_SYSCALL_MEMORY_WRITE(data, sizeof(u32_t));

	return _impl_k_stack_pop((struct k_stack *)stack, (u32_t *)data,
				 timeout);
}
#endif
