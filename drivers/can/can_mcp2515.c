/*
 * Copyright (c) 2018 Karsten Koenig
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <kernel.h>
#include <device.h>
#include <spi.h>
#include <gpio.h>

#define LOG_LEVEL CONFIG_CAN_LOG_LEVEL
#include <logging/log.h>
LOG_MODULE_REGISTER(mcp2515_can);

#include "can_mcp2515.h"

static int mcp2515_cmd_soft_reset(struct device *dev)
{
	u8_t cmd_buf[] = { MCP2515_OPCODE_RESET };

	const struct spi_buf tx_buf = {
		.buf = cmd_buf, .len = sizeof(cmd_buf),
	};
	const struct spi_buf_set tx = {
		.buffers = &tx_buf, .count = 1U
	};

	return spi_write(DEV_DATA(dev)->spi, &DEV_DATA(dev)->spi_cfg, &tx);
}

static int mcp2515_cmd_bit_modify(struct device *dev, u8_t reg_addr, u8_t mask,
				  u8_t data)
{
	u8_t cmd_buf[] = { MCP2515_OPCODE_BIT_MODIFY, reg_addr, mask, data };

	const struct spi_buf tx_buf = {
		.buf = cmd_buf, .len = sizeof(cmd_buf),
	};
	const struct spi_buf_set tx = {
		.buffers = &tx_buf, .count = 1U
	};

	return spi_write(DEV_DATA(dev)->spi, &DEV_DATA(dev)->spi_cfg, &tx);
}

static int mcp2515_cmd_write_reg(struct device *dev, u8_t reg_addr,
				 u8_t *buf_data, u8_t buf_len)
{
	u8_t cmd_buf[] = { MCP2515_OPCODE_WRITE, reg_addr };

	struct spi_buf tx_buf[] = {
		{ .buf = cmd_buf, .len = sizeof(cmd_buf) },
		{ .buf = buf_data, .len = buf_len }
	};
	const struct spi_buf_set tx = {
		.buffers = tx_buf, .count = ARRAY_SIZE(tx_buf)
	};

	return spi_write(DEV_DATA(dev)->spi, &DEV_DATA(dev)->spi_cfg, &tx);
}

static int mcp2515_cmd_read_reg(struct device *dev, u8_t reg_addr,
				u8_t *buf_data, u8_t buf_len)
{
	u8_t cmd_buf[] = { MCP2515_OPCODE_READ, reg_addr };

	struct spi_buf tx_buf[] = {
		{ .buf = cmd_buf, .len = sizeof(cmd_buf) },
		{ .buf = NULL, .len = buf_len }
	};
	const struct spi_buf_set tx = {
		.buffers = tx_buf, .count = ARRAY_SIZE(tx_buf)
	};
	struct spi_buf rx_buf[] = {
		{ .buf = NULL, .len = sizeof(cmd_buf) },
		{ .buf = buf_data, .len = buf_len }
	};
	const struct spi_buf_set rx = {
		.buffers = rx_buf, .count = ARRAY_SIZE(rx_buf)
	};

	return spi_transceive(DEV_DATA(dev)->spi, &DEV_DATA(dev)->spi_cfg,
			      &tx, &rx);
}

static u8_t mcp2515_convert_canmode_to_mcp2515mode(enum can_mode mode)
{
	switch (mode) {
	case CAN_NORMAL_MODE:
		return MCP2515_MODE_NORMAL;
	case CAN_SILENT_MODE:
		return MCP2515_MODE_SILENT;
	case CAN_LOOPBACK_MODE:
		return MCP2515_MODE_LOOPBACK;
	default:
		LOG_ERR("Unsupported CAN Mode %u", mode);
		return MCP2515_MODE_SILENT;
	}
}

static void mcp2515_convert_zcanframe_to_mcp2515frame(const struct zcan_frame
						      *source, u8_t *target)
{
	u8_t rtr;
	u8_t dlc;
	u8_t data_idx = 0U;

	if (source->id_type == CAN_STANDARD_IDENTIFIER) {
		target[MCP2515_FRAME_OFFSET_SIDH] = source->std_id >> 3;
		target[MCP2515_FRAME_OFFSET_SIDL] =
			(source->std_id & 0x07) << 5;
	} else {
		target[MCP2515_FRAME_OFFSET_SIDH] = source->ext_id >> 21;
		target[MCP2515_FRAME_OFFSET_SIDL] =
			(((source->ext_id >> 18) & 0x07) << 5) | (BIT(3)) |
			((source->ext_id >> 16) & 0x03);
		target[MCP2515_FRAME_OFFSET_EID8] = source->ext_id >> 8;
		target[MCP2515_FRAME_OFFSET_EID0] = source->ext_id;
	}

	rtr = (source->rtr == CAN_REMOTEREQUEST) ? BIT(6) : 0;
	dlc = (source->dlc) & 0x0F;

	target[MCP2515_FRAME_OFFSET_DLC] = rtr | dlc;

	for (; data_idx < 8; data_idx++) {
		target[MCP2515_FRAME_OFFSET_D0 + data_idx] =
			source->data[data_idx];
	}
}

static void mcp2515_convert_mcp2515frame_to_zcanframe(const u8_t *source,
						      struct zcan_frame *target)
{
	u8_t data_idx = 0U;

	if (source[MCP2515_FRAME_OFFSET_SIDL] & BIT(3)) {
		target->id_type = CAN_EXTENDED_IDENTIFIER;
		target->ext_id =
			(source[MCP2515_FRAME_OFFSET_SIDH] << 21) |
			((source[MCP2515_FRAME_OFFSET_SIDL] >> 5) << 18) |
			((source[MCP2515_FRAME_OFFSET_SIDL] & 0x03) << 16) |
			(source[MCP2515_FRAME_OFFSET_EID8] << 8) |
			source[MCP2515_FRAME_OFFSET_EID0];
	} else {
		target->id_type = CAN_STANDARD_IDENTIFIER;
		target->std_id = (source[MCP2515_FRAME_OFFSET_SIDH] << 3) |
				 (source[MCP2515_FRAME_OFFSET_SIDL] >> 5);
	}

	target->dlc = source[MCP2515_FRAME_OFFSET_DLC] & 0x0F;
	target->rtr = source[MCP2515_FRAME_OFFSET_DLC] & BIT(6) ?
		      CAN_REMOTEREQUEST : CAN_DATAFRAME;

	for (; data_idx < 8; data_idx++) {
		target->data[data_idx] = source[MCP2515_FRAME_OFFSET_D0 +
						data_idx];
	}
}

const int mcp2515_set_mode(struct device *dev, u8_t mcp2515_mode)
{
	u8_t canstat;

	mcp2515_cmd_bit_modify(dev, MCP2515_ADDR_CANCTRL,
			       MCP2515_CANCTRL_MODE_MASK,
			       mcp2515_mode << MCP2515_CANCTRL_MODE_POS);
	mcp2515_cmd_read_reg(dev, MCP2515_ADDR_CANSTAT, &canstat, 1);

	if (((canstat & MCP2515_CANSTAT_MODE_MASK) >> MCP2515_CANSTAT_MODE_POS)
	    != mcp2515_mode) {
		LOG_ERR("Failed to set MCP2515 operation mode");
		return -EIO;
	}

	return 0;
}

static int mcp2515_configure(struct device *dev, enum can_mode mode,
			     u32_t bitrate)
{
	const struct mcp2515_config *dev_cfg = DEV_CFG(dev);

	/* CNF3, CNF2, CNF1, CANINTE */
	u8_t config_buf[4];

	if (bitrate == 0) {
		bitrate = dev_cfg->bus_speed;
	}

	const u8_t bit_length = 1 + dev_cfg->tq_prop + dev_cfg->tq_bs1 +
				dev_cfg->tq_bs2;

	/* CNF1; SJW<7:6> | BRP<5:0> */
	u8_t brp =
		(CONFIG_CAN_MCP2515_OSC_FREQ / (bit_length * bitrate * 2)) - 1;
	const u8_t sjw = (dev_cfg->tq_sjw - 1) << 6;
	u8_t cnf1 = sjw | brp;

	/* CNF2; BTLMODE<7>|SAM<6>|PHSEG1<5:3>|PRSEG<2:0> */
	const u8_t btlmode = 1 << 7;
	const u8_t sam = 0 << 6;
	const u8_t phseg1 = (dev_cfg->tq_bs1 - 1) << 3;
	const u8_t prseg = (dev_cfg->tq_prop - 1);

	const u8_t cnf2 = btlmode | sam | phseg1 | prseg;

	/* CNF3; SOF<7>|WAKFIL<6>|UND<5:3>|PHSEG2<2:0> */
	const u8_t sof = 0 << 7;
	const u8_t wakfil = 0 << 6;
	const u8_t und = 0 << 3;
	const u8_t phseg2 = (dev_cfg->tq_bs2 - 1);

	const u8_t cnf3 = sof | wakfil | und | phseg2;

	/* CANINTE
	 * MERRE<7>:WAKIE<6>:ERRIE<5>:TX2IE<4>:TX1IE<3>:TX0IE<2>:RX1IE<1>:
	 * RX0IE<0>
	 * all TX and RX buffer interrupts enabled
	 */
	const u8_t caninte = BIT(4) | BIT(3) | BIT(2) | BIT(1) | BIT(0);

	/* Receive everything, filtering done in driver, RXB0 roll over into
	 * RXB1 */
	const u8_t rx0_ctrl = BIT(6) | BIT(5) | BIT(2);
	const u8_t rx1_ctrl = BIT(6) | BIT(5);

	__ASSERT((dev_cfg->tq_sjw >= 1) && (dev_cfg->tq_sjw <= 4),
		 "1 <= SJW <= 4");
	__ASSERT((dev_cfg->tq_prop >= 1) && (dev_cfg->tq_prop <= 8),
		 "1 <= PROP <= 8");
	__ASSERT((dev_cfg->tq_bs1 >= 1) && (dev_cfg->tq_bs1 <= 8),
		 "1 <= BS1 <= 8");
	__ASSERT((dev_cfg->tq_bs2 >= 2) && (dev_cfg->tq_bs2 <= 8),
		 "2 <= BS2 <= 8");
	__ASSERT(dev_cfg->tq_prop + dev_cfg->tq_bs1 >= dev_cfg->tq_bs2,
		 "PROP + BS1 >= BS2");
	__ASSERT(dev_cfg->tq_bs2 > dev_cfg->tq_sjw, "BS2 > SJW");

	if (CONFIG_CAN_MCP2515_OSC_FREQ % (bit_length * bitrate * 2)) {
		LOG_ERR("Prescaler is not a natural number! "
			"prescaler = osc_rate / ((PROP + SEG1 + SEG2 + 1) "
			"* bitrate * 2)\n"
			"prescaler = %d / ((%d + %d + %d + 1) * %d * 2)",
			CONFIG_CAN_MCP2515_OSC_FREQ, dev_cfg->tq_prop,
			dev_cfg->tq_bs1, dev_cfg->tq_bs2, bitrate);
	}

	config_buf[0] = cnf3;
	config_buf[1] = cnf2;
	config_buf[2] = cnf1;
	config_buf[3] = caninte;

	/* will enter configuration mode automatically */
	mcp2515_cmd_soft_reset(dev);

	mcp2515_cmd_write_reg(dev, MCP2515_ADDR_CNF3, config_buf,
			      sizeof(config_buf));

	mcp2515_cmd_bit_modify(dev, MCP2515_ADDR_RXB0CTRL, rx0_ctrl, rx0_ctrl);
	mcp2515_cmd_bit_modify(dev, MCP2515_ADDR_RXB1CTRL, rx1_ctrl, rx1_ctrl);

	return mcp2515_set_mode(dev,
				mcp2515_convert_canmode_to_mcp2515mode(mode));
}

