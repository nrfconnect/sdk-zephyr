/*
 * Copyright (c) 2021, Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdint.h>


#define I2C_READ_FLAG			BIT(7)

#define AK8963_I2C_ADDR			0x0C

#define AK8963_REG_ID			0x00
#define AK8963_REG_ID_VAL		0x48

#define AK8963_REG_DATA			0x03

#define AK8963_ST2_OVRFL_BIT		BIT(3)

#define AK8963_REG_CNTL1			0x0A
#define AK8963_REG_CNTL1_POWERDOWN_VAL	0x00
#define AK8963_REG_CNTL1_FUSE_ROM_VAL		0x0F
#define AK8963_REG_CNTL1_16BIT_100HZ_VAL	0x16

#define AK8963_REG_CNTL2			0x0B
#define AK8963_REG_CNTL2_RESET_VAL		0x01

#define AK8963_REG_ADJ_DATA_X			0x10
#define AK8963_REG_ADJ_DATA_Y			0x11
#define AK8963_REG_ADJ_DATA_Z			0x12

#define AK9863_SCALE_TO_UG			1499

#define MPU9250_REG_I2C_MST_CTRL			0x24
#define MPU9250_REG_I2C_MST_CTRL_WAIT_MAG_400KHZ_VAL	0x4D

#define MPU9250_REG_I2C_SLV0_ADDR			0x25
#define MPU9250_REG_I2C_SLV0_REG			0x26
#define MPU9250_REG_I2C_SLV0_CTRL			0x27
#define MPU9250_REG_I2C_SLV0_DATA0			0x63
#define MPU9250_REG_READOUT_CTRL_VAL			(BIT(7) | 0x07)

#define MPU9250_REG_USER_CTRL				0x6A
#define MPU9250_REG_USER_CTRL_I2C_MASTERMODE_VAL	0x20

#define MPU9250_REG_EXT_DATA00				0x49

#define MPU9250_REG_I2C_SLV4_ADDR			0x31
#define MPU9250_REG_I2C_SLV4_REG			0x32
#define MPU9250_REG_I2C_SLV4_DO				0x33
#define MPU9250_REG_I2C_SLV4_CTRL			0x34
#define MPU9250_REG_I2C_SLV4_CTRL_VAL			0x80
#define MPU9250_REG_I2C_SLV4_DI				0x35

#define MPU9250_I2C_MST_STS				0x36
#define MPU9250_I2C_MST_STS_SLV4_DONE			BIT(6)


int ak8963_convert_magn(struct sensor_value *val, int16_t raw_val,
			 int16_t scale, uint8_t st2);

int ak8963_init(const struct device *dev);
