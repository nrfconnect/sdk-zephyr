/* ieee802154_rf2xx.c - ATMEL RF2XX IEEE 802.15.4 Driver */

/*
 * Copyright (c) 2019 Gerson Fernando Budke
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_MODULE_NAME ieee802154_rf2xx
#define LOG_LEVEL CONFIG_IEEE802154_DRIVER_LOG_LEVEL

#include <logging/log.h>
LOG_MODULE_REGISTER(LOG_MODULE_NAME);

#include <errno.h>
#include <assert.h>
#include <stdio.h>

#include <kernel.h>
#include <arch/cpu.h>

#include <device.h>
#include <init.h>
#include <net/net_if.h>
#include <net/net_pkt.h>

#include <sys/byteorder.h>
#include <string.h>
#include <random/rand32.h>
#include <linker/sections.h>
#include <sys/atomic.h>

#include <drivers/spi.h>
#include <drivers/gpio.h>

#include <net/ieee802154_radio.h>

#include "ieee802154_rf2xx.h"
#include "ieee802154_rf2xx_regs.h"
#include "ieee802154_rf2xx_iface.h"

#define RF2XX_OT_PSDU_LENGTH    1280

/* Radio Transceiver ISR */
static inline void trx_isr_handler(struct device *port,
				   struct gpio_callback *cb,
				   u32_t pins)
{
	struct rf2xx_context *ctx = CONTAINER_OF(cb,
						 struct rf2xx_context,
						 irq_cb);

	ARG_UNUSED(port);
	ARG_UNUSED(pins);

	k_sem_give(&ctx->trx_isr_lock);
}

static void trx_isr_timeout(struct k_timer *timer_id)
{
	struct rf2xx_context *ctx = (struct rf2xx_context *)
				    k_timer_user_data_get(timer_id);

	k_mutex_lock(&ctx->phy_mutex, K_FOREVER);
	ctx->trx_state = RF2XX_TRX_PHY_STATE_IDLE;
	k_mutex_unlock(&ctx->phy_mutex);
}

static void rf2xx_trx_set_state(struct device *dev,
				enum rf2xx_trx_state_cmd_t state)
{
	do {
		rf2xx_iface_reg_write(dev, RF2XX_TRX_STATE_REG,
				      RF2XX_TRX_PHY_STATE_CMD_FORCE_TRX_OFF);
	} while (RF2XX_TRX_PHY_STATUS_TRX_OFF !=
		 (rf2xx_iface_reg_read(dev, RF2XX_TRX_STATUS_REG) &
		  RF2XX_TRX_PHY_STATUS_MASK));

	do {
		rf2xx_iface_reg_write(dev, RF2XX_TRX_STATE_REG, state);
	} while (state !=
		 (rf2xx_iface_reg_read(dev, RF2XX_TRX_STATUS_REG) &
		  RF2XX_TRX_PHY_STATUS_MASK));
}

static void rf2xx_trx_set_rx_state(struct device *dev)
{
	rf2xx_trx_set_state(dev, RF2XX_TRX_PHY_STATE_CMD_TRX_OFF);
	rf2xx_iface_reg_read(dev, RF2XX_IRQ_STATUS_REG);
	/**
	 * Set extended RX mode
	 * Datasheet: chapter 7.2 Extended Operating Mode
	 */
	rf2xx_trx_set_state(dev, RF2XX_TRX_PHY_STATE_CMD_RX_AACK_ON);
}

