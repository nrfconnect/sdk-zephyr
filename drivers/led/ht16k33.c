/*
 * Copyright (c) 2019 Henrik Brix Andersen <henrik@brixandersen.dk>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief LED driver for the HT16K33 I2C LED driver with keyscan
 */

#include <gpio.h>
#include <i2c.h>
#include <kernel.h>
#include <led.h>
#include <misc/byteorder.h>
#include <zephyr.h>

#define LOG_LEVEL CONFIG_LED_LOG_LEVEL
#include <logging/log.h>
LOG_MODULE_REGISTER(ht16k33);

#include <led/ht16k33.h>

#include "led_context.h"

/* HT16K33 commands and options */
#define HT16K33_CMD_DISP_DATA_ADDR 0x00

#define HT16K33_CMD_SYSTEM_SETUP   0x20
#define HT16K33_OPT_S              BIT(0)

#define HT16K33_CMD_KEY_DATA_ADDR  0x40

#define HT16K33_CMD_INT_FLAG_ADDR  0x60

#define HT16K33_CMD_DISP_SETUP     0x80
#define HT16K33_OPT_D              BIT(0)
#define HT16K33_OPT_B0             BIT(1)
#define HT16K33_OPT_B1             BIT(2)
#define HT16K33_OPT_BLINK_OFF      0
#define HT16K33_OPT_BLINK_2HZ      HT16K33_OPT_B0
#define HT16K33_OPT_BLINK_1HZ      HT16K33_OPT_B1
#define HT16K33_OPT_BLINK_05HZ     (HT16K33_OPT_B1 | HT16K33_OPT_B0)

#define HT16K33_CMD_ROW_INT_SET    0xa0
#define HT16K33_OPT_ROW_INT        BIT(0)
#define HT16K33_OPT_ACT            BIT(1)
#define HT16K33_OPT_ROW            0
#define HT16K33_OPT_INT_LOW        HT16K33_OPT_ROW_INT
#define HT16K33_OPT_INT_HIGH       (HT16K33_OPT_ACT | HT16K33_OPT_ROW_INT)

#define HT16K33_CMD_DIMMING_SET    0xe0

/* HT16K33 size definitions */
#define HT16K33_DISP_ROWS          16
#define HT16K33_DISP_COLS          8
#define HT16K33_DISP_DATA_SIZE     HT16K33_DISP_ROWS
#define HT16K33_DISP_SEGMENTS      (HT16K33_DISP_ROWS * HT16K33_DISP_COLS)
#define HT16K33_DIMMING_LEVELS     16
#define HT16K33_KEYSCAN_ROWS       3
#define HT16K33_KEYSCAN_COLS       13
#define HT16K33_KEYSCAN_DATA_SIZE  6

struct ht16k33_cfg {
	char *i2c_dev_name;
	u16_t i2c_addr;
	bool irq_enabled;
#ifdef CONFIG_HT16K33_KEYSCAN
	char *irq_dev_name;
	u32_t irq_pin;
	int irq_flags;
#endif /* CONFIG_HT16K33_KEYSCAN */
};

struct ht16k33_data {
	struct device *i2c;
	struct led_data dev_data;
	 /* Shadow buffer for the display data RAM */
	u8_t buffer[HT16K33_DISP_DATA_SIZE];
#ifdef CONFIG_HT16K33_KEYSCAN
	struct k_mutex lock;
	struct device *children[HT16K33_KEYSCAN_ROWS];
	struct gpio_callback irq_cb;
	struct k_thread irq_thread;
	struct k_sem irq_sem;
	struct k_timer timer;
	u16_t key_state[HT16K33_KEYSCAN_ROWS];

	K_THREAD_STACK_MEMBER(irq_thread_stack,
			      CONFIG_HT16K33_KEYSCAN_IRQ_THREAD_STACK_SIZE);
#endif /* CONFIG_HT16K33_KEYSCAN */
};

