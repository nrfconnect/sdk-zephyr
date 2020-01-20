/*
 * Copyright (c) 2017 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */


#include <kernel.h>
#include <string.h>
#include <sys/math_extras.h>
#include <sys/rb.h>
#include <kernel_structs.h>
#include <sys/sys_io.h>
#include <ksched.h>
#include <syscall.h>
#include <syscall_handler.h>
#include <device.h>
#include <init.h>
#include <stdbool.h>
#include <app_memory/app_memdomain.h>
#include <sys/libc-hooks.h>
#include <sys/mutex.h>
#include <inttypes.h>

#ifdef Z_LIBC_PARTITION_EXISTS
K_APPMEM_PARTITION_DEFINE(z_libc_partition);
#endif

/* TODO: Find a better place to put this. Since we pull the entire
 * lib..__modules__crypto__mbedtls.a  globals into app shared memory
 * section, we can't put this in zephyr_init.c of the mbedtls module.
 */
#ifdef CONFIG_MBEDTLS
K_APPMEM_PARTITION_DEFINE(k_mbedtls_partition);
#endif

#define LOG_LEVEL CONFIG_KERNEL_LOG_LEVEL
#include <logging/log.h>
LOG_MODULE_DECLARE(os);

/* The originally synchronization strategy made heavy use of recursive
 * irq_locking, which ports poorly to spinlocks which are
 * non-recursive.  Rather than try to redesign as part of
 * spinlockification, this uses multiple locks to preserve the
 * original semantics exactly.  The locks are named for the data they
 * protect where possible, or just for the code that uses them where
 * not.
 */
#ifdef CONFIG_DYNAMIC_OBJECTS
static struct k_spinlock lists_lock;       /* kobj rbtree/dlist */
static struct k_spinlock objfree_lock;     /* k_object_free */
#endif
static struct k_spinlock obj_lock;         /* kobj struct data */

#define MAX_THREAD_BITS		(CONFIG_MAX_THREAD_BYTES * 8)

#ifdef CONFIG_DYNAMIC_OBJECTS
extern u8_t _thread_idx_map[CONFIG_MAX_THREAD_BYTES];
#endif

static void clear_perms_cb(struct _k_object *ko, void *ctx_ptr);

const char *otype_to_str(enum k_objects otype)
{
	const char *ret;
	/* -fdata-sections doesn't work right except in very very recent
	 * GCC and these literal strings would appear in the binary even if
	 * otype_to_str was omitted by the linker
	 */
#ifdef CONFIG_LOG
	switch (otype) {
	/* otype-to-str.h is generated automatically during build by
	 * gen_kobject_list.py
	 */
#include <otype-to-str.h>
	default:
		ret = "?";
		break;
	}
#else
	ARG_UNUSED(otype);
	return NULL;
#endif
	return ret;
}

struct perm_ctx {
	int parent_id;
	int child_id;
	struct k_thread *parent;
};

#ifdef CONFIG_DYNAMIC_OBJECTS
struct dyn_obj {
	struct _k_object kobj;
	sys_dnode_t obj_list;
	struct rbnode node; /* must be immediately before data member */
	u8_t data[]; /* The object itself */
};

extern struct _k_object *z_object_gperf_find(void *obj);
extern void z_object_gperf_wordlist_foreach(_wordlist_cb_func_t func,
					     void *context);

static bool node_lessthan(struct rbnode *a, struct rbnode *b);

/*
 * Red/black tree of allocated kernel objects, for reasonably fast lookups
 * based on object pointer values.
 */
static struct rbtree obj_rb_tree = {
	.lessthan_fn = node_lessthan
};

/*
 * Linked list of allocated kernel objects, for iteration over all allocated
 * objects (and potentially deleting them during iteration).
 */
static sys_dlist_t obj_list = SYS_DLIST_STATIC_INIT(&obj_list);

/*
 * TODO: Write some hash table code that will replace both obj_rb_tree
 * and obj_list.
 */

static size_t obj_size_get(enum k_objects otype)
{
	size_t ret;

	switch (otype) {
#include <otype-to-size.h>
	default:
		ret = sizeof(struct device);
		break;
	}

	return ret;
}

static bool node_lessthan(struct rbnode *a, struct rbnode *b)
{
	return a < b;
}

static inline struct dyn_obj *node_to_dyn_obj(struct rbnode *node)
{
	return CONTAINER_OF(node, struct dyn_obj, node);
}

static struct dyn_obj *dyn_object_find(void *obj)
{
	struct rbnode *node;
	struct dyn_obj *ret;

