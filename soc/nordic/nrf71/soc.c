/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief System/hardware module for Nordic Semiconductor nRF71 family processor
 *
 * This module provides routines to initialize and support board-level hardware
 * for the Nordic Semiconductor nRF71 family processor.
 */

#include <zephyr/autoconf.h>

#include <zephyr/kernel.h>
#include <zephyr/devicetree.h>
#include <zephyr/init.h>
#include <zephyr/logging/log.h>
#include <zephyr/cache.h>
#ifndef __ZEPHYR__
#include <hal/nrf_cache.h>
#endif

#if defined(NRF_APPLICATION)
#include <cmsis_core.h>
#include <hal/nrf_glitchdet.h>
#endif

#include <soc.h>
#include <nrfx.h>
#include <helpers/nrfx_ram_ctrl.h>
#include <lib/nrfx_coredep.h>

#include <hal/nrf_spu.h>
#include <hal/nrf_mpc.h>
#include <hal/nrf_lfxo.h>

#include <wicr_setup.h>

LOG_MODULE_REGISTER(soc, CONFIG_SOC_LOG_LEVEL);

#define LFXO_NODE DT_NODELABEL(lfxo)

#if !defined(CONFIG_TRUSTED_EXECUTION_NONSECURE)

struct mpc_region_override {
	nrf_mpc_override_config_t config;
	uintptr_t startaddr;
	uintptr_t endaddr;
	uint32_t perm;
	uint32_t permmask;
};

/*
 * Initialize the override struct with reasonable defaults. This includes:
 *
 * Use a slave number of 0 to avoid redirecting bus transactions from
 * one slave to another.
 *
 * Lock the override to prevent the code that follows from tampering
 * with the configuration.
 *
 * Enable the override so it takes effect.
 *
 * Indicate that secdom is not enabled as this driver is not used on
 * platforms with secdom.
 */
#define MPC_REGION_OVERRIDE_INIT(_startaddr, _endaddr, _secure, _privileged)			\
	{											\
		.config =  {									\
			.slave_number = 0,							\
			.lock = true,								\
			.enable = true,								\
			.secdom_enable = false,							\
			.secure_mask = false,							\
		},										\
		.startaddr = _startaddr,							\
		.endaddr = _endaddr,								\
		.perm = (									\
			(MPC_OVERRIDE_PERM_READ_Allowed << MPC_OVERRIDE_PERM_READ_Pos) |	\
			(MPC_OVERRIDE_PERM_WRITE_Allowed << MPC_OVERRIDE_PERM_WRITE_Pos) |	\
			(MPC_OVERRIDE_PERM_EXECUTE_Allowed << MPC_OVERRIDE_PERM_EXECUTE_Pos) |	\
			(_secure << MPC_OVERRIDE_PERM_SECATTR_Pos) |				\
			(_privileged << MPC_OVERRIDE_PERM_PRIVL_Pos)				\
		),										\
		.permmask = (									\
			MPC_OVERRIDE_PERM_READ_Msk |						\
			MPC_OVERRIDE_PERM_WRITE_Msk |						\
			MPC_OVERRIDE_PERM_EXECUTE_Msk |						\
			MPC_OVERRIDE_PERM_SECATTR_Msk |						\
			MPC_OVERRIDE_PERM_PRIVL_Msk						\
		),										\
	}

static const struct mpc_region_override mpc00_region_overrides[] = {
	/* Make RAM_00/01/02/03 (AMBIX00 + AMBIX03) accessible to all domains */
	MPC_REGION_OVERRIDE_INIT(0x20000000, 0x200FE000, 0, 0),
	/* Make MRAM accessible to all domains */
	MPC_REGION_OVERRIDE_INIT(0x00000000, 0x01000000, 0, 0),
#if CONFIG_SOC_NRF71_WIFI_DAP
	/* Allow access to Wi-Fi debug interface registers */
	MPC_REGION_OVERRIDE_INIT(0x48000000, 0x48100000, 0, 0),
#endif
};

static const struct mpc_region_override mpc03_region_overrides[] = {
	/* Make RAM_02/03 (AMBIX03)  accessible to all domains */
	MPC_REGION_OVERRIDE_INIT(0x200C0000, 0x200FE000, 0, 0),
};

