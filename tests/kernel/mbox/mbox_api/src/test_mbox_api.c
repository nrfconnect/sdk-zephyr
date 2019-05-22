/*
 * Copyright (c) 2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <ztest.h>

#define TIMEOUT 100
#if !defined(CONFIG_BOARD_QEMU_X86)
#define STACK_SIZE (512 + CONFIG_TEST_EXTRA_STACKSIZE)
#else
#define STACK_SIZE (640 + CONFIG_TEST_EXTRA_STACKSIZE)
#endif
#define MAIL_LEN 64

/**TESTPOINT: init via K_MBOX_DEFINE*/
K_MBOX_DEFINE(kmbox);
K_MEM_POOL_DEFINE(mpooltx, 8, MAIL_LEN, 1, 4);
K_MEM_POOL_DEFINE(mpoolrx, 8, MAIL_LEN, 1, 4);

static struct k_mbox mbox;

static k_tid_t sender_tid, receiver_tid, random_tid;

static K_THREAD_STACK_DEFINE(tstack, STACK_SIZE);
static K_THREAD_STACK_DEFINE(tstack_1, STACK_SIZE);
static K_THREAD_STACK_ARRAY_DEFINE(waiting_get_stack, 5, STACK_SIZE);
static struct k_thread tdata, async_tid, waiting_get_tid[5];

static struct k_sem end_sema, sync_sema;

static enum mmsg_type {
	PUT_GET_NULL = 0,
	PUT_GET_BUFFER,
	ASYNC_PUT_GET_BUFFER,
	ASYNC_PUT_GET_BLOCK,
	TARGET_SOURCE_THREAD_BUFFER,
	TARGET_SOURCE_THREAD_BLOCK,
	MAX_INFO_TYPE,
	INCORRECT_RECEIVER_TID,
	INCORRECT_TRANSMIT_TID,
	TIMED_OUT_MBOX_GET,
	BLOCK_GET_INVALID_POOL,
	MSG_TID_MISMATCH,
	BLOCK_GET_BUFF_TO_POOL,
	BLOCK_GET_BUFF_TO_SMALLER_POOL,
	DISPOSE_SIZE_0_MSG,
	CLEAN_UP_TX_POOL,
	ASYNC_PUT_TO_WAITING_GET,
	GET_WAITING_PUT_INCORRECT_TID,
	ASYNC_MULTIPLE_PUT,
	MULTIPLE_WAITING_GET
} info_type;

static char data[MAX_INFO_TYPE][MAIL_LEN] = {
	"send/recv an empty message",
	"send/recv msg using a buffer",
	"async send/recv msg using a buffer",
	"async send/recv msg using a memory block",
	"specify target/source thread, using a buffer",
	"specify target/source thread, using a memory block"
};

static void async_put_sema_give(void *p1, void *p2, void *p3)
{
	k_sem_give(&sync_sema);
}


static void mbox_get_waiting_thread(void *thread_number, void *pmbox, void *p3)
{
	struct k_mbox_msg mmsg = {0};

	switch ((int) thread_number) {
	case 0:
		mmsg.rx_source_thread = K_ANY;
		break;

	case 1:
		mmsg.rx_source_thread = random_tid;
		break;

	case 2:
		mmsg.rx_source_thread = receiver_tid;
		break;

	case 3:
		mmsg.rx_source_thread = &async_tid;
		break;

	case 4:
		mmsg.rx_source_thread = K_ANY;
		break;

	default:
		break;
	}

	mmsg.size = 0;
	zassert_true(k_mbox_get((struct k_mbox *)pmbox,
				&mmsg, NULL, K_FOREVER) == 0,
		     "Failure at thread number %d", thread_number);

}

