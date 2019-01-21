/*
 * Copyright (c) 2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Common fault handler for ARM Cortex-M
 *
 * Common fault handler for ARM Cortex-M processors.
 */

#include <toolchain.h>
#include <linker/sections.h>

#include <kernel.h>
#include <kernel_structs.h>
#include <inttypes.h>
#include <exc_handle.h>
#include <logging/log_ctrl.h>

#ifdef CONFIG_PRINTK
#include <misc/printk.h>
#define PR_EXC(...) printk(__VA_ARGS__)
#define STORE_xFAR(reg_var, reg) u32_t reg_var = (u32_t)reg
#else
#define PR_EXC(...)
#define STORE_xFAR(reg_var, reg)
#endif /* CONFIG_PRINTK */

#if (CONFIG_FAULT_DUMP == 2)
#define PR_FAULT_INFO(...) PR_EXC(__VA_ARGS__)
#else
#define PR_FAULT_INFO(...)
#endif

#if defined(CONFIG_ARM_MPU) && defined(CONFIG_CPU_HAS_NXP_MPU)
#define EMN(edr)   (((edr) & SYSMPU_EDR_EMN_MASK) >> SYSMPU_EDR_EMN_SHIFT)
#define EACD(edr)  (((edr) & SYSMPU_EDR_EACD_MASK) >> SYSMPU_EDR_EACD_SHIFT)
#endif

#if defined(CONFIG_ARM_SECURE_FIRMWARE)

/* Exception Return (EXC_RETURN) is provided in LR upon exception entry.
 * It is used to perform an exception return and to detect possible state
 * transition upon exception.
 */

/* Prefix. Indicates that this is an EXC_RETURN value.
 * This field reads as 0b11111111.
 */
#define EXC_RETURN_INDICATOR_PREFIX     (0xFF << 24)
/* bit[0]: Exception Secure. The security domain the exception was taken to. */
#define EXC_RETURN_EXCEPTION_SECURE_Pos 0
#define EXC_RETURN_EXCEPTION_SECURE_Msk \
		(1 << EXC_RETURN_EXCEPTION_SECURE_Pos)
#define EXC_RETURN_EXCEPTION_SECURE_Non_Secure 0
#define EXC_RETURN_EXCEPTION_SECURE_Secure EXC_RETURN_EXCEPTION_SECURE_Msk
/* bit[2]: Stack Pointer selection. */
#define EXC_RETURN_SPSEL_Pos 2
#define EXC_RETURN_SPSEL_Msk (1 << EXC_RETURN_SPSEL_Pos)
#define EXC_RETURN_SPSEL_MAIN 0
#define EXC_RETURN_SPSEL_PROCESS EXC_RETURN_SPSEL_Msk
/* bit[3]: Mode. Indicates the Mode that was stacked from. */
#define EXC_RETURN_MODE_Pos 3
#define EXC_RETURN_MODE_Msk (1 << EXC_RETURN_MODE_Pos)
#define EXC_RETURN_MODE_HANDLER 0
#define EXC_RETURN_MODE_THREAD EXC_RETURN_MODE_Msk
/* bit[4]: Stack frame type. Indicates whether the stack frame is a standard
 * integer only stack frame or an extended floating-point stack frame.
 */
#define EXC_RETURN_STACK_FRAME_TYPE_Pos 4
#define EXC_RETURN_STACK_FRAME_TYPE_Msk (1 << EXC_RETURN_STACK_FRAME_TYPE_Pos)
#define EXC_RETURN_STACK_FRAME_TYPE_EXTENDED 0
#define EXC_RETURN_STACK_FRAME_TYPE_STANDARD EXC_RETURN_STACK_FRAME_TYPE_Msk
/* bit[5]: Default callee register stacking. Indicates whether the default
 * stacking rules apply, or whether the callee registers are already on the
 * stack.
 */