static int ht16k33_led_blink(struct device *dev, u32_t led,
			     u32_t delay_on, u32_t delay_off)
{
	/* The HT16K33 blinks all LEDs at the same frequency */
	ARG_UNUSED(led);

	const struct ht16k33_cfg *config = dev->config->config_info;
	struct ht16k33_data *data = dev->driver_data;
	struct led_data *dev_data = &data->dev_data;
	u32_t period;
	u8_t cmd;

	period = delay_on + delay_off;
	if (period < dev_data->min_period || period > dev_data->max_period) {
		return -EINVAL;
	}

	cmd = HT16K33_CMD_DISP_SETUP | HT16K33_OPT_D;
	if (delay_off == 0) {
		cmd |= HT16K33_OPT_BLINK_OFF;
	} else if (period > 1500)  {
		cmd |= HT16K33_OPT_BLINK_05HZ;
	} else if (period > 750)  {
		cmd |= HT16K33_OPT_BLINK_1HZ;
	} else {
		cmd |= HT16K33_OPT_BLINK_2HZ;
	}

	if (i2c_write(data->i2c, &cmd, 1, config->i2c_addr)) {
		LOG_ERR("Setting HT16K33 blink frequency failed");
		return -EIO;
	}

	return 0;
}

static int ht16k33_led_set_brightness(struct device *dev, u32_t led,
				      u8_t value)
{
	ARG_UNUSED(led);

	const struct ht16k33_cfg *config = dev->config->config_info;
	struct ht16k33_data *data = dev->driver_data;
	struct led_data *dev_data = &data->dev_data;
	u8_t dim;
	u8_t cmd;

	if (value < dev_data->min_brightness ||
	    value > dev_data->max_brightness) {
		return -EINVAL;
	}

	dim = (value * (HT16K33_DIMMING_LEVELS - 1)) / dev_data->max_brightness;
	cmd = HT16K33_CMD_DIMMING_SET | dim;

	if (i2c_write(data->i2c, &cmd, 1, config->i2c_addr)) {
		LOG_ERR("Setting HT16K33 brightness failed");
		return -EIO;
	}

	return 0;
}

static int ht16k33_led_set_state(struct device *dev, u32_t led, bool on)
{
	const struct ht16k33_cfg *config = dev->config->config_info;
	struct ht16k33_data *data = dev->driver_data;
	u8_t cmd[2];
	u8_t addr;
	u8_t bit;

	if (led >= HT16K33_DISP_SEGMENTS) {
		return -EINVAL;
	}

	addr = led / HT16K33_DISP_COLS;
	bit = led % HT16K33_DISP_COLS;

	cmd[0] = HT16K33_CMD_DISP_DATA_ADDR | addr;
	if (on) {
		cmd[1] = data->buffer[addr] | BIT(bit);
	} else {
		cmd[1] = data->buffer[addr] & ~BIT(bit);
	}

	if (data->buffer[addr] == cmd[1]) {
		return 0;
	}

	if (i2c_write(data->i2c, cmd, sizeof(cmd), config->i2c_addr)) {
		LOG_ERR("Setting HT16K33 LED %s failed", on ? "on" : "off");
		return -EIO;
	}

	data->buffer[addr] = cmd[1];

	return 0;
}

static int ht16k33_led_on(struct device *dev, u32_t led)
{
	return ht16k33_led_set_state(dev, led, true);
}

static int ht16k33_led_off(struct device *dev, u32_t led)
{
	return ht16k33_led_set_state(dev, led, false);
}

#ifdef CONFIG_HT16K33_KEYSCAN
u32_t ht16k33_get_pending_int(struct device *dev)
{
	const struct ht16k33_cfg *config = dev->config->config_info;
	struct ht16k33_data *data = dev->driver_data;
	u8_t cmd;
	u8_t flag;
	int err;

	cmd = HT16K33_CMD_INT_FLAG_ADDR;
	err = i2c_write_read(data->i2c, config->i2c_addr, &cmd, sizeof(cmd),
			     &flag, sizeof(flag));
	if (err) {
		LOG_ERR("Failed to to read HT16K33 IRQ flag");
		return 0;
	}

	return (flag ? 1 : 0);
}