static int mcp2515_send(struct device *dev, const struct zcan_frame *msg,
		 s32_t timeout, can_tx_callback_t callback, void *callback_arg)
{
	struct mcp2515_data *dev_data = DEV_DATA(dev);
	u8_t tx_idx = 0U;
	u8_t addr_tx_ctrl;
	u8_t tx_frame[MCP2515_FRAME_LEN];

	if (k_sem_take(&dev_data->tx_sem, timeout) != 0) {
		return CAN_TIMEOUT;
	}

	k_mutex_lock(&dev_data->tx_mutex, K_FOREVER);

	/* find a free tx slot */
	for (; tx_idx < MCP2515_TX_CNT; tx_idx++) {
		if ((BIT(tx_idx) & dev_data->tx_busy_map) == 0) {
			dev_data->tx_busy_map |= BIT(tx_idx);
			break;
		}
	}

	k_mutex_unlock(&dev_data->tx_mutex);

	if (tx_idx == MCP2515_TX_CNT) {
		LOG_WRN("no free tx slot available");
		return CAN_TX_ERR;
	}

	dev_data->tx_cb[tx_idx].cb = callback;
	dev_data->tx_cb[tx_idx].cb_arg = callback_arg;

	addr_tx_ctrl = MCP2515_ADDR_TXB0CTRL +
		       (tx_idx * MCP2515_ADDR_OFFSET_FRAME2FRAME);

	mcp2515_convert_zcanframe_to_mcp2515frame(msg, tx_frame);
	mcp2515_cmd_write_reg(dev,
			      addr_tx_ctrl + MCP2515_ADDR_OFFSET_CTRL2FRAME,
			      tx_frame, sizeof(tx_frame));
	/* request tx slot transmission */
	mcp2515_cmd_bit_modify(dev, addr_tx_ctrl, MCP2515_TXCTRL_TXREQ,
			       MCP2515_TXCTRL_TXREQ);

	if (callback == NULL) {
		k_sem_take(&dev_data->tx_cb[tx_idx].sem, K_FOREVER);
	}

	return 0;
}