static void tmbox_put(struct k_mbox *pmbox)
{
	struct k_mbox_msg mmsg = {0};

	switch (info_type) {
	case PUT_GET_NULL:
		/**TESTPOINT: mbox sync put empty message*/
		mmsg.info = PUT_GET_NULL;
		mmsg.size = 0;
		mmsg.tx_data = NULL;
		mmsg.tx_target_thread = K_ANY;
		k_mbox_put(pmbox, &mmsg, K_FOREVER);
		break;
	case PUT_GET_BUFFER:
	/*fall through*/
	case TARGET_SOURCE_THREAD_BUFFER:
		/**TESTPOINT: mbox sync put buffer*/
		mmsg.info = PUT_GET_BUFFER;
		mmsg.size = sizeof(data[info_type]);
		mmsg.tx_data = data[info_type];
		if (info_type == TARGET_SOURCE_THREAD_BUFFER) {
			mmsg.tx_target_thread = receiver_tid;
		} else {
			mmsg.tx_target_thread = K_ANY;
		}
		k_mbox_put(pmbox, &mmsg, K_FOREVER);
		break;
	case ASYNC_PUT_GET_BUFFER:
		/**TESTPOINT: mbox async put buffer*/
		mmsg.info = ASYNC_PUT_GET_BUFFER;
		mmsg.size = sizeof(data[info_type]);
		mmsg.tx_data = data[info_type];
		mmsg.tx_target_thread = K_ANY;
		k_mbox_async_put(pmbox, &mmsg, &sync_sema);
		/*wait for msg being taken*/
		k_sem_take(&sync_sema, K_FOREVER);
		break;
	case ASYNC_PUT_GET_BLOCK:
	/*fall through*/
	case TARGET_SOURCE_THREAD_BLOCK:
		/**TESTPOINT: mbox async put mem block*/
		mmsg.info = ASYNC_PUT_GET_BLOCK;
		mmsg.size = MAIL_LEN;
		mmsg.tx_data = NULL;
		zassert_equal(k_mem_pool_alloc(&mpooltx, &mmsg.tx_block,
					       MAIL_LEN, K_NO_WAIT), 0, NULL);
		memcpy(mmsg.tx_block.data, data[info_type], MAIL_LEN);
		if (info_type == TARGET_SOURCE_THREAD_BLOCK) {
			mmsg.tx_target_thread = receiver_tid;
		} else {
			mmsg.tx_target_thread = K_ANY;
		}
		k_mbox_async_put(pmbox, &mmsg, &sync_sema);
		/*wait for msg being taken*/
		k_sem_take(&sync_sema, K_FOREVER);
		break;
	case INCORRECT_TRANSMIT_TID:
		mmsg.tx_target_thread = random_tid;
		zassert_true(k_mbox_put(pmbox,
					&mmsg,
					K_NO_WAIT) == -ENOMSG, NULL);
		break;
	case BLOCK_GET_INVALID_POOL:
		/* To dispose of the rx msg using block get */
		mmsg.info = PUT_GET_NULL;
		mmsg.size = 0;
		mmsg.tx_data = NULL;
		mmsg.tx_target_thread = K_ANY;
		k_mbox_put(pmbox, &mmsg, K_FOREVER);
		break;
	case MSG_TID_MISMATCH:
		/* keep one msg in the queue and try to get with a wrong tid */
		mmsg.info = PUT_GET_NULL;
		mmsg.size = 0;
		mmsg.tx_data = NULL;
		mmsg.tx_target_thread = sender_tid;
		/* timeout because this msg wont be received with a _get*/
		k_mbox_put(pmbox, &mmsg, TIMEOUT);
		break;
	case BLOCK_GET_BUFF_TO_POOL:
		/* copy the tx buffer data onto a pool
		 * block via data_block_get
		 */
		mmsg.size = sizeof(data[1]);
		mmsg.tx_data = data[1];
		mmsg.tx_block.data = NULL;
		mmsg.tx_target_thread = K_ANY;
		zassert_true(k_mbox_put(pmbox, &mmsg, K_FOREVER) == 0, NULL);
		break;
	case BLOCK_GET_BUFF_TO_SMALLER_POOL:
		/* copy the tx buffer data onto a pool block via data_block_get
		 * but size is bigger than what the mem_pool can handle at
		 * that point of time
		 */
		mmsg.size = sizeof(data[1]) * 2;
		mmsg.tx_data = data[1];
		mmsg.tx_block.data = NULL;
		mmsg.tx_target_thread = K_ANY;
		zassert_true(k_mbox_put(pmbox, &mmsg, TIMEOUT) == 0, NULL);
		break;

	case DISPOSE_SIZE_0_MSG:
		/* Get a msg and dispose it by making the size = 0 */
		mmsg.size = 0;
		mmsg.tx_data = data[1];
		mmsg.tx_block.data = NULL;
		mmsg.tx_target_thread = K_ANY;
		zassert_true(k_mbox_put(pmbox, &mmsg, K_FOREVER) == 0, NULL);
		break;

	case CLEAN_UP_TX_POOL:
		/* Dispose of tx mem pool once we receive it */
		mmsg.size = MAIL_LEN;
		mmsg.tx_data = NULL;
		zassert_equal(k_mem_pool_alloc(&mpooltx, &mmsg.tx_block,
					       MAIL_LEN, K_NO_WAIT), 0, NULL);
		memcpy(mmsg.tx_block.data, data[0], MAIL_LEN);
		mmsg.tx_target_thread = K_ANY;
		zassert_true(k_mbox_put(pmbox, &mmsg, K_FOREVER) == 0, NULL);
		break;
	case ASYNC_PUT_TO_WAITING_GET:
		k_sem_take(&sync_sema, K_FOREVER);
		mmsg.size = sizeof(data[0]);
		mmsg.tx_data = data[0];
		mmsg.tx_target_thread = K_ANY;
		k_mbox_async_put(pmbox, &mmsg, NULL);
		break;
	case GET_WAITING_PUT_INCORRECT_TID:
		k_sem_take(&sync_sema, K_FOREVER);
		mmsg.size = sizeof(data[0]);
		mmsg.tx_data = data[0];
		mmsg.tx_target_thread = random_tid;
		k_mbox_async_put(pmbox, &mmsg, &sync_sema);
		break;
	case ASYNC_MULTIPLE_PUT:
		mmsg.size = sizeof(data[0]);
		mmsg.tx_data = data[0];
		mmsg.tx_target_thread = K_ANY;
		k_mbox_async_put(pmbox, &mmsg, NULL);

		mmsg.tx_data = data[1];
		mmsg.tx_target_thread = &async_tid;
		k_mbox_async_put(pmbox, &mmsg, NULL);

		mmsg.tx_data = data[1];
		mmsg.tx_target_thread = receiver_tid;
		k_mbox_async_put(pmbox, &mmsg, NULL);

		mmsg.tx_data = data[1];
		mmsg.tx_target_thread = &async_tid;
		k_mbox_async_put(pmbox, &mmsg, NULL);

		mmsg.tx_data = data[2];
		mmsg.tx_target_thread = receiver_tid;
		k_mbox_async_put(pmbox, &mmsg, &sync_sema);

		k_sem_take(&sync_sema, K_FOREVER);
		break;

	case MULTIPLE_WAITING_GET:
		k_sem_take(&sync_sema, K_FOREVER);

		mmsg.size = sizeof(data[0]);
		mmsg.tx_data = data[0];
		mmsg.tx_target_thread = K_ANY;
		k_mbox_put(pmbox, &mmsg, K_NO_WAIT);

		mmsg.tx_data = data[1];
		mmsg.tx_target_thread = &async_tid;
		k_mbox_put(pmbox, &mmsg, K_NO_WAIT);

		mmsg.tx_data = data[1];
		mmsg.tx_target_thread = receiver_tid;
		k_mbox_put(pmbox, &mmsg, K_NO_WAIT);

		mmsg.tx_data = data[1];
		mmsg.tx_target_thread = &async_tid;
		k_mbox_put(pmbox, &mmsg, K_NO_WAIT);

		mmsg.tx_data = data[2];
		mmsg.tx_target_thread = receiver_tid;
		k_mbox_put(pmbox, &mmsg, K_NO_WAIT);

		break;
	default:
		break;
	}
}

