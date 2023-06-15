/*
 * Copyright (c) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "posix_internal.h"
#include "pthread_sched.h"

#include <stdio.h>

#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/posix/pthread.h>
#include <zephyr/sys/slist.h>

#define PTHREAD_INIT_FLAGS	PTHREAD_CANCEL_ENABLE
#define PTHREAD_CANCELED	((void *) -1)

enum posix_thread_qid {
	/* ready to be started via pthread_create() */
	POSIX_THREAD_READY_Q,
	/* running */
	POSIX_THREAD_RUN_Q,
	/* exited (either joinable or detached) */
	POSIX_THREAD_DONE_Q,
};

BUILD_ASSERT((PTHREAD_CREATE_DETACHED == 0 || PTHREAD_CREATE_JOINABLE == 0) &&
	     (PTHREAD_CREATE_DETACHED == 1 || PTHREAD_CREATE_JOINABLE == 1));

BUILD_ASSERT((PTHREAD_CANCEL_ENABLE == 0 || PTHREAD_CANCEL_DISABLE == 0) &&
	     (PTHREAD_CANCEL_ENABLE == 1 || PTHREAD_CANCEL_DISABLE == 1));

static sys_dlist_t ready_q = SYS_DLIST_STATIC_INIT(&ready_q);
static sys_dlist_t run_q = SYS_DLIST_STATIC_INIT(&run_q);
static sys_dlist_t done_q = SYS_DLIST_STATIC_INIT(&done_q);
static struct posix_thread posix_thread_pool[CONFIG_MAX_PTHREAD_COUNT];
static struct k_spinlock pthread_pool_lock;

static K_MUTEX_DEFINE(pthread_once_lock);

static const struct pthread_attr init_pthread_attrs = {
	.priority = 0,
	.stack = NULL,
	.stacksize = 0,
	.flags = PTHREAD_INIT_FLAGS,
	.delayedstart = 0,
#if defined(CONFIG_PREEMPT_ENABLED)
	.schedpolicy = SCHED_RR,
#else
	.schedpolicy = SCHED_FIFO,
#endif
	.detachstate = PTHREAD_CREATE_JOINABLE,
	.initialized = true,
};

/*
 * We reserve the MSB to mark a pthread_t as initialized (from the
 * perspective of the application). With a linear space, this means that
 * the theoretical pthread_t range is [0,2147483647].
 */
BUILD_ASSERT(CONFIG_MAX_PTHREAD_COUNT < PTHREAD_OBJ_MASK_INIT,
	     "CONFIG_MAX_PTHREAD_COUNT is too high");

static inline size_t posix_thread_to_offset(struct posix_thread *t)
{
	return t - posix_thread_pool;
}

static inline size_t get_posix_thread_idx(pthread_t pth)
{
	return mark_pthread_obj_uninitialized(pth);
}

struct posix_thread *to_posix_thread(pthread_t pthread)
{
	k_spinlock_key_t key;
	struct posix_thread *t;
	bool actually_initialized;
	size_t bit = get_posix_thread_idx(pthread);

	/* if the provided thread does not claim to be initialized, its invalid */
	if (!is_pthread_obj_initialized(pthread)) {
		return NULL;
	}

	if (bit >= CONFIG_MAX_PTHREAD_COUNT) {
		return NULL;
	}

	t = &posix_thread_pool[bit];

	key = k_spin_lock(&pthread_pool_lock);
	/*
	 * Denote a pthread as "initialized" (i.e. allocated) if it is not in ready_q.
	 * This differs from other posix object allocation strategies because they use
	 * a bitarray to indicate whether an object has been allocated.
	 */
	actually_initialized =
		!(t->qid == POSIX_THREAD_READY_Q ||
		  (t->qid == POSIX_THREAD_DONE_Q && t->detachstate == PTHREAD_CREATE_DETACHED));
	k_spin_unlock(&pthread_pool_lock, key);

	if (!actually_initialized) {
		/* The thread claims to be initialized but is actually not */
		return NULL;
	}

	return &posix_thread_pool[bit];
}

pthread_t pthread_self(void)
{
	size_t bit;
	struct posix_thread *t;

	t = (struct posix_thread *)CONTAINER_OF(k_current_get(), struct posix_thread, thread);
	bit = posix_thread_to_offset(t);

	return mark_pthread_obj_initialized(bit);
}

