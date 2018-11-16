/*
 * Copyright (c) 2015 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <zephyr.h>
#include <misc/printk.h>
#include <device.h>
#include <i2c.h>

#if defined(CONFIG_SOC_QUARK_SE_C1000_SS)
#define I2C_DEV DT_I2C_SS_0_NAME
#else
#define I2C_DEV CONFIG_I2C_0_NAME
#endif

/**
 * @file Sample app using the Fujitsu MB85RC256V FRAM through ARC I2C.
 */

#define FRAM_I2C_ADDR	0x50

static int write_bytes(struct device *i2c_dev, u16_t addr,
		       u8_t *data, u32_t num_bytes)
{
	u8_t wr_addr[2];
	struct i2c_msg msgs[2];

	/* FRAM address */
	wr_addr[0] = (addr >> 8) & 0xFF;
	wr_addr[1] = addr & 0xFF;

	/* Setup I2C messages */

	/* Send the address to write to */
	msgs[0].buf = wr_addr;
	msgs[0].len = 2;
	msgs[0].flags = I2C_MSG_WRITE;

	/* Data to be written, and STOP after this. */
	msgs[1].buf = data;
	msgs[1].len = num_bytes;
	msgs[1].flags = I2C_MSG_WRITE | I2C_MSG_STOP;

	return i2c_transfer(i2c_dev, &msgs[0], 2, FRAM_I2C_ADDR);
}

static int read_bytes(struct device *i2c_dev, u16_t addr,
		      u8_t *data, u32_t num_bytes)
{
	u8_t wr_addr[2];
	struct i2c_msg msgs[2];

	/* Now try to read back from FRAM */

	/* FRAM address */
	wr_addr[0] = (addr >> 8) & 0xFF;
	wr_addr[1] = addr & 0xFF;

	/* Setup I2C messages */

	/* Send the address to read from */
	msgs[0].buf = wr_addr;
	msgs[0].len = 2;
	msgs[0].flags = I2C_MSG_WRITE;

	/* Read from device. STOP after this. */
	msgs[1].buf = data;
	msgs[1].len = num_bytes;
	msgs[1].flags = I2C_MSG_READ | I2C_MSG_STOP;

	return i2c_transfer(i2c_dev, &msgs[0], 2, FRAM_I2C_ADDR);
}

void main(void)
{
	struct device *i2c_dev;
	u8_t cmp_data[16];
	u8_t data[16];
	int i, ret;

	i2c_dev = device_get_binding(I2C_DEV);
	if (!i2c_dev) {
		printk("I2C: Device driver not found.\n");
		return;
	}

	/* Do one-byte read/write */

	data[0] = 0xAE;
	ret = write_bytes(i2c_dev, 0x00, &data[0], 1);
	if (ret) {
		printk("Error writing to FRAM! error code (%d)\n", ret);
		return;
	} else {
		printk("Wrote 0xAE to address 0x00.\n");
	}

	data[0] = 0x86;
	ret = write_bytes(i2c_dev, 0x01, &data[0], 1);
	if (ret) {
		printk("Error writing to FRAM! error code (%d)\n", ret);
		return;
	} else {
		printk("Wrote 0x86 to address 0x01.\n");
	}

	data[0] = 0x00;
	ret = read_bytes(i2c_dev, 0x00, &data[0], 1);
	if (ret) {
		printk("Error reading from FRAM! error code (%d)\n", ret);
		return;
	} else {
		printk("Read 0x%X from address 0x00.\n", data[0]);
	}

	data[1] = 0x00;
	ret = read_bytes(i2c_dev, 0x01, &data[0], 1);
	if (ret) {
		printk("Error reading from FRAM! error code (%d)\n", ret);
		return;
	} else {
		printk("Read 0x%X from address 0x01.\n", data[0]);
	}

	/* Do multi-byte read/write */

	/* get some random data, and clear out data[] */
	for (i = 0; i < sizeof(cmp_data); i++) {
		cmp_data[i] = k_cycle_get_32() & 0xFF;
		data[i] = 0x00;
	}

	/* write them to the FRAM */
	ret = write_bytes(i2c_dev, 0x00, cmp_data, sizeof(cmp_data));
	if (ret) {
		printk("Error writing to FRAM! error code (%d)\n", ret);
		return;
	} else {
		printk("Wrote %zu bytes to address 0x00.\n", sizeof(cmp_data));
	}

	ret = read_bytes(i2c_dev, 0x00, data, sizeof(data));
	if (ret) {
		printk("Error reading from FRAM! error code (%d)\n", ret);
		return;
	} else {
		printk("Read %zu bytes from address 0x00.\n", sizeof(data));
	}

	ret = 0;
	for (i = 0; i < sizeof(cmp_data); i++) {
		/* uncomment below if you want to see all the bytes */
		/* printk("0x%X ?= 0x%X\n", cmp_data[i], data[i]); */
		if (cmp_data[i] != data[i]) {
			printk("Data comparison failed @ %d.\n", i);
			ret = -EIO;
		}
	}
	if (ret == 0) {
		printk("Data comparison successful.\n");
	}
}