static void rf2xx_trx_rx(struct device *dev)
{
	struct rf2xx_context *ctx = dev->driver_data;
	struct net_pkt *pkt = NULL;
	u8_t rx_buf[RX2XX_MAX_FRAME_SIZE];
	u8_t pkt_len;
	u8_t frame_len;
	u8_t trac;

	/*
	 * The rf2xx frame buffer can have length > 128 bytes. The
	 * net_pkt_alloc_with_buffer allocates max value of 128 bytes.
	 *
	 * This obligate the driver to have rx_buf statically allocated with
	 * RX2XX_MAX_FRAME_SIZE.
	 */
	rf2xx_iface_frame_read(dev, rx_buf, RX2XX_FRAME_HEADER_SIZE);
	pkt_len = rx_buf[RX2XX_FRAME_PHR_INDEX];

	if (pkt_len < RX2XX_FRAME_MIN_PHR_SIZE) {
		LOG_ERR("invalid RX frame length");
		return;
	}

	frame_len = RX2XX_FRAME_HEADER_SIZE + pkt_len +
		    RX2XX_FRAME_FOOTER_SIZE;

	rf2xx_iface_frame_read(dev, rx_buf, frame_len);

	trac = rx_buf[pkt_len + RX2XX_FRAME_TRAC_INDEX];
	trac = (trac >> RF2XX_RX_TRAC_STATUS) & RF2XX_RX_TRAC_BIT_MASK;

	if (trac == RF2XX_TRX_PHY_STATE_TRAC_INVALID) {
		LOG_ERR("invalid RX frame");

		return;
	}

	ctx->pkt_lqi = rx_buf[pkt_len + RX2XX_FRAME_LQI_INDEX];
	ctx->pkt_ed = rx_buf[pkt_len + RX2XX_FRAME_ED_INDEX];

	if (!IS_ENABLED(CONFIG_IEEE802154_RAW_MODE) &&
	    !IS_ENABLED(CONFIG_NET_L2_OPENTHREAD)) {
		pkt_len -= RX2XX_FRAME_FCS_LENGTH;
	}

	pkt = net_pkt_alloc_with_buffer(ctx->iface, pkt_len,
					AF_UNSPEC, 0, K_NO_WAIT);

	if (!pkt) {
		LOG_ERR("No buf available");
		return;
	}

	memcpy(pkt->buffer->data, rx_buf + RX2XX_FRAME_HEADER_SIZE, pkt_len);
	net_buf_add(pkt->buffer, pkt_len);
	net_pkt_set_ieee802154_lqi(pkt, ctx->pkt_lqi);
	net_pkt_set_ieee802154_rssi(pkt, ctx->pkt_ed + ctx->trx_rssi_base);

	LOG_DBG("Caught a packet (%02X) (LQI: %02X, RSSI: %d, ED: %02X)",
		pkt_len, ctx->pkt_lqi, ctx->trx_rssi_base + ctx->pkt_ed,
		ctx->pkt_ed);

	if (net_recv_data(ctx->iface, pkt) < 0) {
		LOG_DBG("Packet dropped by NET stack");
		net_pkt_unref(pkt);
		return;
	}

	if (LOG_LEVEL >= LOG_LEVEL_DBG) {
		net_analyze_stack("RF2XX Rx stack",
				Z_THREAD_STACK_BUFFER(ctx->trx_stack),
				K_THREAD_STACK_SIZEOF(ctx->trx_stack));
	}
}
static void rf2xx_thread_main(void *arg)
{
	struct device *dev = INT_TO_POINTER(arg);
	struct rf2xx_context *ctx = dev->driver_data;
	u8_t isr_status;

	while (true) {
		k_sem_take(&ctx->trx_isr_lock, K_FOREVER);
		k_mutex_lock(&ctx->phy_mutex, K_FOREVER);

		isr_status = rf2xx_iface_reg_read(dev, RF2XX_IRQ_STATUS_REG);
		/*
		 *  IRQ_7 (BAT_LOW) Indicates a supply voltage below the
		 *    programmed threshold. 9.5.4
		 *  IRQ_6 (TRX_UR) Indicates a Frame Buffer access
		 *    violation. 9.3.3
		 *  IRQ_5 (AMI) Indicates address matching. 8.2
		 *  IRQ_4 (CCA_ED_DONE) Multi-functional interrupt:
		 *   1. AWAKE_END: 7.1.2.5
		 *      • Indicates finished transition to TRX_OFF state
		 *        from P_ON, SLEEP, DEEP_SLEEP, or RESET state.
		 *   2. CCA_ED_DONE: 8.5.4
		 *      • Indicates the end of a CCA or ED
		 *        measurement. 8.6.4
		 *  IRQ_3 (TRX_END)
		 *    RX: Indicates the completion of a frame
		 *      reception. 7.1.3
		 *    TX: Indicates the completion of a frame
		 *      transmission. 7.1.3
		 *  IRQ_2 (RX_START) Indicates the start of a PSDU
		 *    reception; the AT86RF233 state changed to BUSY_RX;
		 *    the PHR can be read from Frame Buffer. 7.1.3
		 *  IRQ_1 (PLL_UNLOCK) Indicates PLL unlock. If the radio
		 *    transceiver is in BUSY_TX / BUSY_TX_ARET state, the
		 *    PA is turned off immediately. 9.7.5
		 *  IRQ_0 (PLL_LOCK) Indicates PLL lock.
		 */
		if (isr_status & (1 << RF2XX_RX_START)) {
			ctx->trx_state = RF2XX_TRX_PHY_BUSY_RX;
			k_timer_start(&ctx->trx_isr_timeout, K_MSEC(10), 0);
		} else if (isr_status & (1 << RF2XX_TRX_END)) {
			if (ctx->trx_state == RF2XX_TRX_PHY_BUSY_RX) {
				/* Set PLL_ON to avoid transceiver receive
				 * new data until finish reading process
				 */
				rf2xx_trx_set_state(dev,
						    RF2XX_TRX_PHY_STATE_CMD_PLL_ON);
				k_timer_stop(&ctx->trx_isr_timeout);
				rf2xx_trx_rx(dev);
				rf2xx_trx_set_state(dev,
						    RF2XX_TRX_PHY_STATE_CMD_RX_AACK_ON);
				/*if (ctx->trx_state == RF2XX_TRX_PHY_BUSY_TX)*/
			} else {
				ctx->trx_trac =
					(rf2xx_iface_reg_read(dev,
							      RF2XX_TRX_STATE_REG) >>
					 RF2XX_TRAC_STATUS) & 7;
				k_sem_give(&ctx->trx_tx_sync);
				rf2xx_trx_set_rx_state(dev);
			}
			ctx->trx_state = RF2XX_TRX_PHY_STATE_IDLE;
		}
		k_mutex_unlock(&ctx->phy_mutex);
	}
}

