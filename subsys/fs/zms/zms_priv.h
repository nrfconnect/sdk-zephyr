/*  ZMS: Zephyr Memory Storage
 *
 * Copyright (c) 2024 BayLibre SAS
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef __ZMS_PRIV_H_
#define __ZMS_PRIV_H_

#ifdef __cplusplus
extern "C" {
#endif

/*
 * MASKS AND SHIFT FOR ADDRESSES
 * an address in zms is an uint64_t where:
 *   high 4 bytes represent the sector number
 *   low 4 bytes represent the offset in a sector
 */
#define ADDR_SECT_MASK   GENMASK64(63, 32)
#define ADDR_SECT_SHIFT  32
#define ADDR_OFFS_MASK   GENMASK64(31, 0)
#define SECTOR_NUM(x)    FIELD_GET(ADDR_SECT_MASK, x)
#define SECTOR_OFFSET(x) FIELD_GET(ADDR_OFFS_MASK, x)

#if defined(CONFIG_ZMS_CUSTOM_BLOCK_SIZE)
#define ZMS_BLOCK_SIZE CONFIG_ZMS_MAX_BLOCK_SIZE
#else
#define ZMS_BLOCK_SIZE 32
#endif

#define ZMS_LOOKUP_CACHE_NO_ADDR GENMASK64(63, 0)
#define ZMS_HEAD_ID              GENMASK(31, 0)

#define ZMS_VERSION_MASK        GENMASK(7, 0)
#define ZMS_GET_VERSION(x)      FIELD_GET(ZMS_VERSION_MASK, x)
#define ZMS_DEFAULT_VERSION     1
#define ZMS_MAGIC_NUMBER        0x42 /* murmur3a hash of "ZMS" (MSB) */
#define ZMS_MAGIC_NUMBER_MASK   GENMASK(15, 8)
#define ZMS_GET_MAGIC_NUMBER(x) FIELD_GET(ZMS_MAGIC_NUMBER_MASK, x)
#define ZMS_MIN_ATE_NUM         5

#define ZMS_INVALID_SECTOR_NUM -1
#define ZMS_DATA_IN_ATE_SIZE   8

struct zms_ate {
	uint8_t crc8;      /* crc8 check of the entry */
	uint8_t cycle_cnt; /* cycle counter for non erasable devices */
	uint32_t id;       /* data id */
	uint16_t len;      /* data len within sector */
	union {
		uint8_t data[8]; /* used to store small size data */
		struct {
			uint32_t offset; /* data offset within sector */
			union {
				uint32_t data_crc; /*
						    * crc for data: The data CRC is checked only
						    * when the whole data of the element is read.
						    * The data CRC is not checked for a partial
						    * read, as it is computed for the complete
						    * set of data.
						    */
				uint32_t metadata; /*
						    * Used to store metadata information
						    * such as storage version.
						    */
			};
		};
	};
} __packed;

#ifdef __cplusplus
}
#endif

#endif /* __ZMS_PRIV_H_ */
