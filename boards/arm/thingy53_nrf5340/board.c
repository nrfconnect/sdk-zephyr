/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <init.h>
#include <hal/nrf_gpio.h>
#include <nrfx.h>
#include <device.h>
#include <drivers/gpio.h>

#include <logging/log.h>
#define LOG_LEVEL LOG_LEVEL_ERR
LOG_MODULE_REGISTER(thingy53_board_init, LOG_LEVEL);

#define NRF21540FEM_CTRL_LABEL		 DT_NODELABEL(nrf21540fem_ctrl)
#define NRF21540FEM_CTRL_RX_EN_PIN	 DT_GPIO_PIN(NRF21540FEM_CTRL_LABEL, rx_en_gpios)
#define NRF21540FEM_CTRL_MODE_PIN	 DT_GPIO_PIN(NRF21540FEM_CTRL_LABEL, mode_gpios)
#define NRF21540FEM_CTRL_TX_EN_PIN	 DT_GPIO_PIN(NRF21540FEM_CTRL_LABEL, tx_en_gpios)
#define NRF21540FEM_CTRL_PDN_GPIO_NODE   DT_GPIO_CTLR(NRF21540FEM_CTRL_LABEL, pdn_gpios)
#define NRF21540FEM_CTRL_PDN_PIN	 DT_GPIO_PIN(NRF21540FEM_CTRL_LABEL, pdn_gpios)
#define NRF21540FEM_CTRL_PDN_FLAGS	 DT_GPIO_FLAGS(NRF21540FEM_CTRL_LABEL, pdn_gpios)

#define REG_3V3_LABEL			 DT_NODELABEL(regulator_3v3)
#define REG_3V3_GPIO_NODE		 DT_GPIO_CTLR(REG_3V3_LABEL, enable_gpios)
#define REG_3V3_ENABLE_PIN		 DT_GPIO_PIN(REG_3V3_LABEL, enable_gpios)
#define REG_3V3_ENABLE_FLAGS		 DT_GPIO_FLAGS(REG_3V3_LABEL, enable_gpios)

#define SENSOR_PWR_CTRL_LABEL		 DT_NODELABEL(sensor_pwr_ctrl)
#define SENSOR_PWR_CTRL_GPIO_NODE	 DT_GPIO_CTLR(SENSOR_PWR_CTRL_LABEL, enable_gpios)
#define SENSOR_PWR_CTRL_PIN		 DT_GPIO_PIN(SENSOR_PWR_CTRL_LABEL, enable_gpios)
#define SENSOR_PWR_CTRL_FLAGS		 DT_GPIO_FLAGS(SENSOR_PWR_CTRL_LABEL, enable_gpios)

#define ADXL362_LABEL			 DT_NODELABEL(adxl362)
#define ADXL362_GPIO_NODE		 DT_SPI_DEV_CS_GPIOS_CTLR(ADXL362_LABEL)
#define ADXL362_CS			 DT_SPI_DEV_CS_GPIOS_PIN(ADXL362_LABEL)
#define ADXL362_FLAGS			 DT_SPI_DEV_CS_GPIOS_FLAGS(ADXL362_LABEL)

#define BMI270_LABEL			 DT_NODELABEL(bmi270)
#define BMI270_GPIO_NODE		 DT_SPI_DEV_CS_GPIOS_CTLR(BMI270_LABEL)
#define BMI270_CS			 DT_SPI_DEV_CS_GPIOS_PIN(BMI270_LABEL)
#define BMI270_FLAGS			 DT_SPI_DEV_CS_GPIOS_FLAGS(BMI270_LABEL)

#define NRF21540FEM_LABEL		 DT_NODELABEL(nrf21540fem)
#define NRF21540FEM_GPIO_NODE		 DT_SPI_DEV_CS_GPIOS_CTLR(NRF21540FEM_LABEL)
#define NRF21540FEM_CS			 DT_SPI_DEV_CS_GPIOS_PIN(NRF21540FEM_LABEL)
#define NRF21540FEM_FLAGS		 DT_SPI_DEV_CS_GPIOS_FLAGS(NRF21540FEM_LABEL)

