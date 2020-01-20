/*
 * Copyright (c) 2019 Thomas Schmid <tom@lfence.de>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __SENSOR_MS5607_H__
#define __SENSOR_MS5607_H__

#include <zephyr/types.h>
#include <device.h>

#define MS5607_CMD_RESET 0x1E
#define MS5607_CMD_CONV_P_256 0x40
#define MS5607_CMD_CONV_P_512 0x42
#define MS5607_CMD_CONV_P_1024 0x44
#define MS5607_CMD_CONV_P_2048 0x46
#define MS5607_CMD_CONV_P_4096 0x48

#define MS5607_CMD_CONV_T_256 0x50
#define MS5607_CMD_CONV_T_512 0x52
#define MS5607_CMD_CONV_T_1024 0x54
#define MS5607_CMD_CONV_T_2048 0x56
#define MS5607_CMD_CONV_T_4096 0x58

#define MS5607_CMD_CONV_READ_ADC 0x00

#define MS5607_CMD_CONV_READ_SENSE_T1 0xA2
#define MS5607_CMD_CONV_READ_OFF_T1 0xA4
#define MS5607_CMD_CONV_READ_TCS 0xA6
#define MS5607_CMD_CONV_READ_TCO 0xA8
#define MS5607_CMD_CONV_READ_T_REF 0xAA
#define MS5607_CMD_CONV_READ_TEMPSENS 0xAC
#define MS5607_CMD_CONV_READ_CRC 0xAE

#if defined(CONFIG_MS5607_PRES_OVER_256X)
	#define MS5607_PRES_OVER_DEFAULT 256
#elif defined(CONFIG_MS5607_PRES_OVER_512X)
	#define MS5607_PRES_OVER_DEFAULT 512
#elif defined(CONFIG_MS5607_PRES_OVER_1024X)
	#define MS5607_PRES_OVER_DEFAULT 1024
#elif defined(CONFIG_MS5607_PRES_OVER_2048X)
	#define MS5607_PRES_OVER_DEFAULT 2048
#elif defined(CONFIG_MS5607_PRES_OVER_4096X)
	#define MS5607_PRES_OVER_DEFAULT 4096
#else
	#define MS5607_PRES_OVER_DEFAULT 2048
#endif

#if defined(CONFIG_MS5607_TEMP_OVER_256X)
	#define MS5607_TEMP_OVER_DEFAULT 256
#elif defined(CONFIG_MS5607_TEMP_OVER_512X)
	#define MS5607_TEMP_OVER_DEFAULT 512
#elif defined(CONFIG_MS5607_TEMP_OVER_1024X)
	#define MS5607_TEMP_OVER_DEFAULT 1024
#elif defined(CONFIG_MS5607_TEMP_OVER_2048X)
	#define MS5607_TEMP_OVER_DEFAULT 2048
#elif defined(CONFIG_MS5607_TEMP_OVER_4096X)
	#define MS5607_TEMP_OVER_DEFAULT 4096
#else
	#define MS5607_TEMP_OVER_DEFAULT 2048
#endif

#ifdef DT_MEAS_MS5607_BUS_SPI
int ms5607_spi_init(struct device *dev);
#else
/* I2c Interface not implemented yet */
BUILD_ASSERT_MSG(1, "I2c interface not implemented yet");
#endif

struct ms5607_config {
	char *ms5607_device_name;
};

struct ms5607_data {
	struct device *ms5607_device;
	const struct ms5607_transfer_function *tf;
	/* Calibration values */
	u16_t sens_t1;
	u16_t off_t1;
	u16_t tcs;
	u16_t tco;
	u16_t t_ref;
	u16_t tempsens;

	/* Measured values */
	s32_t pressure;
	s32_t temperature;

	/* conversion commands */
	u8_t pressure_conv_cmd;
	u8_t temperature_conv_cmd;

	u8_t pressure_conv_delay;
	u8_t temperature_conv_delay;
};

struct ms5607_transfer_function {
	int (*reset)(const struct ms5607_data *data);
	int (*read_prom)(const struct ms5607_data *data, u8_t cmd, u16_t *val);
	int (*start_conversion)(const struct ms5607_data *data, u8_t cmd);
	int (*read_adc)(const struct ms5607_data *data, u32_t *val);
};

#endif /* __SENSOR_MS607_H__*/
