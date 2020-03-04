/*
 * Copyright (c) 2019, Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <drivers/flash.h>
#include <init.h>
#include <string.h>
#include <logging/log.h>

#include "spi_nor.h"
#include "flash_priv.h"
#include <nrfx_qspi.h>

#define qspi_nor_config spi_nor_config
#define QSPI_NOR_MAX_ID_LEN SPI_NOR_MAX_ID_LEN

/* Status register bits */
#define QSPI_SECTOR_SIZE SPI_NOR_SECTOR_SIZE
#define QSPI_BLOCK_SIZE SPI_NOR_BLOCK_SIZE

/* instance 0 flash size in bytes */
#define INST_0_BYTES (DT_INST_0_NORDIC_QSPI_NOR_SIZE / 8)

LOG_MODULE_REGISTER(qspi_nor, CONFIG_FLASH_LOG_LEVEL);

/**
 * @brief QSPI buffer structure
 * Structure used both for TX and RX purposes.
 *
 * @param buf is a valid pointer to a data buffer.
 * Can not be NULL.
 * @param len is the length of the data to be handled.
 * If no data to transmit/receive - pass 0.
 */
struct qspi_buf {
	u8_t *buf;
	size_t len;
};

/**
 * @brief QSPI command structure
 * Structure used for custom command usage.
 *
 * @param op_code is a command value (i.e 0x9F - get Jedec ID)
 * @param tx_buf structure used for TX purposes. Can be NULL if not used.
 * @param rx_buf structure used for RX purposes. Can be NULL if not used.
 */
struct qspi_cmd {
	u8_t op_code;
	const struct qspi_buf *tx_buf;
	const struct qspi_buf *rx_buf;
};

/**
 * @brief Structure for defining the QSPI NOR access
 * @param sem The semaphore to access to the flash
 * @param sync The semaphore to ensure that transfer has finished
 * @param write_protection Indicates if write protection for flash
 *  device is enabled
 */
struct qspi_nor_data {
	struct k_sem sem;
	struct k_sem sync;
	bool write_protection;
};

static inline int qspi_get_mode(bool cpol, bool cpha)
{
	register int ret = -EINVAL;

	if ((!cpol) && (!cpha)) {
		ret = 0;
	} else if (cpol && cpha) {
		ret = 1;
	}
	__ASSERT(ret != -EINVAL, "Invalid QSPI mode");
	return ret;
}

static inline bool qspi_is_used_write_quad_mode(nrf_qspi_writeoc_t lines)
{
	switch (lines) {
	case NRF_QSPI_WRITEOC_PP4IO:
	case NRF_QSPI_WRITEOC_PP4O:
		return true;
	default:
		return false;
	}
}

static inline bool qspi_is_used_read_quad_mode(nrf_qspi_readoc_t lines)
{
	switch (lines) {
	case NRF_QSPI_READOC_READ4IO:
	case NRF_QSPI_READOC_READ4O:
		return true;
	default:
		return false;
	}
}

static inline int qspi_get_lines_write(u8_t lines)
{
	register int ret = -EINVAL;

	switch (lines) {
	case 3:
		ret =  NRF_QSPI_WRITEOC_PP4IO;
		break;
	case 2:
		ret = NRF_QSPI_WRITEOC_PP4O;
		break;
	case 1:
		ret = NRF_QSPI_WRITEOC_PP2O;
		break;
	case 0:
		ret = NRF_QSPI_WRITEOC_PP;
		break;
	default:
		break;
	}
	__ASSERT(ret != -EINVAL, "Invalid QSPI write line");
	return ret;
}

static inline int qspi_get_lines_read(u8_t lines)
{
	register int ret = -EINVAL;

	switch (lines) {
	case 4:
		ret = NRF_QSPI_READOC_READ4IO;
		break;
	case 3:
		ret = NRF_QSPI_READOC_READ4O;
		break;
	case 2:
		ret = NRF_QSPI_READOC_READ2IO;
		break;
	case 1:
		ret = NRF_QSPI_READOC_READ2O;
		break;
	case 0:
		ret = NRF_QSPI_READOC_FASTREAD;
		break;
	default:
		break;
	}
	__ASSERT(ret != -EINVAL, "Invalid QSPI read line");
	return ret;
}