static void set_mpc_region_override(NRF_MPC_Type *mpc,
				    size_t index,
				    const struct mpc_region_override *override)
{
	nrf_mpc_override_startaddr_set(mpc, index, override->startaddr);
	nrf_mpc_override_endaddr_set(mpc, index, override->endaddr);
	nrf_mpc_override_perm_set(mpc, index, override->perm);
	nrf_mpc_override_permmask_set(mpc, index, override->permmask);
	nrf_mpc_override_config_set(mpc, index, &override->config);
}

static void mpc_configuration(void)
{
	ARRAY_FOR_EACH(mpc00_region_overrides, i) {
		set_mpc_region_override(NRF_MPC00, i, &mpc00_region_overrides[i]);
	}

	ARRAY_FOR_EACH(mpc03_region_overrides, i) {
		set_mpc_region_override(NRF_MPC03, i, &mpc03_region_overrides[i]);
	}
}

/**
 * Return the SPU instance that can be used to configure the
 * peripheral at the given base address.
 */
static inline NRF_SPU_Type *spu_instance_from_peripheral_addr(uint32_t peripheral_addr)
{
	/* See the SPU chapter in the IPS for how this is calculated */

	uint32_t apb_bus_number = peripheral_addr & 0x00FC0000;

	return (NRF_SPU_Type *)(0x50000000 | apb_bus_number);
}

static void grtc_configuration(void)
{
	/* Split security configuration to let Wi-Fi access GRTC */
	nrf_spu_feature_secattr_set(NRF_SPU20, NRF_SPU_FEATURE_GRTC_CC, 15, 0, 0);
	nrf_spu_feature_secattr_set(NRF_SPU20, NRF_SPU_FEATURE_GRTC_CC, 14, 0, 0);
	nrf_spu_feature_secattr_set(NRF_SPU20, NRF_SPU_FEATURE_GRTC_INTERRUPT, 4, 0, 0);
	nrf_spu_feature_secattr_set(NRF_SPU20, NRF_SPU_FEATURE_GRTC_INTERRUPT, 5, 0, 0);
	nrf_spu_feature_secattr_set(NRF_SPU20, NRF_SPU_FEATURE_GRTC_SYSCOUNTER, 0, 0, 0);
}

static void ipct_configuration(void)
{
	/* Grant secure access to IPCT, since NS by default */
	nrf_spu_periph_perm_secattr_set(NRF_SPU10, 13, true);
}
#endif /* CONFIG_TRUSTED_EXECUTION_NONSECURE */

#if defined(CONFIG_SOC_NRF71_WIFI_BOOT)
#if (defined(NRF_APPLICATION) && !defined(CONFIG_TRUSTED_EXECUTION_NONSECURE)) || \
	!defined(__ZEPHYR__)
/*
 * HFXO64M register offsets and bit fields, taken from the nRF71 datasheet. The MDK only
 * describes the event and interrupt registers of this peripheral, so the configuration
 * registers are accessed directly. Remove this block once the MDK covers them.
 *
 * NRF_HFXO64M resolves to the secure or the non-secure alias to match the build, the same way
 * NRF_LFXO does for the sibling peripheral on this bus.
 */
#define HFXO64M_REG(offset)	(*(volatile uint32_t *)((uintptr_t)NRF_HFXO64M_NS + (offset)))
#define HFXO64M_REG_TRIM_RTUNE		HFXO64M_REG(0x440UL)
#define HFXO64M_REG_TRIM_CHIRPTUNE	HFXO64M_REG(0x444UL)
#define HFXO64M_REG_TRIM_DOUBLERCOMP	HFXO64M_REG(0x448UL)
#define HFXO64M_REG_MIRROR		HFXO64M_REG(0x480UL)
#define HFXO64M_REG_PWRUPCTRL		HFXO64M_REG(0x484UL)
#define HFXO64M_REG_MODE		HFXO64M_REG(0x488UL)
#define HFXO64M_REG_XTALSETTLETIME	HFXO64M_REG(0x48CUL)
#define HFXO64M_REG_CHIRPTIME		HFXO64M_REG(0x490UL)
#define HFXO64M_REG_ENABLEDAMPING	HFXO64M_REG(0x494UL)
#define HFXO64M_REG_CFG			HFXO64M_REG(0x49CUL)