static bool is_posix_policy_prio_valid(uint32_t priority, int policy)
{
	if (priority >= sched_get_priority_min(policy) &&
	    priority <= sched_get_priority_max(policy)) {
		return true;
	}

	return false;
}

static uint32_t zephyr_to_posix_priority(int32_t z_prio, int *policy)
{
	uint32_t prio;

	if (z_prio < 0) {
		*policy = SCHED_FIFO;
		prio = -1 * (z_prio + 1);
		__ASSERT_NO_MSG(prio < CONFIG_NUM_COOP_PRIORITIES);
	} else {
		*policy = SCHED_RR;
		prio = (CONFIG_NUM_PREEMPT_PRIORITIES - z_prio - 1);
		__ASSERT_NO_MSG(prio < CONFIG_NUM_PREEMPT_PRIORITIES);
	}

	return prio;
}

static int32_t posix_to_zephyr_priority(uint32_t priority, int policy)
{
	int32_t prio;

	if (policy == SCHED_FIFO) {
		/* Zephyr COOP priority starts from -1 */
		__ASSERT_NO_MSG(priority < CONFIG_NUM_COOP_PRIORITIES);
		prio = -1 * (priority + 1);
	} else {
		__ASSERT_NO_MSG(priority < CONFIG_NUM_PREEMPT_PRIORITIES);
		prio = (CONFIG_NUM_PREEMPT_PRIORITIES - priority - 1);
	}

	return prio;
}

/**
 * @brief Set scheduling parameter attributes in thread attributes object.
 *
 * See IEEE 1003.1
 */
int pthread_attr_setschedparam(pthread_attr_t *_attr, const struct sched_param *schedparam)
{
	struct pthread_attr *attr = (struct pthread_attr *)_attr;
	int priority = schedparam->sched_priority;

	if ((attr == NULL) || (attr->initialized == 0U) ||
	    (is_posix_policy_prio_valid(priority, attr->schedpolicy) == false)) {
		return EINVAL;
	}

	attr->priority = priority;
	return 0;
}

/**
 * @brief Set stack attributes in thread attributes object.
 *
 * See IEEE 1003.1
 */
int pthread_attr_setstack(pthread_attr_t *_attr, void *stackaddr, size_t stacksize)
{
	struct pthread_attr *attr = (struct pthread_attr *)_attr;

	if (stackaddr == NULL) {
		return EACCES;
	}

	attr->stack = stackaddr;
	attr->stacksize = stacksize;
	return 0;
}

static bool pthread_attr_is_valid(const struct pthread_attr *attr)
{
	/*
	 * FIXME: Pthread attribute must be non-null and it provides stack
	 * pointer and stack size. So even though POSIX 1003.1 spec accepts
	 * attrib as NULL but zephyr needs it initialized with valid stack.
	 */
	if (attr == NULL || attr->initialized == 0U || attr->stack == NULL ||
	    attr->stacksize == 0) {
		return false;
	}

	/* require a valid scheduler policy */
	if (!valid_posix_policy(attr->schedpolicy)) {
		return false;
	}

	/* require a valid detachstate */
	if (!(attr->detachstate == PTHREAD_CREATE_JOINABLE ||
	      attr->detachstate == PTHREAD_CREATE_DETACHED)) {
		return false;
	}

	/* we cannot create an essential thread (i.e. one that may not abort) */
	if ((attr->flags & K_ESSENTIAL) != 0) {
		return false;
	}

	return true;
}

static void posix_thread_finalize(struct posix_thread *t, void *retval)
{
	sys_snode_t *node_l;
	k_spinlock_key_t key;
	pthread_key_obj *key_obj;
	pthread_thread_data *thread_spec_data;

	SYS_SLIST_FOR_EACH_NODE(&t->key_list, node_l) {
		thread_spec_data = (pthread_thread_data *)node_l;
		if (thread_spec_data != NULL) {
			key_obj = thread_spec_data->key;
			if (key_obj->destructor != NULL) {
				(key_obj->destructor)(thread_spec_data->spec_data);
			}
		}
	}

	/* move thread from run_q to done_q */
	key = k_spin_lock(&pthread_pool_lock);
	sys_dlist_remove(&t->q_node);
	sys_dlist_append(&done_q, &t->q_node);
	t->qid = POSIX_THREAD_DONE_Q;
	t->retval = retval;
	k_spin_unlock(&pthread_pool_lock, key);

	/* abort the underlying k_thread */
	k_thread_abort(&t->thread);
}