	/* For any dynamically allocated kernel object, the object
	 * pointer is just a member of the conatining struct dyn_obj,
	 * so just a little arithmetic is necessary to locate the
	 * corresponding struct rbnode
	 */
	node = (struct rbnode *)((char *)obj - sizeof(struct rbnode));

	k_spinlock_key_t key = k_spin_lock(&lists_lock);
	if (rb_contains(&obj_rb_tree, node)) {
		ret = node_to_dyn_obj(node);
	} else {
		ret = NULL;
	}
	k_spin_unlock(&lists_lock, key);

	return ret;
}

/**
 * @internal
 *
 * @brief Allocate a new thread index for a new thread.
 *
 * This finds an unused thread index that can be assigned to a new
 * thread. If too many threads have been allocated, the kernel will
 * run out of indexes and this function will fail.
 *
 * Note that if an unused index is found, that index will be marked as
 * used after return of this function.
 *
 * @param tidx The new thread index if successful
 *
 * @return true if successful, false if failed
 **/
static bool thread_idx_alloc(uintptr_t *tidx)
{
	int i;
	int idx;
	int base;

	base = 0;
	for (i = 0; i < CONFIG_MAX_THREAD_BYTES; i++) {
		idx = find_lsb_set(_thread_idx_map[i]);

		if (idx != 0) {
			*tidx = base + (idx - 1);

			sys_bitfield_clear_bit((mem_addr_t)_thread_idx_map,
					       *tidx);

			/* Clear permission from all objects */
			z_object_wordlist_foreach(clear_perms_cb,
						   (void *)*tidx);

			return true;
		}

		base += 8;
	}

	return false;
}

/**
 * @internal
 *
 * @brief Free a thread index.
 *
 * This frees a thread index so it can be used by another
 * thread.
 *
 * @param tidx The thread index to be freed
 **/
static void thread_idx_free(uintptr_t tidx)
{
	/* To prevent leaked permission when index is recycled */
	z_object_wordlist_foreach(clear_perms_cb, (void *)tidx);

	sys_bitfield_set_bit((mem_addr_t)_thread_idx_map, tidx);
}

void *z_impl_k_object_alloc(enum k_objects otype)
{
	struct dyn_obj *dyn_obj;
	uintptr_t tidx;

	/* Stacks are not supported, we don't yet have mem pool APIs
	 * to request memory that is aligned
	 */
	__ASSERT(otype > K_OBJ_ANY && otype < K_OBJ_LAST &&
		 otype != K_OBJ__THREAD_STACK_ELEMENT,
		 "bad object type requested");

	dyn_obj = z_thread_malloc(sizeof(*dyn_obj) + obj_size_get(otype));
	if (dyn_obj == NULL) {
		LOG_WRN("could not allocate kernel object");
		return NULL;
	}

	dyn_obj->kobj.name = (char *)&dyn_obj->data;
	dyn_obj->kobj.type = otype;
	dyn_obj->kobj.flags = K_OBJ_FLAG_ALLOC;
	(void)memset(dyn_obj->kobj.perms, 0, CONFIG_MAX_THREAD_BYTES);

	/* Need to grab a new thread index for k_thread */
	if (otype == K_OBJ_THREAD) {
		if (!thread_idx_alloc(&tidx)) {
			k_free(dyn_obj);
			return NULL;
		}

		dyn_obj->kobj.data = tidx;
	}

	/* The allocating thread implicitly gets permission on kernel objects
	 * that it allocates
	 */
	z_thread_perms_set(&dyn_obj->kobj, _current);

	k_spinlock_key_t key = k_spin_lock(&lists_lock);

	rb_insert(&obj_rb_tree, &dyn_obj->node);
	sys_dlist_append(&obj_list, &dyn_obj->obj_list);
	k_spin_unlock(&lists_lock, key);

	return dyn_obj->kobj.name;
}

void k_object_free(void *obj)
{
	struct dyn_obj *dyn_obj;

	/* This function is intentionally not exposed to user mode.
	 * There's currently no robust way to track that an object isn't
	 * being used by some other thread
	 */

	k_spinlock_key_t key = k_spin_lock(&objfree_lock);

	dyn_obj = dyn_object_find(obj);
	if (dyn_obj != NULL) {
		rb_remove(&obj_rb_tree, &dyn_obj->node);
		sys_dlist_remove(&dyn_obj->obj_list);

		if (dyn_obj->kobj.type == K_OBJ_THREAD) {
			thread_idx_free(dyn_obj->kobj.data);
		}
	}
	k_spin_unlock(&objfree_lock, key);

	if (dyn_obj != NULL) {
		k_free(dyn_obj);
	}
}

