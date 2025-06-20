/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "bs_bt_utils.h"
#include "bstests.h"

#if defined(CONFIG_BT_CENTRAL)
void central_mult_adv_reject(void);
#endif /* CONFIG_BT_CENTRAL */

#if defined(CONFIG_BT_PERIPHERAL)
void peripheral_mult_adv_reject(void);
#endif /* CONFIG_BT_PERIPHERAL */

#if !defined(CONFIG_BT_CENTRAL) && !defined(CONFIG_BT_PERIPHERAL)
#error "At least one of CONFIG_BT_CENTRAL or CONFIG_BT_PERIPHERAL must be enabled"
#endif /* !CONFIG_BT_CENTRAL && !CONFIG_BT_PERIPHERAL */

#if defined(CONFIG_BT_CENTRAL)
static void central_preinit(void)
{
	g_is_central = true;
}
#endif /* CONFIG_BT_CENTRAL */

#if defined(CONFIG_BT_PERIPHERAL)
static void peripheral_preinit(void)
{
	g_is_central = false;
}
#endif /* CONFIG_BT_PERIPHERAL */

static const struct bst_test_instance test_to_add[] = {
#if defined(CONFIG_BT_CENTRAL)
	{
		.test_pre_init_f = central_preinit,
		.test_id = "central_mult_adv_reject",
		.test_main_f = central_mult_adv_reject,
	},
#endif /* CONFIG_BT_CENTRAL */
#if defined(CONFIG_BT_PERIPHERAL)
	{
		.test_pre_init_f = peripheral_preinit,
		.test_id = "peripheral_mult_adv_reject",
		.test_main_f = peripheral_mult_adv_reject,
	},
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