static int mcp2515_attach_isr(struct device *dev, can_rx_callback_t rx_cb,
			      void *cb_arg,
			      const struct zcan_filter *filter)
{
	struct mcp2515_data *dev_data = DEV_DATA(dev);
	int filter_idx = 0;

	__ASSERT(rx_cb != NULL, "response_ptr can not be null");

	k_mutex_lock(&dev_data->filter_mutex, K_FOREVER);

	/* find free filter */
	while ((BIT(filter_idx) & dev_data->filter_usage)
	       && (filter_idx < CONFIG_CAN_MCP2515_MAX_FILTER)) {
		filter_idx++;
	}

	/* setup filter */
	if (filter_idx < CONFIG_CAN_MCP2515_MAX_FILTER) {
		dev_data->filter_usage |= BIT(filter_idx);

		dev_data->filter[filter_idx] = *filter;
		dev_data->rx_cb[filter_idx] = rx_cb;
		dev_data->cb_arg[filter_idx] = cb_arg;

	} else {
		filter_idx = CAN_NO_FREE_FILTER;
	}

	k_mutex_unlock(&dev_data->filter_mutex);

	return filter_idx;
}

static void mcp2515_detach(struct device *dev, int filter_nr)
{
	struct mcp2515_data *dev_data = DEV_DATA(dev);

	k_mutex_lock(&dev_data->filter_mutex, K_FOREVER);
	dev_data->filter_usage &= ~BIT(filter_nr);
	k_mutex_unlock(&dev_data->filter_mutex);
}

