/* ipm_stm32wb.c - HCI driver for stm32wb shared ram */

/*
 * Copyright (c) 2019 Linaro Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */


#include <init.h>
#include <sys/util.h>
#include <bluetooth/hci.h>
#include <drivers/bluetooth/hci_driver.h>
#include "bluetooth/addr.h"

#include "app_conf.h"
#include "stm32_wpan_common.h"
#include "shci.h"
#include "shci_tl.h"

#define POOL_SIZE (CFG_TLBLE_EVT_QUEUE_LENGTH * 4 * \
		DIVC((sizeof(TL_PacketHeader_t) + TL_BLE_EVENT_FRAME_SIZE), 4))

/* Private variables ---------------------------------------------------------*/
PLACE_IN_SECTION("MB_MEM1") ALIGN(4) static TL_CmdPacket_t BleCmdBuffer;
PLACE_IN_SECTION("MB_MEM2") ALIGN(4) static u8_t EvtPool[POOL_SIZE];
PLACE_IN_SECTION("MB_MEM2") ALIGN(4) static TL_CmdPacket_t SystemCmdBuffer;
PLACE_IN_SECTION("MB_MEM2") ALIGN(4) static u8_t
	SystemSpareEvtBuffer[sizeof(TL_PacketHeader_t) + TL_EVT_HDR_SIZE + 255];
PLACE_IN_SECTION("MB_MEM2") ALIGN(4) static u8_t
	BleSpareEvtBuffer[sizeof(TL_PacketHeader_t) + TL_EVT_HDR_SIZE + 255];
PLACE_IN_SECTION("MB_MEM2") ALIGN(4) static u8_t
	HciAclDataBuffer[sizeof(TL_PacketHeader_t) + 5 + 251];

static void syscmd_status_not(SHCI_TL_CmdStatus_t status);
static void sysevt_received(void *pdata);

#define BT_DBG_ENABLED IS_ENABLED(CONFIG_BT_DEBUG_HCI_DRIVER)
#define LOG_MODULE_NAME hci_ipm
#include "common/log.h"

#define HCI_CMD                 0x01
#define HCI_ACL                 0x02
#define HCI_SCO                 0x03
#define HCI_EVT                 0x04

static K_SEM_DEFINE(c2_started, 0, 1);
static K_SEM_DEFINE(ble_sys_wait_cmd_rsp, 0, 1);
static K_SEM_DEFINE(acl_data_ack, 1, 1);
static K_SEM_DEFINE(ipm_busy, 1, 1);

struct aci_set_tx_power {
	u8_t cmd;
	u8_t value[2];
};

struct aci_set_ble_addr {
	u8_t config_offset;
	u8_t length;
	u8_t value[6];
} __packed;

#define ACI_WRITE_SET_TX_POWER_LEVEL       BT_OP(BT_OGF_VS, 0xFC0F)
#define ACI_HAL_WRITE_CONFIG_DATA	   BT_OP(BT_OGF_VS, 0xFC0C)

#define HCI_CONFIG_DATA_PUBADDR_OFFSET		0
#define HCI_CONFIG_DATA_RANDOM_ADDRESS_OFFSET	0x2E

static bt_addr_t bd_addr_udn;

/* Rx thread definitions */
K_FIFO_DEFINE(ipm_rx_events_fifo);
static K_THREAD_STACK_DEFINE(ipm_rx_stack, CONFIG_BT_STM32_IPM_RX_STACK_SIZE);
static struct k_thread ipm_rx_thread_data;

static void stm32wb_start_ble(void)
{
	SHCI_C2_Ble_Init_Cmd_Packet_t ble_init_cmd_packet = {
	  { { 0, 0, 0 } },                     /**< Header unused */
	  { 0,                                 /** pBleBufferAddress not used */
	    0,                                 /** BleBufferSize not used */
	    CFG_BLE_NUM_GATT_ATTRIBUTES,
	    CFG_BLE_NUM_GATT_SERVICES,
	    CFG_BLE_ATT_VALUE_ARRAY_SIZE,
	    CFG_BLE_NUM_LINK,
	    CFG_BLE_DATA_LENGTH_EXTENSION,
	    CFG_BLE_PREPARE_WRITE_LIST_SIZE,
	    CFG_BLE_MBLOCK_COUNT,
	    CFG_BLE_MAX_ATT_MTU,
	    CFG_BLE_SLAVE_SCA,
	    CFG_BLE_MASTER_SCA,
	    CFG_BLE_LSE_SOURCE,
	    CFG_BLE_MAX_CONN_EVENT_LENGTH,
	    CFG_BLE_HSE_STARTUP_TIME,
	    CFG_BLE_VITERBI_MODE,
	    CFG_BLE_LL_ONLY,
	    0 }
	};

	/**
	 * Starts the BLE Stack on CPU2
	 */
	SHCI_C2_BLE_Init(&ble_init_cmd_packet);
}