static inline u8_t *get_mac(struct device *dev)
{
	struct rf2xx_context *ctx = dev->driver_data;
	u32_t *ptr = (u32_t *)(ctx->mac_addr);

	UNALIGNED_PUT(sys_rand32_get(), ptr);
	ptr = (u32_t *)(ctx->mac_addr + 4);
	UNALIGNED_PUT(sys_rand32_get(), ptr);

	/*
	 * Clear bit 0 to ensure it isn't a multicast address and set
	 * bit 1 to indicate address is locally administrered and may
	 * not be globally unique.
	 */
	ctx->mac_addr[0] = (ctx->mac_addr[0] & ~0x01) | 0x02;

	return ctx->mac_addr;
}

static enum ieee802154_hw_caps rf2xx_get_capabilities(struct device *dev)
{
	ARG_UNUSED(dev);

	return IEEE802154_HW_FCS |
	       IEEE802154_HW_PROMISC |
	       IEEE802154_HW_FILTER |
	       IEEE802154_HW_CSMA |
	       IEEE802154_HW_TX_RX_ACK |
	       IEEE802154_HW_2_4_GHZ;
}

static int rf2xx_cca(struct device *dev)
{
	ARG_UNUSED(dev);

	return 0;
}

static int rf2xx_set_channel(struct device *dev, u16_t channel)
{
	u8_t reg;

	if (channel < 11 || channel > 26) {
		LOG_ERR("Unsupported channel %u", channel);
		return -EINVAL;
	}

	reg = rf2xx_iface_reg_read(dev, RF2XX_PHY_CC_CCA_REG) & ~0x1f;
	rf2xx_iface_reg_write(dev, RF2XX_PHY_CC_CCA_REG, reg | channel);

	return 0;
}

static int rf2xx_set_txpower(struct device *dev, s16_t dbm)
{
	u8_t reg;

	ARG_UNUSED(dbm);

	/* TODO: Add look-up table
	 * Now will max power
	 */
	reg = rf2xx_iface_reg_read(dev, RF2XX_PHY_TX_PWR_REG) & ~0x0f;
	rf2xx_iface_reg_write(dev, RF2XX_PHY_TX_PWR_REG, reg);

	return 0;
}

static int rf2xx_set_ieee_addr(struct device *dev, bool set,
			       const u8_t *ieee_addr)
{
	const u8_t *ptr_to_reg = ieee_addr;

	LOG_DBG("IEEE address %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x",
		ieee_addr[7], ieee_addr[6], ieee_addr[5], ieee_addr[4],
		ieee_addr[3], ieee_addr[2], ieee_addr[1], ieee_addr[0]);

	if (set) {
		for (u8_t i = 0; i < 8; i++, ptr_to_reg++) {
			rf2xx_iface_reg_write(dev, (RF2XX_IEEE_ADDR_0_REG + i),
					      *ptr_to_reg);
		}
	} else {
		for (u8_t i = 0; i < 8; i++) {
			rf2xx_iface_reg_write(dev, (RF2XX_IEEE_ADDR_0_REG + i),
					      0);
		}
	}

	return 0;
}
static int rf2xx_set_short_addr(struct device *dev, bool set,
				u16_t short_addr)
{
	u8_t short_addr_le[2] = { 0xFF, 0xFF };

	if (set) {
		sys_put_le16(short_addr, short_addr_le);
	}

	rf2xx_iface_reg_write(dev, RF2XX_SHORT_ADDR_0_REG, short_addr_le[0]);
	rf2xx_iface_reg_write(dev, RF2XX_SHORT_ADDR_1_REG, short_addr_le[1]);
	rf2xx_iface_reg_write(dev, RF2XX_CSMA_SEED_0_REG,
			      short_addr_le[0] + short_addr_le[1]);

	LOG_DBG("Short Address: 0x%02X%02X", short_addr_le[1],
		short_addr_le[0]);

	return 0;
}
static int rf2xx_set_pan_id(struct device *dev, bool set, u16_t pan_id)
{
	u8_t pan_id_le[2] = { 0xFF, 0xFF };

	if (set) {
		sys_put_le16(pan_id, pan_id_le);
	}

	rf2xx_iface_reg_write(dev, RF2XX_PAN_ID_0_REG, pan_id_le[0]);
	rf2xx_iface_reg_write(dev, RF2XX_PAN_ID_1_REG, pan_id_le[1]);

	LOG_DBG("Pan Id: 0x%02X%02X", pan_id_le[1], pan_id_le[0]);

	return 0;
}

