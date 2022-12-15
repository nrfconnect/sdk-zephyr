/* Bluetooth: Mesh Generic OnOff, Generic Level, Lighting & Vendor Models
 *
 * Copyright (c) 2018 Vikrant More
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/sys/__assert.h>
#include <zephyr/stats/stats.h>
#include <stdlib.h>
#include <string.h>
#include <zephyr/kernel.h>

#ifdef CONFIG_MCUMGR_CMD_FS_MGMT
#include <zephyr/mgmt/mcumgr/grp/fs_mgmt/fs_mgmt.h>
#endif
#ifdef CONFIG_MCUMGR_CMD_IMG_MGMT
#include <zephyr/mgmt/mcumgr/grp/img_mgmt/img_mgmt.h>
#endif
#ifdef CONFIG_MCUMGR_CMD_OS_MGMT
#include <zephyr/mgmt/mcumgr/grp/os_mgmt/os_mgmt.h>
#endif
#ifdef CONFIG_MCUMGR_CMD_STAT_MGMT
#include <zephyr/mgmt/mcumgr/grp/stat_mgmt/stat_mgmt.h>
#endif

/* Define an example stats group; approximates seconds since boot. */
STATS_SECT_START(smp_svr_stats)
STATS_SECT_ENTRY(ticks)
STATS_SECT_END;

/* Assign a name to the `ticks` stat. */
STATS_NAME_START(smp_svr_stats)
STATS_NAME(smp_svr_stats, ticks)
STATS_NAME_END(smp_svr_stats);

/* Define an instance of the stats group. */
STATS_SECT_DECL(smp_svr_stats) smp_svr_stats;

void smp_svr_init(void)
{
	int rc;

	rc = STATS_INIT_AND_REG(smp_svr_stats, STATS_SIZE_32, "smp_svr_stats");
	__ASSERT_NO_MSG(rc == 0);

	/* Register the built-in mcumgr command handlers. */
#ifdef CONFIG_MCUMGR_CMD_FS_MGMT
	fs_mgmt_register_group();
#endif
#ifdef CONFIG_MCUMGR_CMD_OS_MGMT
	os_mgmt_register_group();
#endif
#ifdef CONFIG_MCUMGR_CMD_IMG_MGMT
	img_mgmt_register_group();
#endif
#ifdef CONFIG_MCUMGR_CMD_STAT_MGMT
	stat_mgmt_register_group();
#endif
}

static void smp_svr_timer_handler(struct k_timer *dummy)
{
	STATS_INC(smp_svr_stats, ticks);
}

K_TIMER_DEFINE(smp_svr_timer, smp_svr_timer_handler, NULL);