#define HFXO64M_TRIM_RTUNE_VAL_Pos		(0UL)
#define HFXO64M_TRIM_CHIRPTUNE_VAL_Pos		(0UL)
#define HFXO64M_TRIM_DOUBLERCOMP_VAL_Pos	(0UL)

#define HFXO64M_MIRROR_LOCK_Pos			(0UL)
#define HFXO64M_MIRROR_LOCK_Disabled		(0UL)

#define HFXO64M_PWRUPCTRL_CTRL_Pos		(0UL)
#define HFXO64M_PWRUPCTRL_CTRL_Auto		(0UL)

#define HFXO64M_MODE_MODE_Pos			(0UL)
#define HFXO64M_MODE_MODE_Normal		(0UL)

#define HFXO64M_XTALSETTLETIME_VAL_Pos		(0UL)
#define HFXO64M_XTALSETTLETIME_VAL_Settle300us	(1UL)

#define HFXO64M_CHIRPTIME_VAL_Pos		(0UL)
#define HFXO64M_CHIRPTIME_VAL_Chirp56us		(3UL)

#define HFXO64M_ENABLEDAMPING_VAL_Pos		(0UL)
#define HFXO64M_ENABLEDAMPING_VAL_Disabled	(0UL)

#define HFXO64M_CFG_LEVELSELECT_Pos		(0UL)
#define HFXO64M_CFG_LEVELSELECT_Normal		(0UL)
#define HFXO64M_CFG_ENABLENORMALBIASMODE_Pos	(1UL)
#define HFXO64M_CFG_ENABLENORMALBIASMODE_Normal	(1UL)
#define HFXO64M_CFG_BYPASSREG0V8_Pos		(2UL)
#define HFXO64M_CFG_BYPASSREG0V8_Normal		(0UL)
#define HFXO64M_CFG_BYPASSREG1V5_Pos		(3UL)
#define HFXO64M_CFG_BYPASSREG1V5_Normal		(0UL)
#define HFXO64M_CFG_ENABLECMOS1DIVIDER_Pos	(4UL)
#define HFXO64M_CFG_ENABLECMOS1DIVIDER_Enabled	(1UL)
#define HFXO64M_CFG_ENABLECMOS2DIVIDER_Pos	(5UL)
#define HFXO64M_CFG_ENABLECMOS2DIVIDER_Disabled	(0UL)
#define HFXO64M_CFG_ENABLECMOS3DIVIDER_Pos	(6UL)
#define HFXO64M_CFG_ENABLECMOS3DIVIDER_Enabled	(1UL)
#define HFXO64M_CFG_ENABLETSDIVIDER_Pos		(7UL)
#define HFXO64M_CFG_ENABLETSDIVIDER_Disabled	(0UL)
#define HFXO64M_CFG_BUFFDRIVECMOS1_Msk		(0x3UL << 8UL)
#define HFXO64M_CFG_BUFFDRIVECMOS2_Msk		(0x3UL << 10UL)
#define HFXO64M_CFG_BUFFDRIVECMOS3_Msk		(0x3UL << 12UL)
#define HFXO64M_CFG_BUFFDRIVETS_Msk		(0x3UL << 14UL)
#define HFXO64M_CFG_CHIRPEN_Msk			(0x1UL << 16UL)

/*
 * Trim codes applied to HFXO64M. These are the default codes for the crystal, used until
 * device specific values are available from FICR.
 */
#define HFXO64M_TRIM_RTUNE_DEFAULT		(0x0UL)
#define HFXO64M_TRIM_CHIRPTUNE_DEFAULT		(0x2UL)
#define HFXO64M_TRIM_DOUBLERCOMP_DEFAULT	(0x3UL)

/*
 * HFXO64M is disabled after cold boot and ignores hardware clock requests until software has
 * applied the trim values, configured the oscillator, released the mirror lock and handed
 * power control over to the HFXO64M controller. This must happen before the Wi-Fi core, which
 * requests the 64 MHz clock, is started.
 */
