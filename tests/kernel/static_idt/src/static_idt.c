
/*
 * Copyright (c) 2012-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * test static IDT APIs
 *  Ensures interrupt and exception stubs are installed correctly.
 */

#include <zephyr.h>
#include <ztest.h>
#include <tc_util.h>
#include <arch/x86/segmentation.h>

#include <kernel_structs.h>
#if defined(__GNUC__)
#include "test_asm_inline_gcc.h"
#else
#include "test_asm_inline_other.h"
#endif

/* These vectors are somewhat arbitrary. We try and use unused vectors */
#define TEST_SOFT_INT 60
#define TEST_SPUR_INT 61


#define MY_STACK_SIZE 2048
#define MY_PRIORITY 5

K_THREAD_STACK_DEFINE(my_stack_area, MY_STACK_SIZE);
static struct k_thread my_thread;

/* externs */

/* The _idt_base_address symbol is generated via a linker script */

extern unsigned char _idt_base_address[];

extern void *int_stub;
NANO_CPU_INT_REGISTER(int_stub, -1, -1, TEST_SOFT_INT, 0);

static volatile int exc_handler_executed;
static volatile int int_handler_executed;
/* Assume the spurious interrupt handler will execute and abort the task */
static volatile int spur_handler_aborted_thread = 1;


/**
 * Handler to perform various actions from within an ISR context
 *
 * This routine is the ISR handler for _trigger_isr_handler().
 *
 * @return N/A
 */

void isr_handler(void)
{
	int_handler_executed++;
}

/**
 *
 * This is the handler for the divide by zero exception.
 *
 * The source of this divide-by-zero error comes from the following line in
 * main() ...
 *         error = error / exc_handler_executed;
 * Where exc_handler_executed is zero.
 * The disassembled code for it looks something like ....
 *         f7 fb                   idiv   %ecx
 * This handler is part of a test that is only interested in detecting the
 * error so that we know the exception connect code is working.  Therefore,
 * a very quick and dirty approach is taken for dealing with the exception;
 * we skip the offending instruction by adding 2 to the EIP.  (If nothing is
 * done, then control goes back to the offending instruction and an infinite
 * loop of divide-by-zero errors would be created.)
 *
 * @return N/A
 */

void exc_divide_error_handler(NANO_ESF *p_esf)
{
	p_esf->eip += 2;
	/* provide evidence that the handler executed */
	exc_handler_executed = 1;
}
_EXCEPTION_CONNECT_NOCODE(exc_divide_error_handler, IV_DIVIDE_ERROR);
extern void *_EXCEPTION_STUB_NAME(exc_divide_error_handler, IV_DIVIDE_ERROR);

/**
 * @brief Check the IDT.
 *
 * This test examines the IDT and verifies that the static interrupt and
 * exception stubs are installed at the correct place.
 *
 * @return TC_PASS on success, TC_FAIL on failure
 */

void test_idt_stub(void)
{
	struct segment_descriptor *p_idt_entry;
	u32_t offset;

	TC_PRINT("Testing to see if IDT has address of test stubs()\n");
	/* Check for the interrupt stub */
	p_idt_entry = (struct segment_descriptor *)
		      (_idt_base_address + (TEST_SOFT_INT << 3));
	offset = (u32_t)(&int_stub);
	zassert_equal(DTE_OFFSET(p_idt_entry), offset,
		      "Failed to find offset of int_stub (0x%x)"
		      " at vector %d\n", offset, TEST_SOFT_INT);

	/* Check for the exception stub */
	p_idt_entry = (struct segment_descriptor *)
		      (_idt_base_address + (IV_DIVIDE_ERROR << 3));
	offset = (u32_t)(&_EXCEPTION_STUB_NAME(exc_divide_error_handler, 0));
	zassert_equal(DTE_OFFSET(p_idt_entry), offset,
		      "Failed to find offset of exc stub (0x%x)"
		      " at vector %d\n", offset, IV_DIVIDE_ERROR);

	/*
	 * If the other fields are wrong, the system will crash when the
	 * exception and software interrupt are triggered so we don't check
	 * them.
	 */
}

/**
 * @brief Task to test spurious handlers
 *
 * @return 0
 */

void idt_spur_task(void *arg1, void *arg2, void *arg3)
{
	TC_PRINT("- Expect to see unhandled interrupt/exception message\n");

	_trigger_spur_handler();

	/* Shouldn't get here */
	spur_handler_aborted_thread = 0;

}

/**
 * @brief Entry point to static IDT tests
 *
 * This is the entry point to the static IDT tests.
 *
 * @return N/A
 */

void test_static_idt(void)
{
	volatile int error;     /* used to create a divide by zero error */


	TC_PRINT("Testing to see interrupt handler executes properly\n");
	_trigger_isr_handler();

	zassert_not_equal(int_handler_executed, 0,
			  "Interrupt handler did not execute\n");
	zassert_equal(int_handler_executed, 1,
		      "Interrupt handler executed more than once! (%d)\n",
		      int_handler_executed);

	TC_PRINT("Testing to see exception handler executes properly\n");

	/*
	 * Use exc_handler_executed instead of 0 to prevent the compiler
	 * issuing a 'divide by zero' warning.
	 */
	error = 32;     /* avoid static checker uninitialized warnings */
	error = error / exc_handler_executed;

	zassert_not_equal(exc_handler_executed, 0,
			  "Exception handler did not execute\n");
	zassert_equal(exc_handler_executed, 1,
		      "Exception handler executed more than once! (%d)\n",
		      exc_handler_executed);
	/*
	 * Start task to trigger the spurious interrupt handler
	 */
	TC_PRINT("Testing to see spurious handler executes properly\n");
	k_thread_create(&my_thread, my_stack_area, MY_STACK_SIZE,
			idt_spur_task, NULL, NULL, NULL,
			MY_PRIORITY, 0, K_NO_WAIT);

	/*
	 * The thread should not run past where the spurious interrupt is
	 * generated. Therefore spur_handler_aborted_thread should remain at 1.
	 */
	zassert_not_equal(spur_handler_aborted_thread, 0,
			  "Spurious handler did not execute as expected\n");
}

void test_main(void)
{
	ztest_test_suite(test_static_idt,
			 ztest_unit_test(test_idt_stub),
			 ztest_unit_test(test_static_idt)
			 );
	ztest_run_test_suite(test_static_idt);
}
