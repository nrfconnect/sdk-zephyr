/*
 * Copyright (c) 2017 Linaro Limited.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <device.h>
#include <init.h>
#include <kernel.h>
#include <soc.h>
#include <arch/arm/cortex_m/cmsis.h>
#include <arch/arm/cortex_m/mpu/arm_core_mpu.h>
#include <logging/sys_log.h>

#if defined(CONFIG_MPU_STACK_GUARD)
/*
 * @brief Configure MPU stack guard
 *
 * This function configures per thread stack guards reprogramming the MPU.
 * The functionality is meant to be used during context switch.
 *
 * @param thread thread info data structure.
 */
void configure_mpu_stack_guard(struct k_thread *thread)
{
	u32_t guard_size = MPU_GUARD_ALIGN_AND_SIZE;
#if defined(CONFIG_USERSPACE)
	u32_t guard_start = thread->arch.priv_stack_start ?
			    (u32_t)thread->arch.priv_stack_start :
			    (u32_t)thread->stack_obj;
#else
	u32_t guard_start = thread->stack_info.start;
#endif

	arm_core_mpu_disable();
	arm_core_mpu_configure(THREAD_STACK_GUARD_REGION, guard_start,
			       guard_size);
	arm_core_mpu_enable();
}
#endif

#if defined(CONFIG_USERSPACE)
/*
 * @brief Configure MPU user context
 *
 * This function configures the thread's user context.
 * The functionality is meant to be used during context switch.
 *
 * @param thread thread info data structure.
 */
void configure_mpu_user_context(struct k_thread *thread)
{
	SYS_LOG_DBG("configure user thread %p's context", thread);
	arm_core_mpu_disable();
	arm_core_mpu_configure_user_context(thread);
	arm_core_mpu_enable();
}

/*
 * @brief Configure MPU memory domain
 *
 * This function configures per thread memory domain reprogramming the MPU.
 * The functionality is meant to be used during context switch.
 *
 * @param thread thread info data structure.
 */
void configure_mpu_mem_domain(struct k_thread *thread)
{
	SYS_LOG_DBG("configure thread %p's domain", thread);
	arm_core_mpu_disable();
	arm_core_mpu_configure_mem_domain(thread->mem_domain_info.mem_domain);
	arm_core_mpu_enable();
}

void _arch_mem_domain_configure(struct k_thread *thread)
{
	configure_mpu_mem_domain(thread);
}

int _arch_mem_domain_max_partitions_get(void)
{
	return arm_core_mpu_get_max_domain_partition_regions();
}

/*
 * Reset MPU region for a single memory partition
 */
void _arch_mem_domain_partition_remove(struct k_mem_domain *domain,
				       u32_t  partition_id)
{
	ARG_UNUSED(domain);

	arm_core_mpu_disable();
	arm_core_mpu_mem_partition_remove(partition_id);
	arm_core_mpu_enable();

}

/*
 * Destroy MPU regions for the mem domain
 */
void _arch_mem_domain_destroy(struct k_mem_domain *domain)
{
	ARG_UNUSED(domain);

	arm_core_mpu_disable();
	arm_core_mpu_configure_mem_domain(NULL);
	arm_core_mpu_enable();
}

/*
 * Validate the given buffer is user accessible or not
 */
int _arch_buffer_validate(void *addr, size_t size, int write)
{
	return arm_core_mpu_buffer_validate(addr, size, write);
}

#endif
