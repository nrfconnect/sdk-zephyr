/*
 * Copyright (c) 2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @addtogroup t_pipe
 * @{
 * @defgroup t_pipe_api test_pipe_api
 * @}
 */

#include <ztest.h>
extern void test_pipe_thread2thread(void);
extern void test_pipe_put_fail(void);
extern void test_pipe_get_fail(void);
extern void test_pipe_block_put(void);
extern void test_pipe_block_put_sema(void);
extern void test_pipe_get_put(void);

/* k objects */
extern struct k_pipe pipe, kpipe, put_get_pipe;
extern struct k_sem end_sema;
extern struct k_stack tstack;
extern struct k_thread tdata;
/*test case main entry*/
void test_main(void)
{
	k_thread_access_grant(k_current_get(),
			      &kpipe, &pipe, &end_sema, &tdata, &tstack,
			      &put_get_pipe, NULL);

	ztest_test_suite(test_pipe_api,
			 ztest_user_unit_test(test_pipe_thread2thread),
			 ztest_user_unit_test(test_pipe_put_fail),
			 ztest_user_unit_test(test_pipe_get_fail),
			 ztest_unit_test(test_pipe_block_put),
			 ztest_unit_test(test_pipe_block_put_sema),
			 ztest_unit_test(test_pipe_get_put));
	ztest_run_test_suite(test_pipe_api);
}
