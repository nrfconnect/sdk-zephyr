/*
 * Copyright (c) 2018 Diego Sueiro, <diego.sueiro@gmail.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <i2c.h>
#include <soc.h>
#include <i2c_imx.h>
#include <misc/util.h>

#include <logging/log.h>
LOG_MODULE_REGISTER(i2c_imx);

#include "i2c-priv.h"

#define DEV_CFG(dev) \
	((const struct i2c_imx_config * const)(dev)->config->config_info)
#define DEV_DATA(dev) \
	((struct i2c_imx_data * const)(dev)->driver_data)
#define DEV_BASE(dev) \
	((I2C_Type *)(DEV_CFG(dev))->base)

struct i2c_imx_config {
	I2C_Type *base;
	void (*irq_config_func)(struct device *dev);
	u32_t bitrate;
};

struct i2c_master_transfer {
	const u8_t     *txBuff;
	volatile u8_t  *rxBuff;
	volatile u32_t	cmdSize;
	volatile u32_t	txSize;
	volatile u32_t	rxSize;
	volatile bool	isBusy;
	volatile u32_t	currentDir;
	volatile u32_t	currentMode;
	volatile bool	ack;
};

struct i2c_imx_data {
	struct i2c_master_transfer transfer;
	struct k_sem device_sync_sem;
};

static bool i2c_imx_write(struct device *dev, u8_t *txBuffer, u8_t txSize)
{
	I2C_Type *base = DEV_BASE(dev);
	struct i2c_imx_data *data = DEV_DATA(dev);
	struct i2c_master_transfer *transfer = &data->transfer;

	transfer->isBusy = true;

	/* Clear I2C interrupt flag to avoid spurious interrupt */
	I2C_ClearStatusFlag(base, i2cStatusInterrupt);

	/* Set I2C work under Tx mode */
	I2C_SetDirMode(base, i2cDirectionTransmit);
	transfer->currentDir = i2cDirectionTransmit;

	transfer->txBuff = txBuffer;
	transfer->txSize = txSize;

	I2C_WriteByte(base, *transfer->txBuff);
	transfer->txBuff++;
	transfer->txSize--;

	/* Enable I2C interrupt, subsequent data transfer will be handled
	 * in ISR.
	 */
	I2C_SetIntCmd(base, true);

	/* Wait for the transfer to complete */
	k_sem_take(&data->device_sync_sem, K_FOREVER);

	return transfer->ack;
}

static void i2c_imx_read(struct device *dev, u8_t *rxBuffer, u8_t rxSize)
{
	I2C_Type *base = DEV_BASE(dev);
	struct i2c_imx_data *data = DEV_DATA(dev);
	struct i2c_master_transfer *transfer = &data->transfer;

	transfer->isBusy = true;

	/* Clear I2C interrupt flag to avoid spurious interrupt */
	I2C_ClearStatusFlag(base, i2cStatusInterrupt);

	/* Change to receive state. */
	I2C_SetDirMode(base, i2cDirectionReceive);
	transfer->currentDir = i2cDirectionReceive;

	transfer->rxBuff = rxBuffer;
	transfer->rxSize = rxSize;

	if (transfer->rxSize == 1U) {
		/* Send Nack */
		I2C_SetAckBit(base, false);
	} else {
		/* Send Ack */
		I2C_SetAckBit(base, true);
	}

	/* dummy read to clock in 1st byte */
	I2C_ReadByte(base);

	/* Enable I2C interrupt, subsequent data transfer will be handled
	 * in ISR.
	 */
	I2C_SetIntCmd(base, true);

	/* Wait for the transfer to complete */
	k_sem_take(&data->device_sync_sem, K_FOREVER);

}

