/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <drivers/bluetooth/hci_driver.h>
#include <init.h>
#include <kernel.h>

#include <irq.h>
#include <soc.h>

#include "blectlr.h"

#define BT_DBG_ENABLED IS_ENABLED(CONFIG_BT_DEBUG_HCI_DRIVER)
#include "common/log.h"

static K_SEM_DEFINE(sem_recv, 0, UINT_MAX);

bool hci_data_packet_put(u8_t const *buffer);

bool hci_cmd_packet_put(u8_t const *buffer);

bool hci_event_packet_get(u8_t const *buffer);

bool hci_data_packet_get(u8_t *buffer);

void blectlr_set_default_evt_length(void);

void cal_init(void);

static int cmd_handle(struct net_buf *cmd)
{
	if (hci_cmd_packet_put(cmd->data)) {
		k_sem_give(&sem_recv);
	} else {
		return -ENOBUFS;
	}

	return 0;
}

void blectlr_assertion_handler(const char *const file, const u32_t line)
{
#ifdef CONFIG_BT_CTLR_ASSERT_HANDLER
	bt_ctlr_assert_handle(file, line);
#else
	BT_ERR("BleCtlr ASSERT: %s, %d", file, line);
	k_oops();
#endif
}

static int acl_handle(struct net_buf *acl)
{
	if (hci_data_packet_put(acl->data)) {
		/* Succeeded */
		return 0;
	}

	/* Likely buffer overflow event */
	k_sem_give(&sem_recv);

	return -ENOBUFS;
}

static int hci_driver_send(struct net_buf *buf)
{
	u8_t type;
	int err;

	BT_DBG("Enter");

	if (!buf->len) {
		BT_DBG("Empty HCI packet");
		return -EINVAL;
	}

	type = bt_buf_get_type(buf);
	switch (type) {
#if defined(CONFIG_BT_CONN)
	case BT_BUF_ACL_OUT:
		BT_DBG("ACL_OUT");
		err = acl_handle(buf);
		break;
#endif /* CONFIG_BT_CONN */
	case BT_BUF_CMD:
		BT_DBG("CMD");
		err = cmd_handle(buf);
		break;
	default:
		BT_DBG("Unknown HCI type %u", type);
		return -EINVAL;
	}

	if (!err) {
		net_buf_unref(buf);
	}

	BT_DBG("Exit");
	return 0;
}
#ifndef CONFIG_BT_RX_STACK_SIZE
#define CONFIG_BT_RX_STACK_SIZE (1000)
#endif
struct k_thread recv_thread_data;
static BT_STACK_NOINIT(recv_thread_stack, CONFIG_BT_RX_STACK_SIZE);

static void recv_thread(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	static u8_t hci_buffer[256 + 4];

	BT_DBG("Started");

	while (1) {
		k_sem_take(&sem_recv, K_FOREVER);

		if (hci_data_packet_get(hci_buffer)) {
			struct net_buf *data_buf =
			    bt_buf_get_rx(BT_BUF_ACL_IN, K_FOREVER);
			if (data_buf) {
				u16_t handle =
				    hci_buffer[0] | (hci_buffer[1] & 0xF) << 8;
				u8_t pb_flag = (hci_buffer[1] >> 4) & 0x3;
				u8_t bc_flag = (hci_buffer[1] >> 6) & 0x3;
				u16_t data_length =
				    hci_buffer[2] | hci_buffer[3] << 8;
				BT_DBG("Data: Handle(%02x), PB(%01d), "
				       "BC(%01d), Length(%02x)",
				       handle, pb_flag, bc_flag, data_length);
				net_buf_add_mem(data_buf, &hci_buffer[0],
						data_length + 4);
				bt_recv(data_buf);
			} else {
				BT_ERR("No data buffer available");
			}
		}

		if (hci_event_packet_get(hci_buffer)) {
			struct bt_hci_evt_hdr *hdr = (void *)hci_buffer;
			struct net_buf *evt_buf;

			if (hdr->evt == BT_HCI_EVT_CMD_COMPLETE ||
			    hdr->evt == BT_HCI_EVT_CMD_STATUS) {
				u16_t opcode = hci_buffer[3] | hci_buffer[4]
								   << 8;
				if (opcode == 0xC03) {
					BT_DBG("Reset command complete");
					cal_init();
					blectlr_set_default_evt_length();
				}

				evt_buf = bt_buf_get_cmd_complete(K_FOREVER);
			} else {
				evt_buf = bt_buf_get_rx(BT_BUF_EVT, K_FOREVER);
			}

			if (evt_buf) {
				if (hdr->evt == 0x3E) {
					BT_DBG("LE Meta Event: subevent code "
					       "(%02x), length (%d)",
					       hci_buffer[2], hci_buffer[1]);
				} else {
					BT_DBG("Event: event code (%02x), "
					       "length (%d)",
					       hci_buffer[0], hci_buffer[1]);
				}

				net_buf_add_mem(evt_buf, &hci_buffer[0],
						hdr->len + 2);
				if (bt_hci_evt_is_prio(hdr->evt)) {
					bt_recv_prio(evt_buf);
				} else {
					bt_recv(evt_buf);
				}
			} else {
				BT_ERR("No event buffer available");
			}
		}

		/* Let other threads of same priority run in between. */
		k_yield();
	}
}