FUNC_NORETURN
static void zephyr_thread_wrapper(void *arg1, void *arg2, void *arg3)
{
	int err;
	int barrier;
	void *(*fun_ptr)(void *arg) = arg2;
	struct posix_thread *t = CONTAINER_OF(k_current_get(), struct posix_thread, thread);

	if (IS_ENABLED(CONFIG_PTHREAD_CREATE_BARRIER)) {
		/* cross the barrier so that pthread_create() can continue */
		barrier = POINTER_TO_UINT(arg3);
		err = pthread_barrier_wait(&barrier);
		__ASSERT_NO_MSG(err == 0 || err == PTHREAD_BARRIER_SERIAL_THREAD);
	}

	posix_thread_finalize(t, fun_ptr(arg1));

	CODE_UNREACHABLE;
}

/**
 * @brief Create a new thread.
 *
 * Pthread attribute should not be NULL. API will return Error on NULL
 * attribute value.
 *
 * See IEEE 1003.1
 */
int pthread_create(pthread_t *th, const pthread_attr_t *_attr, void *(*threadroutine)(void *),
		   void *arg)
{
	int err;
	k_spinlock_key_t key;
	pthread_barrier_t barrier;
	struct posix_thread *safe_t;
	struct posix_thread *t = NULL;
	const struct pthread_attr *attr = (const struct pthread_attr *)_attr;

	if (!pthread_attr_is_valid(attr)) {
		return EINVAL;
	}

	key = k_spin_lock(&pthread_pool_lock);
	if (!sys_dlist_is_empty(&ready_q)) {
		/* spawn thread 't' directly from ready_q */
		t = CONTAINER_OF(sys_dlist_get(&ready_q), struct posix_thread, q_node);
	} else {
		SYS_DLIST_FOR_EACH_CONTAINER_SAFE(&done_q, t, safe_t, q_node) {
			if (t->detachstate == PTHREAD_CREATE_JOINABLE) {
				/* thread has not been joined yet */
				continue;
			}

			/* spawn thread 't' from done_q */
			sys_dlist_remove(&t->q_node);
			break;
		}
	}

	if (t != NULL) {
		/* initialize thread state */
		sys_dlist_append(&run_q, &t->q_node);
		t->qid = POSIX_THREAD_RUN_Q;
		t->detachstate = attr->detachstate;
		if ((BIT(_PTHREAD_CANCEL_POS) & attr->flags) != 0) {
			t->cancel_state = PTHREAD_CANCEL_ENABLE;
		}
		t->cancel_pending = false;
		sys_slist_init(&t->key_list);
	}
	k_spin_unlock(&pthread_pool_lock, key);

	if (IS_ENABLED(CONFIG_PTHREAD_CREATE_BARRIER)) {
		err = pthread_barrier_init(&barrier, NULL, 2);
		if (err != 0) {
			/* cannot allocate barrier. move thread back to ready_q */
			key = k_spin_lock(&pthread_pool_lock);
			sys_dlist_remove(&t->q_node);
			sys_dlist_append(&ready_q, &t->q_node);
			t->qid = POSIX_THREAD_READY_Q;
			k_spin_unlock(&pthread_pool_lock, key);
			t = NULL;
		}
	}

	if (t == NULL) {
		/* no threads are ready */
		return EAGAIN;
	}

	/* spawn the thread */
	k_thread_create(&t->thread, attr->stack, attr->stacksize, zephyr_thread_wrapper,
			(void *)arg, threadroutine,
			IS_ENABLED(CONFIG_PTHREAD_CREATE_BARRIER) ? UINT_TO_POINTER(barrier)
								       : NULL,
			posix_to_zephyr_priority(attr->priority, attr->schedpolicy), attr->flags,
			K_MSEC(attr->delayedstart));

	if (IS_ENABLED(CONFIG_PTHREAD_CREATE_BARRIER)) {
		/* wait for the spawned thread to cross our barrier */
		err = pthread_barrier_wait(&barrier);
		__ASSERT_NO_MSG(err == 0 || err == PTHREAD_BARRIER_SERIAL_THREAD);
		err = pthread_barrier_destroy(&barrier);
		__ASSERT_NO_MSG(err == 0);
	}

	/* finally provide the initialized thread to the caller */
	*th = mark_pthread_obj_initialized(posix_thread_to_offset(t));

	return 0;
}