static int i2c_imx_configure(struct device *dev, u32_t dev_config_raw)
{
	I2C_Type *base = DEV_BASE(dev);
	struct i2c_imx_data *data = DEV_DATA(dev);
	struct i2c_master_transfer *transfer = &data->transfer;
	u32_t baudrate;

	if (!(I2C_MODE_MASTER & dev_config_raw)) {
		return -EINVAL;
	}

	if (I2C_ADDR_10_BITS & dev_config_raw) {
		return -EINVAL;
	}

	/* Initialize I2C state structure content. */
	transfer->txBuff = 0;
	transfer->rxBuff = 0;
	transfer->cmdSize = 0U;
	transfer->txSize = 0U;
	transfer->rxSize = 0U;
	transfer->isBusy = false;
	transfer->currentDir = i2cDirectionReceive;
	transfer->currentMode = i2cModeSlave;

	switch (I2C_SPEED_GET(dev_config_raw)) {
	case I2C_SPEED_STANDARD:
		baudrate = KHZ(100);
		break;
	case I2C_SPEED_FAST:
		baudrate = MHZ(1);
		break;
	default:
		return -EINVAL;
	}

	/* Setup I2C init structure. */
	i2c_init_config_t i2cInitConfig = {
		.baudRate	  = baudrate,
		.slaveAddress = 0x00
	};

	i2cInitConfig.clockRate = get_i2c_clock_freq(base);

	I2C_Init(base, &i2cInitConfig);

	I2C_Enable(base);

	return 0;
}

static int i2c_imx_send_addr(struct device *dev, u16_t addr, u8_t flags)
{
	u8_t byte0 = addr << 1;

	byte0 |= (flags & I2C_MSG_RW_MASK) == I2C_MSG_READ;
	return i2c_imx_write(dev, &byte0, 1);
}

static int i2c_imx_transfer(struct device *dev, struct i2c_msg *msgs,
		u8_t num_msgs, u16_t addr)
{
	I2C_Type *base = DEV_BASE(dev);
	struct i2c_imx_data *data = DEV_DATA(dev);
	struct i2c_master_transfer *transfer = &data->transfer;
	u8_t *buf, *buf_end;
	u16_t timeout = UINT16_MAX;
	int result = -EIO;

	if (!num_msgs) {
		return 0;
	}

	/* Wait until bus not busy */
	while ((I2C_I2SR_REG(base) & i2cStatusBusBusy) && (--timeout)) {
	}

	if (timeout == 0U) {
		return result;
	}

	/* Make sure we're in a good state so slave recognises the Start */
	I2C_SetWorkMode(base, i2cModeSlave);
	transfer->currentMode = i2cModeSlave;
	/* Switch back to Rx direction. */
	I2C_SetDirMode(base, i2cDirectionReceive);
	transfer->currentDir = i2cDirectionReceive;
	/* Start condition */
	I2C_SetDirMode(base, i2cDirectionTransmit);
	transfer->currentDir = i2cDirectionTransmit;
	I2C_SetWorkMode(base, i2cModeMaster);
	transfer->currentMode = i2cModeMaster;

	/* Send address after any Start condition */
	if (!i2c_imx_send_addr(dev, addr, msgs->flags)) {
		goto finish; /* No ACK received */
	}

	do {
		if (msgs->flags & I2C_MSG_RESTART) {
			I2C_SendRepeatStart(base);
			if (!i2c_imx_send_addr(dev, addr, msgs->flags)) {
				goto finish; /* No ACK received */
			}
		}

		/* Transfer data */
		buf = msgs->buf;
		buf_end = buf + msgs->len;
		if ((msgs->flags & I2C_MSG_RW_MASK) == I2C_MSG_READ) {
			i2c_imx_read(dev, msgs->buf, msgs->len);
		} else {
			if (!i2c_imx_write(dev, msgs->buf, msgs->len)) {
				goto finish; /* No ACK received */
			}
		}

		if (msgs->flags & I2C_MSG_STOP) {
			I2C_SetWorkMode(base, i2cModeSlave);
			transfer->currentMode = i2cModeSlave;
			I2C_SetDirMode(base, i2cDirectionReceive);
			transfer->currentDir = i2cDirectionReceive;
		}

		/* Next message */
		msgs++;
		num_msgs--;
	} while (num_msgs);

	/* Complete without error */
	result = 0;
	return result;

finish:
	I2C_SetWorkMode(base, i2cModeSlave);
	transfer->currentMode = i2cModeSlave;
	I2C_SetDirMode(base, i2cDirectionReceive);
	transfer->currentDir = i2cDirectionReceive;

	return result;
}