static int rf2xx_filter(struct device *dev,
			bool set, enum ieee802154_filter_type type,
			const struct ieee802154_filter *filter)
{
	LOG_DBG("Applying filter %u", type);

	if (type == IEEE802154_FILTER_TYPE_IEEE_ADDR) {
		return rf2xx_set_ieee_addr(dev, set, filter->ieee_addr);
	} else if (type == IEEE802154_FILTER_TYPE_SHORT_ADDR) {
		return rf2xx_set_short_addr(dev, set, filter->short_addr);
	} else if (type == IEEE802154_FILTER_TYPE_PAN_ID) {
		return rf2xx_set_pan_id(dev, set, filter->pan_id);
	}

	return -ENOTSUP;
}

static int rf2xx_tx(struct device *dev,
		    struct net_pkt *pkt,
		    struct net_buf *frag)
{
	struct rf2xx_context *ctx = dev->driver_data;
	bool abort = true;
	int response;

	k_mutex_lock(&ctx->phy_mutex, K_FOREVER);
	/* Reset semaphore in case ACK was received after timeout */
	k_sem_reset(&ctx->trx_tx_sync);

	if (ctx->trx_state == RF2XX_TRX_PHY_STATE_IDLE) {
		ctx->trx_state = RF2XX_TRX_PHY_BUSY_TX;
		abort = false;

		/**
		 * Set extended TX mode
		 * Datasheet: chapter 7.2 Extended Operating Mode
		 */
		rf2xx_trx_set_state(dev, RF2XX_TRX_PHY_STATE_CMD_TX_ARET_ON);
		rf2xx_iface_reg_read(dev, RF2XX_IRQ_STATUS_REG);
		rf2xx_iface_frame_write(dev, frag->data, frag->len);
		rf2xx_iface_phy_tx_start(dev);
	}

	k_mutex_unlock(&ctx->phy_mutex);

	if (abort) {
		LOG_DBG("TX Abort, TRX isn't idle!");
		return -EBUSY;
	}

	/**
	 * Wait transceiver...
	 */
	k_sem_take(&ctx->trx_tx_sync, K_FOREVER);

	switch (ctx->trx_trac) {
	/* Channel is still busy after attempting MAX_CSMA_RETRIES of
	 * CSMA-CA
	 */
	case RF2XX_TRX_PHY_STATE_TRAC_CHANNEL_ACCESS_FAILED:
		response = -EBUSY;
		break;
	/* No acknowledgment frames were received during all retry
	 * attempts
	 */
	case RF2XX_TRX_PHY_STATE_TRAC_NO_ACK:
		response = -EAGAIN;
		break;
	/* Transaction not yet finished */
	case RF2XX_TRX_PHY_STATE_TRAC_INVALID:
		response = -EINTR;
		break;
	/* RF2XX_TRX_PHY_STATE_TRAC_SUCCESS:
	 *  The transaction was responded to by a valid ACK, or, if no
	 *  ACK is requested, after a successful frame transmission.
	 *
	 * RF2XX_TRX_PHY_STATE_TRAC_SUCCESS_DATA_PENDING:
	 * Equivalent to SUCCESS and indicating that the “Frame
	 * Pending” bit (see Section 8.1.2.2) of the received
	 * acknowledgment frame was set.
	 */
	default:
		response = 0;
	}

	return response;
}

static int rf2xx_start(struct device *dev)
{
	const struct rf2xx_config *conf = dev->config->config_info;
	struct rf2xx_context *ctx = dev->driver_data;

	k_mutex_lock(&ctx->phy_mutex, K_FOREVER);
	gpio_pin_enable_callback(ctx->irq_gpio, conf->irq.pin);
	rf2xx_trx_set_rx_state(dev);
	k_mutex_unlock(&ctx->phy_mutex);

	return 0;
}

static int rf2xx_stop(struct device *dev)
{
	const struct rf2xx_config *conf = dev->config->config_info;
	struct rf2xx_context *ctx = dev->driver_data;

	k_mutex_lock(&ctx->phy_mutex, K_FOREVER);
	gpio_pin_disable_callback(ctx->irq_gpio, conf->irq.pin);
	rf2xx_trx_set_state(dev, RF2XX_TRX_PHY_STATE_CMD_TRX_OFF);
	k_mutex_unlock(&ctx->phy_mutex);

	return 0;
}