static void tmbox_get(struct k_mbox *pmbox)
{
	struct k_mbox_msg mmsg = {0};
	char rxdata[MAIL_LEN];
	struct k_mem_block rxblock;

	switch (info_type) {
	case PUT_GET_NULL:
		/**TESTPOINT: mbox sync get buffer*/
		mmsg.size = sizeof(rxdata);
		mmsg.rx_source_thread = K_ANY;
		/*verify return value*/
		zassert_true(k_mbox_get(pmbox, &mmsg, rxdata, K_FOREVER) == 0,
			     NULL);
		/*verify .info*/
		zassert_equal(mmsg.info, PUT_GET_NULL, NULL);
		/*verify .size*/
		zassert_equal(mmsg.size, 0, NULL);
		break;
	case PUT_GET_BUFFER:
	/*fall through*/
	case TARGET_SOURCE_THREAD_BUFFER:
		/**TESTPOINT: mbox sync get buffer*/
		mmsg.size = sizeof(rxdata);
		if (info_type == TARGET_SOURCE_THREAD_BUFFER) {
			mmsg.rx_source_thread = sender_tid;
		} else {
			mmsg.rx_source_thread = K_ANY;
		}
		zassert_true(k_mbox_get(pmbox, &mmsg, rxdata, K_FOREVER) == 0,
			     NULL);
		zassert_equal(mmsg.info, PUT_GET_BUFFER, NULL);
		zassert_equal(mmsg.size, sizeof(data[info_type]), NULL);
		/*verify rxdata*/
		zassert_true(memcmp(rxdata, data[info_type], MAIL_LEN) == 0,
			     NULL);
		break;
	case ASYNC_PUT_GET_BUFFER:
		/**TESTPOINT: mbox async get buffer*/
		mmsg.size = sizeof(rxdata);
		mmsg.rx_source_thread = K_ANY;
		zassert_true(k_mbox_get(pmbox, &mmsg, NULL, K_FOREVER) == 0,
			     NULL);
		zassert_equal(mmsg.info, ASYNC_PUT_GET_BUFFER, NULL);
		zassert_equal(mmsg.size, sizeof(data[info_type]), NULL);
		k_mbox_data_get(&mmsg, rxdata);
		zassert_true(memcmp(rxdata, data[info_type], MAIL_LEN) == 0,
			     NULL);
		break;
	case ASYNC_PUT_GET_BLOCK:
	/*fall through*/
	case TARGET_SOURCE_THREAD_BLOCK:
		/**TESTPOINT: mbox async get mem block*/
		mmsg.size = MAIL_LEN;
		if (info_type == TARGET_SOURCE_THREAD_BLOCK) {
			mmsg.rx_source_thread = sender_tid;
		} else {
			mmsg.rx_source_thread = K_ANY;
		}
		zassert_true(k_mbox_get(pmbox, &mmsg, NULL, K_FOREVER) == 0,
			     NULL);
		zassert_true(k_mbox_data_block_get
				     (&mmsg, &mpoolrx, &rxblock, K_FOREVER) == 0
			     , NULL);
		zassert_equal(mmsg.info, ASYNC_PUT_GET_BLOCK, NULL);
		zassert_equal(mmsg.size, MAIL_LEN, NULL);
		/*verify rxblock*/
		zassert_true(memcmp(rxblock.data, data[info_type], MAIL_LEN)
			     == 0, NULL);
		k_mem_pool_free(&rxblock);
		break;
	case INCORRECT_RECEIVER_TID:
		mmsg.rx_source_thread = random_tid;
		zassert_true(k_mbox_get
			     (pmbox, &mmsg, NULL, K_NO_WAIT) == -ENOMSG,
			     NULL);
		break;
	case TIMED_OUT_MBOX_GET:
		mmsg.rx_source_thread = random_tid;
		zassert_true(k_mbox_get(pmbox, &mmsg, NULL, TIMEOUT) == -EAGAIN,
			     NULL);
		break;
	case BLOCK_GET_INVALID_POOL:
		/* To dispose of the rx msg using block get */
		mmsg.rx_source_thread = K_ANY;
		zassert_true(k_mbox_get(pmbox, &mmsg, NULL, K_FOREVER) == 0,
			     NULL);
		zassert_true(k_mbox_data_block_get
			     (&mmsg, NULL, NULL, K_FOREVER) == 0,
			     NULL);
		break;
	case MSG_TID_MISMATCH:
		mmsg.rx_source_thread = random_tid;
		zassert_true(k_mbox_get
			     (pmbox, &mmsg, NULL, K_NO_WAIT) == -ENOMSG, NULL);
		break;

	case BLOCK_GET_BUFF_TO_POOL:
		/* copy the tx buffer data onto a pool
		 * block via data_block_get
		 */
		mmsg.rx_source_thread = K_ANY;
		mmsg.size = MAIL_LEN;
		zassert_true(k_mbox_get(pmbox, &mmsg, NULL, K_FOREVER) == 0,
			     NULL);
		zassert_true(k_mbox_data_block_get
			     (&mmsg, &mpoolrx, &rxblock, K_FOREVER) == 0, NULL);

		/* verfiy */
		zassert_true(memcmp(rxblock.data, data[1], MAIL_LEN)
			     == 0, NULL);
		/* free the block */
		k_mem_pool_free(&rxblock);

		break;
	case BLOCK_GET_BUFF_TO_SMALLER_POOL:
		/* copy the tx buffer data onto a smaller
		 * pool block via data_block_get
		 */
		mmsg.rx_source_thread = K_ANY;
		mmsg.size = MAIL_LEN * 2;
		zassert_true(k_mbox_get(pmbox, &mmsg, NULL, K_FOREVER) == 0,
			     NULL);

		zassert_true(k_mbox_data_block_get
			     (&mmsg, &mpoolrx, &rxblock, 1) == -EAGAIN, NULL);

		/* Now dispose of the block since the test case finished */
		k_mbox_data_get(&mmsg, NULL);
		break;

	case DISPOSE_SIZE_0_MSG:
		mmsg.rx_source_thread = K_ANY;
		mmsg.size = 0;
		zassert_true(k_mbox_get(pmbox, &mmsg, &rxdata, K_FOREVER) == 0,
			     NULL);
		break;

	case CLEAN_UP_TX_POOL:

		mmsg.rx_source_thread = K_ANY;
		mmsg.size = 0;
		zassert_true(k_mbox_get(pmbox, &mmsg, NULL, K_FOREVER) == 0,
			     NULL);
		break;
	case ASYNC_PUT_TO_WAITING_GET:

		/* Create a new thread to trigger the semaphore needed for the
		 * async put.
		 */
		k_thread_create(&async_tid, tstack_1, STACK_SIZE,
				       async_put_sema_give, NULL, NULL, NULL,
				       K_PRIO_PREEMPT(0), 0, K_NO_WAIT);
		mmsg.rx_source_thread = K_ANY;
		mmsg.size = 0;
		/* Here get is blocked until the thread we created releases
		 *  the semaphore and the async put completes it operation.
		 */
		zassert_true(k_mbox_get(pmbox, &mmsg, NULL, K_FOREVER) == 0,
			     NULL);
		break;
	case GET_WAITING_PUT_INCORRECT_TID:
		/* Create a new thread to trigger the semaphore needed for the
		 * async put.
		 */
		k_thread_create(&async_tid, tstack_1, STACK_SIZE,
				       async_put_sema_give, NULL, NULL, NULL,
				       K_PRIO_PREEMPT(0), 0, K_NO_WAIT);
		mmsg.rx_source_thread = &async_tid;
		mmsg.size = 0;
		/* Here the get is waiting for a async put to complete
		 * but the TIDs of the msgs doesn't match and hence
		 * causing a timeout.
		 */
		zassert_true(k_mbox_get(pmbox, &mmsg, NULL, TIMEOUT) == -EAGAIN,
			     NULL);
		/* clean up  */
		mmsg.rx_source_thread = K_ANY;
		zassert_true(k_mbox_get(pmbox, &mmsg, NULL, TIMEOUT) == 0,
			     NULL);
		break;

	case ASYNC_MULTIPLE_PUT:
		/* Async put has now populated the msgs. Now retrieve all
		 * the msgs from the mailbox.
		 */
		mmsg.rx_source_thread = K_ANY;
		mmsg.size = 0;
		/* get K_any msg */
		zassert_true(k_mbox_get(pmbox, &mmsg, NULL, TIMEOUT) == 0,
			     NULL);
		/* get the msg specific to receiver_tid */
		mmsg.rx_source_thread = sender_tid;
		zassert_true(k_mbox_get
			     (pmbox, &mmsg, NULL, TIMEOUT) == 0, NULL);

		/* get msg from async or random tid */
		mmsg.rx_source_thread = K_ANY;
		zassert_true(k_mbox_get
			     (pmbox, &mmsg, NULL, TIMEOUT) == 0, NULL);
		break;
	case MULTIPLE_WAITING_GET:
		/* Create 5 threads who will wait on a mbox_get. */
		for (u32_t i = 0; i < 5; i++) {
			k_thread_create(&waiting_get_tid[i],
					waiting_get_stack[i],
					STACK_SIZE,
					mbox_get_waiting_thread,
					(void *)i, pmbox, NULL,
					K_PRIO_PREEMPT(0), 0, K_NO_WAIT);
		}
		/* Create a new thread to trigger the semaphore needed for the
		 * async put. This will trigger the start of the msg transfer.
		 */
		k_thread_create(&async_tid, tstack_1, STACK_SIZE,
				       async_put_sema_give, NULL, NULL, NULL,
				       K_PRIO_PREEMPT(1), 0, K_NO_WAIT);
		break;

	default:
		break;
	}
}



