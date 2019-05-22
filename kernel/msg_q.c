/*
 * Copyright (c) 2016 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Message queues.
 */


#include <kernel.h>
#include <kernel_structs.h>
#include <debug/object_tracing_common.h>
#include <toolchain.h>
#include <linker/sections.h>
#include <string.h>
#include <wait_q.h>
#include <misc/dlist.h>
#include <init.h>
#include <syscall_handler.h>
#include <kernel_internal.h>

extern struct k_msgq _k_msgq_list_start[];
extern struct k_msgq _k_msgq_list_end[];

#ifdef CONFIG_OBJECT_TRACING

struct k_msgq *_trace_list_k_msgq;

/*
 * Complete initialization of statically defined message queues.
 */
static int init_msgq_module(struct device *dev)
{
	ARG_UNUSED(dev);

	struct k_msgq *msgq;

	for (msgq = _k_msgq_list_start; msgq < _k_msgq_list_end; msgq++) {
		SYS_TRACING_OBJ_INIT(k_msgq, msgq);
	}
	return 0;
}

SYS_INIT(init_msgq_module, PRE_KERNEL_1, CONFIG_KERNEL_INIT_PRIORITY_OBJECTS);

#endif /* CONFIG_OBJECT_TRACING */

void k_msgq_init(struct k_msgq *q, char *buffer, size_t msg_size,
		 u32_t max_msgs)
{
	q->msg_size = msg_size;
	q->max_msgs = max_msgs;
	q->buffer_start = buffer;
	q->buffer_end = buffer + (max_msgs * msg_size);
	q->read_ptr = buffer;
	q->write_ptr = buffer;
	q->used_msgs = 0;
	q->flags = 0;
	z_waitq_init(&q->wait_q);
	q->lock = (struct k_spinlock) {};
	SYS_TRACING_OBJ_INIT(k_msgq, q);

	z_object_init(q);
}

int z_impl_k_msgq_alloc_init(struct k_msgq *q, size_t msg_size,
			    u32_t max_msgs)
{
	void *buffer;
	int ret;
	size_t total_size;

	if (__builtin_umul_overflow((unsigned int)msg_size, max_msgs,
				    (unsigned int *)&total_size)) {
		ret = -EINVAL;
	} else {
		buffer = z_thread_malloc(total_size);
		if (buffer != NULL) {
			k_msgq_init(q, buffer, msg_size, max_msgs);
			q->flags = K_MSGQ_FLAG_ALLOC;
			ret = 0;
		} else {
			ret = -ENOMEM;
		}
	}

	return ret;
}

#ifdef CONFIG_USERSPACE
Z_SYSCALL_HANDLER(k_msgq_alloc_init, q, msg_size, max_msgs)
{
	Z_OOPS(Z_SYSCALL_OBJ_NEVER_INIT(q, K_OBJ_MSGQ));

	return z_impl_k_msgq_alloc_init((struct k_msgq *)q, msg_size, max_msgs);
}
#endif

void k_msgq_cleanup(struct k_msgq *q)
{
	__ASSERT_NO_MSG(z_waitq_head(&q->wait_q) == NULL);

	if ((q->flags & K_MSGQ_FLAG_ALLOC) != 0) {
		k_free(q->buffer_start);
		q->flags &= ~K_MSGQ_FLAG_ALLOC;
	}
}


int z_impl_k_msgq_put(struct k_msgq *q, void *data, s32_t timeout)
{
	__ASSERT(!z_is_in_isr() || timeout == K_NO_WAIT, "");

	k_spinlock_key_t key = k_spin_lock(&q->lock);
	struct k_thread *pending_thread;
	int result;

	if (q->used_msgs < q->max_msgs) {
		/* message queue isn't full */
		pending_thread = z_unpend_first_thread(&q->wait_q);
		if (pending_thread != NULL) {
			/* give message to waiting thread */
			(void)memcpy(pending_thread->base.swap_data, data,
			       q->msg_size);
			/* wake up waiting thread */
			z_set_thread_return_value(pending_thread, 0);
			z_ready_thread(pending_thread);
			z_reschedule(&q->lock, key);
			return 0;
		} else {
			/* put message in queue */
			(void)memcpy(q->write_ptr, data, q->msg_size);
			q->write_ptr += q->msg_size;
			if (q->write_ptr == q->buffer_end) {
				q->write_ptr = q->buffer_start;
			}
			q->used_msgs++;
		}
		result = 0;
	} else if (timeout == K_NO_WAIT) {
		/* don't wait for message space to become available */
		result = -ENOMSG;
	} else {
		/* wait for put message success, failure, or timeout */
		_current->base.swap_data = data;
		return z_pend_curr(&q->lock, key, &q->wait_q, timeout);
	}

	k_spin_unlock(&q->lock, key);

	return result;
}

#ifdef CONFIG_USERSPACE
Z_SYSCALL_HANDLER(k_msgq_put, msgq_p, data, timeout)
{
	struct k_msgq *q = (struct k_msgq *)msgq_p;

	Z_OOPS(Z_SYSCALL_OBJ(q, K_OBJ_MSGQ));
	Z_OOPS(Z_SYSCALL_MEMORY_READ(data, q->msg_size));

	return z_impl_k_msgq_put(q, (void *)data, timeout);
}
#endif

