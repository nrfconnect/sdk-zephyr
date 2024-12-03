/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef TEST_COMMANDS_H
#include <stdint.h>

/**
 * @brief Test commands executable by remote
 */
enum ipc_test_commands {
	IPC_TEST_CMD_NONE,     /**< Command to be ingored */
	IPC_TEST_CMD_PING,     /**< Respond with the @ref IPC_TEST_CMD_PONG message */
	IPC_TEST_CMD_PONG,     /**< Expected response to IPC_TEST_CMD_PING */
	IPC_TEST_CMD_ECHO,     /**< Respond with the same data */
	IPC_TEST_CMD_ECHO_RSP, /**< Echo respond */
	IPC_TEST_CMD_REBOND,   /**< Unbond and rebond back whole interface */
	IPC_TEST_CMD_REBOOT,   /**< Restart remote CPU after a given delay */
};

/**
 * @brief Base command structure
 */
struct ipc_test_cmd {
	uint32_t cmd;   /**< The command of @ref ipc_test_command type */
	uint8_t data[]; /**< Command data depending on the command itself */
};

/**
 * @brief Rebond command structure
 */
struct ipc_test_cmd_rebond {
	struct ipc_test_cmd base;
	uint32_t timeout_ms;
};

/**
 * @brief Reboot command structure
 */
struct ipc_test_cmd_reboot {
	struct ipc_test_cmd base;
	uint32_t timeout_ms;
};

#endif /* TEST_COMMANDS_H */
