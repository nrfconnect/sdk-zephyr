/*
 * Copyright Runtime.io 2018. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef H_MGMT_BUF
#define H_MGMT_BUF

#include <inttypes.h>
#include "cbor_encoder_writer.h"
#include "cbor_decoder_reader.h"
struct net_buf;

struct cbor_nb_reader {
	struct cbor_decoder_reader r;
	struct net_buf *nb;
};

struct cbor_nb_writer {
	struct cbor_encoder_writer enc;
	struct net_buf *nb;
};

/**
 * @brief Allocates a net_buf for holding an mcumgr request or response.
 *
 * @return                      A newly-allocated buffer net_buf on success;
 *                              NULL on failure.
 */
struct net_buf *mcumgr_buf_alloc(void);

/**
 * @brief Frees an mcumgr net_buf
 *
 * @param nb                    The net_buf to free.
 */
void mcumgr_buf_free(struct net_buf *nb);

/**
 * @brief Initializes a CBOR writer with the specified net_buf.
 *
 * @param cnw                   The writer to initialize.
 * @param nb                    The net_buf that the writer will write to.
 */
void cbor_nb_writer_init(struct cbor_nb_writer *cnw,
			 struct net_buf *nb);

/**
 * @brief Initializes a CBOR reader with the specified net_buf.
 *
 * @param cnr                   The reader to initialize.
 * @param nb                    The net_buf that the reader will read from.
 */
void cbor_nb_reader_init(struct cbor_nb_reader *cnr,
			 struct net_buf *nb);

#endif