#define EXC_RETURN_CALLEE_STACK_Pos 5
#define EXC_RETURN_CALLEE_STACK_Msk (1 << EXC_RETURN_CALLEE_STACK_Pos)
#define EXC_RETURN_CALLEE_STACK_SKIPPED 0
#define EXC_RETURN_CALLEE_STACK_DEFAULT EXC_RETURN_CALLEE_STACK_Msk
/* bit[6]: Secure or Non-secure stack. Indicates whether a Secure or
 * Non-secure stack is used to restore stack frame on exception return.
 */
#define EXC_RETURN_RETURN_STACK_Pos 6
#define EXC_RETURN_RETURN_STACK_Msk (1 << EXC_RETURN_RETURN_STACK_Pos)
#define EXC_RETURN_RETURN_STACK_Non_Secure 0
#define EXC_RETURN_RETURN_STACK_Secure EXC_RETURN_RETURN_STACK_Msk

/* Integrity signature for an ARMv8-M implementation */
#if defined(CONFIG_ARMV7_M_ARMV8_M_FP)
#define INTEGRITY_SIGNATURE_STD 0xFEFA125BUL
#define INTEGRITY_SIGNATURE_EXT 0xFEFA125AUL
#else
#define INTEGRITY_SIGNATURE 0xFEFA125BUL
#endif /* CONFIG_ARMV7_M_ARMV8_M_FP */
/* Size (in words) of the additional state context that is pushed
 * to the Secure stack during a Non-Secure exception entry.
 */
#define ADDITIONAL_STATE_CONTEXT_WORDS 10
#endif /* CONFIG_ARM_SECURE_FIRMWARE */

/**
 *
 * Dump information regarding fault (FAULT_DUMP == 1)
 *
 * Dump information regarding the fault when CONFIG_FAULT_DUMP is set to 1
 * (short form).
 *
 * eg. (precise bus error escalated to hard fault):
 *
 * Fault! EXC #3
 * HARD FAULT: Escalation (see below)!
 * MMFSR: 0x00000000, BFSR: 0x00000082, UFSR: 0x00000000
 * BFAR: 0xff001234
 *
 *
 *
 * Dump information regarding fault (FAULT_DUMP == 2)
 *
 * Dump information regarding the fault when CONFIG_FAULT_DUMP is set to 2
 * (long form), and return the error code for the kernel to identify the fatal
 * error reason.
 *
 * eg. (precise bus error escalated to hard fault):
 *
 * ***** HARD FAULT *****
 *    Fault escalation (see below)
 * ***** BUS FAULT *****
 *   Precise data bus error
 *   Address: 0xff001234
 *
 */

#if (CONFIG_FAULT_DUMP == 1)
static void _FaultShow(const NANO_ESF *esf, int fault)
{
	PR_EXC("Fault! EXC #%d\n", fault);

#if defined(CONFIG_ARMV7_M_ARMV8_M_MAINLINE)
	PR_EXC("MMFSR: 0x%x, BFSR: 0x%x, UFSR: 0x%x\n",
	       SCB_MMFSR, SCB_BFSR, SCB_UFSR);
#if defined(CONFIG_ARM_SECURE_FIRMWARE)
	PR_EXC("SFSR: 0x%x\n", SAU->SFSR);
#endif /* CONFIG_ARM_SECURE_FIRMWARE */
#endif /* CONFIG_ARMV7_M_ARMV8_M_MAINLINE */
}
#else
/* For Dump level 2, detailed information is generated by the
 * fault handling functions for individual fault conditions, so this
 * function is left empty.
 *
 * For Dump level 0, no information needs to be generated.
 */
static void _FaultShow(const NANO_ESF *esf, int fault)
{
	(void)esf;
	(void)fault;
}
#endif /* FAULT_DUMP == 1 */

#ifdef CONFIG_USERSPACE
Z_EXC_DECLARE(z_arch_user_string_nlen);

static const struct z_exc_handle exceptions[] = {
	Z_EXC_HANDLE(z_arch_user_string_nlen)
};
#endif