/**
 * @brief Get QSPI prescaler
 * Get supported frequency prescaler not exceeding the requested one.
 *
 * @param frequency - desired QSPI bus frequency
 * @retval NRF_SPI_PRESCALER in case of success or;
 *		   -EINVAL in case of failure
 */
static inline nrf_qspi_frequency_t get_nrf_qspi_prescaler(u32_t frequency)
{
	register int ret = -EINVAL;

	if (frequency < 2000000UL) {
		ret = -EINVAL;
	} else if (frequency >= 32000000UL) {
		ret = NRF_QSPI_FREQ_32MDIV1;
	} else {
		ret = (nrf_qspi_frequency_t)((32000000UL / frequency) - 1);
	}

	__ASSERT(ret != -EINVAL, "Invalid QSPI frequency");
	return ret;
}

static inline nrf_qspi_addrmode_t qspi_get_address_size(bool addr_size)
{
	return addr_size ? NRF_QSPI_ADDRMODE_32BIT : NRF_QSPI_ADDRMODE_24BIT;
}

/**
 * @brief Test whether offset is aligned.
 */
#define QSPI_IS_SECTOR_ALIGNED(_ofs) (((_ofs) & (QSPI_SECTOR_SIZE - 1U)) == 0)
#define QSPI_IS_BLOCK_ALIGNED(_ofs) (((_ofs) & (QSPI_BLOCK_SIZE - 1U)) == 0)

/**
 * @brief Main configuration structure
 */
static struct qspi_nor_data qspi_nor_memory_data = {
	.sem = Z_SEM_INITIALIZER(qspi_nor_memory_data.sem, 1, 1),
	.sync = Z_SEM_INITIALIZER(qspi_nor_memory_data.sync, 0, 1),
};

/**
 * @brief Converts NRFX return codes to the zephyr ones
 */
static inline int qspi_get_zephyr_ret_code(int res)
{
	switch (res) {
	case NRFX_SUCCESS:
		return 0;
	case NRFX_ERROR_INVALID_PARAM:
	case NRFX_ERROR_INVALID_ADDR:
		return -EINVAL;
	case NRFX_ERROR_INVALID_STATE:
		return -ECANCELED;
	case NRFX_ERROR_BUSY:
	case NRFX_ERROR_TIMEOUT:
	default:
		return -EBUSY;
	}
}

static inline struct qspi_nor_data *get_dev_data(struct device *dev)
{
	return dev->driver_data;
}

static inline void qspi_lock(struct device *dev)
{
	struct qspi_nor_data *dev_data = get_dev_data(dev);

	k_sem_take(&dev_data->sem, K_FOREVER);
}

static inline void qspi_unlock(struct device *dev)
{
	struct qspi_nor_data *dev_data = get_dev_data(dev);

	k_sem_give(&dev_data->sem);
}

static inline void qspi_wait_for_completion(struct device *dev)
{
	struct qspi_nor_data *dev_data = get_dev_data(dev);

	k_sem_take(&dev_data->sync, K_FOREVER);
}

static inline void qspi_complete(struct device *dev)
{
	struct qspi_nor_data *dev_data = get_dev_data(dev);

	k_sem_give(&dev_data->sync);
}

/**
 * @brief QSPI handler
 *
 * @param event Driver event type
 * @param p_context Pointer to context. Use in interrupt handler.
 * @retval None
 */
static void qspi_handler(nrfx_qspi_evt_t event, void *p_context)
{
	struct device *dev = p_context;

	if (event == NRFX_QSPI_EVENT_DONE) {
		qspi_complete(dev);
		qspi_unlock(dev);
	}
}