/*entry of contexts*/
static void tmbox_entry(void *p1, void *p2, void *p3)
{
	tmbox_get((struct k_mbox *)p1);
	k_sem_give(&end_sema);
}

static void tmbox(struct k_mbox *pmbox)
{
	/*test case setup*/
	k_sem_reset(&end_sema);
	k_sem_reset(&sync_sema);

	/**TESTPOINT: thread-thread data passing via mbox*/
	sender_tid = k_current_get();
	receiver_tid = k_thread_create(&tdata, tstack, STACK_SIZE,
				       tmbox_entry, pmbox, NULL, NULL,
				       K_PRIO_PREEMPT(0), 0, 0);
	tmbox_put(pmbox);
	k_sem_take(&end_sema, K_FOREVER);

	/*test case teardown*/
	k_thread_abort(receiver_tid);
}

/*test cases*/
void test_mbox_kinit(void)
{
	/**TESTPOINT: init via k_mbox_init*/
	k_mbox_init(&mbox);
	k_sem_init(&end_sema, 0, 1);
	k_sem_init(&sync_sema, 0, 1);
}

void test_mbox_kdefine(void)
{
	info_type = PUT_GET_NULL;
	tmbox(&kmbox);
}

void test_mbox_put_get_null(void)
{
	info_type = PUT_GET_NULL;
	tmbox(&mbox);
}