/**
 * @brief Set cancelability State.
 *
 * See IEEE 1003.1
 */
int pthread_setcancelstate(int state, int *oldstate)
{
	bool cancel_pending;
	k_spinlock_key_t key;
	struct posix_thread *t;

	if (state != PTHREAD_CANCEL_ENABLE && state != PTHREAD_CANCEL_DISABLE) {
		return EINVAL;
	}

	t = to_posix_thread(pthread_self());
	if (t == NULL) {
		return EINVAL;
	}

	key = k_spin_lock(&pthread_pool_lock);
	*oldstate = t->cancel_state;
	t->cancel_state = state;
	cancel_pending = t->cancel_pending;
	k_spin_unlock(&pthread_pool_lock, key);

	if (state == PTHREAD_CANCEL_ENABLE && cancel_pending) {
		posix_thread_finalize(t, PTHREAD_CANCELED);
	}

	return 0;
}

/**
 * @brief Cancel execution of a thread.
 *
 * See IEEE 1003.1
 */
int pthread_cancel(pthread_t pthread)
{
	int cancel_state;
	k_spinlock_key_t key;
	struct posix_thread *t;

	t = to_posix_thread(pthread);
	if (t == NULL) {
		return ESRCH;
	}

	key = k_spin_lock(&pthread_pool_lock);
	t->cancel_pending = true;
	cancel_state = t->cancel_state;
	k_spin_unlock(&pthread_pool_lock, key);

	if (cancel_state == PTHREAD_CANCEL_ENABLE) {
		posix_thread_finalize(t, PTHREAD_CANCELED);
	}

	return 0;
}

/**
 * @brief Set thread scheduling policy and parameters.
 *
 * See IEEE 1003.1
 */
int pthread_setschedparam(pthread_t pthread, int policy, const struct sched_param *param)
{
	struct posix_thread *t = to_posix_thread(pthread);
	int new_prio;

	if (t == NULL) {
		return ESRCH;
	}

	if (!valid_posix_policy(policy)) {
		return EINVAL;
	}

	if (is_posix_policy_prio_valid(param->sched_priority, policy) == false) {
		return EINVAL;
	}

	new_prio = posix_to_zephyr_priority(param->sched_priority, policy);

	k_thread_priority_set(&t->thread, new_prio);
	return 0;
}

/**
 * @brief Initialise threads attribute object
 *
 * See IEEE 1003.1
 */
int pthread_attr_init(pthread_attr_t *attr)
{

	if (attr == NULL) {
		return ENOMEM;
	}

	(void)memcpy(attr, &init_pthread_attrs, sizeof(pthread_attr_t));

	return 0;
}

/**
 * @brief Get thread scheduling policy and parameters
 *
 * See IEEE 1003.1
 */
int pthread_getschedparam(pthread_t pthread, int *policy, struct sched_param *param)
{
	uint32_t priority;
	struct posix_thread *t;

	t = to_posix_thread(pthread);
	if (t == NULL) {
		return ESRCH;
	}

	priority = k_thread_priority_get(&t->thread);

	param->sched_priority = zephyr_to_posix_priority(priority, policy);
	return 0;
}

/**
 * @brief Dynamic package initialization
 *
 * See IEEE 1003.1
 */
int pthread_once(pthread_once_t *once, void (*init_func)(void))
{
	k_mutex_lock(&pthread_once_lock, K_FOREVER);

	if (once->is_initialized != 0 && once->init_executed == 0) {
		init_func();
		once->init_executed = 1;
	}

	k_mutex_unlock(&pthread_once_lock);

	return 0;
}

/**
 * @brief Terminate calling thread.
 *
 * See IEEE 1003.1
 */