/* Perform an assessment whether an MPU fault shall be
 * treated as recoverable.
 *
 * @return 1 if error is recoverable, otherwise return 0.
 */
static int _MemoryFaultIsRecoverable(NANO_ESF *esf)
{
#ifdef CONFIG_USERSPACE
	for (int i = 0; i < ARRAY_SIZE(exceptions); i++) {
		/* Mask out instruction mode */
		u32_t start = (u32_t)exceptions[i].start & ~0x1;
		u32_t end = (u32_t)exceptions[i].end & ~0x1;

		if (esf->pc >= start && esf->pc < end) {
			esf->pc = (u32_t)(exceptions[i].fixup);
			return 1;
		}
	}
#endif

	return 0;
}

#if defined(CONFIG_ARMV6_M_ARMV8_M_BASELINE)
/* HardFault is used for all fault conditions on ARMv6-M. */
#elif defined(CONFIG_ARMV7_M_ARMV8_M_MAINLINE)

/**
 *
 * @brief Dump MPU fault information
 *
 * See _FaultDump() for example.
 *
 * @return error code to identify the fatal error reason
 */
static u32_t _MpuFault(NANO_ESF *esf, int fromHardFault)
{
	u32_t reason = _NANO_ERR_HW_EXCEPTION;

	PR_FAULT_INFO("***** MPU FAULT *****\n");

	if ((SCB->CFSR & SCB_CFSR_MSTKERR_Msk) != 0) {
		PR_FAULT_INFO("  Stacking error\n");
	}
	if ((SCB->CFSR & SCB_CFSR_MUNSTKERR_Msk) != 0) {
		PR_FAULT_INFO("  Unstacking error\n");
	}
	if ((SCB->CFSR & SCB_CFSR_DACCVIOL_Msk) != 0) {
		PR_FAULT_INFO("  Data Access Violation\n");
		/* In a fault handler, to determine the true faulting address:
		 * 1. Read and save the MMFAR value.
		 * 2. Read the MMARVALID bit in the MMFSR.
		 * The MMFAR address is valid only if this bit is 1.
		 *
		 * Software must follow this sequence because another higher
		 * priority exception might change the MMFAR value.
		 */
		u32_t mmfar = SCB->MMFAR;

		if ((SCB->CFSR & SCB_CFSR_MMARVALID_Msk) != 0) {
			PR_EXC("  MMFAR Address: 0x%x\n", mmfar);
			if (fromHardFault) {
				/* clear SCB_MMAR[VALID] to reset */
				SCB->CFSR &= ~SCB_CFSR_MMARVALID_Msk;
			}
#if defined(CONFIG_HW_STACK_PROTECTION)
			/* When stack protection is enabled, we need to see
			 * if the memory violation error is a stack corruption.
			 * For that we investigate the address fail.
			 */
			struct k_thread *thread = _current;
			u32_t guard_start;
			if (thread != NULL) {
#if defined(CONFIG_USERSPACE)
				guard_start =
					thread->arch.priv_stack_start ?
					(u32_t)thread->arch.priv_stack_start :
					(u32_t)thread->stack_obj;
#else
				guard_start = thread->stack_info.start;
#endif
				if (mmfar >= guard_start &&
					mmfar < guard_start +
					MPU_GUARD_ALIGN_AND_SIZE) {
					/* Thread stack corruption */
					reason = _NANO_ERR_STACK_CHK_FAIL;
				}
			}
#else
		(void)mmfar;
#endif /* CONFIG_HW_STACK_PROTECTION */
		}
	}
	if ((SCB->CFSR & SCB_CFSR_IACCVIOL_Msk) != 0) {
		PR_FAULT_INFO("  Instruction Access Violation\n");
	}
#if defined(CONFIG_ARMV7_M_ARMV8_M_FP)
	if ((SCB->CFSR & SCB_CFSR_MLSPERR_Msk) != 0) {
		PR_FAULT_INFO(
			"  Floating-point lazy state preservation error\n");
	}
#endif /* !defined(CONFIG_ARMV7_M_ARMV8_M_FP) */

	/* Assess whether system shall ignore/recover from this MPU fault. */
	if (_MemoryFaultIsRecoverable(esf)) {
		reason = _NANO_ERR_RECOVERABLE;
	}

	return reason;
}

