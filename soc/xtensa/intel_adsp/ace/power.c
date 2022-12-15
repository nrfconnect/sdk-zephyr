/*
 * Copyright (c) 2022 Intel Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <zephyr/kernel.h>
#include <zephyr/pm/pm.h>
#include <zephyr/device.h>
#include <cpu_init.h>

#include <xtensa/corebits.h>
#include <adsp_boot.h>
#include <adsp_power.h>
#include <adsp_memory.h>
#include <adsp_imr_layout.h>
#include <zephyr/drivers/mm/mm_drv_intel_adsp_mtl_tlb.h>

#define LPSRAM_MAGIC_VALUE      0x13579BDF
#define LPSCTL_BATTR_MASK       GENMASK(16, 12)
#define SRAM_ALIAS_BASE         0xA0000000
#define SRAM_ALIAS_MASK         0xF0000000
#define MEMCTL_DEFAULT_VALUE    (MEMCTL_INV_EN | MEMCTL_ICWU_MASK | MEMCTL_DCWA_MASK | \
				 MEMCTL_DCWU_MASK | MEMCTL_L0IBUF_EN)

__imr void power_init(void)
{
	/* Disable idle power gating */
	DFDSPBRCP.bootctl[0].bctl |= DFDSPBRCP_BCTL_WAITIPCG | DFDSPBRCP_BCTL_WAITIPPG;
}

#ifdef CONFIG_PM

#define uncache_to_cache(address) \
				((__typeof__(address))(((uint32_t)(address) &  \
				~SRAM_ALIAS_MASK) | SRAM_ALIAS_BASE))

#define L2_INTERRUPT_NUMBER     4
#define L2_INTERRUPT_MASK       (1<<L2_INTERRUPT_NUMBER)

#define L3_INTERRUPT_NUMBER     6
#define L3_INTERRUPT_MASK       (1<<L3_INTERRUPT_NUMBER)

#define ALL_USED_INT_LEVELS_MASK (L2_INTERRUPT_MASK | L3_INTERRUPT_MASK)

__aligned(XCHAL_DCACHE_LINESIZE) uint8_t d0i3_stack[CONFIG_MM_DRV_PAGE_SIZE];

/**
 * @brief Power down procedure.
 *
 * Locks its code in L1 cache and shuts down memories.
 * NOTE: there's no return from this function.
 *
 * @param disable_lpsram        flag if LPSRAM is to be disabled (whole)
 * @param hpsram_pg_mask pointer to memory segments power gating mask
 * (each bit corresponds to one ebb)
 * @param response_to_ipc       flag if ipc response should be send during power down
 */
extern void power_down(bool disable_lpsram, uint32_t *hpsram_pg_mask,
			   bool response_to_ipc);

/**
 *  @brief platform specific context restore procedure
 *
 *  Should be called when soc context restore is completed
 */
extern void platform_context_restore(void);

/*
 * @brief pointer to a persistent storage space, to be set by platform code
 */
uint8_t *global_imr_ram_storage;

/*8
 * @biref a d3 restore boot entry point
 */
extern void boot_entry_d3_restore(void);

/* NOTE: This struct will grow with all values that have to be stored for
 * proper cpu restore after PG.
 */
struct core_state {
	uint32_t a0;
	uint32_t a1;
	uint32_t vecbase;
	uint32_t excsave2;
	uint32_t excsave3;
	uint32_t thread_ptr;
	uint32_t intenable;
	uint32_t bctl;
};

static struct core_state core_desc[CONFIG_MP_MAX_NUM_CPUS] = {{0}};

struct lpsram_header {
	uint32_t alt_reset_vector;
	uint32_t adsp_lpsram_magic;
	void *lp_restore_vector;
	uint32_t reserved;
	uint32_t slave_core_vector;
	uint8_t rom_bypass_vectors_reserved[0xC00 - 0x14];
};

static ALWAYS_INLINE void _core_basic_init(void)
{
	XTENSA_WSR("MEMCTL", MEMCTL_DEFAULT_VALUE);
	XTENSA_WSR("PREFCTL", ADSP_L1_CACHE_PREFCTL_VALUE);
	ARCH_XTENSA_SET_RPO_TLB();
	XTENSA_WSR("ATOMCTL", 0x15);
	__asm__ volatile("rsync");
}

static ALWAYS_INLINE void _save_core_context(uint32_t core_id)
{
	core_desc[core_id].vecbase = XTENSA_RSR("VECBASE");
	core_desc[core_id].excsave2 = XTENSA_RSR("EXCSAVE2");
	core_desc[core_id].excsave3 = XTENSA_RSR("EXCSAVE3");
	core_desc[core_id].thread_ptr = XTENSA_RUR("THREADPTR");
	__asm__ volatile("mov %0, a0" : "=r"(core_desc[core_id].a0));
	__asm__ volatile("mov %0, a1" : "=r"(core_desc[core_id].a1));
}

