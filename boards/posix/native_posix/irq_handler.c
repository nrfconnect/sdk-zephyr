/*
 * Copyright (c) 2014 Wind River Systems, Inc.
 * Copyright (c) 2017 Oticon A/S
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * SW side of the IRQ handling
 */

#include <stdint.h>
#include "irq_handler.h"
#include "irq_offload.h"
#include "kernel_structs.h"
#include "kernel_internal.h"
#include "kswap.h"
#include "irq_ctrl.h"
#include "posix_core.h"
#include "board_soc.h"
#include "sw_isr_table.h"
#include "soc.h"
#include <tracing.h>

typedef void (*normal_irq_f_ptr)(void *);
typedef int (*direct_irq_f_ptr)(void);

typedef struct _isr_list isr_table_entry_t;
static isr_table_entry_t irq_vector_table[N_IRQS] = { { 0 } };

static int currently_running_irq = -1;

static inline void vector_to_irq(int irq_nbr, int *may_swap)
{
	/*
	 * As in this architecture an irq (code) executes in 0 time,
	 * it is a bit senseless to call _int_latency_start/stop()
	 */
	/* _int_latency_start(); */

	sys_trace_isr_enter();

	if (irq_vector_table[irq_nbr].func == NULL) { /* LCOV_EXCL_BR_LINE */
		/* LCOV_EXCL_START */
		posix_print_error_and_exit("Received irq %i without a "
					"registered handler\n",
					irq_nbr);
		/* LCOV_EXCL_STOP */
	} else {
		if (irq_vector_table[irq_nbr].flags & ISR_FLAG_DIRECT) {
			*may_swap |= ((direct_irq_f_ptr)
					irq_vector_table[irq_nbr].func)();
		} else {
#ifdef CONFIG_SYS_POWER_MANAGEMENT
			posix_irq_check_idle_exit();
#endif
			((normal_irq_f_ptr)irq_vector_table[irq_nbr].func)
					(irq_vector_table[irq_nbr].param);
			*may_swap = 1;
		}
	}
	/* _int_latency_stop(); */
}

/**
 * When an interrupt is raised, this function is called to handle it and, if
 * needed, swap to a re-enabled thread
 *
 * Note that even that this function is executing in a Zephyr thread,  it is
 * effectively the model of the interrupt controller passing context to the IRQ
 * handler and therefore its priority handling
 */
void posix_irq_handler(void)
{
	uint64_t irq_lock;
	int irq_nbr;
	static int may_swap;

	irq_lock = hw_irq_ctrl_get_current_lock();

	if (irq_lock) {
		/* "spurious" wakes can happen with interrupts locked */
		return;
	}

	if (_kernel.nested == 0) {
		may_swap = 0;
	}

	_kernel.nested++;

	while ((irq_nbr = hw_irq_ctrl_get_highest_prio_irq()) != -1) {
		int last_current_running_prio = hw_irq_ctrl_get_cur_prio();
		int last_running_irq = currently_running_irq;

		hw_irq_ctrl_set_cur_prio(hw_irq_ctrl_get_prio(irq_nbr));
		hw_irq_ctrl_clear_irq(irq_nbr);

		currently_running_irq = irq_nbr;
		vector_to_irq(irq_nbr, &may_swap);
		currently_running_irq = last_running_irq;

		hw_irq_ctrl_set_cur_prio(last_current_running_prio);
	}

	_kernel.nested--;

	/* Call swap if all the following is true:
	 * 1) may_swap was enabled
	 * 2) We are not nesting irq_handler calls (interrupts)
	 * 3) Next thread to run in the ready queue is not this thread
	 */
	if (may_swap
		&& (hw_irq_ctrl_get_cur_prio() == 256)
		&& (_kernel.ready_q.cache != _current)) {

		_Swap(irq_lock);
	}
}

/**
 * Thru this function the IRQ controller can raise an immediate  interrupt which
 * will interrupt the SW itself
 * (this function should only be called from the HW model code, from SW threads)
 */
void posix_irq_handler_im_from_sw(void)
{
	/*
	 * if a higher priority interrupt than the possibly currently running is
	 * pending we go immediately into irq_handler() to vector into its
	 * handler
	 */
	if (hw_irq_ctrl_get_highest_prio_irq() != -1) {
		if (!posix_is_cpu_running()) { /* LCOV_EXCL_BR_LINE */
			/* LCOV_EXCL_START */
			posix_print_error_and_exit("programming error: %s "
					"called from a HW model thread\n",
					__func__);
			/* LCOV_EXCL_STOP */
		}
		posix_irq_handler();
	}
}