/**
 *
 * @brief Dump bus fault information
 *
 * See _FaultDump() for example.
 *
 * @return N/A
 */
static int _BusFault(NANO_ESF *esf, int fromHardFault)
{
	PR_FAULT_INFO("***** BUS FAULT *****\n");

	if (SCB->CFSR & SCB_CFSR_STKERR_Msk) {
		PR_FAULT_INFO("  Stacking error\n");
	} else if (SCB->CFSR & SCB_CFSR_UNSTKERR_Msk) {
		PR_FAULT_INFO("  Unstacking error\n");
	} else if (SCB->CFSR & SCB_CFSR_PRECISERR_Msk) {
		PR_FAULT_INFO("  Precise data bus error\n");
		/* In a fault handler, to determine the true faulting address:
		 * 1. Read and save the BFAR value.
		 * 2. Read the BFARVALID bit in the BFSR.
		 * The BFAR address is valid only if this bit is 1.
		 *
		 * Software must follow this sequence because another
		 * higher priority exception might change the BFAR value.
		 */
		STORE_xFAR(bfar, SCB->BFAR);

		if ((SCB->CFSR & SCB_CFSR_BFARVALID_Msk) != 0) {
			PR_EXC("  BFAR Address: 0x%x\n", bfar);
			if (fromHardFault) {
				/* clear SCB_CFSR_BFAR[VALID] to reset */
				SCB->CFSR &= ~SCB_CFSR_BFARVALID_Msk;
			}
		}
		/* it's possible to have both a precise and imprecise fault */
		if ((SCB->CFSR & SCB_CFSR_IMPRECISERR_Msk) != 0) {
			PR_FAULT_INFO("  Imprecise data bus error\n");
		}
	} else if (SCB->CFSR & SCB_CFSR_IMPRECISERR_Msk) {
		PR_FAULT_INFO("  Imprecise data bus error\n");
	} else if ((SCB->CFSR & SCB_CFSR_IBUSERR_Msk) != 0) {
		PR_FAULT_INFO("  Instruction bus error\n");
#if !defined(CONFIG_ARMV7_M_ARMV8_M_FP)
	}
#else
	} else if (SCB->CFSR & SCB_CFSR_LSPERR_Msk) {
		PR_FAULT_INFO("  Floating-point lazy state preservation error\n");
	}
#endif /* !defined(CONFIG_ARMV7_M_ARMV8_M_FP) */

#if defined(CONFIG_ARM_MPU) && defined(CONFIG_CPU_HAS_NXP_MPU)
	u32_t sperr = SYSMPU->CESR & SYSMPU_CESR_SPERR_MASK;
	u32_t mask = BIT(31);
	int i;

	if (sperr) {
		for (i = 0; i < SYSMPU_EAR_COUNT; i++, mask >>= 1) {
			if ((sperr & mask) == 0) {
				continue;
			}
			STORE_xFAR(edr, SYSMPU->SP[i].EDR);
			STORE_xFAR(ear, SYSMPU->SP[i].EAR);

			PR_FAULT_INFO("  NXP MPU error, port %d\n", i);
			PR_FAULT_INFO("    Mode: %s, %s Address: 0x%x\n",
			       edr & BIT(2) ? "Supervisor" : "User",
			       edr & BIT(1) ? "Data" : "Instruction",
			       ear);
			PR_FAULT_INFO(
					"    Type: %s, Master: %d, Regions: 0x%x\n",
			       edr & BIT(0) ? "Write" : "Read",
			       EMN(edr), EACD(edr));
		}
		SYSMPU->CESR &= ~sperr;
	}
#endif /* defined(CONFIG_ARM_MPU) && defined(CONFIG_CPU_HAS_NXP_MPU) */

