/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <zephyr/bluetooth/hci_fatal_report.h>
#include <zephyr/cache.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/mbox.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/barrier.h>
#include <zephyr/sys/util.h>

LOG_MODULE_REGISTER(bt_hci_fatal_report, CONFIG_BT_HCI_FATAL_REPORT_LOG_LEVEL);

#define REPORT_NODE DT_COMPAT_GET_ANY_STATUS_OKAY(zephyr_bt_hci_fatal_report)
#define REGION_NODE DT_PHANDLE(REPORT_NODE, memory_region)

#define REGION_ADDR DT_REG_ADDR(REGION_NODE)
#define REGION_SIZE DT_REG_SIZE(REGION_NODE)

/* Where the region is cached, the length word has to sit in a cache line of its own. The two are
 * flushed separately to keep the report visible to the reader before the length that publishes it,
 * and that ordering is lost if a single line covers both.
 */
#define REPORT_DCACHE_ALIGNMENT DT_PROP_OR(REPORT_NODE, dcache_alignment, sizeof(uint32_t))

#define REPORT_HEADER_SIZE MAX(REPORT_DCACHE_ALIGNMENT, sizeof(uint32_t))

/** Largest report the region can hold. */
#define REPORT_MAX_LEN (REGION_SIZE - REPORT_HEADER_SIZE)

BUILD_ASSERT(REGION_SIZE > REPORT_HEADER_SIZE, "Shared memory region leaves no room for a report.");
BUILD_ASSERT((REGION_ADDR % REPORT_DCACHE_ALIGNMENT) == 0,
	     "Shared memory region is not aligned to the data cache line size.");

/* Length of the report, and zero when there is none. Written last by the sender and read first by
 * the receiver, so it is what publishes the report.
 */
static volatile uint32_t *const report_len = (volatile uint32_t *)REGION_ADDR;
static uint8_t *const report_data = (uint8_t *)(REGION_ADDR + REPORT_HEADER_SIZE);

static const struct mbox_dt_spec report_mbox = MBOX_DT_SPEC_GET(REPORT_NODE, report);

/* Set once the shared memory has been cleared, which is all the preparation sending needs. Read
 * from a fault handler, so it deliberately does not depend on anything the kernel owns.
 */
static bool tx_enabled;

static bt_hci_fatal_report_cb_t report_cb;
static void *report_cb_user_data;

static void report_len_set(uint32_t len)
{
	*report_len = len;
	barrier_dmem_fence_full();
	sys_cache_data_flush_range((void *)report_len, sizeof(*report_len));
}

static uint32_t report_len_get(void)
{
	sys_cache_data_invd_range((void *)report_len, sizeof(*report_len));
	barrier_dmem_fence_full();

	return *report_len;
}

int bt_hci_fatal_report_tx_enable(void)
{
	report_len_set(0);

	tx_enabled = true;

	return 0;
}

int bt_hci_fatal_report_send(const void *data, size_t len)
{
	if ((data == NULL) || (len == 0)) {
		return -EINVAL;
	}

	if (len > REPORT_MAX_LEN) {
		return -ENOMEM;
	}

	if (!tx_enabled) {
		return -EIO;
	}

	/* Everything below is a memory copy, a cache maintenance operation and a register write.
	 * Nothing takes a lock, allocates, or hands work to a thread, which is what makes this
	 * usable once the kernel can no longer be trusted.
	 */
	memcpy(report_data, data, len);
	sys_cache_data_flush_range(report_data, len);

	/* Publishes the report. The reader sees a complete one or none at all. */
	report_len_set(len);

	return mbox_send_dt(&report_mbox, NULL);
}

static void report_take(struct k_work *work)
{
	uint32_t len;

	ARG_UNUSED(work);

	len = report_len_get();
	if (len == 0) {
		return;
	}

	if (len > REPORT_MAX_LEN) {
		LOG_ERR("Discarding a report of %u bytes, which cannot have been written here", len);
		report_len_set(0);
		return;
	}

	sys_cache_data_invd_range(report_data, len);

	if (report_cb != NULL) {
		report_cb(report_data, len, report_cb_user_data);
	}

	/* Only now that the report has been passed on may the sender overwrite it. In practice the
	 * sender is a faulted core waiting to be reset, so this is for the next session.
	 */
	report_len_set(0);
}

static K_WORK_DEFINE(report_work, report_take);

static void report_signalled(const struct device *dev, mbox_channel_id_t channel_id,
			     void *user_data, struct mbox_msg *msg)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(channel_id);
	ARG_UNUSED(user_data);
	ARG_UNUSED(msg);

	/* The sender has faulted and is not going to send anything else, so there is no need to
	 * take the report from interrupt context.
	 */
	k_work_submit(&report_work);
}

int bt_hci_fatal_report_rx_enable(bt_hci_fatal_report_cb_t cb, void *user_data)
{
	int err;

	if (cb == NULL) {
		return -EINVAL;
	}

	report_cb = cb;
	report_cb_user_data = user_data;

	err = mbox_register_callback_dt(&report_mbox, report_signalled, NULL);
	if (err < 0) {
		return err;
	}

	err = mbox_set_enabled_dt(&report_mbox, 1);
	if ((err < 0) && (err != -EALREADY)) {
		return err;
	}

	/* A controller that faulted before this point signalled a mailbox nobody was listening to,
	 * but its report is still sitting in the shared memory. Look for it rather than lose it.
	 */
	k_work_submit(&report_work);

	return 0;
}

int bt_hci_fatal_report_rx_disable(void)
{
	int err;

	err = mbox_set_enabled_dt(&report_mbox, 0);
	if ((err < 0) && (err != -EALREADY)) {
		return err;
	}

	report_cb = NULL;
	report_cb_user_data = NULL;

	return 0;
}