int rf2xx_configure(struct device *dev, enum ieee802154_config_type type,
		    const struct ieee802154_config *config)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(type);
	ARG_UNUSED(config);

	return 0;
}

static int power_on_and_setup(struct device *dev)
{
	const struct rf2xx_config *conf = dev->config->config_info;
	struct rf2xx_context *ctx = dev->driver_data;
	u8_t config;

	ctx->trx_state = RF2XX_TRX_PHY_STATE_IDLE;

	rf2xx_iface_phy_rst(dev);

	/* Sync transceiver state */
	do {
		rf2xx_iface_reg_write(dev, RF2XX_TRX_STATE_REG,
				      RF2XX_TRX_PHY_STATE_CMD_TRX_OFF);
	} while (RF2XX_TRX_PHY_STATUS_TRX_OFF !=
		 (rf2xx_iface_reg_read(dev, RF2XX_TRX_STATUS_REG) &
		  RF2XX_TRX_PHY_STATUS_MASK));

	/* get device identification */
	ctx->trx_model = rf2xx_iface_reg_read(dev, RF2XX_PART_NUM_REG);
	ctx->trx_version = rf2xx_iface_reg_read(dev, RF2XX_VERSION_NUM_REG);

	/**
	 * Valid transceiver are:
	 *  231-Rev-A (Version 0x02)
	 *  232-Rev-A (Version 0x02)
	 *  233-Rev-A (Version 0x01) (Warning)
	 *  233-Rev-B (Version 0x02)
	 */
	if (ctx->trx_model != RF2XX_TRX_MODEL_231 &&
	    ctx->trx_model != RF2XX_TRX_MODEL_232 &&
	    ctx->trx_model != RF2XX_TRX_MODEL_233) {
		LOG_DBG("Invalid or not supported transceiver");
		return -ENODEV;
	}

	if (ctx->trx_version < 0x02) {
		LOG_DBG("Transceiver is old and unstable release");
	}

	/* Set RSSI base */
	if (ctx->trx_model == RF2XX_TRX_MODEL_233) {
		ctx->trx_rssi_base = -94;
	} else if (ctx->trx_model == RF2XX_TRX_MODEL_231) {
		ctx->trx_rssi_base = -91;
	} else {
		ctx->trx_rssi_base = -90;
	}

	/* Configure PHY behaviour */
	config = (1 << RF2XX_TX_AUTO_CRC_ON) |
		 (3 << RF2XX_SPI_CMD_MODE) |
		 (1 << RF2XX_IRQ_MASK_MODE);
	rf2xx_iface_reg_write(dev, RF2XX_TRX_CTRL_1_REG, config);

	config = (1 << RF2XX_RX_SAFE_MODE);
	if (ctx->trx_model != RF2XX_TRX_MODEL_232) {
		config |= (1 << RF2XX_OQPSK_SCRAM_EN);
	}
	rf2xx_iface_reg_write(dev, RF2XX_TRX_CTRL_2_REG, config);

	/* Configure INT behaviour */
	config = (1 << RF2XX_RX_START) |
		 (1 << RF2XX_TRX_END);
	rf2xx_iface_reg_write(dev, RF2XX_IRQ_MASK_REG, config);

	gpio_init_callback(&ctx->irq_cb, trx_isr_handler, BIT(conf->irq.pin));
	gpio_add_callback(ctx->irq_gpio, &ctx->irq_cb);

	return 0;
}

