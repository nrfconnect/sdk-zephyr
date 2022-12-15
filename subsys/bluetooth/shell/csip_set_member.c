/**
 * @file
 * @brief Shell APIs for Bluetooth CSIP set member
 *
 * Copyright (c) 2020 Bose Corporation
 * Copyright (c) 2021-2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/types.h>

#include <zephyr/kernel.h>
#include <zephyr/types.h>
#include <zephyr/shell/shell.h>
#include <stdlib.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/audio/csip.h>
#include "bt.h"

extern const struct shell *ctx_shell;
struct bt_csip_set_member_svc_inst *svc_inst;
static uint8_t sirk_read_rsp = BT_CSIP_READ_SIRK_REQ_RSP_ACCEPT;

static void locked_cb(struct bt_conn *conn,
		      struct bt_csip_set_member_svc_inst *svc_inst,
		      bool locked)
{
	if (conn == NULL) {
		shell_error(ctx_shell, "Server %s the device",
			    locked ? "locked" : "released");
	} else {
		char addr[BT_ADDR_LE_STR_LEN];

		conn_addr_str(conn, addr, sizeof(addr));

		shell_print(ctx_shell, "Client %s %s the device",
			    addr, locked ? "locked" : "released");
	}
}

static uint8_t sirk_read_req_cb(struct bt_conn *conn,
				struct bt_csip_set_member_svc_inst *svc_inst)
{
	char addr[BT_ADDR_LE_STR_LEN];
	static const char *const rsp_strings[] = {
		"Accept", "Accept Enc", "Reject", "OOB only"
	};

	conn_addr_str(conn, addr, sizeof(addr));

	shell_print(ctx_shell, "Client %s requested to read the sirk. "
		    "Responding with %s", addr, rsp_strings[sirk_read_rsp]);

	return sirk_read_rsp;
}

static struct bt_csip_set_member_cb csip_set_member_cbs = {
	.lock_changed = locked_cb,
	.sirk_read_req = sirk_read_req_cb,
};

static int cm_csip_set_member_register(const struct shell *sh, size_t argc, char **argv)
{
	int err;
	struct bt_csip_set_member_register_param param = {
		.set_size = 2,
		.rank = 1,
		.lockable = true,
		/* Using the CSIS test sample SIRK */
		.set_sirk = { 0xcd, 0xcc, 0x72, 0xdd, 0x86, 0x8c, 0xcd, 0xce,
			      0x22, 0xfd, 0xa1, 0x21, 0x09, 0x7d, 0x7d, 0x45 },
		.cb = &csip_set_member_cbs
	};

	for (size_t argn = 1; argn < argc; argn++) {
		const char *arg = argv[argn];

		if (strcmp(arg, "size") == 0) {
			param.set_size = strtol(argv[++argn], NULL, 10);
		} else if (strcmp(arg, "rank") == 0) {
			param.rank = strtol(argv[++argn], NULL, 10);
		} else if (strcmp(arg, "not-lockable") == 0) {
			param.lockable = false;
		} else if (strcmp(arg, "sirk") == 0) {
			size_t len;

			argn++;
			len = hex2bin(argv[argn], strlen(argv[argn]),
				      param.set_sirk, sizeof(param.set_sirk));
			if (len == 0) {
				shell_error(sh, "Could not parse SIRK");
				return -ENOEXEC;
			}
		} else {
			shell_help(sh);
			return SHELL_CMD_HELP_PRINTED;
		}
	}

	err = bt_csip_set_member_register(&param, &svc_inst);
	if (err != 0) {
		shell_error(sh, "Could not register CSIP: %d", err);
		return err;
	}

	return 0;
}

static int cm_csip_set_member_print_sirk(const struct shell *sh, size_t argc,
			       char *argv[])
{
	bt_csip_set_member_print_sirk(svc_inst);
	return 0;
}

static int cm_csip_set_member_lock(const struct shell *sh, size_t argc, char *argv[])
{
	int err;

	err = bt_csip_set_member_lock(svc_inst, true, false);
	if (err != 0) {
		shell_error(sh, "Failed to set lock: %d", err);
		return -ENOEXEC;
	}

	shell_print(sh, "Set locked");

	return 0;
}