static u8_t mcp2515_filter_match(struct zcan_frame *msg,
				 struct zcan_filter *filter)
{
	if (msg->id_type != filter->id_type) {
		return 0;
	}

	if ((msg->rtr ^ filter->rtr) & filter->rtr_mask) {
		return 0;
	}

	if (msg->id_type == CAN_STANDARD_IDENTIFIER) {
		if ((msg->std_id ^ filter->std_id) & filter->std_id_mask) {
			return 0;
		}
	} else {
		if ((msg->ext_id ^ filter->ext_id) & filter->ext_id_mask) {
			return 0;
		}
	}

	return 1;
}

static void mcp2515_rx_filter(struct device *dev, struct zcan_frame *msg)
{
	struct mcp2515_data *dev_data = DEV_DATA(dev);
	u8_t filter_idx = 0U;
	can_rx_callback_t callback;
	struct zcan_frame tmp_msg;

	k_mutex_lock(&dev_data->filter_mutex, K_FOREVER);

	for (; filter_idx < CONFIG_CAN_MCP2515_MAX_FILTER; filter_idx++) {
		if (!(BIT(filter_idx) & dev_data->filter_usage)) {
			continue; /* filter slot empty */
		}

		if (!mcp2515_filter_match(msg,
					  &dev_data->filter[filter_idx])) {
			continue; /* filter did not match */
		}

		callback = dev_data->rx_cb[filter_idx];
		/*Make a temporary copy in case the user modifies the message*/
		tmp_msg = *msg;

		callback(&tmp_msg, dev_data->cb_arg[filter_idx]);
	}

	k_mutex_unlock(&dev_data->filter_mutex);
}