struct _k_object *z_object_find(void *obj)
{
	struct _k_object *ret;

	ret = z_object_gperf_find(obj);

	if (ret == NULL) {
		struct dyn_obj *dynamic_obj;

		dynamic_obj = dyn_object_find(obj);
		if (dynamic_obj != NULL) {
			ret = &dynamic_obj->kobj;
		}
	}

	return ret;
}

void z_object_wordlist_foreach(_wordlist_cb_func_t func, void *context)
{
	struct dyn_obj *obj, *next;

	z_object_gperf_wordlist_foreach(func, context);

	k_spinlock_key_t key = k_spin_lock(&lists_lock);

	SYS_DLIST_FOR_EACH_CONTAINER_SAFE(&obj_list, obj, next, obj_list) {
		func(&obj->kobj, context);
	}
	k_spin_unlock(&lists_lock, key);
}
#endif /* CONFIG_DYNAMIC_OBJECTS */

static int thread_index_get(struct k_thread *thread)
{
	struct _k_object *ko;

	ko = z_object_find(thread);

	if (ko == NULL) {
		return -1;
	}

	return ko->data;
}

static void unref_check(struct _k_object *ko, uintptr_t index)
{
	k_spinlock_key_t key = k_spin_lock(&obj_lock);

	sys_bitfield_clear_bit((mem_addr_t)&ko->perms, index);

#ifdef CONFIG_DYNAMIC_OBJECTS
	struct dyn_obj *dyn_obj =
			CONTAINER_OF(ko, struct dyn_obj, kobj);

	if ((ko->flags & K_OBJ_FLAG_ALLOC) == 0U) {
		goto out;
	}

	for (int i = 0; i < CONFIG_MAX_THREAD_BYTES; i++) {
		if (ko->perms[i] != 0U) {
			goto out;
		}
	}

	/* This object has no more references. Some objects may have
	 * dynamically allocated resources, require cleanup, or need to be
	 * marked as uninitailized when all references are gone. What
	 * specifically needs to happen depends on the object type.
	 */
	switch (ko->type) {
	case K_OBJ_PIPE:
		k_pipe_cleanup((struct k_pipe *)ko->name);
		break;
	case K_OBJ_MSGQ:
		k_msgq_cleanup((struct k_msgq *)ko->name);
		break;
	case K_OBJ_STACK:
		k_stack_cleanup((struct k_stack *)ko->name);
		break;
	default:
		/* Nothing to do */
		break;
	}

	rb_remove(&obj_rb_tree, &dyn_obj->node);
	sys_dlist_remove(&dyn_obj->obj_list);
	k_free(dyn_obj);
out:
#endif
	k_spin_unlock(&obj_lock, key);
}

static void wordlist_cb(struct _k_object *ko, void *ctx_ptr)
{
	struct perm_ctx *ctx = (struct perm_ctx *)ctx_ptr;

	if (sys_bitfield_test_bit((mem_addr_t)&ko->perms, ctx->parent_id) &&
				  (struct k_thread *)ko->name != ctx->parent) {
		sys_bitfield_set_bit((mem_addr_t)&ko->perms, ctx->child_id);
	}
}

void z_thread_perms_inherit(struct k_thread *parent, struct k_thread *child)
{
	struct perm_ctx ctx = {
		thread_index_get(parent),
		thread_index_get(child),
		parent
	};

	if ((ctx.parent_id != -1) && (ctx.child_id != -1)) {
		z_object_wordlist_foreach(wordlist_cb, &ctx);
	}
}

void z_thread_perms_set(struct _k_object *ko, struct k_thread *thread)
{
	int index = thread_index_get(thread);

	if (index != -1) {
		sys_bitfield_set_bit((mem_addr_t)&ko->perms, index);
	}
}

void z_thread_perms_clear(struct _k_object *ko, struct k_thread *thread)
{
	int index = thread_index_get(thread);

	if (index != -1) {
		sys_bitfield_clear_bit((mem_addr_t)&ko->perms, index);
		unref_check(ko, index);
	}
}

static void clear_perms_cb(struct _k_object *ko, void *ctx_ptr)
{
	uintptr_t id = (uintptr_t)ctx_ptr;

	unref_check(ko, id);
}

