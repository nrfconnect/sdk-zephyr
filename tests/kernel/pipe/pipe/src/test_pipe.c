/*
 * Copyright (c) 2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */


#include <ztest.h>

K_PIPE_DEFINE(test_pipe, 256, 4);
#define STACK_SIZE (512 + CONFIG_TEST_EXTRA_STACKSIZE)
#define PIPE_SIZE (256)

K_THREAD_STACK_DEFINE(stack_1, STACK_SIZE);

K_SEM_DEFINE(get_sem, 0, 1);
K_SEM_DEFINE(put_sem, 1, 1);
K_SEM_DEFINE(sync_sem, 0, 1);
K_SEM_DEFINE(multiple_send_sem, 0, 1);


ZTEST_BMEM u8_t tx_buffer[PIPE_SIZE];
ZTEST_BMEM u8_t rx_buffer[PIPE_SIZE];

#define TOTAL_ELEMENTS (sizeof(single_elements) / sizeof(struct pipe_sequence))
#define TOTAL_WAIT_ELEMENTS (sizeof(wait_elements) / \
			     sizeof(struct pipe_sequence))
#define TOTAL_TIMEOUT_ELEMENTS (sizeof(timeout_elements) / \
				sizeof(struct pipe_sequence))


/* Minimum tx/rx size*/
/* the pipe will always pass */
#define NO_CONSTRAINT (0U)

/* Pipe will atleast put one byte */
#define ATLEAST_1 (1U)

/* Pipe must put all data on the buffer */
#define ALL_BYTES (sizeof(tx_buffer))

#define RETURN_SUCCESS  (0)
#define TIMEOUT_VAL (K_MSEC(10))
#define TIMEOUT_200MSEC (K_MSEC(200))

/* encompasing structs */
struct pipe_sequence {
	u32_t size;
	u32_t min_size;
	u32_t sent_bytes;
	int return_value;
};

static const struct pipe_sequence single_elements[] = {
	{ 0, ALL_BYTES, 0, 0 },
	{ 1, ALL_BYTES, 1, RETURN_SUCCESS },
	{ PIPE_SIZE - 1, ALL_BYTES, PIPE_SIZE - 1, RETURN_SUCCESS },
	{ PIPE_SIZE, ALL_BYTES, PIPE_SIZE, RETURN_SUCCESS },
	{ PIPE_SIZE + 1, ALL_BYTES, 0, -EIO },
	/* minimum 1 byte */
	/* {0, ATLEAST_1, 0, -EIO}, */
	{ 1, ATLEAST_1, 1, RETURN_SUCCESS },
	{ PIPE_SIZE - 1, ATLEAST_1, PIPE_SIZE - 1, RETURN_SUCCESS },
	{ PIPE_SIZE, ATLEAST_1, PIPE_SIZE, RETURN_SUCCESS },
	{ PIPE_SIZE + 1, ATLEAST_1, PIPE_SIZE, RETURN_SUCCESS },
	/* /\* any number of bytes *\/ */
	{ 0, NO_CONSTRAINT, 0, 0 },
	{ 1, NO_CONSTRAINT, 1, RETURN_SUCCESS },
	{ PIPE_SIZE - 1, NO_CONSTRAINT, PIPE_SIZE - 1, RETURN_SUCCESS },
	{ PIPE_SIZE, NO_CONSTRAINT, PIPE_SIZE, RETURN_SUCCESS },
	{ PIPE_SIZE + 1, NO_CONSTRAINT, PIPE_SIZE, RETURN_SUCCESS }
};

