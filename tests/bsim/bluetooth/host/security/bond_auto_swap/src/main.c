/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "bs_bt_utils.h"
#include "bstests.h"

#if defined(CONFIG_BT_CENTRAL)
void central_basic(void);
void central_fal(void);
void central_dir(void);
void central_unpair(void);
#if defined(CONFIG_BT_SETTINGS)
void central_persist_bond(void);
void central_persist_reconnect(void);
#endif /* CONFIG_BT_SETTINGS */
#endif /* CONFIG_BT_CENTRAL */

#if defined(CONFIG_BT_PERIPHERAL)
void peripheral_basic(void);
void peripheral_fal(void);
void peripheral_dir(void);
void peripheral_unpair(void);
#if defined(CONFIG_BT_SETTINGS)
void peripheral_persist_bond(void);
void peripheral_persist_reconnect(void);
#endif /* CONFIG_BT_SETTINGS */
#endif /* CONFIG_BT_PERIPHERAL */

#if !defined(CONFIG_BT_CENTRAL) && !defined(CONFIG_BT_PERIPHERAL)
#error "At least one of CONFIG_BT_CENTRAL or CONFIG_BT_PERIPHERAL must be enabled"
#endif /* !CONFIG_BT_CENTRAL && !CONFIG_BT_PERIPHERAL */

static const struct bst_test_instance test_to_add[] = {
#if defined(CONFIG_BT_CENTRAL)
	{
		.test_id = "central_basic",
		.test_main_f = central_basic,
	},
	{
		.test_id = "central_fal",
		.test_main_f = central_fal,
	},
	{
		.test_id = "central_dir",
		.test_main_f = central_dir,
	},
	{
		.test_id = "central_unpair",
		.test_main_f = central_unpair,
	},
#if defined(CONFIG_BT_SETTINGS)
	{
		.test_id = "central_persist_bond",
		.test_main_f = central_persist_bond,
	},
	{
		.test_id = "central_persist_reconnect",
		.test_main_f = central_persist_reconnect,
	},
#endif /* CONFIG_BT_SETTINGS */
#endif /* CONFIG_BT_CENTRAL */
#if defined(CONFIG_BT_PERIPHERAL)
	{
		.test_id = "peripheral_basic",
		.test_main_f = peripheral_basic,
	},
	{
		.test_id = "peripheral_fal",
		.test_main_f = peripheral_fal,
	},
	{
		.test_id = "peripheral_dir",
		.test_main_f = peripheral_dir,
	},
	{
		.test_id = "peripheral_unpair",
		.test_main_f = peripheral_unpair,
	},
#if defined(CONFIG_BT_SETTINGS)
	{
		.test_id = "peripheral_persist_bond",
		.test_main_f = peripheral_persist_bond,
	},
	{
		.test_id = "peripheral_persist_reconnect",
		.test_main_f = peripheral_persist_reconnect,
	},
#endif /* CONFIG_BT_SETTINGS */
#endif /* CONFIG_BT_PERIPHERAL */
	BSTEST_END_MARKER,
};

static struct bst_test_list *install(struct bst_test_list *tests)
{
	return bst_add_tests(tests, test_to_add);
};

bst_test_install_t test_installers[] = {install, NULL};

int main(void)
{
	bst_main();
	return 0;
}