/* QSPI send custom command */
static int qspi_send_cmd(struct device *dev, const struct qspi_cmd *cmd)
{
	/* Check input parameters */
	if (!cmd) {
		return -EINVAL;
	}

	qspi_lock(dev);

	nrf_qspi_cinstr_conf_t cinstr_cfg = {
		.opcode = cmd->op_code,
		.io2_level = true,
		.io3_level = true,
		.wipwait = false,
		.wren = true,
	};
	cinstr_cfg.length = sizeof(cmd->op_code);
	if ((cmd->tx_buf != 0) && (cmd->rx_buf != 0)) {
		cinstr_cfg.length += cmd->tx_buf->len + cmd->rx_buf->len;
	} else if ((cmd->tx_buf != 0) && (cmd->rx_buf == 0)) {
		cinstr_cfg.length += cmd->tx_buf->len;
	} else if ((cmd->tx_buf == 0) && (cmd->rx_buf != 0)) {
		cinstr_cfg.length += cmd->rx_buf->len;
	}

	int res = nrfx_qspi_cinstr_xfer(&cinstr_cfg, cmd->tx_buf->buf,
					cmd->rx_buf->buf);

	qspi_unlock(dev);
	return qspi_get_zephyr_ret_code(res);
}

/* QSPI erase */
static int qspi_erase(struct device *dev, u32_t addr, u32_t size)
{
	/* Check input parameters */
	if (!size) {
		return -EINVAL;
	}

	int res = 0;
	const struct qspi_nor_config *params = dev->config->config_info;

	while (size) {
		if (size == params->size) {
			/* chip erase */
			qspi_lock(dev);
			res = nrfx_qspi_chip_erase();
			qspi_wait_for_completion(dev);
			size = 0;
		} else if ((size >= QSPI_BLOCK_SIZE) &&
			   QSPI_IS_BLOCK_ALIGNED(addr)) {
			/* 64 kB block erase */
			qspi_lock(dev);
			res = nrfx_qspi_erase(NRF_QSPI_ERASE_LEN_64KB, addr);
			qspi_wait_for_completion(dev);
			addr += QSPI_BLOCK_SIZE;
			size -= QSPI_BLOCK_SIZE;
		} else if ((size >= QSPI_SECTOR_SIZE) &&
			   QSPI_IS_SECTOR_ALIGNED(addr)) {
			/* 4kB sector erase */
			qspi_lock(dev);
			res = nrfx_qspi_erase(NRF_QSPI_ERASE_LEN_4KB, addr);
			qspi_wait_for_completion(dev);
			addr += QSPI_SECTOR_SIZE;
			size -= QSPI_SECTOR_SIZE;
		} else {
			/* minimal erase size is at least a sector size */
			LOG_ERR("unsupported at 0x%lx size %zu", (long)addr, size);
			return -EINVAL;
		}
		if (res != NRFX_SUCCESS) {
			LOG_ERR("erase error at 0x%lx size %zu", (long)addr, size);
			return -EIO;
		}
	}

	return qspi_get_zephyr_ret_code(res);
}

/**
 * @brief Fills init struct
 *
 * @param config Pointer to the config struct provided by user
 * @param initstruct Pointer to the configuration struct
 * @retval None
 */
static inline void qspi_fill_init_struct(nrfx_qspi_config_t *initstruct)
{
	/* Configure XIP offset */
	initstruct->xip_offset = 0;

	/* Configure pins */
	initstruct->pins.sck_pin = DT_NORDIC_NRF_QSPI_QSPI_0_SCK_PIN;
	initstruct->pins.csn_pin = DT_NORDIC_NRF_QSPI_QSPI_0_CSN_PINS_0;
	initstruct->pins.io0_pin = DT_NORDIC_NRF_QSPI_QSPI_0_IO_PINS_0;
	initstruct->pins.io1_pin = DT_NORDIC_NRF_QSPI_QSPI_0_IO_PINS_1;
	initstruct->pins.io2_pin = DT_NORDIC_NRF_QSPI_QSPI_0_IO_PINS_2;
	initstruct->pins.io3_pin = DT_NORDIC_NRF_QSPI_QSPI_0_IO_PINS_3;

	/* Configure Protocol interface */
#ifdef DT_INST_0_NORDIC_QSPI_NOR_READOC_ENUM
	initstruct->prot_if.readoc =
		(nrf_qspi_writeoc_t)qspi_get_lines_read(DT_INST_0_NORDIC_QSPI_NOR_READOC_ENUM);
#else
	initstruct->prot_if.readoc = NRF_QSPI_READOC_FASTREAD;
#endif

#ifdef DT_INST_0_NORDIC_QSPI_NOR_WRITEOC_ENUM
	initstruct->prot_if.writeoc =
		(nrf_qspi_writeoc_t)qspi_get_lines_write(DT_INST_0_NORDIC_QSPI_NOR_WRITEOC_ENUM);
#else
	initstruct->prot_if.writeoc = NRF_QSPI_WRITEOC_PP;
#endif
	initstruct->prot_if.addrmode =
		qspi_get_address_size(DT_INST_0_NORDIC_QSPI_NOR_ADDRESS_SIZE_32);

	initstruct->prot_if.dpmconfig = false;

	/* Configure physical interface */
	initstruct->phy_if.sck_freq =
		get_nrf_qspi_prescaler(DT_INST_0_NORDIC_QSPI_NOR_SCK_FREQUENCY);
	initstruct->phy_if.sck_delay = DT_INST_0_NORDIC_QSPI_NOR_SCK_DELAY;
	initstruct->phy_if.spi_mode = qspi_get_mode(DT_INST_0_NORDIC_QSPI_NOR_CPOL,
						    DT_INST_0_NORDIC_QSPI_NOR_CPHA);

	initstruct->phy_if.dpmen = false;
}

