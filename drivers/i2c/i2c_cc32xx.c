/*
 * Copyright (c) 2017, Texas Instruments Incorporated
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* The logic here is adapted from SimpleLink SDK's I2CCC32XX.c module. */

#include <kernel.h>
#include <errno.h>
#include <i2c.h>
#include <soc.h>

/* Driverlib includes */
#include <inc/hw_memmap.h>
#include <inc/hw_common_reg.h>
#include <driverlib/rom.h>
#include <driverlib/rom_map.h>
#include <driverlib/i2c.h>

#define LOG_LEVEL CONFIG_I2C_LOG_LEVEL
#include <logging/log.h>
LOG_MODULE_REGISTER(i2c_cc32xx);

#include "i2c-priv.h"

#define I2C_MASTER_CMD_BURST_RECEIVE_START_NACK	 I2C_MASTER_CMD_BURST_SEND_START
#define I2C_MASTER_CMD_BURST_RECEIVE_STOP \
	I2C_MASTER_CMD_BURST_RECEIVE_ERROR_STOP
#define I2C_MASTER_CMD_BURST_RECEIVE_CONT_NACK	 I2C_MASTER_CMD_BURST_SEND_CONT

#define I2C_SEM_MASK \
	COMMON_REG_I2C_Properties_Register_I2C_Properties_Register_M
#define I2C_SEM_TAKE \
	COMMON_REG_I2C_Properties_Register_I2C_Properties_Register_S

#define IS_I2C_MSG_WRITE(flags) ((flags & I2C_MSG_RW_MASK) == I2C_MSG_WRITE)

#define DEV_CFG(dev) \
	((const struct i2c_cc32xx_config *const)(dev)->config->config_info)
#define DEV_DATA(dev) \
	((struct i2c_cc32xx_data *const)(dev)->driver_data)
#define DEV_BASE(dev) \
	((DEV_CFG(dev))->base)


/* Since this driver does not explicitly enable the TX/RX FIFOs, there
 * are no interrupts received which can distinguish between read and write
 * completion.
 * So, we need the READ and WRITE state flags to determine whether the
 * completed transmission was started as a write or a read.
 * The ERROR flag is used to convey error status from the ISR back to the
 * I2C API without having to re-read I2C registers.
 */
enum i2c_cc32xx_state {
	/* I2C was primed for a write operation */
	I2C_CC32XX_WRITE_MODE,
	/* I2C was primed for a read operation */
	I2C_CC32XX_READ_MODE,
	/* I2C error occurred */
	I2C_CC32XX_ERROR = 0xFF
};

struct i2c_cc32xx_config {
	u32_t base;
	u32_t bitrate;
	unsigned int irq_no;
};

struct i2c_cc32xx_data {
	struct k_sem mutex;
	struct k_sem transfer_complete;

	volatile enum i2c_cc32xx_state state;

	struct i2c_msg msg; /* Cache msg for transfer state machine */
	u16_t  slave_addr; /* Cache slave address for ISR use */
};

static void configure_i2c_irq(const struct i2c_cc32xx_config *config);

static int i2c_cc32xx_configure(struct device *dev, u32_t dev_config_raw)
{
	u32_t base = DEV_BASE(dev);
	u32_t bitrate_id;

	if (!(dev_config_raw & I2C_MODE_MASTER)) {
		return -EINVAL;
	}

	if (dev_config_raw & I2C_ADDR_10_BITS) {
		return -EINVAL;
	}

	switch (I2C_SPEED_GET(dev_config_raw)) {
	case I2C_SPEED_STANDARD:
		bitrate_id = 0;
		break;
	case I2C_SPEED_FAST:
		bitrate_id = 1;
		break;
	default:
		return -EINVAL;
	}

	MAP_I2CMasterInitExpClk(base, CONFIG_SYS_CLOCK_HW_CYCLES_PER_SEC,
				bitrate_id);

	return 0;
}