static const struct pipe_sequence multiple_elements[] = {
	{ PIPE_SIZE / 3, ALL_BYTES, PIPE_SIZE / 3, RETURN_SUCCESS, },
	{ PIPE_SIZE / 3, ALL_BYTES, PIPE_SIZE / 3, RETURN_SUCCESS, },
	{ PIPE_SIZE / 3, ALL_BYTES, PIPE_SIZE / 3, RETURN_SUCCESS, },
	{ PIPE_SIZE / 3, ALL_BYTES, 0, -EIO },

	{ PIPE_SIZE / 3, ATLEAST_1, PIPE_SIZE / 3, RETURN_SUCCESS },
	{ PIPE_SIZE / 3, ATLEAST_1, PIPE_SIZE / 3, RETURN_SUCCESS },
	{ PIPE_SIZE / 3, ATLEAST_1, PIPE_SIZE / 3, RETURN_SUCCESS },
	{ PIPE_SIZE / 3, ATLEAST_1, 1, RETURN_SUCCESS },
	{ PIPE_SIZE / 3, ATLEAST_1, 0, -EIO },

	{ PIPE_SIZE / 3, NO_CONSTRAINT, PIPE_SIZE / 3, RETURN_SUCCESS },
	{ PIPE_SIZE / 3, NO_CONSTRAINT, PIPE_SIZE / 3, RETURN_SUCCESS },
	{ PIPE_SIZE / 3, NO_CONSTRAINT, PIPE_SIZE / 3, RETURN_SUCCESS },
	{ PIPE_SIZE / 3, NO_CONSTRAINT, 1, RETURN_SUCCESS },
	{ PIPE_SIZE / 3, NO_CONSTRAINT, 0, RETURN_SUCCESS }
};

static const struct pipe_sequence wait_elements[] = {
	{            1, ALL_BYTES,             1, RETURN_SUCCESS },
	{ PIPE_SIZE - 1, ALL_BYTES, PIPE_SIZE - 1, RETURN_SUCCESS },
	{    PIPE_SIZE, ALL_BYTES,     PIPE_SIZE, RETURN_SUCCESS },
	{ PIPE_SIZE + 1, ALL_BYTES, PIPE_SIZE + 1, RETURN_SUCCESS },

	{ PIPE_SIZE - 1, ATLEAST_1, PIPE_SIZE - 1, RETURN_SUCCESS },
	{    PIPE_SIZE, ATLEAST_1,     PIPE_SIZE, RETURN_SUCCESS },
	{ PIPE_SIZE + 1, ATLEAST_1, PIPE_SIZE + 1, RETURN_SUCCESS }
};

static const struct pipe_sequence timeout_elements[] = {
	{            0, ALL_BYTES, 0, 0 },
	{            1, ALL_BYTES, 0, -EAGAIN },
	{ PIPE_SIZE - 1, ALL_BYTES, 0, -EAGAIN },
	{    PIPE_SIZE, ALL_BYTES, 0, -EAGAIN },
	{ PIPE_SIZE + 1, ALL_BYTES, 0, -EAGAIN },

	{            1, ATLEAST_1, 0, -EAGAIN },
	{ PIPE_SIZE - 1, ATLEAST_1, 0, -EAGAIN },
	{    PIPE_SIZE, ATLEAST_1, 0, -EAGAIN },
	{ PIPE_SIZE + 1, ATLEAST_1, 0, -EAGAIN }
};

struct k_thread get_single_tid;

/* Helper functions */

u32_t rx_buffer_check(char *buffer, u32_t size)
{
	u32_t index;

	for (index = 0U; index < size; index++) {
		if (buffer[index] != (char) index) {
			printk("buffer[index] = %d index = %d\n",
			       buffer[index], (char) index);
			return index;
		}
	}

	return size;
}


/******************************************************************************/
void pipe_put_single(void)
{
	u32_t index;
	size_t written;
	int return_value;
	size_t min_xfer;

	for (index = 0U; index < TOTAL_ELEMENTS; index++) {
		k_sem_take(&put_sem, K_FOREVER);

		min_xfer = (single_elements[index].min_size == ALL_BYTES ?
			    single_elements[index].size :
			    single_elements[index].min_size);

		return_value = k_pipe_put(&test_pipe, &tx_buffer,
					  single_elements[index].size, &written,
					  min_xfer, K_NO_WAIT);

		zassert_true((return_value ==
			      single_elements[index].return_value),
			     " Return value of k_pipe_put missmatch at index = %d expected =%d received = %d\n",
			     index,
			     single_elements[index].return_value, return_value);

		zassert_true((written == single_elements[index].sent_bytes),
			     "Bytes written missmatch written is %d but expected is %d index = %d\n",
			     written,
			     single_elements[index].sent_bytes, index);

		k_sem_give(&get_sem);
	}

}