static bool ht16k33_process_keyscan_data(struct device *dev)
{
	const struct ht16k33_cfg *config = dev->config->config_info;
	struct ht16k33_data *data = dev->driver_data;
	u8_t keys[HT16K33_KEYSCAN_DATA_SIZE];
	bool pressed;
	u16_t row;
	u16_t new;
	int err;
	int i;

	err = i2c_burst_read(data->i2c, config->i2c_addr,
			     HT16K33_CMD_KEY_DATA_ADDR, keys,
			     sizeof(keys));
	if (err) {
		LOG_ERR("Failed to to read HT16K33 key data (err %d)", err);
		return false;
	}

	k_mutex_lock(&data->lock, K_FOREVER);
	for (i = 0; i < HT16K33_KEYSCAN_ROWS; i++) {
		row = sys_get_le16(&keys[i * 2]);
		if (row) {
			pressed = true;
			new = data->key_state[i] ^ row;
			new &= row;
			if (data->children[i] && new) {
				ht16k33_process_keyscan_row_data(
					data->children[i], new);
			}
		}
		data->key_state[i] = row;
	}
	k_mutex_unlock(&data->lock);

	return pressed;
}

static void ht16k33_irq_thread(struct device *dev)
{
	struct ht16k33_data *data = dev->driver_data;
	bool pressed;

	while (true) {
		k_sem_take(&data->irq_sem, K_FOREVER);

		do {
			k_sem_reset(&data->irq_sem);
			pressed = ht16k33_process_keyscan_data(dev);
			k_sleep(CONFIG_HT16K33_KEYSCAN_DEBOUNCE_MSEC);
		} while (pressed);
	}
}

static void ht16k33_irq_callback(struct device *gpiob,
				 struct gpio_callback *cb, u32_t pins)
{
	struct ht16k33_data *data;

	ARG_UNUSED(gpiob);
	ARG_UNUSED(pins);

	data = CONTAINER_OF(cb, struct ht16k33_data, irq_cb);
	k_sem_give(&data->irq_sem);
}

static void ht16k33_timer_callback(struct k_timer *timer)
{
	struct ht16k33_data *data;

	data = CONTAINER_OF(timer, struct ht16k33_data, timer);
	k_sem_give(&data->irq_sem);
}

int ht16k33_register_keyscan_device(struct device *parent,
					   struct device *child,
					   u8_t keyscan_idx)
{
	struct ht16k33_data *data = parent->driver_data;

	k_mutex_lock(&data->lock, K_FOREVER);

	if (data->children[keyscan_idx]) {
		k_mutex_unlock(&data->lock);
		LOG_ERR("HT16K33 keyscan device %d already registered",
			keyscan_idx);
		return -EINVAL;
	}

	data->children[keyscan_idx] = child;
	k_mutex_unlock(&data->lock);

	return 0;
}
#endif /* CONFIG_HT16K33_KEYSCAN */