static void i2c_imx_isr(void *arg)
{
	struct device *dev = (struct device *)arg;
	I2C_Type *base = DEV_BASE(dev);
	struct i2c_imx_data *data = DEV_DATA(dev);
	struct i2c_master_transfer *transfer = &data->transfer;

	/* Clear interrupt flag. */
	I2C_ClearStatusFlag(base, i2cStatusInterrupt);


	/* Exit the ISR if no transfer is happening for this instance. */
	if (!transfer->isBusy)
		return;

	if (i2cModeMaster == transfer->currentMode) {
		if (i2cDirectionTransmit == transfer->currentDir) {
			/* Normal write operation. */
			transfer->ack =
			!(I2C_GetStatusFlag(base, i2cStatusReceivedAck));

			if (transfer->txSize == 0U) {
				/* Close I2C interrupt. */
				I2C_SetIntCmd(base, false);
				/* Release I2C Bus. */
				transfer->isBusy = false;
				k_sem_give(&data->device_sync_sem);
			} else {
				I2C_WriteByte(base, *transfer->txBuff);
				transfer->txBuff++;
				transfer->txSize--;
			}
		} else {
			/* Normal read operation. */
			if (transfer->rxSize == 2U) {
				/* Send Nack */
				I2C_SetAckBit(base, false);
			} else {
				/* Send Ack */
				I2C_SetAckBit(base, true);
			}

			if (transfer->rxSize == 1U) {
				/* Switch back to Tx direction to avoid
				 * additional I2C bus read.
				 */
				I2C_SetDirMode(base, i2cDirectionTransmit);
				transfer->currentDir = i2cDirectionTransmit;
			}

			*transfer->rxBuff = I2C_ReadByte(base);
			transfer->rxBuff++;
			transfer->rxSize--;

			/* receive finished. */
			if (transfer->rxSize == 0U) {
				/* Close I2C interrupt. */
				I2C_SetIntCmd(base, false);
				/* Release I2C Bus. */
				transfer->isBusy = false;
				k_sem_give(&data->device_sync_sem);
			}
		}
	}
}

static int i2c_imx_init(struct device *dev)
{
	const struct i2c_imx_config *config = DEV_CFG(dev);
	struct i2c_imx_data *data = DEV_DATA(dev);
	u32_t bitrate_cfg;
	int error;

	k_sem_init(&data->device_sync_sem, 0, UINT_MAX);

	bitrate_cfg = i2c_map_dt_bitrate(config->bitrate);

	error = i2c_imx_configure(dev, I2C_MODE_MASTER | bitrate_cfg);
	if (error) {
		return error;
	}

	config->irq_config_func(dev);

	return 0;
}

static const struct i2c_driver_api i2c_imx_driver_api = {
	.configure = i2c_imx_configure,
	.transfer = i2c_imx_transfer,
};

#ifdef CONFIG_I2C_1
static void i2c_imx_config_func_1(struct device *dev);

static const struct i2c_imx_config i2c_imx_config_1 = {
	.base = (I2C_Type *)DT_FSL_IMX7D_I2C_I2C_1_BASE_ADDRESS,
	.irq_config_func = i2c_imx_config_func_1,
	.bitrate = DT_FSL_IMX7D_I2C_I2C_1_CLOCK_FREQUENCY,
};

static struct i2c_imx_data i2c_imx_data_1;

DEVICE_AND_API_INIT(i2c_imx_1, DT_FSL_IMX7D_I2C_I2C_1_LABEL, &i2c_imx_init,
			&i2c_imx_data_1, &i2c_imx_config_1,
			POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEVICE,
			&i2c_imx_driver_api);

static void i2c_imx_config_func_1(struct device *dev)
{
	ARG_UNUSED(dev);

	IRQ_CONNECT(DT_FSL_IMX7D_I2C_I2C_1_IRQ, DT_FSL_IMX7D_I2C_I2C_1_IRQ_PRIORITY,
			i2c_imx_isr, DEVICE_GET(i2c_imx_1), 0);

	irq_enable(DT_FSL_IMX7D_I2C_I2C_1_IRQ);
}
#endif /* CONFIG_I2C_1 */

