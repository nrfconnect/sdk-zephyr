/*
 * Copyright (c) 2018 Peter Bigot Consulting, LLC
 * Copyright (c) 2018 Linaro Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_DRIVERS_SENSOR_CCS811_CCS811_H_
#define ZEPHYR_DRIVERS_SENSOR_CCS811_CCS811_H_

#include <device.h>
#include <drivers/gpio.h>
#include <sys/util.h>
#include <drivers/sensor/ccs811.h>

/* Registers */
#define CCS811_REG_STATUS               0x00
#define CCS811_REG_MEAS_MODE            0x01
#define CCS811_REG_ALG_RESULT_DATA      0x02
#define CCS811_REG_RAW_DATA             0x03
#define CCS811_REG_ENV_DATA             0x05
#define CCS811_REG_THRESHOLDS           0x10
#define CCS811_REG_BASELINE             0x11
#define CCS811_REG_HW_ID                0x20
#define CCS811_REG_HW_VERSION           0x21
#define CCS811_REG_FW_BOOT_VERSION      0x23
#define CCS811_REG_FW_APP_VERSION       0x24
#define CCS811_REG_ERROR_ID             0xE0
#define CCS811_REG_APP_START            0xF4

#define CCS881_HW_ID                    0x81
#define CCS811_HW_VERSION_MSK           0xF0

/* Measurement modes */
#define CCS811_MODE_RAW_DATA            0x40
#define CCS811_MODE_DATARDY             0x08
#define CCS811_MODE_THRESH              0x04

#define CCS811_RAW_VOLTAGE_POS          0
#define CCS811_RAW_VOLTAGE_MSK          (0x3FF << CCS811_RAW_VOLTAGE_POS)
#define CCS811_RAW_VOLTAGE_SCALE        (1650000U / (CCS811_RAW_VOLTAGE_MSK \
						     >> CCS811_RAW_VOLTAGE_POS))
#define CCS811_RAW_CURRENT_POS          10
#define CCS811_RAW_CURRENT_MSK          (0x3F << CCS811_RAW_CURRENT_POS)
#define CCS811_RAW_CURRENT_SCALE        1

#define CCS811_CO2_MIN_PPM              400
#define CCS811_CO2_MAX_PPM              32767

struct ccs811_data {
	struct device *i2c;
#ifdef DT_INST_0_AMS_CCS811_IRQ_GPIOS_CONTROLLER
	struct device *int_gpio;
#ifdef CONFIG_CCS811_TRIGGER
	/*
	 * DATARDY is configured through SENSOR_CHAN_ALL.
	 * THRESH would be configured through SENSOR_CHAN_CO2.
	 */
	struct gpio_callback gpio_cb;
	sensor_trigger_handler_t handler;
	struct sensor_trigger trigger;
#if defined(CONFIG_CCS811_TRIGGER_OWN_THREAD)
	K_THREAD_STACK_MEMBER(thread_stack, CONFIG_CCS811_THREAD_STACK_SIZE);
	struct k_sem gpio_sem;
	struct k_thread thread;
#elif defined(CONFIG_CCS811_TRIGGER_GLOBAL_THREAD)
	struct k_work work;
	struct device *dev;
#endif
	u16_t co2_l2m;
	u16_t co2_m2h;
#endif /* CONFIG_CCS811_TRIGGER */
#endif
#ifdef DT_INST_0_AMS_CCS811_RESET_GPIOS_CONTROLLER
	struct device *reset_gpio;
#endif
#ifdef DT_INST_0_AMS_CCS811_WAKE_GPIOS_CONTROLLER
	struct device *wake_gpio;
#endif
	struct ccs811_result_type result;
	u8_t mode;
	u8_t app_fw_ver;
};

#ifdef CONFIG_CCS811_TRIGGER

int ccs811_mutate_meas_mode(struct device *dev,
			    u8_t set,
			    u8_t clear);

int ccs811_set_thresholds(struct device *dev);

int ccs811_attr_set(struct device *dev,
		    enum sensor_channel chan,
		    enum sensor_attribute attr,
		    const struct sensor_value *val);

int ccs811_trigger_set(struct device *dev,
		       const struct sensor_trigger *trig,
		       sensor_trigger_handler_t handler);

int ccs811_init_interrupt(struct device *dev);

#endif  /* CONFIG_CCS811_TRIGGER */

#endif  /* _SENSOR_CCS811_ */