void pipe_get_single(void *p1, void *p2, void *p3)
{
	u32_t index;
	size_t read;
	int return_value;
	size_t min_xfer;

	for (index = 0U; index < TOTAL_ELEMENTS; index++) {
		k_sem_take(&get_sem, K_FOREVER);

		/* reset the rx buffer for the next interation */
		(void)memset(rx_buffer, 0, sizeof(rx_buffer));

		min_xfer = (single_elements[index].min_size == ALL_BYTES ?
			    single_elements[index].size :
			    single_elements[index].min_size);

		return_value = k_pipe_get(&test_pipe, &rx_buffer,
					  single_elements[index].size, &read,
					  min_xfer, K_NO_WAIT);


		zassert_true((return_value ==
			      single_elements[index].return_value),
			     "Return value of k_pipe_get missmatch at index = %d expected =%d received = %d\n",
			     index, single_elements[index].return_value,
			     return_value);

		zassert_true((read == single_elements[index].sent_bytes),
			     "Bytes read missmatch read is %d but expected is %d index = %d\n",
			     read, single_elements[index].sent_bytes, index);

		zassert_true(rx_buffer_check(rx_buffer, read) == read,
			     "Bytes read are not matching at index= %d\n expected =%d but received= %d",
			     index, read, rx_buffer_check(rx_buffer, read));
		k_sem_give(&put_sem);
	}
	k_sem_give(&sync_sem);
}

/******************************************************************************/
void pipe_put_multiple(void)
{
	u32_t index;
	size_t written;
	int return_value;
	size_t min_xfer;

	for (index = 0U; index < TOTAL_ELEMENTS; index++) {

		min_xfer = (multiple_elements[index].min_size == ALL_BYTES ?
			    multiple_elements[index].size :
			    multiple_elements[index].min_size);

		return_value = k_pipe_put(&test_pipe, &tx_buffer,
					  multiple_elements[index].size,
					  &written,
					  min_xfer, K_NO_WAIT);

		zassert_true((return_value ==
			      multiple_elements[index].return_value),
			     "Return value of k_pipe_put missmatch at index = %d expected =%d received = %d\n",
			     index,
			     multiple_elements[index].return_value,
			     return_value);

		zassert_true((written == multiple_elements[index].sent_bytes),
			     "Bytes written missmatch written is %d but expected is %d index = %d\n",
			     written,
			     multiple_elements[index].sent_bytes, index);
		if (return_value != RETURN_SUCCESS) {
			k_sem_take(&multiple_send_sem, K_FOREVER);
		}

	}

}

void pipe_get_multiple(void *p1, void *p2, void *p3)
{
	u32_t index;
	size_t read;
	int return_value;
	size_t min_xfer;

	for (index = 0U; index < TOTAL_ELEMENTS; index++) {


		/* reset the rx buffer for the next interation */
		(void)memset(rx_buffer, 0, sizeof(rx_buffer));

		min_xfer = (multiple_elements[index].min_size == ALL_BYTES ?
			    multiple_elements[index].size :
			    multiple_elements[index].min_size);

		return_value = k_pipe_get(&test_pipe, &rx_buffer,
					  multiple_elements[index].size, &read,
					  min_xfer, K_NO_WAIT);


		zassert_true((return_value ==
			      multiple_elements[index].return_value),
			     "Return value of k_pipe_get missmatch at index = %d expected =%d received = %d\n",
			     index, multiple_elements[index].return_value,
			     return_value);

		zassert_true((read == multiple_elements[index].sent_bytes),
			     "Bytes read missmatch read is %d but expected is %d index = %d\n",
			     read, multiple_elements[index].sent_bytes, index);

		zassert_true(rx_buffer_check(rx_buffer, read) == read,
			     "Bytes read are not matching at index= %d\n expected =%d but received= %d",
			     index, read, rx_buffer_check(rx_buffer, read));

		if (return_value != RETURN_SUCCESS) {
			k_sem_give(&multiple_send_sem);
		}

	}
	k_sem_give(&sync_sem);
}

/******************************************************************************/