static void sysevt_received(void *pdata)
{
	k_sem_give(&c2_started);
}

static void syscmd_status_not(SHCI_TL_CmdStatus_t status)
{
	BT_DBG("status:%d", status);
}

/*
 * https://github.com/zephyrproject-rtos/zephyr/issues/19509
 * Tested on nucleo_wb55rg (stm32wb55rg) BLE stack (v1.2.0)
 * Unresolved Resolvable Private Addresses (RPA)
 * is reported in the peer_rpa field, and not in the peer address,
 * as it should, when this happens the peer address is set to all FFs
 * 0A 00 01 08 01 01 FF FF FF FF FF FF 00 00 00 00 00 00 0C AA C5 B3 3D 6B ...
 * If such message is passed to HCI core than pairing will essentially fail.
 * Solution: Rewrite the event with the RPA in the PEER address field
 */
static void tryfix_event(TL_Evt_t *tev)
{
	struct bt_hci_evt_le_meta_event *mev = (void *)&tev->payload;

	if (tev->evtcode != BT_HCI_EVT_LE_META_EVENT ||
	    mev->subevent != BT_HCI_EVT_LE_ENH_CONN_COMPLETE) {
		return;
	}

	struct bt_hci_evt_le_enh_conn_complete *evt =
			(void *)((u8_t *)mev + (sizeof(*mev)));

	if (!bt_addr_cmp(&evt->peer_addr.a, BT_ADDR_NONE)) {
		BT_WARN("Invalid peer addr %s", bt_addr_le_str(&evt->peer_addr));
		bt_addr_copy(&evt->peer_addr.a, &evt->peer_rpa);
		evt->peer_addr.type = BT_ADDR_LE_RANDOM;
	}
}

void TM_EvtReceivedCb(TL_EvtPacket_t *hcievt)
{
	k_fifo_put(&ipm_rx_events_fifo, hcievt);
}

static void bt_ipm_rx_thread(void)
{
	while (true) {
		static TL_EvtPacket_t *hcievt;
		struct net_buf *buf = NULL;
		struct bt_hci_acl_hdr acl_hdr;
		TL_AclDataSerial_t *acl;

		hcievt = k_fifo_get(&ipm_rx_events_fifo, K_FOREVER);

		k_sem_take(&ipm_busy, K_FOREVER);

		switch (hcievt->evtserial.type) {
		case HCI_EVT:
			BT_DBG("EVT: hcievt->evtserial.evt.evtcode: 0x%02x",
			       hcievt->evtserial.evt.evtcode);
			switch (hcievt->evtserial.evt.evtcode) {
			case BT_HCI_EVT_VENDOR:
				/* Vendor events are currently unsupported */
				BT_ERR("Unknown evtcode type 0x%02x",
				       hcievt->evtserial.evt.evtcode);
				TL_MM_EvtDone(hcievt);
				goto end_loop;
			default:
				buf = bt_buf_get_evt(
					hcievt->evtserial.evt.evtcode,
					false, K_FOREVER);
			}
			tryfix_event(&hcievt->evtserial.evt);
			net_buf_add_mem(buf, &hcievt->evtserial.evt,
					hcievt->evtserial.evt.plen + 2);
			break;
		case HCI_ACL:
			acl = &(((TL_AclDataPacket_t *)hcievt)->AclDataSerial);
			buf = bt_buf_get_rx(BT_BUF_ACL_IN, K_FOREVER);
			acl_hdr.handle = acl->handle;
			acl_hdr.len = acl->length;
			BT_DBG("ACL: handle %x, len %x",
			       acl_hdr.handle, acl_hdr.len);
			net_buf_add_mem(buf, &acl_hdr, sizeof(acl_hdr));
			net_buf_add_mem(buf, (u8_t *)&acl->acl_data,
					acl_hdr.len);
			break;
		default:
			BT_ERR("Unknown BT buf type %d",
			       hcievt->evtserial.type);
			TL_MM_EvtDone(hcievt);
			goto end_loop;
		}

		TL_MM_EvtDone(hcievt);

		bt_recv(buf);
end_loop:
		k_sem_give(&ipm_busy);
	}

}