FUNC_NORETURN
void pthread_exit(void *retval)
{
	k_spinlock_key_t key;
	struct posix_thread *self;

	self = to_posix_thread(pthread_self());
	if (self == NULL) {
		/* not a valid posix_thread */
		__ASSERT_NO_MSG(self != NULL);
		k_thread_abort(k_current_get());
	}

	/* Make a thread as cancelable before exiting */
	key = k_spin_lock(&pthread_pool_lock);
	self->cancel_state = PTHREAD_CANCEL_ENABLE;
	k_spin_unlock(&pthread_pool_lock, key);

	posix_thread_finalize(self, retval);
	CODE_UNREACHABLE;
}

/**
 * @brief Wait for a thread termination.
 *
 * See IEEE 1003.1
 */
int pthread_join(pthread_t pthread, void **status)
{
	int err;
	int ret;
	k_spinlock_key_t key;
	struct posix_thread *t;

	if (pthread == pthread_self()) {
		return EDEADLK;
	}

	t = to_posix_thread(pthread);
	if (t == NULL) {
		return ESRCH;
	}

	ret = 0;
	key = k_spin_lock(&pthread_pool_lock);
	if (t->detachstate != PTHREAD_CREATE_JOINABLE) {
		ret = EINVAL;
	} else if (t->qid == POSIX_THREAD_READY_Q) {
		/* marginal chance thread has moved to ready_q between to_posix_thread() and here */
		ret = ESRCH;
	} else {
		/*
		 * thread is joinable and is in run_q or done_q.
		 * let's ensure that the thread cannot be joined again after this point.
		 */
		t->detachstate = PTHREAD_CREATE_DETACHED;
		ret = 0;
	}
	k_spin_unlock(&pthread_pool_lock, key);

	if (ret == 0) {
		err = k_thread_join(&t->thread, K_FOREVER);
		/* other possibilities? */
		__ASSERT_NO_MSG(err == 0);
	}

	return 0;
}

/**
 * @brief Detach a thread.
 *
 * See IEEE 1003.1
 */
int pthread_detach(pthread_t pthread)
{
	int ret;
	k_spinlock_key_t key;
	struct posix_thread *t;
	enum posix_thread_qid qid;

	t = to_posix_thread(pthread);
	if (t == NULL) {
		return ESRCH;
	}

	key = k_spin_lock(&pthread_pool_lock);
	qid = t->qid;
	if (qid == POSIX_THREAD_READY_Q || t->detachstate != PTHREAD_CREATE_JOINABLE) {
		ret = EINVAL;
	} else {
		ret = 0;
		t->detachstate = PTHREAD_CREATE_DETACHED;
	}
	k_spin_unlock(&pthread_pool_lock, key);

	return ret;
}

/**
 * @brief Get detach state attribute in thread attributes object.
 *
 * See IEEE 1003.1
 */
int pthread_attr_getdetachstate(const pthread_attr_t *_attr, int *detachstate)
{
	const struct pthread_attr *attr = (const struct pthread_attr *)_attr;

	if ((attr == NULL) || (attr->initialized == 0U)) {
		return EINVAL;
	}

	*detachstate = attr->detachstate;
	return 0;
}

/**
 * @brief Set detach state attribute in thread attributes object.
 *
 * See IEEE 1003.1
 */
int pthread_attr_setdetachstate(pthread_attr_t *_attr, int detachstate)
{
	struct pthread_attr *attr = (struct pthread_attr *)_attr;

	if ((attr == NULL) || (attr->initialized == 0U) ||
	    (detachstate != PTHREAD_CREATE_DETACHED && detachstate != PTHREAD_CREATE_JOINABLE)) {
		return EINVAL;
	}

	attr->detachstate = detachstate;
	return 0;
}

/**
 * @brief Get scheduling policy attribute in Thread attributes.
 *
 * See IEEE 1003.1
 */
int pthread_attr_getschedpolicy(const pthread_attr_t *_attr, int *policy)
{
	const struct pthread_attr *attr = (const struct pthread_attr *)_attr;

	if ((attr == NULL) || (attr->initialized == 0U)) {
		return EINVAL;
	}

	*policy = attr->schedpolicy;
	return 0;
}

/**
 * @brief Set scheduling policy attribute in Thread attributes object.
 *
 * See IEEE 1003.1
 */