static ALWAYS_INLINE void _restore_core_context(void)
{
	uint32_t core_id = arch_proc_id();

	XTENSA_WSR("VECBASE", core_desc[core_id].vecbase);
	XTENSA_WSR("EXCSAVE2", core_desc[core_id].excsave2);
	XTENSA_WSR("EXCSAVE3", core_desc[core_id].excsave3);
	XTENSA_WUR("THREADPTR", core_desc[core_id].thread_ptr);
	__asm__ volatile("mov a0, %0" :: "r"(core_desc[core_id].a0));
	__asm__ volatile("mov a1, %0" :: "r"(core_desc[core_id].a1));
	__asm__ volatile("rsync");
}

void dsp_restore_vector(void);

void power_gate_entry(uint32_t core_id)
{
	struct lpsram_header *lpsheader =
		(struct lpsram_header *) DT_REG_ADDR(DT_NODELABEL(sram1));

	xthal_window_spill();
	_save_core_context(core_id);
	lpsheader->adsp_lpsram_magic = LPSRAM_MAGIC_VALUE;
	lpsheader->lp_restore_vector = &dsp_restore_vector;
	soc_cpus_active[core_id] = false;
	z_xtensa_cache_flush_inv_all();
	z_xt_ints_on(ALL_USED_INT_LEVELS_MASK);
	k_cpu_idle();
	z_xt_ints_off(0xffffffff);
}

void power_gate_exit(void)
{
	_core_basic_init();
	_restore_core_context();
}

static void ALWAYS_INLINE power_off_exit(void)
{
	__asm__(
		"  movi  a0, 0\n\t"
		"  movi  a1, 1\n\t"
		"  movi  a2, 0x40020\n\t"/* PS_UM|PS_WOE */
		"  wsr   a2, PS\n\t"
		"  wsr   a1, WINDOWSTART\n\t"
		"  wsr   a0, WINDOWBASE\n\t"
		"  rsync\n\t");
	_restore_core_context();
}

__asm__(".align 4\n\t"
	"dsp_restore_vector:\n\t"
	"  movi  a0, 0\n\t"
	"  movi  a1, 1\n\t"
	"  movi  a2, 0x40020\n\t"/* PS_UM|PS_WOE */
	"  wsr   a2, PS\n\t"
	"  wsr   a1, WINDOWSTART\n\t"
	"  wsr   a0, WINDOWBASE\n\t"
	"  rsync\n\t"
	"  movi  sp, d0i3_stack\n\t"
	"  movi a2, 0x1000\n\t"
	"  add sp, sp, a2\n\t"
	"  call0 power_gate_exit\n\t");

__imr void pm_state_imr_restore(void)
{
	struct imr_layout *imr_layout = (struct imr_layout *)(IMR_LAYOUT_ADDRESS);
	/* restore lpsram power and contents */
	bmemcpy(z_soc_uncached_ptr(UINT_TO_POINTER(LP_SRAM_BASE)),
		imr_layout->imr_state.header.imr_ram_storage,
		LP_SRAM_SIZE);

	/* restore HPSRAM contents, mapping and power states */
	adsp_mm_restore_context(imr_layout->imr_state.header.imr_ram_storage+LP_SRAM_SIZE);

	/* this function won't return, it will restore a saved state */
	power_off_exit();
}