/* Configures QSPI memory for the transfer */
static int qspi_nrfx_configure(struct device *dev)
{
	if (!dev) {
		return -ENXIO;
	}

	/* Main config structure */
	nrfx_qspi_config_t QSPIconfig;

	qspi_fill_init_struct(&QSPIconfig);

	int res = nrfx_qspi_init(&QSPIconfig, qspi_handler, dev);

	if (res == NRFX_SUCCESS) {
		/* If quad transfer was chosen - enable it now */
		if ((qspi_is_used_write_quad_mode(QSPIconfig.prot_if.writeoc))
		    || (qspi_is_used_read_quad_mode(QSPIconfig.prot_if.readoc))) {

			/* WRITE ENABLE has to be sent before QUAR ENABLE */
			struct qspi_cmd cmd = { .op_code = SPI_NOR_CMD_WREN };

			if (qspi_send_cmd(dev, &cmd) != 0) {
				return -EIO;
			}

			u8_t tx = BIT(CONFIG_NORDIC_QSPI_NOR_QE_BIT);

			const struct qspi_buf tx_buff = { .buf = &tx, .len = sizeof(tx), };

			cmd.op_code = SPI_NOR_CMD_WRSR;
			cmd.tx_buf = &tx_buff;
			cmd.rx_buf = NULL;

			if (qspi_send_cmd(dev, &cmd) != 0) {
				return -EIO;
			}
		}
	}

	return qspi_get_zephyr_ret_code(res);
}

/**
 * @brief Retrieve the Flash JEDEC ID and compare it with the one expected
 *
 * @param dev The device structure
 * @param flash_id The flash info structure which contains the
 *		  expected JEDEC ID
 * @return 0 on success, negative errno code otherwise
 */
static inline int qspi_nor_read_id(struct device *dev,
				   const struct qspi_nor_config *const flash_id)
{
	u8_t rx_b[QSPI_NOR_MAX_ID_LEN];
	const struct qspi_buf q_rx_buf = {
		.buf = rx_b,
		.len = QSPI_NOR_MAX_ID_LEN
	};
	const struct qspi_cmd cmd = {
		.op_code = SPI_NOR_CMD_RDID,
		.rx_buf = &q_rx_buf,
		.tx_buf = NULL
	};

	if (qspi_send_cmd(dev, &cmd) != 0) {
		return -EIO;
	}

	if (memcmp(flash_id->id, rx_b, QSPI_NOR_MAX_ID_LEN) != 0) {
		LOG_ERR("flash id error. Extected: [%d %d %d], got: [%d %d %d]",
			flash_id->id[0], flash_id->id[1], flash_id->id[2],
			rx_b[0], rx_b[1], rx_b[2]);
		return -ENODEV;
	}

	return 0;
}

#if (CONFIG_HEAP_MEM_POOL_SIZE > 0)
u8_t *rx_ram_buf;
u8_t *tx_ram_buf;
#endif