static void _i2c_cc32xx_prime_transfer(struct device *dev, struct i2c_msg *msg,
				      u16_t addr)
{
	struct i2c_cc32xx_data *data = DEV_DATA(dev);
	u32_t base = DEV_BASE(dev);

	/* Initialize internal counters and buf pointers: */
	data->msg = *msg;
	data->slave_addr = addr;

	/* Start transfer in Transmit mode */
	if (IS_I2C_MSG_WRITE(data->msg.flags)) {

		/* Specify the I2C slave address */
		MAP_I2CMasterSlaveAddrSet(base, addr, false);

		/* Update the I2C state */
		data->state = I2C_CC32XX_WRITE_MODE;

		/* Write data contents into data register */
		MAP_I2CMasterDataPut(base, *((data->msg.buf)++));

		/* Start the I2C transfer in master transmit mode */
		MAP_I2CMasterControl(base, I2C_MASTER_CMD_BURST_SEND_START);

	} else {
		/* Start transfer in Receive mode */
		/* Specify the I2C slave address */
		MAP_I2CMasterSlaveAddrSet(base, addr, true);

		/* Update the I2C mode */
		data->state = I2C_CC32XX_READ_MODE;

		if (data->msg.len < 2) {
			/* Start the I2C transfer in master receive mode */
			MAP_I2CMasterControl(base,
				       I2C_MASTER_CMD_BURST_RECEIVE_START_NACK);
		} else {
			/* Start the I2C transfer in burst receive mode */
			MAP_I2CMasterControl(base,
					    I2C_MASTER_CMD_BURST_RECEIVE_START);
		}
	}
}

static int i2c_cc32xx_transfer(struct device *dev, struct i2c_msg *msgs,
			       u8_t num_msgs, u16_t addr)
{
	struct i2c_cc32xx_data *data = DEV_DATA(dev);
	int retval = 0;

	/* Acquire the driver mutex */
	k_sem_take(&data->mutex, K_FOREVER);

	/* Iterate over all the messages */
	for (int i = 0; i < num_msgs; i++) {

		/* Begin the transfer */
		_i2c_cc32xx_prime_transfer(dev, msgs, addr);

		/* Wait for the transfer to complete */
		k_sem_take(&data->transfer_complete, K_FOREVER);

		/* Return an error if the transfer didn't complete */
		if (data->state == I2C_CC32XX_ERROR) {
			retval = -EIO;
			break;
		}

		/* Move to the next message */
		msgs++;
	}

	/* Release the mutex */
	k_sem_give(&data->mutex);

	return retval;
}

static void _i2c_cc32xx_isr_handle_write(u32_t base,
					 struct i2c_cc32xx_data *data)
{
	/* Decrement write Counter */
	data->msg.len--;

	/* Check if more data needs to be sent */
	if (data->msg.len) {

		/* Write data contents into data register */
		MAP_I2CMasterDataPut(base, *(data->msg.buf));
		data->msg.buf++;

		if (data->msg.len < 2) {
			/* Everything has been sent, nothing to receive */
			/* Send last byte with STOP bit */
			MAP_I2CMasterControl(base,
					     I2C_MASTER_CMD_BURST_SEND_FINISH);
		} else {
			/*
			 * Either there is more data to be transmitted or some
			 * data needs to be received next
			 */
			MAP_I2CMasterControl(base,
					     I2C_MASTER_CMD_BURST_SEND_CONT);

		}
	} else {
		/*
		 * No more data needs to be sent, so follow up with
		 * a STOP bit.
		 */
		MAP_I2CMasterControl(base,
				     I2C_MASTER_CMD_BURST_RECEIVE_STOP);
	}
}

static void _i2c_cc32xx_isr_handle_read(u32_t base,
					struct i2c_cc32xx_data *data)
{

	/* Save the received data */
	*(data->msg.buf) = MAP_I2CMasterDataGet(base);
	data->msg.buf++;

	/* Check if any data needs to be received */
	data->msg.len--;
	if (data->msg.len) {
		if (data->msg.len > 1) {
			/* More data to be received */
			MAP_I2CMasterControl(base,
					     I2C_MASTER_CMD_BURST_RECEIVE_CONT);
		} else {
			/*
			 * Send NACK because it's the last byte to be received
			 */
			MAP_I2CMasterControl(base,
				       I2C_MASTER_CMD_BURST_RECEIVE_CONT_NACK);
		}
	} else {
		/*
		 * No more data needs to be received, so follow up with a
		 * STOP bit
		 */
		MAP_I2CMasterControl(base,
				     I2C_MASTER_CMD_BURST_RECEIVE_STOP);
	}
}

