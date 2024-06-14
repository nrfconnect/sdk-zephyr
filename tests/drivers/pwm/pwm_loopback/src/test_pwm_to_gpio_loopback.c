#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/ztest.h>

#include "test_pwm_to_gpio_loopback.h"

#define TEST_PWM_PERIOD_NSEC 100000000
#define TEST_PWM_PULSE_NSEC   15000000
#define TEST_PWM_PERIOD_USEC    100000
#define TEST_PWM_PULSE_USEC      75000

enum test_pwm_unit {
	TEST_PWM_UNIT_NSEC,
	TEST_PWM_UNIT_USEC,
};

void get_test_pwms(struct test_pwm *out, struct test_pwm *in)
{
	/* PWM generator device */
	out->dev = DEVICE_DT_GET(PWM_LOOPBACK_OUT_CTLR);
	out->pwm = PWM_LOOPBACK_OUT_CHANNEL;
	out->flags = PWM_LOOPBACK_OUT_FLAGS;
	zassert_true(device_is_ready(out->dev), "pwm loopback output device is not ready");

	/* PWM capture device */
	in->dev = DEVICE_DT_GET(GPIO_LOOPBACK_IN);
	zassert_true(device_is_ready(in->dev), "pwm loopback input device is not ready");
}

static void test_capture(uint32_t period, uint32_t pulse, enum test_pwm_unit unit,
		  pwm_flags_t flags)
{
	struct test_pwm in;
	struct test_pwm out;
	uint64_t period_capture = 0;
	uint64_t pulse_capture = 0;
	int err = 0;

	get_test_pwms(&out, &in);

	switch (unit) {
	case TEST_PWM_UNIT_NSEC:
		TC_PRINT("Testing PWM capture @ %u/%u nsec\n",
			 pulse, period);
		err = pwm_set(out.dev, out.pwm, period, pulse, out.flags ^=
			      (flags & PWM_POLARITY_MASK));
		break;

	case TEST_PWM_UNIT_USEC:
		TC_PRINT("Testing PWM capture @ %u/%u usec\n",
			 pulse, period);
		err = pwm_set(out.dev, out.pwm, PWM_USEC(period),
			      PWM_USEC(pulse), out.flags ^=
			      (flags & PWM_POLARITY_MASK));
		break;

	default:
		TC_PRINT("Unsupported test unit");
		ztest_test_fail();
	}

	zassert_equal(err, 0, "failed to set pwm output (err %d)", err);
}