void pipe_put_forever_wait(void)
{
	size_t written;
	int return_value;

	/* 1. fill the pipe. */
	return_value = k_pipe_put(&test_pipe, &tx_buffer,
				  PIPE_SIZE, &written,
				  PIPE_SIZE, K_FOREVER);

	zassert_true(return_value == RETURN_SUCCESS,
		     "k_pipe_put failed expected = 0 received = %d\n",
		     return_value);

	zassert_true(written == PIPE_SIZE,
		     "k_pipe_put written failed expected = %d received = %d\n",
		     PIPE_SIZE, written);


	/* wake up the get task */
	k_sem_give(&get_sem);
	/* 2. k_pipe_put() will force a context switch to the other thread. */
	return_value = k_pipe_put(&test_pipe, &tx_buffer,
				  PIPE_SIZE, &written,
				  PIPE_SIZE, K_FOREVER);

	zassert_true(return_value == RETURN_SUCCESS,
		     "k_pipe_put failed expected = 0 received = %d\n",
		     return_value);

	zassert_true(written == PIPE_SIZE,
		     "k_pipe_put written failed expected = %d received = %d\n",
		     PIPE_SIZE, written);

	/* 3. k_pipe_put() will force a context switch to the other thread. */
	return_value = k_pipe_put(&test_pipe, &tx_buffer,
				  PIPE_SIZE, &written,
				  ATLEAST_1, K_FOREVER);

	zassert_true(return_value == RETURN_SUCCESS,
		     "k_pipe_put failed expected = 0 received = %d\n",
		     return_value);

	zassert_true(written == PIPE_SIZE,
		     "k_pipe_put written failed expected = %d received = %d\n",
		     PIPE_SIZE, written);

}


void pipe_get_forever_wait(void *pi, void *p2, void *p3)
{
	size_t read;
	int return_value;

	/* get blocked until put forces the execution to come here */
	k_sem_take(&get_sem, K_FOREVER);

	/* k_pipe_get will force a context switch to the put function. */
	return_value = k_pipe_get(&test_pipe, &rx_buffer,
				  PIPE_SIZE, &read,
				  PIPE_SIZE, K_FOREVER);

	zassert_true(return_value == RETURN_SUCCESS,
		     "k_pipe_get failed expected = 0 received = %d\n",
		     return_value);

	zassert_true(read == PIPE_SIZE,
		     "k_pipe_put written failed expected = %d received = %d\n",
		     PIPE_SIZE, read);

	/* k_pipe_get will force a context switch to the other thread. */
	return_value = k_pipe_get(&test_pipe, &rx_buffer,
				  PIPE_SIZE, &read,
				  ATLEAST_1, K_FOREVER);

	zassert_true(return_value == RETURN_SUCCESS,
		     "k_pipe_get failed expected = 0 received = %d\n",
		     return_value);

	zassert_true(read == PIPE_SIZE,
		     "k_pipe_put written failed expected = %d received = %d\n",
		     PIPE_SIZE, read);

	/*3. last read to clear the pipe */
	return_value = k_pipe_get(&test_pipe, &rx_buffer,
				  PIPE_SIZE, &read,
				  ATLEAST_1, K_FOREVER);

	zassert_true(return_value == RETURN_SUCCESS,
		     "k_pipe_get failed expected = 0 received = %d\n",
		     return_value);

	zassert_true(read == PIPE_SIZE,
		     "k_pipe_put written failed expected = %d received = %d\n",
		     PIPE_SIZE, read);


	k_sem_give(&sync_sem);

}