void z_thread_perms_all_clear(struct k_thread *thread)
{
	uintptr_t index = thread_index_get(thread);

	if (index != -1) {
		z_object_wordlist_foreach(clear_perms_cb, (void *)index);
	}
}

static int thread_perms_test(struct _k_object *ko)
{
	int index;

	if ((ko->flags & K_OBJ_FLAG_PUBLIC) != 0U) {
		return 1;
	}

	index = thread_index_get(_current);
	if (index != -1) {
		return sys_bitfield_test_bit((mem_addr_t)&ko->perms, index);
	}
	return 0;
}

static void dump_permission_error(struct _k_object *ko)
{
	int index = thread_index_get(_current);
	LOG_ERR("thread %p (%d) does not have permission on %s %p",
		_current, index,
		otype_to_str(ko->type), ko->name);
	LOG_HEXDUMP_ERR(ko->perms, sizeof(ko->perms), "permission bitmap");
}

void z_dump_object_error(int retval, void *obj, struct _k_object *ko,
			enum k_objects otype)
{
	switch (retval) {
	case -EBADF:
		LOG_ERR("%p is not a valid %s", obj, otype_to_str(otype));
		break;
	case -EPERM:
		dump_permission_error(ko);
		break;
	case -EINVAL:
		LOG_ERR("%p used before initialization", obj);
		break;
	case -EADDRINUSE:
		LOG_ERR("%p %s in use", obj, otype_to_str(otype));
		break;
	default:
		/* Not handled error */
		break;
	}
}

void z_impl_k_object_access_grant(void *object, struct k_thread *thread)
{
	struct _k_object *ko = z_object_find(object);

	if (ko != NULL) {
		z_thread_perms_set(ko, thread);
	}
}

void k_object_access_revoke(void *object, struct k_thread *thread)
{
	struct _k_object *ko = z_object_find(object);

	if (ko != NULL) {
		z_thread_perms_clear(ko, thread);
	}
}

void z_impl_k_object_release(void *object)
{
	k_object_access_revoke(object, _current);
}

void k_object_access_all_grant(void *object)
{
	struct _k_object *ko = z_object_find(object);

	if (ko != NULL) {
		ko->flags |= K_OBJ_FLAG_PUBLIC;
	}
}

int z_object_validate(struct _k_object *ko, enum k_objects otype,
		       enum _obj_init_check init)
{
	if (unlikely((ko == NULL) ||
		(otype != K_OBJ_ANY && ko->type != otype))) {
		return -EBADF;
	}

	/* Manipulation of any kernel objects by a user thread requires that
	 * thread be granted access first, even for uninitialized objects
	 */
	if (unlikely(thread_perms_test(ko) == 0)) {
		return -EPERM;
	}

	/* Initialization state checks. _OBJ_INIT_ANY, we don't care */
	if (likely(init == _OBJ_INIT_TRUE)) {
		/* Object MUST be intialized */
		if (unlikely((ko->flags & K_OBJ_FLAG_INITIALIZED) == 0U)) {
			return -EINVAL;
		}
	} else if (init < _OBJ_INIT_TRUE) { /* _OBJ_INIT_FALSE case */
		/* Object MUST NOT be initialized */
		if (unlikely((ko->flags & K_OBJ_FLAG_INITIALIZED) != 0U)) {
			return -EADDRINUSE;
		}
	} else {
		/* _OBJ_INIT_ANY */
	}

	return 0;
}

void z_object_init(void *obj)
{
	struct _k_object *ko;

	/* By the time we get here, if the caller was from userspace, all the
	 * necessary checks have been done in z_object_validate(), which takes
	 * place before the object is initialized.
	 *
	 * This function runs after the object has been initialized and
	 * finalizes it
	 */

	ko = z_object_find(obj);
	if (ko == NULL) {
		/* Supervisor threads can ignore rules about kernel objects
		 * and may declare them on stacks, etc. Such objects will never
		 * be usable from userspace, but we shouldn't explode.
		 */
		return;
	}

	/* Allows non-initialization system calls to be made on this object */
	ko->flags |= K_OBJ_FLAG_INITIALIZED;
}

void z_object_recycle(void *obj)
{
	struct _k_object *ko = z_object_find(obj);

	if (ko != NULL) {
		(void)memset(ko->perms, 0, sizeof(ko->perms));
		z_thread_perms_set(ko, k_current_get());
		ko->flags |= K_OBJ_FLAG_INITIALIZED;
	}
}

void z_object_uninit(void *obj)
{
	struct _k_object *ko;

	/* See comments in z_object_init() */
	ko = z_object_find(obj);
	if (ko == NULL) {
		return;
	}

	ko->flags &= ~K_OBJ_FLAG_INITIALIZED;
}

