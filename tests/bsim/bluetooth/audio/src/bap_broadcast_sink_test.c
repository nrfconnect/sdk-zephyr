/*
 * Copyright (c) 2021-2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#if defined(CONFIG_BT_BAP_BROADCAST_SINK)

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/audio/audio.h>
#include <zephyr/bluetooth/audio/bap.h>
#include <zephyr/bluetooth/audio/bap_lc3_preset.h>
#include <zephyr/bluetooth/audio/pacs.h>
#include "common.h"

extern enum bst_result_t bst_result;

CREATE_FLAG(broadcaster_found);
CREATE_FLAG(base_received);
CREATE_FLAG(flag_base_metadata_updated);
CREATE_FLAG(pa_synced);
CREATE_FLAG(flag_syncable);
CREATE_FLAG(pa_sync_lost);
CREATE_FLAG(flag_received);

static struct bt_bap_broadcast_sink *g_sink;
static struct bt_bap_stream broadcast_sink_streams[CONFIG_BT_BAP_BROADCAST_SNK_STREAM_COUNT];
static struct bt_bap_stream *streams[ARRAY_SIZE(broadcast_sink_streams)];
static struct bt_bap_lc3_preset preset_16_2_1 = BT_BAP_LC3_BROADCAST_PRESET_16_2_1(
	BT_AUDIO_LOCATION_FRONT_LEFT, BT_AUDIO_CONTEXT_TYPE_UNSPECIFIED);

static K_SEM_DEFINE(sem_started, 0U, ARRAY_SIZE(streams));
static K_SEM_DEFINE(sem_stopped, 0U, ARRAY_SIZE(streams));

static struct bt_codec_data metadata[CONFIG_BT_CODEC_MAX_METADATA_COUNT];

/* Create a mask for the maximum BIS we can sync to using the number of streams
 * we have. We add an additional 1 since the bis indexes start from 1 and not
 * 0.
 */
static const uint32_t bis_index_mask = BIT_MASK(ARRAY_SIZE(streams) + 1U);
static uint32_t bis_index_bitfield;

static bool scan_recv_cb(const struct bt_le_scan_recv_info *info,
			 struct net_buf_simple *ad,
			 uint32_t broadcast_id)
{
	SET_FLAG(broadcaster_found);

	return true;
}

static void scan_term_cb(int err)
{
	if (err != 0) {
		FAIL("Scan terminated with error: %d", err);
	}
}

static void pa_synced_cb(struct bt_bap_broadcast_sink *sink,
			 struct bt_le_per_adv_sync *sync,
			 uint32_t broadcast_id)
{
	if (g_sink != NULL) {
		FAIL("Unexpected PA sync");
		return;
	}

	printk("PA synced for broadcast sink %p with broadcast ID 0x%06X\n",
	       sink, broadcast_id);

	g_sink = sink;

	SET_FLAG(pa_synced);
}

static void base_recv_cb(struct bt_bap_broadcast_sink *sink, const struct bt_bap_base *base)
{
	uint32_t base_bis_index_bitfield = 0U;

	if (TEST_FLAG(base_received)) {

		if (base->subgroup_count > 0 &&
		    memcmp(metadata, base->subgroups[0].codec.meta,
			   sizeof(base->subgroups[0].codec.meta)) != 0) {

			(void)memcpy(metadata, base->subgroups[0].codec.meta,
				     sizeof(base->subgroups[0].codec.meta));

			SET_FLAG(flag_base_metadata_updated);
		}

		return;
	}

	printk("Received BASE with %u subgroups from broadcast sink %p\n",
	       base->subgroup_count, sink);


	for (size_t i = 0U; i < base->subgroup_count; i++) {
		for (size_t j = 0U; j < base->subgroups[i].bis_count; j++) {
			const uint8_t index = base->subgroups[i].bis_data[j].index;

			base_bis_index_bitfield |= BIT(index);
		}
	}

	bis_index_bitfield = base_bis_index_bitfield & bis_index_mask;

	SET_FLAG(base_received);
}

static void syncable_cb(struct bt_bap_broadcast_sink *sink, bool encrypted)
{
	printk("Broadcast sink %p syncable with%s encryption\n",
	       sink, encrypted ? "" : "out");
	SET_FLAG(flag_syncable);
}

static void pa_sync_lost_cb(struct bt_bap_broadcast_sink *sink)
{
	if (g_sink == NULL) {
		FAIL("Unexpected PA sync lost");
		return;
	}

	if (TEST_FLAG(pa_sync_lost)) {
		return;
	}

	printk("Sink %p disconnected\n", sink);

	g_sink = NULL;

	SET_FLAG(pa_sync_lost);
}

static struct bt_bap_broadcast_sink_cb broadcast_sink_cbs = {
	.scan_recv = scan_recv_cb,
	.scan_term = scan_term_cb,
	.base_recv = base_recv_cb,
	.pa_synced = pa_synced_cb,
	.syncable = syncable_cb,
	.pa_sync_lost = pa_sync_lost_cb
};