static inline int configure_gpios(struct device *dev)
{
	const struct rf2xx_config *conf = dev->config->config_info;
	struct rf2xx_context *ctx = dev->driver_data;

	/* Chip IRQ line */
	ctx->irq_gpio = device_get_binding(conf->irq.devname);
	if (ctx->irq_gpio == NULL) {
		LOG_ERR("Failed to get instance of %s device",
			conf->irq.devname);
		return -EINVAL;
	}
	gpio_pin_configure(ctx->irq_gpio, conf->irq.pin,
			   GPIO_DIR_IN | GPIO_INT | GPIO_INT_EDGE |
			   GPIO_PUD_PULL_DOWN | GPIO_INT_ACTIVE_HIGH);

	/* Chip RESET line */
	ctx->reset_gpio = device_get_binding(conf->reset.devname);
	if (ctx->reset_gpio == NULL) {
		LOG_ERR("Failed to get instance of %s device",
			conf->reset.devname);
		return -EINVAL;
	}
	gpio_pin_configure(ctx->reset_gpio, conf->reset.pin,
			   GPIO_DIR_OUT | GPIO_PUD_NORMAL | GPIO_POL_NORMAL);

	/* Chip SLPTR line */
	ctx->slptr_gpio = device_get_binding(conf->slptr.devname);
	if (ctx->slptr_gpio == NULL) {
		LOG_ERR("Failed to get instance of %s device",
			conf->slptr.devname);
		return -EINVAL;
	}
	gpio_pin_configure(ctx->slptr_gpio, conf->slptr.pin,
			   GPIO_DIR_OUT | GPIO_PUD_NORMAL | GPIO_POL_NORMAL);

	/* Chip DIG2 line (Optional feature) */
	ctx->dig2_gpio = device_get_binding(conf->dig2.devname);
	if (ctx->dig2_gpio != NULL) {
		LOG_INF("Optional instance of %s device activated",
			conf->dig2.devname);
		gpio_pin_configure(ctx->dig2_gpio, conf->dig2.pin,
				   GPIO_DIR_IN |
				   GPIO_PUD_PULL_DOWN |
				   GPIO_INT_ACTIVE_HIGH);
	}

	/* Chip CLKM line (Optional feature) */
	ctx->clkm_gpio = device_get_binding(conf->clkm.devname);
	if (ctx->clkm_gpio != NULL) {
		LOG_INF("Optional instance of %s device activated",
			conf->clkm.devname);
		gpio_pin_configure(ctx->clkm_gpio, conf->clkm.pin,
				   GPIO_DIR_IN | GPIO_PUD_NORMAL);
	}

	return 0;
}

static inline int configure_spi(struct device *dev)
{
	struct rf2xx_context *ctx = dev->driver_data;
	const struct rf2xx_config *conf = dev->config->config_info;

	/* Get SPI Driver Instance*/
	ctx->spi = device_get_binding(conf->spi.devname);
	if (!ctx->spi) {
		LOG_ERR("Failed to get instance of %s device",
			conf->spi.devname);

		return -ENODEV;
	}

	/* Apply SPI Config: 8-bit, MSB First, MODE-0 */
	ctx->spi_cfg.operation = SPI_WORD_SET(8) |
				 SPI_TRANSFER_MSB;
	ctx->spi_cfg.slave = conf->spi.addr;
	ctx->spi_cfg.frequency = conf->spi.freq;
	ctx->spi_cfg.cs = NULL;

	/*
	 * Get SPI Chip Select Instance
	 *
	 * This is an optinal feature configured on DTS. Some SPI controllers
	 * automatically set CS line by device slave address. Check your SPI
	 * device driver to understand if you need this option enabled.
	 */
	ctx->spi_cs.gpio_dev = device_get_binding(conf->spi.cs.devname);
	if (ctx->spi_cs.gpio_dev) {
		ctx->spi_cs.gpio_pin = conf->spi.cs.pin;
		ctx->spi_cs.delay = 0U;

		ctx->spi_cfg.cs = &ctx->spi_cs;

		LOG_DBG("SPI GPIO CS configured on %s:%u",
			conf->spi.cs.devname, conf->spi.cs.pin);
	}

	return 0;
}

static int rf2xx_init(struct device *dev)
{
	struct rf2xx_context *ctx = dev->driver_data;
	const struct rf2xx_config *conf = dev->config->config_info;
	char thread_name[20];

	LOG_DBG("\nInitialize RF2XX Transceiver\n");

	k_mutex_init(&ctx->phy_mutex);
	k_sem_init(&ctx->trx_tx_sync, 0, 1);
	k_sem_init(&ctx->trx_isr_lock, 0, 1);
	k_timer_init(&ctx->trx_isr_timeout, trx_isr_timeout, NULL);
	k_timer_user_data_set(&ctx->trx_isr_timeout, ctx);

	if (configure_gpios(dev) != 0) {
		LOG_ERR("Configuring GPIOS failed");
		return -EIO;
	}

	if (configure_spi(dev) != 0) {
		LOG_ERR("Configuring SPI failed");
		return -EIO;
	}

	LOG_DBG("GPIO and SPI configured");

	if (power_on_and_setup(dev) != 0) {
		LOG_ERR("Configuring RF2XX failed");
		return -EIO;
	}

	k_thread_create(&ctx->trx_thread,
			ctx->trx_stack,
			CONFIG_IEEE802154_RF2XX_RX_STACK_SIZE,
			(k_thread_entry_t) rf2xx_thread_main,
			dev, NULL, NULL,
			K_PRIO_COOP(2), 0, K_NO_WAIT);

	sprintf(thread_name, "802.15.4 main [%d]", conf->inst);
	k_thread_name_set(&ctx->trx_thread, thread_name);

	return 0;
}