static int qspi_nor_read(struct device *dev, off_t addr, void *dest,
			 size_t size)
{
	int ret;

	if (!dest) {
		return -EINVAL;
	}
#if (CONFIG_HEAP_MEM_POOL_SIZE > 0)
	rx_ram_buf = NULL;
	void *tmp_dest = dest;
	size_t var_size = size;

	if (((uint32_t)dest < CONFIG_SRAM_BASE_ADDRESS)
	   || (size < sizeof(uint32_t))) {
		if (size < sizeof(uint32_t)) {
			size = 4;
		}
		rx_ram_buf = k_malloc(size);
		if (rx_ram_buf == NULL) {
			return -ENOMEM;
		}
		dest = (void *)rx_ram_buf;
	}
#endif

	/* read size must be multiple of 4 bytes */
	if (size % sizeof(uint32_t) ||
	    !(size > 0)) {
		ret = -EINVAL;
		goto out;
	}

	const struct qspi_nor_config *params = dev->config->config_info;

	/* should be between 0 and flash size */
	if (addr >= params->size ||
	    addr < 0 ||
	    size > params->size ||
	    (addr) + size > params->size) {
		LOG_ERR("read error: address or size "
			"exceeds expected values."
			"Addr: 0x%lx size %zu", (long)addr, size);
		ret = -EINVAL;
		goto out;
	}

	qspi_lock(dev);

	ret = nrfx_qspi_read(dest, size, addr);

	qspi_wait_for_completion(dev);
#if (CONFIG_HEAP_MEM_POOL_SIZE > 0)
	if (rx_ram_buf != NULL) {
		memcpy(tmp_dest, dest, var_size);
		k_free(rx_ram_buf);
	}
#endif
	return qspi_get_zephyr_ret_code(ret);
out:
#if (CONFIG_HEAP_MEM_POOL_SIZE > 0)
	k_free(rx_ram_buf);
#endif
	return ret;
}

static int qspi_nor_write(struct device *dev, off_t addr, const void *src,
			  size_t size)
{
	int ret;

	if (!src) {
		return -EINVAL;
	}

#if (CONFIG_HEAP_MEM_POOL_SIZE > 0)
	tx_ram_buf = NULL;
	if (((uint32_t)src < CONFIG_SRAM_BASE_ADDRESS)
	   || (size < sizeof(uint32_t))) {
		size_t var_size = size;

		if (size < sizeof(uint32_t)) {
			size = 4;
		}
		tx_ram_buf = k_malloc(size);
		if (tx_ram_buf == NULL) {
			ret = -ENOMEM;
		}
		qspi_nor_read(dev, addr, tx_ram_buf, size);
		memcpy(tx_ram_buf, src, var_size);
		src = tx_ram_buf;
	}
#endif

	/* write size must be multiple of 4 bytes */
	if (size % sizeof(uint32_t) ||
	    !(size > 0)) {
		ret = -EINVAL;
		goto out;
	}

	struct qspi_nor_data *const driver_data = dev->driver_data;
	const struct qspi_nor_config *params = dev->config->config_info;

	if (driver_data->write_protection) {
		ret = -EACCES;
		goto out;
	}

	/* should be between 0 and flash size */
	if (addr >= params->size ||
	    addr < 0 ||
	    size > params->size ||
	    (addr) + size > params->size) {
		LOG_ERR("write error: address or size "
			"exceeds expected values."
			"Addr: 0x%lx size %zu", (long)addr, size);
		ret = -EINVAL;
		goto out;
	}

	qspi_lock(dev);

	ret = nrfx_qspi_write(src, size, addr);

	qspi_wait_for_completion(dev);
#if (CONFIG_HEAP_MEM_POOL_SIZE > 0)
	if (tx_ram_buf != NULL) {
		k_free(tx_ram_buf);
	}
#endif
	return qspi_get_zephyr_ret_code(ret);
out:
#if (CONFIG_HEAP_MEM_POOL_SIZE > 0)
	k_free(tx_ram_buf);
#endif
	return ret;
}

static int qspi_nor_erase(struct device *dev, off_t addr, size_t size)
{
	struct qspi_nor_data *const driver_data = dev->driver_data;
	const struct qspi_nor_config *params = dev->config->config_info;

	if (driver_data->write_protection) {
		return -EACCES;
	}

	/* should be between 0 and flash size */
	if (addr >= params->size ||
	    addr < 0 ||
	    size > params->size ||
	    (addr) + size > params->size) {
		LOG_ERR("erase error: address or size "
			"exceeds expected values."
			"Addr: 0x%lx size %zu", (long)addr, size);
		return -EINVAL;
	}

	int ret = qspi_erase(dev, addr, size);

	return ret;
}