static int ht16k33_init(struct device *dev)
{
	const struct ht16k33_cfg *config = dev->config->config_info;
	struct ht16k33_data *data = dev->driver_data;
	struct led_data *dev_data = &data->dev_data;
	u8_t cmd[1 + HT16K33_DISP_DATA_SIZE]; /* 1 byte command + data */
	int err;

	data->i2c = device_get_binding(config->i2c_dev_name);
	if (data->i2c == NULL) {
		LOG_ERR("Failed to get I2C device");
		return -EINVAL;
	}

	memset(&data->buffer, 0, sizeof(data->buffer));

	/* Hardware specific limits */
	dev_data->min_period = 0U;
	dev_data->max_period = 2000U;
	dev_data->min_brightness = 0U;
	dev_data->max_brightness = 100U;

	/* System oscillator on */
	cmd[0] = HT16K33_CMD_SYSTEM_SETUP | HT16K33_OPT_S;
	err = i2c_write(data->i2c, cmd, 1, config->i2c_addr);
	if (err) {
		LOG_ERR("Enabling HT16K33 system oscillator failed (err %d)",
			err);
		return -EIO;
	}

	/* Clear display RAM */
	memset(cmd, 0, sizeof(cmd));
	cmd[0] = HT16K33_CMD_DISP_DATA_ADDR;
	err = i2c_write(data->i2c, cmd, sizeof(cmd), config->i2c_addr);
	if (err) {
		LOG_ERR("Clearing HT16K33 display RAM failed (err %d)", err);
		return -EIO;
	}

	/* Full brightness */
	cmd[0] = HT16K33_CMD_DIMMING_SET | 0x0f;
	err = i2c_write(data->i2c, cmd, 1, config->i2c_addr);
	if (err) {
		LOG_ERR("Setting HT16K33 brightness failed (err %d)", err);
		return -EIO;
	}

	/* Display on, blinking off */
	cmd[0] = HT16K33_CMD_DISP_SETUP | HT16K33_OPT_D | HT16K33_OPT_BLINK_OFF;
	err = i2c_write(data->i2c, cmd, 1, config->i2c_addr);
	if (err) {
		LOG_ERR("Enabling HT16K33 display failed (err %d)", err);
		return -EIO;
	}

#ifdef CONFIG_HT16K33_KEYSCAN
	memset(&data->children, 0, sizeof(data->children));
	k_mutex_init(&data->lock);
	k_sem_init(&data->irq_sem, 0, 1);

	/* Configure interrupt */
	if (config->irq_enabled) {
		struct device *irq_dev;
		u8_t keys[HT16K33_KEYSCAN_DATA_SIZE];

		irq_dev = device_get_binding(config->irq_dev_name);
		if (!irq_dev) {
			LOG_ERR("IRQ device '%s' not found",
				config->irq_dev_name);
			return -EINVAL;
		}

		err = gpio_pin_configure(irq_dev, config->irq_pin,
					 GPIO_DIR_IN | GPIO_INT |
					 GPIO_INT_EDGE | config->irq_flags);
		if (err) {
			LOG_ERR("Failed to configure IRQ pin (err %d)", err);
			return -EINVAL;
		}

		gpio_init_callback(&data->irq_cb, &ht16k33_irq_callback,
				   BIT(config->irq_pin));

		err = gpio_add_callback(irq_dev, &data->irq_cb);
		if (err) {
			LOG_ERR("Failed to add IRQ callback (err %d)", err);
			return -EINVAL;
		}

		/* Enable interrupt pin */
		cmd[0] = HT16K33_CMD_ROW_INT_SET;
		if (config->irq_flags & GPIO_INT_ACTIVE_HIGH) {
			cmd[0] |= HT16K33_OPT_INT_HIGH;
		} else {
			cmd[0] |= HT16K33_OPT_INT_LOW;
		}
		if (i2c_write(data->i2c, cmd, 1, config->i2c_addr)) {
			LOG_ERR("Enabling HT16K33 IRQ output failed");
			return -EIO;
		}

		/* Flush key data before enabling interrupt */
		err = i2c_burst_read(data->i2c, config->i2c_addr,
				HT16K33_CMD_KEY_DATA_ADDR, keys, sizeof(keys));
		if (err) {
			LOG_ERR("Failed to to read HT16K33 key data");
			return -EIO;
		}

		err = gpio_pin_enable_callback(irq_dev, config->irq_pin);
		if (err) {
			LOG_ERR("Failed to enable IRQ callback (err %d)", err);
			return -EINVAL;
		}
	} else {
		/* No interrupt pin, enable ROW15 */
		cmd[0] = HT16K33_CMD_ROW_INT_SET | HT16K33_OPT_ROW;
		if (i2c_write(data->i2c, cmd, 1, config->i2c_addr)) {
			LOG_ERR("Enabling HT16K33 ROW15 output failed");
			return -EIO;
		}

		/* Setup timer for polling key data */
		k_timer_init(&data->timer, ht16k33_timer_callback, NULL);
		k_timer_start(&data->timer, 0,
			      CONFIG_HT16K33_KEYSCAN_POLL_MSEC);
	}

	k_thread_create(&data->irq_thread, data->irq_thread_stack,
			CONFIG_HT16K33_KEYSCAN_IRQ_THREAD_STACK_SIZE,
			(k_thread_entry_t)ht16k33_irq_thread, dev, NULL, NULL,
			K_PRIO_COOP(CONFIG_HT16K33_KEYSCAN_IRQ_THREAD_PRIO),
			0, K_NO_WAIT);
#endif /* CONFIG_HT16K33_KEYSCAN */

	return 0;
}

static const struct led_driver_api ht16k33_leds_api = {
	.blink = ht16k33_led_blink,
	.set_brightness = ht16k33_led_set_brightness,
	.on = ht16k33_led_on,
	.off = ht16k33_led_off,
};

#define HT16K33_DEVICE(id)						\
	static const struct ht16k33_cfg ht16k33_##id##_cfg = {		\
		.i2c_dev_name = DT_HOLTEK_HT16K33_##id##_BUS_NAME,	\
		.i2c_addr     = DT_HOLTEK_HT16K33_##id##_BASE_ADDRESS,	\
		.irq_enabled  = false,					\
	};								\
									\