static void i2c_cc32xx_isr(void *arg)
{
	struct device *dev = (struct device *)arg;
	u32_t base = DEV_BASE(dev);
	struct i2c_cc32xx_data *data = DEV_DATA(dev);
	u32_t err_status;
	u32_t int_status;

	/* Get the error  status of the I2C controller */
	err_status = MAP_I2CMasterErr(base);

	/* Get interrupt cause (from I2CMRIS (raw interrupt) reg): */
	int_status = MAP_I2CMasterIntStatusEx(base, 0);

	/* Clear interrupt source to avoid additional interrupts */
	MAP_I2CMasterIntClearEx(base, int_status);

	LOG_DBG("primed state: %d; err_status: 0x%x; int_status: 0x%x",
		    data->state, err_status, int_status);

	/* Handle errors: */
	if ((err_status != I2C_MASTER_ERR_NONE) ||
	    (int_status &
	     (I2C_MASTER_INT_ARB_LOST | I2C_MASTER_INT_TIMEOUT))) {

		/* Set so API can report I/O error: */
		data->state = I2C_CC32XX_ERROR;

		if (!(err_status & (I2C_MASTER_ERR_ARB_LOST |
				    I2C_MASTER_ERR_ADDR_ACK))) {
			/* Send a STOP bit to end I2C communications */
			/*
			 * I2C_MASTER_CMD_BURST_SEND_ERROR_STOP -and-
			 * I2C_MASTER_CMD_BURST_RECEIVE_ERROR_STOP
			 * have the same values
			 */
			MAP_I2CMasterControl(base,
					  I2C_MASTER_CMD_BURST_SEND_ERROR_STOP);
		}
		/* Indicate transfer complete */
		k_sem_give(&data->transfer_complete);

	/* Handle Stop: */
	} else if (int_status & I2C_MASTER_INT_STOP) {
		/* Indicate transfer complete */
		k_sem_give(&data->transfer_complete);

	/* Handle (read or write) transmit complete: */
	} else if (int_status & (I2C_MASTER_INT_DATA | I2C_MASTER_INT_START)) {
		if (data->state == I2C_CC32XX_WRITE_MODE) {
			_i2c_cc32xx_isr_handle_write(base, data);
		}
		if (data->state == I2C_CC32XX_READ_MODE) {
			_i2c_cc32xx_isr_handle_read(base, data);
		}
	/* Some unanticipated H/W state: */
	} else {
		__ASSERT(1, "Unanticipated I2C Interrupt!");
		data->state = I2C_CC32XX_ERROR;
		k_sem_give(&data->transfer_complete);
	}
}

static int i2c_cc32xx_init(struct device *dev)
{
	u32_t base = DEV_BASE(dev);
	const struct i2c_cc32xx_config *config = DEV_CFG(dev);
	struct i2c_cc32xx_data *data = DEV_DATA(dev);
	u32_t bitrate_cfg;
	int error;
	u32_t regval;

	k_sem_init(&data->mutex, 1, UINT_MAX);
	k_sem_init(&data->transfer_complete, 0, UINT_MAX);

	/* In case of app restart: disable I2C module, clear NVIC interrupt */
	/* Note: this was done *during* pinmux setup in SimpleLink SDK. */
	MAP_I2CMasterDisable(base);

	/* Clear exception INT_I2CA0 */
	MAP_IntPendClear((unsigned long)(config->irq_no + 16));

	configure_i2c_irq(config);

	/* Take I2C hardware semaphore. */
	regval = HWREG(COMMON_REG_BASE);
	regval = (regval & ~I2C_SEM_MASK) | (0x01 << I2C_SEM_TAKE);
	HWREG(COMMON_REG_BASE) = regval;

	/* Set to default configuration: */
	bitrate_cfg = _i2c_map_dt_bitrate(config->bitrate);
	error = i2c_cc32xx_configure(dev, I2C_MODE_MASTER | bitrate_cfg);
	if (error) {
		return error;
	}

	/* Clear any pending interrupts */
	MAP_I2CMasterIntClear(base);

	/* Enable the I2C Master for operation */
	MAP_I2CMasterEnable(base);

	/* Unmask I2C interrupts */
	MAP_I2CMasterIntEnable(base);

	return 0;
}

static const struct i2c_driver_api i2c_cc32xx_driver_api = {
	.configure = i2c_cc32xx_configure,
	.transfer = i2c_cc32xx_transfer,
};


static const struct i2c_cc32xx_config i2c_cc32xx_config = {
	.base = DT_I2C_0_BASE_ADDRESS,
	.bitrate = DT_I2C_0_BITRATE,
	.irq_no = DT_I2C_0_IRQ,
};

static struct i2c_cc32xx_data i2c_cc32xx_data;

DEVICE_AND_API_INIT(i2c_cc32xx, DT_I2C_0_LABEL, &i2c_cc32xx_init,
		    &i2c_cc32xx_data, &i2c_cc32xx_config,
		    POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEVICE,
		    &i2c_cc32xx_driver_api);

static void configure_i2c_irq(const struct i2c_cc32xx_config *config)
{
	IRQ_CONNECT(DT_I2C_0_IRQ,
		    DT_I2C_0_IRQ_PRIORITY,
		    i2c_cc32xx_isr, DEVICE_GET(i2c_cc32xx), 0);

	irq_enable(config->irq_no);
}
