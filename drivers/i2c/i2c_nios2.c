/*
 * Copyright (c) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <i2c.h>
#include <soc.h>
#include <misc/util.h>
#include <altera_common.h>
#include "altera_avalon_i2c.h"

#define SYS_LOG_LEVEL CONFIG_SYS_LOG_I2C_LEVEL
#include <logging/sys_log.h>

#define NIOS2_I2C_TIMEOUT_USEC		1000

#define DEV_CFG(dev) \
	((struct i2c_nios2_config *)(dev)->config->config_info)

struct i2c_nios2_config {
	ALT_AVALON_I2C_DEV_t	i2c_dev;
	IRQ_DATA_t		irq_data;
	struct k_sem		sem_lock;
};

static int i2c_nios2_configure(struct device *dev, u32_t dev_config)
{
	struct i2c_nios2_config *config = DEV_CFG(dev);
	s32_t rc = 0;

	k_sem_take(&config->sem_lock, K_FOREVER);
	if (!(I2C_MODE_MASTER & dev_config)) {
		SYS_LOG_ERR("i2c config mode error\n");
		rc = -EINVAL;
		goto i2c_cfg_err;
	}

	if (I2C_ADDR_10_BITS & dev_config) {
		SYS_LOG_ERR("i2c config addresing error\n");
		rc = -EINVAL;
		goto i2c_cfg_err;
	}

	if (I2C_SPEED_GET(dev_config) != I2C_SPEED_STANDARD) {
		SYS_LOG_ERR("i2c config speed error\n");
		rc = -EINVAL;
		goto i2c_cfg_err;
	}

	alt_avalon_i2c_init(&config->i2c_dev);

i2c_cfg_err:
	k_sem_give(&config->sem_lock);
	return rc;
}

static int i2c_nios2_transfer(struct device *dev, struct i2c_msg *msgs,
			      u8_t num_msgs, u16_t addr)
{
	struct i2c_nios2_config *config = DEV_CFG(dev);
	ALT_AVALON_I2C_STATUS_CODE status;
	u32_t restart, stop;
	s32_t i, timeout, rc = 0;

	k_sem_take(&config->sem_lock, K_FOREVER);
	/* register the optional interrupt callback */
	alt_avalon_i2c_register_optional_irq_handler(
			&config->i2c_dev, &config->irq_data);

	/* Iterate over all the messages */
	for (i = 0; i < num_msgs; i++) {

		/* convert restart flag */
		if (msgs->flags & I2C_MSG_RESTART) {
			restart = ALT_AVALON_I2C_RESTART;
		} else {
			restart = ALT_AVALON_I2C_NO_RESTART;
		}

		/* convert stop flag */
		if (msgs->flags & I2C_MSG_STOP) {
			stop = ALT_AVALON_I2C_STOP;
		} else {
			stop = ALT_AVALON_I2C_NO_STOP;
		}

		/* Set the slave device address */
		alt_avalon_i2c_master_target_set(&config->i2c_dev, addr);

		/* Start the transfer */
		if (msgs->flags & I2C_MSG_READ) {
			status = alt_avalon_i2c_master_receive_using_interrupts(
							&config->i2c_dev,
							msgs->buf, msgs->len,
							restart, stop);
		} else {
			status = alt_avalon_i2c_master_transmit_using_interrupts
							(&config->i2c_dev,
							msgs->buf, msgs->len,
							restart, stop);
		}

		/* Return an error if the transfer didn't
		 * start successfully e.g., if the bus was busy
		 */
		if (status != ALT_AVALON_I2C_SUCCESS) {
			SYS_LOG_ERR("i2c transfer error %lu\n", status);
			rc = -EIO;
			goto i2c_transfer_err;
		}

		timeout = NIOS2_I2C_TIMEOUT_USEC;
		while (timeout) {
			k_busy_wait(1);
			status = alt_avalon_i2c_interrupt_transaction_status(
							&config->i2c_dev);
			if (status == ALT_AVALON_I2C_SUCCESS) {
				break;
			}
			timeout--;
		}

		if (timeout <= 0) {
			SYS_LOG_ERR("i2c busy or timeout error %lu\n", status);
			rc = -EIO;
			goto i2c_transfer_err;
		}

		/* move to the next message */
		msgs++;
	}

i2c_transfer_err:
	alt_avalon_i2c_disable(&config->i2c_dev);
	k_sem_give(&config->sem_lock);
	return rc;
}

static void i2c_nios2_isr(void *arg)
{
	struct device *dev = (struct device *)arg;
	struct i2c_nios2_config *config = DEV_CFG(dev);

	/* Call Altera HAL driver ISR */
	alt_handle_irq(&config->i2c_dev, I2C_0_IRQ);
}

static int i2c_nios2_init(struct device *dev);

static struct i2c_driver_api i2c_nios2_driver_api = {
	.configure = i2c_nios2_configure,
	.transfer = i2c_nios2_transfer,
};

static struct i2c_nios2_config i2c_nios2_cfg = {
	.i2c_dev = {
		.i2c_base = (alt_u32 *)I2C_0_BASE,
		.irq_controller_ID = I2C_0_IRQ_INTERRUPT_CONTROLLER_ID,
		.irq_ID = I2C_0_IRQ,
		.ip_freq_in_hz = I2C_0_FREQ,
	},
};

DEVICE_AND_API_INIT(i2c_nios2_0, CONFIG_I2C_0_NAME, &i2c_nios2_init,
		    NULL, &i2c_nios2_cfg,
		    POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEVICE,
		    &i2c_nios2_driver_api);

static int i2c_nios2_init(struct device *dev)
{
	struct i2c_nios2_config *config = DEV_CFG(dev);
	int rc;

	/* initialize semaphore */
	k_sem_init(&config->sem_lock, 1, 1);

	rc = i2c_nios2_configure(dev,
			I2C_MODE_MASTER |
			I2C_SPEED_SET(I2C_SPEED_STANDARD));
	if (rc) {
		SYS_LOG_ERR("i2c configure failed %d\n", rc);
		return rc;
	}

	/* clear ISR register content */
	alt_avalon_i2c_int_clear(&config->i2c_dev,
			ALT_AVALON_I2C_ISR_ALL_CLEARABLE_INTS_MSK);
	IRQ_CONNECT(I2C_0_IRQ, CONFIG_I2C_0_IRQ_PRI,
			i2c_nios2_isr, DEVICE_GET(i2c_nios2_0), 0);
	irq_enable(I2C_0_IRQ);
	return 0;
}