static void rf2xx_iface_init(struct net_if *iface)
{
	struct device *dev = net_if_get_device(iface);
	struct rf2xx_context *ctx = dev->driver_data;
	u8_t *mac = get_mac(dev);

	net_if_set_link_addr(iface, mac, 8, NET_LINK_IEEE802154);

	ctx->iface = iface;

	ieee802154_init(iface);
}

static struct ieee802154_radio_api rf2xx_radio_api = {
	.iface_api.init   = rf2xx_iface_init,

	.get_capabilities = rf2xx_get_capabilities,
	.cca              = rf2xx_cca,
	.set_channel      = rf2xx_set_channel,
	.filter           = rf2xx_filter,
	.set_txpower      = rf2xx_set_txpower,
	.tx               = rf2xx_tx,
	.start            = rf2xx_start,
	.stop             = rf2xx_stop,
	.configure        = rf2xx_configure,
};

#if !defined(CONFIG_IEEE802154_RAW_MODE)
    #if defined(CONFIG_NET_L2_IEEE802154)
	#define L2 IEEE802154_L2
	#define L2_CTX_TYPE NET_L2_GET_CTX_TYPE(IEEE802154_L2)
	#define MTU RF2XX_MAX_PSDU_LENGTH
    #elif defined(CONFIG_NET_L2_OPENTHREAD)
	#define L2 OPENTHREAD_L2
	#define L2_CTX_TYPE NET_L2_GET_CTX_TYPE(OPENTHREAD_L2)
	#define MTU RF2XX_OT_PSDU_LENGTH
    #endif
#endif /* CONFIG_IEEE802154_RAW_MODE */

/*
 * Optional features place holders
 */
#ifndef DT_INST_0_ATMEL_RF2XX_DIG2_GPIOS_CONTROLLER
#define DT_INST_0_ATMEL_RF2XX_DIG2_GPIOS_CONTROLLER 0
#define DT_INST_0_ATMEL_RF2XX_DIG2_GPIOS_PIN        0
#define DT_INST_0_ATMEL_RF2XX_DIG2_GPIOS_FLAGS      0
#endif
#ifndef DT_INST_1_ATMEL_RF2XX_DIG2_GPIOS_CONTROLLER
#define DT_INST_1_ATMEL_RF2XX_DIG2_GPIOS_CONTROLLER 0
#define DT_INST_1_ATMEL_RF2XX_DIG2_GPIOS_PIN        0
#define DT_INST_1_ATMEL_RF2XX_DIG2_GPIOS_FLAGS      0
#endif
#ifndef DT_INST_0_ATMEL_RF2XX_CLKM_GPIOS_CONTROLLER
#define DT_INST_0_ATMEL_RF2XX_CLKM_GPIOS_CONTROLLER 0
#define DT_INST_0_ATMEL_RF2XX_CLKM_GPIOS_PIN        0
#define DT_INST_0_ATMEL_RF2XX_CLKM_GPIOS_FLAGS      0
#endif
#ifndef DT_INST_1_ATMEL_RF2XX_CLKM_GPIOS_CONTROLLER
#define DT_INST_1_ATMEL_RF2XX_CLKM_GPIOS_CONTROLLER 0
#define DT_INST_1_ATMEL_RF2XX_CLKM_GPIOS_PIN        0
#define DT_INST_1_ATMEL_RF2XX_CLKM_GPIOS_FLAGS      0
#endif
#ifndef DT_INST_0_ATMEL_RF2XX_CS_GPIOS_CONTROLLER
#define DT_INST_0_ATMEL_RF2XX_CS_GPIOS_CONTROLLER   0
#define DT_INST_0_ATMEL_RF2XX_CS_GPIOS_PIN          0
#define DT_INST_0_ATMEL_RF2XX_CS_GPIOS_FLAGS        0
#endif
#ifndef DT_INST_1_ATMEL_RF2XX_CS_GPIOS_CONTROLLER
#define DT_INST_1_ATMEL_RF2XX_CS_GPIOS_CONTROLLER   0
#define DT_INST_1_ATMEL_RF2XX_CS_GPIOS_PIN          0
#define DT_INST_1_ATMEL_RF2XX_CS_GPIOS_FLAGS        0
#endif