#if defined(CONFIG_ARMV8_M_MAINLINE)
	/* clear BSFR sticky bits */
	SCB->CFSR |= SCB_CFSR_BUSFAULTSR_Msk;
#endif /* CONFIG_ARMV8_M_MAINLINE */

	if (_MemoryFaultIsRecoverable(esf)) {
		return _NANO_ERR_RECOVERABLE;
	}

	return _NANO_ERR_HW_EXCEPTION;
}

/**
 *
 * @brief Dump usage fault information
 *
 * See _FaultDump() for example.
 *
 * @return error code to identify the fatal error reason
 */
static u32_t _UsageFault(const NANO_ESF *esf)
{
	u32_t reason = _NANO_ERR_HW_EXCEPTION;

	PR_FAULT_INFO("***** USAGE FAULT *****\n");

	/* bits are sticky: they stack and must be reset */
	if ((SCB->CFSR & SCB_CFSR_DIVBYZERO_Msk) != 0) {
		PR_FAULT_INFO("  Division by zero\n");
	}
	if ((SCB->CFSR & SCB_CFSR_UNALIGNED_Msk) != 0) {
		PR_FAULT_INFO("  Unaligned memory access\n");
	}
#if defined(CONFIG_ARMV8_M_MAINLINE)
	if ((SCB->CFSR & SCB_CFSR_STKOF_Msk) != 0) {
		PR_FAULT_INFO("  Stack overflow\n");
#if defined(CONFIG_HW_STACK_PROTECTION)
		/* Stack Overflows are reported as stack
		 * corruption errors.
		 */
		reason = _NANO_ERR_STACK_CHK_FAIL;
#endif /* CONFIG_HW_STACK_PROTECTION */
	}
#endif /* CONFIG_ARMV8_M_MAINLINE */
	if ((SCB->CFSR & SCB_CFSR_NOCP_Msk) != 0) {
		PR_FAULT_INFO("  No coprocessor instructions\n");
	}
	if ((SCB->CFSR & SCB_CFSR_INVPC_Msk) != 0) {
		PR_FAULT_INFO("  Illegal load of EXC_RETURN into PC\n");
	}
	if ((SCB->CFSR & SCB_CFSR_INVSTATE_Msk) != 0) {
		PR_FAULT_INFO("  Illegal use of the EPSR\n");
	}
	if ((SCB->CFSR & SCB_CFSR_UNDEFINSTR_Msk) != 0) {
		PR_FAULT_INFO("  Attempt to execute undefined instruction\n");
	}

	/* clear USFR sticky bits */
	SCB->CFSR |= SCB_CFSR_USGFAULTSR_Msk;

	return reason;
}

#if defined(CONFIG_ARM_SECURE_FIRMWARE)
/**
 *
 * @brief Dump secure fault information
 *
 * See _FaultDump() for example.
 *
 * @return N/A
 */
static void _SecureFault(const NANO_ESF *esf)
{
	PR_FAULT_INFO("***** SECURE FAULT *****\n");

	STORE_xFAR(sfar, SAU->SFAR);
	if ((SAU->SFSR & SAU_SFSR_SFARVALID_Msk) != 0) {
		PR_EXC("  Address: 0x%x\n", sfar);
	}

	/* bits are sticky: they stack and must be reset */
	if ((SAU->SFSR & SAU_SFSR_INVEP_Msk) != 0) {
		PR_FAULT_INFO("  Invalid entry point\n");
	} else if ((SAU->SFSR & SAU_SFSR_INVIS_Msk) != 0) {
		PR_FAULT_INFO("  Invalid integrity signature\n");
	} else if ((SAU->SFSR & SAU_SFSR_INVER_Msk) != 0) {
		PR_FAULT_INFO("  Invalid exception return\n");
	} else if ((SAU->SFSR & SAU_SFSR_AUVIOL_Msk) != 0) {
		PR_FAULT_INFO("  Attribution unit violation\n");
	} else if ((SAU->SFSR & SAU_SFSR_INVTRAN_Msk) != 0) {
		PR_FAULT_INFO("  Invalid transition\n");
	} else if ((SAU->SFSR & SAU_SFSR_LSPERR_Msk) != 0) {
		PR_FAULT_INFO("  Lazy state preservation\n");
	} else if ((SAU->SFSR & SAU_SFSR_LSERR_Msk) != 0) {
		PR_FAULT_INFO("  Lazy state error\n");
	}

	/* clear SFSR sticky bits */
	SAU->SFSR |= 0xFF;
}
#endif /* defined(CONFIG_ARM_SECURE_FIRMWARE) */