static void mcp2515_rx(struct device *dev, u8_t rx_idx)
{
	struct zcan_frame msg;
	u8_t rx_frame[MCP2515_FRAME_LEN];
	u8_t addr_rx_ctrl = MCP2515_ADDR_RXB0CTRL +
			    (rx_idx * MCP2515_ADDR_OFFSET_FRAME2FRAME);

	/* Fetch rx buffer */
	mcp2515_cmd_read_reg(dev,
			     addr_rx_ctrl + MCP2515_ADDR_OFFSET_CTRL2FRAME,
			     rx_frame, sizeof(rx_frame));
	mcp2515_convert_mcp2515frame_to_zcanframe(rx_frame, &msg);
	mcp2515_rx_filter(dev, &msg);
}

static void mcp2515_tx_done(struct device *dev, u8_t tx_idx)
{
	struct mcp2515_data *dev_data = DEV_DATA(dev);

	if (dev_data->tx_cb[tx_idx].cb == NULL) {
		k_sem_give(&dev_data->tx_cb[tx_idx].sem);
	} else {
		dev_data->tx_cb[tx_idx].cb(0, dev_data->tx_cb[tx_idx].cb_arg);
	}

	k_mutex_lock(&dev_data->tx_mutex, K_FOREVER);
	dev_data->tx_busy_map &= ~BIT(tx_idx);
	k_mutex_unlock(&dev_data->tx_mutex);
	k_sem_give(&dev_data->tx_sem);
}

static void mcp2515_handle_interrupts(struct device *dev)
{
	u8_t canintf;

	mcp2515_cmd_read_reg(dev, MCP2515_ADDR_CANINTF, &canintf, 1);

	while (canintf != 0) {
		if (canintf & MCP2515_CANINTF_RX0IF) {
			mcp2515_rx(dev, 0);
		}

		if (canintf & MCP2515_CANINTF_RX1IF) {
			mcp2515_rx(dev, 1);
		}

		if (canintf & MCP2515_CANINTF_TX0IF) {
			mcp2515_tx_done(dev, 0);
		}

		if (canintf & MCP2515_CANINTF_TX0IF) {
			mcp2515_tx_done(dev, 1);
		}

		if (canintf & MCP2515_CANINTF_TX0IF) {
			mcp2515_tx_done(dev, 2);
		}

		/* clear the flags we handled */
		mcp2515_cmd_bit_modify(dev, MCP2515_ADDR_CANINTF, canintf,
				       ~canintf);

		/* check that no new interrupts happened while clearing known
		 * ones */
		mcp2515_cmd_read_reg(dev, MCP2515_ADDR_CANINTF, &canintf, 1);
	}
}