static void TM_AclDataAck(void)
{
	k_sem_give(&acl_data_ack);
}

void shci_notify_asynch_evt(void *pdata)
{
	shci_user_evt_proc();
}

void shci_cmd_resp_release(uint32_t flag)
{
	k_sem_give(&ble_sys_wait_cmd_rsp);
}

void shci_cmd_resp_wait(uint32_t timeout)
{
	k_sem_take(&ble_sys_wait_cmd_rsp, K_MSEC(timeout));
}

void ipcc_reset(void)
{
	/* Reset IPCC */
	LL_AHB3_GRP1_EnableClock(LL_AHB3_GRP1_PERIPH_IPCC);

	LL_C1_IPCC_ClearFlag_CHx(
		IPCC,
		LL_IPCC_CHANNEL_1 | LL_IPCC_CHANNEL_2 | LL_IPCC_CHANNEL_3 |
		LL_IPCC_CHANNEL_4 | LL_IPCC_CHANNEL_5 | LL_IPCC_CHANNEL_6);

	LL_C2_IPCC_ClearFlag_CHx(
		IPCC,
		LL_IPCC_CHANNEL_1 | LL_IPCC_CHANNEL_2 | LL_IPCC_CHANNEL_3 |
		LL_IPCC_CHANNEL_4 | LL_IPCC_CHANNEL_5 | LL_IPCC_CHANNEL_6);

	LL_C1_IPCC_DisableTransmitChannel(
		IPCC,
		LL_IPCC_CHANNEL_1 | LL_IPCC_CHANNEL_2 | LL_IPCC_CHANNEL_3 |
		LL_IPCC_CHANNEL_4 | LL_IPCC_CHANNEL_5 | LL_IPCC_CHANNEL_6);

	LL_C2_IPCC_DisableTransmitChannel(
		IPCC,
		LL_IPCC_CHANNEL_1 | LL_IPCC_CHANNEL_2 | LL_IPCC_CHANNEL_3 |
		LL_IPCC_CHANNEL_4 | LL_IPCC_CHANNEL_5 | LL_IPCC_CHANNEL_6);

	LL_C1_IPCC_DisableReceiveChannel(
		IPCC,
		LL_IPCC_CHANNEL_1 | LL_IPCC_CHANNEL_2 | LL_IPCC_CHANNEL_3 |
		LL_IPCC_CHANNEL_4 | LL_IPCC_CHANNEL_5 | LL_IPCC_CHANNEL_6);

	LL_C2_IPCC_DisableReceiveChannel(
		IPCC,
		LL_IPCC_CHANNEL_1 | LL_IPCC_CHANNEL_2 | LL_IPCC_CHANNEL_3 |
		LL_IPCC_CHANNEL_4 | LL_IPCC_CHANNEL_5 | LL_IPCC_CHANNEL_6);

	/* Set IPCC default IRQ handlers */
	IRQ_CONNECT(IPCC_C1_RX_IRQn, 0, HW_IPCC_Rx_Handler, NULL, 0);
	IRQ_CONNECT(IPCC_C1_TX_IRQn, 0, HW_IPCC_Tx_Handler, NULL, 0);
}

void transport_init(void)
{
	TL_MM_Config_t tl_mm_config;
	TL_BLE_InitConf_t tl_ble_config;
	SHCI_TL_HciInitConf_t shci_init_config;

	BT_DBG("BleCmdBuffer: %p", (void *)&BleCmdBuffer);
	BT_DBG("HciAclDataBuffer: %p", (void *)&HciAclDataBuffer);
	BT_DBG("SystemCmdBuffer: %p", (void *)&SystemCmdBuffer);
	BT_DBG("EvtPool: %p", (void *)&EvtPool);
	BT_DBG("SystemSpareEvtBuffer: %p", (void *)&SystemSpareEvtBuffer);
	BT_DBG("BleSpareEvtBuffer: %p", (void *)&BleSpareEvtBuffer);

	/**< Reference table initialization */
	TL_Init();

	/**< System channel initialization */
	shci_init_config.p_cmdbuffer = (u8_t *)&SystemCmdBuffer;
	shci_init_config.StatusNotCallBack = syscmd_status_not;
	shci_init(sysevt_received, (void *) &shci_init_config);

	/**< Memory Manager channel initialization */
	tl_mm_config.p_BleSpareEvtBuffer = BleSpareEvtBuffer;
	tl_mm_config.p_SystemSpareEvtBuffer = SystemSpareEvtBuffer;
	tl_mm_config.p_AsynchEvtPool = EvtPool;
	tl_mm_config.AsynchEvtPoolSize = POOL_SIZE;
	TL_MM_Init(&tl_mm_config);

	/**< BLE channel initialization */
	tl_ble_config.p_cmdbuffer = (u8_t *)&BleCmdBuffer;
	tl_ble_config.p_AclDataBuffer = HciAclDataBuffer;
	tl_ble_config.IoBusEvtCallBack = TM_EvtReceivedCb;
	tl_ble_config.IoBusAclDataTxAck = TM_AclDataAck;
	TL_BLE_Init((void *)&tl_ble_config);

	TL_Enable();
}