/**
 * @brief Disable all interrupts on the CPU
 *
 * This routine disables interrupts.  It can be called from either interrupt,
 * task or fiber level.  This routine returns an architecture-dependent
 * lock-out key representing the "interrupt disable state" prior to the call;
 * this key can be passed to irq_unlock() to re-enable interrupts.
 *
 * The lock-out key should only be used as the argument to the irq_unlock()
 * API. It should never be used to manually re-enable interrupts or to inspect
 * or manipulate the contents of the source register.
 *
 * This function can be called recursively: it will return a key to return the
 * state of interrupt locking to the previous level.
 *
 * WARNINGS
 * Invoking a kernel routine with interrupts locked may result in
 * interrupts being re-enabled for an unspecified period of time.  If the
 * called routine blocks, interrupts will be re-enabled while another
 * thread executes, or while the system is idle.
 *
 * The "interrupt disable state" is an attribute of a thread.  Thus, if a
 * fiber or task disables interrupts and subsequently invokes a kernel
 * routine that causes the calling thread to block, the interrupt
 * disable state will be restored when the thread is later rescheduled
 * for execution.
 *
 * @return An architecture-dependent lock-out key representing the
 * "interrupt disable state" prior to the call.
 *
 */
unsigned int posix_irq_lock(void)
{
	return hw_irq_ctrl_change_lock(true);
}

unsigned int _arch_irq_lock(void)
{
	return posix_irq_lock();
}

/**
 *
 * @brief Enable all interrupts on the CPU
 *
 * This routine re-enables interrupts on the CPU.  The @a key parameter is a
 * board-dependent lock-out key that is returned by a previous invocation of
 * board_irq_lock().
 *
 * This routine can be called from either interrupt, task or fiber level.
 *
 * @return N/A
 *
 */
void posix_irq_unlock(unsigned int key)
{
	hw_irq_ctrl_change_lock(key);
}

void _arch_irq_unlock(unsigned int key)
{
	posix_irq_unlock(key);
}


void posix_irq_full_unlock(void)
{
	hw_irq_ctrl_change_lock(false);
}

void _arch_irq_enable(unsigned int irq)
{
	hw_irq_ctrl_enable_irq(irq);
}

void _arch_irq_disable(unsigned int irq)
{
	hw_irq_ctrl_disable_irq(irq);
}

int _arch_irq_is_enabled(unsigned int irq)
{
	return hw_irq_ctrl_is_irq_enabled(irq);
}

int posix_get_current_irq(void)
{
	return currently_running_irq;
}

/**
 * Configure a static interrupt.
 *
 * _isr_declare will populate the interrupt table table with the interrupt's
 * parameters, the vector table and the software ISR table.
 *
 * We additionally set the priority in the interrupt controller at
 * runtime.
 *
 * @param irq_p IRQ line number
 * @param flags [plug it directly (1), or as a SW managed interrupt (0)]
 * @param isr_p Interrupt service routine
 * @param isr_param_p ISR parameter
 * @param flags_p IRQ options
 */
void _isr_declare(unsigned int irq_p, int flags, void isr_p(void *),
		void *isr_param_p)
{
	irq_vector_table[irq_p].irq   = irq_p;
	irq_vector_table[irq_p].func  = isr_p;
	irq_vector_table[irq_p].param = isr_param_p;
	irq_vector_table[irq_p].flags = flags;
}

/*
 * @internal
 *
 * @brief Set an interrupt's priority
 *
 * Lower values take priority over higher values.
 *
 * @return N/A
 */
void _irq_priority_set(unsigned int irq, unsigned int prio, uint32_t flags)
{
	hw_irq_ctrl_prio_set(irq, prio);
}

/**
 * Similar to ARM's NVIC_SetPendingIRQ
 * set a pending IRQ from SW
 *
 * Note that this will interrupt immediately if the interrupt is not masked and
 * IRQs are not locked, and this interrupt has higher priority than a possibly
 * currently running interrupt
 */
void posix_sw_set_pending_IRQ(unsigned int IRQn)
{
	hw_irq_ctrl_raise_im_from_sw(IRQn);
}

/**
 * Similar to ARM's NVIC_ClearPendingIRQ
 * clear a pending irq from SW
 */
void posix_sw_clear_pending_IRQ(unsigned int IRQn)
{
	hw_irq_ctrl_clear_irq(IRQn);
}

/**
 * Storage for functions offloaded to IRQ
 */
static irq_offload_routine_t off_routine;
static void *off_parameter;

/**
 * IRQ handler for the SW interrupt assigned to irq_offload()
 */
static void offload_sw_irq_handler(void *a)
{
	ARG_UNUSED(a);
	off_routine(off_parameter);
}

/**
 * @brief Run a function in interrupt context
 *
 * Raise the SW IRQ assigned to handled this
 */
void irq_offload(irq_offload_routine_t routine, void *parameter)
{
	off_routine = routine;
	off_parameter = parameter;
	_isr_declare(OFFLOAD_SW_IRQ, 0, offload_sw_irq_handler, NULL);
	_arch_irq_enable(OFFLOAD_SW_IRQ);
	posix_sw_set_pending_IRQ(OFFLOAD_SW_IRQ);
	_arch_irq_disable(OFFLOAD_SW_IRQ);
}
