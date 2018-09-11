/*
 * Copyright (c) 2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <ztest.h>
#include <irq_offload.h>
#include "test_mpool.h"
#include <kernel_internal.h>

/** TESTPOINT: Statically define and initialize a memory pool*/
K_MEM_POOL_DEFINE(kmpool, BLK_SIZE_MIN, BLK_SIZE_MAX, BLK_NUM_MAX, BLK_ALIGN);

void tmpool_alloc_free(void *data)
{
	ARG_UNUSED(data);
	struct k_mem_block block[BLK_NUM_MIN];

	for (int i = 0; i < BLK_NUM_MIN; i++) {
		/**
		 * TESTPOINT: This routine allocates a memory block from a
		 * memory pool.
		 */
		/**
		 * TESTPOINT: @retval 0 Memory allocated. The @a data field of
		 * the block descriptor is set to the starting address of the
		 * memory block.
		 */
		zassert_true(k_mem_pool_alloc(&kmpool, &block[i], BLK_SIZE_MIN,
					      K_NO_WAIT) == 0, NULL);
		zassert_not_null(block[i].data, NULL);
	}

	for (int i = 0; i < BLK_NUM_MIN; i++) {
		/**
		 * TESTPOINT: This routine releases a previously allocated
		 * memory block back to its memory pool.
		 */
		k_mem_pool_free(&block[i]);
		block[i].data = NULL;
	}

	/**
	 * TESTPOINT: The memory pool's buffer contains @a n_max blocks that are
	 * @a max_size bytes long.
	 */
	for (int i = 0; i < BLK_NUM_MAX; i++) {
		zassert_true(k_mem_pool_alloc(&kmpool, &block[i], BLK_SIZE_MAX,
					      K_NO_WAIT) == 0, NULL);
		zassert_not_null(block[i].data, NULL);
	}

	for (int i = 0; i < BLK_NUM_MAX; i++) {
		k_mem_pool_free(&block[i]);
		block[i].data = NULL;
	}
}

/*test cases*/
/**
 * @ingroup kernel_memory_pool_tests
 * @brief Verify alloc and free of different block sizes.
 *
 * @details The test is basically checking if allocation
 * happens for MAX_SIZE and MIN_SIZE defined in memory pool.
 *
 * @see k_mem_pool_alloc(), k_mem_pool_free()
 */
void test_mpool_alloc_free_thread(void)
{
	tmpool_alloc_free(NULL);
}

/**
 * @ingroup kernel_memory_pool_tests
 * @brief Test to validate alloc and free on IRQ context
 *
 * @details The test is run on IRQ context.
 * The test checks allocation of MAX_SIZE and MIN_SIZE
 * defined in memory pool.
 *
 * @see k_mem_pool_alloc(), k_mem_pool_free()
 */
void test_mpool_alloc_free_isr(void)
{
	irq_offload(tmpool_alloc_free, NULL);
}

/**
 * @ingroup kernel_memory_pool_tests
 * @brief Validates breaking a block into quarters feature
 *
 * @details The test case validates how a mem_pool provides
 * functionality to break a block into quarters and repeatedly
 * allocate and free the blocks.
 * @see k_mem_pool_alloc(), k_mem_pool_free()
 */
void test_mpool_alloc_size(void)
{
	struct k_mem_block block[BLK_NUM_MIN];
	size_t size = BLK_SIZE_MAX;
	int i = 0;

	/**TESTPOINT: The memory pool allows blocks to be repeatedly partitioned
	 * into quarters, down to blocks of @a min_size bytes long.
	 */
	while (size >= BLK_SIZE_MIN) {
		zassert_true(k_mem_pool_alloc(&kmpool, &block[i], size,
					      K_NO_WAIT) == 0, NULL);
		zassert_not_null(block[i].data, NULL);
		zassert_true((u32_t)(block[i].data) % BLK_ALIGN == 0, NULL);
		i++;
		size = size >> 2;
	}
	while (i--) {
		k_mem_pool_free(&block[i]);
		block[i].data = NULL;
	}

	i = 0;
	size = BLK_SIZE_MIN;
	/**TESTPOINT: To ensure that all blocks in the buffer are similarly
	 * aligned to this boundary, min_size must also be a multiple of align.
	 */
	while (size <= BLK_SIZE_MAX) {
		zassert_true(k_mem_pool_alloc(&kmpool, &block[i], size,
					      K_NO_WAIT) == 0, NULL);
		zassert_not_null(block[i].data, NULL);
		zassert_true((u32_t)(block[i].data) % BLK_ALIGN == 0, NULL);
		i++;
		size = size << 2;
	}
	while (i--) {
		k_mem_pool_free(&block[i]);
		block[i].data = NULL;
	}
}

/**
 * @see k_mem_pool_alloc(), k_mem_pool_free()
 * @brief Verify memory pool allocation with timeouts
 * @see k_mem_pool_alloc(), k_mem_pool_free()
 */
void test_mpool_alloc_timeout(void)
{
	struct k_mem_block block[BLK_NUM_MIN], fblock;
	s64_t tms;

	for (int i = 0; i < BLK_NUM_MIN; i++) {
		zassert_equal(k_mem_pool_alloc(&kmpool, &block[i], BLK_SIZE_MIN,
					       K_NO_WAIT), 0, NULL);
	}

	/** TESTPOINT: Use K_NO_WAIT to return without waiting*/
	/** TESTPOINT: @retval -ENOMEM Returned without waiting*/
	zassert_equal(k_mem_pool_alloc(&kmpool, &fblock, BLK_SIZE_MIN,
				       K_NO_WAIT), -ENOMEM, NULL);
	/** TESTPOINT: @retval -EAGAIN Waiting period timed out*/
	tms = k_uptime_get();
	zassert_equal(k_mem_pool_alloc(&kmpool, &fblock, BLK_SIZE_MIN, TIMEOUT),
		      -EAGAIN, NULL);
	/**
	 * TESTPOINT: Maximum time to wait for operation to complete (in
	 * milliseconds)
	 */
	zassert_true(k_uptime_delta(&tms) >= TIMEOUT, NULL);

	for (int i = 0; i < BLK_NUM_MIN; i++) {
		k_mem_pool_free(&block[i]);
		block[i].data = NULL;
	}
}

/**
 * @brief Validate allocation and free from system heap memory pool
 *
 * @see k_thread_system_pool_assign(), z_thread_malloc(), k_free()
 */
void test_sys_heap_mem_pool_assign(void)
{
	void *ptr;

	k_thread_system_pool_assign(k_current_get());
	ptr = (char *)z_thread_malloc(BLK_SIZE_MIN/2);
	zassert_not_null(ptr, "bytes allocation failed from system pool");
	k_free(ptr);

	zassert_is_null((char *)z_thread_malloc(BLK_SIZE_MAX * 2),
						"overflow check failed");
}