/******************************************************************************/
void pipe_put_timeout(void)
{
	size_t written;
	int return_value;

	/* 1. fill the pipe. */
	return_value = k_pipe_put(&test_pipe, &tx_buffer,
				  PIPE_SIZE, &written,
				  PIPE_SIZE, TIMEOUT_VAL);

	zassert_true(return_value == RETURN_SUCCESS,
		     "k_pipe_put failed expected = 0 received = %d\n",
		     return_value);

	zassert_true(written == PIPE_SIZE,
		     "k_pipe_put written failed expected = %d received = %d\n",
		     PIPE_SIZE, written);


	/* pipe put cant be satisfied and thus timeout */
	return_value = k_pipe_put(&test_pipe, &tx_buffer,
				  PIPE_SIZE, &written,
				  PIPE_SIZE, TIMEOUT_VAL);

	zassert_true(return_value == -EAGAIN,
		     "k_pipe_put failed expected = -EAGAIN received = %d\n",
		     return_value);

	zassert_true(written == 0,
		     "k_pipe_put written failed expected = %d received = %d\n",
		     PIPE_SIZE, written);

	/* Try once more with 1 byte pipe put cant be satisfied and
	 * thus timeout.
	 */
	return_value = k_pipe_put(&test_pipe, &tx_buffer,
				  PIPE_SIZE, &written,
				  ATLEAST_1, TIMEOUT_VAL);

	zassert_true(return_value == -EAGAIN,
		     "k_pipe_put failed expected = -EAGAIN received = %d\n",
		     return_value);

	zassert_true(written == 0,
		     "k_pipe_put written failed expected = %d received = %d\n",
		     PIPE_SIZE, written);

	k_sem_give(&get_sem);

	/* 2. pipe_get thread will now accept this data  */
	return_value = k_pipe_put(&test_pipe, &tx_buffer,
				  PIPE_SIZE, &written,
				  PIPE_SIZE, TIMEOUT_VAL);

	zassert_true(return_value == RETURN_SUCCESS,
		     "k_pipe_put failed expected = 0 received = %d\n",
		     return_value);

	zassert_true(written == PIPE_SIZE,
		     "k_pipe_put written failed expected = %d received = %d\n",
		     PIPE_SIZE, written);

	/* 3. pipe_get thread will now accept this data  */
	return_value = k_pipe_put(&test_pipe, &tx_buffer,
				  PIPE_SIZE, &written,
				  ATLEAST_1, TIMEOUT_VAL);

	zassert_true(return_value == RETURN_SUCCESS,
		     "k_pipe_put failed expected = 0 received = %d\n",
		     return_value);

	zassert_true(written == PIPE_SIZE,
		     "k_pipe_put written failed expected = %d received = %d\n",
		     PIPE_SIZE, written);
}


void pipe_get_timeout(void *pi, void *p2, void *p3)
{
	size_t read;
	int return_value;

	/* get blocked until put forces the execution to come here */
	k_sem_take(&get_sem, K_FOREVER);

	/* k_pipe_get will do a context switch to the put function. */
	return_value = k_pipe_get(&test_pipe, &rx_buffer,
				  PIPE_SIZE, &read,
				  PIPE_SIZE, TIMEOUT_VAL);

	zassert_true(return_value == RETURN_SUCCESS,
		     "k_pipe_get failed expected = 0 received = %d\n",
		     return_value);

	zassert_true(read == PIPE_SIZE,
		     "k_pipe_put written failed expected = %d received = %d\n",
		     PIPE_SIZE, read);

	/* k_pipe_get will do a context switch to the put function. */
	return_value = k_pipe_get(&test_pipe, &rx_buffer,
				  PIPE_SIZE, &read,
				  ATLEAST_1, TIMEOUT_VAL);

	zassert_true(return_value == RETURN_SUCCESS,
		     "k_pipe_get failed expected = 0 received = %d\n",
		     return_value);

	zassert_true(read == PIPE_SIZE,
		     "k_pipe_put written failed expected = %d received = %d\n",
		     PIPE_SIZE, read);

	/* cleanup the pipe */
	return_value = k_pipe_get(&test_pipe, &rx_buffer,
				  PIPE_SIZE, &read,
				  ATLEAST_1, TIMEOUT_VAL);

	zassert_true(return_value == RETURN_SUCCESS,
		     "k_pipe_get failed expected = 0 received = %d\n",
		     return_value);

	zassert_true(read == PIPE_SIZE,
		     "k_pipe_put written failed expected = %d received = %d\n",
		     PIPE_SIZE, read);

	k_sem_give(&sync_sem);
}


/******************************************************************************/
void pipe_get_on_empty_pipe(void)
{
	size_t read;
	int return_value;
	u32_t read_size;
	u32_t size_array[] = { 1, PIPE_SIZE - 1, PIPE_SIZE, PIPE_SIZE + 1 };

	for (int i = 0; i < 4; i++) {
		read_size = size_array[i];
		return_value = k_pipe_get(&test_pipe, &rx_buffer,
					  read_size, &read,
					  read_size, K_NO_WAIT);

		zassert_true(return_value == -EIO,
			     "k_pipe_get failed expected = -EIO received = %d\n",
			     return_value);

		return_value = k_pipe_get(&test_pipe, &rx_buffer,
					  read_size, &read,
					  ATLEAST_1, K_NO_WAIT);

		zassert_true(return_value == -EIO,
			     "k_pipe_get failed expected = -EIO received = %d\n",
			     return_value);

		return_value = k_pipe_get(&test_pipe, &rx_buffer,
					  read_size, &read,
					  NO_CONSTRAINT, K_NO_WAIT);

		zassert_true(return_value == RETURN_SUCCESS,
			     "k_pipe_get failed expected = 0 received = %d\n",
			     return_value);

		zassert_true(read == 0,
			     "k_pipe_put written failed expected = %d received = %d\n",
			     PIPE_SIZE, read);
	}

}