static int setup(const struct device *dev)
{
	ARG_UNUSED(dev);

	const struct device *gpio;
	int err;

	gpio = DEVICE_DT_GET(REG_3V3_GPIO_NODE);
	if (!device_is_ready(gpio)) {
		LOG_ERR("Gpio device not ready");
		return -ENODEV;
	}
	err = gpio_pin_configure(gpio, REG_3V3_ENABLE_PIN,
				 REG_3V3_ENABLE_FLAGS | GPIO_OUTPUT_ACTIVE);
	if (err < 0) {
		LOG_ERR("Failed to configure pin %d", REG_3V3_ENABLE_PIN);
		return err;
	}

	gpio = DEVICE_DT_GET(SENSOR_PWR_CTRL_GPIO_NODE);
	if (!device_is_ready(gpio)) {
		LOG_ERR("Gpio device not ready");
		return -ENODEV;
	}
	err = gpio_pin_configure(gpio, SENSOR_PWR_CTRL_PIN,
				 SENSOR_PWR_CTRL_FLAGS | GPIO_OUTPUT_ACTIVE);
	if (err < 0) {
		LOG_ERR("Failed to configure pin %d", SENSOR_PWR_CTRL_PIN);
		return err;
	}

	gpio = DEVICE_DT_GET(NRF21540FEM_CTRL_PDN_GPIO_NODE);
	if (!device_is_ready(gpio)) {
		LOG_ERR("Gpio device not ready");
		return -ENODEV;
	}
	err = gpio_pin_configure(gpio, NRF21540FEM_CTRL_PDN_PIN,
				 NRF21540FEM_CTRL_PDN_FLAGS |
				 IS_ENABLED(CONFIG_THINGY53_MISO_WORKAROUND) ?
				 GPIO_OUTPUT_ACTIVE : GPIO_OUTPUT_INACTIVE);
	if (err < 0) {
		LOG_ERR("Failed to configure NRF21540FEM PDN Pin");
		return err;
	}

	gpio = DEVICE_DT_GET(ADXL362_GPIO_NODE);
	if (!device_is_ready(gpio)) {
		LOG_ERR("Gpio device not ready");
		return -ENODEV;
	}
	err = gpio_pin_configure(gpio, ADXL362_CS, ADXL362_FLAGS | GPIO_OUTPUT_INACTIVE);
	if (err < 0) {
		LOG_ERR("Failed to configure ADXL362 CS Pin");
		return err;
	}

	gpio = DEVICE_DT_GET(BMI270_GPIO_NODE);
	if (!device_is_ready(gpio)) {
		LOG_ERR("Gpio device not ready");
		return -ENODEV;
	}
	err = gpio_pin_configure(gpio, BMI270_CS, BMI270_FLAGS | GPIO_OUTPUT_INACTIVE);
	if (err < 0) {
		LOG_ERR("Failed to configure BMI270 CS Pin");
		return err;
	}

	gpio = DEVICE_DT_GET(NRF21540FEM_GPIO_NODE);
	if (!device_is_ready(gpio)) {
		LOG_ERR("Gpio device not ready");
		return -ENODEV;
	}
	err = gpio_pin_configure(gpio, NRF21540FEM_CS, NRF21540FEM_FLAGS | GPIO_OUTPUT_INACTIVE);
	if (err < 0) {
		LOG_ERR("Failed to configure NRF21540FEM CS Pin");
		return err;
	}

#if (CONFIG_BOARD_ENABLE_CPUNET && \
	(CONFIG_BOARD_THINGY53_NRF5340_CPUAPP || CONFIG_BOARD_THINGY53_NRF5340_CPUAPPNS))
	/* Give nRF21540fem control pins to NetworkMCU */
	nrf_gpio_pin_mcu_select(NRF21540FEM_CTRL_TX_EN_PIN,
				GPIO_PIN_CNF_MCUSEL_NetworkMCU); /* TX_EN */
	nrf_gpio_pin_mcu_select(32 + NRF21540FEM_CTRL_PDN_PIN,
				GPIO_PIN_CNF_MCUSEL_NetworkMCU); /* PDN */
	nrf_gpio_pin_mcu_select(32 + NRF21540FEM_CTRL_RX_EN_PIN,
				GPIO_PIN_CNF_MCUSEL_NetworkMCU); /* RX_EN */
	nrf_gpio_pin_mcu_select(32 + NRF21540FEM_CTRL_MODE_PIN,
				GPIO_PIN_CNF_MCUSEL_NetworkMCU); /* MODE */
#endif /* (CONFIG_BOARD_ENABLE_CPUNET &&
	* (CONFIG_BOARD_THINGY53_NRF5340_CPUAPP || CONFIG_BOARD_THINGY53_NRF5340_CPUAPPNS))
	*/

	return 0;
}

SYS_INIT(setup, POST_KERNEL, CONFIG_THINGY53_INIT_PRIORITY);