static void mcp2515_int_thread(struct device *dev)
{
	struct mcp2515_data *dev_data = DEV_DATA(dev);

	while (1) {
		k_sem_take(&dev_data->int_sem, K_FOREVER);
		mcp2515_handle_interrupts(dev);
	}
}

static void mcp2515_int_gpio_callback(struct device *dev,
				      struct gpio_callback *cb, u32_t pins)
{
	struct mcp2515_data *dev_data =
		CONTAINER_OF(cb, struct mcp2515_data, int_gpio_cb);

	k_sem_give(&dev_data->int_sem);
}

static const struct can_driver_api can_api_funcs = {
	.configure = mcp2515_configure,
	.send = mcp2515_send,
	.attach_isr = mcp2515_attach_isr,
	.detach = mcp2515_detach
};


static int mcp2515_init(struct device *dev)
{
	const struct mcp2515_config *dev_cfg = DEV_CFG(dev);
	struct mcp2515_data *dev_data = DEV_DATA(dev);
	int ret;

	k_sem_init(&dev_data->int_sem, 0, UINT_MAX);
	k_mutex_init(&dev_data->tx_mutex);
	k_sem_init(&dev_data->tx_sem, 3, 3);
	k_sem_init(&dev_data->tx_cb[0].sem, 0, 1);
	k_sem_init(&dev_data->tx_cb[1].sem, 0, 1);
	k_sem_init(&dev_data->tx_cb[2].sem, 0, 1);
	k_mutex_init(&dev_data->filter_mutex);

	/* SPI config */
	dev_data->spi_cfg.operation = SPI_WORD_SET(8);
	dev_data->spi_cfg.frequency = dev_cfg->spi_freq;
	dev_data->spi_cfg.slave = dev_cfg->spi_slave;

	dev_data->spi = device_get_binding(dev_cfg->spi_port);
	if (!dev_data->spi) {
		LOG_ERR("SPI master port %s not found", dev_cfg->spi_port);
		return -EINVAL;
	}

#ifdef DT_MICROCHIP_MCP2515_0_CS_GPIO_PIN
	dev_data->spi_cs_ctrl.gpio_dev =
		device_get_binding(dev_cfg->spi_cs_port);
	if (!dev_data->spi_cs_ctrl.gpio_dev) {
		LOG_ERR("Unable to get GPIO SPI CS device");
		return -ENODEV;
	}

	dev_data->spi_cs_ctrl.gpio_pin = dev_cfg->spi_cs_pin;
	dev_data->spi_cs_ctrl.delay = 0U;

	dev_data->spi_cfg.cs = &dev_data->spi_cs_ctrl;
#else
	dev_data->spi_cfg.cs = NULL;
#endif  /* DT_MICROCHIP_MCP2515_0_CS_GPIO_PIN */

	/* Reset MCP2515 */
	if (mcp2515_cmd_soft_reset(dev)) {
		LOG_ERR("Soft-reset failed");
		return -EIO;
	}

	/* Initialize interrupt handling  */
	dev_data->int_gpio = device_get_binding(dev_cfg->int_port);
	if (dev_data->int_gpio == NULL) {
		LOG_ERR("GPIO port %s not found", dev_cfg->int_port);
		return -EINVAL;
	}

	if (gpio_pin_configure(dev_data->int_gpio, dev_cfg->int_pin,
			       (GPIO_DIR_IN | GPIO_INT | GPIO_INT_EDGE
				| GPIO_INT_ACTIVE_LOW | GPIO_INT_DEBOUNCE))) {
		LOG_ERR("Unable to configure GPIO pin %u", dev_cfg->int_pin);
		return -EINVAL;
	}

	gpio_init_callback(&(dev_data->int_gpio_cb), mcp2515_int_gpio_callback,
			   BIT(dev_cfg->int_pin));

	if (gpio_add_callback(dev_data->int_gpio, &(dev_data->int_gpio_cb))) {
		return -EINVAL;
	}

	if (gpio_pin_enable_callback(dev_data->int_gpio, dev_cfg->int_pin)) {
		return -EINVAL;
	}

	k_thread_create(&dev_data->int_thread, dev_data->int_thread_stack,
			dev_cfg->int_thread_stack_size,
			(k_thread_entry_t) mcp2515_int_thread, (void *)dev,
			NULL, NULL, K_PRIO_COOP(dev_cfg->int_thread_priority),
			0, K_NO_WAIT);

	(void)memset(dev_data->rx_cb, 0, sizeof(dev_data->rx_cb));
	(void)memset(dev_data->filter, 0, sizeof(dev_data->filter));

	ret = mcp2515_configure(dev, CAN_NORMAL_MODE, dev_cfg->bus_speed);

	return ret;
}

