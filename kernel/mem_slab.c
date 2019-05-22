/*
 * Copyright (c) 2016 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <kernel.h>
#include <kernel_structs.h>
#include <debug/object_tracing_common.h>
#include <toolchain.h>
#include <linker/sections.h>
#include <wait_q.h>
#include <misc/dlist.h>
#include <ksched.h>
#include <init.h>

extern struct k_mem_slab _k_mem_slab_list_start[];
extern struct k_mem_slab _k_mem_slab_list_end[];

static struct k_spinlock lock;

#ifdef CONFIG_OBJECT_TRACING
struct k_mem_slab *_trace_list_k_mem_slab;
#endif	/* CONFIG_OBJECT_TRACING */

/**
 * @brief Initialize kernel memory slab subsystem.
 *
 * Perform any initialization of memory slabs that wasn't done at build time.
 * Currently this just involves creating the list of free blocks for each slab.
 *
 * @return N/A
 */
static void create_free_list(struct k_mem_slab *slab)
{
	u32_t j;
	char *p;

	slab->free_list = NULL;
	p = slab->buffer;

	for (j = 0U; j < slab->num_blocks; j++) {
		*(char **)p = slab->free_list;
		slab->free_list = p;
		p += slab->block_size;
	}
}

/**
 * @brief Complete initialization of statically defined memory slabs.
 *
 * Perform any initialization that wasn't done at build time.
 *
 * @return N/A
 */
static int init_mem_slab_module(struct device *dev)
{
	ARG_UNUSED(dev);

	struct k_mem_slab *slab;

	for (slab = _k_mem_slab_list_start;
	     slab < _k_mem_slab_list_end;
	     slab++) {
		create_free_list(slab);
		SYS_TRACING_OBJ_INIT(k_mem_slab, slab);
		z_object_init(slab);
	}
	return 0;
}

SYS_INIT(init_mem_slab_module, PRE_KERNEL_1,
	 CONFIG_KERNEL_INIT_PRIORITY_OBJECTS);

void k_mem_slab_init(struct k_mem_slab *slab, void *buffer,
		    size_t block_size, u32_t num_blocks)
{
	/* block size must be word aligned */
	__ASSERT((slab->block_size & (sizeof(void *) - 1)) == 0,
		 "block size not word aligned");

	slab->num_blocks = num_blocks;
	slab->block_size = block_size;
	slab->buffer = buffer;
	slab->num_used = 0U;
	create_free_list(slab);
	z_waitq_init(&slab->wait_q);
	SYS_TRACING_OBJ_INIT(k_mem_slab, slab);

	z_object_init(slab);
}

int k_mem_slab_alloc(struct k_mem_slab *slab, void **mem, s32_t timeout)
{
	k_spinlock_key_t key = k_spin_lock(&lock);
	int result;

	/* block size must be word aligned */
	__ASSERT((slab->block_size & (sizeof(void *) - 1)) == 0,
		 "block size not word aligned");

	if (slab->free_list != NULL) {
		/* take a free block */
		*mem = slab->free_list;
		slab->free_list = *(char **)(slab->free_list);
		slab->num_used++;
		result = 0;
	} else if (timeout == K_NO_WAIT) {
		/* don't wait for a free block to become available */
		*mem = NULL;
		result = -ENOMEM;
	} else {
		/* wait for a free block or timeout */
		result = z_pend_curr(&lock, key, &slab->wait_q, timeout);
		if (result == 0) {
			*mem = _current->base.swap_data;
		}
		return result;
	}

	k_spin_unlock(&lock, key);

	return result;
}

void k_mem_slab_free(struct k_mem_slab *slab, void **mem)
{
	k_spinlock_key_t key = k_spin_lock(&lock);
	struct k_thread *pending_thread = z_unpend_first_thread(&slab->wait_q);

	if (pending_thread != NULL) {
		z_set_thread_return_value_with_data(pending_thread, 0, *mem);
		z_ready_thread(pending_thread);
		z_reschedule(&lock, key);
	} else {
		**(char ***)mem = slab->free_list;
		slab->free_list = *(char **)mem;
		slab->num_used--;
		k_spin_unlock(&lock, key);
	}
}