/*
 * Copy to/from helper functions used in syscall handlers
 */
void *z_user_alloc_from_copy(const void *src, size_t size)
{
	void *dst = NULL;

	/* Does the caller in user mode have access to read this memory? */
	if (Z_SYSCALL_MEMORY_READ(src, size)) {
		goto out_err;
	}

	dst = z_thread_malloc(size);
	if (dst == NULL) {
		LOG_ERR("out of thread resource pool memory (%zu)", size);
		goto out_err;
	}

	(void)memcpy(dst, src, size);
out_err:
	return dst;
}

static int user_copy(void *dst, const void *src, size_t size, bool to_user)
{
	int ret = EFAULT;

	/* Does the caller in user mode have access to this memory? */
	if (to_user ? Z_SYSCALL_MEMORY_WRITE(dst, size) :
			Z_SYSCALL_MEMORY_READ(src, size)) {
		goto out_err;
	}

	(void)memcpy(dst, src, size);
	ret = 0;
out_err:
	return ret;
}

int z_user_from_copy(void *dst, const void *src, size_t size)
{
	return user_copy(dst, src, size, false);
}

int z_user_to_copy(void *dst, const void *src, size_t size)
{
	return user_copy(dst, src, size, true);
}

char *z_user_string_alloc_copy(const char *src, size_t maxlen)
{
	size_t actual_len;
	int err;
	char *ret = NULL;

	actual_len = z_user_string_nlen(src, maxlen, &err);
	if (err != 0) {
		goto out;
	}
	if (actual_len == maxlen) {
		/* Not NULL terminated */
		LOG_ERR("string too long %p (%zu)", src, actual_len);
		goto out;
	}
	if (size_add_overflow(actual_len, 1, &actual_len)) {
		LOG_ERR("overflow");
		goto out;
	}

	ret = z_user_alloc_from_copy(src, actual_len);

	/* Someone may have modified the source string during the above
	 * checks. Ensure what we actually copied is still terminated
	 * properly.
	 */
	if (ret != NULL) {
		ret[actual_len - 1] = '\0';
	}
out:
	return ret;
}

int z_user_string_copy(char *dst, const char *src, size_t maxlen)
{
	size_t actual_len;
	int ret, err;

	actual_len = z_user_string_nlen(src, maxlen, &err);
	if (err != 0) {
		ret = EFAULT;
		goto out;
	}
	if (actual_len == maxlen) {
		/* Not NULL terminated */
		LOG_ERR("string too long %p (%zu)", src, actual_len);
		ret = EINVAL;
		goto out;
	}
	if (size_add_overflow(actual_len, 1, &actual_len)) {
		LOG_ERR("overflow");
		ret = EINVAL;
		goto out;
	}

	ret = z_user_from_copy(dst, src, actual_len);

	/* See comment above in z_user_string_alloc_copy() */
	dst[actual_len - 1] = '\0';
out:
	return ret;
}

/*
 * Application memory region initialization
 */

extern char __app_shmem_regions_start[];
extern char __app_shmem_regions_end[];

void z_app_shmem_bss_zero(void)
{
	struct z_app_region *region, *end;

	end = (struct z_app_region *)&__app_shmem_regions_end;
	region = (struct z_app_region *)&__app_shmem_regions_start;

	for ( ; region < end; region++) {
		(void)memset(region->bss_start, 0, region->bss_size);
	}
}

/*
 * Default handlers if otherwise unimplemented
 */

static uintptr_t handler_bad_syscall(uintptr_t bad_id, uintptr_t arg2,
				     uintptr_t arg3, uintptr_t arg4,
				     uintptr_t arg5, uintptr_t arg6,
				     void *ssf)
{
	LOG_ERR("Bad system call id %" PRIuPTR " invoked", bad_id);
	arch_syscall_oops(_current_cpu->syscall_frame);
	CODE_UNREACHABLE; /* LCOV_EXCL_LINE */
}

static uintptr_t handler_no_syscall(uintptr_t arg1, uintptr_t arg2,
				    uintptr_t arg3, uintptr_t arg4,
				    uintptr_t arg5, uintptr_t arg6, void *ssf)
{
	LOG_ERR("Unimplemented system call");
	arch_syscall_oops(_current_cpu->syscall_frame);
	CODE_UNREACHABLE; /* LCOV_EXCL_LINE */
}

#include <syscall_dispatch.c>