static struct bt_pacs_cap cap = {
	.codec = &preset_16_2_1.codec,
};

static void started_cb(struct bt_bap_stream *stream)
{
	printk("Stream %p started\n", stream);
	k_sem_give(&sem_started);
}

static void stopped_cb(struct bt_bap_stream *stream, uint8_t reason)
{
	printk("Stream %p stopped with reason 0x%02X\n", stream, reason);
	k_sem_give(&sem_stopped);
}

static void recv_cb(struct bt_bap_stream *stream,
		    const struct bt_iso_recv_info *info,
		    struct net_buf *buf)
{
	SET_FLAG(flag_received);
}

static struct bt_bap_stream_ops stream_ops = {
	.started = started_cb,
	.stopped = stopped_cb,
	.recv = recv_cb
};

static int init(void)
{
	int err;

	err = bt_enable(NULL);
	if (err) {
		FAIL("Bluetooth enable failed (err %d)\n", err);
		return err;
	}

	printk("Bluetooth initialized\n");

	err = bt_pacs_cap_register(BT_AUDIO_DIR_SINK, &cap);
	if (err) {
		FAIL("Capability register failed (err %d)\n", err);
		return err;
	}

	/* Test invalid input */
	err = bt_bap_broadcast_sink_register_cb(NULL);
	if (err == 0) {
		FAIL("bt_bap_broadcast_sink_register_cb did not fail with NULL cb\n");
		return err;
	}

	err = bt_bap_broadcast_sink_register_cb(&broadcast_sink_cbs);
	if (err != 0) {
		FAIL("Sink callback register failed (err %d)\n", err);
		return err;
	}

	UNSET_FLAG(broadcaster_found);
	UNSET_FLAG(base_received);
	UNSET_FLAG(pa_synced);

	for (size_t i = 0U; i < ARRAY_SIZE(streams); i++) {
		streams[i] = &broadcast_sink_streams[i];
		bt_bap_stream_cb_register(streams[i], &stream_ops);
	}

	return 0;
}

static void test_scan_and_pa_sync(void)
{
	int err;

	printk("Scanning for broadcast sources\n");
	err = bt_bap_broadcast_sink_scan_start(BT_LE_SCAN_ACTIVE);
	if (err != 0) {
		FAIL("Unable to start scan for broadcast sources: %d", err);
		return;
	}

	WAIT_FOR_FLAG(broadcaster_found);
	printk("Broadcast source found, waiting for PA sync\n");
	WAIT_FOR_FLAG(pa_synced);
	printk("Broadcast source PA synced, waiting for BASE\n");
	WAIT_FOR_FLAG(base_received);
	printk("BASE received\n");

	printk("Waiting for BIG syncable\n");
	WAIT_FOR_FLAG(flag_syncable);
}

static void test_scan_and_pa_sync_inval(void)
{
	int err;

	err = bt_bap_broadcast_sink_scan_start(NULL);
	if (err == 0) {
		FAIL("bt_bap_broadcast_sink_scan_start did not fail with NULL param\n");
		return;
	}
}

static void test_broadcast_sync(void)
{
	int err;

	printk("Syncing the sink\n");
	err = bt_bap_broadcast_sink_sync(g_sink, bis_index_bitfield, streams, NULL);
	if (err != 0) {
		FAIL("Unable to sync the sink: %d\n", err);
		return;
	}

	/* Wait for all to be started */
	printk("Waiting for streams to be started\n");
	for (size_t i = 0U; i < ARRAY_SIZE(streams); i++) {
		k_sem_take(&sem_started, K_FOREVER);
	}
}