__weak void pm_state_set(enum pm_state state, uint8_t substate_id)
{
	ARG_UNUSED(substate_id);
	uint32_t cpu = arch_proc_id();

	if (state == PM_STATE_SOFT_OFF) {
		/* save interrupt state and turn off all interrupts */
		core_desc[cpu].intenable = XTENSA_RSR("INTENABLE");
		z_xt_ints_off(0xffffffff);
		core_desc[cpu].bctl = DFDSPBRCP.bootctl[cpu].bctl;
		DFDSPBRCP.bootctl[cpu].wdtcs = DFDSPBRCP_WDT_RESTART_COMMAND;
		DFDSPBRCP.bootctl[cpu].bctl &= ~DFDSPBRCP_BCTL_WAITIPCG;
		soc_cpus_active[cpu] = false;
		z_xtensa_cache_flush_inv_all();
		if (cpu == 0) {
			/* save storage and restore information to imr */
			__ASSERT_NO_MSG(global_imr_ram_storage != NULL);
			struct imr_layout *imr_layout = (struct imr_layout *)(IMR_LAYOUT_ADDRESS);

			imr_layout->imr_state.header.adsp_imr_magic = ADSP_IMR_MAGIC_VALUE;
			imr_layout->imr_state.header.imr_restore_vector =
					(void *)boot_entry_d3_restore;
			imr_layout->imr_state.header.imr_ram_storage = global_imr_ram_storage;
			z_xtensa_cache_flush(imr_layout, sizeof(*imr_layout));

			/* save CPU context here
			 * when _restore_core_context() is called, it will return directly to
			 * the caller of this procedure
			 * any changes to CPU context after _save_core_context
			 * will be lost when power_down is executed
			 * Only data in the imr region survives
			 */
			xthal_window_spill();
			_save_core_context(cpu);

			/* save LPSRAM - a simple copy */
			memcpy(global_imr_ram_storage, (void *)LP_SRAM_BASE, LP_SRAM_SIZE);

			/* save HPSRAM - a multi step procedure, executed by a TLB driver
			 * the TLB driver will change memory mapping
			 * leaving the system not operational
			 * it must be called directly here,
			 * just before power_down
			 */
			const struct device *tlb_dev = DEVICE_DT_GET(DT_NODELABEL(tlb));

			__ASSERT_NO_MSG(tlb_dev != NULL);
			const struct intel_adsp_tlb_api *tlb_api =
					(struct intel_adsp_tlb_api *)tlb_dev->api;

			tlb_api->save_context(global_imr_ram_storage+LP_SRAM_SIZE);

			/* turn off all HPSRAM banks - get a full bitmap */
			uint32_t ebb_banks = ace_hpsram_get_bank_count();
			uint32_t hpsram_mask = (1 << ebb_banks) - 1;
			/* do power down - this function won't return */
			power_down(true, uncache_to_cache(&hpsram_mask),
				   true);
		} else {
			k_cpu_idle();
		}
	} else if (state == PM_STATE_RUNTIME_IDLE) {
		core_desc[cpu].intenable = XTENSA_RSR("INTENABLE");
		z_xt_ints_off(0xffffffff);
		DFDSPBRCP.bootctl[cpu].bctl &= ~DFDSPBRCP_BCTL_WAITIPPG;
		DFDSPBRCP.bootctl[cpu].bctl &= ~DFDSPBRCP_BCTL_WAITIPCG;
		ACE_PWRCTL->wpdsphpxpg &= ~BIT(cpu);
		if (cpu == 0) {
			uint32_t battr = DFDSPBRCP.bootctl[cpu].battr & (~LPSCTL_BATTR_MASK);

			battr |= (DFDSPBRCP_BATTR_LPSCTL_RESTORE_BOOT & LPSCTL_BATTR_MASK);
			DFDSPBRCP.bootctl[cpu].battr = battr;
		}
		power_gate_entry(cpu);
	} else {
		__ASSERT(false, "invalid argument - unsupported power state");
	}
}

/* Handle SOC specific activity after Low Power Mode Exit */
__weak void pm_state_exit_post_ops(enum pm_state state, uint8_t substate_id)
{
	ARG_UNUSED(substate_id);
	uint32_t cpu = arch_proc_id();

	if (state == PM_STATE_SOFT_OFF) {
		if (cpu == 0) {
			struct imr_layout *imr_layout = (struct imr_layout *)(IMR_LAYOUT_ADDRESS);

			DFDSPBRCP.bootctl[cpu].wdtcs = DFDSPBRCP_WDT_RESUME;
			/* restore clock gating state */
			DFDSPBRCP.bootctl[cpu].bctl |=
					(core_desc[0].bctl & DFDSPBRCP_BCTL_WAITIPCG);
			soc_cpus_active[cpu] = true;

			/* clean storage and restore information */
			z_xtensa_cache_inv(imr_layout, sizeof(*imr_layout));
			imr_layout->imr_state.header.adsp_imr_magic = 0;
			imr_layout->imr_state.header.imr_restore_vector = NULL;
			imr_layout->imr_state.header.imr_ram_storage = NULL;

			z_xtensa_cache_flush_inv_all();
			z_xt_ints_on(core_desc[cpu].intenable);
		}

	} else if (state == PM_STATE_RUNTIME_IDLE) {
		if (cpu != 0) {
			/* NOTE: HW should support dynamic power gating on secondary cores.
			 * But since there is no real profit from it, functionality is not
			 * fully implemented.
			 * SOF PM policy will not allowed primary core to enter d0i3 state
			 * when secondary cores are active.
			 */
			__ASSERT(false, "state not supported on secondary core");
			return;
		}

		ACE_PWRCTL->wpdsphpxpg |= BIT(cpu);

		while ((ACE_PWRSTS->dsphpxpgs & BIT(cpu)) == 0) {
			k_busy_wait(HW_STATE_CHECK_DELAY);
		}

		DFDSPBRCP.bootctl[cpu].bctl |=
			DFDSPBRCP_BCTL_WAITIPCG | DFDSPBRCP_BCTL_WAITIPPG;
		if (cpu == 0) {
			DFDSPBRCP.bootctl[cpu].battr &= (~LPSCTL_BATTR_MASK);
		}

		soc_cpus_active[cpu] = true;
		z_xtensa_cache_flush_inv_all();
		z_xt_ints_on(core_desc[cpu].intenable);
	} else {
		__ASSERT(false, "invalid argument - unsupported power state");
	}
}

#endif