static void hfxo64m_setup(void)
{
	HFXO64M_REG_TRIM_RTUNE =
		HFXO64M_TRIM_RTUNE_DEFAULT << HFXO64M_TRIM_RTUNE_VAL_Pos;
	HFXO64M_REG_TRIM_CHIRPTUNE =
		HFXO64M_TRIM_CHIRPTUNE_DEFAULT << HFXO64M_TRIM_CHIRPTUNE_VAL_Pos;
	HFXO64M_REG_TRIM_DOUBLERCOMP =
		HFXO64M_TRIM_DOUBLERCOMP_DEFAULT << HFXO64M_TRIM_DOUBLERCOMP_VAL_Pos;

	/* Keep the buffer drive strengths and the chirp enable as they come out of reset. */
	HFXO64M_REG_CFG =
		(HFXO64M_REG_CFG & (HFXO64M_CFG_BUFFDRIVECMOS1_Msk |
				    HFXO64M_CFG_BUFFDRIVECMOS2_Msk |
				    HFXO64M_CFG_BUFFDRIVECMOS3_Msk |
				    HFXO64M_CFG_BUFFDRIVETS_Msk |
				    HFXO64M_CFG_CHIRPEN_Msk)) |
		(HFXO64M_CFG_LEVELSELECT_Normal << HFXO64M_CFG_LEVELSELECT_Pos) |
		(HFXO64M_CFG_ENABLENORMALBIASMODE_Normal <<
			HFXO64M_CFG_ENABLENORMALBIASMODE_Pos) |
		(HFXO64M_CFG_BYPASSREG0V8_Normal << HFXO64M_CFG_BYPASSREG0V8_Pos) |
		(HFXO64M_CFG_BYPASSREG1V5_Normal << HFXO64M_CFG_BYPASSREG1V5_Pos) |
		(HFXO64M_CFG_ENABLECMOS1DIVIDER_Enabled << HFXO64M_CFG_ENABLECMOS1DIVIDER_Pos) |
		(HFXO64M_CFG_ENABLECMOS2DIVIDER_Disabled << HFXO64M_CFG_ENABLECMOS2DIVIDER_Pos) |
		(HFXO64M_CFG_ENABLECMOS3DIVIDER_Enabled << HFXO64M_CFG_ENABLECMOS3DIVIDER_Pos) |
		(HFXO64M_CFG_ENABLETSDIVIDER_Disabled << HFXO64M_CFG_ENABLETSDIVIDER_Pos);

	HFXO64M_REG_XTALSETTLETIME =
		HFXO64M_XTALSETTLETIME_VAL_Settle300us << HFXO64M_XTALSETTLETIME_VAL_Pos;
	HFXO64M_REG_ENABLEDAMPING =
		HFXO64M_ENABLEDAMPING_VAL_Disabled << HFXO64M_ENABLEDAMPING_VAL_Pos;
	HFXO64M_REG_CHIRPTIME =
		HFXO64M_CHIRPTIME_VAL_Chirp56us << HFXO64M_CHIRPTIME_VAL_Pos;

	/* A crystal is wired to XC1/XC2, so drive the core rather than bypass it for a TCXO. */
	HFXO64M_REG_MODE = HFXO64M_MODE_MODE_Normal << HFXO64M_MODE_MODE_Pos;

	/* Release the lock so that the mirrored registers above are taken into use. */
	HFXO64M_REG_MIRROR = HFXO64M_MIRROR_LOCK_Disabled << HFXO64M_MIRROR_LOCK_Pos;

	/* Power the oscillator automatically, following the hardware clock requests. */
	HFXO64M_REG_PWRUPCTRL = HFXO64M_PWRUPCTRL_CTRL_Auto << HFXO64M_PWRUPCTRL_CTRL_Pos;
}

/*
 * Start the HFXO once it has been configured. The crystal oscillator is started through the
 * CLOCK peripheral (XOSTART task), not through HFXO64M itself, and CLOCK reports readiness via
 * the XOSTARTED event. Both are modelled by the MDK, so use the generated symbols here.
 */
static void hfxo64m_start(void)
{
	NRF_CLOCK->EVENTS_XOSTARTED = 0;
	NRF_CLOCK->TASKS_XOSTART =
		(CLOCK_TASKS_XOSTART_TASKS_XOSTART_Trigger << CLOCK_TASKS_XOSTART_TASKS_XOSTART_Pos);

	/* Wait until the crystal has started. */
	while (NRF_CLOCK->EVENTS_XOSTARTED == 0) {
	}
}

