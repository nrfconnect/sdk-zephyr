/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/bluetooth/audio/cap.h>

#include "cap_commander.h"

/* List of fakes used by this unit tester */
#define FFF_FAKES_LIST(FAKE)                                                                       \
	FAKE(mock_cap_commander_discovery_complete_cb)                                             \
	FAKE(mock_cap_commander_volume_changed_cb)                                                 \
	FAKE(mock_cap_commander_volume_offset_changed_cb)

DEFINE_FAKE_VOID_FUNC(mock_cap_commander_discovery_complete_cb, struct bt_conn *, int,
		      const struct bt_csip_set_coordinator_csis_inst *);

DEFINE_FAKE_VOID_FUNC(mock_cap_commander_volume_changed_cb, struct bt_conn *, int);
DEFINE_FAKE_VOID_FUNC(mock_cap_commander_volume_offset_changed_cb, struct bt_conn *, int);

const struct bt_cap_commander_cb mock_cap_commander_cb = {
	.discovery_complete = mock_cap_commander_discovery_complete_cb,
#if defined(CONFIG_BT_VCP_VOL_CTLR)
	.volume_changed = mock_cap_commander_volume_changed_cb,
#if defined(CONFIG_BT_VCP_VOL_CTLR_VOCS)
	.volume_offset_changed = mock_cap_commander_volume_offset_changed_cb,
#endif /* CONFIG_BT_VCP_VOL_CTLR */
#endif /* CONFIG_BT_VCP_VOL_CTLR */
};

void mock_cap_commander_init(void)
{
	FFF_FAKES_LIST(RESET_FAKE);
}

void mock_cap_commander_cleanup(void)
{
}