/******************************************************************************/
void pipe_put_forever_timeout(void)
{
	u32_t index;
	size_t written;
	int return_value;
	size_t min_xfer;

	/* using this to synchronize the 2 threads  */
	k_sem_take(&put_sem, K_FOREVER);

	for (index = 0U; index < TOTAL_WAIT_ELEMENTS; index++) {

		min_xfer = (wait_elements[index].min_size == ALL_BYTES ?
			    wait_elements[index].size :
			    wait_elements[index].min_size);

		return_value = k_pipe_put(&test_pipe, &tx_buffer,
					  wait_elements[index].size, &written,
					  min_xfer, K_FOREVER);

		zassert_true((return_value ==
			      wait_elements[index].return_value),
			     "Return value of k_pipe_put missmatch at index = %d expected =%d received = %d\n",
			     index, wait_elements[index].return_value,
			     return_value);

		zassert_true((written == wait_elements[index].sent_bytes),
			     "Bytes written missmatch written is %d but expected is %d index = %d\n",
			     written, wait_elements[index].sent_bytes, index);

	}

}

void pipe_get_forever_timeout(void *p1, void *p2, void *p3)
{
	u32_t index;
	size_t read;
	int return_value;
	size_t min_xfer;

	/* using this to synchronize the 2 threads  */
	k_sem_give(&put_sem);
	for (index = 0U; index < TOTAL_WAIT_ELEMENTS; index++) {

		min_xfer = (wait_elements[index].min_size == ALL_BYTES ?
			    wait_elements[index].size :
			    wait_elements[index].min_size);

		return_value = k_pipe_get(&test_pipe, &rx_buffer,
					  wait_elements[index].size, &read,
					  min_xfer, K_FOREVER);


		zassert_true((return_value ==
			      wait_elements[index].return_value),
			     "Return value of k_pipe_get missmatch at index = %d expected =%d received = %d\n",
			     index, wait_elements[index].return_value,
			     return_value);

		zassert_true((read == wait_elements[index].sent_bytes),
			     "Bytes read missmatch read is %d but expected is %d index = %d\n",
			     read, wait_elements[index].sent_bytes, index);


	}
	k_sem_give(&sync_sem);
}



/******************************************************************************/
void pipe_put_get_timeout(void)
{
	u32_t index;
	size_t read;
	int return_value;
	size_t min_xfer;

	for (index = 0U; index < TOTAL_TIMEOUT_ELEMENTS; index++) {

		min_xfer = (timeout_elements[index].min_size == ALL_BYTES ?
			    timeout_elements[index].size :
			    timeout_elements[index].min_size);

		return_value = k_pipe_get(&test_pipe, &rx_buffer,
					  timeout_elements[index].size, &read,
					  min_xfer, TIMEOUT_200MSEC);


		zassert_true((return_value ==
			      timeout_elements[index].return_value),
			     "Return value of k_pipe_get missmatch at index = %d expected =%d received = %d\n",
			     index, timeout_elements[index].return_value,
			     return_value);

		zassert_true((read == timeout_elements[index].sent_bytes),
			     "Bytes read missmatch read is %d but expected is %d index = %d\n",
			     read, timeout_elements[index].sent_bytes, index);


	}

}

/******************************************************************************/
ZTEST_BMEM bool valid_fault;
void z_SysFatalErrorHandler(unsigned int reason, const NANO_ESF *pEsf)
{
	printk("Caught system error -- reason %d\n", reason);
	if (valid_fault) {
		valid_fault = false; /* reset back to normal */
		ztest_test_pass();
	} else {
		ztest_test_fail();
	}
#if !(defined(CONFIG_ARM) || defined(CONFIG_ARC))
	CODE_UNREACHABLE;
#endif

}
/******************************************************************************/
/* Test case entry points */
/**
 * @brief Verify pipe with 1 element insert
 * @ingroup kernel_pipe_tests
 * @see k_pipe_put()
 */