static int bt_ipm_send(struct net_buf *buf)
{
	TL_CmdPacket_t *ble_cmd_buff = &BleCmdBuffer;

	k_sem_take(&ipm_busy, K_FOREVER);

	switch (bt_buf_get_type(buf)) {
	case BT_BUF_ACL_OUT:
		BT_DBG("ACL: buf %p type %u len %u", buf, bt_buf_get_type(buf),
		       buf->len);
		k_sem_take(&acl_data_ack, K_FOREVER);
		net_buf_push_u8(buf, HCI_ACL);
		memcpy((void *)
		       &((TL_AclDataPacket_t *)HciAclDataBuffer)->AclDataSerial,
		       buf->data, buf->len);
		TL_BLE_SendAclData(NULL, 0);
		break;
	case BT_BUF_CMD:
		BT_DBG("CMD: buf %p type %u len %u", buf, bt_buf_get_type(buf),
		       buf->len);
		ble_cmd_buff->cmdserial.type = HCI_CMD;
		ble_cmd_buff->cmdserial.cmd.plen = buf->len;
		memcpy((void *)&ble_cmd_buff->cmdserial.cmd, buf->data,
		       buf->len);
		TL_BLE_SendCmd(NULL, 0);
		break;
	default:
		k_sem_give(&ipm_busy);
		BT_ERR("Unsupported type");
		return -EINVAL;
	}

	k_sem_give(&ipm_busy);

	net_buf_unref(buf);

	return 0;
}

static void start_ble_rf(void)
{
	if ((LL_RCC_IsActiveFlag_PINRST()) && (!LL_RCC_IsActiveFlag_SFTRST())) {
		/* Simulate power off reset */
		LL_PWR_EnableBkUpAccess();
		LL_PWR_EnableBkUpAccess();
		LL_RCC_ForceBackupDomainReset();
		LL_RCC_ReleaseBackupDomainReset();
	}

#ifdef CONFIG_CLOCK_STM32_LSE
	/* Select LSE clock */
	LL_RCC_LSE_Enable();
	while (!LL_RCC_LSE_IsReady()) {
	}

	/* Select wakeup source of BLE RF */
	LL_RCC_SetRFWKPClockSource(LL_RCC_RFWKP_CLKSOURCE_LSE);
	LL_RCC_SetRTCClockSource(LL_RCC_RTC_CLKSOURCE_LSE);

	/* Switch OFF LSI */
	LL_RCC_LSI2_Disable();
#else
	LL_RCC_LSI2_Enable();
	while (!LL_RCC_LSI2_IsReady()) {
	}

	/* Select wakeup source of BLE RF */
	LL_RCC_SetRFWKPClockSource(LL_RCC_RFWKP_CLKSOURCE_LSI);
	LL_RCC_SetRTCClockSource(LL_RCC_RTC_CLKSOURCE_LSI);
#endif

	/* Set RNG on HSI48 */
	LL_RCC_HSI48_Enable();
	while (!LL_RCC_HSI48_IsReady()) {
	}

	LL_RCC_SetCLK48ClockSource(LL_RCC_CLK48_CLKSOURCE_HSI48);
}

