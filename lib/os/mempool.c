/*
 * Copyright (c) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <kernel.h>
#include <string.h>
#include <misc/__assert.h>
#include <misc/mempool_base.h>
#include <misc/mempool.h>

#ifdef CONFIG_MISRA_SANE
#define LVL_ARRAY_SZ(n) (8 * sizeof(void *) / 2)
#else
#define LVL_ARRAY_SZ(n) (n)
#endif

static void *block_ptr(struct sys_mem_pool_base *p, size_t lsz, int block)
{
	return (u8_t *)p->buf + lsz * block;
}

static int block_num(struct sys_mem_pool_base *p, void *block, int sz)
{
	return ((u8_t *)block - (u8_t *)p->buf) / sz;
}

/* Places a 32 bit output pointer in word, and an integer bit index
 * within that word as the return value
 */
static int get_bit_ptr(struct sys_mem_pool_base *p, int level, int bn,
		       u32_t **word)
{
	u32_t *bitarray = level <= p->max_inline_level ?
		&p->levels[level].bits : p->levels[level].bits_p;

	*word = &bitarray[bn / 32];

	return bn & 0x1f;
}

static void set_free_bit(struct sys_mem_pool_base *p, int level, int bn)
{
	u32_t *word;
	int bit = get_bit_ptr(p, level, bn, &word);

	*word |= (1<<bit);
}

static void clear_free_bit(struct sys_mem_pool_base *p, int level, int bn)
{
	u32_t *word;
	int bit = get_bit_ptr(p, level, bn, &word);

	*word &= ~(1<<bit);
}

/* Returns all four of the free bits for the specified blocks
 * "partners" in the bottom 4 bits of the return value
 */
static int partner_bits(struct sys_mem_pool_base *p, int level, int bn)
{
	u32_t *word;
	int bit = get_bit_ptr(p, level, bn, &word);

	return (*word >> (4*(bit / 4))) & 0xf;
}

static size_t buf_size(struct sys_mem_pool_base *p)
{
	return p->n_max * p->max_sz;
}

static bool block_fits(struct sys_mem_pool_base *p, void *block, size_t bsz)
{
	return ((u8_t *)block + bsz - 1 - (u8_t *)p->buf) < buf_size(p);
}

void z_sys_mem_pool_base_init(struct sys_mem_pool_base *p)
{
	int i;
	size_t buflen = p->n_max * p->max_sz, sz = p->max_sz;
	u32_t *bits = (u32_t *)((u8_t *)p->buf + buflen);

	p->max_inline_level = -1;

	for (i = 0; i < p->n_levels; i++) {
		int nblocks = buflen / sz;

		sys_dlist_init(&p->levels[i].free_list);

		if (nblocks < 32) {
			p->max_inline_level = i;
		} else {
			p->levels[i].bits_p = bits;
			bits += (nblocks + 31)/32;
		}

		sz = _ALIGN4(sz / 4);
	}

	for (i = 0; i < p->n_max; i++) {
		void *block = block_ptr(p, p->max_sz, i);

		sys_dlist_append(&p->levels[0].free_list, block);
		set_free_bit(p, 0, i);
	}
}

/* A note on synchronization:
 *
 * For k_mem_pools which are interrupt safe, all manipulation of the actual
 * pool data happens in one of alloc_block()/free_block() or break_block().
 * All of these transition between a state where the caller "holds" a block
 * pointer that is marked used in the store and one where she doesn't (or else
 * they will fail, e.g. if there isn't a free block).  So that is the basic
 * operation that needs synchronization, which we can do piecewise as needed in
 * small one-block chunks to preserve latency.  At most (in free_block) a
 * single locked operation consists of four bit sets and dlist removals. If the
 * overall allocation operation fails, we just free the block we have (putting
 * a block back into the list cannot fail) and return failure.
 *
 * For user mode compatible sys_mem_pool pools, a semaphore is used at the API
 * level since using that does not introduce latency issues like locking
 * interrupts does.
 */

static inline int pool_irq_lock(struct sys_mem_pool_base *p)
{
	if (p->flags & SYS_MEM_POOL_KERNEL) {
		return irq_lock();
	} else {
		return 0;
	}
}

static inline void pool_irq_unlock(struct sys_mem_pool_base *p, int key)
{
	if (p->flags & SYS_MEM_POOL_KERNEL) {
		irq_unlock(key);
	}
}

static void *block_alloc(struct sys_mem_pool_base *p, int l, size_t lsz)
{
	sys_dnode_t *block;

	block = sys_dlist_get(&p->levels[l].free_list);
	if (block != NULL) {
		clear_free_bit(p, l, block_num(p, block, lsz));
	}
	return block;
}

/* Called with lock held */
static unsigned int bfree_recombine(struct sys_mem_pool_base *p, int level,
				    size_t *lsizes, int bn, unsigned int key)
{
	while (level >= 0) {
		int i, lsz = lsizes[level];
		void *block = block_ptr(p, lsz, bn);

		__ASSERT(block_fits(p, block, lsz), "");

		/* Put it back */
		set_free_bit(p, level, bn);
		sys_dlist_append(&p->levels[level].free_list, block);

		/* Relax the lock (might result in it being taken, which is OK!) */
		pool_irq_unlock(p, key);
		key = pool_irq_lock(p);

		/* Check if we can recombine its superblock, and repeat */
		if (level == 0 || partner_bits(p, level, bn) != 0xf) {
			return key;
		}

		for (i = 0; i < 4; i++) {
			int b = (bn & ~3) + i;

			if (block_fits(p, block_ptr(p, lsz, b), lsz)) {
				clear_free_bit(p, level, b);
				sys_dlist_remove(block_ptr(p, lsz, b));
			}
		}

		/* Free the larger block */
		level = level - 1;
		bn = bn / 4;
	}
	__ASSERT(0, "out of levels");
	return -1;
}