static int cm_csip_set_member_release(const struct shell *sh, size_t argc,
			    char *argv[])
{
	bool force = false;
	int err;

	if (argc > 1) {
		if (strcmp(argv[1], "force") == 0) {
			force = true;
		} else {
			shell_error(sh, "Unknown parameter: %s", argv[1]);
			return -ENOEXEC;
		}
	}

	err = bt_csip_set_member_lock(svc_inst, false, force);

	if (err != 0) {
		shell_error(sh, "Failed to release lock: %d", err);
		return -ENOEXEC;
	}

	shell_print(sh, "Set released");

	return 0;
}

static int cm_csip_set_member_set_sirk_rsp(const struct shell *sh, size_t argc,
				 char *argv[])
{
	if (strcmp(argv[1], "accept") == 0) {
		sirk_read_rsp = BT_CSIP_READ_SIRK_REQ_RSP_ACCEPT;
	} else if (strcmp(argv[1], "accept_enc") == 0) {
		sirk_read_rsp = BT_CSIP_READ_SIRK_REQ_RSP_ACCEPT_ENC;
	} else if (strcmp(argv[1], "reject") == 0) {
		sirk_read_rsp = BT_CSIP_READ_SIRK_REQ_RSP_REJECT;
	} else if (strcmp(argv[1], "oob") == 0) {
		sirk_read_rsp = BT_CSIP_READ_SIRK_REQ_RSP_OOB_ONLY;
	} else {
		shell_error(sh, "Unknown parameter: %s", argv[1]);
		return -ENOEXEC;
	}

	return 0;
}

static int cm_csip_set_member(const struct shell *sh, size_t argc, char **argv)
{
	shell_error(sh, "%s unknown parameter: %s", argv[0], argv[1]);

	return -ENOEXEC;
}

SHELL_STATIC_SUBCMD_SET_CREATE(csip_set_member_cmds,
	SHELL_CMD_ARG(register, NULL,
		      "Initialize the service and register callbacks "
		      "[size <int>] [rank <int>] [not-lockable] [sirk <data>]",
		      cm_csip_set_member_register, 1, 4),
	SHELL_CMD_ARG(lock, NULL,
		      "Lock the set",
		      cm_csip_set_member_lock, 1, 0),
	SHELL_CMD_ARG(release, NULL,
		      "Release the set [force]",
		      cm_csip_set_member_release, 1, 1),
	SHELL_CMD_ARG(print_sirk, NULL,
		      "Print the currently used SIRK",
		      cm_csip_set_member_print_sirk, 1, 0),
	SHELL_CMD_ARG(set_sirk_rsp, NULL,
		      "Set the response used in SIRK requests "
		      "<accept, accept_enc, reject, oob>",
		      cm_csip_set_member_set_sirk_rsp, 2, 0),
		      SHELL_SUBCMD_SET_END
);

SHELL_CMD_ARG_REGISTER(csip_set_member, &csip_set_member_cmds,
		       "Bluetooth CSIP set member shell commands",
		       cm_csip_set_member, 1, 1);

ssize_t csis_ad_data_add(struct bt_data *data_array, const size_t data_array_size,
			 const bool discoverable)
{
	size_t ad_len = 0;

	/* Advertise RSI in discoverable mode only */
	if (svc_inst != NULL && discoverable) {
		static uint8_t ad_rsi[BT_CSIP_RSI_SIZE];
		int err;

		/* A privacy-enabled Set Member should only advertise RSI values derived
		 * from a SIRK that is exposed in encrypted form.
		 */
		if (IS_ENABLED(CONFIG_BT_PRIVACY) &&
		    !IS_ENABLED(CONFIG_BT_CSIP_SET_MEMBER_ENC_SIRK_SUPPORT)) {
			shell_warn(ctx_shell, "RSI derived from unencrypted SIRK");
		}

		err = bt_csip_set_member_generate_rsi(svc_inst, ad_rsi);
		if (err != 0) {
			shell_error(ctx_shell, "Failed to generate RSI (err %d)", err);
			return err;
		}

		__ASSERT(data_array_size > ad_len, "No space for AD_RSI");
		data_array[ad_len].type = BT_DATA_CSIS_RSI;
		data_array[ad_len].data_len = ARRAY_SIZE(ad_rsi);
		data_array[ad_len].data = &ad_rsi[0];
		ad_len++;
	}

	return ad_len;
}