void test_mbox_put_get_buffer(void)
{
	info_type = PUT_GET_BUFFER;
	tmbox(&mbox);
}

void test_mbox_async_put_get_buffer(void)
{
	info_type = ASYNC_PUT_GET_BUFFER;
	tmbox(&mbox);
}

void test_mbox_async_put_get_block(void)
{
	info_type = ASYNC_PUT_GET_BLOCK;
	tmbox(&mbox);
}

void test_mbox_target_source_thread_buffer(void)
{
	info_type = TARGET_SOURCE_THREAD_BUFFER;
	tmbox(&mbox);
}

void test_mbox_target_source_thread_block(void)
{
	info_type = TARGET_SOURCE_THREAD_BLOCK;
	tmbox(&mbox);
}

void test_mbox_incorrect_receiver_tid(void)
{
	info_type = INCORRECT_RECEIVER_TID;
	tmbox(&mbox);
}

void test_mbox_incorrect_transmit_tid(void)
{
	info_type = INCORRECT_TRANSMIT_TID;
	tmbox(&mbox);
}

void test_mbox_timed_out_mbox_get(void)
{
	info_type = TIMED_OUT_MBOX_GET;
	tmbox(&mbox);
}

void test_mbox_block_get_invalid_pool(void)
{
	info_type = BLOCK_GET_INVALID_POOL;
	tmbox(&mbox);
}