static void block_free(struct sys_mem_pool_base *p, int level,
		       size_t *lsizes, int bn)
{
	unsigned int key = pool_irq_lock(p);

	key = bfree_recombine(p, level, lsizes, bn, key);
	pool_irq_unlock(p, key);
}

/* Takes a block of a given level, splits it into four blocks of the
 * next smaller level, puts three into the free list as in
 * block_free() but without the need to check adjacent bits or
 * recombine, and returns the remaining smaller block.
 */
static void *block_break(struct sys_mem_pool_base *p, void *block, int l,
				size_t *lsizes)
{
	int i, bn;

	bn = block_num(p, block, lsizes[l]);

	for (i = 1; i < 4; i++) {
		int lbn = 4*bn + i;
		int lsz = lsizes[l + 1];
		void *block2 = (lsz * i) + (char *)block;

		set_free_bit(p, l + 1, lbn);
		if (block_fits(p, block2, lsz)) {
			sys_dlist_append(&p->levels[l + 1].free_list, block2);
		}
	}

	return block;
}

int z_sys_mem_pool_block_alloc(struct sys_mem_pool_base *p, size_t size,
			      u32_t *level_p, u32_t *block_p, void **data_p)
{
	int i, from_l, alloc_l = -1;
	unsigned int key;
	void *data = NULL;
	size_t lsizes[LVL_ARRAY_SZ(p->n_levels)];

	/* Walk down through levels, finding the one from which we
	 * want to allocate and the smallest one with a free entry
	 * from which we can split an allocation if needed.  Along the
	 * way, we populate an array of sizes for each level so we
	 * don't need to waste RAM storing it.
	 */
	lsizes[0] = _ALIGN4(p->max_sz);
	for (i = 0; i < p->n_levels; i++) {
		if (i > 0) {
			lsizes[i] = _ALIGN4(lsizes[i-1] / 4);
		}

		if (lsizes[i] < size) {
			break;
		}

		alloc_l = i;
	}

	if (alloc_l < 0) {
		*data_p = NULL;
		return -ENOMEM;
	}

	/* Now walk back down the levels (i.e. toward bigger sizes)
	 * looking for an available block.  Start at the smallest
	 * enclosing block found above (note that because that loop
	 * was done without synchronization, it may no longer be
	 * available!) as a useful optimization.  Note that the
	 * removal of the block from the list and the re-addition of
	 * its the three unused children needs to be performed
	 * atomically, otherwise we open up a situation where we can
	 * "steal" the top level block of the whole heap, causing a
	 * spurious -ENOMEM.
	 */
	key = pool_irq_lock(p);
	for (i = alloc_l; i >= 0; i--) {
		data = block_alloc(p, i, lsizes[i]);

		/* Found one.  Iteratively break it down to the size
		 * we need.  Note that we relax the lock to allow a
		 * pending interrupt to fire so we don't hurt latency
		 * by locking the full loop.
		 */
		if (data != NULL) {
			for (from_l = i; from_l < alloc_l; from_l++) {
				data = block_break(p, data, from_l, lsizes);
				pool_irq_unlock(p, key);
				key = pool_irq_lock(p);
			}
			break;
		}
	}
	pool_irq_unlock(p, key);

	*data_p = data;

	if (data == NULL) {
		return -ENOMEM;
	}

	*level_p = alloc_l;
	*block_p = block_num(p, data, lsizes[alloc_l]);

	return 0;
}

void z_sys_mem_pool_block_free(struct sys_mem_pool_base *p, u32_t level,
			      u32_t block)
{
	size_t lsizes[LVL_ARRAY_SZ(p->n_levels)];
	int i;

	/* As in z_sys_mem_pool_block_alloc(), we build a table of level sizes
	 * to avoid having to store it in precious RAM bytes.
	 * Overhead here is somewhat higher because block_free()
	 * doesn't inherently need to traverse all the larger
	 * sublevels.
	 */
	lsizes[0] = _ALIGN4(p->max_sz);
	for (i = 1; i <= level; i++) {
		lsizes[i] = _ALIGN4(lsizes[i-1] / 4);
	}

	block_free(p, level, lsizes, block);
}

/*
 * Functions specific to user-mode blocks
 */

void *sys_mem_pool_alloc(struct sys_mem_pool *p, size_t size)
{
	struct sys_mem_pool_block *blk;
	u32_t level, block;
	char *ret;

	sys_mutex_lock(&p->mutex, K_FOREVER);

	size += sizeof(struct sys_mem_pool_block);
	if (z_sys_mem_pool_block_alloc(&p->base, size, &level, &block,
				      (void **)&ret)) {
		ret = NULL;
		goto out;
	}

	blk = (struct sys_mem_pool_block *)ret;
	blk->level = level;
	blk->block = block;
	blk->pool = p;
	ret += sizeof(*blk);
out:
	sys_mutex_unlock(&p->mutex);
	return ret;
}

void sys_mem_pool_free(void *ptr)
{
	struct sys_mem_pool_block *blk;
	struct sys_mem_pool *p;

	if (ptr == NULL) {
		return;
	}

	blk = (struct sys_mem_pool_block *)((char *)ptr - sizeof(*blk));
	p = blk->pool;

	sys_mutex_lock(&p->mutex, K_FOREVER);
	z_sys_mem_pool_block_free(&p->base, blk->level, blk->block);
	sys_mutex_unlock(&p->mutex);
}