void z_impl_k_msgq_get_attrs(struct k_msgq *q, struct k_msgq_attrs *attrs)
{
	attrs->msg_size = q->msg_size;
	attrs->max_msgs = q->max_msgs;
	attrs->used_msgs = q->used_msgs;
}

#ifdef CONFIG_USERSPACE
Z_SYSCALL_HANDLER(k_msgq_get_attrs, msgq_p, attrs)
{
	struct k_msgq *q = (struct k_msgq *)msgq_p;

	Z_OOPS(Z_SYSCALL_OBJ(q, K_OBJ_MSGQ));
	Z_OOPS(Z_SYSCALL_MEMORY_WRITE(attrs, sizeof(struct k_msgq_attrs)));
	z_impl_k_msgq_get_attrs(q, (struct k_msgq_attrs *) attrs);
	return 0;
}
#endif

int z_impl_k_msgq_get(struct k_msgq *q, void *data, s32_t timeout)
{
	__ASSERT(!z_is_in_isr() || timeout == K_NO_WAIT, "");

	k_spinlock_key_t key = k_spin_lock(&q->lock);
	struct k_thread *pending_thread;
	int result;

	if (q->used_msgs > 0) {
		/* take first available message from queue */
		(void)memcpy(data, q->read_ptr, q->msg_size);
		q->read_ptr += q->msg_size;
		if (q->read_ptr == q->buffer_end) {
			q->read_ptr = q->buffer_start;
		}
		q->used_msgs--;

		/* handle first thread waiting to write (if any) */
		pending_thread = z_unpend_first_thread(&q->wait_q);
		if (pending_thread != NULL) {
			/* add thread's message to queue */
			(void)memcpy(q->write_ptr, pending_thread->base.swap_data,
			       q->msg_size);
			q->write_ptr += q->msg_size;
			if (q->write_ptr == q->buffer_end) {
				q->write_ptr = q->buffer_start;
			}
			q->used_msgs++;

			/* wake up waiting thread */
			z_set_thread_return_value(pending_thread, 0);
			z_ready_thread(pending_thread);
			z_reschedule(&q->lock, key);
			return 0;
		}
		result = 0;
	} else if (timeout == K_NO_WAIT) {
		/* don't wait for a message to become available */
		result = -ENOMSG;
	} else {
		/* wait for get message success or timeout */
		_current->base.swap_data = data;
		return z_pend_curr(&q->lock, key, &q->wait_q, timeout);
	}

	k_spin_unlock(&q->lock, key);

	return result;
}

#ifdef CONFIG_USERSPACE
Z_SYSCALL_HANDLER(k_msgq_get, msgq_p, data, timeout)
{
	struct k_msgq *q = (struct k_msgq *)msgq_p;

	Z_OOPS(Z_SYSCALL_OBJ(q, K_OBJ_MSGQ));
	Z_OOPS(Z_SYSCALL_MEMORY_WRITE(data, q->msg_size));

	return z_impl_k_msgq_get(q, (void *)data, timeout);
}
#endif

int z_impl_k_msgq_peek(struct k_msgq *q, void *data)
{
	k_spinlock_key_t key = k_spin_lock(&q->lock);
	int result;

	if (q->used_msgs > 0) {
		/* take first available message from queue */
		(void)memcpy(data, q->read_ptr, q->msg_size);
		result = 0;
	} else {
		/* don't wait for a message to become available */
		result = -ENOMSG;
	}

	k_spin_unlock(&q->lock, key);

	return result;
}

#ifdef CONFIG_USERSPACE
Z_SYSCALL_HANDLER(k_msgq_peek, msgq_p, data)
{
	struct k_msgq *q = (struct k_msgq *)msgq_p;

	Z_OOPS(Z_SYSCALL_OBJ(q, K_OBJ_MSGQ));
	Z_OOPS(Z_SYSCALL_MEMORY_WRITE(data, q->msg_size));

	return z_impl_k_msgq_peek(q, (void *)data);
}
#endif

void z_impl_k_msgq_purge(struct k_msgq *q)
{
	k_spinlock_key_t key = k_spin_lock(&q->lock);
	struct k_thread *pending_thread;

	/* wake up any threads that are waiting to write */
	while ((pending_thread = z_unpend_first_thread(&q->wait_q)) != NULL) {
		z_set_thread_return_value(pending_thread, -ENOMSG);
		z_ready_thread(pending_thread);
	}

	q->used_msgs = 0;
	q->read_ptr = q->write_ptr;

	z_reschedule(&q->lock, key);
}

#ifdef CONFIG_USERSPACE
Z_SYSCALL_HANDLER1_SIMPLE_VOID(k_msgq_purge, K_OBJ_MSGQ, struct k_msgq *);
Z_SYSCALL_HANDLER1_SIMPLE(k_msgq_num_free_get, K_OBJ_MSGQ, struct k_msgq *);
Z_SYSCALL_HANDLER1_SIMPLE(k_msgq_num_used_get, K_OBJ_MSGQ, struct k_msgq *);
#endif
