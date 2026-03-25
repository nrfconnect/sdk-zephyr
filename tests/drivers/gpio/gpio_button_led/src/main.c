/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @brief Test demonstrating button press simulation and LED state observation
 *        using the GPIO emulator in the Twister test framework.
 *
 * This test shows how to:
 * 1. Simulate button presses using gpio_emul_input_set()
 * 2. Observe LED state changes using gpio_emul_output_get()
 * 3. Test button-to-LED interaction logic on native_sim
 *
 * The GPIO emulator (zephyr,gpio-emul) allows testing GPIO-driven
 * interactions without real hardware. Pin 2 is used as a button input
 * and pin 3 is used as an LED output on the emulated GPIO controller.
 */

#include <zephyr/kernel.h>
#include <zephyr/ztest.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/gpio/gpio_emul.h>

#define BUTTON_PIN 2
#define LED_PIN    3

static const struct device *const gpio_dev = DEVICE_DT_GET(DT_NODELABEL(gpio0));

static struct gpio_callback button_cb_data;
static volatile bool led_state;

static void button_handler(const struct device *dev, struct gpio_callback *cb,
			   uint32_t pins)
{
	int val;

	val = gpio_pin_get(gpio_dev, BUTTON_PIN);
	if (val < 0) {
		return;
	}

	led_state = (bool)val;
	gpio_pin_set(gpio_dev, LED_PIN, val);
}

static void *button_led_setup(void)
{
	int ret;

	zassert_true(device_is_ready(gpio_dev), "GPIO device not ready");

	ret = gpio_pin_configure(gpio_dev, BUTTON_PIN, GPIO_INPUT);
	zassert_ok(ret, "Failed to configure button pin");

	ret = gpio_pin_configure(gpio_dev, LED_PIN, GPIO_OUTPUT_INACTIVE);
	zassert_ok(ret, "Failed to configure LED pin");

	ret = gpio_pin_interrupt_configure(gpio_dev, BUTTON_PIN,
					   GPIO_INT_EDGE_BOTH);
	zassert_ok(ret, "Failed to configure button interrupt");

	gpio_init_callback(&button_cb_data, button_handler, BIT(BUTTON_PIN));
	ret = gpio_add_callback(gpio_dev, &button_cb_data);
	zassert_ok(ret, "Failed to add button callback");

	/* Set button to released state initially (logic low) */
	ret = gpio_emul_input_set(gpio_dev, BUTTON_PIN, 0);
	zassert_ok(ret, "Failed to set initial button state");

	led_state = false;

	return NULL;
}

static void button_led_before(void *fixture)
{
	ARG_UNUSED(fixture);

	/* Reset LED to off before each test */
	gpio_pin_set(gpio_dev, LED_PIN, 0);
	led_state = false;

	/* Ensure button is released (logic low) */
	gpio_emul_input_set(gpio_dev, BUTTON_PIN, 0);
	k_msleep(10);
}

ZTEST_SUITE(button_led, NULL, button_led_setup, button_led_before, NULL, NULL);

/**
 * @brief Test that pressing the button turns on the LED.
 *
 * Simulates a button press using gpio_emul_input_set() and verifies
 * the LED output state using gpio_emul_output_get().
 */
ZTEST(button_led, test_button_press_turns_led_on)
{
	int ret;

	/* Verify LED is initially off */
	ret = gpio_emul_output_get(gpio_dev, LED_PIN);
	zassert_equal(ret, 0, "LED should be off initially, got %d", ret);

	/* Simulate button press (drive pin high) */
	ret = gpio_emul_input_set(gpio_dev, BUTTON_PIN, 1);
	zassert_ok(ret, "Failed to simulate button press");

	/* Allow time for the interrupt callback to fire */
	k_msleep(10);

	/* Verify LED is now on */
	ret = gpio_emul_output_get(gpio_dev, LED_PIN);
	zassert_equal(ret, 1, "LED should be on after button press, got %d", ret);
	zassert_true(led_state, "led_state should be true after button press");
}

/**
 * @brief Test that releasing the button turns off the LED.
 *
 * Simulates a button press followed by release and verifies
 * the LED toggles accordingly.
 */
ZTEST(button_led, test_button_release_turns_led_off)
{
	int ret;

	/* Press button first */
	ret = gpio_emul_input_set(gpio_dev, BUTTON_PIN, 1);
	zassert_ok(ret, "Failed to simulate button press");
	k_msleep(10);

	/* Verify LED is on */
	ret = gpio_emul_output_get(gpio_dev, LED_PIN);
	zassert_equal(ret, 1, "LED should be on after press, got %d", ret);

	/* Release button (drive pin low) */
	ret = gpio_emul_input_set(gpio_dev, BUTTON_PIN, 0);
	zassert_ok(ret, "Failed to simulate button release");
	k_msleep(10);

	/* Verify LED is now off */
	ret = gpio_emul_output_get(gpio_dev, LED_PIN);
	zassert_equal(ret, 0, "LED should be off after button release, got %d", ret);
	zassert_false(led_state, "led_state should be false after button release");
}

/**
 * @brief Test multiple button press-release cycles.
 *
 * Verifies that the button-LED interaction works correctly across
 * multiple press-release cycles.
 */
ZTEST(button_led, test_button_multiple_cycles)
{
	int ret;

	for (int i = 0; i < 5; i++) {
		/* Press button */
		ret = gpio_emul_input_set(gpio_dev, BUTTON_PIN, 1);
		zassert_ok(ret, "Failed to simulate button press, cycle %d", i);
		k_msleep(10);

		ret = gpio_emul_output_get(gpio_dev, LED_PIN);
		zassert_equal(ret, 1,
			      "LED should be on after press in cycle %d, got %d",
			      i, ret);

		/* Release button */
		ret = gpio_emul_input_set(gpio_dev, BUTTON_PIN, 0);
		zassert_ok(ret, "Failed to simulate button release, cycle %d", i);
		k_msleep(10);

		ret = gpio_emul_output_get(gpio_dev, LED_PIN);
		zassert_equal(ret, 0,
			      "LED should be off after release in cycle %d, got %d",
			      i, ret);
	}
}