static void test_broadcast_sync_inval(void)
{
	struct bt_bap_stream *tmp_streams[ARRAY_SIZE(streams) + 1] = {0};
	uint32_t bis_index;
	int err;

	err = bt_bap_broadcast_sink_sync(NULL, bis_index_bitfield, streams, NULL);
	if (err == 0) {
		FAIL("bt_bap_broadcast_sink_sync did not fail with NULL sink\n");
		return;
	}

	bis_index = 0;
	err = bt_bap_broadcast_sink_sync(g_sink, bis_index, streams, NULL);
	if (err == 0) {
		FAIL("bt_bap_broadcast_sink_sync did not fail with invalid BIS indexes: 0x%08X\n",
		     bis_index);
		return;
	}

	bis_index = BIT(0);
	err = bt_bap_broadcast_sink_sync(g_sink, bis_index, streams, NULL);
	if (err == 0) {
		FAIL("bt_bap_broadcast_sink_sync did not fail with invalid BIS indexes: 0x%08X\n",
		     bis_index);
		return;
	}

	err = bt_bap_broadcast_sink_sync(g_sink, bis_index, NULL, NULL);
	if (err == 0) {
		FAIL("bt_bap_broadcast_sink_sync did not fail with NULL streams\n");
		return;
	}

	memcpy(tmp_streams, streams, sizeof(streams));
	bis_index = 0U;
	for (size_t i = 0U; i < ARRAY_SIZE(tmp_streams); i++) {
		bis_index |= BIT(i + BT_ISO_BIS_INDEX_MIN);
	}

	err = bt_bap_broadcast_sink_sync(g_sink, bis_index, tmp_streams, NULL);
	if (err == 0) {
		FAIL("bt_bap_broadcast_sink_sync did not fail with NULL streams[%zu]\n",
		     ARRAY_SIZE(tmp_streams) - 1);
		return;
	}

	bis_index = 0U;
	for (size_t i = 0U; i < CONFIG_BT_BAP_BROADCAST_SNK_STREAM_COUNT + 1; i++) {
		bis_index |= BIT(i + BT_ISO_BIS_INDEX_MIN);
	}

	err = bt_bap_broadcast_sink_sync(g_sink, bis_index, tmp_streams, NULL);
	if (err == 0) {
		FAIL("bt_bap_broadcast_sink_sync did not fail with invalid BIS indexes: 0x%08X\n",
		     bis_index);
		return;
	}
}

static void test_broadcast_stop(void)
{
	int err;

	err = bt_bap_broadcast_sink_stop(g_sink);
	if (err != 0) {
		FAIL("Unable to stop sink: %d", err);
		return;
	}

	printk("Waiting for streams to be stopped\n");
	for (size_t i = 0U; i < ARRAY_SIZE(streams); i++) {
		k_sem_take(&sem_stopped, K_FOREVER);
	}
}

static void test_broadcast_stop_inval(void)
{
	int err;

	err = bt_bap_broadcast_sink_stop(NULL);
	if (err == 0) {
		FAIL("bt_bap_broadcast_sink_stop did not fail with NULL sink\n");
		return;
	}
}

static void test_broadcast_delete(void)
{
	int err;

	err = bt_bap_broadcast_sink_delete(g_sink);
	if (err != 0) {
		FAIL("Unable to stop sink: %d", err);
		return;
	}

	/* No "sync lost" event is generated when we initialized the disconnect */
}

static void test_broadcast_delete_inval(void)
{
	int err;

	err = bt_bap_broadcast_sink_delete(NULL);
	if (err == 0) {
		FAIL("bt_bap_broadcast_sink_delete did not fail with NULL sink\n");
		return;
	}
}

static void test_common(void)
{
	int err;

	err = init();
	if (err) {
		FAIL("Init failed (err %d)\n", err);
		return;
	}

	test_scan_and_pa_sync_inval();
	test_scan_and_pa_sync();

	test_broadcast_sync_inval();
	test_broadcast_sync();

	printk("Waiting for data\n");
	WAIT_FOR_FLAG(flag_received);

	/* Ensure that we also see the metadata update */
	printk("Waiting for metadata update\n");
	WAIT_FOR_FLAG(flag_base_metadata_updated)
}

static void test_main(void)
{
	test_common();

	/* The order of PA sync lost and BIG Sync lost is irrelevant
	 * and depend on timeout parameters. We just wait for PA first, but
	 * either way will work.
	 */
	printk("Waiting for PA disconnected\n");
	WAIT_FOR_FLAG(pa_sync_lost);

	printk("Waiting for streams to be stopped\n");
	for (size_t i = 0U; i < ARRAY_SIZE(streams); i++) {
		k_sem_take(&sem_stopped, K_FOREVER);
	}

	PASS("Broadcast sink passed\n");
}

static void test_sink_disconnect(void)
{
	test_common();

	test_broadcast_stop_inval();
	test_broadcast_stop();

	/* Retry sync*/
	test_broadcast_sync();
	test_broadcast_stop();

	test_broadcast_delete_inval();
	test_broadcast_delete();
	g_sink = NULL;

	PASS("Broadcast sink disconnect passed\n");
}

static const struct bst_test_instance test_broadcast_sink[] = {
	{
		.test_id = "broadcast_sink",
		.test_post_init_f = test_init,
		.test_tick_f = test_tick,
		.test_main_f = test_main
	},
	{
		.test_id = "broadcast_sink_disconnect",
		.test_post_init_f = test_init,
		.test_tick_f = test_tick,
		.test_main_f = test_sink_disconnect
	},
	BSTEST_END_MARKER
};

struct bst_test_list *test_broadcast_sink_install(struct bst_test_list *tests)
{
	return bst_add_tests(tests, test_broadcast_sink);
}

#else /* !CONFIG_BT_BAP_BROADCAST_SINK */

struct bst_test_list *test_broadcast_sink_install(struct bst_test_list *tests)
{
	return tests;
}

#endif /* CONFIG_BT_BAP_BROADCAST_SINK */
