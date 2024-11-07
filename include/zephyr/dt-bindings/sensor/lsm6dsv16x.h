/*
 * Copyright (c) 2023 STMicroelectronics
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef ZEPHYR_INCLUDE_DT_BINDINGS_ST_LSM6DSV16X_H_
#define ZEPHYR_INCLUDE_DT_BINDINGS_ST_LSM6DSV16X_H_

/* Accel range */
#define	LSM6DSV16X_DT_FS_2G			0
#define	LSM6DSV16X_DT_FS_4G			1
#define	LSM6DSV16X_DT_FS_8G			2
#define	LSM6DSV16X_DT_FS_16G			3

/* Gyro range */
#define	LSM6DSV16X_DT_FS_125DPS			0x0
#define	LSM6DSV16X_DT_FS_250DPS			0x1
#define	LSM6DSV16X_DT_FS_500DPS			0x2
#define	LSM6DSV16X_DT_FS_1000DPS		0x3
#define	LSM6DSV16X_DT_FS_2000DPS		0x4
#define	LSM6DSV16X_DT_FS_4000DPS		0xc

/* Accel and Gyro Data rates */
#define LSM6DSV16X_DT_ODR_OFF			0x0
#define LSM6DSV16X_DT_ODR_AT_1Hz875		0x1
#define LSM6DSV16X_DT_ODR_AT_7Hz5		0x2
#define LSM6DSV16X_DT_ODR_AT_15Hz		0x3
#define LSM6DSV16X_DT_ODR_AT_30Hz		0x4
#define LSM6DSV16X_DT_ODR_AT_60Hz		0x5
#define LSM6DSV16X_DT_ODR_AT_120Hz		0x6
#define LSM6DSV16X_DT_ODR_AT_240Hz		0x7
#define LSM6DSV16X_DT_ODR_AT_480Hz		0x8
#define LSM6DSV16X_DT_ODR_AT_960Hz		0x9
#define LSM6DSV16X_DT_ODR_AT_1920Hz		0xA
#define LSM6DSV16X_DT_ODR_AT_3840Hz		0xB
#define LSM6DSV16X_DT_ODR_AT_7680Hz		0xC
#define LSM6DSV16X_DT_ODR_HA01_AT_15Hz625	0x13
#define LSM6DSV16X_DT_ODR_HA01_AT_31Hz25	0x14
#define LSM6DSV16X_DT_ODR_HA01_AT_62Hz5		0x15
#define LSM6DSV16X_DT_ODR_HA01_AT_125Hz		0x16
#define LSM6DSV16X_DT_ODR_HA01_AT_250Hz		0x17
#define LSM6DSV16X_DT_ODR_HA01_AT_500Hz		0x18
#define LSM6DSV16X_DT_ODR_HA01_AT_1000Hz	0x19
#define LSM6DSV16X_DT_ODR_HA01_AT_2000Hz	0x1A
#define LSM6DSV16X_DT_ODR_HA01_AT_4000Hz	0x1B
#define LSM6DSV16X_DT_ODR_HA01_AT_8000Hz	0x1C
#define LSM6DSV16X_DT_ODR_HA02_AT_12Hz5		0x23
#define LSM6DSV16X_DT_ODR_HA02_AT_25Hz		0x24
#define LSM6DSV16X_DT_ODR_HA02_AT_50Hz		0x25
#define LSM6DSV16X_DT_ODR_HA02_AT_100Hz		0x26
#define LSM6DSV16X_DT_ODR_HA02_AT_200Hz		0x27
#define LSM6DSV16X_DT_ODR_HA02_AT_400Hz		0x28
#define LSM6DSV16X_DT_ODR_HA02_AT_800Hz		0x29
#define LSM6DSV16X_DT_ODR_HA02_AT_1600Hz	0x2A
#define LSM6DSV16X_DT_ODR_HA02_AT_3200Hz	0x2B
#define LSM6DSV16X_DT_ODR_HA02_AT_6400Hz	0x2C

/* Accelerometer batching rates */
#define LSM6DSV16X_DT_XL_NOT_BATCHED		0x0
#define LSM6DSV16X_DT_XL_BATCHED_AT_1Hz875	0x1
#define LSM6DSV16X_DT_XL_BATCHED_AT_7Hz5	0x2
#define LSM6DSV16X_DT_XL_BATCHED_AT_15Hz	0x3
#define LSM6DSV16X_DT_XL_BATCHED_AT_30Hz	0x4
#define LSM6DSV16X_DT_XL_BATCHED_AT_60Hz	0x5
#define LSM6DSV16X_DT_XL_BATCHED_AT_120Hz	0x6
#define LSM6DSV16X_DT_XL_BATCHED_AT_240Hz	0x7
#define LSM6DSV16X_DT_XL_BATCHED_AT_480Hz	0x8
#define LSM6DSV16X_DT_XL_BATCHED_AT_960Hz	0x9
#define LSM6DSV16X_DT_XL_BATCHED_AT_1920Hz	0xa
#define LSM6DSV16X_DT_XL_BATCHED_AT_3840Hz	0xb
#define LSM6DSV16X_DT_XL_BATCHED_AT_7680Hz	0xc

/* Gyroscope batching rates */
#define LSM6DSV16X_DT_GY_NOT_BATCHED		0x0
#define LSM6DSV16X_DT_GY_BATCHED_AT_1Hz875	0x1
#define LSM6DSV16X_DT_GY_BATCHED_AT_7Hz5	0x2
#define LSM6DSV16X_DT_GY_BATCHED_AT_15Hz	0x3
#define LSM6DSV16X_DT_GY_BATCHED_AT_30Hz	0x4
#define LSM6DSV16X_DT_GY_BATCHED_AT_60Hz	0x5
#define LSM6DSV16X_DT_GY_BATCHED_AT_120Hz	0x6
#define LSM6DSV16X_DT_GY_BATCHED_AT_240Hz	0x7
#define LSM6DSV16X_DT_GY_BATCHED_AT_480Hz	0x8
#define LSM6DSV16X_DT_GY_BATCHED_AT_960Hz	0x9
#define LSM6DSV16X_DT_GY_BATCHED_AT_1920Hz	0xa
#define LSM6DSV16X_DT_GY_BATCHED_AT_3840Hz	0xb
#define LSM6DSV16X_DT_GY_BATCHED_AT_7680Hz	0xc

/* Temperature sensor batching rates */
#define LSM6DSV16X_DT_TEMP_NOT_BATCHED		0x0
#define LSM6DSV16X_DT_TEMP_BATCHED_AT_1Hz875	0x1
#define LSM6DSV16X_DT_TEMP_BATCHED_AT_15Hz	0x2
#define LSM6DSV16X_DT_TEMP_BATCHED_AT_60Hz	0x3

#endif /* ZEPHYR_INCLUDE_DT_BINDINGS_ST_LSM6DSV16X_H_ */
