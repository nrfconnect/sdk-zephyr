/*
 * Copyright (c) 2019 Intel Corp.
 * SPDX-License-Identifier: Apache-2.0
 */

GEN_OFFSET_SYM(_callee_saved_t, rsp);
GEN_OFFSET_SYM(_callee_saved_t, rbp);
GEN_OFFSET_SYM(_callee_saved_t, rbx);
GEN_OFFSET_SYM(_callee_saved_t, r12);
GEN_OFFSET_SYM(_callee_saved_t, r13);
GEN_OFFSET_SYM(_callee_saved_t, r14);
GEN_OFFSET_SYM(_callee_saved_t, r15);
GEN_OFFSET_SYM(_callee_saved_t, rip);
GEN_OFFSET_SYM(_callee_saved_t, rflags);

GEN_OFFSET_SYM(_thread_arch_t, rax);
GEN_OFFSET_SYM(_thread_arch_t, rcx);
GEN_OFFSET_SYM(_thread_arch_t, rdx);
GEN_OFFSET_SYM(_thread_arch_t, rsi);
GEN_OFFSET_SYM(_thread_arch_t, rdi);
GEN_OFFSET_SYM(_thread_arch_t, r8);
GEN_OFFSET_SYM(_thread_arch_t, r9);
GEN_OFFSET_SYM(_thread_arch_t, r10);
GEN_OFFSET_SYM(_thread_arch_t, r11);
GEN_OFFSET_SYM(_thread_arch_t, sse);
#ifdef CONFIG_USERSPACE
GEN_OFFSET_SYM(_thread_arch_t, ss);
GEN_OFFSET_SYM(_thread_arch_t, cs);
#endif

GEN_OFFSET_SYM(x86_tss64_t, ist1);
GEN_OFFSET_SYM(x86_tss64_t, cpu);
#ifdef CONFIG_USERSPACE
GEN_OFFSET_SYM(x86_tss64_t, psp);
GEN_OFFSET_SYM(x86_tss64_t, usp);
#endif
GEN_ABSOLUTE_SYM(__X86_TSS64_SIZEOF, sizeof(x86_tss64_t));

GEN_OFFSET_SYM(x86_cpuboot_t, ready);
GEN_OFFSET_SYM(x86_cpuboot_t, tr);
GEN_OFFSET_SYM(x86_cpuboot_t, gs_base);
GEN_OFFSET_SYM(x86_cpuboot_t, sp);
GEN_OFFSET_SYM(x86_cpuboot_t, fn);
GEN_OFFSET_SYM(x86_cpuboot_t, arg);
#ifdef CONFIG_X86_MMU
GEN_OFFSET_SYM(x86_cpuboot_t, ptables);
#endif /* CONFIG_X86_MMU */
GEN_ABSOLUTE_SYM(__X86_CPUBOOT_SIZEOF, sizeof(x86_cpuboot_t));