static int qspi_nor_write_protection_set(struct device *dev,
					 bool write_protect)
{
	struct qspi_nor_data *const driver_data = dev->driver_data;

	int ret = 0;
	struct qspi_cmd cmd = {
		.op_code = ((write_protect) ? SPI_NOR_CMD_WRDI : SPI_NOR_CMD_WREN),
	};

	driver_data->write_protection = write_protect;

	if (qspi_send_cmd(dev, &cmd) != 0) {
		ret = -EIO;
	}

	return ret;
}

/**
 * @brief Configure the flash
 *
 * @param dev The flash device structure
 * @param info The flash info structure
 * @return 0 on success, negative errno code otherwise
 */
static int qspi_nor_configure(struct device *dev)
{
	const struct qspi_nor_config *params = dev->config->config_info;

	int ret = qspi_nrfx_configure(dev);

	if (ret != 0) {
		return ret;
	}

	/* now the spi bus is configured, we can verify the flash id */
	if (qspi_nor_read_id(dev, params) != 0) {
		return -ENODEV;
	}

	return 0;
}

/**
 * @brief Initialize and configure the flash
 *
 * @param name The flash name
 * @return 0 on success, negative errno code otherwise
 */
static int qspi_nor_init(struct device *dev)
{
#if defined(CONFIG_MULTITHREADING)
	IRQ_CONNECT(DT_NORDIC_NRF_QSPI_QSPI_0_IRQ_0,
		    DT_NORDIC_NRF_QSPI_QSPI_0_IRQ_0_PRIORITY,
		    nrfx_isr, nrfx_qspi_irq_handler, 0);
#else
	IRQ_DIRECT_CONNECT(DT_NORDIC_NRF_QSPI_QSPI_0_IRQ_0,
		    DT_NORDIC_NRF_QSPI_QSPI_0_IRQ_0_PRIORITY,
		    nrfx_qspi_irq_handler, 0);
#endif
	return qspi_nor_configure(dev);
}

#if defined(CONFIG_FLASH_PAGE_LAYOUT)

/* instance 0 page count */
#define LAYOUT_PAGES_COUNT (INST_0_BYTES / \
			    CONFIG_NORDIC_QSPI_NOR_FLASH_LAYOUT_PAGE_SIZE)

BUILD_ASSERT_MSG((CONFIG_NORDIC_QSPI_NOR_FLASH_LAYOUT_PAGE_SIZE *
		  LAYOUT_PAGES_COUNT)
		 == INST_0_BYTES,
		 "QSPI_NOR_FLASH_LAYOUT_PAGE_SIZE incompatible with flash size");

static const struct flash_pages_layout dev_layout = {
	.pages_count = LAYOUT_PAGES_COUNT,
	.pages_size = CONFIG_NORDIC_QSPI_NOR_FLASH_LAYOUT_PAGE_SIZE,
};
#undef LAYOUT_PAGES_COUNT

static void qspi_nor_pages_layout(struct device *dev,
				  const struct flash_pages_layout **layout,
				  size_t *layout_size)
{
	*layout = &dev_layout;
	*layout_size = 1;
}
#endif /* CONFIG_FLASH_PAGE_LAYOUT */

static const struct flash_driver_api qspi_nor_api = {
	.read = qspi_nor_read,
	.write = qspi_nor_write,
	.erase = qspi_nor_erase,
	.write_protection = qspi_nor_write_protection_set,
#if defined(CONFIG_FLASH_PAGE_LAYOUT)
	.page_layout = qspi_nor_pages_layout,
#endif
	.write_block_size = 1,
};


static const struct qspi_nor_config flash_id = {
	.id = DT_INST_0_NORDIC_QSPI_NOR_JEDEC_ID,
	.size = INST_0_BYTES,
};

DEVICE_AND_API_INIT(qspi_flash_memory, DT_INST_0_NORDIC_QSPI_NOR_LABEL,
		    &qspi_nor_init, &qspi_nor_memory_data,
		    &flash_id, POST_KERNEL, CONFIG_NORDIC_QSPI_NOR_INIT_PRIORITY,
		    &qspi_nor_api);