/**
 *
 * @brief Dump debug monitor exception information
 *
 * See _FaultDump() for example.
 *
 * @return N/A
 */
static void _DebugMonitor(const NANO_ESF *esf)
{
	ARG_UNUSED(esf);

	PR_FAULT_INFO(
		"***** Debug monitor exception (not implemented) *****\n");
}

#else
#error Unknown ARM architecture
#endif /* CONFIG_ARMV6_M_ARMV8_M_BASELINE */

/**
 *
 * @brief Dump hard fault information
 *
 * See _FaultDump() for example.
 *
 * @return error code to identify the fatal error reason
 */
static u32_t _HardFault(NANO_ESF *esf)
{
	u32_t reason = _NANO_ERR_HW_EXCEPTION;

	PR_FAULT_INFO("***** HARD FAULT *****\n");

#if defined(CONFIG_ARMV6_M_ARMV8_M_BASELINE)
	if (_MemoryFaultIsRecoverable(esf) != 0) {
		reason = _NANO_ERR_RECOVERABLE;
	}
#elif defined(CONFIG_ARMV7_M_ARMV8_M_MAINLINE)
	if ((SCB->HFSR & SCB_HFSR_VECTTBL_Msk) != 0) {
		PR_EXC("  Bus fault on vector table read\n");
	} else if ((SCB->HFSR & SCB_HFSR_FORCED_Msk) != 0) {
		PR_EXC("  Fault escalation (see below)\n");
		if (SCB_MMFSR != 0) {
			reason = _MpuFault(esf, 1);
		} else if (SCB_BFSR != 0) {
			reason = _BusFault(esf, 1);
		} else if (SCB_UFSR != 0) {
			reason = _UsageFault(esf);
#if defined(CONFIG_ARM_SECURE_FIRMWARE)
		} else if (SAU->SFSR != 0) {
			_SecureFault(esf);
#endif /* CONFIG_ARM_SECURE_FIRMWARE */
		}
	}
#else
#error Unknown ARM architecture
#endif /* CONFIG_ARMV6_M_ARMV8_M_BASELINE */

	return reason;
}

/**
 *
 * @brief Dump reserved exception information
 *
 * See _FaultDump() for example.
 *
 * @return N/A
 */
static void _ReservedException(const NANO_ESF *esf, int fault)
{
	ARG_UNUSED(esf);

	PR_FAULT_INFO("***** %s %d) *****\n",
	       fault < 16 ? "Reserved Exception (" : "Spurious interrupt (IRQ ",
	       fault - 16);
}

/* Handler function for ARM fault conditions. */
static u32_t _FaultHandle(NANO_ESF *esf, int fault)
{
	u32_t reason = _NANO_ERR_HW_EXCEPTION;

	switch (fault) {
	case 3:
		reason = _HardFault(esf);
		break;
#if defined(CONFIG_ARMV6_M_ARMV8_M_BASELINE)
	/* HardFault is used for all fault conditions on ARMv6-M. */
#elif defined(CONFIG_ARMV7_M_ARMV8_M_MAINLINE)
	case 4:
		reason = _MpuFault(esf, 0);
		break;
	case 5:
		reason = _BusFault(esf, 0);
		break;
	case 6:
		reason = _UsageFault(esf);
		break;
#if defined(CONFIG_ARM_SECURE_FIRMWARE)
	case 7:
		_SecureFault(esf);
		break;
#endif /* CONFIG_ARM_SECURE_FIRMWARE */
	case 12:
		_DebugMonitor(esf);
		break;
#else
#error Unknown ARM architecture
#endif /* CONFIG_ARMV6_M_ARMV8_M_BASELINE */
	default:
		_ReservedException(esf, fault);
		break;
	}

	if (reason != _NANO_ERR_RECOVERABLE) {
		/* Dump generic information about the fault. */
		_FaultShow(esf, fault);
	}

	return reason;
}