static struct ht16k33_data ht16k33_##id##_data;				\
									\
DEVICE_AND_API_INIT(ht16k33_##id, DT_HOLTEK_HT16K33_##id##_LABEL,	\
		    &ht16k33_init, &ht16k33_##id##_data,		\
		    &ht16k33_##id##_cfg, POST_KERNEL,			\
		    CONFIG_LED_INIT_PRIORITY, &ht16k33_leds_api)

#ifdef CONFIG_HT16K33_KEYSCAN
#define HT16K33_DEVICE_WITH_IRQ(id)					\
	static const struct ht16k33_cfg ht16k33_##id##_cfg = {		\
		.i2c_dev_name = DT_HOLTEK_HT16K33_##id##_BUS_NAME,	\
		.i2c_addr     = DT_HOLTEK_HT16K33_##id##_BASE_ADDRESS,	\
		.irq_enabled  = true,					\
		.irq_dev_name =						\
			DT_HOLTEK_HT16K33_##id##_IRQ_GPIOS_CONTROLLER,	\
		.irq_pin      = DT_HOLTEK_HT16K33_##id##_IRQ_GPIOS_PIN,	\
		.irq_flags    =						\
			DT_HOLTEK_HT16K33_##id##_IRQ_GPIOS_FLAGS,	\
	};								\
									\
static struct ht16k33_data ht16k33_##id##_data;				\
									\
DEVICE_AND_API_INIT(ht16k33_##id, DT_HOLTEK_HT16K33_##id##_LABEL,	\
		    &ht16k33_init, &ht16k33_##id##_data,		\
		    &ht16k33_##id##_cfg, POST_KERNEL,			\
		    CONFIG_LED_INIT_PRIORITY, &ht16k33_leds_api)
#else /* ! CONFIG_HT16K33_KEYSCAN */
#define HT16K33_DEVICE_WITH_IRQ(id) HT16K33_DEVICE(id)
#endif /* ! CONFIG_HT16K33_KEYSCAN */

/* Support up to eight HT16K33 devices */

#ifdef DT_HOLTEK_HT16K33_0
#ifdef DT_HOLTEK_HT16K33_0_IRQ_GPIOS_CONTROLLER
HT16K33_DEVICE_WITH_IRQ(0);
#else
HT16K33_DEVICE(0);
#endif
#endif

#ifdef DT_HOLTEK_HT16K33_1
#ifdef DT_HOLTEK_HT16K33_1_IRQ_GPIOS_CONTROLLER
HT16K33_DEVICE_WITH_IRQ(1);
#else
HT16K33_DEVICE(1);
#endif
#endif

#ifdef DT_HOLTEK_HT16K33_2
#ifdef DT_HOLTEK_HT16K33_2_IRQ_GPIOS_CONTROLLER
HT16K33_DEVICE_WITH_IRQ(2);
#else
HT16K33_DEVICE(2);
#endif
#endif

#ifdef DT_HOLTEK_HT16K33_3
#ifdef DT_HOLTEK_HT16K33_3_IRQ_GPIOS_CONTROLLER
HT16K33_DEVICE_WITH_IRQ(3);
#else
HT16K33_DEVICE(3);
#endif
#endif

#ifdef DT_HOLTEK_HT16K33_4
#ifdef DT_HOLTEK_HT16K33_4_IRQ_GPIOS_CONTROLLER
HT16K33_DEVICE_WITH_IRQ(4);
#else
HT16K33_DEVICE(4);
#endif
#endif

#ifdef DT_HOLTEK_HT16K33_5
#ifdef DT_HOLTEK_HT16K33_5_IRQ_GPIOS_CONTROLLER
HT16K33_DEVICE_WITH_IRQ(5);
#else
HT16K33_DEVICE(5);
#endif
#endif

#ifdef DT_HOLTEK_HT16K33_6
#ifdef DT_HOLTEK_HT16K33_6_IRQ_GPIOS_CONTROLLER
HT16K33_DEVICE_WITH_IRQ(6);
#else
HT16K33_DEVICE(6);
#endif
#endif

#ifdef DT_HOLTEK_HT16K33_7
#ifdef DT_HOLTEK_HT16K33_7_IRQ_GPIOS_CONTROLLER
HT16K33_DEVICE_WITH_IRQ(7);
#else
HT16K33_DEVICE(7);
#endif
#endif