void test_pipe_on_single_elements(void)
{
	/* initialize the tx buffer */
	for (int i = 0; i < sizeof(tx_buffer); i++) {
		tx_buffer[i] = i;
	}

	k_thread_create(&get_single_tid, stack_1, STACK_SIZE,
			pipe_get_single, NULL, NULL, NULL,
			K_PRIO_PREEMPT(0), K_INHERIT_PERMS | K_USER, 0);

	pipe_put_single();
	k_sem_take(&sync_sem, K_FOREVER);
	k_thread_abort(&get_single_tid);
	ztest_test_pass();
}

/**
 * @brief Test when multiple items are present in the pipe
 * @ingroup kernel_pipe_tests
 * @see k_pipe_put()
 */
void test_pipe_on_multiple_elements(void)
{
	k_thread_create(&get_single_tid, stack_1, STACK_SIZE,
			pipe_get_multiple, NULL, NULL, NULL,
			K_PRIO_PREEMPT(0), K_INHERIT_PERMS | K_USER, 0);

	pipe_put_multiple();
	k_sem_take(&sync_sem, K_FOREVER);
	k_thread_abort(&get_single_tid);
	ztest_test_pass();
}

/**
 * @brief Test when multiple items are present with wait
 * @ingroup kernel_pipe_tests
 * @see k_pipe_put()
 */
void test_pipe_forever_wait(void)
{
	k_thread_create(&get_single_tid, stack_1, STACK_SIZE,
			pipe_get_forever_wait, NULL, NULL, NULL,
			K_PRIO_PREEMPT(0), K_INHERIT_PERMS | K_USER, 0);

	pipe_put_forever_wait();
	k_sem_take(&sync_sem, K_FOREVER);
	k_thread_abort(&get_single_tid);
	ztest_test_pass();
}

/**
 * @brief Test pipes with timeout
 * @ingroup kernel_pipe_tests
 * @see k_pipe_put()
 */
void test_pipe_timeout(void)
{
	k_thread_create(&get_single_tid, stack_1, STACK_SIZE,
			pipe_get_timeout, NULL, NULL, NULL,
			K_PRIO_PREEMPT(0), K_INHERIT_PERMS | K_USER, 0);

	pipe_put_timeout();
	k_sem_take(&sync_sem, K_FOREVER);
	k_thread_abort(&get_single_tid);
	ztest_test_pass();
}

/**
 * @brief Test pipe get from a empty pipe
 * @ingroup kernel_pipe_tests
 * @see k_pipe_get()
 */
void test_pipe_get_on_empty_pipe(void)
{
	pipe_get_on_empty_pipe();
	ztest_test_pass();
}

/**
 * @brief Test the pipe_get with K_FOREVER as timeout.
 * @details Testcase is similar to test_pipe_on_single_elements()
 * but with K_FOREVER as timeout.
 * @ingroup kernel_pipe_tests
 * @see k_pipe_put()
 */
void test_pipe_forever_timeout(void)
{
	k_thread_priority_set(k_current_get(), K_PRIO_PREEMPT(0));

	k_thread_create(&get_single_tid, stack_1, STACK_SIZE,
			pipe_get_forever_timeout, NULL, NULL, NULL,
			K_PRIO_PREEMPT(0), K_INHERIT_PERMS | K_USER, 0);

	pipe_put_forever_timeout();
	k_sem_take(&sync_sem, K_FOREVER);
	ztest_test_pass();
}

/**
 * @brief k_pipe_get timeout test
 * @ingroup kernel_pipe_tests
 * @see k_pipe_get()
 */
void test_pipe_get_timeout(void)
{
	pipe_put_get_timeout();

	ztest_test_pass();
}

/**
 * @brief Test pipe get of invalid size
 * @ingroup kernel_pipe_tests
 * @see k_pipe_get()
 */
#ifdef CONFIG_USERSPACE
/* userspace invalid size */
void test_pipe_get_invalid_size(void)
{
	size_t read;

	valid_fault = true;
	k_pipe_get(&test_pipe, &rx_buffer,
		   0, &read,
		   1, TIMEOUT_200MSEC);

	zassert_unreachable("fault didn't occur for min_xfer <= bytes_to_read");
}
#else
void test_pipe_get_invalid_size(void)
{
	ztest_test_skip();
}
#endif