void test_mbox_msg_tid_mismatch(void)
{
	info_type = MSG_TID_MISMATCH;
	tmbox(&mbox);
}

void test_mbox_block_get_buff_to_pool(void)
{
	info_type = BLOCK_GET_BUFF_TO_POOL;
	tmbox(&mbox);
}

void test_mbox_block_get_buff_to_smaller_pool(void)
{
	info_type = BLOCK_GET_BUFF_TO_SMALLER_POOL;
	tmbox(&mbox);
}

void test_mbox_dispose_size_0_msg(void)
{
	info_type = DISPOSE_SIZE_0_MSG;
	tmbox(&mbox);
}

void test_mbox_clean_up_tx_pool(void)
{
	info_type = CLEAN_UP_TX_POOL;
	tmbox(&mbox);
}

void test_mbox_async_put_to_waiting_get(void)
{
	info_type = ASYNC_PUT_TO_WAITING_GET;
	tmbox(&mbox);
}

void test_mbox_get_waiting_put_incorrect_tid(void)
{
	info_type = GET_WAITING_PUT_INCORRECT_TID;
	tmbox(&mbox);
}

void test_mbox_async_multiple_put(void)
{
	info_type = ASYNC_MULTIPLE_PUT;
	tmbox(&mbox);
}

void test_mbox_multiple_waiting_get(void)
{
	info_type = MULTIPLE_WAITING_GET;
	tmbox(&mbox);
}