static K_SEM_DEFINE(sem_signal, 0, UINT_MAX);
void _signal_handler_irq(void)
{
	k_sem_give(&sem_recv);
}
#ifndef CONFIG_SIGNAL_HANDLER_STACK_SIZE
#define CONFIG_SIGNAL_HANDLER_STACK_SIZE (300)
#endif
struct k_thread signal_thread_data;
static BT_STACK_NOINIT(signal_thread_stack, CONFIG_SIGNAL_HANDLER_STACK_SIZE);

static void signal_thread(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	while (true) {
		k_sem_take(&sem_signal, K_FOREVER);
		blectlr_signal();
	}
}

static int hci_driver_open(void)
{
	BT_DBG("Open");

	k_thread_create(&recv_thread_data, recv_thread_stack,
			K_THREAD_STACK_SIZEOF(recv_thread_stack), recv_thread,
			NULL, NULL, NULL, K_PRIO_COOP(CONFIG_BT_RX_PRIO), 0,
			K_NO_WAIT);

	k_thread_create(&signal_thread_data, signal_thread_stack,
			K_THREAD_STACK_SIZEOF(signal_thread_stack),
			signal_thread, NULL, NULL, NULL,
			K_PRIO_COOP(CONFIG_BT_RX_PRIO), 0, K_NO_WAIT);

	return 0;
}

static const struct bt_hci_driver drv = {
	.name = "Controller",
	.bus = BT_HCI_DRIVER_BUS_VIRTUAL,
	.open = hci_driver_open,
	.send = hci_driver_send,
};

void host_signal(void)
{
	/* Wake up the RX event/data thread */
	k_sem_give(&sem_recv);
}

void SIGNALLING_Handler(void)
{
	k_sem_give(&sem_signal);
}

static int _hci_driver_init(struct device *unused)
{
	ARG_UNUSED(unused);

	u32_t errcode;

	errcode = blectlr_init(host_signal);
	if (errcode) {
		/* Probably memory */
		return -ENOMEM;
	}

	bt_hci_driver_register(&drv);
	IRQ_DIRECT_CONNECT(NRF5_IRQ_RADIO_IRQn, 0, C_RADIO_Handler,
			   IRQ_ZERO_LATENCY);
	IRQ_DIRECT_CONNECT(NRF5_IRQ_RTC0_IRQn, 0, C_RTC0_Handler,
			   IRQ_ZERO_LATENCY);
	IRQ_DIRECT_CONNECT(NRF5_IRQ_TIMER0_IRQn, 0, C_TIMER0_Handler,
			   IRQ_ZERO_LATENCY);
	IRQ_CONNECT(NRF5_IRQ_SWI5_IRQn, 4, SIGNALLING_Handler, NULL, 0);
	IRQ_DIRECT_CONNECT(NRF5_IRQ_RNG_IRQn, 4, C_RNG_Handler, 0);
	IRQ_DIRECT_CONNECT(NRF5_IRQ_POWER_CLOCK_IRQn, 4, C_POWER_CLOCK_Handler,
			   0);

	irq_enable(NRF5_IRQ_RADIO_IRQn);
	irq_enable(NRF5_IRQ_RTC0_IRQn);
	irq_enable(NRF5_IRQ_TIMER0_IRQn);
	irq_enable(NRF5_IRQ_SWI5_IRQn);
	irq_enable(NRF5_IRQ_RNG_IRQn);
	irq_enable(NRF5_IRQ_POWER_CLOCK_IRQn);

	return 0;
}

SYS_INIT(_hci_driver_init, POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEVICE);