#define IEEE802154_RF2XX_DEVICE_CONFIG(n)					   \
	static const struct rf2xx_config rf2xx_ctx_config_##n = {		   \
		.inst = n,							   \
										   \
		.irq.devname = DT_INST_##n##_ATMEL_RF2XX_IRQ_GPIOS_CONTROLLER,	   \
		.irq.pin = DT_INST_##n##_ATMEL_RF2XX_IRQ_GPIOS_PIN,		   \
		.irq.flags = DT_INST_##n##_ATMEL_RF2XX_IRQ_GPIOS_FLAGS,		   \
										   \
		.reset.devname = DT_INST_##n##_ATMEL_RF2XX_RESET_GPIOS_CONTROLLER, \
		.reset.pin = DT_INST_##n##_ATMEL_RF2XX_RESET_GPIOS_PIN,		   \
		.reset.flags = DT_INST_##n##_ATMEL_RF2XX_RESET_GPIOS_FLAGS,	   \
										   \
		.slptr.devname = DT_INST_##n##_ATMEL_RF2XX_SLPTR_GPIOS_CONTROLLER, \
		.slptr.pin = DT_INST_##n##_ATMEL_RF2XX_SLPTR_GPIOS_PIN,		   \
		.slptr.flags = DT_INST_##n##_ATMEL_RF2XX_SLPTR_GPIOS_FLAGS,	   \
										   \
		.dig2.devname = DT_INST_##n##_ATMEL_RF2XX_DIG2_GPIOS_CONTROLLER,   \
		.dig2.pin = DT_INST_##n##_ATMEL_RF2XX_DIG2_GPIOS_PIN,		   \
		.dig2.flags = DT_INST_##n##_ATMEL_RF2XX_DIG2_GPIOS_FLAGS,	   \
										   \
		.clkm.devname = DT_INST_##n##_ATMEL_RF2XX_CLKM_GPIOS_CONTROLLER,   \
		.clkm.pin = DT_INST_##n##_ATMEL_RF2XX_CLKM_GPIOS_PIN,		   \
		.clkm.flags = DT_INST_##n##_ATMEL_RF2XX_CLKM_GPIOS_FLAGS,	   \
										   \
		.spi.devname = DT_INST_##n##_ATMEL_RF2XX_BUS_NAME,		   \
		.spi.addr = DT_INST_##n##_ATMEL_RF2XX_BASE_ADDRESS,		   \
		.spi.freq = DT_INST_##n##_ATMEL_RF2XX_SPI_MAX_FREQUENCY,	   \
		.spi.cs.devname = DT_INST_##n##_ATMEL_RF2XX_CS_GPIOS_CONTROLLER,   \
		.spi.cs.pin = DT_INST_##n##_ATMEL_RF2XX_CS_GPIOS_PIN,		   \
		.spi.cs.flags = DT_INST_##n##_ATMEL_RF2XX_CS_GPIOS_FLAGS,	   \
	}

#define IEEE802154_RF2XX_DEVICE_DATA(n)			   \
	static struct rf2xx_context rf2xx_ctx_data_##n = { \
		.trx_state = RF2XX_TRX_PHY_STATE_INITIAL,  \
	}

#define IEEE802154_RF2XX_RAW_DEVICE_INIT(n)	   \
	DEVICE_AND_API_INIT(			   \
		rf2xx_##n,			   \
		DT_INST_##n##_ATMEL_RF2XX_LABEL,   \
		&rf2xx_init,			   \
		&rf2xx_ctx_data_##n,		   \
		&rf2xx_ctx_config_##n,		   \
		POST_KERNEL,			   \
		CONFIG_IEEE802154_RF2XX_INIT_PRIO, \
		&rf2xx_radio_api)

#define IEEE802154_RF2XX_NET_DEVICE_INIT(n)	   \
	NET_DEVICE_INIT(			   \
		rf2xx_##n,			   \
		DT_INST_##n##_ATMEL_RF2XX_LABEL,   \
		&rf2xx_init,			   \
		&rf2xx_ctx_data_##n,		   \
		&rf2xx_ctx_config_##n,		   \
		CONFIG_IEEE802154_RF2XX_INIT_PRIO, \
		&rf2xx_radio_api,		   \
		L2,				   \
		L2_CTX_TYPE,			   \
		MTU)

#if DT_INST_0_ATMEL_RF2XX
	IEEE802154_RF2XX_DEVICE_CONFIG(0);
	IEEE802154_RF2XX_DEVICE_DATA(0);

	#if defined(CONFIG_IEEE802154_RAW_MODE)
		IEEE802154_RF2XX_RAW_DEVICE_INIT(0);
	#else
		IEEE802154_RF2XX_NET_DEVICE_INIT(0);
	#endif /* CONFIG_IEEE802154_RAW_MODE */
#endif

#if DT_INST_1_ATMEL_RF2XX
	IEEE802154_RF2XX_DEVICE_CONFIG(1);
	IEEE802154_RF2XX_DEVICE_DATA(1);

	#if defined(CONFIG_IEEE802154_RAW_MODE)
		IEEE802154_RF2XX_RAW_DEVICE_INIT(1);
	#else
		IEEE802154_RF2XX_NET_DEVICE_INIT(1);
	#endif /* CONFIG_IEEE802154_RAW_MODE */
#endif