int pthread_attr_setschedpolicy(pthread_attr_t *_attr, int policy)
{
	struct pthread_attr *attr = (struct pthread_attr *)_attr;

	if ((attr == NULL) || (attr->initialized == 0U) || !valid_posix_policy(policy)) {
		return EINVAL;
	}

	attr->schedpolicy = policy;
	return 0;
}

/**
 * @brief Get stack size attribute in thread attributes object.
 *
 * See IEEE 1003.1
 */
int pthread_attr_getstacksize(const pthread_attr_t *_attr, size_t *stacksize)
{
	const struct pthread_attr *attr = (const struct pthread_attr *)_attr;

	if ((attr == NULL) || (attr->initialized == 0U)) {
		return EINVAL;
	}

	*stacksize = attr->stacksize;
	return 0;
}

/**
 * @brief Set stack size attribute in thread attributes object.
 *
 * See IEEE 1003.1
 */
int pthread_attr_setstacksize(pthread_attr_t *_attr, size_t stacksize)
{
	struct pthread_attr *attr = (struct pthread_attr *)_attr;

	if ((attr == NULL) || (attr->initialized == 0U)) {
		return EINVAL;
	}

	if (stacksize < PTHREAD_STACK_MIN) {
		return EINVAL;
	}

	attr->stacksize = stacksize;
	return 0;
}

/**
 * @brief Get stack attributes in thread attributes object.
 *
 * See IEEE 1003.1
 */
int pthread_attr_getstack(const pthread_attr_t *_attr, void **stackaddr, size_t *stacksize)
{
	const struct pthread_attr *attr = (const struct pthread_attr *)_attr;

	if ((attr == NULL) || (attr->initialized == 0U)) {
		return EINVAL;
	}

	*stackaddr = attr->stack;
	*stacksize = attr->stacksize;
	return 0;
}

/**
 * @brief Get thread attributes object scheduling parameters.
 *
 * See IEEE 1003.1
 */
int pthread_attr_getschedparam(const pthread_attr_t *_attr, struct sched_param *schedparam)
{
	struct pthread_attr *attr = (struct pthread_attr *)_attr;

	if ((attr == NULL) || (attr->initialized == 0U)) {
		return EINVAL;
	}

	schedparam->sched_priority = attr->priority;
	return 0;
}

/**
 * @brief Destroy thread attributes object.
 *
 * See IEEE 1003.1
 */
int pthread_attr_destroy(pthread_attr_t *_attr)
{
	struct pthread_attr *attr = (struct pthread_attr *)_attr;

	if ((attr != NULL) && (attr->initialized != 0U)) {
		attr->initialized = false;
		return 0;
	}

	return EINVAL;
}

int pthread_setname_np(pthread_t thread, const char *name)
{
#ifdef CONFIG_THREAD_NAME
	k_tid_t kthread;

	thread = get_posix_thread_idx(thread);
	if (thread >= CONFIG_MAX_PTHREAD_COUNT) {
		return ESRCH;
	}

	kthread = &posix_thread_pool[thread].thread;

	if (name == NULL) {
		return EINVAL;
	}

	return k_thread_name_set(kthread, name);
#else
	ARG_UNUSED(thread);
	ARG_UNUSED(name);
	return 0;
#endif
}

int pthread_getname_np(pthread_t thread, char *name, size_t len)
{
#ifdef CONFIG_THREAD_NAME
	k_tid_t kthread;

	thread = get_posix_thread_idx(thread);
	if (thread >= CONFIG_MAX_PTHREAD_COUNT) {
		return ESRCH;
	}

	if (name == NULL) {
		return EINVAL;
	}

	memset(name, '\0', len);
	kthread = &posix_thread_pool[thread].thread;
	return k_thread_name_copy(kthread, name, len - 1);
#else
	ARG_UNUSED(thread);
	ARG_UNUSED(name);
	ARG_UNUSED(len);
	return 0;
#endif
}

static int posix_thread_pool_init(void)
{
	size_t i;

	for (i = 0; i < CONFIG_MAX_PTHREAD_COUNT; ++i) {
		sys_dlist_append(&ready_q, &posix_thread_pool[i].q_node);
	}

	return 0;
}

SYS_INIT(posix_thread_pool_init, PRE_KERNEL_1, 0);