#ifdef CONFIG_CAN_1

static K_THREAD_STACK_DEFINE(mcp2515_int_thread_stack,
			     CONFIG_CAN_MCP2515_INT_THREAD_STACK_SIZE);

static struct mcp2515_data mcp2515_data_1 = {
	.int_thread_stack = mcp2515_int_thread_stack,
	.tx_cb[0].cb = NULL,
	.tx_cb[1].cb = NULL,
	.tx_cb[2].cb = NULL,
	.tx_busy_map = 0U,
	.filter_usage = 0U,
};

static const struct mcp2515_config mcp2515_config_1 = {
	.spi_port = DT_MICROCHIP_MCP2515_0_BUS_NAME,
	.spi_freq = DT_MICROCHIP_MCP2515_0_SPI_MAX_FREQUENCY,
	.spi_slave = DT_MICROCHIP_MCP2515_0_BASE_ADDRESS,
	.int_pin = DT_MICROCHIP_MCP2515_0_INT_GPIOS_PIN,
	.int_port = DT_MICROCHIP_MCP2515_0_INT_GPIOS_CONTROLLER,
	.int_thread_stack_size = CONFIG_CAN_MCP2515_INT_THREAD_STACK_SIZE,
	.int_thread_priority = CONFIG_CAN_MCP2515_INT_THREAD_PRIO,
#ifdef DT_MICROCHIP_MCP2515_0_CS_GPIO_PIN
	.spi_cs_pin = DT_MICROCHIP_MCP2515_0_CS_GPIO_PIN,
	.spi_cs_port = DT_MICROCHIP_MCP2515_0_CS_GPIO_CONTROLLER,
#endif  /* DT_MICROCHIP_MCP2515_0_CS_GPIO_PIN */
	.tq_sjw = CONFIG_CAN_SJW,
	.tq_prop = CONFIG_CAN_PROP_SEG,
	.tq_bs1 = CONFIG_CAN_PHASE_SEG1,
	.tq_bs2 = CONFIG_CAN_PHASE_SEG2,
	.bus_speed = DT_MICROCHIP_MCP2515_0_BUS_SPEED,
};

DEVICE_AND_API_INIT(can_mcp2515_1, DT_MICROCHIP_MCP2515_0_LABEL, &mcp2515_init,
		    &mcp2515_data_1, &mcp2515_config_1, POST_KERNEL,
		    CONFIG_CAN_MCP2515_INIT_PRIORITY, &can_api_funcs);

#endif /* CONFIG_CAN_1 */
