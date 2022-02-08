/*
 * Copyright (c) 2017-2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

struct ll_conn *ll_conn_acquire(void);
void ll_conn_release(struct ll_conn *conn);
uint16_t ll_conn_handle_get(struct ll_conn *conn);
struct ll_conn *ll_conn_get(uint16_t handle);
struct ll_conn *ll_connected_get(uint16_t handle);
uint16_t ll_conn_free_count_get(void);
void ll_tx_ack_put(uint16_t handle, struct node_tx *node_tx);
int ull_conn_init(void);
int ull_conn_reset(void);
uint16_t ull_conn_default_tx_octets_get(void);
uint16_t ull_conn_default_tx_time_get(void);
uint8_t ull_conn_default_phy_tx_get(void);
uint8_t ull_conn_default_phy_rx_get(void);
bool ull_conn_peer_connected(uint8_t const own_id_addr_type,
			     uint8_t const *const own_id_addr,
			     uint8_t const peer_id_addr_type,
			     uint8_t const *const peer_id_addr);
void ull_conn_setup(memq_link_t *rx_link, struct node_rx_hdr *rx);
int ull_conn_rx(memq_link_t *link, struct node_rx_pdu **rx);
int ull_conn_llcp(struct ll_conn *conn, uint32_t ticks_at_expire, uint16_t lazy);
void ull_conn_done(struct node_rx_event_done *done);
void ull_conn_tx_demux(uint8_t count);
void ull_conn_tx_lll_enqueue(struct ll_conn *conn, uint8_t count);
void ull_conn_link_tx_release(void *link);
uint8_t ull_conn_ack_last_idx_get(void);
memq_link_t *ull_conn_ack_peek(uint8_t *ack_last, uint16_t *handle,
			       struct node_tx **tx);
memq_link_t *ull_conn_ack_by_last_peek(uint8_t last, uint16_t *handle,
				       struct node_tx **tx);
void *ull_conn_ack_dequeue(void);
void ull_conn_tx_ack(uint16_t handle, memq_link_t *link, struct node_tx *tx);
uint8_t ull_conn_llcp_req(void *conn);

void ull_pdu_data_init(struct pdu_data *pdu);

#if !defined(CONFIG_BT_LL_SW_LLCP_LEGACY)

uint16_t ull_conn_event_counter(struct ll_conn *conn);

void ull_conn_update_parameters(struct ll_conn *conn, uint8_t is_cu_proc,
				uint8_t win_size, uint16_t win_offset_us,
				uint16_t interval, uint16_t latency,
				uint16_t timeout, uint16_t instant);

void ull_conn_default_tx_octets_set(uint16_t tx_octets);

void ull_conn_default_tx_time_set(uint16_t tx_time);

uint8_t ull_conn_lll_phy_active(struct ll_conn *conn, uint8_t phy);

void ull_dle_init(struct ll_conn *conn, uint8_t phy);

void ull_dle_max_time_get(struct ll_conn *conn, uint16_t *max_rx_time,
				    uint16_t *max_tx_time);

uint8_t ull_dle_update_eff(struct ll_conn *conn);

void ull_dle_local_tx_update(struct ll_conn *conn, uint16_t tx_octets, uint16_t tx_time);

void ull_conn_default_phy_tx_set(uint8_t tx);

void ull_conn_default_phy_rx_set(uint8_t rx);

void ull_conn_chan_map_set(struct ll_conn *conn, const uint8_t chm[5]);

void *ull_conn_tx_mem_acquire(void);

void ull_conn_tx_mem_release(void *tx);

uint8_t ull_conn_mfifo_get_tx(void **lll_tx);

void ull_conn_mfifo_enqueue_tx(uint8_t idx);

/**
 * @brief Pause the data path of a rx queue.
 */
void ull_conn_pause_rx_data(struct ll_conn *conn);

/**
 * @brief Resume the data path of a rx queue.
 */
void ull_conn_resume_rx_data(struct ll_conn *conn);

/**
 * @brief Restart procedure timeout timer
 */
void ull_conn_prt_reload(struct ll_conn *conn, uint16_t procedure_reload);

/**
 * @brief Clear procedure timeout timer
 */
void ull_conn_prt_clear(struct ll_conn *conn);

#endif /* CONFIG_BT_LL_SW_LLCP_LEGACY */