#if defined(CONFIG_ARM_SECURE_FIRMWARE)
#if (CONFIG_FAULT_DUMP == 2)
/**
 * @brief Dump the Secure Stack information for an exception that
 * has occurred in Non-Secure state.
 *
 * @param secure_esf Pointer to the secure stack frame.
 */
static void _SecureStackDump(const NANO_ESF *secure_esf)
{
	/*
	 * In case a Non-Secure exception interrupted the Secure
	 * execution, the Secure state has stacked the additional
	 * state context and the top of the stack contains the
	 * integrity signature.
	 *
	 * In case of a Non-Secure function call the top of the
	 * stack contains the return address to Secure state.
	 */
	u32_t *top_of_sec_stack = (u32_t *)secure_esf;
	u32_t sec_ret_addr;
#if defined(CONFIG_ARMV7_M_ARMV8_M_FP)
	if ((*top_of_sec_stack == INTEGRITY_SIGNATURE_STD) ||
		(*top_of_sec_stack == INTEGRITY_SIGNATURE_EXT)) {
#else
	if (*top_of_sec_stack == INTEGRITY_SIGNATURE) {
#endif /* CONFIG_ARMV7_M_ARMV8_M_FP */
		/* Secure state interrupted by a Non-Secure exception.
		 * The return address after the additional state
		 * context, stacked by the Secure code upon
		 * Non-Secure exception entry.
		 */
		top_of_sec_stack += ADDITIONAL_STATE_CONTEXT_WORDS;
		secure_esf = (const NANO_ESF *)top_of_sec_stack;
		sec_ret_addr = secure_esf->pc;
	} else {
		/* Exception during Non-Secure function call.
		 * The return address is located on top of stack.
		 */
		sec_ret_addr = *top_of_sec_stack;
	}
	PR_FAULT_INFO("  S instruction address:  0x%x\n", sec_ret_addr);

}
#define SECURE_STACK_DUMP(esf) _SecureStackDump(esf)
#else
/* We do not dump the Secure stack information for lower dump levels. */
#define SECURE_STACK_DUMP(esf)
#endif /* CONFIG_FAULT_DUMP== 2 */
#endif /* CONFIG_ARM_SECURE_FIRMWARE */

/**
 *
 * @brief ARM Fault handler
 *
 * This routine is called when fatal error conditions are detected by hardware
 * and is responsible for:
 * - resetting the processor fault status registers (for the case when the
 *   error handling policy allows the system to recover from the error),
 * - reporting the error information,
 * - determining the error reason to be provided as input to the user-
 *   provided routine, _NanoFatalErrorHandler().
 * The _NanoFatalErrorHandler() is invoked once the above operations are
 * completed, and is responsible for implementing the error handling policy.
 *
 * The provided ESF pointer points to the exception stack frame of the current
 * security state. Note that the current security state might not be the actual
 * state in which the processor was executing, when the exception occurred.
 * The actual state may need to be determined by inspecting the EXC_RETURN
 * value, which is provided as argument to the Fault handler.
 *
 * @param esf Pointer to the exception stack frame of the current security
 * state. The stack frame may be either on the Main stack (MSP) or Process
 * stack (PSP) depending at what execution state the exception was taken.
 *
 * @param exc_return EXC_RETURN value present in LR after exception entry.
 *
 * Note: exc_return argument shall only be used by the Fault handler if we are
 * running a Secure Firmware.
 */
void _Fault(NANO_ESF *esf, u32_t exc_return)
{
	u32_t reason;
	int fault = SCB->ICSR & SCB_ICSR_VECTACTIVE_Msk;

	LOG_PANIC();

#if defined(CONFIG_ARM_SECURE_FIRMWARE)
	if ((exc_return & EXC_RETURN_INDICATOR_PREFIX) !=
			EXC_RETURN_INDICATOR_PREFIX) {
		/* Invalid EXC_RETURN value */
		goto _exit_fatal;
	}
	if ((exc_return & EXC_RETURN_EXCEPTION_SECURE_Secure) == 0) {
		/* Secure Firmware shall only handle Secure Exceptions.
		 * This is a fatal error.
		 */
		goto _exit_fatal;
	}

	if (exc_return & EXC_RETURN_RETURN_STACK_Secure) {
		/* Exception entry occurred in Secure stack. */
	} else {
		/* Exception entry occurred in Non-Secure stack. Therefore, 'esf'
		 * holds the Secure stack information, however, the actual
		 * exception stack frame is located in the Non-Secure stack.
		 */

		/* Dump the Secure stack before handling the actual fault. */
		SECURE_STACK_DUMP(esf);

		/* Handle the actual fault.
		 * Extract the correct stack frame from the Non-Secure state
		 * and supply it to the fault handing function.
		 */
		if (exc_return & EXC_RETURN_MODE_THREAD) {
			esf = (NANO_ESF *)__TZ_get_PSP_NS();
			if ((SCB->ICSR & SCB_ICSR_RETTOBASE_Msk) == 0) {
				PR_EXC("RETTOBASE does not match EXC_RETURN\n");
				goto _exit_fatal;
			}
		} else {
			esf = (NANO_ESF *)__TZ_get_MSP_NS();
			if ((SCB->ICSR & SCB_ICSR_RETTOBASE_Msk) != 0) {
				PR_EXC("RETTOBASE does not match EXC_RETURN\n");
				goto _exit_fatal;
			}
		}
	}
#else
	(void) exc_return;
#endif /* CONFIG_ARM_SECURE_FIRMWARE*/

	reason = _FaultHandle(esf, fault);

	if (reason == _NANO_ERR_RECOVERABLE) {
		return;
	}

#if defined(CONFIG_ARM_SECURE_FIRMWARE)
_exit_fatal:
	reason = _NANO_ERR_HW_EXCEPTION;
#endif
	_NanoFatalErrorHandler(reason, esf);
}

/**
 *
 * @brief Initialization of fault handling
 *
 * Turns on the desired hardware faults.
 *
 * @return N/A
 */
void _FaultInit(void)
{
#if defined(CONFIG_ARMV6_M_ARMV8_M_BASELINE)
#elif defined(CONFIG_ARMV7_M_ARMV8_M_MAINLINE)
	SCB->CCR |= SCB_CCR_DIV_0_TRP_Msk;
#else
#error Unknown ARM architecture
#endif /* CONFIG_ARMV6_M_ARMV8_M_BASELINE */
#if defined(CONFIG_BUILTIN_STACK_GUARD)
	/* If Stack guarding via SP limit checking is enabled, disable
	 * SP limit checking inside HardFault and NMI. This is done
	 * in order to allow for the desired fault logging to execute
	 * properly in all cases.
	 *
	 * Note that this could allow a Secure Firmware Main Stack
	 * to descend into non-secure region during HardFault and
	 * NMI exception entry. To prevent from this, non-secure
	 * memory regions must be located higher than secure memory
	 * regions.
	 *
	 * For Non-Secure Firmware this could allow the Non-Secure Main
	 * Stack to attempt to descend into secure region, in which case a
	 * Secure Hard Fault will occur and we can track the fault from there.
	 */
	SCB->CCR |= SCB_CCR_STKOFHFNMIGN_Msk;
#endif /* CONFIG_BUILTIN_STACK_GUARD */
}