#ifdef CONFIG_I2C_2
static void i2c_imx_config_func_2(struct device *dev);

static const struct i2c_imx_config i2c_imx_config_2 = {
	.base = (I2C_Type *)DT_FSL_IMX7D_I2C_I2C_2_BASE_ADDRESS,
	.irq_config_func = i2c_imx_config_func_2,
	.bitrate = DT_FSL_IMX7D_I2C_I2C_2_CLOCK_FREQUENCY,
};

static struct i2c_imx_data i2c_imx_data_2;

DEVICE_AND_API_INIT(i2c_imx_2, DT_FSL_IMX7D_I2C_I2C_2_LABEL, &i2c_imx_init,
			&i2c_imx_data_2, &i2c_imx_config_2,
			POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEVICE,
			&i2c_imx_driver_api);

static void i2c_imx_config_func_2(struct device *dev)
{
	ARG_UNUSED(dev);

	IRQ_CONNECT(DT_FSL_IMX7D_I2C_I2C_2_IRQ, DT_FSL_IMX7D_I2C_I2C_2_IRQ_PRIORITY,
			i2c_imx_isr, DEVICE_GET(i2c_imx_2), 0);

	irq_enable(DT_FSL_IMX7D_I2C_I2C_2_IRQ);
}
#endif /* CONFIG_I2C_2 */

#ifdef CONFIG_I2C_3
static void i2c_imx_config_func_3(struct device *dev);

static const struct i2c_imx_config i2c_imx_config_3 = {
	.base = (I2C_Type *)DT_FSL_IMX7D_I2C_I2C_3_BASE_ADDRESS,
	.irq_config_func = i2c_imx_config_func_3,
	.bitrate = DT_FSL_IMX7D_I2C_I2C_3_CLOCK_FREQUENCY,
};

static struct i2c_imx_data i2c_imx_data_3;

DEVICE_AND_API_INIT(i2c_imx_3, DT_FSL_IMX7D_I2C_I2C_3_LABEL, &i2c_imx_init,
			&i2c_imx_data_3, &i2c_imx_config_3,
			POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEVICE,
			&i2c_imx_driver_api);

static void i2c_imx_config_func_3(struct device *dev)
{
	ARG_UNUSED(dev);

	IRQ_CONNECT(DT_FSL_IMX7D_I2C_I2C_3_IRQ, DT_FSL_IMX7D_I2C_I2C_3_IRQ_PRIORITY,
			i2c_imx_isr, DEVICE_GET(i2c_imx_3), 0);

	irq_enable(DT_FSL_IMX7D_I2C_I2C_3_IRQ);
}
#endif /* CONFIG_I2C_3 */

#ifdef CONFIG_I2C_4
static void i2c_imx_config_func_4(struct device *dev);

static const struct i2c_imx_config i2c_imx_config_4 = {
	.base = (I2C_Type *)DT_FSL_IMX7D_I2C_I2C_4_BASE_ADDRESS,
	.irq_config_func = i2c_imx_config_func_4,
	.bitrate = DT_FSL_IMX7D_I2C_I2C_4_CLOCK_FREQUENCY,
};

static struct i2c_imx_data i2c_imx_data_4;

DEVICE_AND_API_INIT(i2c_imx_4, DT_FSL_IMX7D_I2C_I2C_4_LABEL, &i2c_imx_init,
			&i2c_imx_data_4, &i2c_imx_config_4,
			POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEVICE,
			&i2c_imx_driver_api);

static void i2c_imx_config_func_4(struct device *dev)
{
	ARG_UNUSED(dev);
	IRQ_CONNECT(DT_FSL_IMX7D_I2C_I2C_4_IRQ, DT_FSL_IMX7D_I2C_I2C_4_IRQ_PRIORITY,
			i2c_imx_isr, DEVICE_GET(i2c_imx_4), 0);

	irq_enable(DT_FSL_IMX7D_I2C_I2C_4_IRQ);
}
#endif /* CONFIG_I2C_4 */