/* Antenna switch (ANTSW) GPIO setup, done before the Wi-Fi core is started. */
#define ANTSW_P0_09_PIN_CNF	0x5010A0A4UL	/* NRF_P0->PIN_CNF[9] */
#define ANTSW_P0_09_PULL_UP	0x40FUL		/* Output, pull-up, drive H1 */
#define ANTSW_P0_05_PIN_CNF	0x5010A094UL	/* NRF_P0->PIN_CNF[5] */
#define ANTSW_P0_05_PULL_DOWN	0x7UL		/* Output, pull-down */

static void antsw_setup(void)
{
	/* P0.09 pull-up: power on ANTSW. */
	*(volatile uint32_t *)ANTSW_P0_09_PIN_CNF = ANTSW_P0_09_PULL_UP;
	/* P0.05 pull-down: place ANTSW towards WLAN. */
	*(volatile uint32_t *)ANTSW_P0_05_PIN_CNF = ANTSW_P0_05_PULL_DOWN;
}

static void wifi_setup(void)
{
	/* Kickstart the LMAC processor */
	NRF_WIFICORE_LRCCONF_LRC0->POWERON =
		(LRCCONF_POWERON_MAIN_AlwaysOn << LRCCONF_POWERON_MAIN_Pos);
	NRF_WIFICORE_LMAC_VPR->INITPC = (uint32_t)(uintptr_t)NRF_WICR->FIRMWARE.LMACINITPC;
	NRF_WIFICORE_LMAC_VPR->CPURUN = (VPR_CPURUN_EN_Running << VPR_CPURUN_EN_Pos);
}
#endif
#endif

void soc_early_init_hook(void)
{
#if defined(CONFIG_HAS_NORDIC_RAM_CTRL) && !defined(CONFIG_TRUSTED_EXECUTION_NONSECURE)
	nrfx_ram_ctrl_retention_enable_all_set(false);
#endif

	/* Update the SystemCoreClock global variable with current core clock
	 * retrieved from the DT.
	 */
	SystemCoreClock = NRF_PERIPH_GET_FREQUENCY(DT_NODELABEL(cpu));

#if !defined(CONFIG_TRUSTED_EXECUTION_NONSECURE)
	/* Skip for tf-m, configuration exist in target_cfg_71.c */
	mpc_configuration();
	grtc_configuration();
	ipct_configuration();
#endif

#if (defined(NRF_APPLICATION) && !defined(CONFIG_TRUSTED_EXECUTION_NONSECURE)) || \
	!defined(__ZEPHYR__)
#if defined(CONFIG_SOC_NRF7120_WICR_SETUP)
	int ret = wicr_setup();

	if (ret != 0) {
		LOG_ERR("WICR programming failed: %d", ret);
	}
#endif

#if defined(CONFIG_SOC_NRF71_WIFI_BOOT)
	/* Bring up the 64 MHz crystal oscillator the Wi-Fi core depends on. */
	hfxo64m_setup();
	hfxo64m_start();
	/* Configure ANTSW GPIOs before starting comms with the Wi-Fi core. */
	antsw_setup();
	wifi_setup();
#endif

#if DT_HAS_COMPAT_STATUS_OKAY(nordic_nrf_pwr_antswc)
	*(volatile uint32_t *)PWR_ANTSWC_REG |= PWR_ANTSWC_ENABLE;
#endif

	/* Configure LFXO capacitive load if internal load capacitors are used */
#if DT_ENUM_HAS_VALUE(LFXO_NODE, load_capacitors, internal)
	nrf_lfxo_cload_set(NRF_LFXO,
			(uint8_t)(DT_PROP(LFXO_NODE, load_capacitance_femtofarad) / 1000));
#endif
#endif /* (NRF_APPLICATION && !CONFIG_TRUSTED_EXECUTION_NONSECURE) || !__ZEPHYR__  */

#ifdef __ZEPHYR__
	sys_cache_instr_enable();
#elif defined(NRF_ICACHE)
	nrf_cache_enable(NRF_ICACHE);
#endif
}

void arch_busy_wait(uint32_t time_us)
{
	nrfx_coredep_delay_us(time_us);
}

#ifdef CONFIG_SOC_RESET_HOOK
void soc_reset_hook(void)
{
	SystemInit();
}
#endif