bt_addr_t *bt_get_ble_addr(void)
{
	bt_addr_t *bd_addr;
	u32_t udn;
	u32_t company_id;
	u32_t device_id;

	/* Get the 64 bit Unique Device Number UID */
	/* The UID is used by firmware to derive   */
	/* 48-bit Device Address EUI-48 */
	udn = LL_FLASH_GetUDN();

	if (udn != 0xFFFFFFFF) {
		/* Get the ST Company ID */
		company_id = LL_FLASH_GetSTCompanyID();
		/* Get the STM32 Device ID */
		device_id = LL_FLASH_GetDeviceID();
		bd_addr_udn.val[0] = (uint8_t)(udn & 0x000000FF);
		bd_addr_udn.val[1] = (uint8_t)((udn & 0x0000FF00) >> 8);
		bd_addr_udn.val[2] = (uint8_t)((udn & 0x00FF0000) >> 16);
		bd_addr_udn.val[3] = (uint8_t)device_id;
		bd_addr_udn.val[4] = (uint8_t)(company_id & 0x000000FF);
		bd_addr_udn.val[5] = (uint8_t)((company_id & 0x0000FF00) >> 8);
		bd_addr = &bd_addr_udn;
	} else {
		bd_addr = NULL;
	}

	return bd_addr;
}

static int bt_ipm_set_addr(void)
{
	bt_addr_t *uid_addr;
	struct aci_set_ble_addr *param;
	struct net_buf *buf, *rsp;
	int err;

	uid_addr = bt_get_ble_addr();
	if (!uid_addr) {
		return -ENOMSG;
	}

	buf = bt_hci_cmd_create(ACI_HAL_WRITE_CONFIG_DATA, sizeof(*param));

	if (!buf) {
		return -ENOBUFS;
	}

	param = net_buf_add(buf, sizeof(*param));
	param->config_offset = HCI_CONFIG_DATA_PUBADDR_OFFSET;
	param->length = 6;
	param->value[0] = uid_addr->val[0];
	param->value[1] = uid_addr->val[1];
	param->value[2] = uid_addr->val[2];
	param->value[3] = uid_addr->val[3];
	param->value[4] = uid_addr->val[4];
	param->value[5] = uid_addr->val[5];

	err = bt_hci_cmd_send_sync(ACI_HAL_WRITE_CONFIG_DATA, buf, &rsp);
	if (err) {
		return err;
	}
	net_buf_unref(rsp);
	return 0;
}

static int bt_ipm_ble_init(void)
{
	struct aci_set_tx_power *param;
	struct net_buf *buf, *rsp;
	int err;

	/* Send HCI_RESET */
	err = bt_hci_cmd_send_sync(BT_HCI_OP_RESET, NULL, &rsp);
	if (err) {
		return err;
	}
	/* TDB: Something to do on reset complete? */
	net_buf_unref(rsp);
	err = bt_ipm_set_addr();
	if (err) {
		BT_ERR("Can't set BLE UID addr");
	}
	/* Send ACI_WRITE_SET_TX_POWER_LEVEL */
	buf = bt_hci_cmd_create(ACI_WRITE_SET_TX_POWER_LEVEL, 3);
	if (!buf) {
		return -ENOBUFS;
	}
	param = net_buf_add(buf, sizeof(*param));
	param->cmd = 0x0F;
	param->value[0] = 0x18;
	param->value[1] = 0x01;

	err = bt_hci_cmd_send_sync(ACI_WRITE_SET_TX_POWER_LEVEL, buf, &rsp);
	if (err) {
		return err;
	}
	net_buf_unref(rsp);

	return 0;
}

static int bt_ipm_open(void)
{
	int err;

	/* Start RX thread */
	k_thread_create(&ipm_rx_thread_data, ipm_rx_stack,
			K_THREAD_STACK_SIZEOF(ipm_rx_stack),
			(k_thread_entry_t)bt_ipm_rx_thread, NULL, NULL, NULL,
			K_PRIO_COOP(CONFIG_BT_RX_PRIO - 1),
			0, K_NO_WAIT);

	/* Take BLE out of reset */
	ipcc_reset();

	transport_init();

	/* Device will let us know when it's ready */
	k_sem_take(&c2_started, K_FOREVER);
	BT_DBG("C2 unlocked");

	stm32wb_start_ble();

	BT_DBG("IPM Channel Open Completed");

	err = bt_ipm_ble_init();
	if (err) {
		return err;
	}

	return 0;
}

static const struct bt_hci_driver drv = {
	.name           = "BT IPM",
	.bus            = BT_HCI_DRIVER_BUS_IPM,
	.quirks         = BT_QUIRK_NO_RESET,
	.open           = bt_ipm_open,
	.send           = bt_ipm_send,
};

static int _bt_ipm_init(struct device *unused)
{
	ARG_UNUSED(unused);

	bt_hci_driver_register(&drv);

	start_ble_rf();
	return 0;
}

SYS_INIT(_bt_ipm_init, POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEVICE);
